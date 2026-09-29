#include "Sim/AnastasisSimulationSubsystem.h"

#include "Anastasis_UnrealV2.h"
#include "Stats/Stats.h"
#include "Core/AnastasisJsNumeric.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Village/AnastasisVillage.h"
#include "Village/AnastasisVillageInteractionSubsystem.h"
#include "WorldView/AnastasisWorldView.h"

static TAutoConsoleVariable<float> CVarSimSpeed(
	TEXT("anastasis.Sim.Speed"),
	10.0f,
	TEXT("Simulation speed scale (JS 1/2/5/10). Default 10 so midnight is visible in a short PIE. Set 1 for JS realtime."),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarVillageDebug(
	TEXT("anastasis.Village.Debug"),
	1,
	TEXT("1 = draw the simulated village (buildings, access points, inhabitants, targets) in PIE."),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarSimOverlay(
	TEXT("anastasis.Sim.Overlay"),
	1,
	TEXT("1 = draw day/time overlay in PIE. 0 = log only."),
	ECVF_Default);

namespace
{
	uint32 SeedFromCVar()
	{
		if (IConsoleVariable* Var = IConsoleManager::Get().FindConsoleVariable(TEXT("anastasis.WorldView.Seed")))
		{
			return static_cast<uint32>(Var->GetInt());
		}
		return AnastasisWorldView::ReferenceSeed;
	}
}

bool UAnastasisSimulationSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld();
}

void UAnastasisSimulationSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	ResetCanonical(SeedFromCVar());
	bPumpFromEngineTick = true;
}

void UAnastasisSimulationSubsystem::Deinitialize()
{
	// Le monde se defait : ses acteurs partent avec lui. On oublie seulement les liens.
	VillagePresentation = FAnastasisVillagePresentation();
	Super::Deinitialize();
}

void UAnastasisSimulationSubsystem::ResetCanonical(uint32 Seed)
{
	Simulation.Reset(Seed, AnastasisWorldView::ReferenceWidth, AnastasisWorldView::ReferenceHeight);
	LoggedDay = Simulation.GetDay();
	UE_LOG(
		LogAnastasis_UnrealV2,
		Display,
		TEXT("ANASTASIS_SIM reset seed=%u day=%d time=%.17g tiles=%d speed=%.4g"),
		Simulation.GetSeed(),
		Simulation.GetDay(),
		Simulation.GetTime(),
		Simulation.GetWorld().Tiles.Num(),
		CVarSimSpeed.GetValueOnGameThread());
}

void UAnastasisSimulationSubsystem::Tick(float DeltaTime)
{
	if (!Simulation.IsRunning())
	{
		return;
	}

	const double Speed = static_cast<double>(CVarSimSpeed.GetValueOnGameThread());
	Simulation.PumpFrame(static_cast<double>(DeltaTime), Speed);
	LogDayIfChanged();
	SyncVillagePresentation();
	if (CVarVillageDebug.GetValueOnGameThread() != 0)
	{
		FAnastasisVillagePresentation::DrawDebug(GetWorld(), Simulation.GetVillage(), Simulation.GetWorld());
	}
	DrawOverlay();
}

int32 UAnastasisSimulationSubsystem::SyncVillagePresentation()
{
	UWorld* World = GetWorld();
	UAnastasisVillageInteractionSubsystem* Rooms = World ? World->GetSubsystem<UAnastasisVillageInteractionSubsystem>() : nullptr;
	if (!Rooms || !Simulation.IsRunning())
	{
		return 0;
	}
	return VillagePresentation.Sync(Simulation.GetVillage(), Simulation.GetWorld(), *Rooms);
}

FString UAnastasisSimulationSubsystem::SeedFirstWell(int32 NpcCount, int32 TileX, int32 TileY)
{
	if (!Simulation.IsRunning())
	{
		return FString();
	}
	AnastasisVillage::FVillage& Village = Simulation.GetVillage();
	const AnastasisNav::FNavGrid& Nav = Village.GetNavGrid();

	auto FreeNeighbours = [&](int32 X, int32 Y)
	{
		int32 Count = 0;
		for (int32 DY = -1; DY <= 1; ++DY)
		{
			for (int32 DX = -1; DX <= 1; ++DX)
			{
				if ((DX || DY) && !Village.IsFootBlocked(X + DX + 0.5, Y + DY + 0.5)) ++Count;
			}
		}
		return Count;
	};

	// Anneaux croissants autour de la case demandee : le premier sol libre dont
	// au moins trois voisins le sont aussi (un puits sans seuil ne sert a rien).
	FString WellId;
	int32 WellX = 0;
	int32 WellY = 0;
	for (int32 Radius = 0; Radius <= 24 && WellId.IsEmpty(); ++Radius)
	{
		for (int32 DY = -Radius; DY <= Radius && WellId.IsEmpty(); ++DY)
		{
			for (int32 DX = -Radius; DX <= Radius && WellId.IsEmpty(); ++DX)
			{
				if (FMath::Max(FMath::Abs(DX), FMath::Abs(DY)) != Radius) continue;
				const int32 X = TileX + DX;
				const int32 Y = TileY + DY;
				if (X < 2 || Y < 2 || X > Nav.W - 3 || Y > Nav.H - 3) continue;
				if (Village.IsFootBlocked(X + 0.5, Y + 0.5) || FreeNeighbours(X, Y) < 3) continue;
				WellId = Village.AddBuilding(AnastasisVillage::WellType, X, Y, 1.0, Simulation.GetDay());
				WellX = X;
				WellY = Y;
			}
		}
	}
	if (WellId.IsEmpty())
	{
		UE_LOG(LogAnastasis_UnrealV2, Warning, TEXT("ANASTASIS_VILLAGE first well: no free tile near (%d,%d)"), TileX, TileY);
		return WellId;
	}

	// Habitants en couronne a ~7 cases, soifs echelonnees : le plus assoiffe part
	// tout de suite, les autres quand leur soif franchit le seuil.
	for (int32 K = 0; K < NpcCount; ++K)
	{
		const double Angle = 2.0 * UE_DOUBLE_PI * K / FMath::Max(1, NpcCount);
		const int32 CX = WellX + FMath::RoundToInt32(7.0 * FMath::Cos(Angle));
		const int32 CY = WellY + FMath::RoundToInt32(7.0 * FMath::Sin(Angle));
		bool bPlaced = false;
		for (int32 R = 0; R <= 6 && !bPlaced; ++R)
		{
			for (int32 DY = -R; DY <= R && !bPlaced; ++DY)
			{
				for (int32 DX = -R; DX <= R && !bPlaced; ++DX)
				{
					const int32 X = CX + DX;
					const int32 Y = CY + DY;
					if (FMath::Max(FMath::Abs(DX), FMath::Abs(DY)) != R || !Nav.IsInBounds(X, Y)) continue;
					if (Village.IsFootBlocked(X + 0.5, Y + 0.5)) continue;
					AnastasisNeeds::FNeeds Needs;
					Needs.Hunger = 10.0;
					Needs.Energy = 80.0;
					Needs.Social = 70.0;
					Needs.Leisure = 70.0;
					Needs.Hygiene = 60.0;
					Needs.Thirst = FMath::Max(5.0, 58.0 - 13.0 * K);
					Needs.Health = 90.0;
					Needs.Morale = 55.0;
					Village.SpawnNpc(X + 0.5, Y + 0.5, Needs, 4.0);
					bPlaced = true;
				}
			}
		}
	}

	SyncVillagePresentation();
	UE_LOG(
		LogAnastasis_UnrealV2,
		Display,
		TEXT("ANASTASIS_VILLAGE first well %s at tile (%d,%d), %d inhabitants"),
		*WellId,
		WellX,
		WellY,
		Village.GetActors().Num());
	FAnastasisVillagePresentation::LogStatus(Village, Simulation.GetTime());
	return WellId;
}

TStatId UAnastasisSimulationSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UAnastasisSimulationSubsystem, STATGROUP_Tickables);
}

bool UAnastasisSimulationSubsystem::IsTickable() const
{
	const UWorld* World = GetWorld();
	return bPumpFromEngineTick
		&& !IsTemplate()
		&& World
		&& World->IsGameWorld()
		&& Simulation.IsRunning();
}

void UAnastasisSimulationSubsystem::LogStatus() const
{
	UE_LOG(
		LogAnastasis_UnrealV2,
		Display,
		TEXT("ANASTASIS_SIM status seed=%u day=%d time=%.17g frac=%.17g newDays=%d tiles=%d running=%d"),
		Simulation.GetSeed(),
		Simulation.GetDay(),
		Simulation.GetTime(),
		Simulation.DayFrac(),
		Simulation.GetNewDayCount(),
		Simulation.GetWorld().Tiles.Num(),
		Simulation.IsRunning() ? 1 : 0);
}

void UAnastasisSimulationSubsystem::LogDayIfChanged()
{
	if (Simulation.GetDay() == LoggedDay)
	{
		return;
	}

	LoggedDay = Simulation.GetDay();
	UE_LOG(
		LogAnastasis_UnrealV2,
		Display,
		TEXT("ANASTASIS_SIM_DAY seed=%u day=%d time=%.17g newDays=%d"),
		Simulation.GetSeed(),
		Simulation.GetDay(),
		Simulation.GetTime(),
		Simulation.GetNewDayCount());
}

void UAnastasisSimulationSubsystem::DrawOverlay() const
{
	if (CVarSimOverlay.GetValueOnGameThread() == 0 || !GEngine)
	{
		return;
	}

	const double Hours = Simulation.DayFrac() * 24.0;
	const int32 Hour = static_cast<int32>(AnastasisJs::Floor(Hours));
	const int32 Minute = static_cast<int32>(AnastasisJs::Floor((Hours - static_cast<double>(Hour)) * 60.0));

	GEngine->AddOnScreenDebugMessage(
		0xA51A51,
		0.0f,
		FColor::Cyan,
		FString::Printf(
			TEXT("ANASTASIS  Jour %d  %02d:%02d  t=%.3f  seed=%u"),
			Simulation.GetDay(),
			Hour,
			Minute,
			Simulation.GetTime(),
			Simulation.GetSeed()));
}

static FAutoConsoleCommandWithWorld CmdAnastasisSimStatus(
	TEXT("Anastasis.Sim.Status"),
	TEXT("Logs the live simulation clock: seed, day, time, dayFrac, onNewDay count."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if (!World)
		{
			return;
		}
		if (UAnastasisSimulationSubsystem* Host = World->GetSubsystem<UAnastasisSimulationSubsystem>())
		{
			Host->LogStatus();
		}
		else
		{
			UE_LOG(LogAnastasis_UnrealV2, Warning, TEXT("ANASTASIS_SIM status: no subsystem in this world"));
		}
	}));

namespace
{
	UAnastasisSimulationSubsystem* VillageHost(UWorld* World)
	{
		UAnastasisSimulationSubsystem* Host = World ? World->GetSubsystem<UAnastasisSimulationSubsystem>() : nullptr;
		if (!Host || !Host->GetSimulation().IsRunning())
		{
			UE_LOG(LogAnastasis_UnrealV2, Warning, TEXT("ANASTASIS_VILLAGE: no running simulation in this world (PIE only)"));
			return nullptr;
		}
		return Host;
	}
}

static FAutoConsoleCommandWithWorldAndArgs CmdAnastasisVillageFirstWell(
	TEXT("Anastasis.Village.FirstWell"),
	TEXT("Anastasis.Village.FirstWell [NpcCount=4] [TileX] [TileY] - poses the first well in the simulation near a tile (default: settlement) and inhabitants around it."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (UAnastasisSimulationSubsystem* Host = VillageHost(World))
		{
			const AnastasisVillage::FPoint Settlement = Host->GetSimulation().GetVillage().GetSettlement();
			const int32 Count = Args.IsValidIndex(0) ? FCString::Atoi(*Args[0]) : 4;
			const int32 X = Args.IsValidIndex(1) ? FCString::Atoi(*Args[1]) : FMath::FloorToInt32(Settlement.X);
			const int32 Y = Args.IsValidIndex(2) ? FCString::Atoi(*Args[2]) : FMath::FloorToInt32(Settlement.Y);
			Host->SeedFirstWell(Count, X, Y);
		}
	}));

static FAutoConsoleCommandWithWorld CmdAnastasisVillageStatus(
	TEXT("Anastasis.Village.Status"),
	TEXT("Logs every simulated building (state, access points, users) and inhabitant (goal, needs, target, why)."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if (UAnastasisSimulationSubsystem* Host = VillageHost(World))
		{
			FAnastasisVillagePresentation::LogStatus(Host->GetSimulation().GetVillage(), Host->GetSimulation().GetTime());
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs CmdAnastasisVillageRemoveBuilding(
	TEXT("Anastasis.Village.RemoveBuilding"),
	TEXT("Anastasis.Village.RemoveBuilding <building-N> - removes a building from the simulation; its actor follows."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		UAnastasisSimulationSubsystem* Host = VillageHost(World);
		if (!Host || !Args.IsValidIndex(0))
		{
			return;
		}
		const bool bRemoved = Host->GetSimulation().GetVillage().RemoveBuilding(Args[0]);
		Host->SyncVillagePresentation();
		UE_LOG(LogAnastasis_UnrealV2, Display, TEXT("ANASTASIS_VILLAGE remove %s -> %s"), *Args[0], bRemoved ? TEXT("removed") : TEXT("unknown id"));
	}));

static FAutoConsoleCommandWithWorldAndArgs CmdAnastasisVillageRemoveNpc(
	TEXT("Anastasis.Village.RemoveNpc"),
	TEXT("Anastasis.Village.RemoveNpc <npc-N> - removes an inhabitant from the simulation, even mid-interaction."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		UAnastasisSimulationSubsystem* Host = VillageHost(World);
		if (!Host || !Args.IsValidIndex(0))
		{
			return;
		}
		const bool bRemoved = Host->GetSimulation().GetVillage().RemoveNpc(Args[0]);
		UE_LOG(LogAnastasis_UnrealV2, Display, TEXT("ANASTASIS_VILLAGE remove %s -> %s"), *Args[0], bRemoved ? TEXT("removed") : TEXT("unknown id"));
	}));
