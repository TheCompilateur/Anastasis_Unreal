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
	TEXT("Anastasis.World.GotoBookmark <name> — moves the probe camera to a bookmark (OVERVIEW, GROUND, SHORE, SETTLEMENT, or a custom one)."),
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
