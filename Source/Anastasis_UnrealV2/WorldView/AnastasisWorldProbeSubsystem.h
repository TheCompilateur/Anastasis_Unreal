#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "AnastasisWorldProbeSubsystem.generated.h"

class AAnastasisWorldEmbodiment;
class ACameraActor;
class FJsonObject;

/** One deterministic observation viewpoint: position, rotation, FOV, stable name. */
struct FAnastasisCameraBookmark
{
	FName Name;
	FVector Location = FVector::ZeroVector;
	FRotator Rotation = FRotator::ZeroRotator;
	float FieldOfView = 90.0f;
	bool bReachable = false;
	FString UnreachableReason;
};

/**
 * WORLD PROBE OWNER.
 * Runtime developer observability: JSON world snapshot, camera bookmarks,
 * capture pipeline (screenshot + paired snapshot). Reads world state via
 * AAnastasisWorldEmbodiment / engine query APIs; does not own worldgen,
 * terrain, or gameplay. Driven by the Anastasis.World.* console commands
 * registered in AnastasisWorldProbeCommands.cpp.
 */
UCLASS()
class UAnastasisWorldProbeSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

	/** Anastasis.World.Status — one-line summary to the log. */
	void LogStatus();

	/** Anastasis.World.Snapshot — writes Saved/Anastasis/Diagnostics/<timestamp>.json (+ latest.json). Returns the written path, or empty on failure. */
	FString WriteSnapshot();

	/** Anastasis.World.Bookmark <name> — records the current camera pose under Name. */
	void SetBookmarkFromCurrentCamera(FName Name);

	/** Anastasis.World.GotoBookmark <name> — moves the probe camera to the bookmark. Returns false (+ OutReason) when NOT_REACHABLE. */
	bool GotoBookmark(FName Name, FString& OutReason);

	/** Anastasis.World.NavStatus — logs NavigationSystem presence and build state. */
	void LogNavStatus();

	/** Anastasis.World.Capture <bookmark> [mission] — goto + screenshot + paired snapshot JSON, under Saved/Anastasis/Captures/<mission>/<bookmark>/. Async; result is logged. */
	void RequestCapture(FName BookmarkName, const FString& MissionName);

	/** Anastasis.Inspect.World — writes a read-only, agent-facing world inspection JSON. */
	FString InspectWorld();

	/** Anastasis.Inspect.Tile <x> <y> — writes a read-only tile inspection JSON for canonical/source coordinates. */
	FString InspectTile(int32 TileX, int32 TileY);

	/** Anastasis.Inspect.Settlement [id] — reports settlement observability honestly; currently NOT_IMPLEMENTED in Unreal. */
	FString InspectSettlement(const FString& SettlementId);

	/** Anastasis.Inspect.Actor <query> — writes read-only actor facts for name/class/tag matches. */
	FString InspectActor(const FString& ActorQuery);

	/** Anastasis.Inspect.VisualSceneState — writes read-only visual scene/component delivery facts. */
	FString InspectVisualSceneState();

	/** Anastasis.Verify.WorldContract — verifies terrain/world invariants as PASS/FAIL/UNKNOWN. */
	FString VerifyWorldContract();

	/** Anastasis.Verify.SettlementContract [id] — verifies settlement delivery honestly; currently UNKNOWN until implemented. */
	FString VerifySettlementContract(const FString& SettlementId);

	/** Anastasis.Verify.NavigationContract — verifies NavigationSystem/NavData/build state as PASS/FAIL/UNKNOWN. */
	FString VerifyNavigationContract();

	/** Anastasis.Verify.VisualDelivery — separates mechanical delivery from SCN/PLY proof. */
	FString VerifyVisualDelivery();

	/** Anastasis.Verify.SemanticSlice [x y size] — verifies semantic richness of the embodied slice. */
	FString VerifySemanticSlice(int32 OriginX, int32 OriginY, int32 Size);

	/** Anastasis.Probe.Worldgen [seed sourceW sourceH hour profile] - deterministic worldgen metrics/fingerprint. */
	FString RunWorldgenProbe(uint32 Seed, int32 SourceW, int32 SourceH, double Hour, const FString& Profile);

	/** Anastasis.Probe.FindSemanticSlice [seed size profile] - ranked deterministic semantic slice search. */
	FString FindSemanticSlice(uint32 Seed, int32 Size, const FString& Profile);

	/** Anastasis.Probe.CompareSliceCandidates [seed size x,y;x,y profile] - ranked candidate comparison. */
	FString CompareSliceCandidates(uint32 Seed, int32 Size, const FString& CandidateSpec, const FString& Profile);

	/** Anastasis.Probe.CaptureFixedView [seed x y size hour camera profile] - fixed-view capture request/proof boundary. */
	FString CaptureFixedViewProbe(uint32 Seed, int32 OriginX, int32 OriginY, int32 Size, double Hour, const FString& CameraName, const FString& Profile);

private:
	void EnsureDefaultBookmarks();
	AAnastasisWorldEmbodiment* FindEmbodiment() const;
	ACameraActor* GetOrCreateProbeCamera();
	bool GetCurrentCameraPose(FVector& OutLocation, FRotator& OutRotation, float& OutFov, FString& OutReason) const;
	TSharedRef<FJsonObject> BuildSnapshotObject() const;
	FString WriteInspectionObject(const FString& Slug, const TSharedRef<FJsonObject>& Root) const;
	FString WriteVerificationObject(const FString& Slug, const TSharedRef<FJsonObject>& Root) const;
	FString WriteProbeObject(const FString& Slug, const TSharedRef<FJsonObject>& Root) const;
	void HandleScreenshotProcessed();
	void CancelPendingCapture(const TCHAR* Reason);

	TMap<FName, FAnastasisCameraBookmark> Bookmarks;
	TWeakObjectPtr<AAnastasisWorldEmbodiment> BookmarkSourceEmbodiment;

	UPROPERTY()
	TObjectPtr<ACameraActor> ProbeCamera;

	bool bCaptureInFlight = false;
	FDelegateHandle ScreenshotDelegateHandle;
	FString PendingCaptureShotPath;
	FString PendingCaptureJsonPath;
	FString PendingCaptureBookmarkName;
	FString PendingCaptureMissionName;
};
