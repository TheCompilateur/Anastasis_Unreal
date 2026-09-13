#include "WorldView/AnastasisWorldProbeSubsystem.h"

#include "Anastasis_UnrealV2.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Containers/Ticker.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/WorldSettings.h"
#include "HAL/FileManager.h"
#include "Misc/App.h"
#include "Misc/DateTime.h"
#include "Misc/EngineVersion.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "NavigationSystem.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UnrealClient.h"
#include "WorldPartition/DataLayer/DataLayerManager.h"
#include "WorldPartition/WorldPartition.h"
#include "WorldView/AnastasisWorldEmbodiment.h"
#include "WorldView/AnastasisWorldView.h"

namespace
{
	TArray<TSharedPtr<FJsonValue>> Vec3Array(const FVector& V)
	{
		return {
			MakeShared<FJsonValueNumber>(V.X),
			MakeShared<FJsonValueNumber>(V.Y),
			MakeShared<FJsonValueNumber>(V.Z)
		};
	}

	TArray<TSharedPtr<FJsonValue>> RotArray(const FRotator& R)
	{
		return {
			MakeShared<FJsonValueNumber>(R.Pitch),
			MakeShared<FJsonValueNumber>(R.Yaw),
			MakeShared<FJsonValueNumber>(R.Roll)
		};
	}

	FString WriteJson(const TSharedRef<FJsonObject>& Root)
	{
		FString Out;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
		FJsonSerializer::Serialize(Root, Writer);
		return Out;
	}
}

void UAnastasisWorldProbeSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	UE_LOG(LogAnastasis_UnrealV2, Display, TEXT("ANASTASIS_WORLD_PROBE_READY map=%s"), *InWorld.GetMapName());
}

void UAnastasisWorldProbeSubsystem::Deinitialize()
{
	if (bCaptureInFlight)
	{
		CancelPendingCapture(TEXT("subsystem shutting down"));
	}
	Super::Deinitialize();
}

AAnastasisWorldEmbodiment* UAnastasisWorldProbeSubsystem::FindEmbodiment() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	// Prefer an instance that has actually embodied a world (Plan.TileCount > 0): a level can
	// hold an inert, never-BeginPlay'd placeholder (e.g. in a pure Editor world, where BeginPlay
	// never runs) alongside the live one a GameMode or test spawned and embodied.
	AAnastasisWorldEmbodiment* FirstFound = nullptr;
	for (TActorIterator<AAnastasisWorldEmbodiment> It(World); It; ++It)
	{
		if (!FirstFound)
		{
			FirstFound = *It;
		}
		if (It->GetPlan().TileCount > 0)
		{
			return *It;
		}
	}
	return FirstFound;
}

void UAnastasisWorldProbeSubsystem::EnsureDefaultBookmarks()
{
	AAnastasisWorldEmbodiment* Embodiment = FindEmbodiment();
	if (Bookmarks.Num() > 0 && BookmarkSourceEmbodiment.Get() == Embodiment)
	{
		return;
	}
	BookmarkSourceEmbodiment = Embodiment;

	if (!Embodiment)
	{
		for (const TCHAR* Name : {TEXT("OVERVIEW"), TEXT("GROUND"), TEXT("SHORE"), TEXT("SETTLEMENT")})
		{
			FAnastasisCameraBookmark Bookmark;
			Bookmark.Name = FName(Name);
			Bookmark.bReachable = false;
			Bookmark.UnreachableReason = TEXT("no AAnastasisWorldEmbodiment in this world");
			Bookmarks.Add(Bookmark.Name, Bookmark);
		}
		return;
	}

	const AnastasisWorldView::FPlan& Plan = Embodiment->GetPlan();
	// The actually rendered/collidable footprint — not PlanBounds(Plan), which reflects the full
	// crop BeginPlay requested and can be much larger than what surface mode actually built (see
	// AAnastasisWorldEmbodiment::ActiveFootprintBounds).
	const FBox& Bounds = Embodiment->GetActiveFootprintBounds();

	// OVERVIEW: top-down over the active footprint.
	{
		FAnastasisCameraBookmark Bookmark;
		Bookmark.Name = TEXT("OVERVIEW");
		if (Bounds.IsValid && Plan.TileCount > 0)
		{
			const FVector Center = Bounds.GetCenter();
			const double Span = Bounds.GetSize().GetMax();
			Bookmark.Location = FVector(Center.X, Center.Y, Bounds.Max.Z + Span * 0.9 + 500.0);
			Bookmark.Rotation = FRotator(-75.0, 0.0, 0.0);
			Bookmark.FieldOfView = 80.0f;
			Bookmark.bReachable = true;
		}
		else
		{
			Bookmark.UnreachableReason = TEXT("empty terrain plan (0 tiles)");
		}
		Bookmarks.Add(Bookmark.Name, Bookmark);
	}

	// GROUND: eye-level over the land tile nearest the crop center.
	{
		FAnastasisCameraBookmark Bookmark;
		Bookmark.Name = TEXT("GROUND");
		const int32 W = FMath::Max(1, Plan.W);
		const int32 CenterLocalX = Plan.W / 2;
		const int32 CenterLocalY = Plan.H / 2;
		int32 BestIndex = INDEX_NONE;
		int64 BestDistSq = TNumericLimits<int64>::Max();
		for (int32 Index = 0; Index < Plan.TileCount; ++Index)
		{
			if (Plan.Types[Index] == AnastasisWorld::ETileType::Water)
			{
				continue;
			}
			if (Bounds.IsValid && !Bounds.IsInsideXY(Plan.Locations[Index]))
			{
				continue;
			}
			const int32 LocalX = Index % W;
			const int32 LocalY = Index / W;
			const int64 DistSq = FMath::Square<int64>(LocalX - CenterLocalX) + FMath::Square<int64>(LocalY - CenterLocalY);
			if (DistSq < BestDistSq)
			{
				BestDistSq = DistSq;
				BestIndex = Index;
			}
		}
		if (BestIndex != INDEX_NONE)
		{
			Bookmark.Location = Plan.Locations[BestIndex] + FVector(0.0, 0.0, 180.0);
			Bookmark.Rotation = FRotator(-5.0, 45.0, 0.0);
			Bookmark.FieldOfView = 90.0f;
			Bookmark.bReachable = true;
		}
		else
		{
			Bookmark.UnreachableReason = TEXT("no land tile found in the embodied crop");
		}
		Bookmarks.Add(Bookmark.Name, Bookmark);
	}

	// SHORE: land tile with the smallest Shore distance to water (i.e. the coastline).
	{
		FAnastasisCameraBookmark Bookmark;
		Bookmark.Name = TEXT("SHORE");
		const AnastasisWorldView::FWorldVisualSnapshot& Snapshot = Embodiment->GetSnapshot();
		int32 BestIndex = INDEX_NONE;
		double BestShore = TNumericLimits<double>::Max();
		for (int32 Index = 0; Index < Snapshot.Tiles.Num(); ++Index)
		{
			const AnastasisWorldView::FVisualTile& Tile = Snapshot.Tiles[Index];
			if (Tile.Type == AnastasisWorld::ETileType::Water)
			{
				continue;
			}
			const FVector TileLoc = AnastasisWorldView::TileToUnreal(Tile.X, Tile.Y, Tile.Alt);
			if (Bounds.IsValid && !Bounds.IsInsideXY(TileLoc))
			{
				continue;
			}
			if (Tile.Shore < BestShore)
			{
				BestShore = Tile.Shore;
				BestIndex = Index;
			}
		}
		if (BestIndex != INDEX_NONE)
		{
			const AnastasisWorldView::FVisualTile& Tile = Snapshot.Tiles[BestIndex];
			const FVector TileLoc = AnastasisWorldView::TileToUnreal(Tile.X, Tile.Y, Tile.Alt);
			Bookmark.Location = TileLoc + FVector(-300.0, -300.0, 250.0);
			Bookmark.Rotation = (TileLoc - Bookmark.Location).Rotation();
			Bookmark.FieldOfView = 85.0f;
			Bookmark.bReachable = true;
		}
		else
		{
			Bookmark.UnreachableReason = TEXT("no shoreline tile in the embodied crop (no land/water boundary)");
		}
		Bookmarks.Add(Bookmark.Name, Bookmark);
	}

	// SETTLEMENT: honestly unreachable — no building/settlement placement system exists yet.
	{
		FAnastasisCameraBookmark Bookmark;
		Bookmark.Name = TEXT("SETTLEMENT");
		Bookmark.bReachable = false;
		Bookmark.UnreachableReason = TEXT("no settlement/building system implemented yet (PLAYER content stage NOT_IMPLEMENTED per AGENTS.md)");
		Bookmarks.Add(Bookmark.Name, Bookmark);
	}
}

bool UAnastasisWorldProbeSubsystem::GetCurrentCameraPose(FVector& OutLocation, FRotator& OutRotation, float& OutFov, FString& OutReason) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		OutReason = TEXT("no world");
		return false;
	}

	if (const APlayerController* PC = World->GetFirstPlayerController())
	{
		if (const APlayerCameraManager* CameraManager = PC->PlayerCameraManager)
		{
			OutLocation = CameraManager->GetCameraLocation();
			OutRotation = CameraManager->GetCameraRotation();
			OutFov = CameraManager->GetFOVAngle();
			return true;
		}
	}

	for (TActorIterator<ACameraActor> It(World); It; ++It)
	{
		if (UCameraComponent* Comp = It->GetCameraComponent())
		{
			OutLocation = It->GetActorLocation();
			OutRotation = It->GetActorRotation();
			OutFov = Comp->FieldOfView;
			return true;
		}
	}

	OutReason = TEXT("no PlayerCameraManager and no CameraActor in the world");
	return false;
}

ACameraActor* UAnastasisWorldProbeSubsystem::GetOrCreateProbeCamera()
{
	if (ProbeCamera)
	{
		return ProbeCamera;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	FActorSpawnParameters Params;
	Params.Name = TEXT("Anastasis_ProbeCamera");
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ACameraActor* Camera = World->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params);
#if WITH_EDITOR
	if (Camera)
	{
		Camera->SetActorLabel(TEXT("Anastasis_ProbeCamera"));
	}
#endif
	ProbeCamera = Camera;
	return Camera;
}

bool UAnastasisWorldProbeSubsystem::GotoBookmark(FName Name, FString& OutReason)
{
	EnsureDefaultBookmarks();

	const FAnastasisCameraBookmark* Bookmark = Bookmarks.Find(Name);
	if (!Bookmark)
	{
		OutReason = FString::Printf(TEXT("unknown bookmark '%s'"), *Name.ToString());
		return false;
	}
	if (!Bookmark->bReachable)
	{
		OutReason = Bookmark->UnreachableReason;
		return false;
	}

	UWorld* World = GetWorld();
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	if (!PC)
	{
		OutReason = TEXT("no PlayerController available in this world context");
		return false;
	}

	ACameraActor* Camera = GetOrCreateProbeCamera();
	if (!Camera)
	{
		OutReason = TEXT("failed to spawn the probe camera");
		return false;
	}

	Camera->SetActorLocationAndRotation(Bookmark->Location, Bookmark->Rotation);
	if (UCameraComponent* Comp = Camera->GetCameraComponent())
	{
		Comp->SetFieldOfView(Bookmark->FieldOfView);
	}
	PC->SetViewTarget(Camera);

	UE_LOG(LogAnastasis_UnrealV2, Display,
		TEXT("ANASTASIS_WORLD_GOTO bookmark=%s loc=(%.1f,%.1f,%.1f) rot=(%.1f,%.1f,%.1f) fov=%.1f"),
		*Name.ToString(), Bookmark->Location.X, Bookmark->Location.Y, Bookmark->Location.Z,
		Bookmark->Rotation.Pitch, Bookmark->Rotation.Yaw, Bookmark->Rotation.Roll, Bookmark->FieldOfView);
	return true;
}

void UAnastasisWorldProbeSubsystem::SetBookmarkFromCurrentCamera(FName Name)
{
	EnsureDefaultBookmarks();

	FVector Location;
	FRotator Rotation;
	float Fov = 90.0f;
	FString Reason;
	if (!GetCurrentCameraPose(Location, Rotation, Fov, Reason))
	{
		UE_LOG(LogAnastasis_UnrealV2, Warning, TEXT("ANASTASIS_WORLD_BOOKMARK reject name=%s reason=%s"), *Name.ToString(), *Reason);
		return;
	}

	FAnastasisCameraBookmark Bookmark;
	Bookmark.Name = Name;
	Bookmark.Location = Location;
	Bookmark.Rotation = Rotation;
	Bookmark.FieldOfView = Fov;
	Bookmark.bReachable = true;
	Bookmarks.Add(Name, Bookmark);

	UE_LOG(LogAnastasis_UnrealV2, Display,
		TEXT("ANASTASIS_WORLD_BOOKMARK_SET name=%s loc=(%.1f,%.1f,%.1f) rot=(%.1f,%.1f,%.1f) fov=%.1f"),
		*Name.ToString(), Location.X, Location.Y, Location.Z, Rotation.Pitch, Rotation.Yaw, Rotation.Roll, Fov);
}

void UAnastasisWorldProbeSubsystem::LogNavStatus()
{
	UWorld* World = GetWorld();
	UNavigationSystemV1* NavSys = World ? FNavigationSystem::GetCurrent<UNavigationSystemV1>(World) : nullptr;
	if (!NavSys)
	{
		UE_LOG(LogAnastasis_UnrealV2, Display, TEXT("ANASTASIS_WORLD_NAV present=0"));
		return;
	}

	const ANavigationData* NavData = NavSys->GetDefaultNavDataInstance(FNavigationSystem::DontCreate);
	const AWorldSettings* Settings = World->GetWorldSettings();
	const bool bBuilt = Settings ? NavSys->IsNavigationBuilt(Settings) : false;
	UE_LOG(LogAnastasis_UnrealV2, Display, TEXT("ANASTASIS_WORLD_NAV present=1 nav_data=%d built=%d"), NavData != nullptr, bBuilt);
}

TSharedRef<FJsonObject> UAnastasisWorldProbeSubsystem::BuildSnapshotObject() const
{
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> Errors;

	UWorld* World = GetWorld();
	Root->SetStringField(TEXT("schema"), TEXT("anastasis.world_snapshot.v1"));
	Root->SetStringField(TEXT("captured_at_utc"), FDateTime::UtcNow().ToIso8601());
	Root->SetStringField(TEXT("map"), World ? World->GetMapName() : FString());
	Root->SetStringField(TEXT("engine_version"), FEngineVersion::Current().ToString());
	Root->SetStringField(TEXT("build_configuration"), LexToString(FApp::GetBuildConfiguration()));

	if (!World)
	{
		Errors.Add(MakeShared<FJsonValueString>(TEXT("NO_WORLD")));
		Root->SetArrayField(TEXT("errors"), Errors);
		return Root;
	}

	// --- Actors ---
	int32 ActorCount = 0;
	TMap<FString, int32> ClassCounts;
	FBox WorldBounds(ForceInit);
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!IsValid(Actor))
		{
			continue;
		}
		++ActorCount;
		ClassCounts.FindOrAdd(Actor->GetClass()->GetName())++;

		FVector Origin, Extent;
		Actor->GetActorBounds(false, Origin, Extent);
		if (!Extent.IsNearlyZero())
		{
			WorldBounds += FBox::BuildAABB(Origin, Extent);
		}
	}
	Root->SetNumberField(TEXT("actor_count"), ActorCount);

	TArray<TPair<FString, int32>> ClassCountArray;
	ClassCountArray.Reserve(ClassCounts.Num());
	for (const TPair<FString, int32>& Pair : ClassCounts)
	{
		ClassCountArray.Add(Pair);
	}
	ClassCountArray.Sort([](const TPair<FString, int32>& A, const TPair<FString, int32>& B) { return A.Value > B.Value; });

	TArray<TSharedPtr<FJsonValue>> TopClasses;
	for (int32 Index = 0; Index < FMath::Min(10, ClassCountArray.Num()); ++Index)
	{
		TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
		Entry->SetStringField(TEXT("class"), ClassCountArray[Index].Key);
		Entry->SetNumberField(TEXT("count"), ClassCountArray[Index].Value);
		TopClasses.Add(MakeShared<FJsonValueObject>(Entry));
	}
	Root->SetArrayField(TEXT("top_classes"), TopClasses);

	const bool bWorldBoundsValid = WorldBounds.IsValid != 0;
	TSharedRef<FJsonObject> WorldBoundsObj = MakeShared<FJsonObject>();
	WorldBoundsObj->SetBoolField(TEXT("valid"), bWorldBoundsValid);
	if (bWorldBoundsValid)
	{
		WorldBoundsObj->SetArrayField(TEXT("min"), Vec3Array(WorldBounds.Min));
		WorldBoundsObj->SetArrayField(TEXT("max"), Vec3Array(WorldBounds.Max));
	}
	Root->SetObjectField(TEXT("world_bounds"), WorldBoundsObj);

	// --- Terrain (AAnastasisWorldEmbodiment) ---
	AAnastasisWorldEmbodiment* Embodiment = FindEmbodiment();
	TSharedRef<FJsonObject> TerrainObj = MakeShared<FJsonObject>();
	TerrainObj->SetBoolField(TEXT("present"), Embodiment != nullptr);
	bool bTerrainBoundsValid = false;
	FBox TerrainBounds(ForceInit);
	int32 TerrainTileCount = 0;
	bool bNanFound = false;

	if (Embodiment)
	{
		const AnastasisWorldView::FPlan& Plan = Embodiment->GetPlan();
		TerrainTileCount = Plan.TileCount;
		TerrainObj->SetNumberField(TEXT("seed"), Plan.Seed);
		TerrainObj->SetNumberField(TEXT("source_w"), Plan.SourceW);
		TerrainObj->SetNumberField(TEXT("source_h"), Plan.SourceH);
		TerrainObj->SetNumberField(TEXT("crop_origin_x"), Plan.OriginX);
		TerrainObj->SetNumberField(TEXT("crop_origin_y"), Plan.OriginY);
		TerrainObj->SetNumberField(TEXT("crop_w"), Plan.W);
		TerrainObj->SetNumberField(TEXT("crop_h"), Plan.H);
		TerrainObj->SetNumberField(TEXT("tile_count"), Plan.TileCount);
		TerrainObj->SetNumberField(TEXT("instance_count"), Embodiment->GetInstanceCount());
		TerrainObj->SetNumberField(TEXT("dressing_instance_count"), Embodiment->GetDressingInstanceCount());
		TerrainObj->SetNumberField(TEXT("min_alt_uu"), Plan.MinAlt * AnastasisWorldView::AltitudeScale);
		TerrainObj->SetNumberField(TEXT("max_alt_uu"), Plan.MaxAlt * AnastasisWorldView::AltitudeScale);

		TSharedRef<FJsonObject> CountsObj = MakeShared<FJsonObject>();
		for (int32 TypeIndex = 0; TypeIndex < AnastasisWorld::TileTypeCount; ++TypeIndex)
		{
			CountsObj->SetNumberField(
				AnastasisWorld::TileTypeName(static_cast<AnastasisWorld::ETileType>(TypeIndex)),
				Plan.TerrainCounts[TypeIndex]);
		}
		TerrainObj->SetObjectField(TEXT("counts_by_type"), CountsObj);

		// "bounds" is the actually rendered/collidable footprint (can be smaller than the
		// requested crop in surface mode, see AAnastasisWorldEmbodiment::ActiveFootprintBounds).
		// "requested_crop_bounds" is the full Plan the BeginPlay/Embody call asked for.
		TerrainBounds = Embodiment->GetActiveFootprintBounds();
		bTerrainBoundsValid = TerrainBounds.IsValid != 0;
		TSharedRef<FJsonObject> TerrainBoundsObj = MakeShared<FJsonObject>();
		TerrainBoundsObj->SetBoolField(TEXT("valid"), bTerrainBoundsValid);
		if (bTerrainBoundsValid)
		{
			TerrainBoundsObj->SetArrayField(TEXT("min"), Vec3Array(TerrainBounds.Min));
			TerrainBoundsObj->SetArrayField(TEXT("max"), Vec3Array(TerrainBounds.Max));
		}
		TerrainObj->SetObjectField(TEXT("bounds"), TerrainBoundsObj);

		const FBox RequestedBounds = AnastasisWorldView::PlanBounds(Plan);
		TSharedRef<FJsonObject> RequestedBoundsObj = MakeShared<FJsonObject>();
		RequestedBoundsObj->SetBoolField(TEXT("valid"), RequestedBounds.IsValid != 0);
		if (RequestedBounds.IsValid)
		{
			RequestedBoundsObj->SetArrayField(TEXT("min"), Vec3Array(RequestedBounds.Min));
			RequestedBoundsObj->SetArrayField(TEXT("max"), Vec3Array(RequestedBounds.Max));
		}
		TerrainObj->SetObjectField(TEXT("requested_crop_bounds"), RequestedBoundsObj);

		for (const FVector& Location : Plan.Locations)
		{
			if (Location.ContainsNaN())
			{
				bNanFound = true;
				break;
			}
		}
		if (!bNanFound && (FMath::IsNaN(Plan.MinAlt) || FMath::IsNaN(Plan.MaxAlt) || !FMath::IsFinite(Plan.MinAlt) || !FMath::IsFinite(Plan.MaxAlt)))
		{
			bNanFound = true;
		}
	}
	else
	{
		Errors.Add(MakeShared<FJsonValueString>(TEXT("TERRAIN_NOT_PRESENT")));
	}
	Root->SetObjectField(TEXT("terrain"), TerrainObj);

	// --- Water ---
	TSharedRef<FJsonObject> WaterObj = MakeShared<FJsonObject>();
	WaterObj->SetBoolField(TEXT("water_plugin_enabled"), false);
	const bool bCustomWaterPresent = Embodiment && Embodiment->HasWaterSurface();
	WaterObj->SetBoolField(TEXT("custom_water_surface_present"), bCustomWaterPresent);
	WaterObj->SetNumberField(TEXT("sea_level_uu"), AnastasisWorld::SeaLevel * AnastasisWorldView::AltitudeScale);
	Root->SetObjectField(TEXT("water"), WaterObj);

	// --- Camera ---
	TSharedRef<FJsonObject> CameraObj = MakeShared<FJsonObject>();
	FVector CamLoc;
	FRotator CamRot;
	float CamFov = 0.0f;
	FString CamReason;
	const bool bCameraActive = GetCurrentCameraPose(CamLoc, CamRot, CamFov, CamReason);
	CameraObj->SetBoolField(TEXT("active"), bCameraActive);
	if (bCameraActive)
	{
		CameraObj->SetArrayField(TEXT("location"), Vec3Array(CamLoc));
		CameraObj->SetArrayField(TEXT("rotation_pitch_yaw_roll"), RotArray(CamRot));
		CameraObj->SetNumberField(TEXT("fov"), CamFov);
	}
	else
	{
		CameraObj->SetStringField(TEXT("reason"), CamReason);
		Errors.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("CAMERA_NOT_AVAILABLE: %s"), *CamReason)));
	}
	Root->SetObjectField(TEXT("camera"), CameraObj);

	// --- PlayerStart / Pawn ---
	int32 PlayerStartCount = 0;
	FVector FirstPlayerStartLocation = FVector::ZeroVector;
	for (TActorIterator<APlayerStart> It(World); It; ++It)
	{
		if (PlayerStartCount == 0)
		{
			FirstPlayerStartLocation = It->GetActorLocation();
		}
		++PlayerStartCount;
	}
	APlayerController* PC = World->GetFirstPlayerController();
	APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	TSharedRef<FJsonObject> PlayerObj = MakeShared<FJsonObject>();
	PlayerObj->SetNumberField(TEXT("player_start_count"), PlayerStartCount);
	if (PlayerStartCount > 0)
	{
		PlayerObj->SetArrayField(TEXT("first_player_start_location"), Vec3Array(FirstPlayerStartLocation));
	}
	PlayerObj->SetBoolField(TEXT("controller_present"), PC != nullptr);
	PlayerObj->SetBoolField(TEXT("pawn_present"), Pawn != nullptr);
	PlayerObj->SetStringField(TEXT("pawn_class"), Pawn ? Pawn->GetClass()->GetName() : FString());
	Root->SetObjectField(TEXT("player"), PlayerObj);

	// --- World Partition / Data Layers ---
	UWorldPartition* WorldPartition = World->GetWorldPartition();
	TSharedRef<FJsonObject> WorldPartitionObj = MakeShared<FJsonObject>();
	WorldPartitionObj->SetBoolField(TEXT("enabled"), WorldPartition != nullptr);
	Root->SetObjectField(TEXT("world_partition"), WorldPartitionObj);

	TSharedRef<FJsonObject> DataLayersObj = MakeShared<FJsonObject>();
	UDataLayerManager* DataLayerManager = UDataLayerManager::GetDataLayerManager(World);
	DataLayersObj->SetBoolField(TEXT("available"), DataLayerManager != nullptr);
	if (DataLayerManager)
	{
		DataLayersObj->SetNumberField(TEXT("instance_count"), DataLayerManager->GetDataLayerInstances().Num());
	}
	Root->SetObjectField(TEXT("data_layers"), DataLayersObj);

	// --- Navigation ---
	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	TSharedRef<FJsonObject> NavObj = MakeShared<FJsonObject>();
	NavObj->SetBoolField(TEXT("present"), NavSys != nullptr);
	bool bNavBuilt = false;
	if (NavSys)
	{
		NavObj->SetBoolField(TEXT("nav_data_present"), NavSys->GetDefaultNavDataInstance(FNavigationSystem::DontCreate) != nullptr);
		if (const AWorldSettings* Settings = World->GetWorldSettings())
		{
			bNavBuilt = NavSys->IsNavigationBuilt(Settings);
		}
		NavObj->SetBoolField(TEXT("built"), bNavBuilt);
	}
	Root->SetObjectField(TEXT("navigation"), NavObj);

	// --- Bookmarks ---
	const_cast<UAnastasisWorldProbeSubsystem*>(this)->EnsureDefaultBookmarks();
	TArray<TSharedPtr<FJsonValue>> BookmarksArray;
	for (const TPair<FName, FAnastasisCameraBookmark>& Pair : Bookmarks)
	{
		const FAnastasisCameraBookmark& Bookmark = Pair.Value;
		TSharedRef<FJsonObject> BookmarkObj = MakeShared<FJsonObject>();
		BookmarkObj->SetStringField(TEXT("name"), Bookmark.Name.ToString());
		BookmarkObj->SetBoolField(TEXT("reachable"), Bookmark.bReachable);
		if (Bookmark.bReachable)
		{
			BookmarkObj->SetArrayField(TEXT("location"), Vec3Array(Bookmark.Location));
			BookmarkObj->SetArrayField(TEXT("rotation_pitch_yaw_roll"), RotArray(Bookmark.Rotation));
			BookmarkObj->SetNumberField(TEXT("fov"), Bookmark.FieldOfView);
		}
		else
		{
			BookmarkObj->SetStringField(TEXT("reason"), Bookmark.UnreachableReason);
		}
		BookmarksArray.Add(MakeShared<FJsonValueObject>(BookmarkObj));
	}
	Root->SetArrayField(TEXT("bookmarks"), BookmarksArray);

	// --- Sanity checks (Phase F) ---
	const bool bActorCountZero = ActorCount == 0;
	const bool bBoundsInvalid = !bWorldBoundsValid;
	TSharedRef<FJsonObject> SanityObj = MakeShared<FJsonObject>();
	SanityObj->SetBoolField(TEXT("terrain_present"), Embodiment != nullptr);
	SanityObj->SetBoolField(TEXT("water_present"), bCustomWaterPresent);
	SanityObj->SetBoolField(TEXT("camera_present"), bCameraActive);
	SanityObj->SetBoolField(TEXT("world_generated"), Embodiment != nullptr && TerrainTileCount > 0);
	SanityObj->SetBoolField(TEXT("nan_or_invalid_coordinates"), bNanFound);
	SanityObj->SetBoolField(TEXT("actor_count_zero"), bActorCountZero);
	SanityObj->SetBoolField(TEXT("bounds_invalid"), bBoundsInvalid);
	SanityObj->SetBoolField(TEXT("navigation_available"), NavSys != nullptr);
	const bool bCriticalOk = !bNanFound && !bActorCountZero && !bBoundsInvalid;
	SanityObj->SetBoolField(TEXT("passed"), bCriticalOk);
	Root->SetObjectField(TEXT("sanity"), SanityObj);

	if (bNanFound)
	{
		Errors.Add(MakeShared<FJsonValueString>(TEXT("NAN_OR_INVALID_COORDINATES")));
	}
	if (bActorCountZero)
	{
		Errors.Add(MakeShared<FJsonValueString>(TEXT("ACTOR_COUNT_ZERO")));
	}
	if (bBoundsInvalid)
	{
		Errors.Add(MakeShared<FJsonValueString>(TEXT("WORLD_BOUNDS_INVALID")));
	}
	if (Embodiment && !bTerrainBoundsValid)
	{
		Errors.Add(MakeShared<FJsonValueString>(TEXT("TERRAIN_BOUNDS_INVALID")));
	}
	Root->SetArrayField(TEXT("errors"), Errors);

	return Root;
}

FString UAnastasisWorldProbeSubsystem::WriteSnapshot()
{
	const TSharedRef<FJsonObject> Snapshot = BuildSnapshotObject();
	const FString Json = WriteJson(Snapshot);

	const FString Dir = FPaths::ProjectSavedDir() / TEXT("Anastasis/Diagnostics");
	IFileManager::Get().MakeDirectory(*Dir, true);

	const FString Timestamp = FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S"));
	const FString Path = FPaths::ConvertRelativePathToFull(Dir / (Timestamp + TEXT(".json")));
	const FString LatestPath = FPaths::ConvertRelativePathToFull(Dir / TEXT("latest.json"));

	if (!FFileHelper::SaveStringToFile(Json, *Path))
	{
		UE_LOG(LogAnastasis_UnrealV2, Error, TEXT("ANASTASIS_WORLD_SNAPSHOT write failed path=%s"), *Path);
		return FString();
	}
	FFileHelper::SaveStringToFile(Json, *LatestPath);

	UE_LOG(LogAnastasis_UnrealV2, Display, TEXT("ANASTASIS_WORLD_SNAPSHOT path=%s"), *Path);
	return Path;
}

void UAnastasisWorldProbeSubsystem::LogStatus()
{
	UWorld* World = GetWorld();
	AAnastasisWorldEmbodiment* Embodiment = FindEmbodiment();

	FVector CamLoc;
	FRotator CamRot;
	float CamFov = 0.0f;
	FString CamReason;
	const bool bCameraActive = GetCurrentCameraPose(CamLoc, CamRot, CamFov, CamReason);

	int32 ActorCount = 0;
	if (World)
	{
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			++ActorCount;
		}
	}

	UNavigationSystemV1* NavSys = World ? FNavigationSystem::GetCurrent<UNavigationSystemV1>(World) : nullptr;

	UE_LOG(LogAnastasis_UnrealV2, Display,
		TEXT("ANASTASIS_WORLD_STATUS map=%s actors=%d terrain_present=%d tiles=%d camera_active=%d nav_present=%d"),
		World ? *World->GetMapName() : TEXT("NONE"),
		ActorCount,
		Embodiment != nullptr,
		Embodiment ? Embodiment->GetPlan().TileCount : 0,
		bCameraActive,
		NavSys != nullptr);
}

void UAnastasisWorldProbeSubsystem::CancelPendingCapture(const TCHAR* Reason)
{
	if (ScreenshotDelegateHandle.IsValid())
	{
		FScreenshotRequest::OnScreenshotRequestProcessed().Remove(ScreenshotDelegateHandle);
		ScreenshotDelegateHandle.Reset();
	}
	UE_LOG(LogAnastasis_UnrealV2, Error, TEXT("CAPTURE::FAIL bookmark=%s reason=%s"), *PendingCaptureBookmarkName, Reason);
	bCaptureInFlight = false;
}

void UAnastasisWorldProbeSubsystem::HandleScreenshotProcessed()
{
	if (!bCaptureInFlight)
	{
		return;
	}
	FScreenshotRequest::OnScreenshotRequestProcessed().Remove(ScreenshotDelegateHandle);
	ScreenshotDelegateHandle.Reset();
	bCaptureInFlight = false;

	if (!FPaths::FileExists(PendingCaptureShotPath))
	{
		UE_LOG(LogAnastasis_UnrealV2, Error, TEXT("CAPTURE::FAIL no screenshot file at %s"), *PendingCaptureShotPath);
		return;
	}

	const TSharedRef<FJsonObject> Snapshot = BuildSnapshotObject();
	Snapshot->SetStringField(TEXT("capture_mission"), PendingCaptureMissionName);
	Snapshot->SetStringField(TEXT("capture_bookmark"), PendingCaptureBookmarkName);
	Snapshot->SetStringField(TEXT("capture_screenshot_path"), PendingCaptureShotPath);
	const FString Json = WriteJson(Snapshot);
	FFileHelper::SaveStringToFile(Json, *PendingCaptureJsonPath);

	UE_LOG(LogAnastasis_UnrealV2, Display, TEXT("CAPTURE::PASS shot=%s snapshot=%s"), *PendingCaptureShotPath, *PendingCaptureJsonPath);
}

void UAnastasisWorldProbeSubsystem::RequestCapture(FName BookmarkName, const FString& MissionName)
{
	if (bCaptureInFlight)
	{
		UE_LOG(LogAnastasis_UnrealV2, Warning, TEXT("CAPTURE::BUSY another capture is already in flight (bookmark=%s)"), *PendingCaptureBookmarkName);
		return;
	}

	FString Reason;
	if (!GotoBookmark(BookmarkName, Reason))
	{
		UE_LOG(LogAnastasis_UnrealV2, Warning, TEXT("CAPTURE::NOT_REACHABLE bookmark=%s reason=%s"), *BookmarkName.ToString(), *Reason);
		return;
	}

	const FString Mission = MissionName.IsEmpty() ? TEXT("adhoc") : MissionName;
	const FString Timestamp = FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S"));
	const FString Dir = FPaths::ProjectSavedDir() / TEXT("Anastasis/Captures") / Mission / BookmarkName.ToString();
	IFileManager::Get().MakeDirectory(*Dir, true);

	PendingCaptureShotPath = FPaths::ConvertRelativePathToFull(Dir / (Timestamp + TEXT(".png")));
	PendingCaptureJsonPath = FPaths::ConvertRelativePathToFull(Dir / (Timestamp + TEXT(".json")));
	PendingCaptureBookmarkName = BookmarkName.ToString();
	PendingCaptureMissionName = Mission;
	bCaptureInFlight = true;

	ScreenshotDelegateHandle = FScreenshotRequest::OnScreenshotRequestProcessed().AddUObject(
		this, &UAnastasisWorldProbeSubsystem::HandleScreenshotProcessed);

	const TWeakObjectPtr<UAnastasisWorldProbeSubsystem> WeakThis(this);
	const FString CaptureToken = PendingCaptureShotPath;

	// Give the view target blend / streaming a moment to settle before the shot is taken.
	FTSTicker::GetCoreTicker().AddTicker(TEXT("AnastasisCaptureSettle"), 0.5f, [WeakThis, CaptureToken](float) -> bool
	{
		UAnastasisWorldProbeSubsystem* Self = WeakThis.Get();
		if (Self && Self->bCaptureInFlight && Self->PendingCaptureShotPath == CaptureToken)
		{
			FScreenshotRequest::RequestScreenshot(Self->PendingCaptureShotPath, /*bInShowUI*/ false, /*bAddFilenameSuffix*/ false);
			UE_LOG(LogAnastasis_UnrealV2, Display, TEXT("ANASTASIS_WORLD_CAPTURE_REQUESTED shot=%s"), *Self->PendingCaptureShotPath);
		}
		return false;
	});

	// Fallback in case the screenshot pipeline never fires the processed delegate (e.g. no viewport).
	FTSTicker::GetCoreTicker().AddTicker(TEXT("AnastasisCaptureTimeout"), 8.0f, [WeakThis, CaptureToken](float) -> bool
	{
		UAnastasisWorldProbeSubsystem* Self = WeakThis.Get();
		if (Self && Self->bCaptureInFlight && Self->PendingCaptureShotPath == CaptureToken)
		{
			Self->CancelPendingCapture(TEXT("timeout waiting for OnScreenshotRequestProcessed"));
		}
		return false;
	});
}
