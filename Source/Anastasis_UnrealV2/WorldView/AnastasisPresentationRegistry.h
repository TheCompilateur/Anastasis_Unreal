#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "World/AnastasisWorld.h"
#include "AnastasisPresentationRegistry.generated.h"

class UStaticMesh;
class UMaterialInterface;

/**
 * Editor-facing mirror of AnastasisWorld::ETileType.
 *
 * AnastasisSim owns the truth and is not a UObject module; this UENUM exists only so the
 * presentation data asset is editable. Values are locked to ETileType by static_assert in
 * the .cpp — adding a tile type there without mirroring it here is a compile error, never
 * a silent mismatch.
 */
UENUM(BlueprintType)
enum class EAnastasisSemanticType : uint8
{
	Grass = 0,
	Water = 1,
	Stone = 2,
	Ruin = 3,
	Forest = 4,
	Scrub = 5,
	Field = 6,
};

inline EAnastasisSemanticType ToSemanticType(AnastasisWorld::ETileType Type)
{
	return static_cast<EAnastasisSemanticType>(Type);
}

inline AnastasisWorld::ETileType ToTileType(EAnastasisSemanticType Type)
{
	return static_cast<AnastasisWorld::ETileType>(Type);
}

/** One interchangeable look for an archetype. Adding a second entry here is how FOREST gets a second tree. */
USTRUCT(BlueprintType)
struct FAnastasisPresentationVariant
{
	GENERATED_BODY()

	/** Soft: the registry must be loadable without dragging every mesh in with it. */
	UPROPERTY(EditAnywhere, Category = "Presentation")
	TSoftObjectPtr<UStaticMesh> Mesh;

	/** Optional. Left empty, the archetype's Tint is applied over the shared placeholder material instead. */
	UPROPERTY(EditAnywhere, Category = "Presentation")
	TSoftObjectPtr<UMaterialInterface> MaterialOverride;
};

/** What one semantic type looks like. The simulation never sees this struct. */
USTRUCT(BlueprintType)
struct FAnastasisPresentationEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Presentation")
	EAnastasisSemanticType SemanticType = EAnastasisSemanticType::Forest;

	/** Stable identity for logs, diagnostics and component naming. Not a mesh path. */
	UPROPERTY(EditAnywhere, Category = "Presentation")
	FName ArchetypeId;

	/** False renders nothing for this type — an explicit, data-side off switch, not a missing-data accident. */
	UPROPERTY(EditAnywhere, Category = "Presentation")
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, Category = "Presentation")
	TArray<FAnastasisPresentationVariant> Variants;

	/** Applied to the shared placeholder material when a variant has no MaterialOverride. */
	UPROPERTY(EditAnywhere, Category = "Presentation")
	FLinearColor Tint = FLinearColor::White;

	/** Uniform scale envelope applied to the mesh, sampled deterministically per tile. */
	UPROPERTY(EditAnywhere, Category = "Presentation")
	float MinUniformScale = 1.0f;

	UPROPERTY(EditAnywhere, Category = "Presentation")
	float MaxUniformScale = 1.0f;

	/** Max XY jitter as a fraction of AnastasisWorldView::TileWorldSize. 0 pins instances to tile centres. */
	UPROPERTY(EditAnywhere, Category = "Presentation")
	float JitterRadiusFraction = 0.0f;

	UPROPERTY(EditAnywhere, Category = "Presentation")
	bool bRandomYaw = true;
};

/**
 * PRESENTATION DATA OWNER.
 *
 * The asset an art pass edits: semantic type -> archetype -> meshes/materials/envelope.
 * Nothing here is known to AnastasisSim, and nothing here can change simulation truth —
 * it only decides how already-decided truth is drawn.
 *
 * Loaded by AnastasisPresentationResolver from AnastasisPresentation::RegistryAssetPath.
 * When that asset is missing or unusable the resolver falls back to CreateCodeDefaults(),
 * so the world still renders and the failure is logged rather than silent.
 */
UCLASS(BlueprintType)
class UAnastasisPresentationRegistry : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Presentation")
	TArray<FAnastasisPresentationEntry> Entries;

	/** First enabled entry for this type, or nullptr. */
	const FAnastasisPresentationEntry* FindEntry(AnastasisWorld::ETileType Type) const;

	/** The VISUAL_BUILD_001 values, as data. The fallback when the asset cannot be used. */
	static UAnastasisPresentationRegistry* CreateCodeDefaults(UObject* Outer);
};
