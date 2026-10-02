#include "Misc/AutomationTest.h"

#include "Village/AnastasisVillage.h"
#include "Work/AnastasisBuild.h"
#include "World/AnastasisWorld.h"

#if WITH_DEV_AUTOMATION_TESTS

// Le planificateur collectif branche sur le village (mission planner-wiring-001).
//
// `CollectiveDecisionOf` lit le planificateur (`AnastasisPlanner::DecisionFor`) sur une vue du village
// quand le village a une colonie (reprise d'une sauvegarde), et rien sinon (ecart n°27). La ligne `build`
// se calcule alors sans chantier (`buildScore` : besoin, liquidite), et sa cible est l'acces au site du
// marche (`marketAccessPoint`).

namespace AnastasisVillagePlannerWiringTest
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

	AnastasisVillage::FNpc Adult(const TCHAR* Id, double X, double Y)
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
		return N;
	}

	/** La colonie d'`endurance` au jour 1, reduite a ce que lit le planificateur. */
	AnastasisPlanner::FColonyState EnduranceColony()
	{
		AnastasisPlanner::FColonyState C;
		C.Morale = 56.0;
		for (const TCHAR* Type : { TEXT("food"), TEXT("housing"), TEXT("tools"), TEXT("labor"), TEXT("transport"), TEXT("security") })
		{
			C.Priorities.Levels.Set(Type, 0.0);
		}
		C.StockReport.bPresent = true;
		C.StockReport.Day = 1.0;
		C.StockReport.LastRefreshDay = 1.0;
		for (const TCHAR* Res : { TEXT("wood"), TEXT("stone"), TEXT("food"), TEXT("tools"), TEXT("planks") })
		{
			C.StockReport.Stock.Set(Res, 0.0);
		}
		C.StockReport.CertifiedNear = 2.0;
		return C;
	}

	void MakeVillage(AnastasisVillage::FVillage& V, const AnastasisWorld::FWorld& World, const AnastasisVillage::FNpc& Npc, bool bColony)
	{
		V.Bind(World);
		FString Error;
		V.RestoreForHarness({}, { Npc }, {}, 0, 1, 10, Error);
		V.SetSettlement(15.0, 15.0);
		V.SetMarketOffset(5.0, -3.0);
		if (bColony)
		{
			AnastasisPlanner::FOrderedMap Market;
			Market.Set(TEXT("wood"), 0.0);
			V.RestoreColonyForHarness(EnduranceColony(), Market, TOptional<double>(9.0), TOptional<double>(420.0));
		}
		V.SetSimRngState(12345u);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisVillagePlannerWiringTest,
	"Anastasis.Sim.Village.PlanificateurBranche",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisVillagePlannerWiringTest::RunTest(const FString& Parameters)
{
	using namespace AnastasisVillagePlannerWiringTest;
	using namespace AnastasisVillage;
	const AnastasisWorld::FWorld World = MakeFlatWorld(30, 30);
	const FNpc Settler = Adult(TEXT("npc-0"), 12.5, 15.5);

	// Sans colonie : le planificateur n'est pas consulte, la decision est vide.
	{
		FVillage V;
		MakeVillage(V, World, Settler, false);
		const FCollectiveDecision D = V.CollectiveDecisionOf(*V.FindNpc(Settler.Id));
		TestFalse(TEXT("sans colonie : pas de colonie"), D.bHasColony);
		TestEqual(TEXT("sans colonie : aucun biais"), D.GoalBias.Num(), 0);
		TestEqual(TEXT("sans colonie : aucun plancher"), D.GoalFloor.Num(), 0);
	}

	// Avec la colonie : la decision vient du planificateur, la ligne `build` a son besoin.
	{
		FVillage V;
		MakeVillage(V, World, Settler, true);
		const uint32 RngBefore = V.GetSimRngState();
		const FCollectiveDecision D = V.CollectiveDecisionOf(*V.FindNpc(Settler.Id));
		TestTrue(TEXT("colonie : decision du planificateur"), D.bHasColony);
		TestTrue(TEXT("colonie : un biais par but de la reference"), D.GoalBias.Num() > 0);
		TestTrue(TEXT("colonie : besoin de batir"), D.BuildingNeedScore > 0.0);
		TestFalse(TEXT("colonie : la ligne build n'est pas nulle"), D.bBuildIdle);
		// Tresor 420 >= BUILD_WAGE : liquidite 1, le terme vaut le plancher du besoin.
		TestTrue(TEXT("colonie : needFloor x 1 >= besoin"), D.BuildNeedTerm >= D.BuildingNeedScore);
		// Le rapport de stock a ete rafraichi ce jour : aucun tirage.
		TestEqual(TEXT("colonie : flux intact"), V.GetSimRngState(), RngBefore);
		// La decision se relit a l'identique (caches du jour).
		const FCollectiveDecision Again = V.CollectiveDecisionOf(*V.FindNpc(Settler.Id));
		TestEqual(TEXT("colonie : besoin stable"), Again.BuildingNeedScore, D.BuildingNeedScore);
		TestEqual(TEXT("colonie : terme stable"), Again.BuildNeedTerm, D.BuildNeedTerm);
		TestEqual(TEXT("colonie : biais build stable"), Again.BiasOf(AnastasisBuild::GoalBuild), D.BiasOf(AnastasisBuild::GoalBuild));
	}

	// Sans chantier, un batisseur choisi va au site du marche.
	{
		FVillage V;
		MakeVillage(V, World, Settler, true);
		V.CollectiveDecisionOverride = [](const FNpc&)
		{
			FCollectiveDecision D;
			D.bHasColony = true;
			D.BuildNeedTerm = 1000.0;
			// Le village de test est a minuit : seul un plancher collectif fait passer `build` devant `rest`.
			D.GoalFloor.Add(AnastasisBuild::GoalBuild, 1000.0);
			return D;
		};
		V.ChooseGoalNow(Settler.Id);
		const FNpc* After = V.FindNpc(Settler.Id);
		if (!TestNotNull(TEXT("cible : habitant"), After)) return false;
		TestEqual(TEXT("cible : but build"), After->Goal, FString(AnastasisBuild::GoalBuild));
		TestTrue(TEXT("cible : une cible"), After->bHasTarget);
		FPoint Market;
		TestTrue(TEXT("cible : acces au marche"), V.MarketAccessPoint(After, Market));
		TestEqual(TEXT("cible : x du marche"), After->Target.X, Market.X);
		TestEqual(TEXT("cible : y du marche"), After->Target.Y, Market.Y);
		TestEqual(TEXT("cible : source"), After->LastDecision.TargetSource, FString(TEXT("market")));
	}
	return true;
}

#endif
