#include "WorldView/AnastasisForestUse.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"

namespace AnastasisForestUse
{
double Depletion(const AnastasisWorld::FTile& Original, const AnastasisWorld::FTile* Live)
{
    using namespace AnastasisWorld;
    if (!Live || Original.Type != ETileType::Forest || Original.Resource != EResource::Wood || Original.Amount <= 0)
        return 0.0;
    // A type/resource conversion is not sufficient evidence of wood extraction.
    if (Live->Resource != EResource::Wood || Live->X != Original.X || Live->Y != Original.Y) return 0.0;
    return FMath::Clamp(double(Original.Amount - FMath::Max(0, Live->Amount)) / Original.Amount, 0.0, 1.0);
}
bool HideTree(double Fraction, uint32 StableSeed)
{
    uint32 H = StableSeed ^ 0x7f4a7c15u;
    H ^= H >> 16; H *= 0x7feb352du; H ^= H >> 15; H *= 0x846ca68bu; H ^= H >> 16;
    const double Threshold = (double(H) + 0.5) / 4294967296.0;
    return FMath::IsFinite(Fraction) && Fraction >= Threshold;
}
void FBinding::Reset() { Trees.Reset(); Shells.Reset(); Hidden = Changed = Conflicts = 0; }
void FBinding::Add(UHierarchicalInstancedStaticMeshComponent* Mesh, int32 Index, int32 SourceIndex, uint32 Seed)
{
    FTransform Pose;
    if (Mesh && Index != INDEX_NONE && Mesh->GetInstanceTransform(Index, Pose, false))
        Trees.Add({Mesh, Index, SourceIndex, Seed, Pose, false});
}
void FBinding::AddShell(UHierarchicalInstancedStaticMeshComponent* Mesh, int32 Index, FVector2D Center, double Radius)
{
    FTransform Pose;
    if (Mesh && Index != INDEX_NONE && Mesh->GetInstanceTransform(Index, Pose, false))
        Shells.Add({{Mesh, Index, INDEX_NONE, 0, Pose, false}, Center, Radius});
}
void FBinding::Apply(const AnastasisWorld::FWorld& World, const TMap<int32, AnastasisWorld::FTile>& Live, bool bEnabled)
{
    Changed = Hidden = Conflicts = 0;
    TSet<UHierarchicalInstancedStaticMeshComponent*> Dirty;
    auto SetHidden = [&](FInstance& Entry, bool bHide)
    {
        auto* Mesh = Entry.Mesh.Get();
        if (!Mesh) return;
        FTransform Expected = Entry.Original;
        if (Entry.bHidden) Expected.SetScale3D(FVector::ZeroVector);
        FTransform Current;
        if (!Mesh->GetInstanceTransform(Entry.Index, Current, false) || !(Entry.bHidden
            ? Current.GetLocation().Equals(Expected.GetLocation(), 0.001) && Current.GetScale3D().IsNearlyZero(0.001)
            : Current.Equals(Expected, 0.001)))
        { ++Conflicts; Entry.Mesh.Reset(); return; } // relinquish stale/external edits
        if (Entry.bHidden == bHide) return;
        FTransform Desired = Entry.Original;
        if (bHide) Desired.SetScale3D(FVector::ZeroVector);
        // Stable instance indices; no RemoveInstance swap, no reroll, no tree shrinking.
        if (Mesh->UpdateInstanceTransform(Entry.Index, Desired, false, false, true))
        { Entry.bHidden = bHide; Dirty.Add(Mesh); ++Changed; }
    };
    TArray<FVector2D> Removed;
    for (FInstance& Tree : Trees)
    {
        const double Fraction = bEnabled && World.Tiles.IsValidIndex(Tree.SourceIndex)
            ? Depletion(World.Tiles[Tree.SourceIndex], Live.Find(Tree.SourceIndex)) : 0.0;
        SetHidden(Tree, HideTree(Fraction, Tree.StableSeed));
        if (Tree.Mesh.IsValid() && Tree.bHidden)
        { ++Hidden; Removed.Add(FVector2D(Tree.Original.GetLocation())); }
    }
    // Optional far proxies cannot continue filling a harvested opening. Ordinary trees outside
    // that opening stay intact; only an overlapping aggregate shell is conservatively suppressed.
    for (FShell& Shell : Shells)
    {
        bool bHide = false;
        for (const FVector2D& P : Removed)
            if (FVector2D::DistSquared(P, Shell.Center) <= FMath::Square(Shell.Radius)) { bHide = true; break; }
        SetHidden(Shell.Instance, bHide);
    }
    for (auto* Mesh : Dirty) Mesh->MarkRenderStateDirty();
}
}
