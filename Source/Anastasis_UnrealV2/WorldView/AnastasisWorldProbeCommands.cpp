// Anastasis.World.* console commands: thin bindings onto UAnastasisWorldProbeSubsystem.
// No logic lives here — this file only resolves the per-world subsystem and forwards args.

#include "Anastasis_UnrealV2.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "WorldView/AnastasisWorldProbeSubsystem.h"

namespace
{
	UAnastasisWorldProbeSubsystem* ResolveProbe(UWorld* World)
	{
		return World ? World->GetSubsystem<UAnastasisWorldProbeSubsystem>() : nullptr;
	}
}

static FAutoConsoleCommandWithWorld CmdAnastasisWorldStatus(
	TEXT("Anastasis.World.Status"),
	TEXT("Logs a one-line summary of the current world state: map, actor count, terrain, camera, nav."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if (UAnastasisWorldProbeSubsystem* Probe = ResolveProbe(World))
		{
			Probe->LogStatus();
		}
	}));

static FAutoConsoleCommandWithWorld CmdAnastasisWorldSnapshot(
	TEXT("Anastasis.World.Snapshot"),
	TEXT("Writes a structured JSON world snapshot to Saved/Anastasis/Diagnostics/<timestamp>.json (+ latest.json)."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if (UAnastasisWorldProbeSubsystem* Probe = ResolveProbe(World))
		{
			Probe->WriteSnapshot();
		}
	}));

static FAutoConsoleCommandWithWorld CmdAnastasisWorldNavStatus(
	TEXT("Anastasis.World.NavStatus"),
	TEXT("Logs NavigationSystem presence and build state."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if (UAnastasisWorldProbeSubsystem* Probe = ResolveProbe(World))
		{
			Probe->LogNavStatus();
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs CmdAnastasisWorldBookmark(
	TEXT("Anastasis.World.Bookmark"),
	TEXT("Anastasis.World.Bookmark <name> — records the current camera pose under <name>."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (Args.Num() < 1)
		{
			UE_LOG(LogAnastasis_UnrealV2, Warning, TEXT("Anastasis.World.Bookmark requires a <name> argument"));
			return;
		}
		if (UAnastasisWorldProbeSubsystem* Probe = ResolveProbe(World))
		{
			Probe->SetBookmarkFromCurrentCamera(FName(*Args[0]));
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs CmdAnastasisWorldGotoBookmark(
	TEXT("Anastasis.World.GotoBookmark"),
	TEXT("Anastasis.World.GotoBookmark <name> — moves the probe camera to a bookmark (OVERVIEW, GROUND, FOREST, SHORE, SETTLEMENT, or a custom one)."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (Args.Num() < 1)
		{
			UE_LOG(LogAnastasis_UnrealV2, Warning, TEXT("Anastasis.World.GotoBookmark requires a <name> argument"));
			return;
		}
		if (UAnastasisWorldProbeSubsystem* Probe = ResolveProbe(World))
		{
			FString Reason;
			if (!Probe->GotoBookmark(FName(*Args[0]), Reason))
			{
				UE_LOG(LogAnastasis_UnrealV2, Warning, TEXT("GOTO::NOT_REACHABLE bookmark=%s reason=%s"), *Args[0], *Reason);
			}
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs CmdAnastasisWorldCapture(
	TEXT("Anastasis.World.Capture"),
	TEXT("Anastasis.World.Capture <bookmark> [mission] — goto bookmark, capture a screenshot, and write a paired JSON snapshot under Saved/Anastasis/Captures/<mission>/<bookmark>/."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (Args.Num() < 1)
		{
			UE_LOG(LogAnastasis_UnrealV2, Warning, TEXT("Anastasis.World.Capture requires a <bookmark> argument"));
			return;
		}
		if (UAnastasisWorldProbeSubsystem* Probe = ResolveProbe(World))
		{
			Probe->RequestCapture(FName(*Args[0]), Args.Num() > 1 ? Args[1] : FString());
		}
	}));

static FAutoConsoleCommandWithWorld CmdAnastasisInspectWorld(
	TEXT("Anastasis.Inspect.World"),
	TEXT("Writes a read-only agent-facing world inspection JSON: inspect_anastasis_world."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if (UAnastasisWorldProbeSubsystem* Probe = ResolveProbe(World))
		{
			Probe->InspectWorld();
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs CmdAnastasisInspectTile(
	TEXT("Anastasis.Inspect.Tile"),
	TEXT("Anastasis.Inspect.Tile <x> <y> — writes read-only tile facts for canonical/source tile coordinates."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (Args.Num() < 2)
		{
			UE_LOG(LogAnastasis_UnrealV2, Warning, TEXT("Anastasis.Inspect.Tile requires <x> <y> arguments"));
			return;
		}
		if (UAnastasisWorldProbeSubsystem* Probe = ResolveProbe(World))
		{
			Probe->InspectTile(FCString::Atoi(*Args[0]), FCString::Atoi(*Args[1]));
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs CmdAnastasisInspectSettlement(
	TEXT("Anastasis.Inspect.Settlement"),
	TEXT("Anastasis.Inspect.Settlement [id] — reports settlement observability honestly; currently NOT_IMPLEMENTED in Unreal."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (UAnastasisWorldProbeSubsystem* Probe = ResolveProbe(World))
		{
			Probe->InspectSettlement(Args.Num() > 0 ? Args[0] : FString());
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs CmdAnastasisInspectActor(
	TEXT("Anastasis.Inspect.Actor"),
	TEXT("Anastasis.Inspect.Actor <query> — writes read-only actor facts for name/class/tag matches."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (Args.Num() < 1)
		{
			UE_LOG(LogAnastasis_UnrealV2, Warning, TEXT("Anastasis.Inspect.Actor requires a <query> argument"));
			return;
		}
		if (UAnastasisWorldProbeSubsystem* Probe = ResolveProbe(World))
		{
			Probe->InspectActor(FString::Join(Args, TEXT(" ")));
		}
	}));

static FAutoConsoleCommandWithWorld CmdAnastasisInspectVisualSceneState(
	TEXT("Anastasis.Inspect.VisualSceneState"),
	TEXT("Writes read-only visual scene/component delivery facts: inspect_visual_scene_state."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if (UAnastasisWorldProbeSubsystem* Probe = ResolveProbe(World))
		{
			Probe->InspectVisualSceneState();
		}
	}));

static FAutoConsoleCommandWithWorld CmdAnastasisVerifyWorldContract(
	TEXT("Anastasis.Verify.WorldContract"),
	TEXT("Writes PASS/FAIL/UNKNOWN verification JSON for core Anastasis world invariants."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if (UAnastasisWorldProbeSubsystem* Probe = ResolveProbe(World))
		{
			Probe->VerifyWorldContract();
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs CmdAnastasisVerifySettlementContract(
	TEXT("Anastasis.Verify.SettlementContract"),
	TEXT("Anastasis.Verify.SettlementContract [id] — writes honest settlement verification JSON; UNKNOWN until implemented."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (UAnastasisWorldProbeSubsystem* Probe = ResolveProbe(World))
		{
			Probe->VerifySettlementContract(Args.Num() > 0 ? Args[0] : FString());
		}
	}));

static FAutoConsoleCommandWithWorld CmdAnastasisVerifyNavigationContract(
	TEXT("Anastasis.Verify.NavigationContract"),
	TEXT("Writes PASS/FAIL/UNKNOWN verification JSON for NavigationSystem/NavData/build state."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if (UAnastasisWorldProbeSubsystem* Probe = ResolveProbe(World))
		{
			Probe->VerifyNavigationContract();
		}
	}));

static FAutoConsoleCommandWithWorld CmdAnastasisVerifyVisualDelivery(
	TEXT("Anastasis.Verify.VisualDelivery"),
	TEXT("Writes verification JSON that separates MEC terrain delivery from SCN/PLY evidence."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if (UAnastasisWorldProbeSubsystem* Probe = ResolveProbe(World))
		{
			Probe->VerifyVisualDelivery();
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs CmdAnastasisVerifySemanticSlice(
	TEXT("Anastasis.Verify.SemanticSlice"),
	TEXT("Anastasis.Verify.SemanticSlice [x y size] — verifies semantic slice richness as PASS/FAIL/UNKNOWN."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		int32 OriginX = 0;
		int32 OriginY = 0;
		int32 Size = 32;
		if (Args.Num() > 0)
		{
			OriginX = FCString::Atoi(*Args[0]);
		}
		if (Args.Num() > 1)
		{
			OriginY = FCString::Atoi(*Args[1]);
		}
		if (Args.Num() > 2)
		{
			Size = FCString::Atoi(*Args[2]);
		}
		if (UAnastasisWorldProbeSubsystem* Probe = ResolveProbe(World))
		{
			Probe->VerifySemanticSlice(OriginX, OriginY, Size);
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs CmdAnastasisProbeWorldgen(
	TEXT("Anastasis.Probe.Worldgen"),
	TEXT("Anastasis.Probe.Worldgen [seed sourceW sourceH hour profile] - deterministic worldgen probe JSON."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		uint32 Seed = 12345u;
		int32 SourceW = 96;
		int32 SourceH = 96;
		double Hour = 12.0;
		FString Profile = TEXT("canonical");
		if (Args.Num() > 0) { Seed = static_cast<uint32>(FCString::Atoi(*Args[0])); }
		if (Args.Num() > 1) { SourceW = FCString::Atoi(*Args[1]); }
		if (Args.Num() > 2) { SourceH = FCString::Atoi(*Args[2]); }
		if (Args.Num() > 3) { Hour = FCString::Atod(*Args[3]); }
		if (Args.Num() > 4) { Profile = Args[4]; }
		if (UAnastasisWorldProbeSubsystem* Probe = ResolveProbe(World))
		{
			Probe->RunWorldgenProbe(Seed, SourceW, SourceH, Hour, Profile);
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs CmdAnastasisProbeFindSemanticSlice(
	TEXT("Anastasis.Probe.FindSemanticSlice"),
	TEXT("Anastasis.Probe.FindSemanticSlice [seed size profile] - deterministic best semantic slice search."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		uint32 Seed = 12345u;
		int32 Size = 32;
		FString Profile = TEXT("canonical");
		if (Args.Num() > 0) { Seed = static_cast<uint32>(FCString::Atoi(*Args[0])); }
		if (Args.Num() > 1) { Size = FCString::Atoi(*Args[1]); }
		if (Args.Num() > 2) { Profile = Args[2]; }
		if (UAnastasisWorldProbeSubsystem* Probe = ResolveProbe(World))
		{
			Probe->FindSemanticSlice(Seed, Size, Profile);
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs CmdAnastasisProbeCompareSliceCandidates(
	TEXT("Anastasis.Probe.CompareSliceCandidates"),
	TEXT("Anastasis.Probe.CompareSliceCandidates [seed size x,y;x,y profile] - compare deterministic slice candidates."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		uint32 Seed = 12345u;
		int32 Size = 32;
		FString CandidateSpec = TEXT("0,0");
		FString Profile = TEXT("canonical");
		if (Args.Num() > 0) { Seed = static_cast<uint32>(FCString::Atoi(*Args[0])); }
		if (Args.Num() > 1) { Size = FCString::Atoi(*Args[1]); }
		if (Args.Num() > 2) { CandidateSpec = Args[2]; }
		if (Args.Num() > 3) { Profile = Args[3]; }
		if (UAnastasisWorldProbeSubsystem* Probe = ResolveProbe(World))
		{
			Probe->CompareSliceCandidates(Seed, Size, CandidateSpec, Profile);
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs CmdAnastasisProbeCaptureFixedView(
	TEXT("Anastasis.Probe.CaptureFixedView"),
	TEXT("Anastasis.Probe.CaptureFixedView [seed x y size hour camera profile] - request fixed camera proof without claiming PLY."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		uint32 Seed = 12345u;
		int32 OriginX = 0;
		int32 OriginY = 0;
		int32 Size = 32;
		double Hour = 12.0;
		FString Camera = TEXT("OVERVIEW");
		FString Profile = TEXT("canonical");
		if (Args.Num() > 0) { Seed = static_cast<uint32>(FCString::Atoi(*Args[0])); }
		if (Args.Num() > 1) { OriginX = FCString::Atoi(*Args[1]); }
		if (Args.Num() > 2) { OriginY = FCString::Atoi(*Args[2]); }
		if (Args.Num() > 3) { Size = FCString::Atoi(*Args[3]); }
		if (Args.Num() > 4) { Hour = FCString::Atod(*Args[4]); }
		if (Args.Num() > 5) { Camera = Args[5]; }
		if (Args.Num() > 6) { Profile = Args[6]; }
		if (UAnastasisWorldProbeSubsystem* Probe = ResolveProbe(World))
		{
			Probe->CaptureFixedViewProbe(Seed, OriginX, OriginY, Size, Hour, Camera, Profile);
		}
	}));
