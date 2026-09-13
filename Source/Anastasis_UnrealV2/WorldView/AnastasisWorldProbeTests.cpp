#include "Misc/AutomationTest.h"

#include "Dom/JsonObject.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "WorldView/AnastasisWorldEmbodiment.h"
#include "WorldView/AnastasisWorldProbeSubsystem.h"
#include "WorldView/AnastasisWorldView.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	UWorld* FindProbeAutomationWorld()
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

	AAnastasisWorldEmbodiment* SpawnCanonicalEmbodiment(UWorld* World)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AAnastasisWorldEmbodiment* Embodiment = World->SpawnActor<AAnastasisWorldEmbodiment>(
			AAnastasisWorldEmbodiment::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params);
		if (Embodiment)
		{
			Embodiment->EmbodyCanonical(static_cast<int32>(AnastasisWorldView::ReferenceSeed));
		}
		return Embodiment;
	}

	TSharedPtr<FJsonObject> LoadJsonObjectFromPath(const FString& Path)
	{
		FString Json;
		if (!FFileHelper::LoadFileToString(Json, *Path))
		{
			return nullptr;
		}

		TSharedPtr<FJsonObject> Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
		if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		{
			return nullptr;
		}
		return Root;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisWorldProbeSnapshotTest,
	"Anastasis.WorldProbe.Snapshot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisWorldProbeSnapshotTest::RunTest(const FString&)
{
	UWorld* World = FindProbeAutomationWorld();
	if (!TestNotNull(TEXT("world"), World))
	{
		return false;
	}

	UAnastasisWorldProbeSubsystem* Probe = World->GetSubsystem<UAnastasisWorldProbeSubsystem>();
	if (!TestNotNull(TEXT("probe subsystem"), Probe))
	{
		return false;
	}

	AAnastasisWorldEmbodiment* Embodiment = SpawnCanonicalEmbodiment(World);
	if (!TestNotNull(TEXT("embodiment"), Embodiment))
	{
		return false;
	}

	const FString Path = Probe->WriteSnapshot();
	TestTrue(TEXT("snapshot path non-empty"), !Path.IsEmpty());
	if (Path.IsEmpty())
	{
		Embodiment->Destroy();
		return false;
	}

	FString Json;
	const bool bRead = FFileHelper::LoadFileToString(Json, *Path);
	TestTrue(TEXT("snapshot file readable"), bRead);

	if (bRead)
	{
		TSharedPtr<FJsonObject> Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
		const bool bParsed = FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid();
		TestTrue(TEXT("snapshot parses as JSON"), bParsed);

		if (bParsed)
		{
			TestTrue(TEXT("has schema field"), Root->HasField(TEXT("schema")));
			TestTrue(TEXT("has actor_count field"), Root->HasField(TEXT("actor_count")));
			TestTrue(TEXT("has terrain field"), Root->HasField(TEXT("terrain")));
			TestTrue(TEXT("has sanity field"), Root->HasField(TEXT("sanity")));

			const TSharedPtr<FJsonObject>* TerrainObj = nullptr;
			if (TestTrue(TEXT("terrain is an object"), Root->TryGetObjectField(TEXT("terrain"), TerrainObj)))
			{
				bool bPresent = false;
				(*TerrainObj)->TryGetBoolField(TEXT("present"), bPresent);
				TestTrue(TEXT("terrain reported present"), bPresent);

				int32 TileCount = 0;
				(*TerrainObj)->TryGetNumberField(TEXT("tile_count"), TileCount);
				TestEqual(TEXT("tile_count matches canonical 96x96"), TileCount, AnastasisWorldView::ReferenceWidth * AnastasisWorldView::ReferenceHeight);
			}

			const TSharedPtr<FJsonObject>* SanityObj = nullptr;
			if (TestTrue(TEXT("sanity is an object"), Root->TryGetObjectField(TEXT("sanity"), SanityObj)))
			{
				bool bNan = true;
				(*SanityObj)->TryGetBoolField(TEXT("nan_or_invalid_coordinates"), bNan);
				TestFalse(TEXT("no NaN in canonical embodiment"), bNan);
			}
		}
	}

	IFileManager::Get().Delete(*Path);
	Embodiment->Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisWorldProbeBookmarkTest,
	"Anastasis.WorldProbe.Bookmarks",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisWorldProbeBookmarkTest::RunTest(const FString&)
{
	UWorld* World = FindProbeAutomationWorld();
	if (!TestNotNull(TEXT("world"), World))
	{
		return false;
	}

	UAnastasisWorldProbeSubsystem* Probe = World->GetSubsystem<UAnastasisWorldProbeSubsystem>();
	if (!TestNotNull(TEXT("probe subsystem"), Probe))
	{
		return false;
	}

	FString Reason;
	TestFalse(TEXT("unknown bookmark is not reachable"), Probe->GotoBookmark(TEXT("DOES_NOT_EXIST"), Reason));
	TestTrue(TEXT("unknown bookmark reports a reason"), !Reason.IsEmpty());

	AAnastasisWorldEmbodiment* Embodiment = SpawnCanonicalEmbodiment(World);
	if (!TestNotNull(TEXT("embodiment"), Embodiment))
	{
		return false;
	}

	// SETTLEMENT must stay honest: no building/settlement system exists yet.
	TestFalse(TEXT("SETTLEMENT is NOT_REACHABLE"), Probe->GotoBookmark(TEXT("SETTLEMENT"), Reason));
	TestTrue(TEXT("SETTLEMENT reports a reason"), !Reason.IsEmpty());

	// OVERVIEW/GROUND/SHORE become geometrically reachable once terrain exists; actually
	// moving the camera additionally needs a PlayerController, which this test context may lack.
	const bool bHasController = World->GetFirstPlayerController() != nullptr;
	const bool bOverviewMoved = Probe->GotoBookmark(TEXT("OVERVIEW"), Reason);
	TestEqual(TEXT("OVERVIEW move outcome matches PlayerController availability"), bOverviewMoved, bHasController);

	Embodiment->Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisWorldProbePhase1InspectorsTest,
	"Anastasis.WorldProbe.Phase1Inspectors",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisWorldProbePhase1InspectorsTest::RunTest(const FString&)
{
	UWorld* World = FindProbeAutomationWorld();
	if (!TestNotNull(TEXT("world"), World))
	{
		return false;
	}

	UAnastasisWorldProbeSubsystem* Probe = World->GetSubsystem<UAnastasisWorldProbeSubsystem>();
	if (!TestNotNull(TEXT("probe subsystem"), Probe))
	{
		return false;
	}

	AAnastasisWorldEmbodiment* Embodiment = SpawnCanonicalEmbodiment(World);
	if (!TestNotNull(TEXT("embodiment"), Embodiment))
	{
		return false;
	}

	TArray<FString> Paths;
	Paths.Add(Probe->InspectWorld());
	Paths.Add(Probe->InspectTile(0, 0));
	Paths.Add(Probe->InspectSettlement(TEXT("canonical")));
	Paths.Add(Probe->InspectActor(TEXT("AnastasisWorldEmbodiment")));
	Paths.Add(Probe->InspectVisualSceneState());

	for (const FString& Path : Paths)
	{
		TestTrue(TEXT("inspection path non-empty"), !Path.IsEmpty());
		TestTrue(TEXT("inspection file exists"), FPaths::FileExists(Path));
	}

	const TSharedPtr<FJsonObject> WorldInspect = LoadJsonObjectFromPath(Paths[0]);
	if (TestTrue(TEXT("world inspection parses"), WorldInspect.IsValid()))
	{
		TestEqual(TEXT("world inspect schema"), WorldInspect->GetStringField(TEXT("schema")), FString(TEXT("anastasis.inspect_world.v1")));
		TestEqual(TEXT("world inspect status"), WorldInspect->GetStringField(TEXT("status")), FString(TEXT("OBSERVED")));
	}

	const TSharedPtr<FJsonObject> TileInspect = LoadJsonObjectFromPath(Paths[1]);
	if (TestTrue(TEXT("tile inspection parses"), TileInspect.IsValid()))
	{
		TestEqual(TEXT("tile inspect schema"), TileInspect->GetStringField(TEXT("schema")), FString(TEXT("anastasis.inspect_tile.v1")));
		TestEqual(TEXT("tile inspect status"), TileInspect->GetStringField(TEXT("status")), FString(TEXT("OBSERVED")));
		TestTrue(TEXT("tile object present"), TileInspect->HasTypedField<EJson::Object>(TEXT("tile")));
	}

	const TSharedPtr<FJsonObject> SettlementInspect = LoadJsonObjectFromPath(Paths[2]);
	if (TestTrue(TEXT("settlement inspection parses"), SettlementInspect.IsValid()))
	{
		TestEqual(TEXT("settlement inspect schema"), SettlementInspect->GetStringField(TEXT("schema")), FString(TEXT("anastasis.inspect_settlement.v1")));
		TestEqual(TEXT("settlement honest status"), SettlementInspect->GetStringField(TEXT("status")), FString(TEXT("NOT_IMPLEMENTED")));
	}

	const TSharedPtr<FJsonObject> ActorInspect = LoadJsonObjectFromPath(Paths[3]);
	if (TestTrue(TEXT("actor inspection parses"), ActorInspect.IsValid()))
	{
		TestEqual(TEXT("actor inspect schema"), ActorInspect->GetStringField(TEXT("schema")), FString(TEXT("anastasis.inspect_actor.v1")));
		TestEqual(TEXT("actor inspect status"), ActorInspect->GetStringField(TEXT("status")), FString(TEXT("OBSERVED")));
		TestTrue(TEXT("actor match count > 0"), ActorInspect->GetIntegerField(TEXT("match_count")) > 0);
	}

	const TSharedPtr<FJsonObject> VisualInspect = LoadJsonObjectFromPath(Paths[4]);
	if (TestTrue(TEXT("visual scene inspection parses"), VisualInspect.IsValid()))
	{
		TestEqual(TEXT("visual inspect schema"), VisualInspect->GetStringField(TEXT("schema")), FString(TEXT("anastasis.inspect_visual_scene_state.v1")));
		TestEqual(TEXT("visual inspect status"), VisualInspect->GetStringField(TEXT("status")), FString(TEXT("OBSERVED")));
		TestTrue(TEXT("component summary present"), VisualInspect->HasTypedField<EJson::Object>(TEXT("component_summary")));
	}

	for (const FString& Path : Paths)
	{
		IFileManager::Get().Delete(*Path);
	}
	Embodiment->Destroy();
	return true;
}

#endif
