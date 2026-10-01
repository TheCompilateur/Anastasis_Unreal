#include "WorldView/AnastasisWorldAtmosphere.h"

#include "Anastasis_UnrealV2.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/LocalFogVolumeComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/LocalFogVolume.h"
#include "Engine/PostProcessVolume.h"
#include "Components/VolumetricCloudComponent.h"
#include "Engine/SkyLight.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialParameterCollection.h"
#include "Materials/MaterialParameterCollectionInstance.h"
#include "Life/AnastasisVillageRhythm.h"
#include "Sim/AnastasisSimulationSubsystem.h"
#include "WorldView/AnastasisAtmosphereProfile.h"
#include "WorldView/AnastasisAtmosphereResolver.h"
#include "WorldView/AnastasisMistField.h"
#include "WorldView/AnastasisTerrainSurface.h"
#include "WorldView/AnastasisTerrainForge.h"
#include "WorldView/AnastasisWorldEmbodiment.h"

static TAutoConsoleVariable<int32> CVarMist(
	TEXT("anastasis.Atmosphere.Mist"),
	1,
	TEXT("Wetness-driven mist pockets. 0=place none (the global atmosphere is untouched), 1=place them; read when the mist is applied."),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarAtmosphere(
	TEXT("anastasis.Atmosphere"),
	1,
	TEXT("World atmosphere. 0=leave the level's own lighting alone, 1=apply UAnastasisAtmosphereProfile; read when the world is embodied."),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarSkyClock(
	TEXT("anastasis.Sky.Clock"),
	1,
	TEXT("DAY_NIGHT_WEATHER_001. 1=the sky follows the simulation's hour, season and weather; 0=the profile's fixed sun (the observation rig). Read on every Apply() and Tick."),
	ECVF_Default);

static TAutoConsoleVariable<float> CVarSkyHour(
	TEXT("anastasis.Sky.Hour"),
	-1.0f,
	TEXT("Pins the SKY at this hour [0,24) for captures; -1 follows the simulation. Never moves the simulation's own clock."),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarSkyDay(
	TEXT("anastasis.Sky.Day"),
	-1,
	TEXT("Pins the SKY on this simulation day (>=1: season and weather) for captures; -1 follows the simulation. Never moves the simulation."),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarSkyWeather(
	TEXT("anastasis.Sky.Weather"),
	1,
	TEXT("1=the simulation's weather drives cloud coverage and fog density; 0=fixed fair-weather sky."),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarSkyTwilight(
	TEXT("anastasis.Sky.Twilight"),
	1,
	TEXT("ENV_REALISM_002. 1=thinner mist, fog and aerosol and a calmer grade while the sun grazes the horizon, moon white point only at night; 0=the DAY_NIGHT_WEATHER_001 twilight (the A/B)."),
	ECVF_Default);

static TAutoConsoleVariable<float> CVarSkyGroundWetness(
	TEXT("anastasis.Sky.GroundWetness"),
	-1.0f,
	TEXT("ENV_REALISM_002. Pins the ground's rain wetness [0,1] for captures (M_AnastasisGround via MPC_AnastasisWeather); -1 follows the simulation's weather."),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarRealism(
	TEXT("anastasis.Atmosphere.Realism"),
	1,
	TEXT("ENV_REALISM_001 layer: tuned sky, clouds, moon, valley fog, volumetric fog. 0=engine defaults for every property it owns (the pre-realism image), 1=profile values; read on every Apply()."),
	ECVF_Default);

const FName AAnastasisWorldAtmosphere::MoonTag(TEXT("AnastasisMoon"));
const FName AAnastasisWorldAtmosphere::RealismCloudTag(TEXT("AnastasisRealismCloud"));

namespace
{
	/**
	 * First actor of this class already in the level, or null.
	 *
	 * Deliberately "first" rather than "the one we like best": a level with two suns is a
	 * level-authoring problem, and silently picking among them would hide it. The summary
	 * line reports what was adopted so a duplicate is visible in the log instead.
	 *
	 * The moon is never "an existing sun": it is a DirectionalLight too, and adopting it as the
	 * sun would light the world at moonlight intensity after the second Apply().
	 */
	template <typename ActorType>
	ActorType* FindExisting(UWorld* World)
	{
		for (TActorIterator<ActorType> It(World); It; ++It)
		{
			if (IsValid(*It) && !It->ActorHasTag(AAnastasisWorldAtmosphere::MoonTag))
			{
				return *It;
			}
		}
		return nullptr;
	}

	template <typename ActorType>
	ActorType* FindTagged(UWorld* World, const FName Tag)
	{
		for (TActorIterator<ActorType> It(World); It; ++It)
		{
			if (IsValid(*It) && It->ActorHasTag(Tag))
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
	// Ticks only to follow the simulation clock; Tick returns at once when the clock is off.
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
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

	// The moon and the managed cloud layer are always this layer's own (found by tag, never
	// adopted from the level), so they go too.
	if (IsValid(Moon))
	{
		Moon->Destroy();
	}
	Moon = nullptr;
	if (IsValid(Cloud))
	{
		Cloud->Destroy();
	}
	Cloud = nullptr;
}

bool AAnastasisWorldAtmosphere::IsEnabledByCVar()
{
	return CVarAtmosphere.GetValueOnAnyThread() != 0;
}

bool AAnastasisWorldAtmosphere::IsRealismEnabledByCVar()
{
	return CVarRealism.GetValueOnAnyThread() != 0;
}

bool AAnastasisWorldAtmosphere::IsSkyClockEnabledByCVar()
{
	return CVarSkyClock.GetValueOnAnyThread() != 0;
}

double AAnastasisWorldAtmosphere::ResolveSkySimTime(uint32& OutSeed) const
{
	// The canonical seed: the world every capture and test of this project is taken on.
	OutSeed = 12345u;
	double SimTime = AnastasisSkyClock::InitialSimTime;

	if (const UWorld* World = GetWorld())
	{
		if (const UAnastasisSimulationSubsystem* Host = World->GetSubsystem<UAnastasisSimulationSubsystem>())
		{
			const FAnastasisSimulation& Simulation = Host->GetSimulation();
			if (Simulation.IsRunning())
			{
				SimTime = Simulation.GetTime();
				OutSeed = Simulation.GetSeed();
			}
		}
	}

	// Pins for captures. They move what the sky SHOWS; the simulation keeps its own time.
	const int32 PinnedDay = CVarSkyDay.GetValueOnAnyThread();
	const float PinnedHour = CVarSkyHour.GetValueOnAnyThread();
	if (PinnedDay >= 1 || PinnedHour >= 0.0f)
	{
		const double Day = PinnedDay >= 1 ? static_cast<double>(PinnedDay) : 1.0 + FMath::FloorToDouble(SimTime / AnastasisSkyClock::DayLengthSeconds);
		const double Hours = PinnedHour >= 0.0f ? static_cast<double>(PinnedHour) : AnastasisRhythm::DayFracOf(SimTime) * 24.0;
		SimTime = AnastasisSkyClock::SimTimeFor(Day, Hours);
	}
	return SimTime;
}

void AAnastasisWorldAtmosphere::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bSkyClockActive)
	{
		return;
	}
	const UAnastasisAtmosphereProfile& Profile = AnastasisAtmosphere::GetProfile();
	// The CVar is re-read here too: switching the clock off in a live session freezes the
	// sky where it is until the next Apply() restores the fixed rig.
	if (!Profile.bEnabled || !Profile.bSkyFollowsSimulation || !IsSkyClockEnabledByCVar())
	{
		return;
	}
	uint32 Seed = 0;
	const double SimTime = ResolveSkySimTime(Seed);
	LastSky = AnastasisSkyClock::Evaluate(Profile, SimTime, Seed);
	if (CVarSkyTwilight.GetValueOnAnyThread() == 0)
	{
		LastSky = AnastasisSkyClock::WithoutTwilight(Profile, LastSky);
	}
	UpdateSky(Profile, /*bForceLog*/ false);
}

void AAnastasisWorldAtmosphere::UpdateSky(const UAnastasisAtmosphereProfile& Profile, const bool bForceLog)
{
	const bool bSunUp = !AnastasisAtmosphere::IsBelowHorizon(LastSky.SunRotation);

	// --- Sun and moon ------------------------------------------------------------------
	if (Sun)
	{
		Sun->SetActorRotation(LastSky.SunRotation);
		if (UDirectionalLightComponent* SunComponent = Cast<UDirectionalLightComponent>(Sun->GetLightComponent()))
		{
			// A sun under the horizon lights nothing through the planet, but its shadow
			// pass would still be paid every frame.
			const bool bShadows = Profile.bSunCastsShadows && bSunUp;
			if ((SunComponent->CastShadows != 0) != bShadows)
			{
				SunComponent->SetCastShadows(bShadows);
			}
		}
	}
	if (Moon)
	{
		Moon->SetActorRotation(LastSky.MoonRotation);
		if (UDirectionalLightComponent* MoonComponent = Cast<UDirectionalLightComponent>(Moon->GetLightComponent()))
		{
			// The moon earns its shadows once the sun has set, and only while it is itself up.
			const bool bShadows = !bSunUp && !AnastasisAtmosphere::IsBelowHorizon(LastSky.MoonRotation);
			if ((MoonComponent->CastShadows != 0) != bShadows)
			{
				MoonComponent->SetCastShadows(bShadows);
			}
		}
	}

	// --- Exposure: pinned, but pinned to the hour --------------------------------------
	if (Profile.bFixedExposure && ExposureVolume)
	{
		const float EV = static_cast<float>(LastSky.ExposureEV100);
		// Saturation and white point no longer move only with the exposure: the twilight
		// grade and the moon's white point have their own bands (ENV_REALISM_002), and the
		// exposure is flat above +10 deg while the twilight band runs to +14.
		const bool bGradeMoved = FMath::Abs(static_cast<float>(LastSky.ColorSaturation) - LastSaturationWritten) > 0.002f
			|| FMath::Abs(static_cast<float>(LastSky.WhiteTemp) - LastWhiteTempWritten) > 1.0f;
		if (FMath::Abs(EV - LastExposureWritten) > 0.005f || bGradeMoved)
		{
			ExposureVolume->Settings.AutoExposureMinBrightness = EV;
			ExposureVolume->Settings.AutoExposureMaxBrightness = EV;
			// Lumen's cached lighting pre-exposes against this window; it has to follow the
			// camera's exposure through the night, not stay at the day's EV (see Apply()).
			if (IConsoleVariable* CachedLightingPreExposure =
				IConsoleManager::Get().FindConsoleVariable(TEXT("r.EyeAdaptation.CachedLightingPreExposure")))
			{
				CachedLightingPreExposure->Set(EV, ECVF_SetByCode);
			}
			// Night vision on the same curve: colour fades and the white point follows the moon.
			const float Sat = static_cast<float>(LastSky.ColorSaturation);
			ExposureVolume->Settings.bOverride_ColorSaturation = true;
			ExposureVolume->Settings.ColorSaturation = FVector4(Sat, Sat, Sat, 1.0f);
			ExposureVolume->Settings.bOverride_WhiteTemp = true;
			ExposureVolume->Settings.WhiteTemp = static_cast<float>(LastSky.WhiteTemp);
			LastExposureWritten = EV;
			LastSaturationWritten = Sat;
			LastWhiteTempWritten = static_cast<float>(LastSky.WhiteTemp);
		}
	}

	// --- Weather: clouds and fog -------------------------------------------------------
	const bool bWeather = Profile.bWeatherDrivesSky && CVarSkyWeather.GetValueOnAnyThread() != 0;
	if (Fog)
	{
		if (UExponentialHeightFogComponent* FogComponent = Fog->GetComponent())
		{
			const double Scale = (bWeather ? AnastasisSkyClock::FogDensityScaleFor(Profile, LastSky.Humidity) : 1.0)
				* AnastasisSkyClock::TwilightScale(LastSky.Twilight, Profile.TwilightFogDensityScale);
			const float Density = static_cast<float>(Profile.FogDensity * Scale);
			if (!FMath::IsNearlyEqual(FogComponent->FogDensity, Density, 1e-6f))
			{
				FogComponent->SetFogDensity(Density);
			}

			// The authored inscattering is an absolute luminance tuned for the day's EV: it has
			// to dim with the light, or the night fog glows (see FogInscatteringScaleFor).
			const float Inscatter = static_cast<float>(AnastasisSkyClock::FogInscatteringScaleFor(LastSky.ExposureEV100, Profile.ExposureEV100));
			const FLinearColor Scattered = Profile.FogInscatteringColor * Inscatter;
			if (!FogComponent->FogInscatteringLuminance.Equals(Scattered, 1e-7f))
			{
				FogComponent->SetFogInscatteringColor(Scattered);
			}
		}
	}

	// --- Mist: where is the simulation's wetness, when and how much is the sky's --------
	const float MistFactor = static_cast<float>(AnastasisSkyClock::MistFactorFor(Profile, LastSky));
	if (FMath::Abs(MistFactor - LastMistFactor) > 0.002f)
	{
		for (int32 I = 0; I < MistVolumes.Num(); ++I)
		{
			ULocalFogVolumeComponent* Component = IsValid(MistVolumes[I]) ? MistVolumes[I]->GetComponent() : nullptr;
			if (Component && MistBaseExtinction.IsValidIndex(I))
			{
				Component->SetRadialFogExtinction(MistBaseExtinction[I] * MistFactor);
				Component->SetHeightFogExtinction(MistBaseExtinction[I] * MistFactor);
			}
		}
		LastMistFactor = MistFactor;
	}
	// --- Aerosol: thinner while the sun grazes the horizon (ENV_REALISM_002) --------------
	// Only over the realism layer, which owns these two properties: with it off they hold the
	// engine defaults and nothing here may move them.
	if (bRealismApplied && SkyAtmosphere)
	{
		const float Aerosol = static_cast<float>(AnastasisSkyClock::TwilightScale(LastSky.Twilight, Profile.TwilightAerosolScale));
		if (FMath::Abs(Aerosol - LastAerosolScale) > 0.002f)
		{
			if (USkyAtmosphereComponent* SkyComponent = SkyAtmosphere->GetComponent())
			{
				SkyComponent->SetMieScatteringScale(Profile.SkyMieScatteringScale * Aerosol);
				SkyComponent->SetAerialPespectiveViewDistanceScale(Profile.SkyAerialPerspectiveDistanceScale * Aerosol);
			}
			LastAerosolScale = Aerosol;
		}
	}

	// --- Ground: rain wets the soil (ENV_REALISM_002) ------------------------------------
	const double PinnedWetness = CVarSkyGroundWetness.GetValueOnAnyThread();
	const float GroundWetness = (bWeather || PinnedWetness >= 0.0)
		? static_cast<float>(AnastasisSkyClock::GroundWetnessFor(Profile, LastSky, PinnedWetness)) : 0.0f;
	WriteGroundWetness(Profile, GroundWetness);

	if (UVolumetricCloudComponent* CloudComponent = Cloud ? Cloud->FindComponentByClass<UVolumetricCloudComponent>() : nullptr)
	{
		if (bWeather)
		{
			// Apply() hands the layer back its shared engine instance on every pass; the
			// dynamic instance is (re)made on top of whatever the component carries then.
			if (!CloudMaterialInstance || CloudComponent->GetMaterial() != CloudMaterialInstance)
			{
				if (UMaterialInterface* Base = Cast<UMaterialInterface>(Profile.CloudMaterial.TryLoad()))
				{
					CloudMaterialInstance = UMaterialInstanceDynamic::Create(Base, this);
					CloudComponent->SetMaterial(CloudMaterialInstance);
				}
			}
			if (CloudMaterialInstance)
			{
				CloudMaterialInstance->SetScalarParameterValue(TEXT("Cloud_GlobalCoverage"),
					static_cast<float>(AnastasisSkyClock::CloudCoverageFor(Profile, LastSky.Weather.Cover)));
			}
		}
	}

	const FString Phase = LastSky.VillagePhase;
	if (bForceLog || Phase != LastLoggedPhase)
	{
		LastLoggedPhase = Phase;
		UE_LOG(LogAnastasis_UnrealV2, Display,
			TEXT("ANASTASIS_SKY day=%.0f hour=%.2f phase=%s season=%s decl=%.2f sun_elev=%.2f moon_elev=%.2f ev100=%.2f ")
			TEXT("cover=%.3f rain=%.3f snow=%.3f humidity=%.3f wind=%.3f weather=%d mist_factor=%.3f ")
			TEXT("twilight=%.3f saturation=%.3f white_temp=%.0f ground_wetness=%.3f"),
			LastSky.Day, LastSky.Hours, *Phase, AnastasisWeather::SeasonId(LastSky.Weather.Season),
			LastSky.DeclinationDegrees, LastSky.SunElevationDegrees,
			AnastasisSkyClock::ElevationOf(LastSky.MoonRotation), LastSky.ExposureEV100,
			LastSky.Weather.Cover, LastSky.Weather.Rain, LastSky.Weather.Snow, LastSky.Humidity,
			LastSky.Weather.Wind, bWeather ? 1 : 0, MistFactor,
			LastSky.Twilight, LastSky.ColorSaturation, LastSky.WhiteTemp, GroundWetness);
	}
}

void AAnastasisWorldAtmosphere::WriteGroundWetness(const UAnastasisAtmosphereProfile& Profile, const float Wetness)
{
	// Unchanged, or the collection already found missing: a TryLoad of an absent asset is a disk
	// lookup, and the weather moves this value every tick in PIE. Apply() clears the refusal.
	if (FMath::Abs(Wetness - LastGroundWetnessWritten) <= 0.002f || bGroundWetnessMissingLogged)
	{
		return;
	}
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	if (!WeatherCollection)
	{
		WeatherCollection = Cast<UMaterialParameterCollection>(Profile.WeatherParameterCollection.TryLoad());
	}
	UMaterialParameterCollectionInstance* Instance = WeatherCollection ? World->GetParameterCollectionInstance(WeatherCollection) : nullptr;
	// A missing collection is a dry ground, said once -- not a crash, and not a silent no-op:
	// it means tools/unreal/ground-material.py has not been run since ENV_REALISM_002.
	if (!Instance || !Instance->SetScalarParameterValue(TEXT("RainWetness"), Wetness))
	{
		if (!bGroundWetnessMissingLogged)
		{
			// Display, not Warning: until the collection is generated and committed this is the
			// expected state of every run, and the editor log baseline must not learn it as noise.
			UE_LOG(LogAnastasis_UnrealV2, Display,
				TEXT("ANASTASIS_ATMOSPHERE ground_wetness_unavailable path=%s collection=%d (run tools/unreal/ground-material.ps1)"),
				*Profile.WeatherParameterCollection.ToString(), WeatherCollection ? 1 : 0);
			bGroundWetnessMissingLogged = true;
		}
		LastGroundWetnessWritten = Wetness;
		return;
	}
	LastGroundWetnessWritten = Wetness;
}

void AAnastasisWorldAtmosphere::ApplyRealism(const UAnastasisAtmosphereProfile& Profile, const bool bOn, const FRotator& SunRotation, const FRotator& MoonRotation)
{
	UWorld* World = GetWorld();
	bRealismApplied = bOn;
	if (!World)
	{
		return;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.ObjectFlags |= RF_Transient;

	// "Off" writes the engine's own component defaults: the rig and ATMOSPHERE_001/002 never
	// touched any of these properties, so the defaults ARE the pre-realism state, without this
	// actor having to remember anything about what it found.

	// --- Sun ---------------------------------------------------------------------------
	if (Sun)
	{
		if (UDirectionalLightComponent* SunComponent = Cast<UDirectionalLightComponent>(Sun->GetLightComponent()))
		{
			const UDirectionalLightComponent* D = GetDefault<UDirectionalLightComponent>();
			// Index 0 in both states: the moon takes 1, and two lights on one index would
			// make the sky atmosphere pick one arbitrarily.
			SunComponent->SetAtmosphereSunLightIndex(0);
			SunComponent->SetUseTemperature(bOn ? true : D->bUseTemperature);
			SunComponent->SetTemperature(bOn ? Profile.SunTemperatureKelvin : D->Temperature);
			SunComponent->bCastCloudShadows = bOn ? true : D->bCastCloudShadows;
			SunComponent->CloudShadowStrength = bOn ? Profile.SunCloudShadowStrength : D->CloudShadowStrength;
			SunComponent->MarkRenderStateDirty();
		}
	}

	// --- Sky atmosphere ----------------------------------------------------------------
	if (SkyAtmosphere)
	{
		if (USkyAtmosphereComponent* SkyComponent = SkyAtmosphere->GetComponent())
		{
			const USkyAtmosphereComponent* D = GetDefault<USkyAtmosphereComponent>();
			SkyComponent->SetMieScatteringScale(bOn ? Profile.SkyMieScatteringScale : D->MieScatteringScale);
			SkyComponent->SetAerialPespectiveViewDistanceScale(bOn ? Profile.SkyAerialPerspectiveDistanceScale : D->AerialPespectiveViewDistanceScale);
			SkyComponent->SetGroundAlbedo(bOn ? Profile.SkyGroundAlbedo : D->GroundAlbedo);
		}
	}

	// --- Sky light ---------------------------------------------------------------------
	if (SkyLight)
	{
		if (USkyLightComponent* SkyComponent = SkyLight->GetLightComponent())
		{
			const USkyLightComponent* D = GetDefault<USkyLightComponent>();
			SkyComponent->bCloudAmbientOcclusion = bOn ? Profile.bSkyLightCloudOcclusion : D->bCloudAmbientOcclusion;
			SkyComponent->MarkRenderStateDirty();
		}
	}

	// --- Fog ---------------------------------------------------------------------------
	if (Fog)
	{
		if (UExponentialHeightFogComponent* FogComponent = Fog->GetComponent())
		{
			const UExponentialHeightFogComponent* D = GetDefault<UExponentialHeightFogComponent>();
			FExponentialHeightFogData Valley = D->SecondFogData;
			if (bOn)
			{
				Valley.FogDensity = Profile.ValleyFogDensity;
				Valley.FogHeightFalloff = Profile.ValleyFogHeightFalloff;
				Valley.FogHeightOffset = Profile.ValleyFogHeightOffsetUU;
			}
			FogComponent->SetSecondFogData(Valley);

			const bool bVolumetric = bOn && Profile.bVolumetricFog;
			FogComponent->SetVolumetricFog(bVolumetric ? true : D->bEnableVolumetricFog);
			FogComponent->SetVolumetricFogDistance(bVolumetric ? Profile.VolumetricFogDistanceUU : D->VolumetricFogDistance);
			FogComponent->SetVolumetricFogExtinctionScale(bVolumetric ? Profile.VolumetricFogExtinctionScale : D->VolumetricFogExtinctionScale);
			FogComponent->SetVolumetricFogScatteringDistribution(bVolumetric ? Profile.VolumetricFogScatteringDistribution : D->VolumetricFogScatteringDistribution);
		}
	}

	// --- Moon --------------------------------------------------------------------------
	// Found by tag, never by class: see MoonTag. Removed with the layer off.
	Moon = FindTagged<ADirectionalLight>(World, MoonTag);
	if (bOn && Profile.bMoonEnabled)
	{
		if (!Moon)
		{
			Moon = World->SpawnActor<ADirectionalLight>(FVector(0.0, 0.0, 4200.0), MoonRotation, Params);
			if (Moon)
			{
				Moon->Tags.AddUnique(MoonTag);
#if WITH_EDITOR
				Moon->SetActorLabel(TEXT("Anastasis_Moon"));
#endif
			}
		}
		if (Moon)
		{
			MakeMovable(Moon->GetRootComponent());
			Moon->SetActorRotation(MoonRotation);
			if (UDirectionalLightComponent* MoonComponent = Cast<UDirectionalLightComponent>(Moon->GetLightComponent()))
			{
				MoonComponent->SetIntensity(Profile.MoonIlluminanceLux);
				MoonComponent->SetLightColor(FLinearColor::White);
				MoonComponent->SetUseTemperature(true);
				MoonComponent->SetTemperature(Profile.MoonTemperatureKelvin);
				MoonComponent->SetAtmosphereSunLight(true);
				MoonComponent->SetAtmosphereSunLightIndex(1);
				// A second shadow-casting directional light doubles the shadow cost for 0.3 lux.
				// It only earns its shadows once the sun has set.
				MoonComponent->SetCastShadows(AnastasisAtmosphere::IsBelowHorizon(SunRotation));
			}
		}
	}
	else if (Moon)
	{
		Moon->Destroy();
		Moon = nullptr;
	}

	// --- Clouds ------------------------------------------------------------------------
	// Only a layer this actor created is managed. A cloud layer authored into the level is the
	// level's: adopted neither on nor off, and reported.
	Cloud = FindTagged<AVolumetricCloud>(World, RealismCloudTag);
	const bool bLevelOwnsClouds = !Cloud && FindExisting<AVolumetricCloud>(World) != nullptr;
	if (bOn && Profile.bCloudsEnabled && !bLevelOwnsClouds)
	{
		if (!Cloud)
		{
			Cloud = World->SpawnActor<AVolumetricCloud>(FVector::ZeroVector, FRotator::ZeroRotator, Params);
			if (Cloud)
			{
				Cloud->Tags.AddUnique(RealismCloudTag);
#if WITH_EDITOR
				Cloud->SetActorLabel(TEXT("Anastasis_Clouds"));
#endif
			}
		}
		if (UVolumetricCloudComponent* CloudComponent = Cloud ? Cloud->FindComponentByClass<UVolumetricCloudComponent>() : nullptr)
		{
			CloudComponent->SetLayerBottomAltitude(Profile.CloudLayerBottomKm);
			CloudComponent->SetLayerHeight(Profile.CloudLayerHeightKm);
			// A missing engine material leaves the component with the engine's own default
			// rather than no clouds -- and says so, instead of silently shipping a blank sky.
			if (UMaterialInterface* CloudMaterial = Cast<UMaterialInterface>(Profile.CloudMaterial.TryLoad()))
			{
				CloudComponent->SetMaterial(CloudMaterial);
			}
			else
			{
				UE_LOG(LogAnastasis_UnrealV2, Warning,
					TEXT("ANASTASIS_ATMOSPHERE cloud_material_missing path=%s"), *Profile.CloudMaterial.ToString());
			}
		}
	}
	else if (Cloud)
	{
		Cloud->Destroy();
		Cloud = nullptr;
	}
	if (bLevelOwnsClouds)
	{
		UE_LOG(LogAnastasis_UnrealV2, Display, TEXT("ANASTASIS_ATMOSPHERE clouds=level_owned (not managed)"));
	}
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

	// The sky clock decides the sun when it is active; the fixed profile angles otherwise.
	bSkyClockActive = Profile.bSkyFollowsSimulation && IsSkyClockEnabledByCVar();
	if (bSkyClockActive)
	{
		uint32 Seed = 0;
		const double SimTime = ResolveSkySimTime(Seed);
		LastSky = AnastasisSkyClock::Evaluate(Profile, SimTime, Seed);
		if (CVarSkyTwilight.GetValueOnAnyThread() == 0)
		{
			LastSky = AnastasisSkyClock::WithoutTwilight(Profile, LastSky);
		}
	}
	const FRotator SunRotation = bSkyClockActive ? LastSky.SunRotation : AnastasisAtmosphere::ResolveSunRotation(Profile);
	const FRotator MoonRotation = bSkyClockActive ? LastSky.MoonRotation : AnastasisAtmosphere::ResolveMoonRotation(Profile);

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
			// Night vision belongs to the sky clock (UpdateSky); the fixed rig never set it.
			ExposureVolume->Settings.bOverride_ColorSaturation = false;
			ExposureVolume->Settings.bOverride_WhiteTemp = false;
		}

		// Pinning the post-process exposure above says nothing to Lumen's CACHED lighting
		// (surface cache, and the SkyLight's real-time-captured cubemap): that path pre-exposes
		// values against its own EV window -- 4.0 EV by default -- before storing them in a
		// limited-range buffer. At this project's fixed EV100=14 (75000 lux sun), that window is
		// far too narrow and the cached lighting clips, which is exactly the on-screen warning
		// ("adjust r.EyeAdaptation.CachedLightingPreExposure to match expected exposure range in
		// project"). Matching it to ExposureEV100 is that adjustment, kept here instead of an
		// .ini so it stays one profile edit away like every other exposure knob.
		if (IConsoleVariable* CachedLightingPreExposure =
			IConsoleManager::Get().FindConsoleVariable(TEXT("r.EyeAdaptation.CachedLightingPreExposure")))
		{
			CachedLightingPreExposure->Set(Profile.ExposureEV100, ECVF_SetByCode);
		}
	}

	// --- Realism (ENV_REALISM_001) -----------------------------------------------------
	// After the five rig actors, because it writes into them. Run in both states: "off" is an
	// active restore, not a skip.
	ApplyRealism(Profile, Profile.bRealismEnabled && IsRealismEnabledByCVar(), SunRotation, MoonRotation);

	// --- Sky clock (DAY_NIGHT_WEATHER_001) ----------------------------------------------
	// Last, because it overrides what the fixed profile just wrote: exposure by sun
	// elevation, fog by humidity, clouds by cover. With the clock off, nothing below runs and
	// the image is the fixed rig's, exactly as before.
	if (bSkyClockActive)
	{
		// Apply() just wrote the profile's day EV into the volume: the cache of what the
		// clock last wrote is stale, and must not suppress the rewrite.
		LastExposureWritten = TNumericLimits<float>::Lowest();
		LastSaturationWritten = TNumericLimits<float>::Lowest();
		LastWhiteTempWritten = TNumericLimits<float>::Lowest();
		// ApplyRealism just wrote the profile's aerosol: same reasoning.
		LastAerosolScale = -1.0f;
		// A collection generated since the last Apply() gets one more look.
		bGroundWetnessMissingLogged = false;
		UpdateSky(Profile, /*bForceLog*/ true);
	}
	else if (LastGroundWetnessWritten > 0.0f)
	{
		// Without the clock there is no weather: a ground the clock had wetted dries.
		WriteGroundWetness(Profile, 0.0f);
	}
	if (!bSkyClockActive && CloudMaterialInstance)
	{
		// The clock was on and is now off: give the cloud layer its profile material back.
		if (UVolumetricCloudComponent* CloudComponent = Cloud ? Cloud->FindComponentByClass<UVolumetricCloudComponent>() : nullptr)
		{
			CloudComponent->SetMaterial(Cast<UMaterialInterface>(Profile.CloudMaterial.TryLoad()));
		}
		CloudMaterialInstance = nullptr;
	}

	LastSummary = FString::Printf(
		TEXT("ANASTASIS_ATMOSPHERE applied=1 profile=%s sun_source=%s sun_pitch=%.3f sun_yaw=%.3f lux=%.1f ")
		TEXT("fog=%d fog_density=%.4f ev100=%.2f adopted=%d spawned=%d realism=%d moon=%d clouds=%d volumetric_fog=%d"),
		Source,
		Profile.bDeriveSunFromTimeOfDay ? TEXT("time_of_day") : TEXT("explicit"),
		SunRotation.Pitch, SunRotation.Yaw, Profile.SunIntensityLux,
		Profile.bFogEnabled ? 1 : 0, Profile.FogDensity, Profile.ExposureEV100,
		AdoptedCount, SpawnedCount,
		bRealismApplied ? 1 : 0, Moon ? 1 : 0, Cloud ? 1 : 0,
		(Fog && Fog->GetComponent() && Fog->GetComponent()->bEnableVolumetricFog) ? 1 : 0);
	UE_LOG(LogAnastasis_UnrealV2, Display, TEXT("%s"), *LastSummary);

	return true;
}

int32 AAnastasisWorldAtmosphere::ApplyMist()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return 0;
	}

	// Rebuild, never accumulate: a second pass must replace the field, not lay a second one
	// over it. (The five singleton rig actors above are adopted instead — different problem.)
	for (const TObjectPtr<ALocalFogVolume>& Volume : MistVolumes)
	{
		if (IsValid(Volume))
		{
			Volume->Destroy();
		}
	}
	MistVolumes.Reset();
	MistBaseExtinction.Reset();
	LastMistFactor = -1.0f;

	// The CVar is the A/B switch: without a way to turn the mist off from outside the data,
	// "the mist changed this image" would be an assertion rather than a measurement.
	if (CVarMist.GetValueOnAnyThread() == 0)
	{
		UE_LOG(LogAnastasis_UnrealV2, Display, TEXT("ANASTASIS_MIST pockets=0 reason=cvar_off"));
		return 0;
	}

	const UAnastasisAtmosphereProfile& Profile = AnastasisAtmosphere::GetProfile();
	if (!Profile.bEnabled || !Profile.bMistEnabled)
	{
		UE_LOG(LogAnastasis_UnrealV2, Display,
			TEXT("ANASTASIS_MIST pockets=0 reason=%s"),
			Profile.bEnabled ? TEXT("mist_disabled") : TEXT("profile_disabled"));
		return 0;
	}

	AAnastasisWorldEmbodiment* Embodiment = FindExisting<AAnastasisWorldEmbodiment>(World);
	if (!Embodiment)
	{
		// A refusal with a reason, not a crash and not silence: mist is read from the world's
		// tiles, so a world that was never embodied has nothing to be wet.
		UE_LOG(LogAnastasis_UnrealV2, Warning,
			TEXT("ANASTASIS_MIST pockets=0 reason=no_embodiment"));
		return 0;
	}

	const AnastasisWorldView::FWorldVisualSnapshot& Snapshot = Embodiment->GetSnapshot();
	if (Snapshot.Tiles.Num() == 0)
	{
		UE_LOG(LogAnastasis_UnrealV2, Warning,
			TEXT("ANASTASIS_MIST pockets=0 reason=empty_snapshot"));
		return 0;
	}

	AnastasisMist::FMistParams Params;
	Params.CellTiles = Profile.MistCellTiles;
	Params.WetnessThreshold = Profile.MistWetnessThreshold;
	Params.VolumeRadiusFraction = Profile.MistVolumeRadiusFraction;
	Params.MaxVolumes = Profile.MistMaxVolumes;

	bool bTruncated = false;
	const TArray<AnastasisMist::FMistPocket> Pockets =
		AnastasisMist::BuildMistField(Snapshot, Params, &bTruncated);

	FActorSpawnParameters Params2;
	Params2.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params2.ObjectFlags |= RF_Transient;

	int32 GroundSampled = 0;
	for (const AnastasisMist::FMistPocket& Pocket : Pockets)
	{
		FVector Location = Pocket.Location;

		// The tile altitude is a step; the rendered ground is a slope. Reuse the terrain
		// lane's own sampler so the mist lies on exactly the surface the trees stand on. It
		// refuses outside the built footprint, and that refusal is honoured: the pocket then
		// falls back to the tile altitude it was born with rather than being dropped, since
		// the DEBUG slab mode has no continuous surface at all.
		double GroundZ = 0.0;
		if (AnastasisTerrainForge::SampleActive(Location.X, Location.Y, GroundZ)
			|| AnastasisTerrainSurface::SampleHeight(Snapshot, Location.X, Location.Y, GroundZ))
		{
			Location.Z = GroundZ;
			++GroundSampled;
		}
		Location.Z += Profile.MistGroundOffsetUU;

		ALocalFogVolume* Volume = World->SpawnActor<ALocalFogVolume>(Location, FRotator::ZeroRotator, Params2);
		if (!Volume)
		{
			continue;
		}

		// ULocalFogVolumeComponent's volume is a unit sphere of GetBaseVolumeSize() uu scaled
		// by the transform, so the radius we want has to go through that constant rather than
		// be written as a world size.
		const double Scale = Pocket.RadiusUU / static_cast<double>(ULocalFogVolumeComponent::GetBaseVolumeSize());
		// Under the sky clock a pocket is a bank, not a dome (MistVerticalScale). With the clock
		// off it keeps its ATMOSPHERE_002 sphere, so Sky.Clock 0 stays the image it was.
		Volume->SetActorScale3D(FVector(Scale, Scale, bSkyClockActive ? Scale * Profile.MistVerticalScale : Scale));

		// Thickness follows wetness: the wettest cell gets MistMaxExtinction, a cell barely over
		// the threshold gets almost nothing. Identical pockets everywhere would be decoration;
		// this is the simulation showing through.
		const float Extinction = static_cast<float>(Pocket.Density01) * Profile.MistMaxExtinction;
		if (ULocalFogVolumeComponent* Component = Volume->GetComponent())
		{
			Component->SetRadialFogExtinction(Extinction);
			Component->SetHeightFogExtinction(Extinction);
			Component->SetHeightFogFalloff(Profile.MistHeightFalloff);
			Component->SetFogPhaseG(Profile.MistPhaseG);
			Component->SetFogAlbedo(Profile.MistAlbedo);
		}

		MistVolumes.Add(Volume);
		MistBaseExtinction.Add(Extinction);
	}

	// The clock decides how much of that mist the hour and the weather allow: apply it now
	// rather than leave a full-strength field on screen until the next tick.
	if (bSkyClockActive)
	{
		UpdateSky(Profile, /*bForceLog*/ false);
	}

	UE_LOG(LogAnastasis_UnrealV2, Display,
		TEXT("ANASTASIS_MIST pockets=%d cell_tiles=%d threshold=%.3f crop=%dx%d ground_sampled=%d ")
		TEXT("max_extinction=%.3f truncated=%d"),
		MistVolumes.Num(), Params.CellTiles, Params.WetnessThreshold,
		Snapshot.W, Snapshot.H, GroundSampled, Profile.MistMaxExtinction, bTruncated ? 1 : 0);

	if (bTruncated)
	{
		UE_LOG(LogAnastasis_UnrealV2, Warning,
			TEXT("ANASTASIS_MIST truncated at MistMaxVolumes=%d: the wettest cells were kept, the rest have no fog"),
			Params.MaxVolumes);
	}

	return MistVolumes.Num();
}
