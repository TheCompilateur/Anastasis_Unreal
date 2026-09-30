#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "WorldView/AnastasisDrainage.h"
#include "WorldView/AnastasisHumanGeography.h"
#include "WorldView/AnastasisTerrainForge.h"
#include "WorldView/AnastasisTerrainSurface.h"
#include "WorldView/AnastasisWorldView.h"

namespace
{
using namespace AnastasisWorldView;

/** Le monde entier tel que l'incarnation mode 2 le forge : echelle 5, couche HG au choix. */
bool Forge(bool bHumanGeography, FWorldVisualSnapshot& S, AnastasisTerrainForge::FMesh& M)
{
	S = CaptureCanonicalWorld(ReferenceSeed);
	S.SpatialScale = 5;
	S.bHumanGeography = bHumanGeography;
	AnastasisTerrainSurface::FGeometry G;
	return AnastasisTerrainSurface::Build(S, G) && AnastasisTerrainForge::Apply(S, G, M);
}

bool Drain(bool bHumanGeography, FWorldVisualSnapshot& S, AnastasisTerrainForge::FMesh& M, AnastasisDrainage::FNetwork& Net)
{
	return Forge(bHumanGeography, S, M) && AnastasisDrainage::Apply(S, M, Net);
}

bool ExpectCoherent(FAutomationTestBase& T, const AnastasisDrainage::FNetwork& Net, const AnastasisTerrainForge::FMesh& M)
{
	const AnastasisDrainage::FCheck C = AnastasisDrainage::Check(Net, M);
	T.AddInfo(AnastasisDrainage::Describe(Net));
	T.AddInfo(FString::Printf(TEXT("check uphill=%d narrowing=%d confluence_narrower=%d dangling=%d isolated=%d lakes_without_role=%d containment=%.3f"),
		C.UphillSteps, C.NarrowingSteps, C.ConfluenceNarrower, C.DanglingMouths, C.IsolatedWaterBodies, C.LakesWithoutRole, C.BankContainment));
	T.TestEqual(TEXT("water surface never rises downstream"), C.UphillSteps, 0);
	T.TestEqual(TEXT("channels never narrow downstream"), C.NarrowingSteps, 0);
	T.TestEqual(TEXT("below a confluence the receiver is at least as wide as the tributary"), C.ConfluenceNarrower, 0);
	T.TestEqual(TEXT("every mouth reaches a river, a lake or the world edge"), C.DanglingMouths, 0);
	T.TestEqual(TEXT("no rendered water body outside the network"), C.IsolatedWaterBodies, 0);
	T.TestEqual(TEXT("every interior lake is fed or drained"), C.LakesWithoutRole, 0);
	T.TestTrue(TEXT("banks contain the water on 90% of probed points"), C.BankContainment >= 0.9);
	// Clairseme : quelques rivieres lisibles, pas des dizaines.
	T.TestTrue(TEXT("sparse network: 3..20 rivers"), Net.Rivers.Num() >= 3 && Net.Rivers.Num() <= 20);
	T.TestTrue(TEXT("at most MaxHeads headwaters"), Net.Heads <= AnastasisDrainage::FParams().MaxHeads);
	T.TestTrue(TEXT("tributaries join: at least one confluence"), Net.Confluences >= 1);
	T.TestTrue(TEXT("few wetlands"), Net.Wetlands.Num() <= AnastasisDrainage::FParams().MaxWetlands);
	int32 MaxOrder = 0;
	for (const AnastasisDrainage::FRiver& River : Net.Rivers) MaxOrder = FMath::Max(MaxOrder, River.Order);
	T.TestTrue(TEXT("a hierarchy exists (Strahler >= 2)"), MaxOrder >= 2);
	return true;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisDrainageNetwork, "Anastasis.Terrain.Drainage.Network",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnastasisDrainageNetwork::RunTest(const FString&)
{
	FWorldVisualSnapshot S;
	AnastasisTerrainForge::FMesh M;
	AnastasisDrainage::FNetwork Net;
	if (!TestTrue(TEXT("Human_Geography_V2 world drains"), Drain(true, S, M, Net))) return false;
	ExpectCoherent(*this, Net, M);

	// Largeur et profondeur croissent de la source a l'embouchure, sur chaque riviere de plus de 300 m.
	for (const AnastasisDrainage::FRiver& River : Net.Rivers)
	{
		if (River.LengthM < 300.0) continue;
		TestTrue(TEXT("wider at the mouth than at the source"), River.Points.Last().Width > River.Points[0].Width
			|| River.Points[0].Width >= AnastasisDrainage::FParams().MaxWidthM * 100.0 - 1.0);
		TestTrue(TEXT("water descends from source to mouth"), River.Points.Last().Location.Z <= River.Points[0].Location.Z);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisDrainageOriginalForms, "Anastasis.Terrain.Drainage.OriginalForms",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnastasisDrainageOriginalForms::RunTest(const FString&)
{
	// La couche ne depend pas des rivieres ecrites : le relief corrige seul draine aussi.
	FWorldVisualSnapshot S;
	AnastasisTerrainForge::FMesh M;
	AnastasisDrainage::FNetwork Net;
	if (!TestTrue(TEXT("original-forms world drains"), Drain(false, S, M, Net))) return false;
	ExpectCoherent(*this, Net, M);
	for (const AnastasisDrainage::FRiver& River : Net.Rivers) TestFalse(TEXT("no authored river without the layer"), River.bAuthored);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisDrainageAuthored, "Anastasis.Terrain.Drainage.KeepsAuthoredRivers",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnastasisDrainageAuthored::RunTest(const FString&)
{
	FWorldVisualSnapshot S;
	AnastasisTerrainForge::FMesh M;
	AnastasisDrainage::FNetwork Net;
	if (!TestTrue(TEXT("drains"), Drain(true, S, M, Net))) return false;
	AnastasisTerrainForge::SetActive(M);
	const double Unit = TileWorldSize * S.SpatialScale;
	int32 Probed = 0, Wet = 0;
	for (const TArray<FVector>& Poly : AnastasisHumanGeography::AuthoredRivers())
	{
		// Chaque riviere ecrite reste de l'eau courante, du premier au dernier tiers de son trace.
		for (int32 K = Poly.Num() / 6; K < Poly.Num() * 5 / 6; K += 3)
		{
			const double X = Poly[K].X * Unit, Y = Poly[K].Y * Unit;
			double Ground = 0.0, Water = 0.0;
			if (!AnastasisTerrainForge::SampleActive(X, Y, Ground) || !AnastasisTerrainForge::SampleActiveWater(X, Y, Water)) continue;
			++Probed;
			Wet += Ground < Water ? 1 : 0;
		}
	}
	AnastasisTerrainForge::ClearActive();
	AddInfo(FString::Printf(TEXT("authored centreline under water: %d / %d"), Wet, Probed));
	TestTrue(TEXT("authored rivers were probed"), Probed > 20);
	TestTrue(TEXT("authored rivers stay water on 95% of their centreline"), Wet >= Probed * 95 / 100);
	int32 AuthoredRivers = 0;
	for (const AnastasisDrainage::FRiver& River : Net.Rivers) AuthoredRivers += River.bAuthored ? 1 : 0;
	TestTrue(TEXT("the network follows authored rivers"), AuthoredRivers >= 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisDrainageDeterminism, "Anastasis.Terrain.Drainage.DeterminismAndSnapshot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnastasisDrainageDeterminism::RunTest(const FString&)
{
	FWorldVisualSnapshot S;
	AnastasisTerrainForge::FMesh A, B;
	if (!TestTrue(TEXT("forge"), Forge(true, S, A))) return false;
	B = A;
	const FWorldVisualSnapshot Before = S;
	AnastasisDrainage::FNetwork NA, NB;
	if (!TestTrue(TEXT("first run"), AnastasisDrainage::Apply(S, A, NA))) return false;
	if (!TestTrue(TEXT("second run"), AnastasisDrainage::Apply(S, B, NB))) return false;
	bool bSame = A.Geometry.Vertices.Num() == B.Geometry.Vertices.Num();
	for (int32 I = 0; bSame && I < A.Geometry.Vertices.Num(); ++I)
	{
		bSame = A.Geometry.Vertices[I] == B.Geometry.Vertices[I] && A.Geometry.WaterVertices[I] == B.Geometry.WaterVertices[I];
	}
	TestTrue(TEXT("same relief and water, bit for bit"), bSame);
	TestEqual(TEXT("same rivers"), NA.Rivers.Num(), NB.Rivers.Num());
	// La simulation n'est pas touchee : Alt, Type, FlowAmt restent ceux du snapshot.
	bool bSnapshot = Before.Tiles.Num() == S.Tiles.Num();
	for (int32 I = 0; bSnapshot && I < S.Tiles.Num(); ++I)
	{
		bSnapshot = Before.Tiles[I].Alt == S.Tiles[I].Alt && Before.Tiles[I].Type == S.Tiles[I].Type && Before.Tiles[I].FlowAmt == S.Tiles[I].FlowAmt;
	}
	TestTrue(TEXT("simulation snapshot untouched"), bSnapshot);

	// Grille incoherente : refus, et rien n'est ecrit.
	AnastasisTerrainForge::FMesh Bad = A;
	Bad.FineW -= 1;
	const TArray<FVector> Kept = Bad.Geometry.Vertices;
	AnastasisDrainage::FNetwork NBad;
	TestFalse(TEXT("inconsistent grid rejected"), AnastasisDrainage::Apply(S, Bad, NBad));
	TestTrue(TEXT("rejected grid unchanged"), Bad.Geometry.Vertices == Kept);
	return true;
}
#endif
