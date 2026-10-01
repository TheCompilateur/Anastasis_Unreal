#include "Misc/AutomationTest.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/Engine.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "WorldView/AnastasisAtmosphereProfile.h"
#include "WorldView/AnastasisAtmosphereResolver.h"
#include "WorldView/AnastasisSkyClock.h"
#include "WorldView/AnastasisWorldAtmosphere.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	UWorld* FindAtmosphereAutomationWorld()
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

	/**
	 * Actors of this class, the moon excepted. Since ENV_REALISM_001 the moon is a second
	 * DirectionalLight; "how many suns" has to mean suns.
	 */
	template <typename ActorType>
	int32 CountActors(UWorld* World)
	{
		int32 Count = 0;
		for (TActorIterator<ActorType> It(World); It; ++It)
		{
			if (IsValid(*It) && !It->ActorHasTag(AAnastasisWorldAtmosphere::MoonTag))
			{
				++Count;
			}
		}
		return Count;
	}

	template <typename ActorType>
	int32 CountTagged(UWorld* World, const FName Tag)
	{
		int32 Count = 0;
		for (TActorIterator<ActorType> It(World); It; ++It)
		{
			if (IsValid(*It) && It->ActorHasTag(Tag))
			{
				++Count;
			}
		}
		return Count;
	}

	/** Forces anastasis.Atmosphere.Realism for the scope of a test, and puts it back. */
	struct FScopedRealismCVar
	{
		IConsoleVariable* CVar = nullptr;
		int32 Before = 1;

		explicit FScopedRealismCVar(const int32 Value)
		{
			CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("anastasis.Atmosphere.Realism"));
			if (CVar)
			{
				Before = CVar->GetInt();
				CVar->Set(Value, ECVF_SetByCode);
			}
		}
		void Set(const int32 Value) const
		{
			if (CVar)
			{
				CVar->Set(Value, ECVF_SetByCode);
			}
		}
		~FScopedRealismCVar()
		{
			if (CVar)
			{
				CVar->Set(Before, ECVF_SetByCode);
			}
		}
	};
}

/**
 * The code defaults ARE tools/unreal/observe-slice.py's rig. That rig is the only lighting
 * ANASTASIS has ever taken comparable captures under, so this test is what makes it safe for
 * the atmosphere to start owning the light: if someone edits a default here, the observation
 * scene silently stops matching every capture in docs/unreal/evidence, and this fails first.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisAtmosphereRigParity, "Anastasis.Atmosphere.RigParity", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnastasisAtmosphereRigParity::RunTest(const FString&)
{
	UAnastasisAtmosphereProfile* Profile = UAnastasisAtmosphereProfile::CreateCodeDefaults(GetTransientPackage());
	if (!TestNotNull(TEXT("code defaults are never null"), Profile))
	{
		return false;
	}

	// observe-slice.py: unreal.Rotator(0.0, -38.0, -55.0) -> roll 0, pitch -38, yaw -55.
	TestEqual(TEXT("sun pitch matches the observation rig"), Profile->SunPitchDegrees, -38.0f);
	TestEqual(TEXT("sun yaw matches the observation rig"), Profile->SunYawDegrees, -55.0f);
	// observe-slice.py: SUN_LUX = 75000.0, EV100 = 14.0.
	TestEqual(TEXT("sun intensity matches SUN_LUX"), Profile->SunIntensityLux, 75000.0f);
	TestEqual(TEXT("exposure matches EV100"), Profile->ExposureEV100, 14.0f);
	TestTrue(TEXT("exposure stays fixed: two captures must stay comparable"), Profile->bFixedExposure);
	TestTrue(TEXT("sun lights the atmosphere, else the sky is black"), Profile->bSunIsAtmosphereLight);
	TestTrue(TEXT("sky light captures in real time"), Profile->bSkyLightRealTimeCapture);

	// The rig's own angles are used as-is: the derived sun stays opt-in until something in
	// ANASTASIS actually drives a time of day.
	TestFalse(TEXT("time-of-day derivation is off by default"), Profile->bDeriveSunFromTimeOfDay);
	const FRotator Resolved = AnastasisAtmosphere::ResolveSunRotation(*Profile);
	TestEqual(TEXT("resolved pitch is the explicit one"), static_cast<float>(Resolved.Pitch), -38.0f);
	TestEqual(TEXT("resolved yaw is the explicit one"), static_cast<float>(Resolved.Yaw), -55.0f);

	// Fog is this mission's one new visual element: present, and restrained.
	TestTrue(TEXT("fog is on"), Profile->bFogEnabled);
	TestTrue(TEXT("fog density is positive"), Profile->FogDensity > 0.0f);
	TestTrue(TEXT("fog never fully paints over the horizon"), Profile->FogMaxOpacity < 1.0f);
	TestTrue(TEXT("the near field stays clear"), Profile->FogStartDistance > 0.0f);

	return true;
}

/**
 * Solar geometry, not vibes. Locks the convention (UE X=north, Y=east, light points ALONG
 * the rays) and the physical facts a mirrored azimuth or an inverted pitch would break.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisAtmosphereSunFromTime, "Anastasis.Atmosphere.SunFromTime", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnastasisAtmosphereSunFromTime::RunTest(const FString&)
{
	using AnastasisAtmosphere::SunRotationForTimeOfDay;

	constexpr double Latitude = 41.0;   // Pontic coast, the profile's default
	constexpr double Equinox = 0.0;

	const FRotator Noon = SunRotationForTimeOfDay(12.0, Latitude, Equinox);
	const FRotator Morning = SunRotationForTimeOfDay(9.0, Latitude, Equinox);
	const FRotator Afternoon = SunRotationForTimeOfDay(15.0, Latitude, Equinox);
	const FRotator Midnight = SunRotationForTimeOfDay(0.0, Latitude, Equinox);

	// Determinism first: this feeds captures, so the same inputs must give the same light,
	// always. Not "close enough".
	const FRotator NoonAgain = SunRotationForTimeOfDay(12.0, Latitude, Equinox);
	TestTrue(TEXT("same inputs reproduce the same rotation exactly"), Noon == NoonAgain);

	// At equinox, solar noon at latitude L puts the sun at elevation 90-L, due south. The
	// light therefore points due north (yaw 0) and downward by that elevation.
	TestEqual(TEXT("noon elevation is 90 - latitude"), static_cast<double>(-Noon.Pitch), 90.0 - Latitude, 0.001);
	TestEqual(TEXT("noon sun is due south, so the light points due north"), static_cast<double>(FRotator::NormalizeAxis(Noon.Yaw)), 0.0, 0.001);

	// Sun higher at noon than at 9h or 15h; below the horizon at midnight.
	TestTrue(TEXT("noon is the highest sun"), Noon.Pitch < Morning.Pitch && Noon.Pitch < Afternoon.Pitch);
	TestTrue(TEXT("midnight sun is below the horizon"), -Midnight.Pitch < 0.0);

	// Symmetry about solar noon: 9h and 15h are the same height, mirrored east/west.
	TestEqual(TEXT("morning and afternoon are equally high"), static_cast<double>(Morning.Pitch), static_cast<double>(Afternoon.Pitch), 0.001);
	TestEqual(TEXT("their yaws mirror about north"),
		static_cast<double>(FRotator::NormalizeAxis(Morning.Yaw)),
		-static_cast<double>(FRotator::NormalizeAxis(Afternoon.Yaw)), 0.001);

	// The sun rises in the east: a morning sun in the south-east means light aimed
	// north-west, i.e. a negative yaw. This is the assertion that catches a mirrored azimuth.
	TestTrue(TEXT("morning light comes from the east"), FRotator::NormalizeAxis(Morning.Yaw) < 0.0);
	TestTrue(TEXT("afternoon light comes from the west"), FRotator::NormalizeAxis(Afternoon.Yaw) > 0.0);

	// Declination is the seasonal input and it has to move the sun the right way.
	const FRotator Summer = SunRotationForTimeOfDay(12.0, Latitude, 23.44);
	const FRotator Winter = SunRotationForTimeOfDay(12.0, Latitude, -23.44);
	TestTrue(TEXT("summer noon is higher than equinox noon"), Summer.Pitch < Noon.Pitch);
	TestTrue(TEXT("winter noon is lower than equinox noon"), Winter.Pitch > Noon.Pitch);

	// Roll is never used: a rolled directional light is meaningless and would show up as an
	// unexplained change in shadow direction.
	TestEqual(TEXT("no roll"), static_cast<double>(Noon.Roll), 0.0, 0.0001);

	return true;
}

/**
 * Fail-closed. A missing or unreadable profile must degrade to the rig defaults, loudly --
 * never to an unlit world, never to a null dereference.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisAtmosphereProfileFallback, "Anastasis.Atmosphere.ProfileFallback", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnastasisAtmosphereProfileFallback::RunTest(const FString&)
{
	AnastasisAtmosphere::InvalidateProfileCache();

	const UAnastasisAtmosphereProfile& Profile = AnastasisAtmosphere::GetProfile();
	const bool bDataDriven = AnastasisAtmosphere::IsProfileDataDriven();

	// Both outcomes are legitimate -- the shipped asset may or may not be present in the
	// branch under test -- but the resolver must always hand back a usable profile and say
	// which source it used.
	TestTrue(TEXT("a profile is always served"), Profile.SunIntensityLux > 0.0f);
	TestTrue(TEXT("exposure value is sane"), Profile.ExposureEV100 > 0.0f);

	if (!bDataDriven)
	{
		// The fallback is the rig, value for value -- same guarantee as RigParity, checked
		// here on the object the runtime will actually use.
		TestEqual(TEXT("fallback sun pitch is the rig's"), Profile.SunPitchDegrees, -38.0f);
		TestEqual(TEXT("fallback lux is the rig's"), Profile.SunIntensityLux, 75000.0f);
	}

	// The cache is a cache: dropping it must re-resolve to an equivalent profile, not to
	// nothing.
	AnastasisAtmosphere::InvalidateProfileCache();
	const UAnastasisAtmosphereProfile& Again = AnastasisAtmosphere::GetProfile();
	TestEqual(TEXT("re-resolution is stable"), Again.SunIntensityLux, Profile.SunIntensityLux);
	TestTrue(TEXT("re-resolution keeps the same source"), AnastasisAtmosphere::IsProfileDataDriven() == bDataDriven);

	return true;
}

/**
 * ADOPT BEFORE SPAWN, proven. Applying the atmosphere twice must leave one sun and one fog
 * in the level, not two: the second pass has to recognise what the first one created. A
 * world that accumulates a sun per Apply() would double its exposure and quietly invalidate
 * every capture taken after the second one.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisAtmosphereIdempotence, "Anastasis.Atmosphere.Idempotence", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnastasisAtmosphereIdempotence::RunTest(const FString&)
{
	UWorld* World = FindAtmosphereAutomationWorld();
	if (!World)
	{
		AddInfo(TEXT("no editor/game world available; idempotence not exercised"));
		return true;
	}

	const int32 SunsBefore = CountActors<ADirectionalLight>(World);
	const int32 FogsBefore = CountActors<AExponentialHeightFog>(World);

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.ObjectFlags |= RF_Transient;

	AAnastasisWorldAtmosphere* First = World->SpawnActor<AAnastasisWorldAtmosphere>(
		AAnastasisWorldAtmosphere::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params);
	if (!TestNotNull(TEXT("atmosphere actor spawns"), First))
	{
		return false;
	}
	TestTrue(TEXT("first apply runs"), First->Apply());

	const int32 SunsAfterFirst = CountActors<ADirectionalLight>(World);
	const int32 FogsAfterFirst = CountActors<AExponentialHeightFog>(World);
	TestTrue(TEXT("after one apply the world has a sun"), SunsAfterFirst >= 1);
	TestTrue(TEXT("after one apply the world has fog"), FogsAfterFirst >= 1);
	TestTrue(TEXT("at most one sun was added"), SunsAfterFirst <= FMath::Max(SunsBefore, 1));
	TestTrue(TEXT("at most one fog was added"), FogsAfterFirst <= FMath::Max(FogsBefore, 1));

	AAnastasisWorldAtmosphere* Second = World->SpawnActor<AAnastasisWorldAtmosphere>(
		AAnastasisWorldAtmosphere::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params);
	if (TestNotNull(TEXT("second atmosphere actor spawns"), Second))
	{
		TestTrue(TEXT("second apply runs"), Second->Apply());
		TestEqual(TEXT("no second sun"), CountActors<ADirectionalLight>(World), SunsAfterFirst);
		TestEqual(TEXT("no second fog"), CountActors<AExponentialHeightFog>(World), FogsAfterFirst);
		TestEqual(TEXT("the second pass adopted rather than spawned"), Second->GetSpawnedCount(), 0);
		TestTrue(TEXT("the second pass adopted something"), Second->GetAdoptedCount() > 0);
		TestTrue(TEXT("both passes converge on the same sun"), Second->GetSun() == First->GetSun());
	}

	// The sun actually carries the profile, not just a default light.
	if (ADirectionalLight* Applied = First->GetSun())
	{
		if (UDirectionalLightComponent* Component = Cast<UDirectionalLightComponent>(Applied->GetLightComponent()))
		{
			const UAnastasisAtmosphereProfile& Profile = AnastasisAtmosphere::GetProfile();
			TestEqual(TEXT("sun intensity comes from the profile"), Component->Intensity, Profile.SunIntensityLux);
			TestTrue(TEXT("sun lights the sky atmosphere"), Component->IsUsedAsAtmosphereSunLight());
			// DAY_NIGHT_WEATHER_001: with the sky clock active the sun is the clock's, else the profile's fixed angle.
			const FRotator Expected = First->IsSkyClockActive() ? First->GetLastSkyState().SunRotation : AnastasisAtmosphere::ResolveSunRotation(Profile);
			TestEqual(TEXT("sun pitch comes from the clock or the profile"), static_cast<double>(Applied->GetActorRotation().Pitch), static_cast<double>(Expected.Pitch), 0.01);
		}
	}

	// Automation must not leave lights behind in whatever level happened to be open. Only
	// what these two instances created is removed; anything they adopted was already there.
	if (Second)
	{
		Second->DestroySpawnedActors();
		Second->Destroy();
	}
	First->DestroySpawnedActors();
	First->Destroy();

	TestEqual(TEXT("the world is left as it was found: suns"), CountActors<ADirectionalLight>(World), SunsBefore);
	TestEqual(TEXT("the world is left as it was found: fog"), CountActors<AExponentialHeightFog>(World), FogsBefore);

	return true;
}

/**
 * ATMOSPHERE_003. The post-process volume above pins the CAMERA's exposure; it says nothing to
 * Lumen's cached lighting (surface cache, SkyLight real-time capture), which pre-exposes against
 * its own EV window before writing into a limited-range buffer. Left at the engine default (4.0
 * EV) against this project's fixed EV100=14, that window clips -- the exact on-screen warning
 * Apply() is meant to silence. This locks the fix: the CVar must track the profile's exposure,
 * not some value picked once and forgotten.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisAtmosphereCachedLightingPreExposure, "Anastasis.Atmosphere.CachedLightingPreExposure", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnastasisAtmosphereCachedLightingPreExposure::RunTest(const FString&)
{
	UWorld* World = FindAtmosphereAutomationWorld();
	if (!World)
	{
		AddInfo(TEXT("no editor/game world available; not exercised"));
		return true;
	}

	IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.EyeAdaptation.CachedLightingPreExposure"));
	if (!TestNotNull(TEXT("the engine still exposes this cvar"), CVar))
	{
		return false;
	}
	const float BeforeValue = CVar->GetFloat();

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.ObjectFlags |= RF_Transient;

	AAnastasisWorldAtmosphere* Atmosphere = World->SpawnActor<AAnastasisWorldAtmosphere>(
		AAnastasisWorldAtmosphere::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params);
	if (!TestNotNull(TEXT("atmosphere actor spawns"), Atmosphere))
	{
		return false;
	}
	TestTrue(TEXT("apply runs"), Atmosphere->Apply());

	const UAnastasisAtmosphereProfile& Profile = AnastasisAtmosphere::GetProfile();
	if (Profile.bFixedExposure)
	{
		TestEqual(TEXT("cached lighting pre-exposure tracks the profile's EV100"), CVar->GetFloat(), Profile.ExposureEV100);
	}

	Atmosphere->DestroySpawnedActors();
	Atmosphere->Destroy();
	CVar->Set(BeforeValue, ECVF_SetByCode);

	return true;
}

/**
 * ENV_REALISM_001. The moon is a second DirectionalLight, and the sun has always been adopted as
 * "the first DirectionalLight in the level". This locks the one failure that makes a moon
 * dangerous: a second Apply() lighting the world with it. It also locks the two physical claims
 * the moon is allowed to make -- moonlight is sub-lux and is not bluer than sunlight (a blue
 * night is a grade, not a light) -- and that it only pays for shadows once the sun has set.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisAtmosphereRealismMoonIsNotTheSun, "Anastasis.Atmosphere.Realism.MoonIsNotTheSun", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnastasisAtmosphereRealismMoonIsNotTheSun::RunTest(const FString&)
{
	UWorld* World = FindAtmosphereAutomationWorld();
	if (!World)
	{
		AddInfo(TEXT("no editor/game world available; not exercised"));
		return true;
	}
	const UAnastasisAtmosphereProfile& Profile = AnastasisAtmosphere::GetProfile();
	if (!Profile.bRealismEnabled || !Profile.bMoonEnabled)
	{
		AddInfo(TEXT("profile disables the realism layer or the moon; not exercised"));
		return true;
	}

	// Physical claims, on the data itself.
	TestTrue(TEXT("moonlight is sub-lux"), Profile.MoonIlluminanceLux < 1.0f);
	TestTrue(TEXT("moonlight is not bluer than sunlight"), Profile.MoonTemperatureKelvin <= Profile.SunTemperatureKelvin);

	const FScopedRealismCVar Realism(1);
	const int32 SunsBefore = CountActors<ADirectionalLight>(World);
	const int32 MoonsBefore = CountTagged<ADirectionalLight>(World, AAnastasisWorldAtmosphere::MoonTag);

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.ObjectFlags |= RF_Transient;

	AAnastasisWorldAtmosphere* First = World->SpawnActor<AAnastasisWorldAtmosphere>(
		AAnastasisWorldAtmosphere::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params);
	AAnastasisWorldAtmosphere* Second = World->SpawnActor<AAnastasisWorldAtmosphere>(
		AAnastasisWorldAtmosphere::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params);
	if (!TestNotNull(TEXT("first actor"), First) || !TestNotNull(TEXT("second actor"), Second))
	{
		return false;
	}
	TestTrue(TEXT("first apply"), First->Apply());
	TestTrue(TEXT("second apply"), Second->Apply());

	TestTrue(TEXT("the realism layer ran"), Second->WasRealismApplied());
	TestEqual(TEXT("exactly one moon after two applies"), CountTagged<ADirectionalLight>(World, AAnastasisWorldAtmosphere::MoonTag), 1);
	TestEqual(TEXT("no second sun"), CountActors<ADirectionalLight>(World), FMath::Max(SunsBefore, 1));
	TestTrue(TEXT("both passes converge on the same sun"), First->GetSun() == Second->GetSun());
	TestTrue(TEXT("the sun is not the moon"), Second->GetSun() != Second->GetMoon());

	if (ADirectionalLight* SunActor = Second->GetSun())
	{
		if (const UDirectionalLightComponent* SunComponent = Cast<UDirectionalLightComponent>(SunActor->GetLightComponent()))
		{
			TestEqual(TEXT("the adopted sun keeps the sun's intensity"), SunComponent->Intensity, Profile.SunIntensityLux);
			TestEqual(TEXT("the sun drives atmosphere slot 0"), SunComponent->GetAtmosphereSunLightIndex(), 0);
		}
	}
	if (ADirectionalLight* MoonActor = Second->GetMoon())
	{
		if (const UDirectionalLightComponent* MoonComponent = Cast<UDirectionalLightComponent>(MoonActor->GetLightComponent()))
		{
			TestEqual(TEXT("moon intensity comes from the profile"), MoonComponent->Intensity, Profile.MoonIlluminanceLux);
			TestEqual(TEXT("the moon drives atmosphere slot 1"), MoonComponent->GetAtmosphereSunLightIndex(), 1);
			TestTrue(TEXT("the moon lights the atmosphere"), MoonComponent->IsUsedAsAtmosphereSunLight());
			const FRotator SunNow = Second->IsSkyClockActive() ? Second->GetLastSkyState().SunRotation : AnastasisAtmosphere::ResolveSunRotation(Profile);
			const FRotator MoonNow = Second->IsSkyClockActive() ? Second->GetLastSkyState().MoonRotation : AnastasisAtmosphere::ResolveMoonRotation(Profile);
			const bool bSunDown = AnastasisAtmosphere::IsBelowHorizon(SunNow);
			TestTrue(TEXT("the moon never shadows while the sun is up"), bSunDown || MoonComponent->CastShadows == 0);
			if (Second->IsSkyClockActive())
			{
				TestEqual(TEXT("under the clock the moon shadows exactly when it is up and the sun is down"),
					MoonComponent->CastShadows != 0, bSunDown && !AnastasisAtmosphere::IsBelowHorizon(MoonNow));
			}
		}
	}

	Second->DestroySpawnedActors();
	Second->Destroy();
	First->DestroySpawnedActors();
	First->Destroy();
	TestEqual(TEXT("the world is left as it was found: suns"), CountActors<ADirectionalLight>(World), SunsBefore);
	TestEqual(TEXT("the world is left as it was found: moons"), CountTagged<ADirectionalLight>(World, AAnastasisWorldAtmosphere::MoonTag), MoonsBefore);
	return true;
}

/**
 * ENV_REALISM_001. The A/B of this mission is one CVar in one session, which is only honest if
 * "off" really is the pre-realism image. Off must write back the engine component defaults of
 * every property the layer owns -- neither the observation rig nor ATMOSPHERE_001/002 ever wrote
 * them, so those defaults are the old state -- and take the moon and the clouds away; on again
 * must bring the profile back.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisAtmosphereRealismReversible, "Anastasis.Atmosphere.Realism.Reversible", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnastasisAtmosphereRealismReversible::RunTest(const FString&)
{
	UWorld* World = FindAtmosphereAutomationWorld();
	if (!World)
	{
		AddInfo(TEXT("no editor/game world available; not exercised"));
		return true;
	}
	const UAnastasisAtmosphereProfile& Profile = AnastasisAtmosphere::GetProfile();
	if (!Profile.bRealismEnabled)
	{
		AddInfo(TEXT("profile disables the realism layer; not exercised"));
		return true;
	}

	FScopedRealismCVar Realism(1);
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.ObjectFlags |= RF_Transient;
	AAnastasisWorldAtmosphere* Atmosphere = World->SpawnActor<AAnastasisWorldAtmosphere>(
		AAnastasisWorldAtmosphere::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params);
	if (!TestNotNull(TEXT("atmosphere actor spawns"), Atmosphere))
	{
		return false;
	}

	const USkyAtmosphereComponent* SkyDefault = GetDefault<USkyAtmosphereComponent>();
	const UExponentialHeightFogComponent* FogDefault = GetDefault<UExponentialHeightFogComponent>();
	const UDirectionalLightComponent* SunDefault = GetDefault<UDirectionalLightComponent>();

	auto Sky = [Atmosphere]() { return Atmosphere->GetSkyAtmosphere() ? Atmosphere->GetSkyAtmosphere()->GetComponent() : nullptr; };
	auto FogC = [Atmosphere]() { return Atmosphere->GetFog() ? Atmosphere->GetFog()->GetComponent() : nullptr; };
	auto SunC = [Atmosphere]() { return Atmosphere->GetSun() ? Cast<UDirectionalLightComponent>(Atmosphere->GetSun()->GetLightComponent()) : nullptr; };

	// On.
	TestTrue(TEXT("apply (on)"), Atmosphere->Apply());
	if (const USkyAtmosphereComponent* S = Sky())
	{
		TestEqual(TEXT("on: mie from the profile"), S->MieScatteringScale, Profile.SkyMieScatteringScale);
		TestEqual(TEXT("on: aerial perspective from the profile"), S->AerialPespectiveViewDistanceScale, Profile.SkyAerialPerspectiveDistanceScale);
	}
	if (const UExponentialHeightFogComponent* F = FogC())
	{
		TestEqual(TEXT("on: valley fog layer from the profile"), F->SecondFogData.FogDensity, Profile.ValleyFogDensity);
		TestEqual(TEXT("on: volumetric fog from the profile"), F->bEnableVolumetricFog, Profile.bVolumetricFog);
	}
	if (const UDirectionalLightComponent* L = SunC())
	{
		TestTrue(TEXT("on: sun uses a colour temperature"), L->bUseTemperature != 0);
		TestEqual(TEXT("on: sun temperature from the profile"), L->Temperature, Profile.SunTemperatureKelvin);
	}
	TestEqual(TEXT("on: moon present iff the profile asks"), Atmosphere->GetMoon() != nullptr, Profile.bMoonEnabled);

	// Off.
	Realism.Set(0);
	TestTrue(TEXT("apply (off)"), Atmosphere->Apply());
	TestFalse(TEXT("off: the layer reports itself off"), Atmosphere->WasRealismApplied());
	if (const USkyAtmosphereComponent* S = Sky())
	{
		TestEqual(TEXT("off: mie back to the engine default"), S->MieScatteringScale, SkyDefault->MieScatteringScale);
		TestEqual(TEXT("off: aerial perspective back to the engine default"), S->AerialPespectiveViewDistanceScale, SkyDefault->AerialPespectiveViewDistanceScale);
		TestTrue(TEXT("off: ground albedo back to the engine default"), S->GroundAlbedo == SkyDefault->GroundAlbedo);
	}
	if (const UExponentialHeightFogComponent* F = FogC())
	{
		TestEqual(TEXT("off: valley layer back to the engine default"), F->SecondFogData.FogDensity, FogDefault->SecondFogData.FogDensity);
		TestEqual(TEXT("off: volumetric fog back to the engine default"), F->bEnableVolumetricFog, FogDefault->bEnableVolumetricFog);
		TestEqual(TEXT("off: volumetric distance back to the engine default"), F->VolumetricFogDistance, FogDefault->VolumetricFogDistance);
	}
	if (const UDirectionalLightComponent* L = SunC())
	{
		TestEqual(TEXT("off: sun temperature flag back to the engine default"), L->bUseTemperature != 0, SunDefault->bUseTemperature != 0);
		TestEqual(TEXT("off: cloud shadows back to the engine default"), L->bCastCloudShadows != 0, SunDefault->bCastCloudShadows != 0);
		TestEqual(TEXT("off: the sun keeps its intensity"), L->Intensity, Profile.SunIntensityLux);
	}
	TestNull(TEXT("off: no moon"), Atmosphere->GetMoon());
	TestNull(TEXT("off: no managed cloud layer"), Atmosphere->GetCloud());
	TestEqual(TEXT("off: no moon left in the level"), CountTagged<ADirectionalLight>(World, AAnastasisWorldAtmosphere::MoonTag), 0);

	// On again: the same values, not a drift.
	Realism.Set(1);
	TestTrue(TEXT("apply (on again)"), Atmosphere->Apply());
	if (const USkyAtmosphereComponent* S = Sky())
	{
		TestEqual(TEXT("on again: mie from the profile"), S->MieScatteringScale, Profile.SkyMieScatteringScale);
	}
	TestTrue(TEXT("on again: the layer reports itself on"), Atmosphere->WasRealismApplied());

	Atmosphere->DestroySpawnedActors();
	Atmosphere->Destroy();
	return true;
}

/**
 * ENV_REALISM_001, pure geometry. The derived moon is a full moon: opposite the sun in hour
 * angle. What the night needs from that is simple and checkable -- when the sun is down the moon
 * is up, and vice versa -- and it must be as deterministic as the sun it mirrors.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisAtmosphereRealismMoonGeometry, "Anastasis.Atmosphere.Realism.MoonGeometry", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnastasisAtmosphereRealismMoonGeometry::RunTest(const FString&)
{
	UAnastasisAtmosphereProfile* Profile = UAnastasisAtmosphereProfile::CreateCodeDefaults(GetTransientPackage());
	if (!TestNotNull(TEXT("code defaults"), Profile))
	{
		return false;
	}

	// Explicit mode: the profile's own angles, the sun's untouched.
	TestFalse(TEXT("default sun is explicit"), Profile->bDeriveSunFromTimeOfDay);
	const FRotator Explicit = AnastasisAtmosphere::ResolveMoonRotation(*Profile);
	TestEqual(TEXT("explicit moon pitch"), static_cast<float>(Explicit.Pitch), Profile->MoonPitchDegrees);
	TestEqual(TEXT("explicit moon yaw"), static_cast<float>(Explicit.Yaw), Profile->MoonYawDegrees);
	TestFalse(TEXT("default sun is above the horizon"), AnastasisAtmosphere::IsBelowHorizon(AnastasisAtmosphere::ResolveSunRotation(*Profile)));

	// Derived mode, across a day.
	Profile->bDeriveSunFromTimeOfDay = true;
	for (const float Hours : {0.0f, 3.0f, 12.0f, 15.0f, 21.0f})
	{
		Profile->TimeOfDayHours = Hours;
		const FRotator SunR = AnastasisAtmosphere::ResolveSunRotation(*Profile);
		const FRotator MoonR = AnastasisAtmosphere::ResolveMoonRotation(*Profile);
		const FString At = FString::Printf(TEXT(" at %.0fh"), Hours);
		TestTrue(TEXT("sun and full moon are on opposite sides of the horizon") + At,
			AnastasisAtmosphere::IsBelowHorizon(SunR) != AnastasisAtmosphere::IsBelowHorizon(MoonR));
		TestTrue(TEXT("the moon is deterministic") + At, MoonR == AnastasisAtmosphere::ResolveMoonRotation(*Profile));
	}
	Profile->TimeOfDayHours = 0.0f;
	const FRotator Midnight = AnastasisAtmosphere::ResolveMoonRotation(*Profile);
	Profile->TimeOfDayHours = 12.0f;
	const FRotator NoonSun = AnastasisAtmosphere::ResolveSunRotation(*Profile);
	TestEqual(TEXT("the midnight full moon stands where the noon sun did (equinox)"),
		static_cast<double>(Midnight.Pitch), static_cast<double>(NoonSun.Pitch), 0.001);
	return true;
}

/**
 * ATMOSPHERE_COHERENCE_001, in a real world. The renderer's warning "multiple directional lights
 * are competing to be the single one used for forward shading, translucent, water or volumetric
 * fog" is exactly two directional lights at the same ForwardShadingPriority. With the sky pinned
 * at noon and at midnight, after Apply(): sun and moon never share a priority, the sun leads at
 * noon, the moon at midnight, and a set sun scatters nothing in the fog.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisAtmosphereForwardLightOneLeader, "Anastasis.Atmosphere.ForwardLight.OneLeader", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnastasisAtmosphereForwardLightOneLeader::RunTest(const FString&)
{
	UWorld* World = FindAtmosphereAutomationWorld();
	if (!World)
	{
		AddInfo(TEXT("no editor/game world available; not exercised"));
		return true;
	}
	const UAnastasisAtmosphereProfile& Profile = AnastasisAtmosphere::GetProfile();
	IConsoleVariable* Clock = IConsoleManager::Get().FindConsoleVariable(TEXT("anastasis.Sky.Clock"));
	IConsoleVariable* Hour = IConsoleManager::Get().FindConsoleVariable(TEXT("anastasis.Sky.Hour"));
	if (!Profile.bRealismEnabled || !Profile.bMoonEnabled || !Profile.bSkyFollowsSimulation || !Clock || !Hour)
	{
		AddInfo(TEXT("profile or CVars disable the moon or the sky clock; not exercised"));
		return true;
	}

	const FScopedRealismCVar Realism(1);
	const int32 ClockBefore = Clock->GetInt();
	const float HourBefore = Hour->GetFloat();
	Clock->Set(1, ECVF_SetByCode);

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.ObjectFlags |= RF_Transient;
	AAnastasisWorldAtmosphere* Atmosphere = World->SpawnActor<AAnastasisWorldAtmosphere>(
		AAnastasisWorldAtmosphere::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params);
	if (TestNotNull(TEXT("atmosphere actor spawns"), Atmosphere))
	{
		for (const float Pinned : {12.0f, 0.0f})
		{
			Hour->Set(Pinned, ECVF_SetByCode);
			const FString At = FString::Printf(TEXT(" at %.0fh"), Pinned);
			TestTrue(TEXT("apply") + At, Atmosphere->Apply());
			const UDirectionalLightComponent* SunC = Atmosphere->GetSun() ? Cast<UDirectionalLightComponent>(Atmosphere->GetSun()->GetLightComponent()) : nullptr;
			const UDirectionalLightComponent* MoonC = Atmosphere->GetMoon() ? Cast<UDirectionalLightComponent>(Atmosphere->GetMoon()->GetLightComponent()) : nullptr;
			if (!TestNotNull(TEXT("sun") + At, SunC) || !TestNotNull(TEXT("moon") + At, MoonC))
			{
				continue;
			}
			const bool bNight = Pinned < 6.0f;
			TestTrue(TEXT("sun and moon never share a forward priority") + At, SunC->ForwardShadingPriority != MoonC->ForwardShadingPriority);
			TestEqual(TEXT("the leader is the sun by day, the moon by night") + At, Atmosphere->IsMoonLeadingForward(), bNight);
			TestEqual(TEXT("the leader holds the higher priority") + At,
				(bNight ? MoonC : SunC)->ForwardShadingPriority, AnastasisSkyClock::ForwardPriorityLead);
			if (bNight)
			{
				TestEqual(TEXT("a set sun scatters nothing in the fog") + At, SunC->VolumetricScatteringIntensity, 0.0f);
			}
			else
			{
				TestEqual(TEXT("a high sun scatters fully") + At, SunC->VolumetricScatteringIntensity, 1.0f);
			}
		}
		AddInfo(FString::Printf(TEXT("ANASTASIS_ATMOSPHERE_FORWARD extra_directional_lights=%d"), Atmosphere->GetExtraDirectionalLightCount()));
		Atmosphere->DestroySpawnedActors();
		Atmosphere->Destroy();
	}

	Hour->Set(HourBefore, ECVF_SetByCode);
	Clock->Set(ClockBefore, ECVF_SetByCode);
	return true;
}

#endif
