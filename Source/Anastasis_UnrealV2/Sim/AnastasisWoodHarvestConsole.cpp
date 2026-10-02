#include "Sim/AnastasisSimulationSubsystem.h"
#include "HAL/IConsoleManager.h"
#include "Engine/World.h"
#include "Village/AnastasisVillage.h"

static FAutoConsoleCommandWithWorldAndArgs WoodcutterCommand(
    TEXT("Anastasis.Village.Woodcutter"),
    TEXT("Woodcutter <existing NPC id>: assign woodcutter; no spawn, teleport, resources or forced goal."),
    FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args,UWorld* World)
    {
        auto* Host=World ? World->GetSubsystem<UAnastasisSimulationSubsystem>() : nullptr;
        if(!Host || !Host->GetSimulation().IsRunning() || Args.Num()!=1) return;
        const bool Assigned=Host->GetSimulation().GetVillage().SetJob(Args[0],TEXT("woodcutter"));
        UE_LOG(LogTemp,Display,TEXT("ANASTASIS_WOOD assigned=%d npc=%s"),Assigned,*Args[0]);
    }));

static FAutoConsoleCommandWithWorldAndArgs WoodStatusCommand(
    TEXT("Anastasis.Village.WoodStatus"),TEXT("Read live wood and carried wood; no mutation."),
    FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args,UWorld* World)
    {
        auto* Host=World ? World->GetSubsystem<UAnastasisSimulationSubsystem>() : nullptr;
        if(!Host || !Host->GetSimulation().IsRunning()) return;
        const auto& Sim=Host->GetSimulation(); const auto& V=Sim.GetVillage();
        int64 Remaining=0,Carried=0,Gathered=0;
        for(const auto& Base:Sim.GetWorld().Tiles) {
            const auto T=V.LiveTileAt(Base.X,Base.Y);
            if(T.Resource==AnastasisWorld::EResource::Wood) Remaining+=T.Amount;
        }
        for(const auto& N:V.GetActors()) {
            Carried+=N.InventoryWood; Gathered+=N.GatheredWood;
            if(N.JobId==TEXT("woodcutter")) UE_LOG(LogTemp,Display,TEXT("ANASTASIS_WOOD npc=%s goal=%s carried=%d gathered=%d x=%.3f y=%.3f"),*N.Id,*N.Goal,N.InventoryWood,N.GatheredWood,N.X,N.Y);
        }
        UE_LOG(LogTemp,Display,TEXT("ANASTASIS_WOOD remaining=%lld carried=%lld gathered=%lld total=%lld"),Remaining,Carried,Gathered,Remaining+Carried);
    }));
