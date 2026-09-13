#include "WorldView/AnastasisPresentationRegistry.h"
#include "WorldView/AnastasisPresentationResolver.h"
#include "WorldView/AnastasisWorldView.h"
#include "Misc/AutomationTest.h"

#include "Engine/StaticMesh.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	/** A registry built in-test, so fallback/variant behaviour is exercised without depending on the shipped asset. */
	UAnastasisPresentationRegistry* MakeRegistry()
	{
		return NewObject<UAnastasisPresentationRegistry>(GetTransientPackage());
	}

	FAnastasisPresentationVariant MakeVariant(const TCHAR* MeshPath)
	{
		FAnastasisPresentationVariant Variant;
		if (MeshPath)
		{
			Variant.Mesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(MeshPath));
		}
		return Variant;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisPresentationReachability, "Anastasis.Presentation.Reachability", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnastasisPresentationReachability::RunTest(const FString&)
{
	using namespace AnastasisPresentation;

	// Only Forest/Ruin carry a discrete instance in this slice: everything else stays
	// ground-colour only (AnastasisTerrainSurface). This is the REACHABILITY_GATE boundary
	// the audit documents -- lock it so a silent drift is a test failure.
	const AnastasisWorld::ETileType Instanced[] = {AnastasisWorld::ETileType::Forest, AnastasisWorld::ETileType::Ruin};
	const AnastasisWorld::ETileType GroundOnly[] = {
		AnastasisWorld::ETileType::Grass, AnastasisWorld::ETileType::Water,
		AnastasisWorld::ETileType::Stone, AnastasisWorld::ETileType::Scrub,
		AnastasisWorld::ETileType::Field};

	for (const AnastasisWorld::ETileType Type : Instanced)
	{
		TestNotNull(*FString::Printf(TEXT("%s resolves to an entry"), AnastasisWorld::TileTypeName(Type)), FindEntry(Type));
	}
	for (const AnastasisWorld::ETileType Type : GroundOnly)
	{
		TestNull(*FString::Printf(TEXT("%s has no discrete instance in this slice"), AnastasisWorld::TileTypeName(Type)), FindEntry(Type));
	}

	const FAnastasisPresentationEntry* Tree = FindEntry(AnastasisWorld::ETileType::Forest);
	const FAnastasisPresentationEntry* Ruin = FindEntry(AnastasisWorld::ETileType::Ruin);
	if (!TestNotNull(TEXT("forest entry"), Tree) || !TestNotNull(TEXT("ruin entry"), Ruin))
	{
		return false;
	}

	// The registry is the source of the look: each migrated type must name an archetype and
	// carry at least one variant with a mesh, whether it came from the asset or the fallback.
	TestTrue(TEXT("forest names an archetype"), !Tree->ArchetypeId.IsNone());
	TestTrue(TEXT("ruin names an archetype"), !Ruin->ArchetypeId.IsNone());
	TestTrue(TEXT("forest has a usable variant"), SelectVariantIndex(*Tree, 12345u, 3, 7) != INDEX_NONE);
	TestTrue(TEXT("ruin has a usable variant"), SelectVariantIndex(*Ruin, 12345u, 3, 7) != INDEX_NONE);
	TestTrue(TEXT("forest scale envelope is ordered"), Tree->MinUniformScale <= Tree->MaxUniformScale);
	TestTrue(TEXT("ruin scale envelope is ordered"), Ruin->MinUniformScale <= Ruin->MaxUniformScale);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisPresentationDeterminism, "Anastasis.Presentation.Determinism", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnastasisPresentationDeterminism::RunTest(const FString&)
{
	using namespace AnastasisPresentation;

	const FAnastasisPresentationEntry* Tree = FindEntry(AnastasisWorld::ETileType::Forest);
	if (!TestNotNull(TEXT("forest entry"), Tree))
	{
		return false;
	}

	const FTransform A = ResolveInstanceTransform(*Tree, 12345u, 10, 20, 0.4);
	const FTransform B = ResolveInstanceTransform(*Tree, 12345u, 10, 20, 0.4);
	TestTrue(TEXT("same seed+tile+alt reproduces the exact transform"), A.Equals(B, 0.0));

	const FTransform Other = ResolveInstanceTransform(*Tree, 12345u, 10, 21, 0.4);
	TestTrue(TEXT("a different tile does not collide onto the same transform"), !A.Equals(Other, 0.01));

	const FVector Expected = AnastasisWorldView::TileToUnreal(10, 20, 0.4);
	const FVector2D Delta(A.GetLocation().X - Expected.X, A.GetLocation().Y - Expected.Y);
	TestTrue(TEXT("XY jitter stays within the tile"),
		Delta.Size() <= Tree->JitterRadiusFraction * AnastasisWorldView::TileWorldSize + KINDA_SMALL_NUMBER);
	TestTrue(TEXT("instance base rests at Alt, not its centre"), A.GetLocation().Z > Expected.Z);

	const double Scale = A.GetScale3D().X;
	TestTrue(TEXT("scale within the entry's declared envelope"),
		Scale >= Tree->MinUniformScale - KINDA_SMALL_NUMBER && Scale <= Tree->MaxUniformScale + KINDA_SMALL_NUMBER);

	// Variant identity must be as reproducible as the transform, and must actually vary
	// across tiles once more than one variant is eligible.
	UAnastasisPresentationRegistry* Registry = MakeRegistry();
	FAnastasisPresentationEntry Multi;
	Multi.SemanticType = EAnastasisSemanticType::Forest;
	Multi.ArchetypeId = FName(TEXT("Tree_MultiVariant"));
	Multi.Variants.Add(MakeVariant(TEXT("/Engine/BasicShapes/Cone.Cone")));
	Multi.Variants.Add(MakeVariant(TEXT("/Engine/BasicShapes/Cylinder.Cylinder")));
	Multi.Variants.Add(MakeVariant(TEXT("/Engine/BasicShapes/Cube.Cube")));
	Registry->Entries.Add(Multi);
	const FAnastasisPresentationEntry& MultiEntry = Registry->Entries[0];

	TestEqual(TEXT("variant choice is reproducible"),
		SelectVariantIndex(MultiEntry, 12345u, 10, 20), SelectVariantIndex(MultiEntry, 12345u, 10, 20));

	TSet<int32> Seen;
	for (int32 Y = 0; Y < 16; ++Y)
	{
		for (int32 X = 0; X < 16; ++X)
		{
			const int32 Chosen = SelectVariantIndex(MultiEntry, 12345u, X, Y);
			TestTrue(TEXT("variant index within range"), Chosen >= 0 && Chosen < MultiEntry.Variants.Num());
			Seen.Add(Chosen);
		}
	}
	TestTrue(TEXT("different eligible tiles do reach different variants"), Seen.Num() > 1);

	// A different seed must be able to move a tile onto another variant: identity, not position alone.
	bool bSeedChangesSomething = false;
	for (int32 X = 0; X < 64 && !bSeedChangesSomething; ++X)
	{
		bSeedChangesSomething = SelectVariantIndex(MultiEntry, 12345u, X, 0) != SelectVariantIndex(MultiEntry, 999u, X, 0);
	}
	TestTrue(TEXT("seed participates in variant identity"), bSeedChangesSomething);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisPresentationFallback, "Anastasis.Presentation.Fallback", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnastasisPresentationFallback::RunTest(const FString&)
{
	using namespace AnastasisPresentation;

	// Missing data must degrade in a controlled way: never crash, never silently render
	// something the data did not ask for.
	UAnastasisPresentationRegistry* Registry = MakeRegistry();

	FAnastasisPresentationEntry NoVariants;
	NoVariants.SemanticType = EAnastasisSemanticType::Forest;
	NoVariants.ArchetypeId = FName(TEXT("Tree_Empty"));
	Registry->Entries.Add(NoVariants);

	FAnastasisPresentationEntry EmptyMesh;
	EmptyMesh.SemanticType = EAnastasisSemanticType::Ruin;
	EmptyMesh.ArchetypeId = FName(TEXT("Ruin_NoMesh"));
	EmptyMesh.Variants.Add(MakeVariant(nullptr));
	Registry->Entries.Add(EmptyMesh);

	TestEqual(TEXT("no variants -> nothing to draw"), SelectVariantIndex(Registry->Entries[0], 12345u, 1, 1), int32(INDEX_NONE));
	TestEqual(TEXT("variant without a mesh is not eligible"), SelectVariantIndex(Registry->Entries[1], 12345u, 1, 1), int32(INDEX_NONE));

	// A disabled entry is an explicit data-side off switch, distinct from missing data.
	FAnastasisPresentationEntry Disabled;
	Disabled.SemanticType = EAnastasisSemanticType::Forest;
	Disabled.ArchetypeId = FName(TEXT("Tree_Disabled"));
	Disabled.bEnabled = false;
	Disabled.Variants.Add(MakeVariant(TEXT("/Engine/BasicShapes/Cone.Cone")));
	UAnastasisPresentationRegistry* DisabledRegistry = MakeRegistry();
	DisabledRegistry->Entries.Add(Disabled);
	TestNull(TEXT("disabled entry is not served"), DisabledRegistry->FindEntry(AnastasisWorld::ETileType::Forest));

	// An empty registry yields nothing rather than a dangling entry pointer.
	UAnastasisPresentationRegistry* Empty = MakeRegistry();
	TestNull(TEXT("empty registry resolves nothing"), Empty->FindEntry(AnastasisWorld::ETileType::Forest));

	// The code defaults are a real, usable registry: this is what a missing asset falls back to.
	UAnastasisPresentationRegistry* Defaults = UAnastasisPresentationRegistry::CreateCodeDefaults(GetTransientPackage());
	if (!TestNotNull(TEXT("code defaults exist"), Defaults))
	{
		return false;
	}
	TestNotNull(TEXT("code defaults cover Forest"), Defaults->FindEntry(AnastasisWorld::ETileType::Forest));
	TestNotNull(TEXT("code defaults cover Ruin"), Defaults->FindEntry(AnastasisWorld::ETileType::Ruin));
	TestNull(TEXT("code defaults do not invent other types"), Defaults->FindEntry(AnastasisWorld::ETileType::Grass));

	// The live resolver must always hand back a usable registry, asset or not.
	TestTrue(TEXT("live registry is never empty"), GetRegistry().Entries.Num() > 0);

	return true;
}

#endif
