#include "WorldView/AnastasisWorldEmbodiment.h"
#include "Sim/AnastasisSimulationSubsystem.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"

static TAutoConsoleVariable<int32> CVarForestUse(TEXT("anastasis.Dressing.ForestUse"), 1,
    TEXT("Read live forest wood depletion, hide stable tree instances. 0 restores baseline presentation. No simulation writes or regrowth."));
void AAnastasisWorldEmbodiment::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!GetWorld() || !GetWorld()->IsGameWorld() || !GetWorld()->HasBegunPlay()) return;
    const double Start = FPlatformTime::Seconds();
    const auto* Host = GetWorld()->GetSubsystem<UAnastasisSimulationSubsystem>();
    if (!Host) return;
    const auto& Sim = Host->GetSimulation();
    const auto& World = Sim.GetWorld();
    bForestUseMatched = Sim.IsRunning() && Sim.GetSeed() == Snapshot.Seed
        && World.W == Snapshot.SourceW && World.H == Snapshot.SourceH;
    ForestUse.Apply(World, Sim.GetVillage().GetLiveTiles(),
        bForestUseMatched && CVarForestUse.GetValueOnGameThread() != 0);
    ForestUseMs = (FPlatformTime::Seconds() - Start) * 1000.0;
}
FString AAnastasisWorldEmbodiment::GetForestUseReport() const
{
    return FString::Printf(TEXT("enabled=%d matched=%d bound=%d hidden=%d changed=%d conflicts=%d apply_ms=%.3f"),
        CVarForestUse.GetValueOnGameThread(), bForestUseMatched, ForestUse.Count(), ForestUse.Hidden,
        ForestUse.Changed, ForestUse.Conflicts, ForestUseMs);
}
