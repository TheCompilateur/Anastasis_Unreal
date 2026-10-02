#include "Misc/AutomationTest.h"

#include "Village/AnastasisVillage.h"
#include "Work/AnastasisFields.h"
#include "Work/AnastasisGather.h"
#include "World/AnastasisWorld.h"

#if WITH_DEV_AUTOMATION_TESTS

// Soigner une parcelle : le but `helpFarm` (mission help-farm-001).
//
// Dans la reference, sans ferme la ligne `helpFarm` vaut 0 et c'est le plancher collectif (28 sur
// `endurance`) qui la fait gagner ; l'habitant va a la parcelle la plus faible (`findTendFieldNear`),
// y ouvre une session `tend` et la fait repousser coup apres coup (`progressTendWork`, 4 coups au
// plus). Le planificateur n'est pas encore porte : le plancher est injecte.

namespace AnastasisVillageHelpFarmTest
{
	using namespace AnastasisVillage;

	AnastasisWorld::FWorld MakeWorld()
	{
		AnastasisWorld::FWorld World;
		World.W = 40;
		World.H = 32;
		World.Tiles.SetNum(World.W * World.H);
		for (int32 Y = 0; Y < World.H; ++Y)
		{
			for (int32 X = 0; X < World.W; ++X)
			{
				AnastasisWorld::FTile& Tile = World.Tiles[Y * World.W + X];
				Tile.X = X;
				Tile.Y = Y;
				Tile.Type = AnastasisWorld::ETileType::Grass;
				Tile.Alt = 0.5;
				Tile.Wetness = 0.3;
			}
		}
		// Une parcelle faible (5 vivres sur 37) a quelques cases du grenier.
		for (int32 X = 10; X <= 11; ++X)
		{
			AnastasisWorld::FTile& Tile = World.Tiles[11 * World.W + X];
			Tile.Type = AnastasisWorld::ETileType::Field;
			Tile.Resource = AnastasisWorld::EResource::Food;
			Tile.Amount = 5;
			Tile.CropId = AnastasisWorld::ECropId::Grain;
		}
		return World;
	}

	AnastasisNeeds::FNeeds Rested()
	{
		AnastasisNeeds::FNeeds N;
		N.Hunger = 15.0;
		N.Thirst = 10.0;
		N.Energy = 90.0;
		N.Social = 85.0;
		N.Leisure = 85.0;
		N.Hygiene = 85.0;
		N.Health = 95.0;
		N.Morale = 60.0;
		return N;
	}

	int32 PlotFood(const FVillage& Village)
	{
		return Village.LiveTileAt(10, 11).Amount + Village.LiveTileAt(11, 11).Amount;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisVillageHelpFarmTest,
	"Anastasis.Sim.Village.SoinsDeParcelle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisVillageHelpFarmTest::RunTest(const FString& Parameters)
{
	using namespace AnastasisVillageHelpFarmTest;
	namespace G = AnastasisGather;
	const AnastasisWorld::FWorld World = MakeWorld();
	FVillage Village;
	Village.Bind(World);
	const FString Granary = Village.AddBuilding(GranaryType, 16, 11);
	const FString Farmer = Village.SpawnNpc(14.5, 11.5, Rested());
	TestTrue(TEXT("fermier au grenier"), Village.AssignWorkplace(Farmer, G::JobFarmer, Granary));
	// Le plancher collectif de `helpFarm`, plus haut que toute la table de cet habitant repose.
	Village.CollectiveDecisionOverride = [](const FNpc&)
	{
		FCollectiveDecision C;
		C.GoalFloor.Add(GoalHelpFarm, 500.0);
		return C;
	};

	// Matin du jour 1 (la journee dure 90 s).
	const double Dt = 1.0 / 60.0;
	double Time = 90.0 * 0.35;
	const int32 FoodBefore = PlotFood(Village);
	bool bChose = false;
	bool bSession = false;
	bool bOnPlot = false;
	for (int32 Tick = 0; Tick < 60 * 40; ++Tick)
	{
		Time += Dt;
		Village.UpdateActors(Time, Dt);
		const FNpc* N = Village.FindNpc(Farmer);
		if (!N) break;
		if (N->Goal == GoalHelpFarm)
		{
			bChose = true;
			if (N->bHasTarget && FMath::FloorToInt(N->Target.Y) == 11
				&& (FMath::FloorToInt(N->Target.X) == 10 || FMath::FloorToInt(N->Target.X) == 11)) bOnPlot = true;
		}
		if (N->WorkSession.bActive && N->WorkSession.CraftId == TEXT("tend")) bSession = true;
		if (N->DeedsHelped >= 4) break;
	}
	const FNpc* N = Village.FindNpc(Farmer);
	if (!N)
	{
		AddError(TEXT("le fermier a disparu"));
		return false;
	}
	TestTrue(TEXT("sous le plancher collectif, le fermier choisit de soigner la parcelle"), bChose);
	TestTrue(TEXT("sa cible est un poste de la parcelle faible"), bOnPlot);
	TestTrue(TEXT("il ouvre une session de soins (tend)"), bSession);
	TestTrue(FString::Printf(TEXT("la parcelle repousse (%d -> %d)"), FoodBefore, PlotFood(Village)), PlotFood(Village) > FoodBefore);
	TestTrue(FString::Printf(TEXT("ses soins comptent (deeds.helped = %d)"), N->DeedsHelped), N->DeedsHelped >= 2);
	// Un coup soigne `fieldSeasonTendAmount(2, jour)`, ajuste par la fertilite (1 ici) : au jour 1, printemps.
	const int32 PerSwing = G::FieldSeasonTendAmount(2, 1);
	TestEqual(TEXT("chaque coup qui soigne ajoute fieldSeasonTendAmount(2, 1)"), (PlotFood(Village) - FoodBefore) % PerSwing, 0);
	AddInfo(FString::Printf(TEXT("parcelle %d -> %d, %d par coup, deeds.helped %d"), FoodBefore, PlotFood(Village), PerSwing, N->DeedsHelped));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
