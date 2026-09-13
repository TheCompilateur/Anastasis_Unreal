#include "WorldView/AnastasisPresentationRegistry.h"

#include "Engine/StaticMesh.h"

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
	// Same placeholder primitives and envelope VISUAL_BUILD_001 hardcoded. Kept as the
	// fallback so a missing data asset degrades to the previous, known-good look rather
	// than to an empty world.
	constexpr const TCHAR* DefaultTreeMeshPath = TEXT("/Engine/BasicShapes/Cone.Cone");
	constexpr const TCHAR* DefaultRuinMeshPath = TEXT("/Engine/BasicShapes/Cylinder.Cylinder");

	FAnastasisPresentationEntry MakeDefaultEntry(
		EAnastasisSemanticType SemanticType,
		const TCHAR* ArchetypeId,
		const TCHAR* MeshPath,
		const FLinearColor& Tint,
		float MinScale,
		float MaxScale,
		float JitterRadiusFraction)
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

		FAnastasisPresentationVariant Variant;
		Variant.Mesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(MeshPath));
		Entry.Variants.Add(Variant);
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

	// Tints mirror AnastasisTerrainSurface::SurfaceTypeColor for the same ETileType, so an
	// instance and the ground it stands on read as one material family.
	Registry->Entries.Add(MakeDefaultEntry(
		EAnastasisSemanticType::Forest, TEXT("Tree_Generic"), DefaultTreeMeshPath,
		FLinearColor(0.102f, 0.243f, 0.114f), 1.6f, 2.4f, 0.30f));
	Registry->Entries.Add(MakeDefaultEntry(
		EAnastasisSemanticType::Ruin, TEXT("Ruin_Generic"), DefaultRuinMeshPath,
		FLinearColor(0.353f, 0.302f, 0.318f), 0.6f, 1.1f, 0.20f));

	return Registry;
}
