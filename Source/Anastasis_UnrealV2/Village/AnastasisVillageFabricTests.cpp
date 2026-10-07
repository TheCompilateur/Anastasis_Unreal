#include "Misc/AutomationTest.h"

#include "Village/AnastasisVillageFabric.h"

#if WITH_DEV_AUTOMATION_TESTS


namespace AnastasisVillageFabric
{
namespace
{
	constexpr double Cell = 2000.0;

	/** Un batiment dont l'acces est la case voisine tournee vers (TowardX, TowardY), comme la simulation. */
	FPlot Plot(const TCHAR* Id, const TCHAR* Type, int32 X, int32 Y, int32 TowardX, int32 TowardY)
	{
		FPlot P;
		P.Id = Id;
		P.Type = Type;
		P.Cell = FIntPoint(X, Y);
		const int32 DX = FMath::Sign(TowardX - X);
		const int32 DY = FMath::Sign(TowardY - Y);
		const bool bAlongX = FMath::Abs(TowardX - X) >= FMath::Abs(TowardY - Y);
		P.Access = FVector2D(X + 0.5 + (bAlongX ? DX : 0), Y + 0.5 + (bAlongX ? 0 : DY));
		P.bHasAccess = DX != 0 || DY != 0;
		return P;
	}

	TArray<FPlot> Hamlet()
	{
		return {
			Plot(TEXT("b-1"), TEXT("well"), 10, 10, 10, 10),
			Plot(TEXT("b-2"), TEXT("house"), 11, 11, 10, 10),
			Plot(TEXT("b-3"), TEXT("house"), 9, 11, 10, 10),
			Plot(TEXT("b-4"), TEXT("house"), 12, 10, 10, 10),
			Plot(TEXT("b-5"), TEXT("granary"), 10, 8, 10, 10),
			Plot(TEXT("b-6"), TEXT("house"), 8, 9, 10, 10),
		};
	}

	bool Flat(double, double, double& Z) { Z = 1000.0; return true; }
}
}

/** Sol plat : tout seuil rejoint la placette, aucune chaussee sur un corps, pas de mur, pas d'escalier. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisVillageFabricFlatTest,
	"Anastasis.Village.Fabric.Flat",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisVillageFabricFlatTest::RunTest(const FString&)
{
	using namespace AnastasisVillageFabric;
	const FFabric F = Build(Hamlet(), Cell, Flat);
	const FReport& R = F.Report;
	TestEqual(TEXT("plots"), R.Plots, 6);
	TestEqual(TEXT("seuils (puits exclu)"), R.Doors, 5);
	TestEqual(TEXT("tous relies"), R.ConnectedDoors, R.Doors);
	TestEqual(TEXT("une ruelle par seuil"), R.Lanes, 5);
	TestEqual(TEXT("aucune chaussee sur un corps"), R.BodyIntrusions, 0);
	TestEqual(TEXT("plat : aucun mur"), R.Walls, 0);
	TestEqual(TEXT("plat : aucune marche"), R.StepLengthCm, 0.0);
	TestTrue(TEXT("placette au puits"), F.Plaza.bValid && F.Plaza.WellId == TEXT("b-1"));
	TestTrue(TEXT("platane hors chaussee"), F.Plaza.bHasTree && !F.IsPavedAt(F.Plaza.TreeSpot.X, F.Plaza.TreeSpot.Y));
	TestTrue(TEXT("placette pavee"), F.IsPavedAt(10.5 * Cell + 600.0, 10.5 * Cell));
	// Le carre ou l'architecture defriche deja (emprise + 120) n'est jamais a nous.
	TestFalse(TEXT("centre d'une maison non pave"), F.IsPavedAt(11.5 * Cell, 11.5 * Cell));
	TestFalse(TEXT("bord d'emprise non pave"), F.IsPavedAt(11.5 * Cell + 1000.0, 11.5 * Cell));
	for (const FLane& Lane : F.Lanes)
	{
		TestTrue(*FString::Printf(TEXT("ruelle %s non vide"), *Lane.FromId), Lane.Points.Num() >= 2 && Lane.LengthCm > 0.0);
		TestTrue(*FString::Printf(TEXT("ruelle %s largeur"), *Lane.FromId), Lane.WidthCm >= 150.0 && Lane.WidthCm <= 240.0);
	}
	return true;
}

/** Ni l'ordre de la simulation ni un second appel ne changent le tissu. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisVillageFabricDeterminismTest,
	"Anastasis.Village.Fabric.Determinism",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisVillageFabricDeterminismTest::RunTest(const FString&)
{
	using namespace AnastasisVillageFabric;
	const auto Slope = [](double X, double Y, double& Z) { Z = 0.12 * Y + 0.05 * X; return true; };
	const FFabric A = Build(Hamlet(), Cell, Slope);
	TArray<FPlot> Shuffled = Hamlet();
	Algo::Reverse(Shuffled);
	Shuffled.Swap(1, 3);
	const FFabric B = Build(Shuffled, Cell, Slope);
	const FFabric C = Build(Hamlet(), Cell, Slope);
	TestEqual(TEXT("ordre inverse : meme tissu"), A.Report.Signature, B.Report.Signature);
	TestEqual(TEXT("second appel : meme tissu"), A.Report.Signature, C.Report.Signature);
	TestNotEqual(TEXT("relief different : autre tissu"), A.Report.Signature, Build(Hamlet(), Cell, Flat).Report.Signature);
	return true;
}

/**
 * Pente de 30 % : la ruelle ne monte pas droit, elle prend des lacets (pente moyenne nettement sous celle
 * de la ligne droite) ; la terrasse se tient par un soutenement aval qui regarde l'aval, et un deblai amont.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisVillageFabricSlopeTest,
	"Anastasis.Village.Fabric.Slope",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisVillageFabricSlopeTest::RunTest(const FString&)
{
	using namespace AnastasisVillageFabric;
	const auto Steep = [](double, double Y, double& Z) { Z = 0.30 * Y; return true; };
	const TArray<FPlot> Plots = {
		Plot(TEXT("b-1"), TEXT("well"), 10, 10, 10, 10),
		Plot(TEXT("b-2"), TEXT("house"), 10, 13, 10, 10),
		Plot(TEXT("b-3"), TEXT("house"), 12, 12, 10, 10),
	};
	const FFabric F = Build(Plots, Cell, Steep);
	const FReport& R = F.Report;
	AddInfo(ToLogLine(R));
	TestEqual(TEXT("tous relies"), R.ConnectedDoors, R.Doors);
	TestEqual(TEXT("aucune chaussee sur un corps"), R.BodyIntrusions, 0);
	TestTrue(TEXT("la ruelle suit les courbes"), R.LaneMeanGrade < R.StraightMeanGrade * 0.85);
	TestTrue(TEXT("plus d'un sentier droit"), R.LaneLengthCm > 0.0);
	bool bRetainingDownhill = false;
	bool bCutUphill = false;
	for (const FWallRun& Wall : F.Walls)
	{
		TestTrue(TEXT("mur borne"), Wall.MaxHeightCm <= 320.0 + 1.0);
		bRetainingDownhill |= Wall.bRetaining && Wall.Face.Y < -0.7;
		bCutUphill |= !Wall.bRetaining && Wall.Face.Y < -0.7 && Wall.Bottom[0].Y > 0.0;
	}
	TestTrue(TEXT("soutenement qui regarde l'aval"), bRetainingDownhill);
	TestTrue(TEXT("deblai amont qui regarde la maison"), bCutUphill);
	return true;
}

/** Sans puits : la maisonnee centrale est la racine, pas de placette ; un trou dans le relief ne casse rien. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisVillageFabricEdgeCasesTest,
	"Anastasis.Village.Fabric.EdgeCases",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisVillageFabricEdgeCasesTest::RunTest(const FString&)
{
	using namespace AnastasisVillageFabric;
	TArray<FPlot> Houses = Hamlet();
	Houses.RemoveAt(0);
	const FFabric NoWell = Build(Houses, Cell, Flat);
	TestFalse(TEXT("pas de placette"), NoWell.Plaza.bValid);
	TestEqual(TEXT("tous relies sans puits"), NoWell.Report.ConnectedDoors, NoWell.Report.Doors);

	const auto Holed = [](double X, double Y, double& Z) { Z = 0.0; return X < 11.5 * Cell; };
	const FFabric Hole = Build(Hamlet(), Cell, Holed);
	TestTrue(TEXT("seuil hors relief : non relie, compte"), Hole.Report.ConnectedDoors < Hole.Report.Doors);

	const FFabric Empty = Build({}, Cell, Flat);
	TestEqual(TEXT("vide"), Empty.Report.Lanes, 0);
	return true;
}

#endif
