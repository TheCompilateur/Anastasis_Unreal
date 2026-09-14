#include "WorldView/AnastasisEcotoneDressing.h"
#include "WorldView/AnastasisTerrainSurface.h"
#include "Misc/AutomationTest.h"
#include <limits>

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
using AnastasisWorld::ETileType;

AnastasisWorldView::FWorldVisualSnapshot FlatSplit(ETileType Left, ETileType Right, double LeftAlt, double RightAlt)
{
	auto S = AnastasisWorldView::CaptureCanonicalWorld(12345u);
	for (auto& T : S.Tiles)
	{
		const bool bRight = T.X >= 48;
		T.Type = bRight ? Right : Left;
		T.Alt = bRight ? RightAlt : LeftAlt;
		T.Wetness = 0.0;
		T.Shore = 0.0;
	}
	return S;
}

void PaintShore(AnastasisWorldView::FWorldVisualSnapshot& S)
{
	for (auto& T : S.Tiles)
	{
		if (T.Type == ETileType::Water)
		{
			T.Shore = 0.0;
			continue;
		}
		double Best = 0.0;
		for (int32 DY = -5; DY <= 5; ++DY)
		{
			for (int32 DX = -5; DX <= 5; ++DX)
			{
				const auto* N = AnastasisWorldView::FindTile(S, T.X + DX, T.Y + DY);
				if (!N || N->Type != ETileType::Water)
				{
					continue;
				}
				const double Dist = FVector2D(static_cast<double>(DX), static_cast<double>(DY)).Size();
				Best = FMath::Max(Best, FMath::Clamp(1.0 - Dist / 5.0, 0.0, 1.0));
			}
		}
		T.Shore = Best;
	}
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisEcotoneDeterminism, "Anastasis.Ecotone.DeterminismAndAnchoring",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnastasisEcotoneDeterminism::RunTest(const FString&)
{
	using namespace AnastasisEcotoneDressing;
	const auto S = AnastasisWorldView::CaptureCanonicalWorld(12345u);
	const auto Before = S.Tiles;
	FAnastasisEcotoneDressingSettings C;
	FPlan A, B;
	FString E;
	TestTrue(TEXT("canonical plan A"), Build(S, C, A, E));
	TestTrue(TEXT("canonical plan B"), Build(S, C, B, E));
	TestTrue(TEXT("reachable ecotone"), A.Instances.Num() > 0);
	TestEqual(TEXT("deterministic count"), A.Instances.Num(), B.Instances.Num());
	int32 ContextsHit = 0;
	for (int32 I = 0; I < ContextCount; ++I)
	{
		ContextsHit += A.ContextCounts[I] > 0;
	}
	for (int32 I = 0; I < A.Instances.Num(); ++I)
	{
		const FPlacement& P = A.Instances[I];
		if (B.Instances.IsValidIndex(I))
		{
			TestTrue(TEXT("exact placement"), P.Ground == B.Instances[I].Ground);
			TestEqual(TEXT("exact scale"), P.ScaleMultiplier, B.Instances[I].ScaleMultiplier);
			TestEqual(TEXT("exact yaw"), P.YawDegrees, B.Instances[I].YawDegrees);
			TestEqual(TEXT("exact asset"), static_cast<int32>(P.Asset), static_cast<int32>(B.Instances[I].Asset));
			TestEqual(TEXT("exact context"), static_cast<int32>(P.Context), static_cast<int32>(B.Instances[I].Context));
			TestEqual(TEXT("exact companion"), P.bCompanion, B.Instances[I].bCompanion);
		}
		double Z;
		TestTrue(TEXT("rendered ground exists"), AnastasisTerrainSurface::SampleHeight(S, P.Ground.X, P.Ground.Y, Z));
		TestTrue(TEXT("anchored exactly"), FMath::IsNearlyEqual(Z, P.Ground.Z, 1.e-9));
		TestTrue(TEXT("above water"), Z > AnastasisTerrainSurface::WaterPlaneZ + C.WaterClearanceUU);
		TestTrue(TEXT("slope bound"), P.SlopeDegrees <= C.MaxSlopeDegrees);
	}
	for (int32 I = 0; I < S.Tiles.Num(); ++I)
	{
		if (S.Tiles[I].Alt != Before[I].Alt || S.Tiles[I].Type != Before[I].Type
			|| S.Tiles[I].Wetness != Before[I].Wetness || S.Tiles[I].Shore != Before[I].Shore)
		{
			AddError(TEXT("snapshot mutated"));
		}
	}
	TestTrue(TEXT("more than one ecological context"), ContextsHit >= 2);
	AddInfo(FString::Printf(
		TEXT("seed=12345 instances=%d understory=%d edge=%d shore=%d rock=%d companions=%d water=%d slope=%d spacing=%d"),
		A.Instances.Num(), A.ContextCounts[0], A.ContextCounts[1], A.ContextCounts[2], A.ContextCounts[3],
		A.CompanionCount, A.RejectedWaterOrFootprint, A.RejectedSlope, A.RejectedSpacing));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisEcotoneContexts, "Anastasis.Ecotone.ThreeContexts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnastasisEcotoneContexts::RunTest(const FString&)
{
	using namespace AnastasisEcotoneDressing;
	FAnastasisEcotoneDressingSettings C;
	FString E;

	auto Forest = FlatSplit(ETileType::Grass, ETileType::Forest, 0.50, 0.50);
	FPlan ForestPlan;
	TestTrue(TEXT("forest-edge fixture"), Build(Forest, C, ForestPlan, E));
	int32 ForestEdge = 0, ForestUnder = 0, ForestShore = 0, ForestRock = 0;
	int32 ForestStory = 0;
	for (const FPlacement& P : ForestPlan.Instances)
	{
		const double X = P.Ground.X / 100.0;
		TestTrue(TEXT("no distant prairie scatter"), X > 44.0);
		ForestEdge += P.Context == EContext::ForestEdge;
		ForestUnder += P.Context == EContext::Understory;
		ForestShore += P.Context == EContext::Shore;
		ForestRock += P.Context == EContext::RockFoot;
		ForestStory += (P.Asset == EAsset::Stump || P.Asset == EAsset::FallenLog
			|| P.Asset == EAsset::BushLow || P.Asset == EAsset::Sapling);
	}
	TestTrue(TEXT("forest fringe is an ecotone"), ForestEdge > 0);
	TestTrue(TEXT("wooded interior still has a ground layer"), ForestUnder > 0);
	TestTrue(TEXT("forest fixture is not a shoreline"), ForestShore == 0);
	TestTrue(TEXT("forest fixture is not a rock foot"), ForestRock == 0);
	TestTrue(TEXT("dead wood or transition plants exist"), ForestStory > 0);

	auto Shore = FlatSplit(ETileType::Water, ETileType::Grass, 0.27, 0.30);
	PaintShore(Shore);
	FPlan ShorePlan;
	TestTrue(TEXT("shore fixture"), Build(Shore, C, ShorePlan, E));
	int32 ShoreCtx = 0, ShoreReeds = 0, ShoreOnWater = 0;
	for (const FPlacement& P : ShorePlan.Instances)
	{
		ShoreCtx += P.Context == EContext::Shore;
		ShoreReeds += (P.Asset == EAsset::Reed || P.Asset == EAsset::ShoreTuft || P.Asset == EAsset::Driftwood);
		const int32 TX = FMath::FloorToInt(P.Ground.X / 100.0);
		const int32 TY = FMath::FloorToInt(P.Ground.Y / 100.0);
		if (const auto* Tile = AnastasisWorldView::FindTile(Shore, TX, TY))
		{
			ShoreOnWater += Tile->Type == ETileType::Water;
		}
	}
	TestTrue(TEXT("shoreline context reachable"), ShoreCtx > 0);
	TestTrue(TEXT("wet vegetation or driftwood on the bank"), ShoreReeds > 0);
	TestEqual(TEXT("nothing planted in the water"), ShoreOnWater, 0);

	auto Rock = FlatSplit(ETileType::Stone, ETileType::Grass, 0.50, 0.50);
	FPlan RockPlan;
	TestTrue(TEXT("rock-foot fixture"), Build(Rock, C, RockPlan, E));
	int32 RockCtx = 0, RockMatter = 0;
	for (const FPlacement& P : RockPlan.Instances)
	{
		RockCtx += P.Context == EContext::RockFoot;
		RockMatter += (P.Asset == EAsset::RockCluster || P.Asset == EAsset::BuriedBlock
			|| P.Asset == EAsset::ExposedRoots || P.Asset == EAsset::GrassTuft);
	}
	TestTrue(TEXT("rock-foot context reachable"), RockCtx > 0);
	TestTrue(TEXT("stone deposits or interstitial plants"), RockMatter > 0);

	C.bEnabled = false;
	FPlan Disabled;
	TestTrue(TEXT("disable accepted"), Build(Forest, C, Disabled, E));
	TestEqual(TEXT("disabled empty"), Disabled.Instances.Num(), 0);

	AddInfo(FString::Printf(
		TEXT("edge=%d understory=%d shore=%d reeds=%d rock=%d"),
		ForestEdge, ForestUnder, ShoreCtx, ShoreReeds, RockCtx));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisEcotoneBoundary, "Anastasis.Ecotone.RejectInvalidInput",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnastasisEcotoneBoundary::RunTest(const FString&)
{
	using namespace AnastasisEcotoneDressing;
	auto S = FlatSplit(ETileType::Grass, ETileType::Forest, 0.50, 0.50);
	FAnastasisEcotoneDressingSettings C;
	FPlan P;
	FString E;
	S.Tiles[17].Wetness = std::numeric_limits<double>::quiet_NaN();
	TestFalse(TEXT("NaN rejected"), Build(S, C, P, E));
	TestTrue(TEXT("precise tile path"), E.Contains(TEXT("Source.Tiles[17]")));
	TestEqual(TEXT("no partial placements"), P.Instances.Num(), 0);
	S = FlatSplit(ETileType::Grass, ETileType::Forest, 0.50, 0.50);
	C.MinimumSpacing = 0.0f;
	TestFalse(TEXT("invalid configuration rejected"), Build(S, C, P, E));
	C = FAnastasisEcotoneDressingSettings{};
	S = AnastasisWorldView::CropSnapshot(S, 0, 0, 32, 32);
	TestFalse(TEXT("crop cannot invent missing ecological context"), Build(S, C, P, E));
	return true;
}
#endif
