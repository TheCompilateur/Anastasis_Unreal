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
	 * ANASTASIS_ROCK_GRAMMAR_V1 -- six archetypes x three bounded variants, plus one
	 * composed cluster. Built by tools/unreal/create_rock_assets.py from a parametric
	 * genome, and wired into the data asset by tools/unreal/set_rock_presentation.py;
	 * these paths are the code fallback for the same set. See docs/unreal/ROCK_FORGE_001.md.
	 *
	 * Before this grammar existed, Stone had NO entry at all: ResolvePresentation returned
	 * false for every Stone tile and PlaceDressing drew nothing, so 1396 of the world's
	 * 9216 tiles were a ground colour and no geometry.
	 */
	constexpr const TCHAR* RockMeshPaths[] = {
		TEXT("/Game/Anastasis/Rock/SM_Rock_Massive_01.SM_Rock_Massive_01"),
		TEXT("/Game/Anastasis/Rock/SM_Rock_Massive_02.SM_Rock_Massive_02"),
		TEXT("/Game/Anastasis/Rock/SM_Rock_Massive_03.SM_Rock_Massive_03"),
		TEXT("/Game/Anastasis/Rock/SM_Rock_Low_01.SM_Rock_Low_01"),
		TEXT("/Game/Anastasis/Rock/SM_Rock_Low_02.SM_Rock_Low_02"),
		TEXT("/Game/Anastasis/Rock/SM_Rock_Low_03.SM_Rock_Low_03"),
		TEXT("/Game/Anastasis/Rock/SM_Rock_Vertical_01.SM_Rock_Vertical_01"),
		TEXT("/Game/Anastasis/Rock/SM_Rock_Vertical_02.SM_Rock_Vertical_02"),
		TEXT("/Game/Anastasis/Rock/SM_Rock_Vertical_03.SM_Rock_Vertical_03"),
		TEXT("/Game/Anastasis/Rock/SM_Rock_Split_01.SM_Rock_Split_01"),
		TEXT("/Game/Anastasis/Rock/SM_Rock_Split_02.SM_Rock_Split_02"),
		TEXT("/Game/Anastasis/Rock/SM_Rock_Split_03.SM_Rock_Split_03"),
		TEXT("/Game/Anastasis/Rock/SM_Rock_Boulder_01.SM_Rock_Boulder_01"),
		TEXT("/Game/Anastasis/Rock/SM_Rock_Boulder_02.SM_Rock_Boulder_02"),
		TEXT("/Game/Anastasis/Rock/SM_Rock_Boulder_03.SM_Rock_Boulder_03"),
		TEXT("/Game/Anastasis/Rock/SM_Rock_CliffFragment_01.SM_Rock_CliffFragment_01"),
		TEXT("/Game/Anastasis/Rock/SM_Rock_CliffFragment_02.SM_Rock_CliffFragment_02"),
		TEXT("/Game/Anastasis/Rock/SM_Rock_CliffFragment_03.SM_Rock_CliffFragment_03"),
		TEXT("/Game/Anastasis/Rock/SM_Rock_Cluster_01.SM_Rock_Cluster_01"),
	};

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
	Registry->Entries.Add(MakeDefaultEntry(
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

	// Ruin is unchanged, lean included: a wall stub is a manufactured thing and stands plumb.
	Registry->Entries.Add(MakeDefaultEntry(
		EAnastasisSemanticType::Ruin, TEXT("Ruin_Generic"),
		FLinearColor(0.353f, 0.302f, 0.318f), 0.6f, 1.1f, 0.20f, 0.0f,
		{MakeVariant(DefaultRuinMeshPath, EAnastasisStatureClass::Any, EAnastasisFoliageFamily::Any, 1.0f, nullptr)}));

	// STONE. Read against the same metre-per-tile ruler as the forest above: these meshes
	// stand 150-300uu at scale 1, so this envelope gives blocks of 0.45m to 2.1m -- the
	// ruin's size family, and well under the canopy. A rock is not a hill. An envelope of
	// 0.85-1.85 was tried first and the captures rejected it: from above the whole
	// formation read as a single pale blob instead of distinct mineral masses.
	//
	// Lean is 9 degrees where the ruin's is 0, and that contrast is the point: a wall stub
	// was built plumb, a boulder came to rest however it fell. The lean lifts the foot by
	// (1 - cos L) * 50 * Scale -- about 0.4uu here, against 27-76uu of buried skirt, so
	// ground contact is untouched.
	//
	// No variant carries a stature or family: those axes describe a stand of trees, not an
	// outcrop. SelectVariantIndex reads Any/Any as "no opinion" and fails open, so all
	// nineteen stay eligible.
	{
		TArray<FAnastasisPresentationVariant> RockVariants;
		RockVariants.Reserve(UE_ARRAY_COUNT(RockMeshPaths));
		for (const TCHAR* RockMeshPath : RockMeshPaths)
		{
			RockVariants.Add(MakeVariant(RockMeshPath, EAnastasisStatureClass::Any,
				EAnastasisFoliageFamily::Any, 1.0f, nullptr));
		}
		Registry->Entries.Add(MakeDefaultEntry(
			EAnastasisSemanticType::Stone, TEXT("Rock_Grammar_V1"),
			FLinearColor(0.150f, 0.142f, 0.134f), 0.30f, 0.70f, 0.34f, 9.0f,
			MoveTemp(RockVariants)));
	}

	return Registry;
}
