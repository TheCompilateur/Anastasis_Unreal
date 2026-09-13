#include "WorldView/AnastasisWorldProbeSubsystem.h"

#include "Anastasis_UnrealV2.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Containers/Ticker.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/WorldSettings.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Misc/App.h"
#include "Misc/DateTime.h"
#include "Misc/EngineVersion.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "NavigationSystem.h"
#include "ProceduralMeshComponent.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UnrealClient.h"
#include "WorldPartition/DataLayer/DataLayerManager.h"
#include "WorldPartition/WorldPartition.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "WorldView/AnastasisWorldEmbodiment.h"
#include "WorldView/AnastasisVisualMode.h"
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

	const TCHAR* ResourceName(AnastasisWorld::EResource Resource)
	{
		switch (Resource)
		{
		case AnastasisWorld::EResource::None: return TEXT("None");
		case AnastasisWorld::EResource::Stone: return TEXT("Stone");
		case AnastasisWorld::EResource::Wood: return TEXT("Wood");
		case AnastasisWorld::EResource::Food: return TEXT("Food");
		}
		return TEXT("Unknown");
	}

	const TCHAR* CropName(AnastasisWorld::ECropId Crop)
	{
		switch (Crop)
		{
		case AnastasisWorld::ECropId::None: return TEXT("None");
		case AnastasisWorld::ECropId::Grain: return TEXT("Grain");
		case AnastasisWorld::ECropId::Greens: return TEXT("Greens");
		case AnastasisWorld::ECropId::Fruit: return TEXT("Fruit");
		case AnastasisWorld::ECropId::Fallow: return TEXT("Fallow");
		}
		return TEXT("Unknown");
	}

	void SetVec3Object(TSharedRef<FJsonObject> Obj, const TCHAR* Field, const FVector& V)
	{
		TSharedRef<FJsonObject> Vec = MakeShared<FJsonObject>();
		Vec->SetNumberField(TEXT("x"), V.X);
		Vec->SetNumberField(TEXT("y"), V.Y);
		Vec->SetNumberField(TEXT("z"), V.Z);
		Obj->SetObjectField(Field, Vec);
	}

	void SetRotObject(TSharedRef<FJsonObject> Obj, const TCHAR* Field, const FRotator& R)
	{
		TSharedRef<FJsonObject> Rot = MakeShared<FJsonObject>();
		Rot->SetNumberField(TEXT("pitch"), R.Pitch);
		Rot->SetNumberField(TEXT("yaw"), R.Yaw);
		Rot->SetNumberField(TEXT("roll"), R.Roll);
		Obj->SetObjectField(Field, Rot);
	}

	TSharedRef<FJsonObject> EvidenceScopeObject(const TCHAR* ToolName)
	{
		TSharedRef<FJsonObject> Scope = MakeShared<FJsonObject>();
		Scope->SetStringField(TEXT("tool"), ToolName);
		Scope->SetStringField(TEXT("mode"), TEXT("READ_ONLY"));
		Scope->SetStringField(TEXT("modifies_world"), TEXT("false"));
		Scope->SetStringField(TEXT("mec"), TEXT("observation_only"));
		Scope->SetStringField(TEXT("scn"), TEXT("UNKNOWN unless paired capture exists"));
		Scope->SetStringField(TEXT("ply"), TEXT("UNKNOWN"));
		return Scope;
	}

	TArray<TSharedPtr<FJsonValue>> TagsArray(const AActor* Actor)
	{
		TArray<TSharedPtr<FJsonValue>> Values;
		if (!Actor)
		{
			return Values;
		}
		for (const FName& Tag : Actor->Tags)
		{
			Values.Add(MakeShared<FJsonValueString>(Tag.ToString()));
		}
		return Values;
	}

	FString ActorLabelOrName(const AActor* Actor)
	{
		if (!Actor)
		{
			return FString();
		}
#if WITH_EDITOR
		return Actor->GetActorLabel();
#else
		return Actor->GetName();
#endif
	}

	bool ActorMatchesQuery(const AActor* Actor, const FString& LowerQuery)
	{
		if (!Actor || LowerQuery.IsEmpty())
		{
			return false;
		}

		if (Actor->GetName().ToLower().Contains(LowerQuery)
			|| ActorLabelOrName(Actor).ToLower().Contains(LowerQuery)
			|| Actor->GetClass()->GetName().ToLower().Contains(LowerQuery))
		{
			return true;
		}

		for (const FName& Tag : Actor->Tags)
		{
			if (Tag.ToString().ToLower().Contains(LowerQuery))
			{
				return true;
			}
		}
		return false;
	}

	TSharedRef<FJsonObject> ActorObject(const AActor* Actor)
	{
		TSharedRef<FJsonObject> Obj = MakeShared<FJsonObject>();
		if (!Actor)
		{
			Obj->SetBoolField(TEXT("present"), false);
			return Obj;
		}

		Obj->SetBoolField(TEXT("present"), true);
		Obj->SetStringField(TEXT("name"), Actor->GetName());
		Obj->SetStringField(TEXT("label"), ActorLabelOrName(Actor));
		Obj->SetStringField(TEXT("class"), Actor->GetClass()->GetName());
		Obj->SetArrayField(TEXT("tags"), TagsArray(Actor));
		Obj->SetBoolField(TEXT("hidden"), Actor->IsHidden());
		Obj->SetBoolField(TEXT("pending_kill_or_unreachable"), !IsValid(Actor));
		SetVec3Object(Obj, TEXT("location"), Actor->GetActorLocation());
		SetRotObject(Obj, TEXT("rotation"), Actor->GetActorRotation());
		SetVec3Object(Obj, TEXT("scale"), Actor->GetActorScale3D());

		FVector Origin = FVector::ZeroVector;
		FVector Extent = FVector::ZeroVector;
		Actor->GetActorBounds(false, Origin, Extent);
		TSharedRef<FJsonObject> BoundsObj = MakeShared<FJsonObject>();
		SetVec3Object(BoundsObj, TEXT("origin"), Origin);
		SetVec3Object(BoundsObj, TEXT("extent"), Extent);
		BoundsObj->SetBoolField(TEXT("non_zero"), !Extent.IsNearlyZero());
		Obj->SetObjectField(TEXT("bounds"), BoundsObj);

		TArray<UActorComponent*> Components;
		Actor->GetComponents(Components);
		int32 SceneComponentCount = 0;
		int32 PrimitiveComponentCount = 0;
		int32 VisiblePrimitiveCount = 0;
		int32 CollisionEnabledPrimitiveCount = 0;
		for (const UActorComponent* Component : Components)
		{
			if (Cast<USceneComponent>(Component))
			{
				++SceneComponentCount;
			}
			if (const UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Component))
			{
				++PrimitiveComponentCount;
				if (Primitive->IsVisible())
				{
					++VisiblePrimitiveCount;
				}
				if (Primitive->GetCollisionEnabled() != ECollisionEnabled::NoCollision)
				{
					++CollisionEnabledPrimitiveCount;
				}
			}
		}

		TSharedRef<FJsonObject> ComponentObj = MakeShared<FJsonObject>();
		ComponentObj->SetNumberField(TEXT("total"), Components.Num());
		ComponentObj->SetNumberField(TEXT("scene"), SceneComponentCount);
		ComponentObj->SetNumberField(TEXT("primitive"), PrimitiveComponentCount);
		ComponentObj->SetNumberField(TEXT("visible_primitives"), VisiblePrimitiveCount);
		ComponentObj->SetNumberField(TEXT("collision_enabled_primitives"), CollisionEnabledPrimitiveCount);
		Obj->SetObjectField(TEXT("components"), ComponentObj);

		return Obj;
	}

	int32 CVarIntValue(const TCHAR* Name, int32 DefaultValue)
	{
		if (const IConsoleVariable* Var = IConsoleManager::Get().FindConsoleVariable(Name))
		{
			return Var->GetInt();
		}
		return DefaultValue;
	}

	TSharedRef<FJsonObject> VerificationScopeObject(const TCHAR* ToolName)
	{
		TSharedRef<FJsonObject> Scope = MakeShared<FJsonObject>();
		Scope->SetStringField(TEXT("tool"), ToolName);
		Scope->SetStringField(TEXT("mode"), TEXT("VERIFY_READ_ONLY"));
		Scope->SetStringField(TEXT("modifies_world"), TEXT("false"));
		Scope->SetStringField(TEXT("status_domain"), TEXT("PASS|FAIL|UNKNOWN"));
		Scope->SetStringField(TEXT("mec"), TEXT("explicit"));
		Scope->SetStringField(TEXT("scn"), TEXT("explicit"));
		Scope->SetStringField(TEXT("ply"), TEXT("explicit"));
		return Scope;
	}

	void AddCheck(TArray<TSharedPtr<FJsonValue>>& Checks, const TCHAR* Code, const TCHAR* Status, const FString& Detail)
	{
		TSharedRef<FJsonObject> Check = MakeShared<FJsonObject>();
		Check->SetStringField(TEXT("code"), Code);
		Check->SetStringField(TEXT("status"), Status);
		Check->SetStringField(TEXT("detail"), Detail);
		Checks.Add(MakeShared<FJsonValueObject>(Check));
	}

	const TCHAR* VerificationStatus(int32 FailCount, int32 UnknownCount)
	{
		if (FailCount > 0)
		{
			return TEXT("FAIL");
		}
		if (UnknownCount > 0)
		{
			return TEXT("UNKNOWN");
		}
		return TEXT("PASS");
	}

	void SetCounts(TSharedRef<FJsonObject> Root, int32 PassCount, int32 FailCount, int32 UnknownCount)
	{
		TSharedRef<FJsonObject> Counts = MakeShared<FJsonObject>();
		Counts->SetNumberField(TEXT("pass"), PassCount);
		Counts->SetNumberField(TEXT("fail"), FailCount);
		Counts->SetNumberField(TEXT("unknown"), UnknownCount);
		Root->SetObjectField(TEXT("counts"), Counts);
		Root->SetStringField(TEXT("status"), VerificationStatus(FailCount, UnknownCount));
	}

	bool IsFiniteVector(const FVector& Value)
	{
		return FMath::IsFinite(Value.X) && FMath::IsFinite(Value.Y) && FMath::IsFinite(Value.Z);
	}

	bool HasAnyCaptureArtifact()
	{
		const FString CaptureDir = FPaths::ProjectSavedDir() / TEXT("Anastasis/Captures");
		if (!IFileManager::Get().DirectoryExists(*CaptureDir))
		{
			return false;
		}

		TArray<FString> Files;
		IFileManager::Get().FindFilesRecursive(Files, *CaptureDir, TEXT("*.png"), true, false);
		if (Files.Num() > 0)
		{
			return true;
		}
		IFileManager::Get().FindFilesRecursive(Files, *CaptureDir, TEXT("*.json"), true, false);
		return Files.Num() > 0;
	}

	bool IsInsideSnapshot(const AnastasisWorldView::FWorldVisualSnapshot& Snapshot, int32 X, int32 Y)
	{
		return X >= Snapshot.OriginX
			&& Y >= Snapshot.OriginY
			&& X < Snapshot.OriginX + Snapshot.W
			&& Y < Snapshot.OriginY + Snapshot.H;
	}

	bool IsSemanticLandClearance(AnastasisWorld::ETileType Type)
	{
		return Type == AnastasisWorld::ETileType::Grass || Type == AnastasisWorld::ETileType::Scrub;
	}

	void CountSemanticSlice(
		const AnastasisWorldView::FWorldVisualSnapshot& Snapshot,
		int32 OriginX,
		int32 OriginY,
		int32 Size,
		int32& OutWater,
		int32& OutForest,
		int32& OutField,
		int32& OutClearing,
		int32& OutShoreContacts)
	{
		for (int32 Y = OriginY; Y < OriginY + Size; ++Y)
		{
			for (int32 X = OriginX; X < OriginX + Size; ++X)
			{
				const AnastasisWorldView::FVisualTile* Tile = AnastasisWorldView::FindTile(Snapshot, X, Y);
				if (!Tile)
				{
					continue;
				}
				OutWater += Tile->Type == AnastasisWorld::ETileType::Water ? 1 : 0;
				OutForest += Tile->Type == AnastasisWorld::ETileType::Forest ? 1 : 0;
				OutField += Tile->Type == AnastasisWorld::ETileType::Field ? 1 : 0;
				OutClearing += IsSemanticLandClearance(Tile->Type) ? 1 : 0;

				const int32 Offsets[4][2] = { {-1, 0}, {1, 0}, {0, -1}, {0, 1} };
				for (const auto& Offset : Offsets)
				{
					const AnastasisWorldView::FVisualTile* Neighbour = AnastasisWorldView::FindTile(Snapshot, X + Offset[0], Y + Offset[1]);
					if (Neighbour && Tile->Type == AnastasisWorld::ETileType::Water && Neighbour->Type != AnastasisWorld::ETileType::Water)
					{
						++OutShoreContacts;
					}
				}
			}
		}
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

FString UAnastasisWorldProbeSubsystem::WriteInspectionObject(const FString& Slug, const TSharedRef<FJsonObject>& Root) const
{
	Root->SetStringField(TEXT("written_at_utc"), FDateTime::UtcNow().ToIso8601());

	const FString Json = WriteJson(Root);
	const FString Dir = FPaths::ProjectSavedDir() / TEXT("Anastasis/Diagnostics/inspections");
	IFileManager::Get().MakeDirectory(*Dir, true);

	const FString SafeSlug = FPaths::MakeValidFileName(Slug.IsEmpty() ? TEXT("inspection") : Slug);
	const FString Timestamp = FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S"));
	const FString Path = FPaths::ConvertRelativePathToFull(Dir / (Timestamp + TEXT("-") + SafeSlug + TEXT(".json")));
	const FString LatestPath = FPaths::ConvertRelativePathToFull(Dir / (TEXT("latest-") + SafeSlug + TEXT(".json")));

	if (!FFileHelper::SaveStringToFile(Json, *Path))
	{
		UE_LOG(LogAnastasis_UnrealV2, Error, TEXT("ANASTASIS_INSPECT write failed path=%s"), *Path);
		return FString();
	}
	FFileHelper::SaveStringToFile(Json, *LatestPath);

	FString Schema;
	Root->TryGetStringField(TEXT("schema"), Schema);
	UE_LOG(LogAnastasis_UnrealV2, Display, TEXT("ANASTASIS_INSPECT path=%s schema=%s"), *Path, *Schema);
	return Path;
}

FString UAnastasisWorldProbeSubsystem::WriteVerificationObject(const FString& Slug, const TSharedRef<FJsonObject>& Root) const
{
	Root->SetStringField(TEXT("written_at_utc"), FDateTime::UtcNow().ToIso8601());

	const FString Json = WriteJson(Root);
	const FString Dir = FPaths::ProjectSavedDir() / TEXT("Anastasis/Diagnostics/verifications");
	IFileManager::Get().MakeDirectory(*Dir, true);

	const FString SafeSlug = FPaths::MakeValidFileName(Slug.IsEmpty() ? TEXT("verification") : Slug);
	const FString Timestamp = FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S"));
	const FString Path = FPaths::ConvertRelativePathToFull(Dir / (Timestamp + TEXT("-") + SafeSlug + TEXT(".json")));
	const FString LatestPath = FPaths::ConvertRelativePathToFull(Dir / (TEXT("latest-") + SafeSlug + TEXT(".json")));

	if (!FFileHelper::SaveStringToFile(Json, *Path))
	{
		UE_LOG(LogAnastasis_UnrealV2, Error, TEXT("ANASTASIS_VERIFY write failed path=%s"), *Path);
		return FString();
	}
	FFileHelper::SaveStringToFile(Json, *LatestPath);

	FString Schema;
	FString Status;
	Root->TryGetStringField(TEXT("schema"), Schema);
	Root->TryGetStringField(TEXT("status"), Status);
	UE_LOG(
		LogAnastasis_UnrealV2,
		Display,
		TEXT("ANASTASIS_VERIFY path=%s schema=%s status=%s"),
		*Path,
		*Schema,
		*Status);
	return Path;
}

FString UAnastasisWorldProbeSubsystem::InspectWorld()
{
	TSharedRef<FJsonObject> Root = BuildSnapshotObject();
	Root->SetStringField(TEXT("schema"), TEXT("anastasis.inspect_world.v1"));
	Root->SetStringField(TEXT("status"), TEXT("OBSERVED"));
	Root->SetStringField(TEXT("operation"), TEXT("inspect_anastasis_world"));
	Root->SetObjectField(TEXT("evidence_scope"), EvidenceScopeObject(TEXT("inspect_anastasis_world")));
	return WriteInspectionObject(TEXT("inspect_world"), Root);
}

FString UAnastasisWorldProbeSubsystem::InspectTile(int32 TileX, int32 TileY)
{
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> Errors;
	Root->SetStringField(TEXT("schema"), TEXT("anastasis.inspect_tile.v1"));
	Root->SetStringField(TEXT("operation"), TEXT("inspect_tile"));
	Root->SetObjectField(TEXT("evidence_scope"), EvidenceScopeObject(TEXT("inspect_tile")));

	TSharedRef<FJsonObject> QueryObj = MakeShared<FJsonObject>();
	QueryObj->SetNumberField(TEXT("x"), TileX);
	QueryObj->SetNumberField(TEXT("y"), TileY);
	QueryObj->SetStringField(TEXT("coordinate_space"), TEXT("canonical/source tile coordinates"));
	Root->SetObjectField(TEXT("query"), QueryObj);

	AAnastasisWorldEmbodiment* Embodiment = FindEmbodiment();
	Root->SetBoolField(TEXT("embodiment_present"), Embodiment != nullptr);
	if (!Embodiment)
	{
		Root->SetStringField(TEXT("status"), TEXT("UNKNOWN"));
		Errors.Add(MakeShared<FJsonValueString>(TEXT("TERRAIN_NOT_PRESENT")));
		Root->SetArrayField(TEXT("errors"), Errors);
		return WriteInspectionObject(TEXT("inspect_tile"), Root);
	}

	const AnastasisWorldView::FWorldVisualSnapshot& Snapshot = Embodiment->GetSnapshot();
	TSharedRef<FJsonObject> SnapshotObj = MakeShared<FJsonObject>();
	SnapshotObj->SetNumberField(TEXT("seed"), Snapshot.Seed);
	SnapshotObj->SetNumberField(TEXT("source_w"), Snapshot.SourceW);
	SnapshotObj->SetNumberField(TEXT("source_h"), Snapshot.SourceH);
	SnapshotObj->SetNumberField(TEXT("origin_x"), Snapshot.OriginX);
	SnapshotObj->SetNumberField(TEXT("origin_y"), Snapshot.OriginY);
	SnapshotObj->SetNumberField(TEXT("w"), Snapshot.W);
	SnapshotObj->SetNumberField(TEXT("h"), Snapshot.H);
	Root->SetObjectField(TEXT("snapshot"), SnapshotObj);

	const AnastasisWorldView::FVisualTile* Tile = AnastasisWorldView::FindTile(Snapshot, TileX, TileY);
	if (!Tile)
	{
		Root->SetStringField(TEXT("status"), TEXT("UNKNOWN"));
		Errors.Add(MakeShared<FJsonValueString>(TEXT("TILE_OUTSIDE_EMBODIED_SNAPSHOT")));
		Root->SetArrayField(TEXT("errors"), Errors);
		return WriteInspectionObject(TEXT("inspect_tile"), Root);
	}

	const FVector UnrealLocation = AnastasisWorldView::TileToUnreal(Tile->X, Tile->Y, Tile->Alt);
	const FBox& ActiveBounds = Embodiment->GetActiveFootprintBounds();

	TSharedRef<FJsonObject> TileObj = MakeShared<FJsonObject>();
	TileObj->SetNumberField(TEXT("source_index"), Tile->SourceIndex);
	TileObj->SetNumberField(TEXT("x"), Tile->X);
	TileObj->SetNumberField(TEXT("y"), Tile->Y);
	TileObj->SetStringField(TEXT("type"), AnastasisWorld::TileTypeName(Tile->Type));
	TileObj->SetStringField(TEXT("resource"), ResourceName(Tile->Resource));
	TileObj->SetNumberField(TEXT("amount"), Tile->Amount);
	TileObj->SetNumberField(TEXT("alt"), Tile->Alt);
	TileObj->SetNumberField(TEXT("alt_uu"), Tile->Alt * AnastasisWorldView::AltitudeScale);
	TileObj->SetNumberField(TEXT("shade"), Tile->Shade);
	TileObj->SetNumberField(TEXT("shore"), Tile->Shore);
	TileObj->SetNumberField(TEXT("wetness"), Tile->Wetness);
	TileObj->SetNumberField(TEXT("flow_x"), Tile->FlowX);
	TileObj->SetNumberField(TEXT("flow_z"), Tile->FlowZ);
	TileObj->SetNumberField(TEXT("flow_amt"), Tile->FlowAmt);
	TileObj->SetStringField(TEXT("crop_id"), CropName(Tile->CropId));
	TileObj->SetNumberField(TEXT("fertility"), Tile->Fertility);
	TileObj->SetNumberField(TEXT("forest_margin"), Tile->ForestMargin);
	TileObj->SetBoolField(TEXT("has_forest_margin"), Tile->bHasForestMargin);
	TileObj->SetBoolField(TEXT("inside_active_visual_footprint"), ActiveBounds.IsValid && ActiveBounds.IsInsideXY(UnrealLocation));
	SetVec3Object(TileObj, TEXT("unreal_location"), UnrealLocation);
	Root->SetObjectField(TEXT("tile"), TileObj);

	TArray<TSharedPtr<FJsonValue>> Neighbours;
	const int32 NeighbourOffsets[4][2] = { {-1, 0}, {1, 0}, {0, -1}, {0, 1} };
	for (const auto& Offset : NeighbourOffsets)
	{
		const AnastasisWorldView::FVisualTile* Neighbour = AnastasisWorldView::FindTile(Snapshot, TileX + Offset[0], TileY + Offset[1]);
		if (!Neighbour)
		{
			continue;
		}
		TSharedRef<FJsonObject> NeighbourObj = MakeShared<FJsonObject>();
		NeighbourObj->SetNumberField(TEXT("x"), Neighbour->X);
		NeighbourObj->SetNumberField(TEXT("y"), Neighbour->Y);
		NeighbourObj->SetStringField(TEXT("type"), AnastasisWorld::TileTypeName(Neighbour->Type));
		NeighbourObj->SetNumberField(TEXT("shore"), Neighbour->Shore);
		NeighbourObj->SetNumberField(TEXT("wetness"), Neighbour->Wetness);
		Neighbours.Add(MakeShared<FJsonValueObject>(NeighbourObj));
	}
	Root->SetArrayField(TEXT("cardinal_neighbours"), Neighbours);
	Root->SetStringField(TEXT("status"), TEXT("OBSERVED"));
	Root->SetArrayField(TEXT("errors"), Errors);
	return WriteInspectionObject(TEXT("inspect_tile"), Root);
}

FString UAnastasisWorldProbeSubsystem::InspectSettlement(const FString& SettlementId)
{
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("schema"), TEXT("anastasis.inspect_settlement.v1"));
	Root->SetStringField(TEXT("operation"), TEXT("inspect_settlement"));
	Root->SetStringField(TEXT("status"), TEXT("NOT_IMPLEMENTED"));
	Root->SetStringField(TEXT("settlement_id"), SettlementId);
	Root->SetObjectField(TEXT("evidence_scope"), EvidenceScopeObject(TEXT("inspect_settlement")));
	Root->SetStringField(TEXT("reason"), TEXT("No canonical Unreal settlement/building runtime model is implemented in this module yet."));

	TArray<TSharedPtr<FJsonValue>> Candidates;
	UWorld* World = GetWorld();
	const FString LowerId = SettlementId.ToLower();
	if (World)
	{
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			const AActor* Actor = *It;
			const bool bSettlementLike = ActorMatchesQuery(Actor, TEXT("settlement"))
				|| ActorMatchesQuery(Actor, TEXT("building"))
				|| (!LowerId.IsEmpty() && ActorMatchesQuery(Actor, LowerId));
			if (bSettlementLike)
			{
				Candidates.Add(MakeShared<FJsonValueObject>(ActorObject(Actor)));
			}
			if (Candidates.Num() >= 20)
			{
				break;
			}
		}
	}
	Root->SetArrayField(TEXT("runtime_actor_candidates"), Candidates);
	Root->SetStringField(TEXT("claim_boundary"), TEXT("Actor candidates do not prove a canonical settlement system."));
	return WriteInspectionObject(TEXT("inspect_settlement"), Root);
}

FString UAnastasisWorldProbeSubsystem::InspectActor(const FString& ActorQuery)
{
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> Errors;
	Root->SetStringField(TEXT("schema"), TEXT("anastasis.inspect_actor.v1"));
	Root->SetStringField(TEXT("operation"), TEXT("inspect_actor"));
	Root->SetStringField(TEXT("query"), ActorQuery);
	Root->SetObjectField(TEXT("evidence_scope"), EvidenceScopeObject(TEXT("inspect_actor")));

	if (ActorQuery.TrimStartAndEnd().IsEmpty())
	{
		Root->SetStringField(TEXT("status"), TEXT("UNKNOWN"));
		Errors.Add(MakeShared<FJsonValueString>(TEXT("EMPTY_QUERY")));
		Root->SetArrayField(TEXT("errors"), Errors);
		return WriteInspectionObject(TEXT("inspect_actor"), Root);
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		Root->SetStringField(TEXT("status"), TEXT("UNKNOWN"));
		Errors.Add(MakeShared<FJsonValueString>(TEXT("NO_WORLD")));
		Root->SetArrayField(TEXT("errors"), Errors);
		return WriteInspectionObject(TEXT("inspect_actor"), Root);
	}

	TArray<TSharedPtr<FJsonValue>> Matches;
	const FString LowerQuery = ActorQuery.ToLower();
	int32 TotalMatches = 0;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		const AActor* Actor = *It;
		if (!ActorMatchesQuery(Actor, LowerQuery))
		{
			continue;
		}
		++TotalMatches;
		if (Matches.Num() < 20)
		{
			Matches.Add(MakeShared<FJsonValueObject>(ActorObject(Actor)));
		}
	}

	Root->SetNumberField(TEXT("match_count"), TotalMatches);
	Root->SetArrayField(TEXT("matches"), Matches);
	Root->SetStringField(TEXT("status"), TotalMatches > 0 ? TEXT("OBSERVED") : TEXT("UNKNOWN"));
	if (TotalMatches == 0)
	{
		Errors.Add(MakeShared<FJsonValueString>(TEXT("NO_ACTOR_MATCH")));
	}
	Root->SetArrayField(TEXT("errors"), Errors);
	return WriteInspectionObject(TEXT("inspect_actor"), Root);
}

FString UAnastasisWorldProbeSubsystem::InspectVisualSceneState()
{
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> Errors;
	Root->SetStringField(TEXT("schema"), TEXT("anastasis.inspect_visual_scene_state.v1"));
	Root->SetStringField(TEXT("operation"), TEXT("inspect_visual_scene_state"));
	Root->SetObjectField(TEXT("evidence_scope"), EvidenceScopeObject(TEXT("inspect_visual_scene_state")));

	UWorld* World = GetWorld();
	if (!World)
	{
		Root->SetStringField(TEXT("status"), TEXT("UNKNOWN"));
		Errors.Add(MakeShared<FJsonValueString>(TEXT("NO_WORLD")));
		Root->SetArrayField(TEXT("errors"), Errors);
		return WriteInspectionObject(TEXT("inspect_visual_scene_state"), Root);
	}

	const EAnastasisVisualMode VisualMode = AnastasisVisualMode::Get();
	TSharedRef<FJsonObject> ModeObj = MakeShared<FJsonObject>();
	ModeObj->SetStringField(TEXT("visual_mode"), AnastasisVisualMode::Name(VisualMode));
	ModeObj->SetNumberField(TEXT("anastasis_visual_mode_cvar"), CVarIntValue(TEXT("anastasis.Visual.Mode"), 1));
	ModeObj->SetNumberField(TEXT("terrain_surface_cvar"), CVarIntValue(TEXT("anastasis.Terrain.Surface"), 0));
	ModeObj->SetNumberField(TEXT("worldview_seed_cvar"), CVarIntValue(TEXT("anastasis.WorldView.Seed"), 12345));
	ModeObj->SetNumberField(TEXT("worldview_crop_x_cvar"), CVarIntValue(TEXT("anastasis.WorldView.CropX"), 0));
	ModeObj->SetNumberField(TEXT("worldview_crop_y_cvar"), CVarIntValue(TEXT("anastasis.WorldView.CropY"), 0));
	ModeObj->SetNumberField(TEXT("worldview_width_cvar"), CVarIntValue(TEXT("anastasis.WorldView.Width"), 96));
	ModeObj->SetNumberField(TEXT("worldview_height_cvar"), CVarIntValue(TEXT("anastasis.WorldView.Height"), 96));
	Root->SetObjectField(TEXT("mode"), ModeObj);

	AAnastasisWorldEmbodiment* Embodiment = FindEmbodiment();
	TSharedRef<FJsonObject> EmbodimentObj = MakeShared<FJsonObject>();
	EmbodimentObj->SetBoolField(TEXT("present"), Embodiment != nullptr);
	if (Embodiment)
	{
		const AnastasisWorldView::FPlan& Plan = Embodiment->GetPlan();
		EmbodimentObj->SetObjectField(TEXT("actor"), ActorObject(Embodiment));
		EmbodimentObj->SetNumberField(TEXT("plan_tile_count"), Plan.TileCount);
		EmbodimentObj->SetNumberField(TEXT("instance_count"), Embodiment->GetInstanceCount());
		EmbodimentObj->SetBoolField(TEXT("custom_water_surface_present"), Embodiment->HasWaterSurface());
	}
	else
	{
		Errors.Add(MakeShared<FJsonValueString>(TEXT("TERRAIN_NOT_PRESENT")));
	}
	Root->SetObjectField(TEXT("embodiment"), EmbodimentObj);

	int32 ActorCount = 0;
	int32 PrimitiveComponentCount = 0;
	int32 VisiblePrimitiveComponentCount = 0;
	int32 ProceduralMeshComponentCount = 0;
	int32 HismComponentCount = 0;
	int32 HismInstanceCount = 0;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		++ActorCount;
		TArray<UPrimitiveComponent*> PrimitiveComponents;
		It->GetComponents(PrimitiveComponents);
		for (const UPrimitiveComponent* Primitive : PrimitiveComponents)
		{
			++PrimitiveComponentCount;
			if (Primitive->IsVisible())
			{
				++VisiblePrimitiveComponentCount;
			}
			if (const UHierarchicalInstancedStaticMeshComponent* Hism = Cast<UHierarchicalInstancedStaticMeshComponent>(Primitive))
			{
				++HismComponentCount;
				HismInstanceCount += Hism->GetInstanceCount();
			}
			if (Cast<UProceduralMeshComponent>(Primitive))
			{
				++ProceduralMeshComponentCount;
			}
		}
	}

	TSharedRef<FJsonObject> ComponentsObj = MakeShared<FJsonObject>();
	ComponentsObj->SetNumberField(TEXT("actors"), ActorCount);
	ComponentsObj->SetNumberField(TEXT("primitive_components"), PrimitiveComponentCount);
	ComponentsObj->SetNumberField(TEXT("visible_primitive_components"), VisiblePrimitiveComponentCount);
	ComponentsObj->SetNumberField(TEXT("procedural_mesh_components"), ProceduralMeshComponentCount);
	ComponentsObj->SetNumberField(TEXT("hism_components"), HismComponentCount);
	ComponentsObj->SetNumberField(TEXT("hism_instances"), HismInstanceCount);
	Root->SetObjectField(TEXT("component_summary"), ComponentsObj);

	Root->SetStringField(TEXT("status"), TEXT("OBSERVED"));
	Root->SetArrayField(TEXT("errors"), Errors);
	return WriteInspectionObject(TEXT("inspect_visual_scene_state"), Root);
}

FString UAnastasisWorldProbeSubsystem::VerifyWorldContract()
{
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> Checks;
	Root->SetStringField(TEXT("schema"), TEXT("anastasis.verify_world_contract.v1"));
	Root->SetStringField(TEXT("operation"), TEXT("verify_world_contract"));
	Root->SetObjectField(TEXT("evidence_scope"), VerificationScopeObject(TEXT("verify_world_contract")));

	int32 PassCount = 0;
	int32 FailCount = 0;
	int32 UnknownCount = 0;
	auto Record = [&Checks, &PassCount, &FailCount, &UnknownCount](const TCHAR* Code, const TCHAR* Status, const FString& Detail)
	{
		AddCheck(Checks, Code, Status, Detail);
		if (FCString::Strcmp(Status, TEXT("PASS")) == 0) { ++PassCount; }
		else if (FCString::Strcmp(Status, TEXT("FAIL")) == 0) { ++FailCount; }
		else { ++UnknownCount; }
	};

	UWorld* World = GetWorld();
	if (!World)
	{
		Record(TEXT("world_context"), TEXT("UNKNOWN"), TEXT("No UWorld is available."));
		Root->SetArrayField(TEXT("checks"), Checks);
		SetCounts(Root, PassCount, FailCount, UnknownCount);
		return WriteVerificationObject(TEXT("verify_world_contract"), Root);
	}
	Record(TEXT("world_context"), TEXT("PASS"), FString::Printf(TEXT("World '%s' is available."), *World->GetMapName()));

	AAnastasisWorldEmbodiment* Embodiment = FindEmbodiment();
	if (!Embodiment)
	{
		Record(TEXT("world_embodiment"), TEXT("UNKNOWN"), TEXT("No AAnastasisWorldEmbodiment is present; worldgen delivery cannot be verified."));
		Root->SetArrayField(TEXT("checks"), Checks);
		SetCounts(Root, PassCount, FailCount, UnknownCount);
		return WriteVerificationObject(TEXT("verify_world_contract"), Root);
	}
	Record(TEXT("world_embodiment"), TEXT("PASS"), TEXT("AAnastasisWorldEmbodiment is present."));

	const AnastasisWorldView::FPlan& Plan = Embodiment->GetPlan();
	const bool bPositiveDimensions = Plan.W > 0 && Plan.H > 0 && Plan.SourceW > 0 && Plan.SourceH > 0;
	Record(TEXT("positive_dimensions"), bPositiveDimensions ? TEXT("PASS") : TEXT("FAIL"),
		FString::Printf(TEXT("source=%dx%d crop=%dx%d"), Plan.SourceW, Plan.SourceH, Plan.W, Plan.H));

	const int32 ExpectedTileCount = Plan.W * Plan.H;
	const bool bTileCountMatches = Plan.TileCount > 0
		&& Plan.TileCount == ExpectedTileCount
		&& Plan.Locations.Num() == Plan.TileCount
		&& Plan.Types.Num() == Plan.TileCount
		&& Plan.Alts.Num() == Plan.TileCount;
	Record(TEXT("tile_arrays_consistent"), bTileCountMatches ? TEXT("PASS") : TEXT("FAIL"),
		FString::Printf(TEXT("tiles=%d expected=%d locations=%d types=%d alts=%d"),
			Plan.TileCount, ExpectedTileCount, Plan.Locations.Num(), Plan.Types.Num(), Plan.Alts.Num()));

	int32 CountSum = 0;
	for (int32 TypeIndex = 0; TypeIndex < AnastasisWorld::TileTypeCount; ++TypeIndex)
	{
		CountSum += Plan.TerrainCounts[TypeIndex];
	}
	Record(TEXT("terrain_counts_sum"), CountSum == Plan.TileCount ? TEXT("PASS") : TEXT("FAIL"),
		FString::Printf(TEXT("sum=%d tiles=%d"), CountSum, Plan.TileCount));

	bool bFinite = FMath::IsFinite(Plan.MinAlt) && FMath::IsFinite(Plan.MaxAlt);
	for (const FVector& Location : Plan.Locations)
	{
		if (!IsFiniteVector(Location))
		{
			bFinite = false;
			break;
		}
	}
	for (const double Alt : Plan.Alts)
	{
		if (!FMath::IsFinite(Alt))
		{
			bFinite = false;
			break;
		}
	}
	Record(TEXT("finite_coordinates"), bFinite ? TEXT("PASS") : TEXT("FAIL"), TEXT("Plan altitudes and Unreal locations are finite."));

	const bool bHasLand = Plan.TerrainCounts[static_cast<uint8>(AnastasisWorld::ETileType::Grass)] > 0
		|| Plan.TerrainCounts[static_cast<uint8>(AnastasisWorld::ETileType::Forest)] > 0
		|| Plan.TerrainCounts[static_cast<uint8>(AnastasisWorld::ETileType::Field)] > 0
		|| Plan.TerrainCounts[static_cast<uint8>(AnastasisWorld::ETileType::Scrub)] > 0
		|| Plan.TerrainCounts[static_cast<uint8>(AnastasisWorld::ETileType::Stone)] > 0
		|| Plan.TerrainCounts[static_cast<uint8>(AnastasisWorld::ETileType::Ruin)] > 0;
	Record(TEXT("has_land_material"), bHasLand ? TEXT("PASS") : TEXT("FAIL"), TEXT("World contains at least one non-water terrain category."));

	const FBox& Bounds = Embodiment->GetActiveFootprintBounds();
	Record(TEXT("active_visual_bounds"), Bounds.IsValid ? TEXT("PASS") : TEXT("FAIL"), Bounds.IsValid ? TEXT("Active footprint bounds are valid.") : TEXT("Active footprint bounds are invalid."));

	Root->SetArrayField(TEXT("checks"), Checks);
	SetCounts(Root, PassCount, FailCount, UnknownCount);
	return WriteVerificationObject(TEXT("verify_world_contract"), Root);
}

FString UAnastasisWorldProbeSubsystem::VerifySettlementContract(const FString& SettlementId)
{
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> Checks;
	Root->SetStringField(TEXT("schema"), TEXT("anastasis.verify_settlement_contract.v1"));
	Root->SetStringField(TEXT("operation"), TEXT("verify_settlement_contract"));
	Root->SetStringField(TEXT("settlement_id"), SettlementId);
	Root->SetObjectField(TEXT("evidence_scope"), VerificationScopeObject(TEXT("verify_settlement_contract")));

	int32 PassCount = 0;
	int32 FailCount = 0;
	int32 UnknownCount = 0;
	auto Record = [&Checks, &PassCount, &FailCount, &UnknownCount](const TCHAR* Code, const TCHAR* Status, const FString& Detail)
	{
		AddCheck(Checks, Code, Status, Detail);
		if (FCString::Strcmp(Status, TEXT("PASS")) == 0) { ++PassCount; }
		else if (FCString::Strcmp(Status, TEXT("FAIL")) == 0) { ++FailCount; }
		else { ++UnknownCount; }
	};

	Record(TEXT("canonical_settlement_runtime"), TEXT("UNKNOWN"), TEXT("No canonical settlement/building runtime model is implemented in Unreal yet."));
	Record(TEXT("housing_population_resource_contract"), TEXT("UNKNOWN"), TEXT("Settlement causal fields are unavailable in this Unreal module."));
	Record(TEXT("visual_delivery"), TEXT("UNKNOWN"), TEXT("Actor candidates, if any, do not prove a canonical settlement."));
	Root->SetStringField(TEXT("claim_boundary"), TEXT("UNKNOWN is intentional: settlement verification cannot pass until a canonical Unreal settlement owner exists."));

	Root->SetArrayField(TEXT("checks"), Checks);
	SetCounts(Root, PassCount, FailCount, UnknownCount);
	return WriteVerificationObject(TEXT("verify_settlement_contract"), Root);
}

FString UAnastasisWorldProbeSubsystem::VerifyNavigationContract()
{
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> Checks;
	Root->SetStringField(TEXT("schema"), TEXT("anastasis.verify_navigation_contract.v1"));
	Root->SetStringField(TEXT("operation"), TEXT("verify_navigation_contract"));
	Root->SetObjectField(TEXT("evidence_scope"), VerificationScopeObject(TEXT("verify_navigation_contract")));

	int32 PassCount = 0;
	int32 FailCount = 0;
	int32 UnknownCount = 0;
	auto Record = [&Checks, &PassCount, &FailCount, &UnknownCount](const TCHAR* Code, const TCHAR* Status, const FString& Detail)
	{
		AddCheck(Checks, Code, Status, Detail);
		if (FCString::Strcmp(Status, TEXT("PASS")) == 0) { ++PassCount; }
		else if (FCString::Strcmp(Status, TEXT("FAIL")) == 0) { ++FailCount; }
		else { ++UnknownCount; }
	};

	UWorld* World = GetWorld();
	if (!World)
	{
		Record(TEXT("world_context"), TEXT("UNKNOWN"), TEXT("No UWorld is available."));
		Root->SetArrayField(TEXT("checks"), Checks);
		SetCounts(Root, PassCount, FailCount, UnknownCount);
		return WriteVerificationObject(TEXT("verify_navigation_contract"), Root);
	}
	Record(TEXT("world_context"), TEXT("PASS"), FString::Printf(TEXT("World '%s' is available."), *World->GetMapName()));

	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	if (!NavSys)
	{
		Record(TEXT("navigation_system"), TEXT("UNKNOWN"), TEXT("NavigationSystem is not present in this world context."));
		Root->SetArrayField(TEXT("checks"), Checks);
		SetCounts(Root, PassCount, FailCount, UnknownCount);
		return WriteVerificationObject(TEXT("verify_navigation_contract"), Root);
	}
	Record(TEXT("navigation_system"), TEXT("PASS"), TEXT("NavigationSystem is present."));

	const ANavigationData* NavData = NavSys->GetDefaultNavDataInstance(FNavigationSystem::DontCreate);
	Record(TEXT("nav_data"), NavData ? TEXT("PASS") : TEXT("FAIL"), NavData ? TEXT("Default nav data exists.") : TEXT("Default nav data is missing."));

	if (!NavData)
	{
		Record(TEXT("nav_built"), TEXT("UNKNOWN"), TEXT("Navigation build state is not meaningful without default nav data."));
	}
	else
	{
		const AWorldSettings* Settings = World->GetWorldSettings();
		if (!Settings)
		{
			Record(TEXT("world_settings"), TEXT("UNKNOWN"), TEXT("WorldSettings unavailable; nav build state cannot be verified."));
		}
		else
		{
			const bool bBuilt = NavSys->IsNavigationBuilt(Settings);
			Record(TEXT("nav_built"), bBuilt ? TEXT("PASS") : TEXT("FAIL"), bBuilt ? TEXT("Navigation is built.") : TEXT("Navigation is present but not built."));
		}
	}

	Root->SetArrayField(TEXT("checks"), Checks);
	SetCounts(Root, PassCount, FailCount, UnknownCount);
	return WriteVerificationObject(TEXT("verify_navigation_contract"), Root);
}

FString UAnastasisWorldProbeSubsystem::VerifyVisualDelivery()
{
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> Checks;
	Root->SetStringField(TEXT("schema"), TEXT("anastasis.verify_visual_delivery.v1"));
	Root->SetStringField(TEXT("operation"), TEXT("verify_visual_delivery"));
	Root->SetObjectField(TEXT("evidence_scope"), VerificationScopeObject(TEXT("verify_visual_delivery")));

	int32 PassCount = 0;
	int32 FailCount = 0;
	int32 UnknownCount = 0;
	auto Record = [&Checks, &PassCount, &FailCount, &UnknownCount](const TCHAR* Code, const TCHAR* Status, const FString& Detail)
	{
		AddCheck(Checks, Code, Status, Detail);
		if (FCString::Strcmp(Status, TEXT("PASS")) == 0) { ++PassCount; }
		else if (FCString::Strcmp(Status, TEXT("FAIL")) == 0) { ++FailCount; }
		else { ++UnknownCount; }
	};

	UWorld* World = GetWorld();
	if (!World)
	{
		Record(TEXT("mec_world_context"), TEXT("UNKNOWN"), TEXT("No UWorld is available."));
		Root->SetArrayField(TEXT("checks"), Checks);
		SetCounts(Root, PassCount, FailCount, UnknownCount);
		return WriteVerificationObject(TEXT("verify_visual_delivery"), Root);
	}
	Record(TEXT("mec_world_context"), TEXT("PASS"), FString::Printf(TEXT("World '%s' is available."), *World->GetMapName()));

	AAnastasisWorldEmbodiment* Embodiment = FindEmbodiment();
	const bool bHasMechanicalTerrain = Embodiment && Embodiment->GetPlan().TileCount > 0 && (Embodiment->GetInstanceCount() > 0 || Embodiment->HasWaterSurface());
	Record(TEXT("mec_terrain_component_delivery"), bHasMechanicalTerrain ? TEXT("PASS") : TEXT("FAIL"),
		bHasMechanicalTerrain ? TEXT("Embodiment has a non-empty delivered terrain representation.") : TEXT("No non-empty terrain representation is delivered."));

	const bool bCanRender = FApp::CanEverRender();
	Record(TEXT("scn_render_context"), bCanRender ? TEXT("PASS") : TEXT("UNKNOWN"),
		bCanRender ? TEXT("Engine can render in this process.") : TEXT("Current process cannot render, e.g. -nullrhi/headless."));

	const bool bCapturePresent = HasAnyCaptureArtifact();
	Record(TEXT("scn_capture_artifact"), bCapturePresent ? TEXT("PASS") : TEXT("UNKNOWN"),
		bCapturePresent ? TEXT("At least one capture artifact exists under Saved/Anastasis/Captures.") : TEXT("No paired capture artifact found."));

	Record(TEXT("ply_player_verdict"), TEXT("UNKNOWN"), TEXT("No human/player acceptance verdict is encoded by this verifier."));

	Root->SetArrayField(TEXT("checks"), Checks);
	SetCounts(Root, PassCount, FailCount, UnknownCount);
	return WriteVerificationObject(TEXT("verify_visual_delivery"), Root);
}

FString UAnastasisWorldProbeSubsystem::VerifySemanticSlice(int32 OriginX, int32 OriginY, int32 Size)
{
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> Checks;
	Root->SetStringField(TEXT("schema"), TEXT("anastasis.verify_semantic_slice.v1"));
	Root->SetStringField(TEXT("operation"), TEXT("verify_semantic_slice"));
	Root->SetObjectField(TEXT("evidence_scope"), VerificationScopeObject(TEXT("verify_semantic_slice")));

	TSharedRef<FJsonObject> Query = MakeShared<FJsonObject>();
	Query->SetNumberField(TEXT("origin_x"), OriginX);
	Query->SetNumberField(TEXT("origin_y"), OriginY);
	Query->SetNumberField(TEXT("size"), Size);
	Root->SetObjectField(TEXT("query"), Query);

	int32 PassCount = 0;
	int32 FailCount = 0;
	int32 UnknownCount = 0;
	auto Record = [&Checks, &PassCount, &FailCount, &UnknownCount](const TCHAR* Code, const TCHAR* Status, const FString& Detail)
	{
		AddCheck(Checks, Code, Status, Detail);
		if (FCString::Strcmp(Status, TEXT("PASS")) == 0) { ++PassCount; }
		else if (FCString::Strcmp(Status, TEXT("FAIL")) == 0) { ++FailCount; }
		else { ++UnknownCount; }
	};

	if (Size <= 0)
	{
		Record(TEXT("valid_size"), TEXT("FAIL"), FString::Printf(TEXT("Invalid size=%d."), Size));
		Root->SetArrayField(TEXT("checks"), Checks);
		SetCounts(Root, PassCount, FailCount, UnknownCount);
		return WriteVerificationObject(TEXT("verify_semantic_slice"), Root);
	}
	Record(TEXT("valid_size"), TEXT("PASS"), FString::Printf(TEXT("size=%d"), Size));

	AAnastasisWorldEmbodiment* Embodiment = FindEmbodiment();
	if (!Embodiment)
	{
		Record(TEXT("world_embodiment"), TEXT("UNKNOWN"), TEXT("No AAnastasisWorldEmbodiment is present; no embodied slice can be checked."));
		Root->SetArrayField(TEXT("checks"), Checks);
		SetCounts(Root, PassCount, FailCount, UnknownCount);
		return WriteVerificationObject(TEXT("verify_semantic_slice"), Root);
	}
	Record(TEXT("world_embodiment"), TEXT("PASS"), TEXT("AAnastasisWorldEmbodiment is present."));

	const AnastasisWorldView::FWorldVisualSnapshot& Snapshot = Embodiment->GetSnapshot();
	const bool bInside = IsInsideSnapshot(Snapshot, OriginX, OriginY)
		&& IsInsideSnapshot(Snapshot, OriginX + Size - 1, OriginY + Size - 1);
	Record(TEXT("inside_embodied_snapshot"), bInside ? TEXT("PASS") : TEXT("FAIL"),
		FString::Printf(TEXT("snapshot origin=(%d,%d) size=%dx%d"), Snapshot.OriginX, Snapshot.OriginY, Snapshot.W, Snapshot.H));

	int32 Water = 0;
	int32 Forest = 0;
	int32 Field = 0;
	int32 Clearing = 0;
	int32 ShoreContacts = 0;
	if (bInside)
	{
		CountSemanticSlice(Snapshot, OriginX, OriginY, Size, Water, Forest, Field, Clearing, ShoreContacts);
	}

	TSharedRef<FJsonObject> Metrics = MakeShared<FJsonObject>();
	Metrics->SetNumberField(TEXT("water"), Water);
	Metrics->SetNumberField(TEXT("forest"), Forest);
	Metrics->SetNumberField(TEXT("field"), Field);
	Metrics->SetNumberField(TEXT("clearing"), Clearing);
	Metrics->SetNumberField(TEXT("shore_contacts"), ShoreContacts);
	Root->SetObjectField(TEXT("metrics"), Metrics);

	if (bInside)
	{
		Record(TEXT("has_water"), Water > 0 ? TEXT("PASS") : TEXT("FAIL"), FString::Printf(TEXT("water=%d"), Water));
		Record(TEXT("has_forest"), Forest > 0 ? TEXT("PASS") : TEXT("FAIL"), FString::Printf(TEXT("forest=%d"), Forest));
		Record(TEXT("has_field"), Field > 0 ? TEXT("PASS") : TEXT("FAIL"), FString::Printf(TEXT("field=%d"), Field));
		Record(TEXT("has_clearing"), Clearing > 0 ? TEXT("PASS") : TEXT("FAIL"), FString::Printf(TEXT("clearing=%d"), Clearing));
		Record(TEXT("has_shore_contact"), ShoreContacts > 0 ? TEXT("PASS") : TEXT("FAIL"), FString::Printf(TEXT("shore_contacts=%d"), ShoreContacts));
	}

	Root->SetArrayField(TEXT("checks"), Checks);
	SetCounts(Root, PassCount, FailCount, UnknownCount);
	return WriteVerificationObject(TEXT("verify_semantic_slice"), Root);
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
