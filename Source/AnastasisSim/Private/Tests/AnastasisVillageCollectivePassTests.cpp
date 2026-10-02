#include "Misc/AutomationTest.h"

#include "Village/AnastasisVillage.h"
#include "World/AnastasisWorld.h"

#if WITH_DEV_AUTOMATION_TESTS

// La passe collective de fin d'`adultScores` (mission collective-pass-001).
//
// Dans la reference, apres la chaine des biais, chaque ligne recoit l'urgence collective, puis
// le plancher collectif apres `workFactor` (un plancher, pas un appoint ; en appoint si un besoin
// est critique ; corvee de bois a 165), puis le rush famine (npc.js l. 1205-1258). Le planificateur
// qui les calcule n'est pas encore porte : ce test injecte ses sorties et prouve la passe.
// Sans planificateur, la passe ne touche rien : la table garde ses bits.

namespace AnastasisVillageCollectivePassTest
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

	/** Une decision, et la trace qu'elle laisse. */
	AnastasisVillage::FDecisionTrace Decide(const AnastasisWorld::FWorld& World, TFunction<AnastasisVillage::FCollectiveDecision(const AnastasisVillage::FNpc&)> Override,
		const AnastasisVillage::FNpc& Npc, FString& OutGoal)
	{
		AnastasisVillage::FVillage V;
		V.Bind(World);
		FString Error;
		V.RestoreForHarness({}, { Npc }, {}, 0, 1, 10, Error);
		V.CollectiveDecisionOverride = MoveTemp(Override);
		V.SetSimRngState(12345u);
		V.ChooseGoalNow(Npc.Id);
		const AnastasisVillage::FNpc* After = V.FindNpc(Npc.Id);
		OutGoal = After ? After->Goal : FString();
		return After ? After->LastDecision : AnastasisVillage::FDecisionTrace();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisVillageCollectivePassTest,
	"Anastasis.Sim.Village.PasseCollective",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisVillageCollectivePassTest::RunTest(const FString& Parameters)
{
	using namespace AnastasisVillageCollectivePassTest;
	using AnastasisVillage::FCollectiveDecision;
	const AnastasisWorld::FWorld World = MakeFlatWorld(48, 48);
	const AnastasisVillage::FNpc Npc = Adult(TEXT("npc-a"), 20.5, 20.5);

	// 1. Sans planificateur : la passe ne touche rien.
	FString GoalPlain;
	const AnastasisVillage::FDecisionTrace Plain = Decide(World, nullptr, Npc, GoalPlain);
	TestEqual(TEXT("sans planificateur : aucune ligne touchee"), Plain.CollectiveDelta.Num(), 0);

	// 2. Un plancher au-dessus de toute la table : il gagne (helpFarm 500, comme le 28 de la reference
	//    mais assez haut pour battre les besoins de cet habitant repose).
	FString GoalFloor;
	const AnastasisVillage::FDecisionTrace Floored = Decide(World, [](const AnastasisVillage::FNpc&)
	{
		FCollectiveDecision C;
		C.GoalFloor.Add(TEXT("helpFarm"), 500.0);
		return C;
	}, Npc, GoalFloor);
	TestEqual(TEXT("plancher : la ligne monte au plancher, et gagne la table"), Floored.TableWinner, FString(TEXT("helpFarm")));
	TestTrue(TEXT("plancher : la trace dit de combien la ligne est montee"), Floored.CollectiveDelta.Contains(TEXT("helpFarm")));

	// 3. Un plancher SOUS la ligne : un plancher n'ajoute rien. La ligne `rest` (village sans horloge :
	//    la nuit, besoin de repos + priorite de metier + rythme) est nettement au-dessus de 1.
	TestTrue(FString::Printf(TEXT("precondition : la ligne rest vaut plus que 1 (%.3f)"), Plain.RestRowScore), Plain.RestRowScore > 1.0);
	FString GoalLow;
	const AnastasisVillage::FDecisionTrace Low = Decide(World, [](const AnastasisVillage::FNpc&)
	{
		FCollectiveDecision C;
		C.GoalFloor.Add(AnastasisVillage::GoalRest, 1.0);
		return C;
	}, Npc, GoalLow);
	TestFalse(TEXT("plancher sous la ligne : rien ne change"), Low.CollectiveDelta.Contains(AnastasisVillage::GoalRest));

	// 4. Besoin critique : le plancher devient un appoint (+ plancher), il ne remplace pas la ligne.
	AnastasisVillage::FNpc Thirsty = Npc;
	Thirsty.Needs.Thirst = 80.0;
	FString GoalCrit;
	const AnastasisVillage::FDecisionTrace Crit = Decide(World, [](const AnastasisVillage::FNpc&)
	{
		FCollectiveDecision C;
		C.GoalFloor.Add(TEXT("helpFarm"), 1.0);
		return C;
	}, Thirsty, GoalCrit);
	TestTrue(TEXT("besoin critique : le plancher s'ajoute (+1)"), Crit.CollectiveDelta.Contains(TEXT("helpFarm")) && Crit.CollectiveDelta[TEXT("helpFarm")] == 1.0);

	// 5. Urgence : additive, propre a l'habitant.
	FString GoalUrg;
	const AnastasisVillage::FDecisionTrace Urg = Decide(World, [](const AnastasisVillage::FNpc&)
	{
		FCollectiveDecision C;
		C.UrgencyBias.Add(TEXT("drink"), 7.5);
		return C;
	}, Npc, GoalUrg);
	TestTrue(TEXT("urgence : +7,5 sur drink"), Urg.CollectiveDelta.Contains(TEXT("drink")) && Urg.CollectiveDelta[TEXT("drink")] == 7.5);

	// 6. Corvee de bois : plancher 165 sur gatherWood pour le requisitionne.
	FString GoalDraft;
	const AnastasisVillage::FDecisionTrace Draft = Decide(World, [](const AnastasisVillage::FNpc&)
	{
		FCollectiveDecision C;
		C.bWoodBootstrapDraftee = true;
		return C;
	}, Npc, GoalDraft);
	TestTrue(TEXT("corvee de bois : gatherWood monte a 165 au moins"), Draft.CollectiveDelta.Contains(TEXT("gatherWood")));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
