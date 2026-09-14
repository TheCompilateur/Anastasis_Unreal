#include "Misc/AutomationTest.h"

#include "WorldView/AnastasisMistField.h"
#include "WorldView/AnastasisWorldView.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	AnastasisMist::FMistParams DefaultParams()
	{
		// The profile's shipped defaults, mirrored here so the tests describe the configuration
		// the game actually runs rather than a convenient one.
		AnastasisMist::FMistParams Params;
		Params.CellTiles = 8;
		Params.WetnessThreshold = 0.35;
		Params.VolumeRadiusFraction = 0.75;
		Params.MaxVolumes = 192;
		return Params;
	}

	/** A synthetic snapshot: W x H tiles, every wetness supplied by the caller. */
	AnastasisWorldView::FWorldVisualSnapshot MakeSnapshot(int32 W, int32 H, TFunctionRef<double(int32, int32)> WetnessAt)
	{
		AnastasisWorldView::FWorldVisualSnapshot Snapshot;
		Snapshot.Seed = 1u;
		Snapshot.SourceW = W;
		Snapshot.SourceH = H;
		Snapshot.W = W;
		Snapshot.H = H;
		Snapshot.Tiles.SetNum(W * H);
		for (int32 Y = 0; Y < H; ++Y)
		{
			for (int32 X = 0; X < W; ++X)
			{
				AnastasisWorldView::FVisualTile& Tile = Snapshot.Tiles[Y * W + X];
				Tile.SourceIndex = Y * W + X;
				Tile.X = X;
				Tile.Y = Y;
				Tile.Alt = 0.3;
				Tile.Wetness = WetnessAt(X, Y);
			}
		}
		return Snapshot;
	}
}

/**
 * THE CAUSAL PROPERTY, which is the whole point of driving mist from the simulation: the only
 * way to move the fog is to move the water. Same params, two worlds differing in wetness
 * alone — the pockets follow, and a world with no wet tiles has no mist at all.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisMistCausality, "Anastasis.Mist.Causality", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnastasisMistCausality::RunTest(const FString&)
{
	const AnastasisMist::FMistParams Params = DefaultParams();

	// A bone-dry world: no water anywhere, so Wetness is 0 everywhere (six tiles from water
	// is exactly 0 by AnastasisWorld's own formula).
	const AnastasisWorldView::FWorldVisualSnapshot Dry =
		MakeSnapshot(16, 16, [](int32, int32) { return 0.0; });
	TestEqual(TEXT("a world with no water carries no mist"), AnastasisMist::BuildMistField(Dry, Params).Num(), 0);

	// Same world, one wet half: a river running down the left. Only the left cells fog.
	const AnastasisWorldView::FWorldVisualSnapshot Half =
		MakeSnapshot(16, 16, [](int32 X, int32) { return X < 8 ? 0.9 : 0.0; });
	const TArray<AnastasisMist::FMistPocket> HalfPockets = AnastasisMist::BuildMistField(Half, Params);
	TestEqual(TEXT("only the wet half raises pockets"), HalfPockets.Num(), 2);
	for (const AnastasisMist::FMistPocket& Pocket : HalfPockets)
	{
		TestEqual(TEXT("every pocket is in the wet column of cells"), Pocket.CellX, 0);
		// The centroid must sit over the water, i.e. inside the left half of the world.
		TestTrue(TEXT("the pocket sits over the wet tiles"),
			Pocket.Location.X < 8.0 * AnastasisWorldView::TileWorldSize);
	}

	// Dry the world back out and the mist goes with it — no hysteresis, no cached field.
	const AnastasisWorldView::FWorldVisualSnapshot DriedAgain =
		MakeSnapshot(16, 16, [](int32, int32) { return 0.1; });
	TestEqual(TEXT("wetness below the threshold raises nothing"),
		AnastasisMist::BuildMistField(DriedAgain, Params).Num(), 0);

	return true;
}

/**
 * The threshold applies to the CELL MEAN, not to any single tile: one soaked tile in an
 * otherwise dry cell is a puddle, not a fog bank. This is what keeps the mist reading as an
 * area phenomenon instead of a per-tile decal.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisMistThreshold, "Anastasis.Mist.Threshold", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnastasisMistThreshold::RunTest(const FString&)
{
	const AnastasisMist::FMistParams Params = DefaultParams();

	// One tile at 1.0 in an 8x8 cell: mean = 1/64 = 0.0156, far under 0.35.
	const AnastasisWorldView::FWorldVisualSnapshot Puddle =
		MakeSnapshot(8, 8, [](int32 X, int32 Y) { return (X == 3 && Y == 3) ? 1.0 : 0.0; });
	TestEqual(TEXT("a single soaked tile is a puddle, not a fog bank"),
		AnastasisMist::BuildMistField(Puddle, Params).Num(), 0);

	// Just under and just over the threshold. The boundary itself is deliberately NOT asserted
	// on exact equality: the mean is a sum of doubles, so "exactly 0.35" is a property of the
	// arithmetic, not of the design, and pinning it would be a brittle test of nothing.
	const AnastasisWorldView::FWorldVisualSnapshot Under =
		MakeSnapshot(8, 8, [](int32, int32) { return 0.30; });
	TestEqual(TEXT("a cell under the threshold stays clear"), AnastasisMist::BuildMistField(Under, Params).Num(), 0);

	const AnastasisWorldView::FWorldVisualSnapshot JustOver =
		MakeSnapshot(8, 8, [](int32, int32) { return 0.36; });
	const TArray<AnastasisMist::FMistPocket> OverPockets = AnastasisMist::BuildMistField(JustOver, Params);
	if (TestEqual(TEXT("a cell just over the threshold fogs"), OverPockets.Num(), 1))
	{
		// Density is measured FROM the threshold, so a marginal cell is nearly clear instead of
		// arriving at full strength the instant it qualifies. That is what stops the mist from
		// having a visible on/off edge along the threshold contour.
		TestTrue(TEXT("the marginal cell is nearly clear"), OverPockets[0].Density01 < 0.05);
	}

	// A saturated cell is the thickest mist there is.
	const AnastasisWorldView::FWorldVisualSnapshot Soaked =
		MakeSnapshot(8, 8, [](int32, int32) { return 1.0; });
	const TArray<AnastasisMist::FMistPocket> SoakedPockets = AnastasisMist::BuildMistField(Soaked, Params);
	if (TestEqual(TEXT("a saturated cell fogs"), SoakedPockets.Num(), 1))
	{
		TestEqual(TEXT("a saturated cell is at full density"), SoakedPockets[0].Density01, 1.0, 1e-9);
		TestEqual(TEXT("every tile counted as wet"), SoakedPockets[0].WetTileCount, 64);
	}

	return true;
}

/**
 * Determinism and bounds. The field feeds captures, so the same world must give byte-identical
 * pockets; and it must be impossible for a wet world to spawn an unbounded number of scene
 * proxies, because "it looked fine at seed 12345" is not a performance argument.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisMistDeterminismAndBounds, "Anastasis.Mist.DeterminismAndBounds", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnastasisMistDeterminismAndBounds::RunTest(const FString&)
{
	AnastasisMist::FMistParams Params = DefaultParams();

	// A wetness gradient, so cells differ and the sort has real work to do.
	const AnastasisWorldView::FWorldVisualSnapshot Snapshot =
		MakeSnapshot(96, 96, [](int32, int32 Y)
		{
			return FMath::Clamp(1.0 - FMath::Abs(static_cast<double>(Y) - 48.0) / 24.0, 0.0, 1.0);
		});

	const TArray<AnastasisMist::FMistPocket> First = AnastasisMist::BuildMistField(Snapshot, Params);
	const TArray<AnastasisMist::FMistPocket> Second = AnastasisMist::BuildMistField(Snapshot, Params);

	if (TestEqual(TEXT("the same world gives the same number of pockets"), Second.Num(), First.Num()))
	{
		for (int32 Index = 0; Index < First.Num(); ++Index)
		{
			// Exact equality, not nearly-equal: a pocket that drifts by a hair between two runs
			// makes two captures of the same world incomparable.
			TestTrue(TEXT("pocket location is reproduced exactly"), First[Index].Location == Second[Index].Location);
			TestTrue(TEXT("pocket density is reproduced exactly"), First[Index].Density01 == Second[Index].Density01);
			TestTrue(TEXT("pocket cell is reproduced exactly"),
				First[Index].CellX == Second[Index].CellX && First[Index].CellY == Second[Index].CellY);
		}
	}

	// The unbounded field, for reference, then the same field under a tight cap.
	const int32 Uncapped = First.Num();
	TestTrue(TEXT("the gradient world does raise mist"), Uncapped > 0);
	// (96/8)^2 = 144 cells: bounded by the cell grid before any cap.
	TestTrue(TEXT("pockets can never exceed the cell count"), Uncapped <= 144);

	Params.MaxVolumes = 5;
	bool bTruncated = false;
	const TArray<AnastasisMist::FMistPocket> Capped = AnastasisMist::BuildMistField(Snapshot, Params, &bTruncated);
	TestEqual(TEXT("the cap is respected"), Capped.Num(), 5);
	TestTrue(TEXT("truncation is reported, never silent"), bTruncated);

	// Truncation keeps the WETTEST cells: the fog that survives is the fog that matters most.
	double Lowest = TNumericLimits<double>::Max();
	for (const AnastasisMist::FMistPocket& Pocket : Capped)
	{
		Lowest = FMath::Min(Lowest, Pocket.MeanWetness);
	}
	int32 WetterThanKept = 0;
	for (const AnastasisMist::FMistPocket& Pocket : First)
	{
		if (Pocket.MeanWetness > Lowest)
		{
			++WetterThanKept;
		}
	}
	TestTrue(TEXT("the kept pockets are the wettest ones"), WetterThanKept <= Capped.Num());

	// A cap of zero means "no ceiling", not "no mist" — the field is then the cell grid's own bound.
	Params.MaxVolumes = 0;
	bTruncated = true;
	TestEqual(TEXT("MaxVolumes=0 means uncapped"), AnastasisMist::BuildMistField(Snapshot, Params, &bTruncated).Num(), Uncapped);
	TestFalse(TEXT("an uncapped field is not reported as truncated"), bTruncated);

	return true;
}

/**
 * Refusals. Bad parameters and empty worlds must produce an empty field and say nothing
 * plausible-looking — fog invented from a caller error is worse than no fog.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisMistRefusals, "Anastasis.Mist.Refusals", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnastasisMistRefusals::RunTest(const FString&)
{
	AnastasisMist::FMistParams Params = DefaultParams();
	const AnastasisWorldView::FWorldVisualSnapshot Wet =
		MakeSnapshot(16, 16, [](int32, int32) { return 1.0; });

	Params.CellTiles = 0;
	TestEqual(TEXT("a zero cell size is refused, not silently defaulted"),
		AnastasisMist::BuildMistField(Wet, Params).Num(), 0);

	Params.CellTiles = -8;
	TestEqual(TEXT("a negative cell size is refused"), AnastasisMist::BuildMistField(Wet, Params).Num(), 0);

	Params = DefaultParams();
	const AnastasisWorldView::FWorldVisualSnapshot Empty;
	TestEqual(TEXT("an empty snapshot raises nothing"), AnastasisMist::BuildMistField(Empty, Params).Num(), 0);

	// A cell larger than the world is legitimate: the whole world becomes one pocket.
	Params.CellTiles = 64;
	const TArray<AnastasisMist::FMistPocket> Single = AnastasisMist::BuildMistField(Wet, Params);
	TestEqual(TEXT("a cell larger than the world gives exactly one pocket"), Single.Num(), 1);

	return true;
}

#endif
