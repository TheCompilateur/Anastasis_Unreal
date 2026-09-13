#pragma once

#include "CoreMinimal.h"
#include "World/AnastasisWorld.h"

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

	/** UE BasicShapes (Cube/Cone/Cylinder/...) share this bounding size by convention. */
	inline constexpr double EngineBasicShapeSize = 100.0;

	/** One tile's fully resolved look: the data entry plus the variant deterministically chosen for it. */
	struct FResolvedPresentation
	{
		const FAnastasisPresentationEntry* Entry = nullptr;
		int32 VariantIndex = INDEX_NONE;
		UStaticMesh* Mesh = nullptr;
		UMaterialInterface* MaterialOverride = nullptr;
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
	 * Deterministic variant choice: same (Seed, TileX, TileY) and same entry always give the
	 * same index. Only variants carrying a mesh are eligible. INDEX_NONE = nothing to draw.
	 */
	int32 SelectVariantIndex(const FAnastasisPresentationEntry& Entry, uint32 Seed, int32 TileX, int32 TileY);

	/** Entry + variant + loaded assets for one tile. false = nothing renders here. Loads the variant's mesh synchronously. */
	bool ResolvePresentation(
		AnastasisWorld::ETileType Type,
		uint32 Seed,
		int32 TileX,
		int32 TileY,
		FResolvedPresentation& Out);

	/**
	 * Deterministic placement: same (Seed, TileX, TileY, Entry) always yields the same
	 * transform. Location.Z is lifted so the instance's BASE, not its centre, sits at Alt
	 * (engine primitives are centre-pivoted).
	 */
	FTransform ResolveInstanceTransform(
		const FAnastasisPresentationEntry& Entry,
		uint32 Seed,
		int32 TileX,
		int32 TileY,
		double Alt);
}
