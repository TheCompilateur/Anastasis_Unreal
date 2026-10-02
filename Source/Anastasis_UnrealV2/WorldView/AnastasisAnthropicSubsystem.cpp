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
    Restore(); Memory.Reset(); LastTime = -1.0; Refresh = 0.0; ApplyMs = 0.0; bActive = false; bTruncatedGrass = false;
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
        if (bActive) { Restore(); Memory.Reset(); LastTime = -1.0; bActive = false; }
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
    { Scale = It->GetSnapshot().SpatialScale; break; }
    if (Scale <= 0.0) return; // no known rendered coordinate system, no invented placement
    TArray<AnastasisAnthropic::FObservation> People;
    const auto& W = Sim.GetWorld();
    for (const auto& N : Sim.GetVillage().GetActors())
    {
        const int32 X = FMath::FloorToInt32(N.X), Y = FMath::FloorToInt32(N.Y);
        const bool bLand = X >= 0 && Y >= 0 && X < W.W && Y < W.H
            && W.Tiles.IsValidIndex(Y * W.W + X) && W.Tiles[Y * W.W + X].Type != AnastasisWorld::ETileType::Water;
        People.Add({N.Id, FVector2D(N.X, N.Y) * AnastasisWorldView::TileWorldSize * Scale,
            bLand && !N.Inside.bActive});
    }
    Memory.Observe(Sim.GetTime(), People);
    LastTime = Sim.GetTime();
    Refresh += DeltaTime;
    if (Refresh >= 1.0) { Refresh = 0.0; ApplyGrass(); }
}
void UAnastasisAnthropicSubsystem::ApplyGrass()
{
    const double Start = FPlatformTime::Seconds();
    // Restore before overlap queries: compressed instances must not shrink out of the search.
    Restore();
    bTruncatedGrass = false;
    TArray<AnastasisAnthropic::FCell> Visible;
    for (const auto& Pair : Memory.GetCells())
        if (Pair.Value.Metres > 2.0) Visible.Add(Pair.Value);
    Visible.Sort([](const auto& A, const auto& B)
    {
        if (A.Metres != B.Metres) return A.Metres > B.Metres;
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
                FTransform Applied = Original;
                FVector Size = Original.GetScale3D();
                Size.Z *= FMath::Lerp(1.0, 0.08, Strength);
                // At high repeated use some ground becomes visible; no soil is repainted.
                Size.X *= FMath::Lerp(1.0, 0.45, Strength);
                Size.Y *= FMath::Lerp(1.0, 0.45, Strength);
                Applied.SetScale3D(Size);
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
    return FString::Printf(TEXT("active=%d cells=%d grass=%d gaps=%d jumps=%d dropped=%d grass_cap=%d apply_ms=%.3f restored=%d restore_errors=%d"),
        bActive, Memory.GetCells().Num(), Grass.Num(), Memory.RejectedGaps, Memory.RejectedJumps,
        Memory.DroppedCells, bTruncatedGrass, ApplyMs, Restored, RestoreErrors);
}


FString UAnastasisAnthropicDebugLibrary::GetStatus(const UObject* WorldContextObject)
{
    UWorld* W = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
    auto* S = W ? W->GetSubsystem<UAnastasisAnthropicSubsystem>() : nullptr;
    return S ? S->GetReport() : TEXT("unavailable=1");
}
