#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "World/AnastasisWorld.h"
#include "AnastasisPresentationRegistry.generated.h"

class UStaticMesh;
class UMaterialInterface;
class UTexture2D;

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

/**
 * Which stature a look is built for.
 *
 * The names are the vertical strata of the forest section in
 * docs/visual/reference/pontique-etat-zero-3-stratification-forestiere.png
 * (emergente / canopee / sous-canopee / arbustive), because that plate is the
 * art-direction authority for what a tree of each age looks like. The axis is
 * generic all the same: any archetype with size classes can use it.
 *
 * Any is the untagged value, and it is deliberately value 0: data written
 * before this field existed keeps resolving to every request, so adding the
 * field changes no shipped asset's behaviour.
 */
UENUM(BlueprintType)
enum class EAnastasisStatureClass : uint8
{
	Any = 0,
	Understory = 1,
	Subcanopy = 2,
	Canopy = 3,
	Emergent = 4,
};

/**
 * Which broad species family a look belongs to.
 *
 * The reference plate separates CONIFERES (sapins, epicees, pins -- "verticalite,
 * contraste, altitude") from FEUILLUS (hetres, chenes, charmes). At distance those two
 * read as different things long before a species does: a dark spire against a pale dome.
 *
 * Any is value 0 for the same reason Stature's is: data written before this axis existed
 * answers every request, so adding the field changes no shipped asset's behaviour.
 */
UENUM(BlueprintType)
enum class EAnastasisFoliageFamily : uint8
{
	Any = 0,
	Conifer = 1,
	Broadleaf = 2,
};

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

	/**
	 * Material slots 1..N, when the mesh carries more than one.
	 *
	 * A tree is two materials, not one: wood is opaque and matte, foliage is two-sided and
	 * lets light through. Rendering the bark with a foliage shading model was semantically
	 * wrong and showed -- the trunks lifted in value against a bright ground.
	 *
	 * Slot 0 keeps its own field and its own meaning, so data written before this existed
	 * resolves exactly as it did. An entry that names no extra slot simply has none.
	 */
	UPROPERTY(EditAnywhere, Category = "Presentation")
	TArray<TSoftObjectPtr<UMaterialInterface>> AdditionalMaterialOverrides;

	/**
	 * Stature this look is drawn for. A young tree and a dominant one are not the same
	 * mesh at two scales: they differ in trunk fraction, crown width and tier count, so
	 * the mesh, not the scale, has to change with the age class.
	 */
	UPROPERTY(EditAnywhere, Category = "Presentation")
	EAnastasisStatureClass Stature = EAnastasisStatureClass::Any;

	/**
	 * Species family this look belongs to. Orthogonal to Stature: the pair (stature,
	 * family) is what picks a mesh, so a stand can change species without changing age
	 * structure, and change age structure without changing species.
	 */
	UPROPERTY(EditAnywhere, Category = "Presentation")
	EAnastasisFoliageFamily Family = EAnastasisFoliageFamily::Any;

	/**
	 * Multiplies the entry's uniform-scale envelope for THIS look only. It is how one
	 * archetype spans more height than a single Min/Max pair can: an emergent tree is
	 * taller than a canopy tree of the same species, not merely a different silhouette.
	 */
	UPROPERTY(EditAnywhere, Category = "Presentation", meta = (ClampMin = "0.05", ClampMax = "4.0"))
	float ScaleBias = 1.0f;
};

/**
 * Who a villager portrait depicts. A PRESENTATION category, not a demographic of the
 * simulation: AnastasisVillage::FNpc carries no age and no sex (deviation 8 of
 * AnastasisVillage.h -- every villager is an adult). The category only decides which
 * portraits may be handed to a simulated villager (see AnastasisVillagerLooks).
 */
UENUM(BlueprintType)
enum class EAnastasisVillagerCategory : uint8
{
	AdultMale = 0,
	AdultFemale = 1,
	ElderMale = 2,
	ElderFemale = 3,
	ChildMale = 4,
	ChildFemale = 5,
};

/**
 * One individual of the visual population (VILLAGER_PNG_001): a cut-out PNG, one person.
 *
 * Every portrait shares the canvas of SourceArt/Characters/villager-population.json: 512 x 1024 px
 * for 128 x 256 cm, feet on the vertical axis, 16 px above the bottom edge. The person's
 * stature is therefore baked into the image, and every card is drawn at the same size.
 */
USTRUCT(BlueprintType)
struct FAnastasisVillagerLook
{
	GENERATED_BODY()

	/** CHR_M_Adult_001... -- the manifest id, stable across re-imports. */
	UPROPERTY(EditAnywhere, Category = "Villagers")
	FName LookId;

	UPROPERTY(EditAnywhere, Category = "Villagers")
	EAnastasisVillagerCategory Category = EAnastasisVillagerCategory::AdultMale;

	/** Soft, like meshes: the registry stays loadable without pulling 32 textures in. */
	UPROPERTY(EditAnywhere, Category = "Villagers")
	TSoftObjectPtr<UTexture2D> Portrait;

	/**
	 * False: shown on the lineup, never handed to a simulated villager. A seated elder drawn on a
	 * bench would glide across the village on it. Defaults to true, so data written before the
	 * field existed keeps its meaning.
	 */
	UPROPERTY(EditAnywhere, Category = "Villagers")
	bool bInGame = true;

	/**
	 * Simulated jobs (AnastasisVillage::FNpc::JobId: "settler", "farmer") this portrait may stand for.
	 * The object in the painted hands is the job's: a pitchfork or a basket for a farmer, empty hands
	 * for a settler. Empty: no simulated job matches (guard, monk, fisher...), never handed out.
	 */
	UPROPERTY(EditAnywhere, Category = "Villagers")
	TArray<FName> Jobs;
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

	/**
	 * Deterministic tilt envelope, in degrees, sampled per instance. 0 keeps every
	 * instance plumb -- which is what a manufactured object wants and what every entry
	 * did before this field existed. A few degrees is what stops a stand of trees from
	 * reading as a row of identical posts.
	 */
	UPROPERTY(EditAnywhere, Category = "Presentation", meta = (ClampMin = "0.0", ClampMax = "20.0"))
	float MaxLeanDegrees = 0.0f;
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

	/**
	 * The visual population, written by tools/unreal/import-villagers.py from the manifest.
	 * Empty -- the code fallback, or an asset older than this field -- draws no villager
	 * card; the debug spheres (anastasis.Village.Debug) still show the simulation.
	 */
	UPROPERTY(EditAnywhere, Category = "Villagers")
	TArray<FAnastasisVillagerLook> Villagers;

	/** Masked card material with a `Portrait` texture and a `Mirror` scalar parameter (M_AnastasisVillager). */
	UPROPERTY(EditAnywhere, Category = "Villagers")
	TSoftObjectPtr<UMaterialInterface> VillagerMaterial;

	/** First enabled entry for this type, or nullptr. */
	const FAnastasisPresentationEntry* FindEntry(AnastasisWorld::ETileType Type) const;

	/** The VISUAL_BUILD_001 values, as data. The fallback when the asset cannot be used. */
	static UAnastasisPresentationRegistry* CreateCodeDefaults(UObject* Outer);
};
