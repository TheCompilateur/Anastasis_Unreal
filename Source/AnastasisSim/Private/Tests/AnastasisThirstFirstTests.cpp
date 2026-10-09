#include "Misc/AutomationTest.h"

#include "Life/AnastasisBonds.h"
#include "Life/AnastasisNeeds.h"
#include "Village/AnastasisVillage.h"
#include "World/AnastasisWorld.h"

#if WITH_DEV_AUTOMATION_TESTS

// soif-dabord-001 (ecart n°58) -- quand on meurt de soif, on boit d'abord ; boire assoiffe soulage.

namespace AnastasisThirstFirstTest
{
	using namespace AnastasisVillage;

	constexpr double Dt = 1.0 / 60.0;

	AnastasisWorld::FWorld MakeWorld()
	{
		AnastasisWorld::FWorld World;
		World.W = 40;
		World.H = 40;
		World.Tiles.SetNum(40 * 40);
		for (int32 Y = 0; Y < 40; ++Y)
		{
			for (int32 X = 0; X < 40; ++X)
			{
				AnastasisWorld::FTile& Tile = World.Tiles[Y * 40 + X];
				Tile.X = X;
				Tile.Y = Y;
				Tile.Type = AnastasisWorld::ETileType::Grass;
				Tile.Alt = 0.5;
				Tile.Wetness = 0.3;
			}
		}
		return World;
	}

	AnastasisNeeds::FNeeds Needs(double Thirst, double Hunger)
	{
		AnastasisNeeds::FNeeds N;
		N.Thirst = Thirst;
		N.Hunger = Hunger;
		N.Energy = 80.0;
		N.Health = 80.0;
		return N;
	}

	/** Un puits, un grenier vide, un habitant a soif `Thirst` et faim `Hunger`, `Seconds` simulees ; rend le corps. */
	FNpc Live(const AnastasisWorld::FWorld& World, bool bThirstFirst, double Thirst, double Hunger, double Seconds)
	{
		FVillage V;
		V.Bind(World);
		V.SetThirstFirstEnabled(bThirstFirst);
		V.AddBuilding(WellType, 20, 20);
		V.AddBuilding(GranaryType, 24, 20);
		const FString Id = V.SpawnNpc(14.5, 20.5, Needs(Thirst, Hunger));
		double Time = 0.0;
		const int32 Ticks = FMath::CeilToInt32(Seconds / Dt);
		for (int32 I = 0; I < Ticks; ++I)
		{
			Time += Dt;
			V.UpdateActors(Time, Dt);
		}
		const FNpc* N = V.FindNpc(Id);
		return N ? *N : FNpc();
	}

	bool HasRelief(const FNpc& N)
	{
		return N.Moodlets.ContainsByPredicate([](const AnastasisBonds::FMoodlet& M) { return M.Id == TEXT("drinkRelief"); });
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisThirstReliefTest,
	"Anastasis.Sim.Village.Soif.Recompense",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisThirstReliefTest::RunTest(const FString&)
{
	using namespace AnastasisThirstFirstTest;
	{
		// L'humeur elle-meme : posee, elle monte le moral et tire vers le puits, puis passe.
		TArray<AnastasisBonds::FMoodlet> List;
		double Morale = 50.0;
		AnastasisBonds::StampDrinkRelief(List, Morale, 10.0);
		TestEqual(TEXT("le soulagement monte le moral"), Morale, 50.0 + AnastasisBonds::DrinkReliefMoraleOnStamp);
		TestEqual(TEXT("... et tire vers le puits"), AnastasisBonds::DrinkReliefBias(List, 20.0), AnastasisBonds::DrinkReliefDrinkBias);
		TestEqual(TEXT("... un temps seulement"), AnastasisBonds::DrinkReliefBias(List, 10.0 + AnastasisBonds::DrinkReliefSeconds + 1.0), 0.0);
		AnastasisBonds::StampDrinkRelief(List, Morale, 30.0);
		TestEqual(TEXT("reboire prolonge, ne double pas la case"), List.Num(), 1);
	}
	const AnastasisWorld::FWorld World = MakeWorld();
	const FNpc On = Live(World, true, 60.0, 10.0, 20.0);
	TestTrue(TEXT("assoiffe : il va boire"), On.DrinksTaken >= 1);
	TestTrue(TEXT("... et boire le soulage (allume)"), HasRelief(On));
	const FNpc Off = Live(World, false, 60.0, 10.0, 20.0);
	TestTrue(TEXT("eteint : il boit aussi"), Off.DrinksTaken >= 1);
	TestFalse(TEXT("... mais sans soulagement (reference)"), HasRelief(Off));
	AddInfo(FString::Printf(TEXT("THIRST_RELIEF on drinks=%d morale=%.1f | off drinks=%d morale=%.1f"),
		On.DrinksTaken, On.Needs.Morale, Off.DrinksTaken, Off.Needs.Morale));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisThirstFirstTest,
	"Anastasis.Sim.Village.Soif.Dabord",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisThirstFirstTest::RunTest(const FString&)
{
	using namespace AnastasisThirstFirstTest;
	const AnastasisWorld::FWorld World = MakeWorld();
	// Mourant de soif ET affame, le grenier vide : la faim gagne la table, il faut pourtant boire.
	const FNpc On = Live(World, true, 92.0, 100.0, 20.0);
	TestTrue(TEXT("soif mortelle et faim : il boit d'abord"), On.DrinksTaken >= 1);
	TestTrue(TEXT("... et la soif retombe"), On.Needs.Thirst < 88.0);
	const FNpc Off = Live(World, false, 92.0, 100.0, 20.0);
	AddInfo(FString::Printf(TEXT("THIRST_FIRST on drinks=%d thirst=%.1f goal=%s | reference drinks=%d thirst=%.1f goal=%s gate=%s"),
		On.DrinksTaken, On.Needs.Thirst, *On.Goal, Off.DrinksTaken, Off.Needs.Thirst, *Off.Goal, *Off.LastDecision.CommitGate));
	// Sans soif mortelle, rien ne change : la regle ne touche que ceux qui meurent de soif.
	const FNpc Calm = Live(World, true, 20.0, 70.0, 5.0);
	const FNpc CalmRef = Live(World, false, 20.0, 70.0, 5.0);
	TestEqual(TEXT("soif ordinaire : meme but qu'eteint"), Calm.Goal, CalmRef.Goal);
	return true;
}

#endif
