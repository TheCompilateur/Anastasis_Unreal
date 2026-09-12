#pragma once

#include "CoreMinimal.h"
#include "World/AnastasisWorld.h"

/**
 * WORLDVIEW OWNER: semantic snapshot, crop, coordinate transform.
 *
 * WORLDGEN_OWNER:: AnastasisWorld (AnastasisSim)
 * Does not duplicate generateWorld. Does not own debug cube/slab/color/material.
 * AnastasisSim does not depend on this adapter.
 *
 * COORDINATE_CONVENTION:: index = Y * W + X ; FVisualTile.X / FVisualTile.Y are source tiles
 * CANONICAL_WORLD:: GenerateWorld(Seed, 96, 96) — never GenerateWorld(32, 32) as a visual slice
 */
namespace AnastasisWorldView
{
	/** Unreal units (cm) per simulation tile. Tile (0,0) occupies [0, TileWorldSize). P1.5 diagnostic; Lot 4.5 freeze. */
	inline constexpr double TileWorldSize = 100.0;

	/** Simulation altitude 1.0 maps to this many Unreal units on Z. SeaLevel 0.275 -> 275 UU. */
	inline constexpr double AltitudeScale = 1000.0;

	inline constexpr uint32 ReferenceSeed = 12345u;
	inline constexpr int32 ReferenceWidth = 96;
	inline constexpr int32 ReferenceHeight = 96;

	/** Mandated first visual crop: x=[0,31], y=[0,31] of the canonical 96x96. */
	inline constexpr int32 CanonicalCropOriginX = 0;
	inline constexpr int32 CanonicalCropOriginY = 0;
	inline constexpr int32 CanonicalCropWidth = 32;
	inline constexpr int32 CanonicalCropHeight = 32;

	struct FVisualTile
	{
		int32 SourceIndex = INDEX_NONE;
		int32 X = 0;
		int32 Y = 0;
		AnastasisWorld::ETileType Type = AnastasisWorld::ETileType::Grass;
		AnastasisWorld::EResource Resource = AnastasisWorld::EResource::None;
		int32 Amount = 0;
		double Alt = 0.0;
		double Shade = 0.0;
		double Shore = 0.0;
		double Wetness = 0.0;
		double FlowX = 0.0;
		double FlowZ = 0.0;
		double FlowAmt = 0.0;
		AnastasisWorld::ECropId CropId = AnastasisWorld::ECropId::None;
		double Fertility = 0.0;
		double ForestMargin = 0.0;
		bool bHasForestMargin = false;
	};

	struct FWorldVisualSnapshot
	{
		uint32 Seed = 0;
		int32 SourceW = 0;
		int32 SourceH = 0;
		int32 OriginX = 0;
		int32 OriginY = 0;
		int32 W = 0;
		int32 H = 0;
		TArray<FVisualTile> Tiles;
		int32 TerrainCounts[AnastasisWorld::TileTypeCount] = {};
		double MinAlt = 0.0;
		double MaxAlt = 0.0;
	};

	struct FTilePose
	{
		int32 Index = 0;
		int32 X = 0;
		int32 Y = 0;
		AnastasisWorld::ETileType Type = AnastasisWorld::ETileType::Grass;
		double Alt = 0.0;
		FVector UnrealLocation = FVector::ZeroVector;
	};

	/** Unreal spatialization of a snapshot. Not debug art. */
	struct FPlan
	{
		uint32 Seed = 0;
		int32 SourceW = 0;
		int32 SourceH = 0;
		int32 OriginX = 0;
		int32 OriginY = 0;
		int32 W = 0;
		int32 H = 0;
		int32 TileCount = 0;
		double MinAlt = 0.0;
		double MaxAlt = 0.0;
		int32 TerrainCounts[AnastasisWorld::TileTypeCount] = {};
		TArray<FVector> Locations;
		TArray<AnastasisWorld::ETileType> Types;
		TArray<double> Alts;
	};

	/** SIM (x, y, alt) -> UE (X, Y, Z). Origin: tile centers; axes: SimX->UEX, SimY->UEY, Alt->UEZ. */
	inline FVector TileToUnreal(int32 TileX, int32 TileY, double Alt)
	{
		return FVector(
			(static_cast<double>(TileX) + 0.5) * TileWorldSize,
			(static_cast<double>(TileY) + 0.5) * TileWorldSize,
			Alt * AltitudeScale);
	}

	FVisualTile MakeVisualTile(const AnastasisWorld::FTile& Tile, int32 SourceIndex);
	FWorldVisualSnapshot CaptureSnapshot(uint32 Seed, const AnastasisWorld::FWorld& World);
	FWorldVisualSnapshot CaptureCanonicalWorld(uint32 Seed);
	FWorldVisualSnapshot CropSnapshot(
		const FWorldVisualSnapshot& Source,
		int32 OriginX,
		int32 OriginY,
		int32 CropW,
		int32 CropH);
	const FVisualTile* FindTile(const FWorldVisualSnapshot& Snapshot, int32 TileX, int32 TileY);

	FPlan BuildPlan(const FWorldVisualSnapshot& Snapshot);
	FPlan BuildPlan(uint32 Seed, const AnastasisWorld::FWorld& World);
	FTilePose SamplePose(const FPlan& Plan, int32 TileX, int32 TileY);
	FBox PlanBounds(const FPlan& Plan);
	FBox SnapshotBounds(const FWorldVisualSnapshot& Snapshot);
}
