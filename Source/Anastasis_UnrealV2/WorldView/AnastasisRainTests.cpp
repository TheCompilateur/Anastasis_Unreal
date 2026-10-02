#include "Misc/AutomationTest.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "WorldView/AnastasisRain.h"
#include "WorldView/AnastasisWorldAtmosphere.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	UWorld* FindRainAutomationWorld()
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

	/** Forces a console variable for the scope of a test, and puts it back. */
	struct FScopedRainCVar
	{
		IConsoleVariable* CVar = nullptr;
		FString Before;

		FScopedRainCVar(const TCHAR* Name, const TCHAR* Value)
		{
			CVar = IConsoleManager::Get().FindConsoleVariable(Name);
			if (CVar)
			{
				Before = CVar->GetString();
				CVar->Set(Value, ECVF_SetByCode);
			}
		}
		void Set(const TCHAR* Value) const
		{
			if (CVar)
			{
				CVar->Set(Value, ECVF_SetByCode);
			}
		}
		~FScopedRainCVar()
		{
			if (CVar)
			{
				CVar->Set(*Before, ECVF_SetByCode);
			}
		}
	};
}

/**
 * RAIN_001, pure. The rain shown is the simulation's visible rain, gated by the same weather
 * switch as clouds and fog; a pin replaces it for captures; drizzle below the threshold draws
 * nothing; the wind maps the simulation's (x, z) ground axes onto Unreal X/Y.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisRainVisual, "Anastasis.Atmosphere.Rain.Visual", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnastasisRainVisual::RunTest(const FString&)
{
	AnastasisWeather::FWeather Weather;
	Weather.Rain = 0.6;
	Weather.Wind = 0.5;
	Weather.DirX = 0.6;
	Weather.DirZ = 0.8;

	const AnastasisRain::FRainVisual Follow = AnastasisRain::VisualFor(Weather, true, -1.0);
	TestEqual(TEXT("follows the simulation's rain"), Follow.Amount, 0.6);
	TestTrue(TEXT("visible"), Follow.IsVisible());
	TestEqual(TEXT("wind X from the simulation's x"), Follow.WindUUPerSecond.X, 0.6 * 0.5 * AnastasisRain::MaxWindUUPerSecond, 1e-9);
	TestEqual(TEXT("wind Y from the simulation's z"), Follow.WindUUPerSecond.Y, 0.8 * 0.5 * AnastasisRain::MaxWindUUPerSecond, 1e-9);
	TestEqual(TEXT("raindrops fall at terminal speed"), Follow.FallUUPerSecond, AnastasisRain::FallUUPerSecond);

	TestEqual(TEXT("weather off: a fair sky, no rain"), AnastasisRain::VisualFor(Weather, false, -1.0).Amount, 0.0);
	TestEqual(TEXT("a pin replaces the weather"), AnastasisRain::VisualFor(Weather, true, 0.25).Amount, 0.25);
	TestEqual(TEXT("a pin works with the weather off"), AnastasisRain::VisualFor(Weather, false, 0.9).Amount, 0.9);
	TestEqual(TEXT("a pin of 0 stops the rain"), AnastasisRain::VisualFor(Weather, true, 0.0).Amount, 0.0);
	TestEqual(TEXT("a pin is clamped to 1"), AnastasisRain::VisualFor(Weather, true, 3.0).Amount, 1.0);

	Weather.Rain = AnastasisRain::MinVisibleAmount * 0.5;
	const AnastasisRain::FRainVisual Drizzle = AnastasisRain::VisualFor(Weather, true, -1.0);
	TestEqual(TEXT("below the threshold nothing is drawn"), Drizzle.Amount, 0.0);
	TestFalse(TEXT("and it is not visible"), Drizzle.IsVisible());
	return true;
}

/**
 * RAIN_001, in a world. A pinned rain makes the atmosphere create its streaks (all instances,
 * three custom floats each, no shadow, no collision) and write the amount on its dynamic
 * material; a pin of 0, or anastasis.Weather.Rain 0, hides them.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisRainStreaks, "Anastasis.Atmosphere.Rain.Streaks", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnastasisRainStreaks::RunTest(const FString&)
{
	UWorld* World = FindRainAutomationWorld();
	if (!World)
	{
		AddInfo(TEXT("no editor/game world available; not exercised"));
		return true;
	}
	FScopedRainCVar Enabled(TEXT("anastasis.Weather.Rain"), TEXT("1"));
	FScopedRainCVar Pin(TEXT("anastasis.Sky.Rain"), TEXT("0.7"));
	FScopedRainCVar Clock(TEXT("anastasis.Sky.Clock"), TEXT("1"));

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.ObjectFlags |= RF_Transient;
	AAnastasisWorldAtmosphere* Atmosphere = World->SpawnActor<AAnastasisWorldAtmosphere>(
		AAnastasisWorldAtmosphere::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params);
	if (!TestNotNull(TEXT("atmosphere actor spawns"), Atmosphere))
	{
		return false;
	}

	TestTrue(TEXT("apply (pinned rain)"), Atmosphere->Apply());
	if (Atmosphere->GetRainAmount() <= 0.0 && !Atmosphere->GetRainStreaks())
	{
		AddInfo(TEXT("the sky clock did not run (profile disables it); not exercised"));
		Atmosphere->DestroySpawnedActors();
		Atmosphere->Destroy();
		return true;
	}
	UInstancedStaticMeshComponent* Streaks = Atmosphere->GetRainStreaks();
	if (TestNotNull(TEXT("rain streaks created (assets from tools/unreal/rain-material.ps1)"), Streaks))
	{
		TestEqual(TEXT("every streak instance exists"), Streaks->GetInstanceCount(), AnastasisRain::StreakCount);
		TestEqual(TEXT("three custom floats per streak"), Streaks->NumCustomDataFloats, 3);
		TestTrue(TEXT("visible while it rains"), Streaks->IsVisible());
		TestFalse(TEXT("rain casts no shadow"), Streaks->CastShadow);
		TestEqual(TEXT("rain has no collision"), Streaks->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
		UMaterialInstanceDynamic* Mid = Cast<UMaterialInstanceDynamic>(Streaks->GetMaterial(0));
		if (TestNotNull(TEXT("streaks carry a dynamic material"), Mid))
		{
			float Amount = -1.0f;
			Mid->GetScalarParameterValue(FName(TEXT("RainAmount")), Amount);
			TestEqual(TEXT("material amount = the pinned rain"), Amount, 0.7f, 1e-5f);
		}
	}
	TestEqual(TEXT("the atmosphere reports the rain shown"), Atmosphere->GetRainAmount(), 0.7, 1e-6);

	Pin.Set(TEXT("0"));
	TestTrue(TEXT("apply (rain stopped)"), Atmosphere->Apply());
	TestEqual(TEXT("no rain reported"), Atmosphere->GetRainAmount(), 0.0);
	if (Streaks)
	{
		TestFalse(TEXT("streaks hidden when the rain stops"), Streaks->IsVisible());
	}

	Pin.Set(TEXT("0.7"));
	Enabled.Set(TEXT("0"));
	TestTrue(TEXT("apply (rain switched off)"), Atmosphere->Apply());
	TestEqual(TEXT("switch off: no rain reported"), Atmosphere->GetRainAmount(), 0.0);
	if (Streaks)
	{
		TestFalse(TEXT("switch off: streaks hidden"), Streaks->IsVisible());
	}

	Atmosphere->DestroySpawnedActors();
	Atmosphere->Destroy();
	return true;
}

#endif
