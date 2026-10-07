#include "WorldTheatre/AnastasisWorldTheatre.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace AnastasisWorldTheatreTestSupport
{
using namespace AnastasisWorldTheatre;

/** Sol plat a 1000 uu sur [-100 km, 100 km]^2 ; un lac (nappe 1200) pour X > 50 km. */
inline FGroundSampler FlatGroundWithLake()
{
	return [](double X, double Y, double& G, double& W)
	{
		if (FMath::Abs(X) > 1.0e7 || FMath::Abs(Y) > 1.0e7) return false;
		G = 1000.0;
		W = X > 5.0e6 ? 1200.0 : -TNumericLimits<double>::Max();
		return true;
	};
}

/** Pente reguliere de 30 degres vers +X. */
inline FGroundSampler Slope30()
{
	return [](double X, double, double& G, double& W)
	{
		G = X * FMath::Tan(FMath::DegreesToRadians(30.0));
		W = -TNumericLimits<double>::Max();
		return true;
	};
}

inline TArray<FVector2D> Square(double X0, double Y0, double Side)
{
	return { {X0, Y0}, {X0 + Side, Y0}, {X0 + Side, Y0 + Side}, {X0, Y0 + Side} };
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisWorldTheatreGeometry, "Anastasis.WorldTheatre.Geometry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnastasisWorldTheatreGeometry::RunTest(const FString&)
{
	using namespace AnastasisWorldTheatre;
	const TArray<FVector2D> Sq = AnastasisWorldTheatreTestSupport::Square(0, 0, 1000);
	TestTrue(TEXT("centre inside"), PointInPolygon(Sq, FVector2D(500, 500)));
	TestFalse(TEXT("outside"), PointInPolygon(Sq, FVector2D(1500, 500)));
	TestEqual(TEXT("distance to outline from centre"), DistanceToOutline(Sq, FVector2D(500, 500)), 500.0, 1e-6);
	TestEqual(TEXT("distance to outline from outside"), DistanceToOutline(Sq, FVector2D(1300, 500)), 300.0, 1e-6);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisWorldTheatreMass, "Anastasis.WorldTheatre.Mass",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnastasisWorldTheatreMass::RunTest(const FString&)
{
	using namespace AnastasisWorldTheatre;
	FMass M;
	M.Id = TEXT("test");
	M.Outline = AnastasisWorldTheatreTestSupport::Square(0, 0, 100000); // 1 km x 1 km
	M.CanopyHeight = 1600.0;
	FMeshData Out;
	double Ha = 0.0;
	FString Why;
	TestTrue(TEXT("a 1 km square mass builds on dry flat ground"), BuildMass(M, AnastasisWorldTheatreTestSupport::FlatGroundWithLake(), Out, Ha, Why));
	TestTrue(TEXT("covered area ~100 ha"), Ha > 90.0 && Ha < 110.0);
	double MaxZ = -1e30, MinZ = 1e30;
	bool bInsideOutline = true;
	for (const FVector& V : Out.Vertices)
	{
		MaxZ = FMath::Max(MaxZ, V.Z);
		MinZ = FMath::Min(MinZ, V.Z);
		// Seule la lisiere (un pas de grille au plus) deborde du contour, et elle y est enterree.
		if (!PointInPolygon(M.Outline, FVector2D(V.X, V.Y)) && V.Z > 1000.0) bInsideOutline = false;
	}
	TestTrue(TEXT("canopy never exceeds height x max grain"), MaxZ <= 1000.0 + 1600.0 * 1.16 + 1.0);
	TestTrue(TEXT("canopy rises well above the ground"), MaxZ > 1000.0 + 1600.0 * 0.7);
	TestTrue(TEXT("edges are buried, not floating"), MinZ < 1000.0);
	TestTrue(TEXT("nothing stands outside the outline"), bInsideOutline);
	TestEqual(TEXT("one normal per vertex"), Out.Normals.Num(), Out.Vertices.Num());
	TestEqual(TEXT("one colour per vertex"), Out.Colours.Num(), Out.Vertices.Num());
	bool bUp = true;
	for (const FVector& N : Out.Normals) bUp &= N.Z > 0.0;
	TestTrue(TEXT("a canopy faces the sky"), bUp);

	FMass Wet = M;
	Wet.Outline = AnastasisWorldTheatreTestSupport::Square(6.0e6, 0, 100000);
	FMeshData Out2;
	TestFalse(TEXT("a mass on a lake is refused"), BuildMass(Wet, AnastasisWorldTheatreTestSupport::FlatGroundWithLake(), Out2, Ha, Why));
	TestTrue(TEXT("refusal says why"), Why.Contains(TEXT("eau")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisWorldTheatreSilhouette, "Anastasis.WorldTheatre.Silhouette",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnastasisWorldTheatreSilhouette::RunTest(const FString&)
{
	using namespace AnastasisWorldTheatre;
	FString Why;
	FSilhouetteSpec Tower;
	Tower.Id = TEXT("tower");
	Tower.Kind = ESilhouette::Tower;
	FMeshData Out;
	TestTrue(TEXT("a tower stands on flat ground"), BuildSilhouette(Tower, AnastasisWorldTheatreTestSupport::FlatGroundWithLake(), Out, Why));
	double Top = -1e30;
	for (const FVector& V : Out.Vertices) Top = FMath::Max(Top, V.Z);
	TestEqual(TEXT("tower top = ground + 14 m + 3 m roof"), Top, 1000.0 + 1700.0, 1.0);
	FSilhouetteSpec Ruin;
	Ruin.Id = TEXT("ruin");
	Ruin.Kind = ESilhouette::RuinedTower;
	FMeshData RuinMesh;
	TestTrue(TEXT("a ruined tower stands on flat ground"), BuildSilhouette(Ruin, AnastasisWorldTheatreTestSupport::FlatGroundWithLake(), RuinMesh, Why));
	TSet<int32> Tops;
	for (const FVector& V : RuinMesh.Vertices) if (V.Z > 1000.0 + 900.0) Tops.Add(FMath::RoundToInt32(V.Z));
	TestEqual(TEXT("a broken top has several heights (no roof)"), Tops.Num(), 4);
	double RuinTop = -1e30;
	for (const FVector& V : RuinMesh.Vertices) RuinTop = FMath::Max(RuinTop, V.Z);
	TestEqual(TEXT("ruined tower top = ground + 15 m"), RuinTop, 1000.0 + 1500.0, 1.0);
	FMeshData Steep;
	TestFalse(TEXT("a chapel is refused on a 30 deg slope"),
		BuildSilhouette(FSilhouetteSpec{ TEXT("c"), ESilhouette::Chapel }, AnastasisWorldTheatreTestSupport::Slope30(), Steep, Why));
	TestTrue(TEXT("refusal names the slope"), Why.Contains(TEXT("pente")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisWorldTheatrePlanBuilds, "Anastasis.WorldTheatre.PlanIsWellFormed",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnastasisWorldTheatrePlanBuilds::RunTest(const FString&)
{
	using namespace AnastasisWorldTheatre;
	const FPlan& Plan = CanonicalPlan();
	for (const FMass& M : Plan.Masses) TestTrue(*FString::Printf(TEXT("mass %s has an outline"), M.Id), M.Outline.Num() >= 3);
	TSet<FString> Ids;
	bool bUnique = true;
	for (const FMass& M : Plan.Masses) { bool bDup = false; Ids.Add(M.Id, &bDup); bUnique &= !bDup; }
	for (const FSilhouetteSpec& S : Plan.Silhouettes) { bool bDup = false; Ids.Add(S.Id, &bDup); bUnique &= !bDup; }
	TestTrue(TEXT("every plan element has a unique id"), bUnique);
	return true;
}
#endif
