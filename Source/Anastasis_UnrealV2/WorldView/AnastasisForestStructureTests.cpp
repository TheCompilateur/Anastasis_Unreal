#include "WorldView/AnastasisForestStructure.h"
#include "WorldView/AnastasisHumanGeography.h"
#include "WorldView/AnastasisTerrainSurface.h"
#include "WorldView/AnastasisTerrainForge.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
using AnastasisEcologicalDressing::ELayer;
using AnastasisForestStructure::EStand;

AnastasisWorldView::FWorldVisualSnapshot FlatForest()
{
	auto S = AnastasisWorldView::CaptureCanonicalWorld(12345u);
	for (auto& T : S.Tiles)
	{
		T.Type = T.X >= 40 ? AnastasisWorld::ETileType::Forest : AnastasisWorld::ETileType::Grass;
		T.Alt = 0.5;
		T.Wetness = 0.0;
	}
	return S;
}

double MeanScale(const AnastasisEcologicalDressing::FPlan& Plan, const TArray<AnastasisForestStructure::FNote>& Notes, EStand Stand)
{
	double Sum = 0.0;
	int32 Count = 0;
	for (int32 I = 0; I < Plan.Instances.Num(); ++I)
		if (Notes.IsValidIndex(I) && Notes[I].Stand == Stand)
		{
			Sum += Plan.Instances[I].ScaleMultiplier;
			++Count;
		}
	return Count > 0 ? Sum / static_cast<double>(Count) : 0.0;
}

int32 CountLayer(const AnastasisEcologicalDressing::FPlan& Plan, const TArray<AnastasisForestStructure::FNote>& Notes,
	EStand Stand, ELayer Layer)
{
	int32 Count = 0;
	for (int32 I = 0; I < Plan.Instances.Num(); ++I)
		if (Notes.IsValidIndex(I) && Notes[I].Stand == Stand && Plan.Instances[I].Layer == Layer) ++Count;
	return Count;
}

bool TooClose(const AnastasisEcologicalDressing::FPlan& Plan, double Limit)
{
	const double Cell = FMath::Max(Limit, 1.0);
	TMap<FIntPoint, TArray<int32>> Grid;
	for (int32 I = 0; I < Plan.Instances.Num(); ++I)
	{
		const FVector& G = Plan.Instances[I].Ground;
		Grid.FindOrAdd(FIntPoint(FMath::FloorToInt(G.X / Cell), FMath::FloorToInt(G.Y / Cell))).Add(I);
	}
	for (int32 I = 0; I < Plan.Instances.Num(); ++I)
	{
		const FIntPoint Home(FMath::FloorToInt(Plan.Instances[I].Ground.X / Cell), FMath::FloorToInt(Plan.Instances[I].Ground.Y / Cell));
		for (int32 DY = -1; DY <= 1; ++DY)
			for (int32 DX = -1; DX <= 1; ++DX)
				if (const TArray<int32>* Neighbors = Grid.Find(Home + FIntPoint(DX, DY)))
					for (int32 J : *Neighbors)
						if (J > I && FVector::DistSquared2D(Plan.Instances[I].Ground, Plan.Instances[J].Ground) < Limit * Limit - 1.0)
							return true;
	}
	return false;
}

double MeanNeighbor(const AnastasisEcologicalDressing::FPlan& Plan, const TArray<AnastasisForestStructure::FNote>& Notes, bool bPacked)
{
	const double Cell = 800.0;
	TMap<FIntPoint, TArray<int32>> Grid;
	for (int32 I = 0; I < Plan.Instances.Num(); ++I)
	{
		const FVector& G = Plan.Instances[I].Ground;
		Grid.FindOrAdd(FIntPoint(FMath::FloorToInt(G.X / Cell), FMath::FloorToInt(G.Y / Cell))).Add(I);
	}
	double Sum = 0.0;
	int32 Count = 0;
	for (int32 I = 0; I < Plan.Instances.Num(); ++I)
	{
		if (!Notes.IsValidIndex(I)) continue;
		const bool Packed = Notes[I].bPacked && !Notes[I].bClearingInterior && !Notes[I].bClearingBorder;
		const bool Open = Notes[I].Cluster < 0.40 && !Notes[I].bClearingInterior && !Notes[I].bClearingBorder && !Notes[I].bPacked;
		if (bPacked ? !Packed : !Open) continue;
		const FIntPoint Home(FMath::FloorToInt(Plan.Instances[I].Ground.X / Cell), FMath::FloorToInt(Plan.Instances[I].Ground.Y / Cell));
		double Best = TNumericLimits<double>::Max();
		for (int32 DY = -2; DY <= 2; ++DY)
			for (int32 DX = -2; DX <= 2; ++DX)
				if (const TArray<int32>* Neighbors = Grid.Find(Home + FIntPoint(DX, DY)))
					for (int32 J : *Neighbors)
						if (J != I)
							Best = FMath::Min(Best, FVector::DistSquared2D(Plan.Instances[I].Ground, Plan.Instances[J].Ground));
		if (Best < TNumericLimits<double>::Max())
		{
			Sum += FMath::Sqrt(Best);
			++Count;
		}
	}
	return Count >= 25 ? Sum / static_cast<double>(Count) : -1.0;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisForestStructureShape, "Anastasis.Ecology.ForestStructure",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnastasisForestStructureShape::RunTest(const FString&)
{
	using namespace AnastasisForestStructure;
	const auto Source = FlatForest();
	const auto BeforeTiles = Source.Tiles;
	FAnastasisForestDressingSettings Settings;
	AnastasisEcologicalDressing::FPlan Built;
	FString Error;
	TestTrue(TEXT("flat forest builds"), AnastasisEcologicalDressing::Build(Source, Settings, Built, Error));
	const int32 Before = Built.Instances.Num();
	TestTrue(TEXT("flat forest has trees"), Before > 100);
	AnastasisEcologicalDressing::FPlan A = Built, B = Built;
	TArray<FNote> NotesA, NotesB;
	FReport ReportA, ReportB;
	TestTrue(TEXT("shape A"), Shape(Source, Settings, false, A, NotesA, ReportA, Error));
	TestTrue(TEXT("shape B"), Shape(Source, Settings, false, B, NotesB, ReportB, Error));
	TestEqual(TEXT("does not add trees"), ReportA.After <= ReportA.Before, true);
	TestTrue(TEXT("structure removes the uniform fill"), ReportA.After < ReportA.Before);
	TestTrue(TEXT("a mass remains"), ReportA.After > Before / 3);
	TestEqual(TEXT("deterministic count"), A.Instances.Num(), B.Instances.Num());
	TestEqual(TEXT("notes follow survivors"), NotesA.Num(), A.Instances.Num());
	for (int32 I = 0; I < A.Instances.Num(); ++I)
	{
		TestTrue(TEXT("same ground"), A.Instances[I].Ground.Equals(B.Instances[I].Ground, 1.0e-6));
		TestEqual(TEXT("same scale"), A.Instances[I].ScaleMultiplier, B.Instances[I].ScaleMultiplier);
		TestEqual(TEXT("same stand"), static_cast<int32>(NotesA[I].Stand), static_cast<int32>(NotesB[I].Stand));
	}
	TestTrue(TEXT("young stands exist"), ReportA.ByStand[static_cast<int32>(EStand::Young)] > 20);
	TestTrue(TEXT("old stands exist"), ReportA.ByStand[static_cast<int32>(EStand::Old)] > 20);
	TestTrue(TEXT("clearings meet the forest"), ReportA.ByStand[static_cast<int32>(EStand::Clearing)] > 10);
	TestEqual(TEXT("one or two large clearings"), ReportA.LargeClearings >= 1 && ReportA.LargeClearings <= 2, true);
	TestTrue(TEXT("small clearings exist"), ReportA.SmallClearings >= 3);
	const int32 YoungTrees = ReportA.ByStand[static_cast<int32>(EStand::Young)];
	const int32 OldTrees = ReportA.ByStand[static_cast<int32>(EStand::Old)];
	TestTrue(TEXT("young stands read as young"), CountLayer(A, NotesA, EStand::Young, ELayer::Young) * 2 > YoungTrees);
	TestTrue(TEXT("old stands read as canopy"), CountLayer(A, NotesA, EStand::Old, ELayer::Canopy) * 2 > OldTrees);
	TestTrue(TEXT("old trees stand taller than the regrowth"), MeanScale(A, NotesA, EStand::Old) > MeanScale(A, NotesA, EStand::Young) * 1.4);
	const double Packed = MeanNeighbor(A, NotesA, true);
	const double Open = MeanNeighbor(A, NotesA, false);
	if (Packed > 0.0 && Open > 0.0)
		TestTrue(TEXT("clumps are tighter than the gaps"), Packed < Open * 0.92);
	else
		AddInfo(TEXT("neighbor sample too small to compare clumps"));
	const double Floor = Settings.YoungScale.X - 1.0e-4;
	const double Ceiling = Settings.CanopyScale.Y + 1.0e-4;
	for (const auto& Tree : A.Instances)
	{
		TestTrue(TEXT("scale stays inside the existing envelopes"), Tree.ScaleMultiplier >= Floor && Tree.ScaleMultiplier <= Ceiling);
	}
	TestFalse(TEXT("packed spacing holds"), TooClose(A, Settings.MinimumSpacing * AnastasisWorldView::TileWorldSize * Source.SpatialScale * 0.64));
	for (int32 I = 0; I < Source.Tiles.Num(); ++I)
		if (Source.Tiles[I].Alt != BeforeTiles[I].Alt || Source.Tiles[I].Type != BeforeTiles[I].Type)
			AddError(TEXT("snapshot mutated"));
	AddInfo(FString::Printf(TEXT("FOREST_STRUCTURE flat before=%d after=%d young=%d mature=%d old=%d disturbed=%d clearing=%d large=%d moved=%d packed_nn=%.0f open_nn=%.0f"),
		ReportA.Before, ReportA.After,
		ReportA.ByStand[0], ReportA.ByStand[1], ReportA.ByStand[2], ReportA.ByStand[3], ReportA.ByStand[4],
		ReportA.LargeClearings, ReportA.Moved, Packed, Open));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisForestStructureRelief, "Anastasis.Ecology.ForestStructureRelief",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnastasisForestStructureRelief::RunTest(const FString&)
{
	using namespace AnastasisForestStructure;
	auto S = AnastasisWorldView::CaptureCanonicalWorld(AnastasisWorldView::ReferenceSeed);
	S.SpatialScale = 5.0;
	S.bHumanGeography = true;
	AnastasisTerrainSurface::FGeometry Geometry;
	AnastasisTerrainForge::FMesh Mesh;
	if (!TestTrue(TEXT("surface"), AnastasisTerrainSurface::Build(S, Geometry))
		|| !TestTrue(TEXT("relief"), AnastasisTerrainForge::Apply(S, Geometry, Mesh))) return false;
	AnastasisEcologicalDressing::FRenderedHabitat Habitat;
	Habitat.SampleHeight = [&](double X, double Y, double& Z) { return AnastasisTerrainForge::SampleHeight(Mesh, X, Y, Z); };
	Habitat.SampleWaterHeight = [](double X, double Y, double& Z) { return AnastasisTerrainForge::SampleActiveWater(X, Y, Z); };
	Habitat.Basin = FVector(Mesh.BasinX, Mesh.BasinY, Mesh.BasinZ);
	Habitat.bHasBasin = Mesh.bBasinFound;
	FAnastasisForestDressingSettings Settings;
	AnastasisEcologicalDressing::FPlan Plan;
	FString Error;
	TestTrue(TEXT("macro plan"), AnastasisEcologicalDressing::Build(S, Settings, Plan, Error, &Habitat));
	const int32 Before = Plan.Instances.Num();
	TArray<FNote> Notes;
	FReport Report;
	TestTrue(TEXT("shape relief"), Shape(S, Settings, true, Plan, Notes, Report, Error));
	TestTrue(TEXT("macro mass remains"), Report.After > 4000);
	TestTrue(TEXT("macro fill is broken up"), Report.After < Before);
	TestEqual(TEXT("large clearings stay rare"), Report.LargeClearings >= 1 && Report.LargeClearings <= 2, true);
	const double Mul = Settings.HeightMultiplier;
	const double Ceiling = Settings.CanopyScale.Y * Mul + 1.0e-3;
	int32 Crests = 0;
	double CrestScale = 0.0, MatureScale = 0.0;
	int32 Matures = 0;
	for (int32 I = 0; I < Plan.Instances.Num(); ++I)
	{
		TestTrue(TEXT("macro scale stays inside the current envelopes"), Plan.Instances[I].ScaleMultiplier <= Ceiling);
		TestTrue(TEXT("under the 30m guard, scale is not a second height pass"), Plan.Instances[I].ScaleMultiplier <= Mul * Settings.CanopyScale.Y + 1.0e-3);
		if (Notes[I].bCrest)
		{
			++Crests;
			CrestScale += Plan.Instances[I].ScaleMultiplier;
		}
		else if (Notes[I].Stand == EStand::Mature || Notes[I].Stand == EStand::Old)
		{
			++Matures;
			MatureScale += Plan.Instances[I].ScaleMultiplier;
		}
		if (S.bHumanGeography && !Plan.Instances[I].bLone)
		{
			const auto Geo = AnastasisHumanGeography::Evaluate(
				Plan.Instances[I].Ground.X / (AnastasisWorldView::TileWorldSize * S.SpatialScale),
				Plan.Instances[I].Ground.Y / (AnastasisWorldView::TileWorldSize * S.SpatialScale), 0.0);
			TestTrue(TEXT("valleys stay open"), Geo.ValleyWeight < 0.8);
			TestTrue(TEXT("river stays open"), Geo.RiverWeight < 0.5);
		}
	}
	if (Crests >= 10 && Matures > 0)
		TestTrue(TEXT("crests carry smaller trees than the stands below"), CrestScale / Crests < MatureScale / Matures);
	AddInfo(FString::Printf(TEXT("FOREST_STRUCTURE relief before=%d after=%d crests=%d young=%d old=%d clearing=%d moved=%d"),
		Before, Report.After, Crests, Report.ByStand[0], Report.ByStand[2], Report.ByStand[4], Report.Moved));
	return true;
}
#endif
