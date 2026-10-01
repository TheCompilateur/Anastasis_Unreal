#include "Anastasis_UnrealV2.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"

/**
 * PRESENTATION / DEVELOPMENT (ATMOSPHERE_COHERENCE_001).
 *
 * Nothing new is drawn or hidden here: every developer overlay in the project already has its
 * own switch, and this command only flips those switches together and puts them back.
 *
 *   anastasis.Sim.Overlay      cyan clock line (day, hour, seed)            -> 0
 *   anastasis.Village.Debug    village debug boxes, strings and lines       -> 0
 *   anastasis.Drainage.Debug   drainage network lines (read on embodiment)  -> 0
 *   DisableAllScreenMessages   engine on-screen messages, renderer warnings included
 *
 * The values found on entering PRESENTATION are remembered and written back on leaving it, so
 * a developer who had turned one overlay off finds it off again. Village.Debug belongs to the
 * village lane: its value is only set, its code is not touched. A switch that no longer exists
 * is reported and skipped, never invented.
 */
namespace AnastasisPresentationMode
{
	const TCHAR* const PresentationSwitches[] = {
		TEXT("anastasis.Sim.Overlay"),
		TEXT("anastasis.Village.Debug"),
		TEXT("anastasis.Drainage.Debug"),
	};

	bool GPresentation = false;
	TMap<FString, int32> GSavedSwitches;

	void SetPresentation(UWorld* World, const bool bOn)
	{
		if (bOn == GPresentation)
		{
			UE_LOG(LogAnastasis_UnrealV2, Display, TEXT("ANASTASIS_PRESENTATION mode=%s unchanged"),
				bOn ? TEXT("PRESENTATION") : TEXT("DEVELOPMENT"));
			return;
		}

		FString Changed;
		for (const TCHAR* Name : PresentationSwitches)
		{
			IConsoleVariable* Var = IConsoleManager::Get().FindConsoleVariable(Name);
			if (!Var)
			{
				UE_LOG(LogAnastasis_UnrealV2, Warning, TEXT("ANASTASIS_PRESENTATION missing_switch=%s"), Name);
				continue;
			}
			int32 Value = 0;
			if (bOn)
			{
				GSavedSwitches.Add(Name, Var->GetInt());
			}
			else if (const int32* Saved = GSavedSwitches.Find(Name))
			{
				Value = *Saved;
			}
			else
			{
				continue;
			}
			// SetByConsole: the same priority a developer typing the command would have.
			Var->Set(Value, ECVF_SetByConsole);
			Changed += FString::Printf(TEXT("%s%s=%d"), Changed.IsEmpty() ? TEXT("") : TEXT(","), Name, Value);
		}
		if (!bOn)
		{
			GSavedSwitches.Reset();
		}

		if (GEngine)
		{
			GEngine->Exec(World, bOn ? TEXT("DisableAllScreenMessages") : TEXT("EnableAllScreenMessages"));
		}
		GPresentation = bOn;
		UE_LOG(LogAnastasis_UnrealV2, Display, TEXT("ANASTASIS_PRESENTATION mode=%s switches=%s screen_messages=%d"),
			bOn ? TEXT("PRESENTATION") : TEXT("DEVELOPMENT"), *Changed, bOn ? 0 : 1);
	}
}

static FAutoConsoleCommandWithWorldAndArgs CmdAnastasisPresentation(
	TEXT("Anastasis.Presentation"),
	TEXT("1 = PRESENTATION (clean image for screenshots and video: simulation overlay, village and drainage debug, engine screen messages off), ")
	TEXT("0 = DEVELOPMENT (every switch back to the value it had). No argument toggles."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		const bool bOn = Args.Num() > 0 ? FCString::Atoi(*Args[0]) != 0 : !AnastasisPresentationMode::GPresentation;
		AnastasisPresentationMode::SetPresentation(World, bOn);
	}));
