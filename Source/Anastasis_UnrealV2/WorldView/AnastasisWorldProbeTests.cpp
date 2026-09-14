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

	bool IsVerifierStatus(const FString& Status)
	{
		return Status == TEXT("PASS") || Status == TEXT("FAIL") || Status == TEXT("UNKNOWN");
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisWorldProbePhase2VerifiersTest,
	"Anastasis.WorldProbe.Phase2Verifiers",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisWorldProbePhase2VerifiersTest::RunTest(const FString&)
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
	Paths.Add(Probe->VerifyWorldContract());
	Paths.Add(Probe->VerifySettlementContract(TEXT("canonical")));
	Paths.Add(Probe->VerifyNavigationContract());
	Paths.Add(Probe->VerifyVisualDelivery());
	Paths.Add(Probe->VerifySemanticSlice(0, 0, 32));

	for (const FString& Path : Paths)
	{
		TestTrue(TEXT("verification path non-empty"), !Path.IsEmpty());
		TestTrue(TEXT("verification file exists"), FPaths::FileExists(Path));
	}

	const TSharedPtr<FJsonObject> WorldVerify = LoadJsonObjectFromPath(Paths[0]);
	if (TestTrue(TEXT("world verification parses"), WorldVerify.IsValid()))
	{
		TestEqual(TEXT("world verifier schema"), WorldVerify->GetStringField(TEXT("schema")), FString(TEXT("anastasis.verify_world_contract.v1")));
		TestEqual(TEXT("world verifier passes canonical embodiment"), WorldVerify->GetStringField(TEXT("status")), FString(TEXT("PASS")));
	}

	const TSharedPtr<FJsonObject> SettlementVerify = LoadJsonObjectFromPath(Paths[1]);
	if (TestTrue(TEXT("settlement verification parses"), SettlementVerify.IsValid()))
	{
		TestEqual(TEXT("settlement verifier schema"), SettlementVerify->GetStringField(TEXT("schema")), FString(TEXT("anastasis.verify_settlement_contract.v1")));
		TestEqual(TEXT("settlement verifier stays honest"), SettlementVerify->GetStringField(TEXT("status")), FString(TEXT("UNKNOWN")));
	}

	const TSharedPtr<FJsonObject> NavigationVerify = LoadJsonObjectFromPath(Paths[2]);
	if (TestTrue(TEXT("navigation verification parses"), NavigationVerify.IsValid()))
	{
		TestEqual(TEXT("navigation verifier schema"), NavigationVerify->GetStringField(TEXT("schema")), FString(TEXT("anastasis.verify_navigation_contract.v1")));
		TestTrue(TEXT("navigation verifier status domain"), IsVerifierStatus(NavigationVerify->GetStringField(TEXT("status"))));
	}

	const TSharedPtr<FJsonObject> VisualVerify = LoadJsonObjectFromPath(Paths[3]);
	if (TestTrue(TEXT("visual verification parses"), VisualVerify.IsValid()))
	{
		TestEqual(TEXT("visual verifier schema"), VisualVerify->GetStringField(TEXT("schema")), FString(TEXT("anastasis.verify_visual_delivery.v1")));
		TestTrue(TEXT("visual verifier status domain"), IsVerifierStatus(VisualVerify->GetStringField(TEXT("status"))));
		TestTrue(TEXT("visual verifier has checks"), VisualVerify->HasTypedField<EJson::Array>(TEXT("checks")));
	}

	const TSharedPtr<FJsonObject> SliceVerify = LoadJsonObjectFromPath(Paths[4]);
	if (TestTrue(TEXT("semantic slice verification parses"), SliceVerify.IsValid()))
	{
		TestEqual(TEXT("slice verifier schema"), SliceVerify->GetStringField(TEXT("schema")), FString(TEXT("anastasis.verify_semantic_slice.v1")));
		TestTrue(TEXT("slice verifier status domain"), IsVerifierStatus(SliceVerify->GetStringField(TEXT("status"))));
		TestTrue(TEXT("slice verifier has metrics"), SliceVerify->HasTypedField<EJson::Object>(TEXT("metrics")));
	}

	for (const FString& Path : Paths)
	{
		IFileManager::Get().Delete(*Path);
	}
	Embodiment->Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisWorldProbePhase3ProbesTest,
	"Anastasis.WorldProbe.Phase3Probes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisWorldProbePhase3ProbesTest::RunTest(const FString&)
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
	Paths.Add(Probe->RunWorldgenProbe(12345u, 96, 96, 12.0, TEXT("canonical")));
	Paths.Add(Probe->FindSemanticSlice(12345u, 32, TEXT("canonical")));
	Paths.Add(Probe->CompareSliceCandidates(12345u, 32, TEXT("0,0;24,12;48,48"), TEXT("canonical")));
	Paths.Add(Probe->CaptureFixedViewProbe(12345u, 0, 0, 32, 12.0, TEXT("OVERVIEW"), TEXT("canonical")));

	for (const FString& Path : Paths)
	{
		TestTrue(TEXT("probe path non-empty"), !Path.IsEmpty());
		TestTrue(TEXT("probe file exists"), FPaths::FileExists(Path));
	}

	const TSharedPtr<FJsonObject> WorldgenProbe = LoadJsonObjectFromPath(Paths[0]);
	if (TestTrue(TEXT("worldgen probe parses"), WorldgenProbe.IsValid()))
	{
		TestEqual(TEXT("worldgen probe schema"), WorldgenProbe->GetStringField(TEXT("schema")), FString(TEXT("anastasis.probe_worldgen.v1")));
		TestEqual(TEXT("worldgen probe status"), WorldgenProbe->GetStringField(TEXT("status")), FString(TEXT("PASS")));
		TestTrue(TEXT("worldgen probe has metrics"), WorldgenProbe->HasTypedField<EJson::Object>(TEXT("metrics")));
	}

	const TSharedPtr<FJsonObject> FindProbe = LoadJsonObjectFromPath(Paths[1]);
	if (TestTrue(TEXT("find semantic slice probe parses"), FindProbe.IsValid()))
	{
		TestEqual(TEXT("find slice schema"), FindProbe->GetStringField(TEXT("schema")), FString(TEXT("anastasis.probe_find_semantic_slice.v1")));
		TestEqual(TEXT("find slice status"), FindProbe->GetStringField(TEXT("status")), FString(TEXT("PASS")));
		TestTrue(TEXT("find slice has best candidate"), FindProbe->HasTypedField<EJson::Object>(TEXT("best_candidate")));
		TestTrue(TEXT("find slice has top candidates"), FindProbe->HasTypedField<EJson::Array>(TEXT("top_candidates")));
	}

	const TSharedPtr<FJsonObject> CompareProbe = LoadJsonObjectFromPath(Paths[2]);
	if (TestTrue(TEXT("compare slice probe parses"), CompareProbe.IsValid()))
	{
		TestEqual(TEXT("compare slice schema"), CompareProbe->GetStringField(TEXT("schema")), FString(TEXT("anastasis.probe_compare_slice_candidates.v1")));
		TestEqual(TEXT("compare slice status"), CompareProbe->GetStringField(TEXT("status")), FString(TEXT("PASS")));
		TestTrue(TEXT("compare slice has winner"), CompareProbe->HasTypedField<EJson::Object>(TEXT("winner")));
		TestTrue(TEXT("compare slice has ranked candidates"), CompareProbe->HasTypedField<EJson::Array>(TEXT("ranked_candidates")));
	}

	const TSharedPtr<FJsonObject> CaptureProbe = LoadJsonObjectFromPath(Paths[3]);
	if (TestTrue(TEXT("capture fixed view probe parses"), CaptureProbe.IsValid()))
	{
		TestEqual(TEXT("capture fixed view schema"), CaptureProbe->GetStringField(TEXT("schema")), FString(TEXT("anastasis.probe_capture_fixed_view.v1")));
		TestTrue(TEXT("capture fixed view status domain"), IsVerifierStatus(CaptureProbe->GetStringField(TEXT("status"))));
		TestTrue(TEXT("capture fixed view has locked params"), CaptureProbe->HasTypedField<EJson::Object>(TEXT("locked_parameters")));
		TestTrue(TEXT("capture fixed view has checks"), CaptureProbe->HasTypedField<EJson::Array>(TEXT("checks")));
	}

	for (const FString& Path : Paths)
	{
		IFileManager::Get().Delete(*Path);
	}
	Embodiment->Destroy();
	return true;
}

#endif
