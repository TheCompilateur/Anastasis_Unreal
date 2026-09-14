#include "WorldView/AnastasisWorldAtmosphere.h"

#include "Anastasis_UnrealV2.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/SkyLight.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "WorldView/AnastasisAtmosphereProfile.h"
#include "WorldView/AnastasisAtmosphereResolver.h"

static TAutoConsoleVariable<int32> CVarAtmosphere(
	TEXT("anastasis.Atmosphere"),
	1,
	TEXT("World atmosphere. 0=leave the level's own lighting alone, 1=apply UAnastasisAtmosphereProfile; read when the world is embodied."),
	ECVF_Default);

namespace
{
	/**
	 * First actor of this class already in the level, or null.
	 *
	 * Deliberately "first" rather than "the one we like best": a level with two suns is a
	 * level-authoring problem, and silently picking among them would hide it. The summary
	 * line reports what was adopted so a duplicate is visible in the log instead.
	 */
	template <typename ActorType>
	ActorType* FindExisting(UWorld* World)
	{
		for (TActorIterator<ActorType> It(World); It; ++It)
		{
			if (IsValid(*It))
			{
				return *It;
			}
		}
		return nullptr;
	}

	/**
	 * Static and stationary components refuse runtime changes (and warn about it). The
	 * atmosphere is data now -- it has to be writable -- so anything adopted is promoted to
	 * Movable before being written.
	 */
	void MakeMovable(USceneComponent* Component)
	{
		if (Component && Component->Mobility != EComponentMobility::Movable)
		{
			Component->SetMobility(EComponentMobility::Movable);
		}
	}
}

AAnastasisWorldAtmosphere::AAnastasisWorldAtmosphere()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

template <typename ActorType>
ActorType* AAnastasisWorldAtmosphere::AdoptOrSpawn(const FVector& Location, const FRotator& Rotation, const FActorSpawnParameters& Params)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}
	if (ActorType* Existing = FindExisting<ActorType>(World))
	{
		++AdoptedCount;
		return Existing;
	}
	ActorType* Spawned = World->SpawnActor<ActorType>(Location, Rotation, Params);
	if (Spawned)
	{
		++SpawnedCount;
		SpawnedActors.Add(Spawned);
	}
	return Spawned;
}

void AAnastasisWorldAtmosphere::DestroySpawnedActors()
{
	for (const TObjectPtr<AActor>& Actor : SpawnedActors)
	{
		if (IsValid(Actor))
		{
			Actor->Destroy();
		}
	}
	SpawnedActors.Reset();
	SpawnedCount = 0;
}

bool AAnastasisWorldAtmosphere::IsEnabledByCVar()
{
	return CVarAtmosphere.GetValueOnAnyThread() != 0;
}

void AAnastasisWorldAtmosphere::BeginPlay()
{
	Super::BeginPlay();
	Apply();
}

bool AAnastasisWorldAtmosphere::Apply()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	const UAnastasisAtmosphereProfile& Profile = AnastasisAtmosphere::GetProfile();
	const TCHAR* Source = AnastasisAtmosphere::IsProfileDataDriven() ? TEXT("asset") : TEXT("code_defaults");

	if (!Profile.bEnabled)
	{
		// An explicit off switch, not a failure: the level keeps whatever lighting it was
		// authored with, and says so.
		LastSummary = FString::Printf(TEXT("ANASTASIS_ATMOSPHERE applied=0 profile=%s reason=profile_disabled"), Source);
		UE_LOG(LogAnastasis_UnrealV2, Display, TEXT("%s"), *LastSummary);
		return false;
	}

	SpawnedCount = 0;
	AdoptedCount = 0;

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.ObjectFlags |= RF_Transient;

	const FRotator SunRotation = AnastasisAtmosphere::ResolveSunRotation(Profile);

	// --- Sun ---------------------------------------------------------------------------
	Sun = AdoptOrSpawn<ADirectionalLight>(FVector(0.0, 0.0, 4000.0), SunRotation, Params);
	if (Sun)
	{
		MakeMovable(Sun->GetRootComponent());
		Sun->SetActorRotation(SunRotation);
		if (UDirectionalLightComponent* SunComponent = Cast<UDirectionalLightComponent>(Sun->GetLightComponent()))
		{
			SunComponent->SetIntensity(Profile.SunIntensityLux);
			SunComponent->SetLightColor(Profile.SunColor);
			SunComponent->SetCastShadows(Profile.bSunCastsShadows);
			// Without this the SkyAtmosphere has nothing to scatter: black sky, and the
			// water's smooth specular reflects that black back at the camera.
			SunComponent->SetAtmosphereSunLight(Profile.bSunIsAtmosphereLight);
		}
	}

	// --- Sky ---------------------------------------------------------------------------
	if (Profile.bSkyAtmosphereEnabled)
	{
		SkyAtmosphere = AdoptOrSpawn<ASkyAtmosphere>(FVector::ZeroVector, FRotator::ZeroRotator, Params);
	}

	if (Profile.bSkyLightEnabled)
	{
		SkyLight = AdoptOrSpawn<ASkyLight>(FVector(0.0, 0.0, 2000.0), FRotator::ZeroRotator, Params);
		if (SkyLight)
		{
			if (USkyLightComponent* SkyComponent = SkyLight->GetLightComponent())
			{
				MakeMovable(SkyComponent);
				// Real-time capture instead of a baked cubemap: a sky light that does not
				// follow its sun turns every derived-sun experiment into a lie.
				SkyComponent->SetRealTimeCaptureEnabled(Profile.bSkyLightRealTimeCapture);
				SkyComponent->SetIntensity(Profile.SkyLightIntensity);
			}
		}
	}

	// --- Fog ---------------------------------------------------------------------------
	if (Profile.bFogEnabled)
	{
		Fog = AdoptOrSpawn<AExponentialHeightFog>(
			FVector(0.0, 0.0, Profile.FogHeightZ), FRotator::ZeroRotator, Params);
		if (Fog)
		{
			MakeMovable(Fog->GetRootComponent());
			Fog->SetActorLocation(FVector(0.0, 0.0, Profile.FogHeightZ));
			if (UExponentialHeightFogComponent* FogComponent = Fog->GetComponent())
			{
				FogComponent->SetFogDensity(Profile.FogDensity);
				FogComponent->SetFogHeightFalloff(Profile.FogHeightFalloff);
				FogComponent->SetStartDistance(Profile.FogStartDistance);
				FogComponent->SetFogMaxOpacity(Profile.FogMaxOpacity);
				// Writes FogInscatteringLuminance. Ignored when
				// r.SupportExpFogMatchesVolumetricFog=1, which this project does not set.
				FogComponent->SetFogInscatteringColor(Profile.FogInscatteringColor);
			}
		}
	}

	// --- Exposure ----------------------------------------------------------------------
	if (Profile.bFixedExposure)
	{
		ExposureVolume = AdoptOrSpawn<APostProcessVolume>(FVector::ZeroVector, FRotator::ZeroRotator, Params);
		if (ExposureVolume)
		{
			// Unbound: exposure must hold wherever the camera is, not inside one box in one
			// level. Min = Max pins it: auto-exposure makes two captures incomparable, which
			// is the whole reason this project captures at all.
			ExposureVolume->bUnbound = true;
			ExposureVolume->Settings.bOverride_AutoExposureMethod = true;
			ExposureVolume->Settings.AutoExposureMethod = AEM_Histogram;
			ExposureVolume->Settings.bOverride_AutoExposureMinBrightness = true;
			ExposureVolume->Settings.AutoExposureMinBrightness = Profile.ExposureEV100;
			ExposureVolume->Settings.bOverride_AutoExposureMaxBrightness = true;
			ExposureVolume->Settings.AutoExposureMaxBrightness = Profile.ExposureEV100;
		}
	}

	LastSummary = FString::Printf(
		TEXT("ANASTASIS_ATMOSPHERE applied=1 profile=%s sun_source=%s sun_pitch=%.3f sun_yaw=%.3f lux=%.1f ")
		TEXT("fog=%d fog_density=%.4f ev100=%.2f adopted=%d spawned=%d"),
		Source,
		Profile.bDeriveSunFromTimeOfDay ? TEXT("time_of_day") : TEXT("explicit"),
		SunRotation.Pitch, SunRotation.Yaw, Profile.SunIntensityLux,
		Profile.bFogEnabled ? 1 : 0, Profile.FogDensity, Profile.ExposureEV100,
		AdoptedCount, SpawnedCount);
	UE_LOG(LogAnastasis_UnrealV2, Display, TEXT("%s"), *LastSummary);

	return true;
}
