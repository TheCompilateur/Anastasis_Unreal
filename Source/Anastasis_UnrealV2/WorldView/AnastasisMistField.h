#pragma once

#include "CoreMinimal.h"
#include "WorldView/AnastasisWorldView.h"

/**
 * MIST FIELD: simulation truth -> where fog pockets belong.
 *
 * WHAT WETNESS ACTUALLY IS. AnastasisWorld computes
 * `Wetness = clamp(1 - WaterDistance / 6.5, 0, 1)` (AnastasisWorld.cpp), where
 * WaterDistance is the tile distance to the nearest water tile. A water tile is 1.0,
 * its neighbour 0.846, and everything more than six tiles from water is exactly 0.
 * It is NOT a general humidity field -- the noise-based `Moist` that decides forest is
 * never stored on a tile. So the mist this builds is river and shore fog: it can only
 * appear near water, and it says so rather than pretending to model weather.
 *
 * POCKETS ARE COARSE, DELIBERATELY. Mist forms over an area, not over a 1 m tile, and
 * one ALocalFogVolume per tile would be thousands of scene proxies. The world is cut
 * into cells of CellTiles x CellTiles; a cell whose MEAN wetness clears the threshold
 * gets exactly one pocket. That bounds the count by construction ((96/8)^2 = 144 at
 * the default) before any cap is applied, and it means an isolated puddle in an
 * otherwise dry cell does not raise a fog bank.
 *
 * PRESENCE IS DECIDED ENTIRELY BY SIMULATION TRUTH. No hash, no jitter, no randomness
 * anywhere in this file: the same world always produces the same pockets, and the only
 * way to move the mist is to change the water. Anastasis.Mist.Causality locks that.
 */
namespace AnastasisMist
{
	/** Tunables. Mirrored from UAnastasisAtmosphereProfile so this stays a pure function of data. */
	struct FMistParams
	{
		/** Side of one mist cell, in tiles. <= 0 is refused (empty field), never silently corrected. */
		int32 CellTiles = 8;

		/** Mean cell wetness required for a pocket. 0.35 ~ a cell averaging three tiles from water. */
		double WetnessThreshold = 0.35;

		/** Pocket radius as a fraction of the cell's world size. */
		double VolumeRadiusFraction = 0.75;

		/** Hard ceiling on pockets. Reaching it truncates by wetness and reports it; it never silently drops. */
		int32 MaxVolumes = 192;
	};

	/** One fog pocket, fully determined by the tiles under it. */
	struct FMistPocket
	{
		int32 CellX = 0;
		int32 CellY = 0;

		/** Tiles in this cell carrying any wetness at all (i.e. within six tiles of water). */
		int32 WetTileCount = 0;

		/** Mean wetness over EVERY tile of the cell, dry ones included. The threshold is applied to this. */
		double MeanWetness = 0.0;

		/** (MeanWetness - Threshold) / (1 - Threshold), clamped to [0,1]. Drives thickness. */
		double Density01 = 0.0;

		/**
		 * Wetness-weighted centroid of the cell, in Unreal units. Z is the mean altitude of the
		 * cell's wet tiles; a caller that has the rendered surface should re-sample it with
		 * AnastasisTerrainSurface::SampleHeight, because the tile altitude is a step and the
		 * rendered ground is a slope.
		 */
		FVector Location = FVector::ZeroVector;

		double RadiusUU = 0.0;
	};

	/**
	 * The pockets a world deserves. Deterministic: same snapshot and params, same array, always.
	 *
	 * bOutTruncated (optional) reports that MaxVolumes clipped the field — the caller is expected
	 * to log it, because a silently shortened mist field would look like a data problem later.
	 */
	TArray<FMistPocket> BuildMistField(
		const AnastasisWorldView::FWorldVisualSnapshot& Snapshot,
		const FMistParams& Params,
		bool* bOutTruncated = nullptr);
}
