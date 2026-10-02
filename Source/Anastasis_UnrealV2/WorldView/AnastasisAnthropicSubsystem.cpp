#include "WorldView/AnastasisAnthropicSubsystem.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "Sim/AnastasisSimulationSubsystem.h"
#include "WorldView/AnastasisWorldEmbodiment.h"
#include "WorldView/AnastasisWorldView.h"

static TAutoConsoleVariable<int32> CVarAnthropicMemory(
    TEXT("anastasis.Anthropic.Memory"), 0,
    TEXT("Experimental sampled NPC tread: 1 records outdoor displacement and lowers nearby grass; 0 restores grass and clears presentation memory. No simulation writes, no saved history. Default off pending visual/GPU evidence."));

// Visibility is independent of observation: A/B never invents or erases traffic.
static TAutoConsoleVariable<int32> CVarAnthropicDraw(
    TEXT("anastasis.Anthropic.Draw"), 1,
    TEXT("Show recorded grass tread. 0 restores grass but retains and continues observation memory; 1 reapplies it."));

bool UAnastasisAnthropicSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
    const UWorld* W = Cast<UWorld>(Outer);
    return W && W->IsGameWorld();
}
TStatId UAnastasisAnthropicSubsystem::GetStatId() const
{
    RETURN_QUICK_DECLARE_CYCLE_STAT(UAnastasisAnthropicSubsystem, STATGROUP_Tickables);
}
void UAnastasisAnthropicSubsystem::Restore()
{
    Restored = RestoreErrors = 0;
    TSet<UHierarchicalInstancedStaticMeshComponent*> Dirty;
    for (const FGrass& G : Grass)
    {
        auto* M = G.Component.Get();
        FTransform Current;
        // Never restore stale indices after a terrain rebuild or overwrite another editor's change.
        if (M && M->GetInstanceCount() == G.Count && M->GetInstanceTransform(G.Index, Current, true)
            && Current.Equals(G.Applied, 0.001))
        {
            const bool bUpdated = M->UpdateInstanceTransform(G.Index, G.Original, true, false, true);
            FTransform Check;
            if (bUpdated && M->GetInstanceTransform(G.Index, Check, true) && Check.Equals(G.Original, 0.001)) ++Restored;
            else ++RestoreErrors;
            Dirty.Add(M);
        }
    }
    for (auto* M : Dirty) M->MarkRenderStateDirty();
    Grass.Reset();
}
void UAnastasisAnthropicSubsystem::ResetPresentation()
{
    Restore(); Memory.Reset(); LastTime = -1.0; Refresh = 0.0; ApplyMs = 0.0; bActive = false; bDrawn = false; AppliedTime = -1.0; Focus = FVector::ZeroVector; FocusStrength = 0.0; bTruncatedGrass = false;
}
void UAnastasisAnthropicSubsystem::Deinitialize()
{
    Restore(); Memory.Reset();
    Super::Deinitialize();
}
void UAnastasisAnthropicSubsystem::Tick(float DeltaTime)
{
    if (!GetWorld() || !GetWorld()->HasBegunPlay()) return;
    if (CVarAnthropicMemory.GetValueOnGameThread() == 0)
    {
        if (bActive) { Restore(); Memory.Reset(); LastTime = -1.0; bActive = false; bDrawn = false; AppliedTime = -1.0; }
        return;
    }
    auto* Host = GetWorld()->GetSubsystem<UAnastasisSimulationSubsystem>();
    if (!Host || !Host->GetSimulation().IsRunning()) return;
    const auto& Sim = Host->GetSimulation();
    if (Sim.GetTime() < LastTime || (bActive && Seed != Sim.GetSeed()))
    {
        Restore(); Memory.Reset(); LastTime = -1.0;
    }
    bActive = true; Seed = Sim.GetSeed();
    double Scale = 0.0;
    for (TActorIterator<AAnastasisWorldEmbodiment> It(GetWorld()); It; ++It)
    {
        const auto& Snapshot = It->GetSnapshot();
        if (Snapshot.Seed == Sim.GetSeed() && Snapshot.SourceW == Sim.GetWorld().W && Snapshot.SourceH == Sim.GetWorld().H)
        { Scale = Snapshot.SpatialScale; break; }
    }
    if (Scale <= 0.0) return; // no known rendered coordinate system, no invented placement
    TArray<AnastasisAnthropic::FObservation> People;
    const auto& W = Sim.GetWorld();
    for (const auto& N : Sim.GetVillage().GetActors())
    {
        const int32 X = FMath::FloorToInt32(N.X), Y = FMath::FloorToInt32(N.Y);
        const bool bLand = X >= 0 && Y >= 0 && X < W.W && Y < W.H
            && W.Tiles.IsValidIndex(Y * W.W + X) && W.Tiles[Y * W.W + X].Type != AnastasisWorld::ETileType::Water;
        People.Add({N.Id, FVector2D(N.X, N.Y) * AnastasisWorldView::TileWorldSize * Scale,
            bLand && !N.Inside.bActive, bLand ? W.Tiles[Y * W.W + X].Wetness : 0.0});
    }
    Memory.Observe(Sim.GetTime(), People);
    LastTime = Sim.GetTime();
    Refresh += DeltaTime;
    const bool bWantDraw = CVarAnthropicDraw.GetValueOnGameThread() != 0;
    if (!bWantDraw)
    {
        if (bDrawn) Restore();
        bDrawn = false;
    }
    else if (!bDrawn || (Refresh >= 1.0 && AppliedTime != Sim.GetTime()))
    {
        Refresh = 0.0; ApplyGrass(); bDrawn = true; AppliedTime = Sim.GetTime();
    }
}
void UAnastasisAnthropicSubsystem::ApplyGrass()
{
    const double Start = FPlatformTime::Seconds();
    // Restore before overlap queries: compressed instances must not shrink out of the search.
    Restore();
    bTruncatedGrass = false;
    FocusStrength = 0.0;
    TArray<AnastasisAnthropic::FCell> Visible;
    for (const auto& Pair : Memory.GetCells())
        if (Pair.Value.Metres > 2.0 && Pair.Value.Wear > 2.0) Visible.Add(Pair.Value);
    Visible.Sort([](const auto& A, const auto& B)
    {
        if (A.Wear != B.Wear) return A.Wear > B.Wear;
        if (A.Position.X != B.Position.X) return A.Position.X < B.Position.X;
        return A.Position.Y < B.Position.Y;
    });
    // Separate observation capacity from rendering cost: retain approaches, draw strongest use.
    if (Visible.Num() > 256) { Visible.SetNum(256); bTruncatedGrass = true; }
    for (TActorIterator<AAnastasisWorldEmbodiment> It(GetWorld()); It; ++It)
    {
        TArray<UHierarchicalInstancedStaticMeshComponent*> Meshes;
        It->GetComponents(Meshes);
        for (auto* M : Meshes)
        {
            if (!M || !M->GetName().StartsWith(TEXT("GroundCover_"))) continue;
            TSet<int32> Indices;
            const FBox Bounds = M->Bounds.GetBox();
            for (const auto& C : Visible)
            {
                const FBox Query(FVector(C.Position.X - 75, C.Position.Y - 75, Bounds.Min.Z),
                    FVector(C.Position.X + 75, C.Position.Y + 75, Bounds.Max.Z));
                if (!Bounds.Intersect(Query)) continue;
                for (int32 Index : M->GetInstancesOverlappingBox(Query, true)) Indices.Add(Index);
            }
            bool bDirty = false;
            TArray<int32> Sorted = Indices.Array(); Sorted.Sort();
            for (int32 Index : Sorted)
            {
                if (Grass.Num() >= 8192) { bTruncatedGrass = true; break; }
                FTransform Original;
                if (!M->GetInstanceTransform(Index, Original, true)) continue;
                const FVector P = Original.GetLocation();
                const double Strength = Memory.StrengthAt(FVector2D(P.X, P.Y));
                if (Strength <= 0.001) continue;
                if (Strength > FocusStrength) { FocusStrength = Strength; Focus = P; }
                FTransform Applied = Original;
                FVector Size = Original.GetScale3D();
                Size.Z *= FMath::Lerp(1.0, 0.35, Strength);
                // At high repeated use some ground becomes visible; no soil is repainted.
                Size.X *= FMath::Lerp(1.0, 0.45, Strength);
                Size.Y *= FMath::Lerp(1.0, 0.45, Strength);
                Applied.SetScale3D(Size);
                double Coherence;
                const FVector2D Axis = Memory.AxisAt(FVector2D(P.X, P.Y), Coherence);
                // Tilt around the observed travel axis's perpendicular, preserving the rooted location.
                // Opposing trips reinforce; crossing flows reduce the directional lean.
                const FQuat Lean(FVector(-Axis.Y, Axis.X, 0), FMath::DegreesToRadians(60.0) * Strength * Coherence);
                Applied.SetRotation(Lean * Original.GetRotation());
                if (M->UpdateInstanceTransform(Index, Applied, true, false, true))
                { Grass.Add({M, Index, M->GetInstanceCount(), Original, Applied}); bDirty = true; }
            }
            if (bDirty) M->MarkRenderStateDirty();
        }
    }
    ApplyMs = (FPlatformTime::Seconds() - Start) * 1000.0;
}
FString UAnastasisAnthropicSubsystem::GetReport() const
{
    FVector Peak = FVector::ZeroVector;
    int32 People = -1;
    FVector View = FVector::ZeroVector;
    double ViewStrength = -1;
    for (const FGrass& G : Grass)
    {
        const FVector P = G.Original.GetLocation();
        const double Strength = Memory.StrengthAt(FVector2D(P.X, P.Y));
        if (Strength > ViewStrength) { ViewStrength = Strength; View = P; }
    }
    double PeakStrength = 0.0;
    for (const auto& Pair : Memory.GetCells())
    {
        const double Strength = Memory.StrengthAt(Pair.Value.Position);
        if (Strength > PeakStrength)
        {
            PeakStrength = Strength;
            Peak = FVector(Pair.Value.Position.X, Pair.Value.Position.Y, 0);
        }
    }
    if (const auto* Host = GetWorld()->GetSubsystem<UAnastasisSimulationSubsystem>())
    {
        People = Host->GetSimulation().GetVillage().GetActors().Num();
        for (TActorIterator<AAnastasisWorldEmbodiment> It(GetWorld()); It; ++It)
        {
            const double UU = AnastasisWorldView::TileWorldSize * It->GetSnapshot().SpatialScale;
            if (UU > 0) Peak = FAnastasisVillagePresentation::SimToUnreal(Host->GetSimulation().GetWorld(), Peak.X / UU, Peak.Y / UU, GetWorld());
            break;
        }
    }
    return FString::Printf(TEXT("active=%d cells=%d grass=%d gaps=%d jumps=%d dropped=%d grass_cap=%d apply_ms=%.3f restored=%d restore_errors=%d drawn=%d focus_x=%.3f focus_y=%.3f focus_z=%.3f focus_strength=%.3f display=%d peak=%.6f peak_x=%.3f peak_y=%.3f peak_z=%.3f people=%d view_x=%.3f view_y=%.3f view_z=%.3f"),
        bActive, Memory.GetCells().Num(), Grass.Num(), Memory.RejectedGaps, Memory.RejectedJumps,
        Memory.DroppedCells, bTruncatedGrass, ApplyMs, Restored, RestoreErrors, bDrawn, Focus.X, Focus.Y, Focus.Z, FocusStrength,
        CVarAnthropicDraw.GetValueOnGameThread(), PeakStrength, Peak.X, Peak.Y, Peak.Z, People, View.X, View.Y, View.Z);
}


FString UAnastasisAnthropicDebugLibrary::GetStatus(const UObject* WorldContextObject)
{
    UWorld* W = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
    auto* S = W ? W->GetSubsystem<UAnastasisAnthropicSubsystem>() : nullptr;
    return S ? S->GetReport() : TEXT("unavailable=1");
}
