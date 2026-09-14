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
	constexpr const TCHAR* TreeMeshBroadleafSub = TEXT("/Game/Anastasis/Vegetation/SM_Tree_Broadleaf_Subcanopy_01.SM_Tree_Broadleaf_Subcanopy_01");
	constexpr const TCHAR* TreeMeshBroadleafCanopy = TEXT("/Game/Anastasis/Vegetation/SM_Tree_Broadleaf_Canopy_01.SM_Tree_Broadleaf_Canopy_01");
	constexpr const TCHAR* DefaultRuinMeshPath = TEXT("/Engine/BasicShapes/Cylinder.Cylinder");

	/**
	 * Bark and foliage are vertex colours baked into the mesh; this material is what reads
	 * them. Without it the archetype's single Tint would paint the trunk green too, and the
	 * trunk -- the thing this grammar exists to make visible -- would stop existing.
	 * Owned by tools/unreal/create_tree_asset.py, same file that bakes the colours.
	 */
	constexpr const TCHAR* VegetationMaterialPath = TEXT("/Game/Anastasis/Materials/M_AnastasisVegetation.M_AnastasisVegetation");

	FAnastasisPresentationVariant MakeVariant(
		const TCHAR* MeshPath,
		EAnastasisStatureClass Stature,
		float ScaleBias,
		const TCHAR* MaterialPath)
	{
		FAnastasisPresentationVariant Variant;
		Variant.Mesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(MeshPath));
		Variant.Stature = Stature;
		Variant.ScaleBias = ScaleBias;
		if (MaterialPath)
		{
			Variant.MaterialOverride = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(MaterialPath));
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
			MakeVariant(TreeMeshUnderstory, EAnastasisStatureClass::Understory, 1.00f, VegetationMaterialPath),
			MakeVariant(TreeMeshSubcanopy, EAnastasisStatureClass::Subcanopy, 1.00f, VegetationMaterialPath),
			// Broadleaves carry a bias below 1: hornbeam and beech sit under the spruce in a
			// Pontic stand, and their wide dome needs less height to read than a spire does.
			MakeVariant(TreeMeshBroadleafSub, EAnastasisStatureClass::Subcanopy, 0.92f, VegetationMaterialPath),
			MakeVariant(TreeMeshCanopy, EAnastasisStatureClass::Canopy, 1.00f, VegetationMaterialPath),
			MakeVariant(TreeMeshBroadleafCanopy, EAnastasisStatureClass::Canopy, 0.85f, VegetationMaterialPath),
			MakeVariant(TreeMeshEmergent, EAnastasisStatureClass::Emergent, 1.35f, VegetationMaterialPath),
		}));

	// Ruin is unchanged, lean included: a wall stub is a manufactured thing and stands plumb.
	Registry->Entries.Add(MakeDefaultEntry(
		EAnastasisSemanticType::Ruin, TEXT("Ruin_Generic"),
		FLinearColor(0.353f, 0.302f, 0.318f), 0.6f, 1.1f, 0.20f, 0.0f,
		{MakeVariant(DefaultRuinMeshPath, EAnastasisStatureClass::Any, 1.0f, nullptr)}));

	return Registry;
}
