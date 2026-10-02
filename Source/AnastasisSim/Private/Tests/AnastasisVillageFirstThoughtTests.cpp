#include "Misc/AutomationTest.h"

#include "Village/AnastasisVillage.h"
#include "World/AnastasisWorld.h"

#if WITH_DEV_AUTOMATION_TESTS

// Ce que la premiere pensee ecrit (mission premiere-pensee-001).
//
// Au commit d'un but, la reference ecrit `goalExplain` (les trois premieres lignes de la table et leur
// cause dominante), `streetDecision` (la fenetre d'une bascule), `buildBinding` (`null` hors `build`) ;
// la cible ecrit `socialSeekId`, la table `mind.failures` et `nocturnalIntent`. Les cles apparaissent
// a la premiere pensee, pas avant.

namespace AnastasisVillageFirstThoughtTest
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
		N.Needs.Thirst = 75.0;
		N.Needs.Energy = 85.0;
		N.Needs.Social = 80.0;
		N.Needs.Leisure = 80.0;
		N.Needs.Hygiene = 80.0;
		N.Needs.Health = 95.0;
		N.Needs.Morale = 60.0;
		return N;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisVillageFirstThoughtTest,
	"Anastasis.Sim.Village.PremierePensee",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisVillageFirstThoughtTest::RunTest(const FString& Parameters)
{
	using namespace AnastasisVillageFirstThoughtTest;
	using namespace AnastasisVillage;
	const AnastasisWorld::FWorld World = MakeFlatWorld(30, 30);
	FVillage V;
	V.Bind(World);
	FString Error;
	V.RestoreForHarness({}, { Adult(TEXT("npc-0"), 12.5, 12.5) }, {}, 0, 1, 10, Error);
	V.AddBuilding(WellType, 18, 12);
	V.SetSimRngState(12345u);

	// Avant toute pensee : aucune cle.
	{
		const FNpc* N = V.FindNpc(TEXT("npc-0"));
		TestFalse(TEXT("avant : pas d'explication"), N->GoalExplain.IsSet());
		TestFalse(TEXT("avant : pas de fenetre de rue"), N->StreetDecision.IsSet());
		TestFalse(TEXT("avant : pas de buildBinding"), N->bHasBuildBinding);
		TestFalse(TEXT("avant : pas de socialSeekId"), N->bHasSocialSeekId);
		TestFalse(TEXT("avant : pas de memoire d'echecs"), N->bHasFailureStore);
		TestFalse(TEXT("avant : pas d'intention nocturne"), N->NocturnalIntent.IsSet());
	}

	V.ChooseGoalNow(TEXT("npc-0"));
	const FNpc* N = V.FindNpc(TEXT("npc-0"));
	if (!TestNotNull(TEXT("habitant"), N)) return false;
	const FDecisionTrace& T = N->LastDecision;

	// `goalExplain` : la tete de la table, trois lignes au plus, leurs scores arrondis au dixieme.
	if (TestTrue(TEXT("explication posee"), N->GoalExplain.IsSet()))
	{
		const FGoalExplain& E = N->GoalExplain.GetValue();
		TestEqual(TEXT("explication : a l'heure de la pensee"), E.At, T.Time);
		TestEqual(TEXT("explication : le gagnant de la table"), E.Goal, T.TableWinner);
		TestEqual(TEXT("explication : trois lignes"), E.Top.Num(), 3);
		if (E.Top.Num() == 3)
		{
			TestEqual(TEXT("explication : premiere ligne = gagnant"), E.Top[0].Goal, T.TableWinner);
			TestTrue(TEXT("explication : lignes triees"), E.Top[0].Score >= E.Top[1].Score && E.Top[1].Score >= E.Top[2].Score);
			for (const FGoalExplainEntry& Entry : E.Top)
			{
				TestEqual(TEXT("explication : score au dixieme"), Entry.Score * 10.0, FMath::RoundToDouble(Entry.Score * 10.0));
				TestFalse(TEXT("explication : une cause"), Entry.Cause.IsEmpty());
			}
			const bool bHead = E.Top[0].Score >= E.Top[1].Score + 35.0;
			TestEqual(TEXT("explication : ligne"), E.Line,
				bHead ? FString(TEXT("parce que ")) + E.Top[0].Cause : E.Line);
			if (!bHead) TestTrue(TEXT("explication : trois buts courts"), E.Line.Contains(TEXT(" · ")));
		}
	}
	// `streetDecision` : une bascule depuis `observer`, fenetre de 4,5 s (3 pour un but doux, 6,5 en crise).
	if (TestTrue(TEXT("fenetre de rue posee"), N->StreetDecision.IsSet()) && N->GoalExplain.IsSet())
	{
		const FStreetDecision& D = N->StreetDecision.GetValue();
		const FGoalExplain& E = N->GoalExplain.GetValue();
		TestEqual(TEXT("rue : le gagnant"), D.Goal, T.TableWinner);
		TestEqual(TEXT("rue : depuis observer"), D.From, FString(GoalObserver));
		TestEqual(TEXT("rue : bascule"), D.bChanged, T.TableWinner != FString(GoalObserver));
		TestEqual(TEXT("rue : la cause de l'explication"), D.Cause, E.Top.Num() > 0 ? E.Top[0].Cause : FString());
		TestTrue(TEXT("rue : fenetre 3 / 4,5 / 6,5 s"), D.Until - D.At == 3.0 || D.Until - D.At == 4.5 || D.Until - D.At == 6.5);
		if (E.Top.Num() >= 2) TestTrue(TEXT("rue : marge >= 0"), D.Margin >= 0.0);
	}
	// Les cles posees en passant.
	TestTrue(TEXT("buildBinding ecrit au commit"), N->bHasBuildBinding);
	if (N->Goal != TEXT("build")) TestTrue(TEXT("buildBinding nul hors build"), N->BuildBinding.IsEmpty());
	TestTrue(TEXT("socialSeekId ecrit a la cible"), N->bHasSocialSeekId);
	TestTrue(TEXT("memoire d'echecs creee par la table"), N->bHasFailureStore);
	TestTrue(TEXT("intention nocturne posee par la table"), N->NocturnalIntent.IsSet());
	return true;
}

#endif
