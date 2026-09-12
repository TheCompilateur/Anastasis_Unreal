#include "Misc/AutomationTest.h"

#include "Dom/JsonObject.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
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

#endif
