#include "Misc/AutomationTest.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "WorldView/AnastasisVisualMode.h"
#include "WorldView/AnastasisWorldDebugVisual.h"
#include "WorldView/AnastasisWorldEmbodiment.h"
#include "WorldView/AnastasisWorldView.h"
#include "World/AnastasisWorld.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr uint32 kRefSeed = AnastasisWorldView::ReferenceSeed;
	constexpr int32 kRefW = AnastasisWorldView::ReferenceWidth;
	constexpr int32 kRefH = AnastasisWorldView::ReferenceHeight;

	// Copie documentaire des comptes Parite.Monde seed 12345 96x96 — pas un nouveau vecteur.
	constexpr uint32 kRefCounts[AnastasisWorld::TileTypeCount] = { 3249u, 1198u, 1396u, 462u, 716u, 725u, 1470u };
	constexpr uint32 kRefOriginType = 1u;

	constexpr int32 kSampleXY[][2] = {
		{0, 0}, {1, 1}, {3, 7}, {10, 20}, {15, 15}, {31, 31}, {30, 4}, {8, 29}, {95, 95}
	};

	UWorld* FindSpawnWorld()
	{
		if (!GEngine)
		{
			return nullptr;
		}

		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			UWorld* World = Context.World();
			if (World && (World->WorldType == EWorldType::Editor || World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE))
			{
				return World;
			}
		}
		return nullptr;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisWorldViewCoordinatesTest,
	"Anastasis.WorldView.Coordinates",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisWorldViewCoordinatesTest::RunTest(const FString&)
{
	const FVector Origin = AnastasisWorldView::TileToUnreal(0, 0, 0.0);
	TestEqual(TEXT("origin X"), Origin.X, AnastasisWorldView::TileWorldSize * 0.5);
	TestEqual(TEXT("origin Y"), Origin.Y, AnastasisWorldView::TileWorldSize * 0.5);
	TestEqual(TEXT("origin Z"), Origin.Z, 0.0);

	const FVector Sea = AnastasisWorldView::TileToUnreal(0, 0, AnastasisWorld::SeaLevel);
	TestEqual(TEXT("sea Z"), Sea.Z, AnastasisWorld::SeaLevel * AnastasisWorldView::AltitudeScale);

	const FVector Far = AnastasisWorldView::TileToUnreal(95, 95, 1.0);
	TestEqual(TEXT("far X"), Far.X, 95.5 * AnastasisWorldView::TileWorldSize);
	TestEqual(TEXT("far Y"), Far.Y, 95.5 * AnastasisWorldView::TileWorldSize);
	TestEqual(TEXT("far Z"), Far.Z, AnastasisWorldView::AltitudeScale);

	TestTrue(TEXT("axes SimX->UEX SimY->UEY"), Far.X > Origin.X && Far.Y > Origin.Y);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisWorldViewInstanceCountTest,
	"Anastasis.WorldView.InstanceCount",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisWorldViewInstanceCountTest::RunTest(const FString&)
{
	const AnastasisWorld::FWorld World = AnastasisWorld::GenerateWorld(kRefSeed, kRefW, kRefH);
	const AnastasisWorldView::FPlan Plan = AnastasisWorldView::BuildPlan(kRefSeed, World);

	TestEqual(TEXT("tile count 96x96"), Plan.TileCount, kRefW * kRefH);
	TestEqual(TEXT("location count"), Plan.Locations.Num(), Plan.TileCount);
	TestEqual(TEXT("W"), Plan.W, kRefW);
	TestEqual(TEXT("H"), Plan.H, kRefH);
	TestEqual(TEXT("origin type"), static_cast<uint32>(World.Tiles[0].Type), kRefOriginType);

	int32 Sum = 0;
	for (int32 Type = 0; Type < AnastasisWorld::TileTypeCount; ++Type)
	{
		TestEqual(
			*FString::Printf(TEXT("terrain count %d"), Type),
			static_cast<uint32>(Plan.TerrainCounts[Type]),
			kRefCounts[Type]);
		Sum += Plan.TerrainCounts[Type];
	}
	TestEqual(TEXT("terrain counts sum"), Sum, Plan.TileCount);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisWorldViewSampleMappingTest,
	"Anastasis.WorldView.SampleMapping",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisWorldViewSampleMappingTest::RunTest(const FString&)
{
	const AnastasisWorld::FWorld World = AnastasisWorld::GenerateWorld(kRefSeed, kRefW, kRefH);
	const AnastasisWorldView::FPlan Plan = AnastasisWorldView::BuildPlan(kRefSeed, World);

	for (const int32* XY : kSampleXY)
	{
		const int32 X = XY[0];
		const int32 Y = XY[1];
		const AnastasisWorld::FTile& Tile = World.Tiles[Y * kRefW + X];
		const AnastasisWorldView::FTilePose Pose = AnastasisWorldView::SamplePose(Plan, X, Y);
		const FVector Expected = AnastasisWorldView::TileToUnreal(Tile.X, Tile.Y, Tile.Alt);

		TestEqual(*FString::Printf(TEXT("sample (%d,%d) index"), X, Y), Pose.Index, Y * kRefW + X);
		TestEqual(*FString::Printf(TEXT("sample (%d,%d) type"), X, Y), static_cast<uint32>(Pose.Type), static_cast<uint32>(Tile.Type));
		TestEqual(*FString::Printf(TEXT("sample (%d,%d) X"), X, Y), Pose.UnrealLocation.X, Expected.X);
		TestEqual(*FString::Printf(TEXT("sample (%d,%d) Y"), X, Y), Pose.UnrealLocation.Y, Expected.Y);
		TestEqual(*FString::Printf(TEXT("sample (%d,%d) Z"), X, Y), Pose.UnrealLocation.Z, Expected.Z);
		TestTrue(*FString::Printf(TEXT("sample (%d,%d) alt drives Z"), X, Y), FMath::IsNearlyEqual(Pose.UnrealLocation.Z, Tile.Alt * AnastasisWorldView::AltitudeScale));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisWorldViewBoundsTest,
	"Anastasis.WorldView.Bounds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisWorldViewBoundsTest::RunTest(const FString&)
{
	const AnastasisWorld::FWorld World = AnastasisWorld::GenerateWorld(kRefSeed, kRefW, kRefH);
	const AnastasisWorldView::FPlan Plan = AnastasisWorldView::BuildPlan(kRefSeed, World);
	const FBox Bounds = AnastasisWorldView::PlanBounds(Plan);

	const double Extent = AnastasisWorldView::TileWorldSize * kRefW;
	TestTrue(TEXT("min X"), FMath::IsNearlyEqual(Bounds.Min.X, 0.0, 0.01));
	TestTrue(TEXT("min Y"), FMath::IsNearlyEqual(Bounds.Min.Y, 0.0, 0.01));
	TestTrue(TEXT("max X"), FMath::IsNearlyEqual(Bounds.Max.X, Extent, 0.01));
	TestTrue(TEXT("max Y"), FMath::IsNearlyEqual(Bounds.Max.Y, Extent, 0.01));
	TestTrue(TEXT("Z spans altitude"), Bounds.Max.Z > Bounds.Min.Z);
	TestTrue(
		TEXT("max Z matches maxAlt"),
		FMath::IsNearlyEqual(Bounds.Max.Z, Plan.MaxAlt * AnastasisWorldView::AltitudeScale, 0.01));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisWorldViewEmbodimentSpawnTest,
	"Anastasis.WorldView.EmbodimentSpawn",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisWorldViewEmbodimentSpawnTest::RunTest(const FString&)
{
	UWorld* World = FindSpawnWorld();
	if (!World)
	{
		AddError(TEXT("OBSERVABILITY_BLOCKED: no Editor/Game/PIE UWorld to spawn embodiment"));
		return false;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.ObjectFlags |= RF_Transient;
	AAnastasisWorldEmbodiment* Actor = World->SpawnActor<AAnastasisWorldEmbodiment>(
		AAnastasisWorldEmbodiment::StaticClass(),
		FVector::ZeroVector,
		FRotator::ZeroRotator,
		Params);
	if (!Actor)
	{
		AddError(TEXT("Failed to spawn AAnastasisWorldEmbodiment"));
		return false;
	}

	const bool bEmbodied = Actor->Embody(kRefSeed, kRefW, kRefH);
	TestTrue(TEXT("embody returned true"), bEmbodied);
	TestEqual(TEXT("HISMC instance count"), Actor->GetInstanceCount(), kRefW * kRefH);
	TestEqual(TEXT("plan tile count"), Actor->GetPlan().TileCount, kRefW * kRefH);

	for (int32 Type = 0; Type < AnastasisWorld::TileTypeCount; ++Type)
	{
		TestTrue(
			*FString::Printf(TEXT("terrain class %d represented"), Type),
			Actor->GetPlan().TerrainCounts[Type] > 0);
	}

	const AnastasisWorldView::FTilePose Origin = AnastasisWorldView::SamplePose(Actor->GetPlan(), 0, 0);
	FTransform InstanceTransform;
	TestTrue(TEXT("origin instance readable"), Actor->GetInstanceWorldTransform(Origin.Index, InstanceTransform));
	TestTrue(TEXT("origin instance X"), FMath::IsNearlyEqual(InstanceTransform.GetLocation().X, Origin.UnrealLocation.X, 0.01));
	TestTrue(TEXT("origin instance Y"), FMath::IsNearlyEqual(InstanceTransform.GetLocation().Y, Origin.UnrealLocation.Y, 0.01));
	TestTrue(TEXT("origin instance Z"), FMath::IsNearlyEqual(InstanceTransform.GetLocation().Z, Origin.UnrealLocation.Z, 0.01));

	const AnastasisWorldView::FTilePose Far = AnastasisWorldView::SamplePose(Actor->GetPlan(), 95, 95);
	TestTrue(TEXT("far instance readable"), Actor->GetInstanceWorldTransform(Far.Index, InstanceTransform));
	TestTrue(TEXT("far instance Z uses alt"), FMath::IsNearlyEqual(InstanceTransform.GetLocation().Z, Far.Alt * AnastasisWorldView::AltitudeScale, 0.01));
	TestTrue(TEXT("distinct terrain Z or type"), Origin.Type != Far.Type || !FMath::IsNearlyEqual(Origin.Alt, Far.Alt));

	Actor->Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisWorldViewVisualSnapshotFieldsTest,
	"Anastasis.WorldView.VisualSnapshotFields",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisWorldViewVisualSnapshotFieldsTest::RunTest(const FString&)
{
	const AnastasisWorld::FWorld World = AnastasisWorld::GenerateWorld(kRefSeed, kRefW, kRefH);
	const AnastasisWorldView::FWorldVisualSnapshot Snapshot = AnastasisWorldView::CaptureSnapshot(kRefSeed, World);

	TestEqual(TEXT("snapshot seed"), Snapshot.Seed, kRefSeed);
	TestEqual(TEXT("snapshot source W"), Snapshot.SourceW, kRefW);
	TestEqual(TEXT("snapshot source H"), Snapshot.SourceH, kRefH);
	TestEqual(TEXT("snapshot W"), Snapshot.W, kRefW);
	TestEqual(TEXT("snapshot tile count"), Snapshot.Tiles.Num(), kRefW * kRefH);

	for (const int32* XY : kSampleXY)
	{
		const int32 X = XY[0];
		const int32 Y = XY[1];
		const AnastasisWorld::FTile& Tile = World.Tiles[Y * kRefW + X];
		const AnastasisWorldView::FVisualTile* Visual = AnastasisWorldView::FindTile(Snapshot, X, Y);
		if (!Visual)
		{
			AddError(*FString::Printf(TEXT("missing visual tile (%d,%d)"), X, Y));
			continue;
		}

		TestEqual(*FString::Printf(TEXT("(%d,%d) source index"), X, Y), Visual->SourceIndex, Y * kRefW + X);
		TestEqual(*FString::Printf(TEXT("(%d,%d) type"), X, Y), static_cast<uint32>(Visual->Type), static_cast<uint32>(Tile.Type));
		TestEqual(*FString::Printf(TEXT("(%d,%d) resource"), X, Y), static_cast<uint32>(Visual->Resource), static_cast<uint32>(Tile.Resource));
		TestEqual(*FString::Printf(TEXT("(%d,%d) amount"), X, Y), Visual->Amount, Tile.Amount);
		TestEqual(*FString::Printf(TEXT("(%d,%d) alt"), X, Y), Visual->Alt, Tile.Alt);
		TestEqual(*FString::Printf(TEXT("(%d,%d) shade"), X, Y), Visual->Shade, Tile.Shade);
		TestEqual(*FString::Printf(TEXT("(%d,%d) shore"), X, Y), Visual->Shore, Tile.Shore);
		TestEqual(*FString::Printf(TEXT("(%d,%d) wetness"), X, Y), Visual->Wetness, Tile.Wetness);
		TestEqual(*FString::Printf(TEXT("(%d,%d) flowX"), X, Y), Visual->FlowX, Tile.FlowX);
		TestEqual(*FString::Printf(TEXT("(%d,%d) flowZ"), X, Y), Visual->FlowZ, Tile.FlowZ);
		TestEqual(*FString::Printf(TEXT("(%d,%d) flowAmt"), X, Y), Visual->FlowAmt, Tile.FlowAmt);
		TestEqual(*FString::Printf(TEXT("(%d,%d) crop"), X, Y), static_cast<uint32>(Visual->CropId), static_cast<uint32>(Tile.CropId));
		TestEqual(*FString::Printf(TEXT("(%d,%d) fertility"), X, Y), Visual->Fertility, Tile.Fertility);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisWorldViewCanonicalCropParityTest,
	"Anastasis.WorldView.CanonicalCropParity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisWorldViewCanonicalCropParityTest::RunTest(const FString&)
{
	const AnastasisWorldView::FWorldVisualSnapshot Canonical = AnastasisWorldView::CaptureCanonicalWorld(kRefSeed);
	TestEqual(TEXT("canonical generate is 96x96"), Canonical.W, kRefW);
	TestEqual(TEXT("canonical source is 96"), Canonical.SourceW, kRefW);
	TestEqual(TEXT("canonical tiles"), Canonical.Tiles.Num(), kRefW * kRefH);

	const AnastasisWorldView::FWorldVisualSnapshot Crop = AnastasisWorldView::CropSnapshot(
		Canonical,
		AnastasisWorldView::CanonicalCropOriginX,
		AnastasisWorldView::CanonicalCropOriginY,
		AnastasisWorldView::CanonicalCropWidth,
		AnastasisWorldView::CanonicalCropHeight);

	TestEqual(TEXT("crop keeps source W"), Crop.SourceW, kRefW);
	TestEqual(TEXT("crop keeps source H"), Crop.SourceH, kRefH);
	TestEqual(TEXT("crop W"), Crop.W, AnastasisWorldView::CanonicalCropWidth);
	TestEqual(TEXT("crop H"), Crop.H, AnastasisWorldView::CanonicalCropHeight);
	TestEqual(TEXT("crop origin X"), Crop.OriginX, 0);
	TestEqual(TEXT("crop origin Y"), Crop.OriginY, 0);
	TestEqual(TEXT("crop tile count"), Crop.Tiles.Num(), 32 * 32);

	constexpr int32 kCropSamples[][2] = {
		{0, 0}, {1, 1}, {3, 7}, {10, 20}, {15, 15}, {31, 31}, {30, 4}, {8, 29}
	};

	for (const int32* XY : kCropSamples)
	{
		const int32 X = XY[0];
		const int32 Y = XY[1];
		const AnastasisWorldView::FVisualTile* SourceTile = AnastasisWorldView::FindTile(Canonical, X, Y);
		const AnastasisWorldView::FVisualTile* CropTile = AnastasisWorldView::FindTile(Crop, X, Y);
		if (!SourceTile || !CropTile)
		{
			AddError(*FString::Printf(TEXT("crop missing (%d,%d)"), X, Y));
			continue;
		}

		TestEqual(*FString::Printf(TEXT("crop (%d,%d) source index"), X, Y), CropTile->SourceIndex, Y * kRefW + X);
		TestEqual(*FString::Printf(TEXT("crop (%d,%d) type"), X, Y), static_cast<uint32>(CropTile->Type), static_cast<uint32>(SourceTile->Type));
		TestEqual(*FString::Printf(TEXT("crop (%d,%d) alt"), X, Y), CropTile->Alt, SourceTile->Alt);
		TestEqual(*FString::Printf(TEXT("crop (%d,%d) shade"), X, Y), CropTile->Shade, SourceTile->Shade);
		TestEqual(*FString::Printf(TEXT("crop (%d,%d) shore"), X, Y), CropTile->Shore, SourceTile->Shore);
		TestEqual(*FString::Printf(TEXT("crop (%d,%d) wetness"), X, Y), CropTile->Wetness, SourceTile->Wetness);
		TestEqual(*FString::Printf(TEXT("crop (%d,%d) flowAmt"), X, Y), CropTile->FlowAmt, SourceTile->FlowAmt);
		TestEqual(*FString::Printf(TEXT("crop (%d,%d) cropId"), X, Y), static_cast<uint32>(CropTile->CropId), static_cast<uint32>(SourceTile->CropId));
		TestEqual(*FString::Printf(TEXT("crop (%d,%d) resource"), X, Y), static_cast<uint32>(CropTile->Resource), static_cast<uint32>(SourceTile->Resource));
		TestEqual(*FString::Printf(TEXT("crop (%d,%d) fertility"), X, Y), CropTile->Fertility, SourceTile->Fertility);

		const AnastasisWorldView::FTilePose Pose = AnastasisWorldView::SamplePose(AnastasisWorldView::BuildPlan(Crop), X, Y);
		TestEqual(*FString::Printf(TEXT("crop (%d,%d) UE X"), X, Y), Pose.UnrealLocation.X, (X + 0.5) * AnastasisWorldView::TileWorldSize);
		TestEqual(*FString::Printf(TEXT("crop (%d,%d) UE Z"), X, Y), Pose.UnrealLocation.Z, CropTile->Alt * AnastasisWorldView::AltitudeScale);
	}

	TestTrue(TEXT("far corner of 96 is outside crop"), AnastasisWorldView::FindTile(Crop, 95, 95) == nullptr);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisWorldViewCropIsNotSmallGenerateTest,
	"Anastasis.WorldView.CropIsNotSmallGenerate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisWorldViewCropIsNotSmallGenerateTest::RunTest(const FString&)
{
	const AnastasisWorldView::FWorldVisualSnapshot Crop = AnastasisWorldView::CropSnapshot(
		AnastasisWorldView::CaptureCanonicalWorld(kRefSeed),
		0,
		0,
		32,
		32);
	const AnastasisWorld::FWorld Small = AnastasisWorld::GenerateWorld(kRefSeed, 32, 32);
	const AnastasisWorldView::FWorldVisualSnapshot SmallSnap = AnastasisWorldView::CaptureSnapshot(kRefSeed, Small);

	TestEqual(TEXT("small world is 32x32"), SmallSnap.W, 32);
	TestEqual(TEXT("small source W is 32, not 96"), SmallSnap.SourceW, 32);
	TestEqual(TEXT("crop source W remains 96"), Crop.SourceW, 96);

	int32 Mismatches = 0;
	for (int32 Y = 0; Y < 32; ++Y)
	{
		for (int32 X = 0; X < 32; ++X)
		{
			const AnastasisWorldView::FVisualTile& CropTile = Crop.Tiles[Y * 32 + X];
			const AnastasisWorldView::FVisualTile& SmallTile = SmallSnap.Tiles[Y * 32 + X];
			if (CropTile.Type != SmallTile.Type || CropTile.Alt != SmallTile.Alt)
			{
				++Mismatches;
			}
		}
	}

	TestTrue(TEXT("GenerateWorld(12345,32,32) differs from crop32(GenerateWorld(12345,96,96))"), Mismatches > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisVisualModeDefaultTest,
	"Anastasis.Visual.ModeDefault",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisVisualModeDefaultTest::RunTest(const FString&)
{
	TestEqual(TEXT("default visual mode is DEBUG"), static_cast<uint8>(AnastasisVisualMode::Get()), static_cast<uint8>(EAnastasisVisualMode::Debug));
	TestEqual(TEXT("DEBUG name"), FString(AnastasisVisualMode::Name(EAnastasisVisualMode::Debug)), FString(TEXT("DEBUG")));
	TestEqual(TEXT("PLAYER name"), FString(AnastasisVisualMode::Name(EAnastasisVisualMode::Player)), FString(TEXT("PLAYER")));
	TestEqual(TEXT("NONE name"), FString(AnastasisVisualMode::Name(EAnastasisVisualMode::None)), FString(TEXT("NONE")));
	TestEqual(TEXT("debug slab scale preserved"), AnastasisWorldDebugVisual::TileSlabScaleZ, 0.2);
	TestEqual(TEXT("debug cube mesh size preserved"), AnastasisWorldDebugVisual::CubeMeshSize, 100.0);
	return true;
}

#endif
