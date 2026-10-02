#include "Misc/AutomationTest.h"

#include "Village/AnastasisVillage.h"
#include "World/AnastasisWorld.h"

#if WITH_DEV_AUTOMATION_TESTS

// Le mode de vie dans la boucle du village (mission lifestyle-wiring-001).
//
// `lifestyleDailyUpdate` est prouve bit a bit hors du village (Anastasis.Sim.Parite.ModeDeVie).
// Ce test prouve son BRANCHEMENT dans `FVillage::UpdateNpc` : une fois par jour au plus, le score
// de regularite monte de 1 si le but du moment suit le mode de vie, descend de 0,2 sinon, et le
// jour est note ; un habitant sans mode de vie (cree par le C++, ecart n°8) n'en recoit pas.

namespace AnastasisVillageLifestyleTest
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

	AnastasisVillage::FNpc MakeNpc(const TCHAR* Id, double X, const TCHAR* Goal, const TCHAR* Lifestyle, double RhythmScore)
	{
		AnastasisVillage::FNpc N;
		N.Id = Id;
		N.X = X;
		N.Y = 10.5;
		N.Goal = Goal;
		N.Needs.Hunger = 20.0;
		N.Needs.Thirst = 10.0;
		N.Needs.Energy = 85.0;
		N.Needs.Social = 80.0;
		N.Needs.Leisure = 80.0;
		N.Needs.Hygiene = 80.0;
		N.Needs.Health = 95.0;
		N.Needs.Morale = 60.0;
		if (Lifestyle)
		{
			AnastasisLifestyle::FLifestyle& L = N.Lifestyle.Emplace();
			L.Id = Lifestyle;
			L.SinceDay = 1.0;
			L.RhythmScore = RhythmScore;
			L.LastNotedDay = 0.0;
		}
		return N;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisVillageLifestyleTest,
	"Anastasis.Sim.Village.ModeDeVie",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisVillageLifestyleTest::RunTest(const FString& Parameters)
{
	using namespace AnastasisVillageLifestyleTest;
	constexpr double Dt = 1.0 / 60.0;
	const AnastasisWorld::FWorld World = MakeFlatWorld(48, 48);

	// W : travailleur acharne au travail (aligne). E : leve-tot qui regarde (non aligne). N : sans mode de vie.
	AnastasisVillage::FVillage V;
	V.Bind(World);
	FString Error;
	const bool bRestored = V.RestoreForHarness({},
		{ MakeNpc(TEXT("npc-w"), 10.5, TEXT("gatherFood"), TEXT("workhorse"), 3.0),
		  MakeNpc(TEXT("npc-e"), 12.5, TEXT("observer"), TEXT("earlyBird"), 0.1),
		  MakeNpc(TEXT("npc-n"), 14.5, TEXT("observer"), nullptr, 0.0) },
		{}, 0, 1, 10, Error);
	TestTrue(FString::Printf(TEXT("reprise des trois habitants (%s)"), *Error), bRestored);
	if (!bRestored) return false;

	// Jour 1 (temps 37,8 s sur 90).
	double Time = 37.8 + Dt;
	V.UpdateActors(Time, Dt);
	const AnastasisVillage::FNpc* W = V.FindNpc(TEXT("npc-w"));
	const AnastasisVillage::FNpc* E = V.FindNpc(TEXT("npc-e"));
	const AnastasisVillage::FNpc* N = V.FindNpc(TEXT("npc-n"));
	if (!W || !E || !N || !W->Lifestyle.IsSet() || !E->Lifestyle.IsSet())
	{
		AddError(TEXT("habitant ou mode de vie perdu"));
		return false;
	}
	TestEqual(TEXT("travailleur au travail : score + 1"), W->Lifestyle->RhythmScore, 4.0);
	TestEqual(TEXT("travailleur : jour 1 note"), W->Lifestyle->LastNotedDay, 1.0);
	TestEqual(TEXT("leve-tot qui regarde : score max(0, 0,1 - 0,2)"), E->Lifestyle->RhythmScore, 0.0);
	TestEqual(TEXT("leve-tot : jour 1 note"), E->Lifestyle->LastNotedDay, 1.0);
	TestFalse(TEXT("sans mode de vie : aucun ne lui est donne"), N->Lifestyle.IsSet());

	// Meme jour : une fois par jour au plus.
	Time += Dt;
	V.UpdateActors(Time, Dt);
	TestEqual(TEXT("meme jour : score inchange"), V.FindNpc(TEXT("npc-w"))->Lifestyle->RhythmScore, 4.0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
