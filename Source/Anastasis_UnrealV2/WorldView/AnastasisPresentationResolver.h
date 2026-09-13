#pragma once

#include "CoreMinimal.h"
#include "World/AnastasisWorld.h"

/**
 * VISUAL TRANSLATION LAYER OWNER.
 *
 * WorldSemanticState (AnastasisWorld::ETileType) -> FRenderableDefinition -> deterministic
 * per-tile transform. A logical tile type never names a mesh directly: MeshPath/Tint here
 * are the one seam an asset-integration pass swaps later, without touching worldgen,
 * WorldView, or the placement math below.
 *
 * REACHABLE IN THIS SLICE:: Forest -> Tree, Ruin -> Ruin. Every other ETileType resolves to
 * nullptr (ground color only; see AnastasisTerrainSurface, untouched by this file). Roads and
 * named building categories (House/Market/Workshop...) have no source tile/entity in
 * AnastasisWorld today: see docs/unreal/VISUAL_PIPELINE_AUDIT.md. Not invented here.
 *
 * Presence of an instance is decided ENTIRELY by Resolve(Type) — i.e. by simulation truth.
 * ResolveInstanceTransform only jitters position/yaw/scale; it never decides whether a tile
 * gets an instance.
 */
namespace AnastasisPresentation
{
	enum class EArchetype : uint8
	{
		None = 0,
		Tree = 1,
		Ruin = 2,
		Count = 3,
	};

	inline constexpr const TCHAR* TreeMeshPath = TEXT("/Engine/BasicShapes/Cone.Cone");
	inline constexpr const TCHAR* RuinMeshPath = TEXT("/Engine/BasicShapes/Cylinder.Cylinder");

	/** UE BasicShapes (Cube/Cone/Cylinder/...) share this bounding size by convention; see AnastasisWorldDebugVisual::CubeMeshSize for the cube case this mirrors. */
	inline constexpr double EngineBasicShapeSize = 100.0;

	struct FRenderableDefinition
	{
		EArchetype Archetype = EArchetype::None;
		/** Stable id for logging / a future data-asset lookup key. Not a mesh path. */
		FName ArchetypeId;
		const TCHAR* MeshPath = nullptr;
		/** Placeholder-only tint (matches AnastasisTerrainSurface's ground palette for the same ETileType so instance and ground read as one family). Ignored once a real material replaces BasicShapeMaterial. */
		FLinearColor Tint = FLinearColor::White;
		/** Scale factors applied to the ~100uu engine primitive (EngineBasicShapeSize convention). */
		double MinUniformScale = 1.0;
		double MaxUniformScale = 1.0;
		/** Max XY jitter, as a fraction of AnastasisWorldView::TileWorldSize. */
		double JitterRadiusFraction = 0.0;
	};

	/** WorldSemanticState -> RenderableDefinition. nullptr = no discrete instance for this type in the current slice. */
	const FRenderableDefinition* Resolve(AnastasisWorld::ETileType Type);

	/** Every archetype this slice can instance, in stable EArchetype order. Drives the per-archetype HISM set in AAnastasisWorldEmbodiment. */
	const TArray<FRenderableDefinition>& AllArchetypes();

	/**
	 * Deterministic placement for one tile: same (Seed, TileX, TileY, Definition) always
	 * yields the same transform. Location.Z is lifted so the instance's BASE, not its
	 * center, sits at Alt (engine primitives are center-pivoted).
	 */
	FTransform ResolveInstanceTransform(
		const FRenderableDefinition& Definition,
		uint32 Seed,
		int32 TileX,
		int32 TileY,
		double Alt);
}
