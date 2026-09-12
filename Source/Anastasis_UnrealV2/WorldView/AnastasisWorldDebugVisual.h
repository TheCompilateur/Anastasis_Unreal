#pragma once

#include "CoreMinimal.h"
#include "World/AnastasisWorld.h"
#include "WorldView/AnastasisWorldView.h"

/**
 * DEBUG / METROLOGY OWNER.
 *
 * Cube/slab geometry, debug colors and debug material policy live here.
 * WorldView must not own these decisions. Functionality is preserved for
 * AAnastasisWorldEmbodiment only.
 */
namespace AnastasisWorldDebugVisual
{
	inline constexpr double CubeMeshSize = 100.0;
	inline constexpr double TileSlabScaleZ = 0.2;

	inline FVector TileScale()
	{
		const double XY = AnastasisWorldView::TileWorldSize / CubeMeshSize;
		return FVector(XY, XY, TileSlabScaleZ);
	}

	inline FTransform TileTransform(const FVector& Location)
	{
		return FTransform(FQuat::Identity, Location, TileScale());
	}

	inline FLinearColor TerrainDebugColor(AnastasisWorld::ETileType Type)
	{
		switch (Type)
		{
		case AnastasisWorld::ETileType::Grass:  return FLinearColor(0.25f, 0.55f, 0.18f);
		case AnastasisWorld::ETileType::Water:  return FLinearColor(0.12f, 0.28f, 0.72f);
		case AnastasisWorld::ETileType::Stone:  return FLinearColor(0.45f, 0.45f, 0.48f);
		case AnastasisWorld::ETileType::Ruin:   return FLinearColor(0.45f, 0.22f, 0.55f);
		case AnastasisWorld::ETileType::Forest: return FLinearColor(0.08f, 0.28f, 0.10f);
		case AnastasisWorld::ETileType::Scrub:  return FLinearColor(0.42f, 0.40f, 0.18f);
		case AnastasisWorld::ETileType::Field:  return FLinearColor(0.72f, 0.62f, 0.16f);
		}
		return FLinearColor::White;
	}
}
