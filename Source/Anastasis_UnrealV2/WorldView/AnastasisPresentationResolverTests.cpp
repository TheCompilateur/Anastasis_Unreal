#include "WorldView/AnastasisPresentationResolver.h"
#include "WorldView/AnastasisWorldView.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisPresentationReachability, "Anastasis.Presentation.Reachability", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnastasisPresentationReachability::RunTest(const FString&)
{
	using namespace AnastasisPresentation;

	// Only Forest/Ruin have a discrete instance in this slice: everything else stays
	// ground-color only (AnastasisTerrainSurface). This is the REACHABILITY_GATE
	// boundary the audit documents -- lock it so a silent drift is a test failure.
	const AnastasisWorld::ETileType Instanced[] = {AnastasisWorld::ETileType::Forest, AnastasisWorld::ETileType::Ruin};
	const AnastasisWorld::ETileType Ungrounded[] = {
		AnastasisWorld::ETileType::Grass, AnastasisWorld::ETileType::Water,
		AnastasisWorld::ETileType::Stone, AnastasisWorld::ETileType::Scrub,
		AnastasisWorld::ETileType::Field};

	for (const AnastasisWorld::ETileType Type : Instanced)
	{
		TestNotNull(*FString::Printf(TEXT("%s resolves to a definition"), AnastasisWorld::TileTypeName(Type)), Resolve(Type));
	}
	for (const AnastasisWorld::ETileType Type : Ungrounded)
	{
		TestNull(*FString::Printf(TEXT("%s has no discrete instance in this slice"), AnastasisWorld::TileTypeName(Type)), Resolve(Type));
	}

	const FRenderableDefinition* Tree = Resolve(AnastasisWorld::ETileType::Forest);
	const FRenderableDefinition* Ruin = Resolve(AnastasisWorld::ETileType::Ruin);
	if (!TestNotNull(TEXT("tree definition"), Tree) || !TestNotNull(TEXT("ruin definition"), Ruin))
	{
		return false;
	}
	TestEqual(TEXT("tree archetype id"), Tree->ArchetypeId, FName(TEXT("Tree_Generic")));
	TestEqual(TEXT("ruin archetype id"), Ruin->ArchetypeId, FName(TEXT("Ruin_Generic")));
	TestEqual(TEXT("two archetypes in this slice"), AllArchetypes().Num(), 2);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisPresentationDeterminism, "Anastasis.Presentation.Determinism", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnastasisPresentationDeterminism::RunTest(const FString&)
{
	using namespace AnastasisPresentation;

	const FRenderableDefinition* Tree = Resolve(AnastasisWorld::ETileType::Forest);
	if (!TestNotNull(TEXT("tree definition"), Tree))
	{
		return false;
	}

	const FTransform A = ResolveInstanceTransform(*Tree, 12345u, 10, 20, 0.4);
	const FTransform B = ResolveInstanceTransform(*Tree, 12345u, 10, 20, 0.4);
	TestTrue(TEXT("same seed+tile+alt reproduces the exact transform"), A.Equals(B, 0.0));

	const FTransform Other = ResolveInstanceTransform(*Tree, 12345u, 10, 21, 0.4);
	TestTrue(TEXT("a different tile does not collide onto the same transform"), !A.Equals(Other, 0.01));

	const double TileWorldSize = AnastasisWorldView::TileWorldSize;
	const FVector Expected = AnastasisWorldView::TileToUnreal(10, 20, 0.4);
	const FVector2D Delta(A.GetLocation().X - Expected.X, A.GetLocation().Y - Expected.Y);
	TestTrue(TEXT("XY jitter stays within the tile"), Delta.Size() <= Tree->JitterRadiusFraction * TileWorldSize + KINDA_SMALL_NUMBER);
	TestTrue(TEXT("instance base rests at Alt, not its center"), A.GetLocation().Z > Expected.Z);

	const double Scale = A.GetScale3D().X;
	TestTrue(TEXT("scale within the archetype's declared range"), Scale >= Tree->MinUniformScale - KINDA_SMALL_NUMBER && Scale <= Tree->MaxUniformScale + KINDA_SMALL_NUMBER);

	return true;
}

#endif
