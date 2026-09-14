#include "WorldView/AnastasisGeologicalDressing.h"
#include "WorldView/AnastasisTerrainSurface.h"
#include "Misc/AutomationTest.h"
#include <limits>

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
AnastasisWorldView::FWorldVisualSnapshot CliffStep()
{
	auto S = AnastasisWorldView::CaptureCanonicalWorld(12345u);
	for (auto& T : S.Tiles)
	{
		T.Type = AnastasisWorld::ETileType::Grass;
		T.Wetness = 0.0;
		T.Alt = T.X < 48 ? 0.30 : 0.88;
	}
	return S;
}

AnastasisWorldView::FWorldVisualSnapshot FlatPlateau()
{
	auto S = AnastasisWorldView::CaptureCanonicalWorld(12345u);
	for (auto& T : S.Tiles)
	{
		T.Type = AnastasisWorld::ETileType::Grass;
		T.Wetness = 0.0;
		T.Alt = 0.55;
	}
	return S;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisLithosDeterminism, "Anastasis.Lithos.DeterminismAndCausality",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnastasisLithosDeterminism::RunTest(const FString&)
{
	using namespace AnastasisGeologicalDressing;
	const auto S = AnastasisWorldView::CaptureCanonicalWorld(12345u);
	const auto Before = S.Tiles;
	FAnastasisLithosDressingSettings C;
	FPlan A, B;
	FString E;
	TestTrue(TEXT("canonical plan A"), Build(S, C, A, E));
	TestTrue(TEXT("canonical plan B"), Build(S, C, B, E));
	TestTrue(TEXT("reachable geology"), A.Instances.Num() > 0);
	TestEqual(TEXT("deterministic count"), A.Instances.Num(), B.Instances.Num());
	TestTrue(TEXT("cliff context used"), A.CliffCount > 0);
	TestTrue(TEXT("talus context used"), A.TalusCount > 0);
	int32 Kinds[KindCount] = {};
	for (int32 I = 0; I < A.Instances.Num(); ++I)
	{
		const auto& P = A.Instances[I];
		if (B.Instances.IsValidIndex(I))
		{
			TestTrue(TEXT("exact placement"), P.Ground == B.Instances[I].Ground);
			TestEqual(TEXT("exact kind"), static_cast<int32>(P.Kind), static_cast<int32>(B.Instances[I].Kind));
			TestEqual(TEXT("exact scale"), P.ScaleMultiplier, B.Instances[I].ScaleMultiplier);
			TestEqual(TEXT("exact seed"), P.VisualSeed, B.Instances[I].VisualSeed);
		}
		double Z;
		TestTrue(TEXT("rendered ground exists"), AnastasisTerrainSurface::SampleHeight(S, P.Ground.X, P.Ground.Y, Z));
		TestTrue(TEXT("anchored exactly"), FMath::IsNearlyEqual(Z, P.Ground.Z, 1.e-9));
		TestTrue(TEXT("above water"), Z > AnastasisTerrainSurface::WaterPlaneZ + C.WaterClearanceUU);
		++Kinds[static_cast<uint8>(P.Kind)];
	}
	for (int32 I = 0; I < S.Tiles.Num(); ++I)
	{
		if (S.Tiles[I].Alt != Before[I].Alt || S.Tiles[I].Type != Before[I].Type
			|| S.Tiles[I].Wetness != Before[I].Wetness || S.Tiles[I].Amount != Before[I].Amount)
		{
			AddError(TEXT("snapshot mutated"));
		}
	}
	int32 Distinct = 0;
	for (int32 K = 0; K < KindCount; ++K)
	{
		Distinct += Kinds[K] > 0;
	}
	TestTrue(TEXT("several formation families reachable"), Distinct >= 6);
	AddInfo(FString::Printf(
		TEXT("seed=12345 instances=%d cliff=%d slope=%d talus=%d summit=%d water=%d flat=%d spacing=%d families=%d"),
		A.Instances.Num(), A.CliffCount, A.SlopeCount, A.TalusCount, A.SummitCount,
		A.RejectedWater, A.RejectedFlat, A.RejectedSpacing, Distinct));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisLithosCliffGrammar, "Anastasis.Lithos.CliffFootAndFlat",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnastasisLithosCliffGrammar::RunTest(const FString&)
{
	using namespace AnastasisGeologicalDressing;
	FAnastasisLithosDressingSettings C;
	FPlan Cliff, Flat, Disabled;
	FString E;
	const auto Step = CliffStep();
	TestTrue(TEXT("cliff fixture"), Build(Step, C, Cliff, E));
	TestTrue(TEXT("cliff produces formations"), Cliff.Instances.Num() > 0);
	TestTrue(TEXT("walls on the rupture"), Cliff.CliffCount > 0);
	TestTrue(TEXT("talus at the foot"), Cliff.TalusCount > 0);

	int32 LowFlat = 0, HighInterior = 0, OnBreak = 0, FootBand = 0;
	int32 Wallish = 0, Talusish = 0;
	for (const auto& P : Cliff.Instances)
	{
		const double X = P.Ground.X / AnastasisWorldView::TileWorldSize;
		if (X < 40.0)
		{
			++LowFlat;
		}
		if (X > 58.0)
		{
			++HighInterior;
		}
		if (X >= 46.0 && X < 50.0)
		{
			++OnBreak;
		}
		if (X >= 44.0 && X < 48.0)
		{
			++FootBand;
		}
		if (P.Kind == EKind::VerticalWall || P.Kind == EKind::Stratum
			|| P.Kind == EKind::Fractured || P.Kind == EKind::InclinedWall || P.Kind == EKind::Cornice)
		{
			++Wallish;
		}
		if (P.Kind == EKind::TalusCluster || P.Kind == EKind::DetachedBlock || P.Kind == EKind::Transition)
		{
			++Talusish;
		}
	}
	TestEqual(TEXT("no geology on the distant low flat"), LowFlat, 0);
	TestTrue(TEXT("plateau interior is not a rock dump"), HighInterior < OnBreak);
	TestTrue(TEXT("rupture carries the walls"), OnBreak > 0 && Wallish > 0);
	TestTrue(TEXT("foot carries the debris"), FootBand > 0 && Talusish > 0);

	TestTrue(TEXT("flat fixture"), Build(FlatPlateau(), C, Flat, E));
	TestEqual(TEXT("flat world has no geology"), Flat.Instances.Num(), 0);

	C.bEnabled = false;
	TestTrue(TEXT("disable accepted"), Build(Step, C, Disabled, E));
	TestEqual(TEXT("disabled empty"), Disabled.Instances.Num(), 0);

	AddInfo(FString::Printf(
		TEXT("step=%d cliff=%d talus=%d summit=%d on_break=%d foot=%d high_interior=%d wallish=%d talusish=%d"),
		Cliff.Instances.Num(), Cliff.CliffCount, Cliff.TalusCount, Cliff.SummitCount,
		OnBreak, FootBand, HighInterior, Wallish, Talusish));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisLithosBoundary, "Anastasis.Lithos.RejectInvalidInput",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnastasisLithosBoundary::RunTest(const FString&)
{
	using namespace AnastasisGeologicalDressing;
	auto S = CliffStep();
	FAnastasisLithosDressingSettings C;
	FPlan P;
	FString E;
	S.Tiles[17].Wetness = std::numeric_limits<double>::quiet_NaN();
	TestFalse(TEXT("NaN rejected"), Build(S, C, P, E));
	TestTrue(TEXT("precise tile path"), E.Contains(TEXT("Source.Tiles[17]")));
	TestEqual(TEXT("no partial placements"), P.Instances.Num(), 0);
	S = CliffStep();
	C.CliffSpacing = 0.0f;
	TestFalse(TEXT("invalid configuration rejected"), Build(S, C, P, E));
	C = FAnastasisLithosDressingSettings{};
	S = AnastasisWorldView::CropSnapshot(S, 0, 0, 32, 32);
	TestFalse(TEXT("crop cannot invent missing geological context"), Build(S, C, P, E));
	return true;
}
#endif
