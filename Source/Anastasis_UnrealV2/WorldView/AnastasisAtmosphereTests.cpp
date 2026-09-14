#include "Misc/AutomationTest.h"

#include "Components/DirectionalLightComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/Engine.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "WorldView/AnastasisAtmosphereProfile.h"
#include "WorldView/AnastasisAtmosphereResolver.h"
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

	template <typename ActorType>
	int32 CountActors(UWorld* World)
	{
		int32 Count = 0;
		for (TActorIterator<ActorType> It(World); It; ++It)
		{
			if (IsValid(*It))
			{
				++Count;
			}
		}
		return Count;
	}
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
			const FRotator Expected = AnastasisAtmosphere::ResolveSunRotation(Profile);
			TestEqual(TEXT("sun pitch comes from the profile"), static_cast<double>(Applied->GetActorRotation().Pitch), static_cast<double>(Expected.Pitch), 0.01);
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

#endif
