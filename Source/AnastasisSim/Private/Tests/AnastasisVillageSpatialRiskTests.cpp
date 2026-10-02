#include "Misc/AutomationTest.h"

#include "Ai/AnastasisSpatialRisk.h"
#include "Life/AnastasisNeeds.h"
#include "Village/AnastasisVillage.h"
#include "World/AnastasisWorld.h"

#if WITH_DEV_AUTOMATION_TESTS

// La prevision de survie et le risque spatial dans la decision du village (spatial-risk-test-001).
//
// La partie pure est prouvee bit a bit par Anastasis.Sim.Parite.RisqueSpatial. Ici, l'ASSEMBLAGE :
//   1. la carte est calculee a la decision (un gisement lointain, a minuit : la cueillette paie son trajet) ;
//   2. elle s'ajoute a chaque ligne, apres la meteo, `(ligne + prevision) + risque`, au bit pres ;
//   3. ses replis appellent `buildingAccessPoint`, qui filtre paresseusement les seuils devenus bloques :
//      le seuil du puits recouvert par une maison posee apres lui disparait a la decision, comme au tick 32
//      d'endurance dans la reference, sans que le but choisi vise le puits.

namespace AnastasisVillageSpatialRiskTest
{
	using namespace AnastasisVillage;

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
		// Un champ lointain, hors de la vue de l'habitant : il ne le connait que par sa memoire.
		AnastasisWorld::FTile& Field = World.Tiles[40 * W + 50];
		Field.Type = AnastasisWorld::ETileType::Field;
		Field.Resource = AnastasisWorld::EResource::Food;
		Field.Amount = 20;
		Field.CropId = AnastasisWorld::ECropId::Grain;
		return World;
	}

	AnastasisNeeds::FNeeds Calm()
	{
		AnastasisNeeds::FNeeds N;
		N.Hunger = 30.0;
		N.Energy = 60.0;
		N.Social = 70.0;
		N.Leisure = 70.0;
		N.Hygiene = 70.0;
		N.Thirst = 20.0;
		N.Health = 95.0;
		N.Morale = 60.0;
		return N;
	}

	bool HasPoint(const TArray<FPoint>& Points, const FPoint& P)
	{
		return Points.ContainsByPredicate([&P](const FPoint& Q) { return Q.X == P.X && Q.Y == P.Y; });
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisVillageSpatialRiskTest,
	"Anastasis.Sim.Village.RisqueSpatial",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisVillageSpatialRiskTest::RunTest(const FString&)
{
	using namespace AnastasisVillageSpatialRiskTest;
	const AnastasisWorld::FWorld World = MakeFlatWorld(64, 48);
	FVillage Village;
	Village.Bind(World);
	Village.SetSettlement(20.5, 26.5);

	// Le puits, puis une maison posee sur son premier seuil : le seuil reste enregistre (filtre paresseux).
	const FString Well = Village.AddBuilding(WellType, 20, 20);
	if (!TestFalse(TEXT("puits pose"), Well.IsEmpty())) return false;
	const TArray<FPoint> Before = Village.FindBuilding(Well)->AccessPoints;
	if (!TestTrue(TEXT("le puits a plusieurs seuils"), Before.Num() >= 2)) return false;
	const FPoint Covered = Before[0];
	const FString House = Village.AddBuilding(HouseType, FMath::FloorToInt32(Covered.X), FMath::FloorToInt32(Covered.Y));
	if (!TestFalse(TEXT("maison posee sur le seuil"), House.IsEmpty())) return false;
	TestTrue(TEXT("le seuil recouvert est bloque"), Village.IsFootBlocked(Covered.X, Covered.Y));
	TestTrue(TEXT("filtre paresseux : le puits garde le seuil recouvert"), HasPoint(Village.FindBuilding(Well)->AccessPoints, Covered));

	// Un habitant, sans foyer, qui se souvient d'un champ lointain.
	const FString Id = Village.SpawnNpc(14.5, 26.5, Calm());
	FNpc* Npc = Village.FindNpcMutable(Id);
	if (!TestNotNull(TEXT("habitant"), Npc)) return false;
	FResourceSpot Spot;
	Spot.Key = TEXT("50,40");
	Spot.X = 50.5;
	Spot.Y = 40.5;
	Spot.Resource = TEXT("food");
	Spot.Amount = 20;
	Npc->Spots.Add(Spot);

	// A minuit (horloge 0) : `secondsUntilNight` = 0, tout trajet depasse son budget.
	Village.ChooseGoalNow(Id);
	const FNpc* After = Village.FindNpc(Id);
	const FDecisionTrace& T = After->LastDecision;

	// 1. La carte est calculee.
	const double* Gather = T.SpatialRiskBias.Find(TEXT("gatherFood"));
	if (TestNotNull(TEXT("risque calcule pour gatherFood (gisement lointain)"), Gather))
	{
		TestTrue(TEXT("la cueillette lointaine est penalisee"), *Gather < 0.0);
	}

	// 2. Elle s'ajoute a chaque ligne, au bit pres, apres la meteo : `(ligne + prevision) + risque`.
	TestTrue(TEXT("lignes tracees"), T.RowsAfterRisk.Num() > 0 && T.RowsAfterRisk.Num() == T.RowsBeforeRisk.Num());
	int32 Moved = 0;
	for (const TPair<FString, double>& Row : T.RowsAfterRisk)
	{
		const double Base = T.RowsBeforeRisk.FindRef(Row.Key);
		const double Expected = (Base + T.ForecastBias.FindRef(Row.Key)) + T.SpatialRiskBias.FindRef(Row.Key);
		if (Row.Value != Expected)
		{
			AddError(FString::Printf(TEXT("ligne %s : %.17g, attendu %.17g"), *Row.Key, Row.Value, Expected));
		}
		Moved += Row.Value != Base ? 1 : 0;
	}
	TestTrue(TEXT("la ligne gatherFood a bouge"), T.RowsAfterRisk.FindRef(TEXT("gatherFood")) != T.RowsBeforeRisk.FindRef(TEXT("gatherFood")));
	AddInfo(FString::Printf(TEXT("RISQUE_SPATIAL lignes=%d bougees=%d gatherFood=%.6f gagnant=%s"), T.RowsAfterRisk.Num(), Moved,
		Gather ? *Gather : 0.0, *T.Winner));

	// 3. Le seuil recouvert a disparu a la decision, sans que le but choisi vise le puits.
	TestNotEqual(TEXT("le but choisi ne vise pas le puits (drink)"), T.Winner, FString(GoalDrink));
	TestNotEqual(TEXT("le but choisi ne vise pas le puits (socialize)"), T.Winner, FString(GoalSocialize));
	const TArray<FPoint>& Filtered = Village.FindBuilding(Well)->AccessPoints;
	TestFalse(TEXT("le seuil recouvert est retire par la preparation de la decision"), HasPoint(Filtered, Covered));
	TestEqual(TEXT("les autres seuils restent"), Filtered.Num(), Before.Num() - 1);
	return true;
}

#endif
