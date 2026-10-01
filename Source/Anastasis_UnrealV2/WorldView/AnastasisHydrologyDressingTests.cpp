#include "WorldView/AnastasisHydrologyDressing.h"
#include "WorldView/AnastasisTerrainSurface.h"
#include "World/AnastasisHydrology.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisHydrologyCanonicalArchetypes,
	"Anastasis.Hydrology.CanonicalArchetypes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnastasisHydrologyCanonicalArchetypes::RunTest(const FString&)
{
	const auto S = AnastasisWorldView::CaptureCanonicalWorld(12345u);
	AnastasisHydrologyDressing::FPlan Plan;
	FString Error;
	TestTrue(TEXT("canonical hydrology plan"), AnastasisHydrologyDressing::Build(S, nullptr, Plan, Error));
	TestTrue(TEXT("error empty"), Error.IsEmpty());
	TestTrue(TEXT("flowing water exists"), Plan.FlowingTiles > 0);
	TestTrue(TEXT("still water exists"), Plan.StillTiles > 0);
	TestTrue(TEXT("torrent tiles"), Plan.TorrentTiles > 0);
	TestTrue(TEXT("valley tiles"), Plan.ValleyTiles > 0);
	TestTrue(TEXT("inflow tiles"), Plan.InflowTiles > 0);
	TestTrue(TEXT("torrent site located"), Plan.Torrent.TileCount > 0 && Plan.Torrent.Centroid.Size() > 1.0);
	TestTrue(TEXT("valley site located"), Plan.Valley.TileCount > 0 && Plan.Valley.Centroid.Size() > 1.0);
	TestTrue(TEXT("inflow site located"), Plan.Inflow.TileCount > 0 && Plan.Inflow.Centroid.Size() > 1.0);
	TestTrue(TEXT("torrent is narrower than valley"), Plan.Torrent.MeanWidth < Plan.Valley.MeanWidth + 0.05);
	TestTrue(TEXT("torrent banks are steeper than valley"), Plan.Torrent.MeanBankRelief > Plan.Valley.MeanBankRelief);
	TestTrue(TEXT("flow ribbons exist"), Plan.Flow.Triangles.Num() >= 6);
	TestTrue(TEXT("wet banks exist"), Plan.Bank.Triangles.Num() >= 6);
	TestTrue(TEXT("rocks exist"), Plan.RockCount > 0);
	TestTrue(TEXT("reeds exist"), Plan.ReedCount > 0);
	AddInfo(FString::Printf(
		TEXT("flowing=%d still=%d torrent=%d valley=%d inflow=%d rocks=%d reeds=%d foam=%d torrent_relief=%.3f valley_relief=%.3f torrent_width=%.3f valley_width=%.3f"),
		Plan.FlowingTiles, Plan.StillTiles, Plan.TorrentTiles, Plan.ValleyTiles, Plan.InflowTiles,
		Plan.RockCount, Plan.ReedCount, Plan.FoamCount,
		Plan.Torrent.MeanBankRelief, Plan.Valley.MeanBankRelief,
		Plan.Torrent.MeanWidth, Plan.Valley.MeanWidth));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisHydrologyDeterminism,
	"Anastasis.Hydrology.Determinism",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnastasisHydrologyDeterminism::RunTest(const FString&)
{
	const auto S = AnastasisWorldView::CaptureCanonicalWorld(12345u);
	AnastasisHydrologyDressing::FPlan A, B;
	FString Error;
	TestTrue(TEXT("plan A"), AnastasisHydrologyDressing::Build(S, nullptr, A, Error));
	TestTrue(TEXT("plan B"), AnastasisHydrologyDressing::Build(S, nullptr, B, Error));
	TestEqual(TEXT("flow verts"), A.Flow.Vertices.Num(), B.Flow.Vertices.Num());
	TestEqual(TEXT("bank verts"), A.Bank.Vertices.Num(), B.Bank.Vertices.Num());
	TestEqual(TEXT("prop verts"), A.Props.Vertices.Num(), B.Props.Vertices.Num());
	TestEqual(TEXT("rocks"), A.RockCount, B.RockCount);
	TestEqual(TEXT("reeds"), A.ReedCount, B.ReedCount);
	TestTrue(TEXT("torrent centroid"), A.Torrent.Centroid.Equals(B.Torrent.Centroid, 1.e-6));
	TestTrue(TEXT("valley centroid"), A.Valley.Centroid.Equals(B.Valley.Centroid, 1.e-6));
	TestTrue(TEXT("inflow centroid"), A.Inflow.Centroid.Equals(B.Inflow.Centroid, 1.e-6));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisHydrologyDoesNotMutate,
	"Anastasis.Hydrology.DoesNotMutateSnapshot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnastasisHydrologyDoesNotMutate::RunTest(const FString&)
{
	auto S = AnastasisWorldView::CaptureCanonicalWorld(12345u);
	const auto Before = S.Tiles;
	AnastasisHydrologyDressing::FPlan Plan;
	FString Error;
	TestTrue(TEXT("build"), AnastasisHydrologyDressing::Build(S, nullptr, Plan, Error));
	TestEqual(TEXT("tile count unchanged"), S.Tiles.Num(), Before.Num());
	for (int32 I = 0; I < S.Tiles.Num(); ++I)
	{
		const auto& A = S.Tiles[I];
		const auto& B = Before[I];
		if (A.Type != B.Type || A.Alt != B.Alt || A.FlowAmt != B.FlowAmt || A.FlowX != B.FlowX
			|| A.FlowZ != B.FlowZ || A.Shore != B.Shore || A.Wetness != B.Wetness || A.Shade != B.Shade)
		{
			AddError(FString::Printf(TEXT("snapshot mutated at %d"), I));
			return false;
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisHydrologyGeometryContract,
	"Anastasis.Hydrology.GeometryContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnastasisHydrologyGeometryContract::RunTest(const FString&)
{
	const auto S = AnastasisWorldView::CaptureCanonicalWorld(12345u);
	AnastasisHydrologyDressing::FPlan Plan;
	FString Error;
	TestTrue(TEXT("build"), AnastasisHydrologyDressing::Build(S, nullptr, Plan, Error));
	const double SeaZ = AnastasisTerrainSurface::WaterPlaneZ;
	TestEqual(TEXT("flow tris multiple of 3"), Plan.Flow.Triangles.Num() % 3, 0);
	TestEqual(TEXT("bank tris multiple of 3"), Plan.Bank.Triangles.Num() % 3, 0);
	TestEqual(TEXT("prop tris multiple of 3"), Plan.Props.Triangles.Num() % 3, 0);
	TestEqual(TEXT("flow colors"), Plan.Flow.Colors.Num(), Plan.Flow.Vertices.Num());
	TestEqual(TEXT("flow snap flags"), Plan.Flow.SnapToGround.Num(), Plan.Flow.Vertices.Num());
	double MaxFlowZError = 0.0;
	int32 FlowSnapped = 0;
	for (int32 I = 0; I < Plan.Flow.Vertices.Num(); ++I)
	{
		const FVector& V = Plan.Flow.Vertices[I];
		TestTrue(TEXT("flow finite"), FMath::IsFinite(V.X) && FMath::IsFinite(V.Y) && FMath::IsFinite(V.Z));
		MaxFlowZError = FMath::Max(MaxFlowZError, FMath::Abs(V.Z - SeaZ));
		if (Plan.Flow.SnapToGround[I])
		{
			++FlowSnapped;
		}
		TestTrue(TEXT("flow is specular water"), Plan.Flow.Colors[I].A > 0.5f);
	}
	TestTrue(TEXT("flow stays on the sea plane"), MaxFlowZError <= 16.0);
	TestEqual(TEXT("flow is not snapped to land"), FlowSnapped, 0);

	int32 BankOnLand = 0;
	int32 BankSampled = 0;
	for (int32 I = 0; I < Plan.Bank.Vertices.Num(); ++I)
	{
		const FVector& V = Plan.Bank.Vertices[I];
		TestTrue(TEXT("bank finite"), FMath::IsFinite(V.X) && FMath::IsFinite(V.Y) && FMath::IsFinite(V.Z));
		TestTrue(TEXT("bank is matte earth"), Plan.Bank.Colors[I].A < 0.5f);
		if (Plan.Bank.SnapToGround[I])
		{
			++BankOnLand;
			double GroundZ = 0.0;
			if (AnastasisTerrainSurface::SampleHeight(S, V.X, V.Y, GroundZ))
			{
				++BankSampled;
			}
		}
	}
	TestTrue(TEXT("wet banks have a land side"), BankOnLand > 0);
	TestTrue(TEXT("interior wet banks sit on coarse ground"), BankSampled * 2 >= BankOnLand);

	auto Crop = AnastasisWorldView::CropSnapshot(S, 0, 0, 32, 32);
	AnastasisHydrologyDressing::FPlan Cropped;
	TestTrue(TEXT("cropped plan"), AnastasisHydrologyDressing::Build(S, &Crop, Cropped, Error));
	TestTrue(TEXT("crop emits less or equal geometry"), Cropped.Flow.Vertices.Num() <= Plan.Flow.Vertices.Num());
	TestTrue(TEXT("crop still classifies the world"), Cropped.FlowingTiles == Plan.FlowingTiles);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisHydrologyRejectsInvalid,
	"Anastasis.Hydrology.RejectsInvalid",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnastasisHydrologyRejectsInvalid::RunTest(const FString&)
{
	AnastasisHydrologyDressing::FPlan Plan;
	FString Error;
	AnastasisWorldView::FWorldVisualSnapshot Empty;
	TestFalse(TEXT("empty snapshot refused"), AnastasisHydrologyDressing::Build(Empty, nullptr, Plan, Error));
	TestTrue(TEXT("empty reports error"), !Error.IsEmpty());
	TestEqual(TEXT("empty emits no flow"), Plan.Flow.Vertices.Num(), 0);

	auto S = AnastasisWorldView::CaptureCanonicalWorld(12345u);
	S.Tiles[0].FlowAmt = -1.0;
	Error.Reset();
	TestFalse(TEXT("negative flow refused"), AnastasisHydrologyDressing::Build(S, nullptr, Plan, Error));
	TestEqual(TEXT("invalid emits no flow"), Plan.Flow.Vertices.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisHydrologyUsesSimFlow,
	"Anastasis.Hydrology.UsesSimFlowGate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnastasisHydrologyUsesSimFlow::RunTest(const FString&)
{
	const auto S = AnastasisWorldView::CaptureCanonicalWorld(12345u);
	int32 SimFlowing = 0;
	int32 SimStill = 0;
	for (const auto& T : S.Tiles)
	{
		if (T.Type != AnastasisWorld::ETileType::Water)
		{
			continue;
		}
		if (T.FlowAmt >= AnastasisHydrology::WaterFlowAmtGate)
		{
			++SimFlowing;
		}
		else
		{
			++SimStill;
		}
	}
	AnastasisHydrologyDressing::FPlan Plan;
	FString Error;
	TestTrue(TEXT("build"), AnastasisHydrologyDressing::Build(S, nullptr, Plan, Error));
	TestEqual(TEXT("flowing tiles are exactly the sim gate"), Plan.FlowingTiles, SimFlowing);
	TestEqual(TEXT("still tiles are exactly the sim remainder"), Plan.StillTiles, SimStill);
	TestEqual(TEXT("archetypes partition flowing water"),
		Plan.TorrentTiles + Plan.ValleyTiles + Plan.InflowTiles, Plan.FlowingTiles);
	return true;
}

#endif
