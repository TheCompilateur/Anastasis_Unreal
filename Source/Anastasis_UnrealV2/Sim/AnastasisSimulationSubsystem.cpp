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

// Default 1 since SKY_TRANSITIONS_001 (2026-09-30), the JS reference's realtime: one day = 90 s.
// At the former default of 10, now that the sky follows the simulation, a day lasted ~12 real
// seconds and the sun swept the sky at ~30 deg/s -- Alexandre: "on dirait la magie". Proofs that
// need a fast night set the speed themselves (gather-deliver-pie.py does: 'anastasis.Sim.Speed 10');
// those that only wait for night still find it within their 240 s timeout (night falls ~41 s in).
static TAutoConsoleVariable<float> CVarSimSpeed(
	TEXT("anastasis.Sim.Speed"),
	1.0f,
	TEXT("Simulation speed scale (JS 1/2/5/10). Default 1 = JS realtime, a 90 s day. Set 10 in a proof that needs midnight quickly."),
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
					// Le seuil de soif suit la phase (~40 la nuit et a midi, ~64 le matin) :
					// le plus assoiffe part a toute heure, les autres a leur tour.
					Needs.Thirst = FMath::Max(5.0, 80.0 - 12.0 * K);
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

FString UAnastasisSimulationSubsystem::SeedFirstHouse(int32 NpcCount, int32 TileX, int32 TileY)
{
	if (!Simulation.IsRunning())
	{
		return FString();
	}
	AnastasisVillage::FVillage& Village = Simulation.GetVillage();
	const AnastasisNav::FNavGrid& Nav = Village.GetNavGrid();

	// Premiere case libre, en anneaux, dont la porte vers le camp est libre aussi.
	auto PlaceHouseNear = [&](int32 CX, int32 CY, int32& OutX, int32& OutY)
	{
		for (int32 Radius = 0; Radius <= 24; ++Radius)
		{
			for (int32 DY = -Radius; DY <= Radius; ++DY)
			{
				for (int32 DX = -Radius; DX <= Radius; ++DX)
				{
					if (FMath::Max(FMath::Abs(DX), FMath::Abs(DY)) != Radius) continue;
					const int32 X = CX + DX;
					const int32 Y = CY + DY;
					if (X < 2 || Y < 2 || X > Nav.W - 3 || Y > Nav.H - 3) continue;
					if (Village.IsFootBlocked(X + 0.5, Y + 0.5)) continue;
					if (Village.IsFootBlocked(X + 1.5, Y + 0.5) && Village.IsFootBlocked(X - 0.5, Y + 0.5)) continue;
					const FString Id = Village.AddBuilding(AnastasisVillage::HouseType, X, Y, 1.0, Simulation.GetDay());
					if (!Id.IsEmpty())
					{
						OutX = X;
						OutY = Y;
						return Id;
					}
				}
			}
		}
		return FString();
	};

	int32 HX = 0;
	int32 HY = 0;
	int32 FX = 0;
	int32 FY = 0;
	const FString Owned = PlaceHouseNear(TileX, TileY, HX, HY);
	const FString Free = PlaceHouseNear(TileX + 6, TileY, FX, FY);
	if (Owned.IsEmpty())
	{
		UE_LOG(LogAnastasis_UnrealV2, Warning, TEXT("ANASTASIS_VILLAGE first house: no free tile near (%d,%d)"), TileX, TileY);
		return Owned;
	}

	// Habitants autour, fatigues a des degres divers.
	for (int32 K = 0; K < NpcCount; ++K)
	{
		const double Angle = 2.0 * UE_DOUBLE_PI * K / FMath::Max(1, NpcCount);
		const int32 CX = HX + FMath::RoundToInt32(6.0 * FMath::Cos(Angle));
		const int32 CY = HY + FMath::RoundToInt32(6.0 * FMath::Sin(Angle));
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
					Needs.Energy = FMath::Max(8.0, 70.0 - 20.0 * K);
					Needs.Social = 70.0;
					Needs.Leisure = 70.0;
					Needs.Hygiene = 60.0;
					Needs.Thirst = 10.0;
					Needs.Health = 90.0;
					Needs.Morale = 55.0;
					const FString Id = Village.SpawnNpc(X + 0.5, Y + 0.5, Needs, 4.0);
					if (K == 0)
					{
						Village.AssignHome(Id, Owned);
					}
					bPlaced = true;
				}
			}
		}
	}
	// Ce que ferait le prochain minuit : les sans-toit recoivent un lit.
	const int32 Sheltered = Village.AssignSheltersDaily();

	SyncVillagePresentation();
	UE_LOG(
		LogAnastasis_UnrealV2,
		Display,
		TEXT("ANASTASIS_VILLAGE first house %s at (%d,%d) owned, %s at (%d,%d) free, %d inhabitants, %d sheltered"),
		*Owned,
		HX,
		HY,
		Free.IsEmpty() ? TEXT("-") : *Free,
		FX,
		FY,
		Village.GetActors().Num(),
		Sheltered);
	FAnastasisVillagePresentation::LogStatus(Village, Simulation.GetTime());
	return Owned;
}

FString UAnastasisSimulationSubsystem::SeedFirstGranary(int32 NpcCount, int32 Food, int32 TileX, int32 TileY)
{
	if (!Simulation.IsRunning())
	{
		return FString();
	}
	AnastasisVillage::FVillage& Village = Simulation.GetVillage();
	const AnastasisNav::FNavGrid& Nav = Village.GetNavGrid();

	FString GranaryId;
	int32 GX = 0;
	int32 GY = 0;
	for (int32 Radius = 0; Radius <= 24 && GranaryId.IsEmpty(); ++Radius)
	{
		for (int32 DY = -Radius; DY <= Radius && GranaryId.IsEmpty(); ++DY)
		{
			for (int32 DX = -Radius; DX <= Radius && GranaryId.IsEmpty(); ++DX)
			{
				if (FMath::Max(FMath::Abs(DX), FMath::Abs(DY)) != Radius) continue;
				const int32 X = TileX + DX;
				const int32 Y = TileY + DY;
				if (X < 2 || Y < 2 || X > Nav.W - 3 || Y > Nav.H - 3) continue;
				if (Village.IsFootBlocked(X + 0.5, Y + 0.5)) continue;
				if (Village.IsFootBlocked(X + 1.5, Y + 0.5) && Village.IsFootBlocked(X - 0.5, Y + 0.5)) continue;
				GranaryId = Village.AddBuilding(AnastasisVillage::GranaryType, X, Y, 1.0, Simulation.GetDay());
				GX = X;
				GY = Y;
			}
		}
	}
	if (GranaryId.IsEmpty())
	{
		UE_LOG(LogAnastasis_UnrealV2, Warning, TEXT("ANASTASIS_VILLAGE first granary: no free tile near (%d,%d)"), TileX, TileY);
		return GranaryId;
	}
	const int32 Stored = Village.CreditFood(GranaryId, Food);

	for (int32 K = 0; K < NpcCount; ++K)
	{
		const double Angle = 2.0 * UE_DOUBLE_PI * K / FMath::Max(1, NpcCount);
		const int32 CX = GX + FMath::RoundToInt32(5.0 * FMath::Cos(Angle));
		const int32 CY = GY + FMath::RoundToInt32(5.0 * FMath::Sin(Angle));
		bool bPlaced = false;
		for (int32 R = 0; R <= 2 && !bPlaced; ++R)
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
					Needs.Hunger = FMath::Max(10.0, 80.0 - 15.0 * K);
					Needs.Energy = 80.0;
					Needs.Social = 70.0;
					Needs.Leisure = 70.0;
					Needs.Hygiene = 60.0;
					Needs.Thirst = 10.0;
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
		TEXT("ANASTASIS_VILLAGE first granary %s at (%d,%d), food=%d, %d inhabitants"),
		*GranaryId,
		GX,
		GY,
		Stored,
		Village.GetActors().Num());
	FAnastasisVillagePresentation::LogStatus(Village, Simulation.GetTime());
	return GranaryId;
}

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

static FAutoConsoleCommandWithWorldAndArgs CmdAnastasisVillageFirstHouse(
	TEXT("Anastasis.Village.FirstHouse"),
	TEXT("Anastasis.Village.FirstHouse [NpcCount=4] [TileX] [TileY] - poses an owned house and a free one near a tile (default: settlement), and inhabitants: one owner, the others sheltered."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (UAnastasisSimulationSubsystem* Host = VillageHost(World))
		{
			const AnastasisVillage::FPoint Settlement = Host->GetSimulation().GetVillage().GetSettlement();
			const int32 Count = Args.IsValidIndex(0) ? FCString::Atoi(*Args[0]) : 4;
			const int32 X = Args.IsValidIndex(1) ? FCString::Atoi(*Args[1]) : FMath::FloorToInt32(Settlement.X);
			const int32 Y = Args.IsValidIndex(2) ? FCString::Atoi(*Args[2]) : FMath::FloorToInt32(Settlement.Y);
			Host->SeedFirstHouse(Count, X, Y);
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs CmdAnastasisVillageFirstGranary(
	TEXT("Anastasis.Village.FirstGranary"),
	TEXT("Anastasis.Village.FirstGranary [NpcCount=4] [Food=12] [TileX] [TileY] - poses a granary filled with food near a tile (default: settlement) and homeless, hungry inhabitants who can see it."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (UAnastasisSimulationSubsystem* Host = VillageHost(World))
		{
			const AnastasisVillage::FPoint Settlement = Host->GetSimulation().GetVillage().GetSettlement();
			const int32 Count = Args.IsValidIndex(0) ? FCString::Atoi(*Args[0]) : 4;
			const int32 Food = Args.IsValidIndex(1) ? FCString::Atoi(*Args[1]) : 12;
			const int32 X = Args.IsValidIndex(2) ? FCString::Atoi(*Args[2]) : FMath::FloorToInt32(Settlement.X);
			const int32 Y = Args.IsValidIndex(3) ? FCString::Atoi(*Args[3]) : FMath::FloorToInt32(Settlement.Y);
			Host->SeedFirstGranary(Count, Food, X, Y);
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

namespace
{
	const FAnastasisSimulation* DebugSimulation(const UObject* WorldContextObject)
	{
		const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
		const UAnastasisSimulationSubsystem* Host = World ? World->GetSubsystem<UAnastasisSimulationSubsystem>() : nullptr;
		return Host && Host->GetSimulation().IsRunning() ? &Host->GetSimulation() : nullptr;
	}
}

double UAnastasisSimulationDebugLibrary::GetSimulationTime(const UObject* WorldContextObject)
{
	const FAnastasisSimulation* Sim = DebugSimulation(WorldContextObject);
	return Sim ? Sim->GetTime() : -1.0;
}

FString UAnastasisSimulationDebugLibrary::GetVillagePhase(const UObject* WorldContextObject)
{
	const FAnastasisSimulation* Sim = DebugSimulation(WorldContextObject);
	return Sim ? FString(AnastasisRhythm::PhaseId(AnastasisRhythm::VillagePhase(AnastasisRhythm::DayFracOf(Sim->GetTime())))) : FString();
}

int32 UAnastasisSimulationDebugLibrary::CountInside(const UObject* WorldContextObject, const FString& BuildingId)
{
	const FAnastasisSimulation* Sim = DebugSimulation(WorldContextObject);
	return Sim ? Sim->GetVillage().InsideOf(BuildingId).Num() : -1;
}

int32 UAnastasisSimulationDebugLibrary::GetFoodStock(const UObject* WorldContextObject, const FString& BuildingId)
{
	const FAnastasisSimulation* Sim = DebugSimulation(WorldContextObject);
	const AnastasisVillage::FBuilding* B = Sim ? Sim->GetVillage().FindBuilding(BuildingId) : nullptr;
	return B ? B->FoodPhysical : -1;
}

int32 UAnastasisSimulationDebugLibrary::CountMealsTaken(const UObject* WorldContextObject)
{
	const FAnastasisSimulation* Sim = DebugSimulation(WorldContextObject);
	if (!Sim) return -1;
	int32 Meals = 0;
	for (const AnastasisVillage::FNpc& N : Sim->GetVillage().GetActors()) Meals += N.MealsTaken;
	return Meals;
}

// One opt-in circuit. Uses an actual Food tile near the settlement, an empty
// granary and one healthy adult. Repeating the command cannot inject/refill food.
bool UAnastasisSimulationSubsystem::SeedFoodSupply()
{
	using namespace AnastasisVillage;
	if (!Simulation.IsRunning()) return false;
	auto& V = Simulation.GetVillage();
	if (!V.GetFoodSources().IsEmpty() || !V.GetActors().IsEmpty() || !V.GetBuildings().IsEmpty())
	{
		UE_LOG(LogAnastasis_UnrealV2, Warning, TEXT("FOOD_SUPPLY refused: start in an empty village; existing state preserved"));
		return false;
	}
	const auto& W = Simulation.GetWorld();
	TArray<int32> Candidates;
	for (int32 I=0; I<W.Tiles.Num(); ++I)
	{
		const auto& T = W.Tiles[I];
		if (T.Resource == AnastasisWorld::EResource::Food && T.Amount > 0 && !V.IsFootBlocked(T.X+0.5,T.Y+0.5)) Candidates.Add(I);
	}
	const auto Center=V.GetSettlement();
	Candidates.StableSort([&](int32 A, int32 B)
	{
		const auto& X=W.Tiles[A]; const auto& Y=W.Tiles[B];
		return FMath::Square(X.X-Center.X)+FMath::Square(X.Y-Center.Y) < FMath::Square(Y.X-Center.X)+FMath::Square(Y.Y-Center.Y);
	});
	for (int32 I:Candidates)
	{
		const auto& T=W.Tiles[I];
		for (int32 DY=-4; DY<=4; ++DY) for (int32 DX=-4; DX<=4; ++DX)
		{
			if (FMath::Max(FMath::Abs(DX),FMath::Abs(DY))!=4) continue;
			const int32 X=T.X+DX,Y=T.Y+DY;
			if (V.IsFootBlocked(X+0.5,Y+0.5)) continue;
			const FString B=V.AddBuilding(GranaryType,X,Y);
			if(B.IsEmpty()) continue;
			bool bReachable=false;
			FPoint Start;
			const AnastasisPath::FWorldNavSource Nav(V.GetNavGrid(),W);
			for(const auto& Door:V.FindBuilding(B)->AccessPoints)
			{
				TArray<FPoint> Path;
				if(AnastasisPath::FindPath(Nav,Door,{T.X+0.5,T.Y+0.5},{},Path)) {Start=Door;bReachable=true;break;}
			}
			if(!bReachable) {V.RemoveBuilding(B);continue;}
			V.ActivateFoodSource(T.X,T.Y);
			V.SetSettlement(Start.X,Start.Y);
			AnastasisNeeds::FNeeds N; N.Hunger=10; N.Energy=95; N.Hygiene=95; N.Social=95; N.Leisure=95;
			V.SpawnNpc(Start.X,Start.Y,N);
			SyncVillagePresentation();
			UE_LOG(LogAnastasis_UnrealV2,Display,TEXT("FOOD_SUPPLY ready source=(%d,%d) initial=%d depot=%s empty=1"),T.X,T.Y,T.Amount,*B);
			FAnastasisVillagePresentation::LogStatus(V,Simulation.GetTime());
			return true;
		}
	}
	UE_LOG(LogAnastasis_UnrealV2,Warning,TEXT("FOOD_SUPPLY no reachable generated source"));
	return false;
}

static FAutoConsoleCommandWithWorld CmdAnastasisFoodSupply(
	TEXT("Anastasis.Village.FoodSupply"),
	TEXT("Start one finite food circuit in an empty village: generated resource, empty granary, autonomous inhabitant. No refill."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if(auto* Host=World ? World->GetSubsystem<UAnastasisSimulationSubsystem>() : nullptr) Host->SeedFoodSupply();
	}));

FString UAnastasisSimulationDebugLibrary::GetFoodSupplyStatus(const UObject* WorldContextObject)
{
	const auto* Sim=DebugSimulation(WorldContextObject);
	if(!Sim) return TEXT("{}");
	const auto& V=Sim->GetVillage();
	int32 Initial=0,Remaining=0,Bag=0,Stored=0,Meals=0,Gathered=0,Delivered=0;
	for(const auto& S:V.GetFoodSources()) {Initial+=S.Initial;Remaining+=S.Remaining;}
	for(const auto& B:V.GetBuildings()) Stored+=B.FoodPhysical;
	for(const auto& N:V.GetActors()) {Bag+=N.InventoryFood;Meals+=N.MealsTaken;Gathered+=N.GatheredFood;Delivered+=N.DeliveredFood;}
	const auto* First = V.GetActors().IsEmpty() ? nullptr : &V.GetActors()[0];
	return FString::Printf(TEXT("{\"x\":%.5f,\"y\":%.5f,\"time\":%.4f,\"initial\":%d,\"remaining\":%d,\"bag\":%d,\"stock\":%d,\"meals\":%d,\"gathered\":%d,\"delivered\":%d}"),First ? First->X : 0.0,First ? First->Y : 0.0,Sim->GetTime(),Initial,Remaining,Bag,Stored,Meals,Gathered,Delivered);
}

// Le fermier au grenier (gather-deliver-001). Un champ genere, un grenier vide a
// 3-4 cases dont un seuil atteint le champ, des fermiers embauches au seuil.
FString UAnastasisSimulationSubsystem::SeedFirstFarmer(int32 FarmerCount, int32 TileX, int32 TileY)
{
	using namespace AnastasisVillage;
	if (!Simulation.IsRunning())
	{
		return FString();
	}
	FVillage& V = Simulation.GetVillage();
	const AnastasisWorld::FWorld& W = Simulation.GetWorld();
	TArray<int32> Fields;
	for (int32 I = 0; I < W.Tiles.Num(); ++I)
	{
		const AnastasisWorld::FTile Tile = V.LiveTileAt(W.Tiles[I].X, W.Tiles[I].Y);
		if (Tile.Resource == AnastasisWorld::EResource::Food && Tile.Amount > 0 && !V.IsFootBlocked(Tile.X + 0.5, Tile.Y + 0.5)) Fields.Add(I);
	}
	Fields.StableSort([&](int32 A, int32 B)
	{
		const AnastasisWorld::FTile& TA = W.Tiles[A];
		const AnastasisWorld::FTile& TB = W.Tiles[B];
		return FMath::Square(TA.X - TileX) + FMath::Square(TA.Y - TileY) < FMath::Square(TB.X - TileX) + FMath::Square(TB.Y - TileY);
	});
	const AnastasisPath::FWorldNavSource NavSource(V.GetNavGrid(), W);
	for (const int32 FieldIndex : Fields)
	{
		const AnastasisWorld::FTile& Field = W.Tiles[FieldIndex];
		for (int32 R = 3; R <= 4; ++R)
		{
			for (int32 DY = -R; DY <= R; ++DY)
			{
				for (int32 DX = -R; DX <= R; ++DX)
				{
					if (FMath::Max(FMath::Abs(DX), FMath::Abs(DY)) != R) continue;
					const int32 X = Field.X + DX;
					const int32 Y = Field.Y + DY;
					if (X < 2 || Y < 2 || X > W.W - 3 || Y > W.H - 3) continue;
					// Pas sur un champ : le grenier ne mange pas la recolte.
					if (V.LiveTileAt(X, Y).Resource != AnastasisWorld::EResource::None || V.IsFootBlocked(X + 0.5, Y + 0.5)) continue;
					const FString Granary = V.AddBuilding(GranaryType, X, Y, 1.0, Simulation.GetDay());
					if (Granary.IsEmpty()) continue;
					const FPoint* Door = nullptr;
					for (const FPoint& P : V.FindBuilding(Granary)->AccessPoints)
					{
						TArray<FPoint> Path;
						if (AnastasisPath::FindPath(NavSource, P, { Field.X + 0.5, Field.Y + 0.5 }, {}, Path))
						{
							Door = &P;
							break;
						}
					}
					if (!Door)
					{
						V.RemoveBuilding(Granary);
						continue;
					}
					const FPoint Start = *Door;
					for (int32 K = 0; K < FMath::Max(1, FarmerCount); ++K)
					{
						AnastasisNeeds::FNeeds N;
						N.Hunger = 10.0;
						N.Energy = 90.0;
						N.Social = 80.0;
						N.Leisure = 80.0;
						N.Hygiene = 80.0;
						N.Thirst = 5.0;
						N.Health = 95.0;
						N.Morale = 60.0;
						const FString Id = V.SpawnNpc(Start.X, Start.Y, N);
						V.AssignWorkplace(Id, AnastasisGather::JobFarmer, Granary);
					}
					FarmerGranaryId = Granary;
					FarmerField = FIntPoint(Field.X, Field.Y);
					SyncVillagePresentation();
					UE_LOG(LogAnastasis_UnrealV2, Display,
						TEXT("ANASTASIS_VILLAGE first farmer granary=%s at (%d,%d) field=(%d,%d) food=%d farmers=%d"),
						*Granary, X, Y, Field.X, Field.Y, V.LiveTileAt(Field.X, Field.Y).Amount, FMath::Max(1, FarmerCount));
					FAnastasisVillagePresentation::LogStatus(V, Simulation.GetTime());
					return Granary;
				}
			}
		}
	}
	UE_LOG(LogAnastasis_UnrealV2, Warning, TEXT("ANASTASIS_VILLAGE first farmer: no reachable generated field near (%d,%d)"), TileX, TileY);
	return FString();
}

static FAutoConsoleCommandWithWorldAndArgs CmdAnastasisVillageFirstFarmer(
	TEXT("Anastasis.Village.FirstFarmer"),
	TEXT("Anastasis.Village.FirstFarmer [FarmerCount=1] [TileX] [TileY] - an empty granary near the generated field closest to a tile (default: settlement), and farmers hired there who gather then deliver."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (UAnastasisSimulationSubsystem* Host = VillageHost(World))
		{
			const AnastasisVillage::FPoint Settlement = Host->GetSimulation().GetVillage().GetSettlement();
			const int32 Count = Args.IsValidIndex(0) ? FCString::Atoi(*Args[0]) : 1;
			const int32 X = Args.IsValidIndex(1) ? FCString::Atoi(*Args[1]) : FMath::FloorToInt32(Settlement.X);
			const int32 Y = Args.IsValidIndex(2) ? FCString::Atoi(*Args[2]) : FMath::FloorToInt32(Settlement.Y);
			Host->SeedFirstFarmer(Count, X, Y);
		}
	}));

FString UAnastasisSimulationDebugLibrary::GetGatherStatus(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	const UAnastasisSimulationSubsystem* Host = World ? World->GetSubsystem<UAnastasisSimulationSubsystem>() : nullptr;
	if (!Host || !Host->GetSimulation().IsRunning()) return TEXT("{}");
	const FAnastasisSimulation& Sim = Host->GetSimulation();
	const AnastasisVillage::FVillage& V = Sim.GetVillage();
	const AnastasisWorld::FWorld& W = Sim.GetWorld();
	const AnastasisVillage::FBuilding* Granary = V.FindBuilding(Host->GetFarmerGranaryId());
	if (!Granary) return TEXT("{\"granary\":-1}");
	int32 Field = 0;
	for (const AnastasisWorld::FTile& Generated : W.Tiles)
	{
		const AnastasisWorld::FTile Tile = V.LiveTileAt(Generated.X, Generated.Y);
		if (Tile.Resource == AnastasisWorld::EResource::Food) Field += Tile.Amount;
	}
	int32 Bag = 0, Stock = 0, Meals = 0, Deliveries = 0;
	for (const AnastasisVillage::FBuilding& B : V.GetBuildings()) Stock += B.FoodPhysical;
	const AnastasisVillage::FNpc* Farmer = nullptr;
	for (const AnastasisVillage::FNpc& N : V.GetActors())
	{
		Bag += N.InventoryFood;
		Meals += N.MealsTaken;
		if (N.WorkplaceId == Granary->Id)
		{
			Deliveries += N.Deliveries;
			if (!Farmer) Farmer = &N;
		}
	}
	UWorld* PresentationWorld = const_cast<UWorld*>(World);
	const FVector G = FAnastasisVillagePresentation::SimToUnreal(W, Granary->X + 0.5, Granary->Y + 0.5, PresentationWorld);
	const FIntPoint FieldTile = Host->GetFarmerField();
	const FVector F = FAnastasisVillagePresentation::SimToUnreal(W, FieldTile.X + 0.5, FieldTile.Y + 0.5, PresentationWorld);
	const FVector N = Farmer ? FAnastasisVillagePresentation::SimToUnreal(W, Farmer->X, Farmer->Y, PresentationWorld) : G;
	return FString::Printf(
		TEXT("{\"time\":%.4f,\"granary\":%d,\"field\":%d,\"bag\":%d,\"stock\":%d,\"meals\":%d,\"deliveries\":%d,")
		TEXT("\"goal\":\"%s\",\"activity\":\"%s\",\"session\":%s,")
		TEXT("\"gx\":%.1f,\"gy\":%.1f,\"gz\":%.1f,\"fx\":%.1f,\"fy\":%.1f,\"fz\":%.1f,\"nx\":%.1f,\"ny\":%.1f,\"nz\":%.1f}"),
		Sim.GetTime(), Granary->FoodPhysical, Field, Bag, Stock, Meals, Deliveries,
		Farmer ? *Farmer->Goal : TEXT(""), Farmer ? *Farmer->Activity : TEXT(""),
		Farmer && Farmer->WorkSession.bActive ? TEXT("true") : TEXT("false"),
		G.X, G.Y, G.Z, F.X, F.Y, F.Z, N.X, N.Y, N.Z);
}
