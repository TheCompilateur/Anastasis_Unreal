#include "WorldView/AnastasisPresentationResolver.h"
#include "WorldView/AnastasisWorldView.h"

namespace AnastasisPresentation
{
namespace
{
	// Tint mirrors AnastasisTerrainSurface::SurfaceTypeColor for the same ETileType.
	// Deliberate duplication of two literals, not a shared constant: the ground palette
	// is sealed (WORLD_SLICE_006 TERRAIN_CONTRACT) and this file must not create a
	// compile-time dependency onto it.
	const FRenderableDefinition TreeDefinition{
		EArchetype::Tree, FName(TEXT("Tree_Generic")), TreeMeshPath,
		FLinearColor(0.102f, 0.243f, 0.114f), 1.6, 2.4, 0.30};

	const FRenderableDefinition RuinDefinition{
		EArchetype::Ruin, FName(TEXT("Ruin_Generic")), RuinMeshPath,
		FLinearColor(0.353f, 0.302f, 0.318f), 0.6, 1.1, 0.20};

	/**
	 * Small stable integer hash, local to presentation. Deliberately NOT AnastasisRng /
	 * AnastasisWorldNoise (those are AnastasisSim-owned); the presentation layer must not
	 * reach into simulation internals for jitter that has no bearing on simulation truth.
	 */
	uint32 HashTile(uint32 Seed, int32 TileX, int32 TileY, uint32 Salt)
	{
		uint32 H = Seed ^ 0x9E3779B9u;
		H = (H ^ static_cast<uint32>(TileX)) * 0x85EBCA6Bu;
		H = (H ^ static_cast<uint32>(TileY)) * 0xC2B2AE35u;
		H = (H ^ Salt) * 0x27D4EB2Fu;
		H ^= H >> 15;
		return H;
	}

	double UnitFloat(uint32 H)
	{
		return static_cast<double>(H) / static_cast<double>(MAX_uint32);
	}
}

const FRenderableDefinition* Resolve(AnastasisWorld::ETileType Type)
{
	switch (Type)
	{
	case AnastasisWorld::ETileType::Forest:
		return &TreeDefinition;
	case AnastasisWorld::ETileType::Ruin:
		return &RuinDefinition;
	default:
		return nullptr;
	}
}

const TArray<FRenderableDefinition>& AllArchetypes()
{
	static const TArray<FRenderableDefinition> All{TreeDefinition, RuinDefinition};
	return All;
}

FTransform ResolveInstanceTransform(
	const FRenderableDefinition& Definition,
	uint32 Seed,
	int32 TileX,
	int32 TileY,
	double Alt)
{
	const uint32 HX = HashTile(Seed, TileX, TileY, 0x1u);
	const uint32 HY = HashTile(Seed, TileX, TileY, 0x2u);
	const uint32 HYaw = HashTile(Seed, TileX, TileY, 0x3u);
	const uint32 HScale = HashTile(Seed, TileX, TileY, 0x4u);

	const double JitterRadius = Definition.JitterRadiusFraction * AnastasisWorldView::TileWorldSize;
	const double Scale = FMath::Lerp(Definition.MinUniformScale, Definition.MaxUniformScale, UnitFloat(HScale));

	FVector Location = AnastasisWorldView::TileToUnreal(TileX, TileY, Alt);
	Location.X += (UnitFloat(HX) * 2.0 - 1.0) * JitterRadius;
	Location.Y += (UnitFloat(HY) * 2.0 - 1.0) * JitterRadius;
	// Engine BasicShapes are center-pivoted: lift by half the scaled bounding height so
	// the instance's base sits at Alt instead of clipping half-buried into the ground.
	Location.Z += 0.5 * EngineBasicShapeSize * Scale;

	const double Yaw = UnitFloat(HYaw) * 360.0;
	return FTransform(FRotator(0.0, Yaw, 0.0), Location, FVector(Scale));
}
}
