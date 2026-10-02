#include "Misc/AutomationTest.h"

#include "Village/AnastasisVillage.h"
#include "World/AnastasisWorld.h"

#if WITH_DEV_AUTOMATION_TESTS

// Le geste generique et la memoire des lieux (mission act-gate-001).
//
// Dans la reference, un habitant qui `observe` sans cible passe a chaque tick par la porte
// generique d'`act` (npc.js l. 3515-3540) : `notePlaceUse(waitingActivity, dt * 0.35)` sur le
// batiment le plus proche, `workTimer += dt`, puis a 1 s `perform` echoue (aucun cas pour
// `observer`) et l'echec compte. Ce test le prouve dans `FVillage`, pensee gelee (le but ne
// change pas), au pres d'un grenier : memoire du lieu (gain 0,05 par tick, minimum de
// `notePlaceUse`), mode de vie note, favori, chronometre, echec compte. Les valeurs attendues
// sont celles de `simulation.js` `notePlaceUse`, recomposees ici pas a pas.

namespace AnastasisVillageActGateTest
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

	AnastasisVillage::FNpc Observer(const TCHAR* Id, double X, double Y, const FString& Phase, const TCHAR* Lifestyle)
	{
		AnastasisVillage::FNpc N;
		N.Id = Id;
		N.X = X;
		N.Y = Y;
		N.Goal = AnastasisVillage::GoalObserver;
		N.Needs.Hunger = 20.0;
		N.Needs.Thirst = 10.0;
		N.Needs.Energy = 85.0;
		N.Needs.Social = 80.0;
		N.Needs.Leisure = 80.0;
		N.Needs.Hygiene = 80.0;
		N.Needs.Health = 95.0;
		N.Needs.Morale = 60.0;
		// Pensee gelee : le but reste `observer` pendant tout le test.
		N.AiThinkAt = 1.0e9;
		N.VillagePhase = Phase;
		if (Lifestyle)
		{
			AnastasisLifestyle::FLifestyle& L = N.Lifestyle.Emplace();
			L.Id = Lifestyle;
		}
		return N;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisVillageActGateTest,
	"Anastasis.Sim.Village.GesteEtLieux",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisVillageActGateTest::RunTest(const FString& Parameters)
{
	using namespace AnastasisVillageActGateTest;
	constexpr double Dt = 1.0 / 60.0;
	const AnastasisWorld::FWorld World = MakeFlatWorld(48, 48);
	double Time = 37.8;
	const FString Phase = AnastasisRhythm::PhaseId(AnastasisRhythm::VillagePhase(AnastasisRhythm::DayFracOf(Time + Dt)));

	// Le grenier en (20, 20), centre (20,5 ; 20,5). L'observateur W (flaneur) a 1 case, M (sans mode
	// de vie) a 1,5 case : tous deux sous le rayon de `buildingNearActor` (1,9).
	AnastasisVillage::FBuilding Granary;
	Granary.Id = TEXT("building-0");
	Granary.Type = AnastasisVillage::GranaryType;
	Granary.X = 20.0;
	Granary.Y = 20.0;
	Granary.Progress = 1.0;

	AnastasisVillage::FVillage V;
	V.Bind(World);
	FString Error;
	const bool bRestored = V.RestoreForHarness({ Granary },
		{ Observer(TEXT("npc-w"), 21.5, 20.5, Phase, TEXT("wanderer")), Observer(TEXT("npc-m"), 20.5, 22.0, Phase, nullptr) },
		{}, 0, 1, 10, Error);
	TestTrue(FString::Printf(TEXT("reprise (%s)"), *Error), bRestored);
	if (!bRestored) return false;

	// 1. Premier tick : le lieu entre en memoire, gain minimal 0,05.
	Time += Dt;
	V.UpdateActors(Time, Dt);
	const AnastasisVillage::FNpc* W = V.FindNpc(TEXT("npc-w"));
	const AnastasisVillage::FNpc* M = V.FindNpc(TEXT("npc-m"));
	if (!W || !M || W->PlaceEntries.Num() != 1 || M->PlaceEntries.Num() != 1)
	{
		AddError(TEXT("une entree de lieu par habitant attendue au premier tick"));
		return false;
	}
	const AnastasisVillage::FNpc::FPlaceEntry& E = W->PlaceEntries[0];
	TestEqual(TEXT("le lieu est le grenier"), E.BuildingId, FString(TEXT("building-0")));
	TestEqual(TEXT("type recopie"), E.Type, FString(AnastasisVillage::GranaryType));
	TestEqual(TEXT("score : max(0,05, dt x 0,35)"), E.Score, 0.05);
	TestEqual(TEXT("activite : idem"), E.Activity, 0.05);
	TestEqual(TEXT("ni travail ni social pour « attend »"), E.Work + E.Social + E.Talk + E.Drink + E.Home, 0.0);
	TestEqual(TEXT("jour de declin et dernier jour : 1"), E.DecayDay + E.LastDay, 2.0);
	TestTrue(TEXT("mode de vie note : flaneur 0,05"), E.Lifestyle.Num() == 1 && E.Lifestyle[0].Key == TEXT("wanderer") && E.Lifestyle[0].Value == 0.05);
	TestEqual(TEXT("favori : le grenier"), W->FavoriteBuildingId, FString(TEXT("building-0")));
	TestEqual(TEXT("sans mode de vie : aucune entree de mode de vie"), M->PlaceEntries[0].Lifestyle.Num(), 0);
	TestFalse(TEXT("sans mode de vie : aucun ne lui est donne"), M->Lifestyle.IsSet());
	TestEqual(TEXT("chronometre du geste : dt"), W->WorkTimer, Dt);
	TestEqual(TEXT("activite affichee : attend"), W->Activity, FString(TEXT("attend")));
	TestFalse(TEXT("« attend » n'est pas un travail : pas de laborToday"), V.FindBuilding(TEXT("building-0"))->bHasLaborToday);

	// 2. Jusqu'a l'echec du geste : le score suit la somme des gains, recomposee comme la reference.
	double Expected = 0.05;
	int32 Ticks = 1;
	while (V.FindNpc(TEXT("npc-w"))->FailedActions == 0 && Ticks < 200)
	{
		Time += Dt;
		V.UpdateActors(Time, Dt);
		Expected = FMath::Min(80.0, Expected + 0.05);
		++Ticks;
	}
	W = V.FindNpc(TEXT("npc-w"));
	TestEqual(TEXT("perform echoue pour observer : un echec compte"), W->FailedActions, 1);
	TestTrue(FString::Printf(TEXT("l'echec tombe a 1 s de geste (tick %d)"), Ticks), Ticks >= 60 && Ticks <= 61);
	TestEqual(TEXT("score : un gain de 0,05 par tick"), W->PlaceEntries[0].Score, Expected);
	TestEqual(TEXT("le but reste observer"), W->Goal, FString(AnastasisVillage::GoalObserver));
	AddInfo(FString::Printf(TEXT("echec au tick %d, score %.17g"), Ticks, W->PlaceEntries[0].Score));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
