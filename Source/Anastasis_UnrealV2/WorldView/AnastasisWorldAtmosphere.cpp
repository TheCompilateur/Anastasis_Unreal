#include "WorldView/AnastasisWorldAtmosphere.h"

#include "Anastasis_UnrealV2.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/LocalFogVolumeComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/LocalFogVolume.h"
#include "Engine/PostProcessVolume.h"
#include "Components/VolumetricCloudComponent.h"
#include "Engine/SkyLight.h"
#include "Engine/StaticMesh.h"
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
#include "WorldView/AnastasisRain.h"
#include "WorldView/AnastasisSkyPassage.h"
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

static TAutoConsoleVariable<int32> CVarSkyPassage(
	TEXT("anastasis.Sky.Passage"), 1,
	TEXT("SKY_CONTINUITY_002. Surface sun -> atmospheric twilight -> surface moon; exposure protection at dawn. 0 restores the previous transition for A/B. No simulation clock changes."),
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

static TAutoConsoleVariable<float> CVarSkyHumidity(
	TEXT("anastasis.Sky.Humidity"),
	-1.0f,
	TEXT("Pins the humidity the SKY shows [0,1] (fog density, mist) for captures; -1 follows the simulation's weather. Never moves the simulation."),
	ECVF_Default);

// Presentation only: no writes to AnastasisWeather or simulation RNG.
static TAutoConsoleVariable<int32> CVarWeatherCoupling(
    TEXT("anastasis.Atmosphere.Coupling"), 1,
    TEXT("Shared visual weather in foliage, grass and water. 0=authored legacy material response for A/B."), ECVF_Default);
static TAutoConsoleVariable<int32> CVarAirVisibility(
    TEXT("anastasis.Atmosphere.AirVisibility"), 1,
    TEXT("0=authored global fog, 1=less global haze in unsaturated visual weather; local wetness mist preserved. Requires Coupling."), ECVF_Default);
static TAutoConsoleVariable<float> CVarSkyWind(
    TEXT("anastasis.Sky.Wind"), -1.0f,
    TEXT("Visual wind strength [0,1]; -1 follows the same blended weather as the clouds."), ECVF_Default);
static TAutoConsoleVariable<float> CVarSkyCover(
    TEXT("anastasis.Sky.Cover"), -1.0f,
    TEXT("Visual cloud cover [0,1]; -1 follows simulation weather."), ECVF_Default);
static TAutoConsoleVariable<float> CVarWindHeading(
    TEXT("anastasis.Sky.WindHeading"), 26.565f,
    TEXT("Visual downwind heading in world XY degrees. The simulation has intensity, no direction."), ECVF_Default);

static TAutoConsoleVariable<int32> CVarRealism(
	TEXT("anastasis.Atmosphere.Realism"),
	1,
	TEXT("ENV_REALISM_001 layer: tuned sky, clouds, moon, valley fog, volumetric fog. 0=engine defaults for every property it owns (the pre-realism image), 1=profile values; read on every Apply()."),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarEyePlane(
	TEXT("anastasis.Depth.EyePlane"),
	1,
	TEXT("EYE_PLANE_001. 1=at eye height the first 7 m stay outside the volumetric fog, then local colour (olive, pebble, wall) falls toward the air out to about 160 m. 0=the profile's own aerial scale, fog and extinction. Read on every Apply()."),
	ECVF_Default);

// CONTINENTAL_001. L'echelle de perspective aerienne du profil (3,0, x1,15 au niveau de l'oeil) a ete choisie quand
// le monde faisait 1,9 km : une crete a 1,5 km ne recevait presque aucune diffusion. Il y a desormais des chaines a
// 14 et 40 km (AnastasisTectonics), et cette echelle les noie dans un blanc de craie (captures
// Saved/HorizonEvidence : echelle 3,45 = voile uniforme, 1,7 = encore pale, 1,0 = ombres, couloirs et roche lisibles ; le brouillard
// exponentiel n'y est pour presque rien). Plafond : on ne depasse jamais cette valeur ; 0 = aucun plafond.
static TAutoConsoleVariable<float> CVarAerialCap(
	TEXT("anastasis.Atmosphere.AerialCap"),
	1.2f,
	TEXT("CONTINENTAL_001. Plafond de l'echelle de perspective aerienne (profil x EyePlane compris) : les chaines lointaines gardent leur contraste. 0=aucun plafond. Read on every Apply()."),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarFogScattering(
	TEXT("anastasis.Atmosphere.FogScattering"),
	0,
	TEXT("FOG_FSSS_001: Fog Screen Space Scattering (UE 5.8, experimental) on the height fog, inside the realism layer. 0=engine default (off), 1=profile values; read on every Apply()."),
	ECVF_Default);

static TAutoConsoleVariable<float> CVarFogScatteringSceneColor(
	TEXT("anastasis.Atmosphere.FogScattering.SceneColor"),
	-1.0f,
	TEXT("FOG_FSSS_001 capture override of the profile's FogScatteringSceneColorScale (share of scene colour fed into the scattering); -1 = the profile. Read on every Apply()."),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarWeatherRain(
	TEXT("anastasis.Weather.Rain"),
	1,
	TEXT("RAIN_001. 1=the simulation's rain falls as streaks around the camera, 0=no rain drawn (the image before). Read on every Apply() and Tick."),
	ECVF_Default);

static TAutoConsoleVariable<float> CVarSkyRain(
	TEXT("anastasis.Sky.Rain"),
	-1.0f,
	TEXT("RAIN_001. Pins the rain the SKY shows [0,1] for captures; -1 follows the simulation's weather. Never moves the simulation."),
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

	/** Capture pins that change what the sky SHOWS, never the simulation (cf. anastasis.Sky.Hour). */
	void ApplySkyPins(AnastasisSkyClock::FSkyState& State)
	{
        if (CVarSkyWind.GetValueOnAnyThread() >= 0.0f)
            State.SkyWind = FMath::Clamp(double(CVarSkyWind.GetValueOnAnyThread()), 0.0, 1.0);
        if (CVarSkyCover.GetValueOnAnyThread() >= 0.0f)
            State.SkyCover = FMath::Clamp(double(CVarSkyCover.GetValueOnAnyThread()), 0.0, 1.0);
		const float PinnedHumidity = CVarSkyHumidity.GetValueOnAnyThread();
		if (PinnedHumidity >= 0.0f)
		{
			State.SkyHumidity = FMath::Clamp(static_cast<double>(PinnedHumidity), 0.0, 1.0);
		}
	}

	UDirectionalLightComponent* DirectionalComponentOf(ADirectionalLight* Light)
	{
		return Light ? Cast<UDirectionalLightComponent>(Light->GetLightComponent()) : nullptr;
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
	RestorePassage();
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
		RestorePassage();
		return;
	}
	const UAnastasisAtmosphereProfile& Profile = AnastasisAtmosphere::GetProfile();
	// The CVar is re-read here too: switching the clock off in a live session freezes the
	// sky where it is until the next Apply() restores the fixed rig.
	if (!Profile.bEnabled || !Profile.bSkyFollowsSimulation || !IsSkyClockEnabledByCVar())
	{
		RestorePassage();
		return;
	}
	uint32 Seed = 0;
	const double SimTime = ResolveSkySimTime(Seed);
	LastSky = AnastasisSkyClock::Evaluate(Profile, SimTime, Seed);
	ApplySkyPins(LastSky);
	UpdateSky(Profile, /*bForceLog*/ false, DeltaSeconds);
}

void AAnastasisWorldAtmosphere::ArbitrateDirectionalLights(const bool bMoonLeads, const double SunFogScattering)
{
	// Without a moon there is nothing to arbitrate against: the sun leads whatever the hour.
	const bool bMoonLeadsNow = bMoonLeads && Moon != nullptr;
	if (UDirectionalLightComponent* SunComponent = DirectionalComponentOf(Sun))
	{
		const int32 Priority = bMoonLeadsNow ? AnastasisSkyClock::ForwardPriorityFollow : AnastasisSkyClock::ForwardPriorityLead;
		if (SunComponent->ForwardShadingPriority != Priority)
		{
			SunComponent->SetForwardShadingPriority(Priority);
		}
		// The engine default (1) times the share the fog may scatter: a set sun lights no fog.
		const float Scattering = static_cast<float>(FMath::Clamp(SunFogScattering, 0.0, 1.0));
		if (!FMath::IsNearlyEqual(SunComponent->VolumetricScatteringIntensity, Scattering, 1e-4f))
		{
			SunComponent->SetVolumetricScatteringIntensity(Scattering);
		}
	}
	if (UDirectionalLightComponent* MoonComponent = DirectionalComponentOf(Moon))
	{
		const int32 Priority = bMoonLeadsNow ? AnastasisSkyClock::ForwardPriorityLead : AnastasisSkyClock::ForwardPriorityFollow;
		if (MoonComponent->ForwardShadingPriority != Priority)
		{
			MoonComponent->SetForwardShadingPriority(Priority);
		}
	}
	bMoonLeadsForward = bMoonLeadsNow;
}

void AAnastasisWorldAtmosphere::FPassageLight::Restore()
{
	if (ULightComponent* Light = Component.Get())
	{
		Light->SetDiffuseScale(Diffuse);
		Light->SetSpecularScale(Specular);
		Light->SetIndirectLightingIntensity(Indirect);
		if (bScaledFog) Light->SetVolumetricScatteringIntensity(Fog);
	}
	Component.Reset();
}

void AAnastasisWorldAtmosphere::FPassageLight::Write(ULightComponent* Light, const float Weight, const bool bScaleFog)
{
	if (Component.Get() != Light)
	{
		Restore();
		if (!Light) return;
		Component = Light;
		Diffuse = Light->DiffuseScale;
		Specular = Light->SpecularScale;
		Indirect = Light->IndirectLightingIntensity;
		Fog = Light->VolumetricScatteringIntensity;
		bScaledFog = bScaleFog;
	}
	if (!Light) return;
	const float W = FMath::Clamp(Weight, 0.0f, 1.0f);
	// Only attenuate authored surface energy; never multiply atmospheric lux.
	if (!FMath::IsNearlyEqual(Light->DiffuseScale, Diffuse * W, 1e-6f)) Light->SetDiffuseScale(Diffuse * W);
	if (!FMath::IsNearlyEqual(Light->SpecularScale, Specular * W, 1e-6f)) Light->SetSpecularScale(Specular * W);
	if (!FMath::IsNearlyEqual(Light->IndirectLightingIntensity, Indirect * W, 1e-6f)) Light->SetIndirectLightingIntensity(Indirect * W);
	if (bScaledFog && !FMath::IsNearlyEqual(Light->VolumetricScatteringIntensity, Fog * W, 1e-6f))
		Light->SetVolumetricScatteringIntensity(Fog * W);
}

void AAnastasisWorldAtmosphere::RestorePassage()
{
	PassageSun.Restore();
	PassageMoon.Restore();
}

void AAnastasisWorldAtmosphere::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	RestorePassage();
	Super::EndPlay(EndPlayReason);
}

void AAnastasisWorldAtmosphere::UpdateSky(const UAnastasisAtmosphereProfile& Profile, const bool bForceLog, const float AdaptSeconds)
{
	const bool bPassage = CVarSkyPassage.GetValueOnGameThread() != 0;
	// The legacy symmetric lag could retain night sensitivity after the sun had risen.
	// Passage permits slow dark adaptation, but never that positive exposure mismatch.
	AppliedExposureEV = (AdaptSeconds > 0.0f && bHasAppliedExposure)
		? (bPassage
			? AnastasisSkyPassage::Exposure(AppliedExposureEV, LastSky.ExposureEV100, AdaptSeconds, Profile.MaxExposureChangePerSecond)
			: AnastasisSkyClock::AdaptExposure(AppliedExposureEV, LastSky.ExposureEV100, AdaptSeconds, Profile.MaxExposureChangePerSecond))
		: LastSky.ExposureEV100;
	bHasAppliedExposure = true;

	const AnastasisSkyPassage::FRelay Relay = AnastasisSkyPassage::Resolve(LastSky.SunElevationDegrees,
		AnastasisSkyClock::ElevationOf(LastSky.MoonRotation), Profile.PassageSunFullElevation, Profile.PassageMoonFullElevation);
	if (bPassage)
	{
		PassageSun.Write(DirectionalComponentOf(Sun), static_cast<float>(Relay.Sun), false);
		PassageMoon.Write(DirectionalComponentOf(Moon), static_cast<float>(Relay.Moon), true);
	}
	else RestorePassage();

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

	// --- One forward light: the sun while it is up, the moon after ---------------------
	ArbitrateDirectionalLights(LastSky.bMoonLeadsForward, LastSky.SunFogScattering);

	// --- Exposure: pinned, but pinned to the hour --------------------------------------
	if (Profile.bFixedExposure && ExposureVolume)
	{
		const float EV = static_cast<float>(AppliedExposureEV);
		if (FMath::Abs(EV - LastExposureWritten) > 0.005f)
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
			LastExposureWritten = EV;
		}
		{
			// These follow celestial state even while EV is on a plateau.
			// Night vision on the same curve: colour fades and the white point follows the moon.
			const float Sat = static_cast<float>(LastSky.ColorSaturation);
			ExposureVolume->Settings.bOverride_ColorSaturation = true;
			ExposureVolume->Settings.ColorSaturation = FVector4(Sat, Sat, Sat, 1.0f);
			ExposureVolume->Settings.bOverride_WhiteTemp = true;
			ExposureVolume->Settings.WhiteTemp = static_cast<float>(LastSky.WhiteTemp);
			// Twilight: the sky is the only light and far brighter than the ground it lights.
			// Local exposure compresses that sky instead of a lower EV darkening the land. The
			// day value is the project's own, read where the project sets it.
			if (IConsoleVariable* DayHighlight =
				IConsoleManager::Get().FindConsoleVariable(TEXT("r.DefaultFeature.LocalExposure.HighlightContrastScale")))
			{
				ExposureVolume->Settings.bOverride_LocalExposureHighlightContrastScale = true;
				ExposureVolume->Settings.LocalExposureHighlightContrastScale = static_cast<float>(
					AnastasisSkyClock::HighlightContrastFor(Profile, LastSky.Daylight, DayHighlight->GetFloat()));
			}
		}
	}

	// --- Weather: clouds and fog -------------------------------------------------------
	const bool bWeather = Profile.bWeatherDrivesSky && CVarSkyWeather.GetValueOnAnyThread() != 0;
    // One world-local uniform buffer, shared by all vegetation and water instances.
    // Soft asset reference is serialized on the actor CDO for cooking; failed load logs once.
    if (!WeatherCollection && !bWeatherCollectionLoadAttempted)
    {
        bWeatherCollectionLoadAttempted = true;
        WeatherCollection = WeatherParameters.LoadSynchronous();
        if (!WeatherCollection)
            UE_LOG(LogAnastasis_UnrealV2, Warning, TEXT("ANASTASIS_WEATHER missing_collection rebuild weather-materials.py"));
    }
    const float Wind = bWeather ? float(LastSky.SkyWind) : 0.3f;
    const float Humidity = bWeather ? float(LastSky.SkyHumidity) : 0.45f;
    const float Cover = bWeather ? float(LastSky.SkyCover) : 0.25f;
    const bool bCoupled = CVarWeatherCoupling.GetValueOnAnyThread() != 0;
    const float Heading = FMath::DegreesToRadians(CVarWindHeading.GetValueOnAnyThread());
    // Dampness is a bounded visual readiness signal, not rain accumulation or hydrology.
    const float Dampness = FMath::Clamp((Humidity - 0.65f) / 0.35f, 0.0f, 1.0f);
    if (WeatherCollection)
    {
        UMaterialParameterCollectionInstance* Parameters = GetWorld()->GetParameterCollectionInstance(WeatherCollection);
        Parameters->SetVectorParameterValue(TEXT("WeatherWind"), FLinearColor(FMath::Cos(Heading), FMath::Sin(Heading), Wind, 0));
        Parameters->SetVectorParameterValue(TEXT("WeatherAir"), FLinearColor(Humidity, Dampness, Cover, 0));
        Parameters->SetScalarParameterValue(TEXT("WeatherCoupling"), bCoupled ? 1.0f : 0.0f);
    }
    // Mie scattering remains owned by ApplyRealism and its authored profile.
    // Weather must not overwrite the reversible profile contract here.
    if (bForceLog)
        UE_LOG(LogAnastasis_UnrealV2, Display, TEXT("ANASTASIS_WEATHER coupled=%d collection=%d wind=%.3f heading=%.2f humidity=%.3f cover=%.3f dampness=%.3f"),
            bCoupled ? 1 : 0, WeatherCollection ? 1 : 0, Wind, CVarWindHeading.GetValueOnAnyThread(), Humidity, Cover, Dampness);
	if (Fog)
	{
		if (UExponentialHeightFogComponent* FogComponent = Fog->GetComponent())
		{
			const double Scale = bWeather ? AnastasisSkyClock::FogDensityScaleFor(Profile, LastSky.SkyHumidity) : 1.0;
			// Global aerosol veil is weaker outside saturated weather. Local valley pockets
            // retain their own terrain/wetness signal; they are not flattened into this layer.
            const float Visibility = bCoupled && CVarAirVisibility.GetValueOnAnyThread() != 0
                ? 0.4f + 0.6f * Humidity * Humidity : 1.0f;
            const float Density = static_cast<float>(Profile.FogDensity * Scale) * Visibility;
			if (!FMath::IsNearlyEqual(FogComponent->FogDensity, Density, 1e-6f))
			{
				FogComponent->SetFogDensity(Density);
			}

			// The authored inscattering is an absolute luminance tuned for the day's EV: it has
			// to dim with the light, or the night fog glows (see FogInscatteringScaleFor).
			const double FogEV = bPassage ? LastSky.ExposureEV100 : AppliedExposureEV;
			const float Inscatter = static_cast<float>(AnastasisSkyClock::FogInscatteringScaleFor(FogEV, Profile.ExposureEV100));
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
					static_cast<float>(AnastasisSkyClock::CloudCoverageFor(Profile, LastSky.SkyCover)));
                // Engine material node description verified in Unreal: RGB = signed world-axis
                // wind strength, A = uniform strength multiplier. Preserve authored defaults for A/B.
                FLinearColor AuthoredWind;
                if (UMaterialInterface* Base = CloudMaterialInstance->Parent)
                {
                    if (Base->GetVectorParameterValue(FMaterialParameterInfo(TEXT("Layout_WindControls")), AuthoredWind))
                    {
                        const FLinearColor CloudWind = bCoupled
                            ? FLinearColor(FMath::Cos(Heading), FMath::Sin(Heading), 0.0f, Wind)
                            : AuthoredWind;
                        CloudMaterialInstance->SetVectorParameterValue(TEXT("Layout_WindControls"), CloudWind);
                    }
                }
			}
		}
	}

	UpdateRain(LastSky, bWeather);

	const FString Phase = LastSky.VillagePhase;
	if (bForceLog || Phase != LastLoggedPhase)
	{
		LastLoggedPhase = Phase;
		UE_LOG(LogAnastasis_UnrealV2, Display,
			TEXT("ANASTASIS_SKY day=%.0f hour=%.2f phase=%s season=%s decl=%.2f sun_elev=%.2f moon_elev=%.2f ev100=%.2f ")
			TEXT("cover=%.3f rain=%.3f snow=%.3f humidity=%.3f wind=%.3f weather=%d mist_factor=%.3f ")
			TEXT("sky_humidity=%.3f sky_wind=%.3f sky_cover=%.3f forward_light=%s sun_fog_scatter=%.3f ")
			TEXT("passage=%d applied_ev=%.3f exposure_lag=%.3f surface_sun=%.5f surface_moon=%.5f twilight=%.5f"),
			LastSky.Day, LastSky.Hours, *Phase, AnastasisWeather::SeasonId(LastSky.Weather.Season),
			LastSky.DeclinationDegrees, LastSky.SunElevationDegrees,
			AnastasisSkyClock::ElevationOf(LastSky.MoonRotation), LastSky.ExposureEV100,
			LastSky.Weather.Cover, LastSky.Weather.Rain, LastSky.Weather.Snow, LastSky.Humidity,
			LastSky.Weather.Wind, bWeather ? 1 : 0, MistFactor,
			LastSky.SkyHumidity, LastSky.SkyWind, LastSky.SkyCover,
			bMoonLeadsForward ? TEXT("moon") : TEXT("sun"), LastSky.SunFogScattering,
			bPassage ? 1 : 0, AppliedExposureEV, AppliedExposureEV - LastSky.ExposureEV100,
			bPassage ? Relay.Sun : 1.0, bPassage ? Relay.Moon : 1.0, bPassage ? Relay.Twilight : 0.0);
	}
}

void AAnastasisWorldAtmosphere::UpdateRain(const AnastasisSkyClock::FSkyState& Sky, const bool bWeatherDrivesSky)
{
	const bool bEnabled = CVarWeatherRain.GetValueOnGameThread() != 0;
	// WEATHER_TRANSITION_002: clouds and visible rain share the same calendar blend.
	// Keep the simulation weather untouched, including the raw rain logged below.
	AnastasisWeather::FWeather VisualWeather = Sky.Weather;
	VisualWeather.Rain = Sky.SkyRain;
	const AnastasisRain::FRainVisual Rain = AnastasisRain::VisualFor(
		VisualWeather, bWeatherDrivesSky, static_cast<double>(CVarSkyRain.GetValueOnGameThread()));
	const double Amount = bEnabled ? Rain.Amount : 0.0;

	if (Amount > 0.0 && !RainStreaks)
	{
		UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, AnastasisRain::MeshPath);
		UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, AnastasisRain::MaterialPath);
		if (!Mesh || !Material)
		{
			if (!bRainAssetsMissingLogged)
			{
				UE_LOG(LogAnastasis_UnrealV2, Warning,
					TEXT("ANASTASIS_RAIN assets missing (mesh=%d material=%d): run tools/unreal/rain-material.ps1"),
					Mesh ? 1 : 0, Material ? 1 : 0);
				bRainAssetsMissingLogged = true;
			}
			LastRainAmount = 0.0;
			return;
		}
		RainStreaks = NewObject<UInstancedStaticMeshComponent>(this, TEXT("AnastasisRainStreaks"), RF_Transient);
		RainStreaks->SetupAttachment(GetRootComponent());
		RainStreaks->SetMobility(EComponentMobility::Movable);
		RainStreaks->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		RainStreaks->SetGenerateOverlapEvents(false);
		RainStreaks->SetCanEverAffectNavigation(false);
		RainStreaks->SetCastShadow(false);
		RainStreaks->bAffectDistanceFieldLighting = false;
		RainStreaks->bVisibleInRayTracing = false;
		RainStreaks->bVisibleInReflectionCaptures = false;
		RainStreaks->bVisibleInRealTimeSkyCaptures = false;
		RainStreaks->SetStaticMesh(Mesh);
		RainMaterialInstance = UMaterialInstanceDynamic::Create(Material, this);
		RainStreaks->SetMaterial(0, RainMaterialInstance);
		RainStreaks->NumCustomDataFloats = 3;
		RainStreaks->RegisterComponent();

		// Every streak sits on the actor; the material places it. The start positions in the
		// unit box are seeded: the same rain, streak for streak, in every session and capture.
		FRandomStream Stream(12345);
		TArray<FTransform> Transforms;
		Transforms.Init(FTransform::Identity, AnastasisRain::StreakCount);
		RainStreaks->AddInstances(Transforms, /*bShouldReturnIndices*/ false, /*bWorldSpace*/ false);
		for (int32 I = 0; I < AnastasisRain::StreakCount; ++I)
		{
			const float Data[3] = { Stream.FRand(), Stream.FRand(), Stream.FRand() };
			RainStreaks->SetCustomData(I, MakeArrayView(Data, 3), /*bMarkRenderStateDirty*/ false);
		}
		RainStreaks->MarkRenderStateDirty();
		RainMaterialInstance->SetVectorParameterValue(TEXT("Box"), FLinearColor(
			AnastasisRain::BoxUU.X, AnastasisRain::BoxUU.Y, AnastasisRain::BoxUU.Z, 0.0f));
		RainMaterialInstance->SetScalarParameterValue(TEXT("Len"), static_cast<float>(AnastasisRain::StreakLengthUU));
		RainMaterialInstance->SetScalarParameterValue(TEXT("Width"), static_cast<float>(AnastasisRain::StreakWidthUU));
		RainMaterialInstance->SetScalarParameterValue(TEXT("Opacity"), static_cast<float>(AnastasisRain::StreakOpacity));
	}

	if (RainStreaks)
	{
		const bool bVisible = Amount > 0.0;
		if (RainStreaks->IsVisible() != bVisible)
		{
			RainStreaks->SetVisibility(bVisible);
		}
		if (RainMaterialInstance && bVisible)
		{
			RainMaterialInstance->SetScalarParameterValue(TEXT("RainAmount"), static_cast<float>(Amount));
			RainMaterialInstance->SetScalarParameterValue(TEXT("Fall"), static_cast<float>(Rain.FallUUPerSecond));
			RainMaterialInstance->SetVectorParameterValue(TEXT("Wind"), FLinearColor(
				static_cast<float>(Rain.WindUUPerSecond.X), static_cast<float>(Rain.WindUUPerSecond.Y), 0.0f, 0.0f));
		}
	}

	// One line when the rain starts, stops or moves by a twentieth: not one per tick.
	if (FMath::Abs(Amount - LastRainAmount) >= 0.05 || ((Amount > 0.0) != (LastRainAmount > 0.0)))
	{
		UE_LOG(LogAnastasis_UnrealV2, Display, TEXT("ANASTASIS_RAIN amount=%.3f sim_rain=%.3f wind_uu=(%.0f,%.0f) enabled=%d pinned=%d"),
			Amount, Sky.Weather.Rain, Rain.WindUUPerSecond.X, Rain.WindUUPerSecond.Y, bEnabled ? 1 : 0,
			CVarSkyRain.GetValueOnGameThread() >= 0.0f ? 1 : 0);
	}
	LastRainAmount = Amount;
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
			SkyComponent->SetAerialPerspectiveStartDepth(bOn ? Profile.SkyAerialPerspectiveStartDepthKm : D->AerialPerspectiveStartDepth);
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

			// FSSS has no setter in 5.8: write the properties, then rebuild the fog's render
			// state ourselves -- only on a change, Apply() runs again on every sky pin.
			const bool bScatter = bOn && Profile.bFogScreenSpaceScattering && CVarFogScattering.GetValueOnAnyThread() != 0;
			const float Spread = bScatter ? Profile.FogScatteringSpreadScale : D->FSSSSpreadScale;
			const float SceneColorPin = CVarFogScatteringSceneColor.GetValueOnAnyThread();
			const float SceneColor = !bScatter ? D->FSSSSceneColorScatteringAmountScale
				: (SceneColorPin >= 0.0f ? SceneColorPin : Profile.FogScatteringSceneColorScale);
			if (FogComponent->bEnableFSSS != bScatter
				|| !FMath::IsNearlyEqual(FogComponent->FSSSSpreadScale, Spread)
				|| !FMath::IsNearlyEqual(FogComponent->FSSSSceneColorScatteringAmountScale, SceneColor))
			{
				FogComponent->bEnableFSSS = bScatter;
				FogComponent->FSSSSpreadScale = Spread;
				FogComponent->FSSSSceneColorScatteringAmountScale = SceneColor;
				FogComponent->MarkRenderStateDirty();
			}
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

void AAnastasisWorldAtmosphere::ApplyEyePlane(const UAnastasisAtmosphereProfile& Profile, const bool bRealismOn)
{
	const bool bOn = bRealismOn && CVarEyePlane.GetValueOnGameThread() != 0;
	USkyAtmosphereComponent* SkyComponent = SkyAtmosphere ? SkyAtmosphere->GetComponent() : nullptr;
	UExponentialHeightFogComponent* FogComponent = Fog ? Fog->GetComponent() : nullptr;
	if (!bRealismOn)
	{
		return;
	}

	const UExponentialHeightFogComponent* FogDefault = GetDefault<UExponentialHeightFogComponent>();
	float Aerial = bOn
		? FMath::Min(EyePlaneAerialCap, Profile.SkyAerialPerspectiveDistanceScale * EyePlaneAerialGain)
		: Profile.SkyAerialPerspectiveDistanceScale;
	if (CVarAerialCap.GetValueOnGameThread() > 0.0f)
	{
		Aerial = FMath::Min(Aerial, CVarAerialCap.GetValueOnGameThread());
	}
	const float Start = bOn ? EyePlaneStartUU : Profile.FogStartDistance;
	const float MaxOpacity = bOn ? EyePlaneMaxOpacity : Profile.FogMaxOpacity;
	const float Cutoff = bOn ? EyePlaneCutoffUU : FogDefault->FogCutoffDistance;
	const float Extinction = bOn
		? Profile.VolumetricFogExtinctionScale * EyePlaneExtinctionGain
		: Profile.VolumetricFogExtinctionScale;
	// Volumetric fog ignores the exponential start. Its own start is what keeps a reed sharp.
	const float VolumetricStart = bOn ? EyePlaneStartUU : FogDefault->VolumetricFogStartDistance;
	const float NearFade = bOn ? EyePlaneNearFadeUU : FogDefault->VolumetricFogNearFadeInDistance;
	const float VolumetricDistance = bOn ? EyePlaneVolumetricDistanceUU : Profile.VolumetricFogDistanceUU;

	if (SkyComponent)
	{
		SkyComponent->SetAerialPespectiveViewDistanceScale(Aerial);
	}
	if (FogComponent)
	{
		FogComponent->SetStartDistance(Start);
		FogComponent->SetFogMaxOpacity(MaxOpacity);
		FogComponent->SetFogCutoffDistance(Cutoff);
		FogComponent->SetVolumetricFogExtinctionScale(Extinction);
		FogComponent->SetVolumetricFogStartDistance(VolumetricStart);
		FogComponent->SetVolumetricFogNearFadeInDistance(NearFade);
		FogComponent->SetVolumetricFogDistance(VolumetricDistance);
	}
	UE_LOG(LogAnastasis_UnrealV2, Display,
		TEXT("ANASTASIS_EYE_PLANE on=%d start_cm=%.0f vol_start_cm=%.0f vol_dist_cm=%.0f max_opacity=%.2f aerial=%.2f extinction=%.2f"),
		bOn ? 1 : 0, Start, VolumetricStart, VolumetricDistance, MaxOpacity, Aerial, Extinction);
}

void AAnastasisWorldAtmosphere::BeginPlay()
{
	Super::BeginPlay();
	Apply();
}

bool AAnastasisWorldAtmosphere::Apply()
{
	RestorePassage();
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
		ApplySkyPins(LastSky);
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
			ExposureVolume->Settings.bOverride_LocalExposureHighlightContrastScale = false;
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
	const bool bRealismOn = Profile.bRealismEnabled && IsRealismEnabledByCVar();
	ApplyRealism(Profile, bRealismOn, SunRotation, MoonRotation);
	ApplyEyePlane(Profile, bRealismOn);

	// --- Directional lights: exactly one leads (ATMOSPHERE_COHERENCE_001) -------------
	// After the realism layer, which creates or removes the moon. The clock re-decides it every
	// tick; the fixed rig decides it once, from the same predicate.
	{
		const double SunElevation = AnastasisSkyClock::ElevationOf(SunRotation);
		ArbitrateDirectionalLights(AnastasisSkyClock::MoonLeadsForwardShading(SunElevation),
			AnastasisSkyClock::SunFogScatteringFor(Profile, SunElevation));
	}
	ExtraDirectionalLights = 0;
	FString ExtraNames;
	for (TActorIterator<ADirectionalLight> It(World); It; ++It)
	{
		if (IsValid(*It) && *It != Sun && *It != Moon)
		{
			++ExtraDirectionalLights;
			ExtraNames += (ExtraNames.IsEmpty() ? TEXT("") : TEXT(",")) + It->GetName();
		}
	}
	if (ExtraDirectionalLights > 0)
	{
		// Not adopted, not silenced: a third directional light is a level-authoring problem, and
		// any two at the same ForwardShadingPriority bring the renderer's warning back.
		UE_LOG(LogAnastasis_UnrealV2, Warning,
			TEXT("ANASTASIS_ATMOSPHERE extra_directional_lights=%d names=%s (only the sun and the moon are arbitrated)"),
			ExtraDirectionalLights, *ExtraNames);
	}

	// --- Sky clock (DAY_NIGHT_WEATHER_001) ----------------------------------------------
	// Last, because it overrides what the fixed profile just wrote: exposure by sun
	// elevation, fog by humidity, clouds by cover. With the clock off, nothing below runs and
	// the image is the fixed rig's, exactly as before.
	if (bSkyClockActive)
	{
		// Apply() just wrote the profile's day EV into the volume: the cache of what the
		// clock last wrote is stale, and must not suppress the rewrite.
		LastExposureWritten = TNumericLimits<float>::Lowest();
		UpdateSky(Profile, /*bForceLog*/ true);
	}
	else if (CloudMaterialInstance)
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
		TEXT("fog=%d fog_density=%.4f ev100=%.2f adopted=%d spawned=%d realism=%d moon=%d clouds=%d volumetric_fog=%d ")
		TEXT("fog_scattering=%d forward_light=%s extra_directional_lights=%d eye_plane=%d"),
		Source,
		Profile.bDeriveSunFromTimeOfDay ? TEXT("time_of_day") : TEXT("explicit"),
		SunRotation.Pitch, SunRotation.Yaw, Profile.SunIntensityLux,
		Profile.bFogEnabled ? 1 : 0, Profile.FogDensity, Profile.ExposureEV100,
		AdoptedCount, SpawnedCount,
		bRealismApplied ? 1 : 0, Moon ? 1 : 0, Cloud ? 1 : 0,
		(Fog && Fog->GetComponent() && Fog->GetComponent()->bEnableVolumetricFog) ? 1 : 0,
		(Fog && Fog->GetComponent() && Fog->GetComponent()->bEnableFSSS) ? 1 : 0,
		bMoonLeadsForward ? TEXT("moon") : TEXT("sun"), ExtraDirectionalLights,
		(bRealismOn && CVarEyePlane.GetValueOnGameThread() != 0) ? 1 : 0);
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
