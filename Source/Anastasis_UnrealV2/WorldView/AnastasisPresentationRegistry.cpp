#include "WorldView/AnastasisPresentationRegistry.h"

#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"

// The UENUM is a mirror, not a second source of truth. If AnastasisSim ever renumbers or
// extends ETileType, this stops compiling instead of silently mapping Forest onto Scrub.
static_assert(static_cast<uint8>(EAnastasisSemanticType::Grass) == static_cast<uint8>(AnastasisWorld::ETileType::Grass), "semantic mirror drift: Grass");
static_assert(static_cast<uint8>(EAnastasisSemanticType::Water) == static_cast<uint8>(AnastasisWorld::ETileType::Water), "semantic mirror drift: Water");
static_assert(static_cast<uint8>(EAnastasisSemanticType::Stone) == static_cast<uint8>(AnastasisWorld::ETileType::Stone), "semantic mirror drift: Stone");
static_assert(static_cast<uint8>(EAnastasisSemanticType::Ruin) == static_cast<uint8>(AnastasisWorld::ETileType::Ruin), "semantic mirror drift: Ruin");
static_assert(static_cast<uint8>(EAnastasisSemanticType::Forest) == static_cast<uint8>(AnastasisWorld::ETileType::Forest), "semantic mirror drift: Forest");
static_assert(static_cast<uint8>(EAnastasisSemanticType::Scrub) == static_cast<uint8>(AnastasisWorld::ETileType::Scrub), "semantic mirror drift: Scrub");
static_assert(static_cast<uint8>(EAnastasisSemanticType::Field) == static_cast<uint8>(AnastasisWorld::ETileType::Field), "semantic mirror drift: Field");
static_assert(AnastasisWorld::TileTypeCount == 7, "EAnastasisSemanticType must mirror every ETileType");

namespace
{
	// Fallback look when the data asset is missing -- never an empty world. Ruin stays the
	// VISUAL_BUILD_001 engine primitive. Forest is the ANASTASIS tree grammar: one built
	// silhouette per vertical stratum of the reference plate
	// docs/visual/reference/pontique-etat-zero-3-stratification-forestiere.png, all produced
	// by tools/unreal/create_tree_asset.py, which owns them.
	//
	// Every tree mesh spans exactly Z = [-50, +50], the EngineBasicShapeSize=100uu
	// convention, so the base-lift math in AnastasisWorldEmbodiment and the sealed
	// Anastasis.Terrain.DressingRestsOnRenderedGround identity hold unchanged. Only the
	// PROPORTIONS inside that box differ between statures -- trunk fraction, crown width,
	// tier count -- which is the whole point: a young tree is not a dominant one scaled down.
	constexpr const TCHAR* TreeMeshUnderstory = TEXT("/Game/Anastasis/Vegetation/SM_Tree_Conifer_Understory_01.SM_Tree_Conifer_Understory_01");
	constexpr const TCHAR* TreeMeshSubcanopy = TEXT("/Game/Anastasis/Vegetation/SM_Tree_Conifer_Subcanopy_01.SM_Tree_Conifer_Subcanopy_01");
	constexpr const TCHAR* TreeMeshCanopy = TEXT("/Game/Anastasis/Vegetation/SM_Tree_Conifer_Canopy_01.SM_Tree_Conifer_Canopy_01");
	constexpr const TCHAR* TreeMeshEmergent = TEXT("/Game/Anastasis/Vegetation/SM_Tree_Conifer_Emergent_01.SM_Tree_Conifer_Emergent_01");
	constexpr const TCHAR* TreeMeshBroadleafUnder = TEXT("/Game/Anastasis/Vegetation/SM_Tree_Broadleaf_Understory_01.SM_Tree_Broadleaf_Understory_01");
	constexpr const TCHAR* TreeMeshBroadleafSub = TEXT("/Game/Anastasis/Vegetation/SM_Tree_Broadleaf_Subcanopy_01.SM_Tree_Broadleaf_Subcanopy_01");
	constexpr const TCHAR* TreeMeshBroadleafCanopy = TEXT("/Game/Anastasis/Vegetation/SM_Tree_Broadleaf_Canopy_01.SM_Tree_Broadleaf_Canopy_01");
	constexpr const TCHAR* TreeMeshBroadleafEmergent = TEXT("/Game/Anastasis/Vegetation/SM_Tree_Broadleaf_Emergent_01.SM_Tree_Broadleaf_Emergent_01");
	constexpr const TCHAR* DefaultRuinMeshPath = TEXT("/Engine/BasicShapes/Cylinder.Cylinder");

	/**
	 * Bark and foliage are vertex colours baked into the mesh; this material is what reads
	 * them. Without it the archetype's single Tint would paint the trunk green too, and the
	 * trunk -- the thing this grammar exists to make visible -- would stop existing.
	 * Owned by tools/unreal/create_tree_asset.py, same file that bakes the colours.
	 */
	constexpr const TCHAR* VegetationMaterialPath = TEXT("/Game/Anastasis/Materials/M_AnastasisVegetation.M_AnastasisVegetation");

	/**
	 * Slot 1 of every tree mesh. Default Lit, opaque, matte: wood is not a leaf, and giving
	 * it the foliage shading model lifted every trunk in value against a bright ground.
	 * Owned by the same generator as the meshes and the foliage material.
	 */
	constexpr const TCHAR* BarkMaterialPath = TEXT("/Game/Anastasis/Materials/M_AnastasisBark.M_AnastasisBark");

	FAnastasisPresentationVariant MakeVariant(
		const TCHAR* MeshPath,
		EAnastasisStatureClass Stature,
		EAnastasisFoliageFamily Family,
		float ScaleBias,
		const TCHAR* MaterialPath,
		const TCHAR* Slot1MaterialPath = nullptr)
	{
		FAnastasisPresentationVariant Variant;
		Variant.Mesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(MeshPath));
		Variant.Stature = Stature;
		Variant.Family = Family;
		Variant.ScaleBias = ScaleBias;
		if (MaterialPath)
		{
			Variant.MaterialOverride = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(MaterialPath));
		}
		if (Slot1MaterialPath)
		{
			Variant.AdditionalMaterialOverrides.Add(
				TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(Slot1MaterialPath)));
		}
		return Variant;
	}

	/**
	 * FOREST_TERRAIN_P1 -- the species grammar, also produced by create_tree_asset.py.
	 * Three shapes per species (_01.._03), heights in metres for a mature tree. Kept equal to
	 * SPECIES in tools/unreal/set_tree_grammar.py: the data asset is authoritative, this is its
	 * fallback, and the two must say the same thing.
	 */
	struct FSpeciesDefault
	{
		const TCHAR* MeshStem;
		EAnastasisTreeSpecies Species;
		EAnastasisFoliageFamily Family;
		float MinHeightM;
		float MaxHeightM;
	};
	constexpr int32 ShapesPerSpecies = 3;
	const FSpeciesDefault SpeciesDefaults[] = {
		{TEXT("SM_Tree_AleppoPine"), EAnastasisTreeSpecies::AleppoPine, EAnastasisFoliageFamily::Conifer, 11.0f, 18.0f},
		{TEXT("SM_Tree_Cypress"), EAnastasisTreeSpecies::Cypress, EAnastasisFoliageFamily::Conifer, 12.0f, 20.0f},
		{TEXT("SM_Tree_HolmOak"), EAnastasisTreeSpecies::HolmOak, EAnastasisFoliageFamily::Broadleaf, 8.0f, 14.0f},
		{TEXT("SM_Tree_Olive"), EAnastasisTreeSpecies::Olive, EAnastasisFoliageFamily::Broadleaf, 4.5f, 8.0f},
		{TEXT("SM_Tree_PlaneTree"), EAnastasisTreeSpecies::PlaneTree, EAnastasisFoliageFamily::Broadleaf, 17.0f, 24.0f},
		{TEXT("SM_Tree_BlackPine"), EAnastasisTreeSpecies::BlackPine, EAnastasisFoliageFamily::Conifer, 15.0f, 23.0f},
		{TEXT("SM_Tree_GreekFir"), EAnastasisTreeSpecies::GreekFir, EAnastasisFoliageFamily::Conifer, 14.0f, 22.0f},
	};

	/**
	 * GPT_FLORA_001 -- the trees drawn from the GPT plate (create-gpt-flora.py), one shape each.
	 * Kept equal to GPT_SPECIES in tools/unreal/set_tree_grammar.py. Pin sombre and cypres join the
	 * existing BlackPine and Cypress species as a further shape; the three others are new species.
	 */
	struct FGptSpeciesDefault
	{
		const TCHAR* MeshName;
		EAnastasisTreeSpecies Species;
		EAnastasisFoliageFamily Family;
		float MinHeightM;
		float MaxHeightM;
	};
	const FGptSpeciesDefault GptSpeciesDefaults[] = {
		{TEXT("SM_Gpt_Chene"), EAnastasisTreeSpecies::DeciduousOak, EAnastasisFoliageFamily::Broadleaf, 14.0f, 22.0f},
		{TEXT("SM_Gpt_Bouleau"), EAnastasisTreeSpecies::Birch, EAnastasisFoliageFamily::Broadleaf, 12.0f, 18.0f},
		{TEXT("SM_Gpt_PinSylvestre"), EAnastasisTreeSpecies::ScotsPine, EAnastasisFoliageFamily::Conifer, 20.0f, 30.0f},
		{TEXT("SM_Gpt_SaulePleureur"), EAnastasisTreeSpecies::Willow, EAnastasisFoliageFamily::Broadleaf, 10.0f, 16.0f},
		{TEXT("SM_Gpt_MarronnierFleuri"), EAnastasisTreeSpecies::HorseChestnut, EAnastasisFoliageFamily::Broadleaf, 14.0f, 20.0f},
		{TEXT("SM_Gpt_PinSombre"), EAnastasisTreeSpecies::BlackPine, EAnastasisFoliageFamily::Conifer, 15.0f, 23.0f},
		{TEXT("SM_Gpt_Cypres"), EAnastasisTreeSpecies::Cypress, EAnastasisFoliageFamily::Conifer, 12.0f, 20.0f},
	};
	const TCHAR* GptFoliageMaterialPath = TEXT("/Game/Anastasis/Materials/M_AnastasisGptFoliage.M_AnastasisGptFoliage");

	FAnastasisPresentationEntry MakeDefaultEntry(
		EAnastasisSemanticType SemanticType,
		const TCHAR* ArchetypeId,
		const FLinearColor& Tint,
		float MinScale,
		float MaxScale,
		float JitterRadiusFraction,
		float MaxLeanDegrees,
		TArray<FAnastasisPresentationVariant> Variants)
	{
		FAnastasisPresentationEntry Entry;
		Entry.SemanticType = SemanticType;
		Entry.ArchetypeId = FName(ArchetypeId);
		Entry.bEnabled = true;
		Entry.Tint = Tint;
		Entry.MinUniformScale = MinScale;
		Entry.MaxUniformScale = MaxScale;
		Entry.JitterRadiusFraction = JitterRadiusFraction;
		Entry.bRandomYaw = true;
		Entry.MaxLeanDegrees = MaxLeanDegrees;
		Entry.Variants = MoveTemp(Variants);
		return Entry;
	}
}

const FAnastasisPresentationEntry* UAnastasisPresentationRegistry::FindEntry(AnastasisWorld::ETileType Type) const
{
	const EAnastasisSemanticType Wanted = ToSemanticType(Type);
	for (const FAnastasisPresentationEntry& Entry : Entries)
	{
		if (Entry.SemanticType == Wanted && Entry.bEnabled)
		{
			return &Entry;
		}
	}
	return nullptr;
}

UAnastasisPresentationRegistry* UAnastasisPresentationRegistry::CreateCodeDefaults(UObject* Outer)
{
	UAnastasisPresentationRegistry* Registry = NewObject<UAnastasisPresentationRegistry>(
		Outer ? Outer : GetTransientPackage());

	// The envelope is what turns a 100uu mesh into a tree of a believable height. Measured
	// against this world: one tile is 100uu, and before this grammar the median instance
	// stood at 104uu -- a metre-high marker, shorter than the ruin beside it. Multiplied by
	// the ecological layer factors in FAnastasisForestDressingSettings (young 0.25-0.45,
	// secondary 0.50-0.78, canopy 0.95-1.20) this envelope gives roughly 0.9m of sapling,
	// 2-4m of sub-canopy, 3.4-6m of canopy, and up to ~8m for an emergent through its
	// ScaleBias. The strata overlap, as they do in a real stand.
	FAnastasisPresentationEntry& Forest = Registry->Entries.Add_GetRef(MakeDefaultEntry(
		EAnastasisSemanticType::Forest, TEXT("Tree_Generic"),
		FLinearColor(0.102f, 0.243f, 0.114f), 3.6f, 5.0f, 0.30f, 5.0f,
		{
			// Four statures x two families: the grid the reference plate authorises, filled.
			// Broadleaves carry a bias below 1: hornbeam and beech sit under the spruce in a
			// Pontic stand, and their wide dome needs less height to read than a spire does.
			MakeVariant(TreeMeshUnderstory, EAnastasisStatureClass::Understory, EAnastasisFoliageFamily::Conifer, 1.00f, VegetationMaterialPath, BarkMaterialPath),
			MakeVariant(TreeMeshBroadleafUnder, EAnastasisStatureClass::Understory, EAnastasisFoliageFamily::Broadleaf, 0.90f, VegetationMaterialPath, BarkMaterialPath),
			MakeVariant(TreeMeshSubcanopy, EAnastasisStatureClass::Subcanopy, EAnastasisFoliageFamily::Conifer, 1.00f, VegetationMaterialPath, BarkMaterialPath),
			MakeVariant(TreeMeshBroadleafSub, EAnastasisStatureClass::Subcanopy, EAnastasisFoliageFamily::Broadleaf, 0.92f, VegetationMaterialPath, BarkMaterialPath),
			MakeVariant(TreeMeshCanopy, EAnastasisStatureClass::Canopy, EAnastasisFoliageFamily::Conifer, 1.00f, VegetationMaterialPath, BarkMaterialPath),
			MakeVariant(TreeMeshBroadleafCanopy, EAnastasisStatureClass::Canopy, EAnastasisFoliageFamily::Broadleaf, 0.85f, VegetationMaterialPath, BarkMaterialPath),
			MakeVariant(TreeMeshEmergent, EAnastasisStatureClass::Emergent, EAnastasisFoliageFamily::Conifer, 1.35f, VegetationMaterialPath, BarkMaterialPath),
			MakeVariant(TreeMeshBroadleafEmergent, EAnastasisStatureClass::Emergent, EAnastasisFoliageFamily::Broadleaf, 1.12f, VegetationMaterialPath, BarkMaterialPath),
		}));

	// FOREST_TERRAIN_P1. Appended AFTER the Pontic grid, which stays as the untagged fallback:
	// a request that names no species, or a species whose meshes are not generated yet, still
	// draws a tree. Stature Any -- age is a matter of height here, set by HeightRangeM and the
	// tree's maturity, not of a separate mesh.
	for (const FSpeciesDefault& Row : SpeciesDefaults)
	{
		for (int32 Shape = 1; Shape <= ShapesPerSpecies; ++Shape)
		{
			const FString Name = FString::Printf(TEXT("%s_%02d"), Row.MeshStem, Shape);
			const FString Path = FString::Printf(TEXT("/Game/Anastasis/Vegetation/%s.%s"), *Name, *Name);
			FAnastasisPresentationVariant Variant = MakeVariant(*Path, EAnastasisStatureClass::Any, Row.Family, 1.0f,
				VegetationMaterialPath, BarkMaterialPath);
			Variant.Species = Row.Species;
			Variant.HeightRangeM = FVector2D(Row.MinHeightM, Row.MaxHeightM);
			Forest.Variants.Add(MoveTemp(Variant));
		}
	}

	for (const FGptSpeciesDefault& Row : GptSpeciesDefaults)
	{
		const FString Path = FString::Printf(TEXT("/Game/Anastasis/Vegetation/Gpt/%s.%s"), Row.MeshName, Row.MeshName);
		FAnastasisPresentationVariant Variant = MakeVariant(*Path, EAnastasisStatureClass::Any, Row.Family, 1.0f,
			GptFoliageMaterialPath, BarkMaterialPath);
		Variant.Species = Row.Species;
		Variant.HeightRangeM = FVector2D(Row.MinHeightM, Row.MaxHeightM);
		Forest.Variants.Add(MoveTemp(Variant));
	}

	// Ruin is unchanged, lean included: a wall stub is a manufactured thing and stands plumb.
	Registry->Entries.Add(MakeDefaultEntry(
		EAnastasisSemanticType::Ruin, TEXT("Ruin_Generic"),
		FLinearColor(0.353f, 0.302f, 0.318f), 0.6f, 1.1f, 0.20f, 0.0f,
		{MakeVariant(DefaultRuinMeshPath, EAnastasisStatureClass::Any, EAnastasisFoliageFamily::Any, 1.0f, nullptr)}));

	return Registry;
}
