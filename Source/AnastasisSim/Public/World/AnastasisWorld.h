#pragma once

#include "CoreMinimal.h"
#include "World/AnastasisWorldArchetype.h"

namespace AnastasisWorld
{
	enum class ETileType : uint8
	{
		Grass = 0,
		Water = 1,
		Stone = 2,
		Ruin = 3,
		Forest = 4,
		Scrub = 5,
		Field = 6,
	};

	enum class EResource : uint8
	{
		None = 0,
		Stone = 1,
		Wood = 2,
		Food = 3,
	};

	enum class ECropId : uint8
	{
		None = 0,
		Grain = 1,
		Greens = 2,
		Fruit = 3,
		Fallow = 4,
	};

	inline constexpr int32 TileTypeCount = 7;

	struct FTile
	{
		int32 X = 0;
		int32 Y = 0;
		ETileType Type = ETileType::Grass;
		EResource Resource = EResource::None;
		int32 Amount = 0;
		double Alt = 0.0;
		double Shade = 0.0;
		double Shore = 0.0;
		double Wetness = 0.0;
		double FlowX = 0.0;
		double FlowZ = 0.0;
		double FlowAmt = 0.0;
		ECropId CropId = ECropId::None;
		double Fertility = 0.0;
		double ForestMargin = 0.0;
		bool bHasForestMargin = false;
	};

	struct FWorld
	{
		int32 W = 0;
		int32 H = 0;
		AnastasisWorldArchetype::FKnobs Archetype;
		TArray<FTile> Tiles;
	};

	inline constexpr double SeaLevel = 0.275;
	inline constexpr double ForestTileMaxFrac = 0.14;

	ANASTASISSIM_API FWorld GenerateWorld(uint32 Seed, int32 W, int32 H);
	ANASTASISSIM_API ECropId PickFieldCropId(int32 X, int32 Y, uint32 Salt);
	ANASTASISSIM_API const TCHAR* TileTypeName(ETileType Type);
}

