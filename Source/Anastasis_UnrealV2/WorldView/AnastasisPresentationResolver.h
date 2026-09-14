#pragma once

#include "CoreMinimal.h"
#include "World/AnastasisWorld.h"

#include "WorldView/AnastasisPresentationRegistry.h"

class UAnastasisPresentationRegistry;
class UStaticMesh;
class UMaterialInterface;
struct FAnastasisPresentationEntry;

/**
 * VISUAL TRANSLATION LAYER OWNER.
 *
 * WorldSemanticState (AnastasisWorld::ETileType) -> presentation data -> deterministic
 * variant + transform. The mapping itself now lives in a UAnastasisPresentationRegistry
 * data asset (RegistryAssetPath); this file only reads it. No mesh path, tint or scale is
 * decided in C++ any more except in the code-default fallback.
 *
 * REACHABLE IN THIS SLICE:: Forest and Ruin. Every other ETileType resolves to nothing
 * (ground colour only; see AnastasisTerrainSurface, untouched). Roads and named building
 * categories have no source in AnastasisWorld — see docs/unreal/VISUAL_PIPELINE_AUDIT.md.
 *
 * Presence of an instance is decided ENTIRELY by simulation truth plus the data entry's
 * bEnabled flag. SelectVariantIndex/ResolveInstanceTransform only choose *which* look and
 * *where*; they never invent a tile.
 */
namespace AnastasisPresentation
{
	/** Fixed path + code fallback, the same convention AAnastasisWorldEmbodiment::ResolveSliceMaterial already uses for M_AnastasisSlice. */
	inline constexpr const TCHAR* RegistryAssetPath = TEXT("/Game/Anastasis/Presentation/DA_AnastasisPresentation.DA_AnastasisPresentation");

	/** UE BasicShapes (Cube/Cone/Cylinder/...) share this bounding size by convention; see AnastasisWorldDebugVisual::CubeMeshSize for the cube case this mirrors. */
	inline constexpr double EngineBasicShapeSize = 100.0;

	/** One tile's fully resolved look: the data entry plus the variant deterministically chosen for it. */
	struct FResolvedPresentation
	{
		const FAnastasisPresentationEntry* Entry = nullptr;
		int32 VariantIndex = INDEX_NONE;
		UStaticMesh* Mesh = nullptr;
		UMaterialInterface* MaterialOverride = nullptr;
		/** Loaded materials for slots 1..N. Empty for a single-slot mesh. */
		TArray<UMaterialInterface*> AdditionalMaterials;
		/** The chosen variant's ScaleBias, to be handed back to ResolveInstanceTransform. */
		float ScaleBias = 1.0f;
	};

	/** The live registry: the data asset when it loads, the code defaults otherwise. Never null. */
	const UAnastasisPresentationRegistry& GetRegistry();

	/** True while GetRegistry() is serving the data asset rather than the code fallback. */
	bool IsRegistryDataDriven();

	/** Drops the cache so the next GetRegistry() re-resolves. For tests and for re-editing the asset in-editor. */
	void InvalidateRegistryCache();

	/** Enabled entry for this semantic type, or nullptr when the type renders no instance. */
	const FAnastasisPresentationEntry* FindEntry(AnastasisWorld::ETileType Type);

	/**
	 * Deterministic variant choice: same (Seed, TileX, TileY, Wanted) and same entry always
	 * give the same index. Only variants carrying a mesh are eligible. INDEX_NONE = nothing
	 * to draw.
	 *
	 * Wanted narrows the pool to the looks built for that stature, plus the untagged (Any)
	 * ones. It FAILS OPEN: an entry whose data names no variant for the requested stature
	 * falls back to every eligible variant rather than rendering a hole. A missing art asset
	 * must degrade the look, never the presence of the tree.
	 */
	int32 SelectVariantIndex(
		const FAnastasisPresentationEntry& Entry,
		uint32 Seed,
		int32 TileX,
		int32 TileY,
		EAnastasisStatureClass Wanted = EAnastasisStatureClass::Any,
		EAnastasisFoliageFamily Family = EAnastasisFoliageFamily::Any);

	/**
	 * Which species family the site itself calls for.
	 *
	 * PRESENTATION READS SIMULATION TRUTH; IT DOES NOT ADD ANY. Both inputs already exist
	 * on FVisualTile and are produced by AnastasisWorld:
	 *
	 *   Shade    [-1,1] relief lighting: slope facing the light, blended with altitude.
	 *                   It is already the composition of "altitude et exposition", so
	 *                   reading Alt as a third term would double-count altitude.
	 *   Wetness  [0,1]  moisture.
	 *
	 * Pontic ecology, which is what the reference plate documents: the oriental spruce and
	 * the fir take the high, exposed, harsh ground; the beech, hornbeam and alder hold the
	 * lower, wetter, sheltered ground and the stream bottoms.
	 *
	 * The result is a PROBABILITY resolved against a per-site hash, never a threshold. A
	 * hard cut would draw a visible contour line across the map -- a rendering artefact,
	 * not an ecotone. Real stands are mixed, and their proportions shift gradually.
	 */
	EAnastasisFoliageFamily SelectFoliageFamily(
		double Shade,
		double Wetness,
		uint32 Seed,
		int32 TileX,
		int32 TileY);

	/** Coniferousness in [0,1] for a site, before the hash draw. Exposed so the gradient itself is testable. */
	double Coniferousness(double Shade, double Wetness);

	/** Entry + variant + loaded assets for one tile. false = nothing renders here. Loads the variant's mesh synchronously. */
	bool ResolvePresentation(
		AnastasisWorld::ETileType Type,
		uint32 Seed,
		int32 TileX,
		int32 TileY,
		FResolvedPresentation& Out,
		EAnastasisStatureClass Wanted = EAnastasisStatureClass::Any,
		EAnastasisFoliageFamily Family = EAnastasisFoliageFamily::Any);

	/**
	 * Deterministic placement: same (Seed, TileX, TileY, Entry, ScaleBias) always yields the
	 * same transform. Location.Z is lifted so the instance's BASE, not its centre, sits at Alt
	 * (engine primitives are centre-pivoted).
	 *
	 * ScaleBias multiplies the entry's envelope for one variant: it is how a single archetype
	 * spans several statures. The lift uses the SAME final scale, so the base still rests on
	 * Alt exactly -- 0.5 * EngineBasicShapeSize * FinalScale, the identity
	 * Anastasis.Terrain.DressingRestsOnRenderedGround verifies.
	 *
	 * The tilt sampled from Entry.MaxLeanDegrees rotates about the instance centre, which
	 * raises the trunk foot by (1 - cos Lean) * 50 * Scale -- under one unreal unit at the
	 * envelope this project uses. It is therefore a silhouette change, not a placement one.
	 */
	FTransform ResolveInstanceTransform(
		const FAnastasisPresentationEntry& Entry,
		uint32 Seed,
		int32 TileX,
		int32 TileY,
		double Alt,
		float ScaleBias = 1.0f);
}
