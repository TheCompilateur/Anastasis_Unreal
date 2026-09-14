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

	FAnastasisPresentationVariant MakeStatureVariant(const TCHAR* MeshPath, EAnastasisStatureClass Stature, float Bias = 1.0f)
	{
		FAnastasisPresentationVariant Variant = MakeVariant(MeshPath);
		Variant.Stature = Stature;
		Variant.ScaleBias = Bias;
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisPresentationStature, "Anastasis.Presentation.Stature", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnastasisPresentationStature::RunTest(const FString&)
{
	using namespace AnastasisPresentation;

	// A stature request must reach the look built for it -- otherwise the whole grammar
	// degenerates back into one mesh at several scales, which is exactly what it replaces.
	UAnastasisPresentationRegistry* Registry = MakeRegistry();
	FAnastasisPresentationEntry Graded;
	Graded.SemanticType = EAnastasisSemanticType::Forest;
	Graded.ArchetypeId = FName(TEXT("Tree_Graded"));
	Graded.Variants.Add(MakeStatureVariant(TEXT("/Engine/BasicShapes/Cone.Cone"), EAnastasisStatureClass::Understory));
	Graded.Variants.Add(MakeStatureVariant(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"), EAnastasisStatureClass::Subcanopy));
	Graded.Variants.Add(MakeStatureVariant(TEXT("/Engine/BasicShapes/Cube.Cube"), EAnastasisStatureClass::Canopy));
	Graded.Variants.Add(MakeStatureVariant(TEXT("/Engine/BasicShapes/Sphere.Sphere"), EAnastasisStatureClass::Emergent, 1.35f));
	Registry->Entries.Add(Graded);
	const FAnastasisPresentationEntry& Entry = Registry->Entries[0];

	const EAnastasisStatureClass Wanted[] = {
		EAnastasisStatureClass::Understory, EAnastasisStatureClass::Subcanopy,
		EAnastasisStatureClass::Canopy, EAnastasisStatureClass::Emergent};
	for (int32 I = 0; I < UE_ARRAY_COUNT(Wanted); ++I)
	{
		for (int32 Y = 0; Y < 8; ++Y)
		{
			for (int32 X = 0; X < 8; ++X)
			{
				const int32 Chosen = SelectVariantIndex(Entry, 12345u, X, Y, Wanted[I]);
				if (!TestTrue(TEXT("a tagged stature resolves to its own variant, on every tile"), Chosen == I))
				{
					return false;
				}
			}
		}
	}

	// Same request, same tile, same answer: the grammar must be as reproducible as placement.
	TestEqual(TEXT("stature choice is reproducible"),
		SelectVariantIndex(Entry, 12345u, 4, 9, EAnastasisStatureClass::Canopy),
		SelectVariantIndex(Entry, 12345u, 4, 9, EAnastasisStatureClass::Canopy));

	// Any means "no opinion": it must still draw something, and it must reach more than one look.
	TSet<int32> SeenUnfiltered;
	for (int32 X = 0; X < 64; ++X)
	{
		SeenUnfiltered.Add(SelectVariantIndex(Entry, 12345u, X, 3, EAnastasisStatureClass::Any));
	}
	TestTrue(TEXT("an unfiltered request still spreads across variants"), SeenUnfiltered.Num() > 1);
	TestFalse(TEXT("an unfiltered request never resolves to nothing"), SeenUnfiltered.Contains(INDEX_NONE));

	// FAIL OPEN. Data with no look for the requested stature must still draw the archetype:
	// presence is simulation truth, stature is only dress.
	UAnastasisPresentationRegistry* Partial = MakeRegistry();
	FAnastasisPresentationEntry OnlyCanopy;
	OnlyCanopy.SemanticType = EAnastasisSemanticType::Forest;
	OnlyCanopy.ArchetypeId = FName(TEXT("Tree_OnlyCanopy"));
	OnlyCanopy.Variants.Add(MakeStatureVariant(TEXT("/Engine/BasicShapes/Cube.Cube"), EAnastasisStatureClass::Canopy));
	Partial->Entries.Add(OnlyCanopy);
	TestEqual(TEXT("a stature with no look falls back rather than rendering a hole"),
		SelectVariantIndex(Partial->Entries[0], 12345u, 2, 2, EAnastasisStatureClass::Emergent), 0);

	// Untagged data predates this axis and must keep answering every request unchanged.
	UAnastasisPresentationRegistry* Legacy = MakeRegistry();
	FAnastasisPresentationEntry Untagged;
	Untagged.SemanticType = EAnastasisSemanticType::Forest;
	Untagged.ArchetypeId = FName(TEXT("Tree_Untagged"));
	Untagged.Variants.Add(MakeVariant(TEXT("/Engine/BasicShapes/Cone.Cone")));
	Legacy->Entries.Add(Untagged);
	for (int32 I = 0; I < UE_ARRAY_COUNT(Wanted); ++I)
	{
		TestEqual(TEXT("an untagged variant serves every stature"),
			SelectVariantIndex(Legacy->Entries[0], 12345u, 5, 5, Wanted[I]), 0);
	}

	// ScaleBias must actually move the height, and must do it deterministically.
	const FTransform Plain = ResolveInstanceTransform(Entry, 12345u, 6, 6, 0.4, 1.0f);
	const FTransform Biased = ResolveInstanceTransform(Entry, 12345u, 6, 6, 0.4, 1.35f);
	TestTrue(TEXT("a scale bias raises the instance's scale"), Biased.GetScale3D().X > Plain.GetScale3D().X * 1.3);
	TestTrue(TEXT("a biased transform is reproducible"),
		Biased.Equals(ResolveInstanceTransform(Entry, 12345u, 6, 6, 0.4, 1.35f), 0.0));
	// The lift follows the biased scale, or the taller tree would hover.
	const FVector Ground = AnastasisWorldView::TileToUnreal(6, 6, 0.4);
	TestTrue(TEXT("the lift tracks the biased scale"),
		FMath::IsNearlyEqual(Biased.GetLocation().Z - Ground.Z,
			0.5 * EngineBasicShapeSize * Biased.GetScale3D().X, 1.e-6));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisPresentationLean, "Anastasis.Presentation.Lean", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnastasisPresentationLean::RunTest(const FString&)
{
	using namespace AnastasisPresentation;

	UAnastasisPresentationRegistry* Registry = MakeRegistry();
	FAnastasisPresentationEntry Plumb;
	Plumb.SemanticType = EAnastasisSemanticType::Ruin;
	Plumb.ArchetypeId = FName(TEXT("Plumb"));
	Plumb.MaxLeanDegrees = 0.0f;
	Plumb.Variants.Add(MakeVariant(TEXT("/Engine/BasicShapes/Cylinder.Cylinder")));

	FAnastasisPresentationEntry Leaning = Plumb;
	Leaning.SemanticType = EAnastasisSemanticType::Forest;
	Leaning.ArchetypeId = FName(TEXT("Leaning"));
	Leaning.MaxLeanDegrees = 5.0f;

	Registry->Entries.Add(Plumb);
	Registry->Entries.Add(Leaning);

	// MaxLeanDegrees=0 must reproduce the historical transform exactly: every entry that
	// does not ask for a tilt keeps standing the way it always did.
	double WorstPlumb = 0.0;
	double WorstLean = 0.0;
	bool bSomethingLeans = false;
	for (int32 Y = 0; Y < 16; ++Y)
	{
		for (int32 X = 0; X < 16; ++X)
		{
			const FVector PlumbUp = ResolveInstanceTransform(Registry->Entries[0], 12345u, X, Y, 0.4)
				.TransformVectorNoScale(FVector::UpVector);
			WorstPlumb = FMath::Max(WorstPlumb, FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(PlumbUp.Z, -1.0, 1.0))));

			const FVector LeanUp = ResolveInstanceTransform(Registry->Entries[1], 12345u, X, Y, 0.4)
				.TransformVectorNoScale(FVector::UpVector);
			const double Tilt = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(LeanUp.Z, -1.0, 1.0)));
			WorstLean = FMath::Max(WorstLean, Tilt);
			bSomethingLeans |= Tilt > 0.5;
		}
	}
	TestTrue(TEXT("MaxLeanDegrees=0 stays exactly plumb"), WorstPlumb < 1.e-6);
	TestTrue(TEXT("a lean envelope actually tilts instances"), bSomethingLeans);
	TestTrue(TEXT("no instance exceeds the declared envelope"), WorstLean <= 5.0 + 1.e-6);

	AddInfo(FString::Printf(TEXT("LEAN worst_plumb=%.9f worst_lean=%.3f"), WorstPlumb, WorstLean));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisTreePivotConvention, "Anastasis.Presentation.TreePivotConvention", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnastasisTreePivotConvention::RunTest(const FString&)
{
	using namespace AnastasisPresentation;

	// Two independent lift formulas coexist in AnastasisWorldEmbodiment: the legacy tile path
	// uses 0.5 * EngineBasicShapeSize * Scale, the ecological path uses -MeshBounds.Min.Z *
	// Scale. They only agree while every Forest mesh spans Z = [-50, +50]. A mesh rebuilt off
	// that convention would make trees hover or sink, and only in one of the two paths.
	const FAnastasisPresentationEntry* Forest = FindEntry(AnastasisWorld::ETileType::Forest);
	if (!TestNotNull(TEXT("forest entry"), Forest))
	{
		return false;
	}

	int32 Checked = 0;
	for (const FAnastasisPresentationVariant& Variant : Forest->Variants)
	{
		if (Variant.Mesh.IsNull())
		{
			continue;
		}
		UStaticMesh* Mesh = Variant.Mesh.LoadSynchronous();
		if (!Mesh)
		{
			AddWarning(FString::Printf(TEXT("variant mesh will not load: %s"), *Variant.Mesh.ToString()));
			continue;
		}
		const FBox Bounds = Mesh->GetBoundingBox();
		TestTrue(*FString::Printf(TEXT("%s base sits at -50"), *Mesh->GetName()),
			FMath::IsNearlyEqual(Bounds.Min.Z, -0.5 * EngineBasicShapeSize, 0.05));
		TestTrue(*FString::Printf(TEXT("%s top sits at +50"), *Mesh->GetName()),
			FMath::IsNearlyEqual(Bounds.Max.Z, 0.5 * EngineBasicShapeSize, 0.05));
		++Checked;
	}
	TestTrue(TEXT("the forest archetype names at least one loadable mesh"), Checked > 0);
	AddInfo(FString::Printf(TEXT("TREE_PIVOT variants_checked=%d"), Checked));
	return true;
}

#endif
