#include "Misc/AutomationTest.h"

#include "Village/AnastasisVillage.h"
#include "World/AnastasisWorld.h"

#if WITH_DEV_AUTOMATION_TESTS

// La cadence du budget dans la boucle des habitants (mission budget-cadence-001).
//
// `consumeNpcSimulationCadence` est prouvee bit a bit depuis la couche 3
// (Anastasis.Sim.Parite.Budget). Ce test prouve son BRANCHEMENT : une fois la vue
// posee, l'habitant proche tourne a chaque tick avec `dt`, l'habitant en bande
// medium accumule `_simBudgetAccum` et ne tourne qu'au dixieme de seconde, avec le
// temps accumule ; sans vue (ecart n°5, reste), rien ne change.

namespace AnastasisVillageCadenceTest
{
	AnastasisWorld::FWorld MakeFlatWorld(int32 W, int32 H)
	{
		AnastasisWorld::FWorld World;
		World.W = W;
		World.H = H;
		World.Tiles.SetNum(W * H);
		for (int32 Y = 0; Y < H; ++Y)
		{
			for (int32 X = 0; X < W; ++X)
			{
				AnastasisWorld::FTile& Tile = World.Tiles[Y * W + X];
				Tile.X = X;
				Tile.Y = Y;
				Tile.Type = AnastasisWorld::ETileType::Grass;
				Tile.Alt = 0.5;
				Tile.Wetness = 0.3;
			}
		}
		return World;
	}

	AnastasisNeeds::FNeeds Calm()
	{
		AnastasisNeeds::FNeeds N;
		N.Hunger = 20.0;
		N.Thirst = 10.0;
		N.Energy = 85.0;
		N.Social = 80.0;
		N.Leisure = 80.0;
		N.Hygiene = 80.0;
		N.Health = 95.0;
		N.Morale = 60.0;
		return N;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisVillageCadenceTest,
	"Anastasis.Sim.Village.Cadence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisVillageCadenceTest::RunTest(const FString& Parameters)
{
	using namespace AnastasisVillageCadenceTest;
	constexpr double Dt = 1.0 / 60.0;
	const AnastasisWorld::FWorld World = MakeFlatWorld(64, 64);

	// 1. Vue posee en (16, 16) : un habitant a 0,7 case (near), un autre a 30,5 (medium).
	{
		AnastasisVillage::FVillage V;
		V.Bind(World);
		V.SetSimulationView(16.0, 16.0);
		const FString Near = V.SpawnNpc(16.5, 16.5, Calm());
		const FString Medium = V.SpawnNpc(16.5, 46.5, Calm());
		const double MediumHunger0 = V.FindNpc(Medium)->Needs.Hunger;
		double Time = 37.8;

		for (int32 Tick = 1; Tick <= 5; ++Tick)
		{
			const double NearHungerBefore = V.FindNpc(Near)->Needs.Hunger;
			Time += Dt;
			V.UpdateActors(Time, Dt);
			const AnastasisVillage::FNpc* N = V.FindNpc(Near);
			const AnastasisVillage::FNpc* M = V.FindNpc(Medium);
			TestTrue(FString::Printf(TEXT("tick %d : l'habitant near tourne"), Tick), N->Needs.Hunger != NearHungerBefore);
			TestFalse(FString::Printf(TEXT("tick %d : near n'ecrit pas _simBudgetAccum"), Tick), N->bHasSimBudgetAccum);
			TestEqual(FString::Printf(TEXT("tick %d : medium attend (faim inchangee)"), Tick), M->Needs.Hunger, MediumHunger0);
			TestTrue(FString::Printf(TEXT("tick %d : medium accumule"), Tick), M->bHasSimBudgetAccum && M->SimBudgetAccum > 0.0);
		}
		Time += Dt;
		V.UpdateActors(Time, Dt);
		const AnastasisVillage::FNpc* M = V.FindNpc(Medium);
		TestTrue(TEXT("tick 6 : 0,1 s accumulee, medium tourne"), M->Needs.Hunger != MediumHunger0);
		TestEqual(TEXT("tick 6 : l'accumulateur repart de 0"), M->SimBudgetAccum, 0.0);
		AddInfo(FString::Printf(TEXT("medium : faim %.6f -> %.6f au 6e tick (dt 0,1 s)"), MediumHunger0, M->Needs.Hunger));
	}

	// 2. Sans vue (ecart n°5, reste) : le meme habitant lointain tourne a chaque tick.
	{
		AnastasisVillage::FVillage V;
		V.Bind(World);
		const FString Medium = V.SpawnNpc(16.5, 46.5, Calm());
		double Time = 37.8;
		const double Before = V.FindNpc(Medium)->Needs.Hunger;
		Time += Dt;
		V.UpdateActors(Time, Dt);
		TestTrue(TEXT("sans vue : tourne des le premier tick"), V.FindNpc(Medium)->Needs.Hunger != Before);
		TestFalse(TEXT("sans vue : pas d'accumulateur"), V.FindNpc(Medium)->bHasSimBudgetAccum);
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
