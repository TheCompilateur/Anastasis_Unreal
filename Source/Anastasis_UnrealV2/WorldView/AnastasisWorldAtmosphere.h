#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WorldView/AnastasisSkyClock.h"
#include "AnastasisWorldAtmosphere.generated.h"

class ADirectionalLight;
class AExponentialHeightFog;
class APostProcessVolume;
class ASkyAtmosphere;
class ASkyLight;
class AVolumetricCloud;
class UInstancedStaticMeshComponent;
class UStaticMeshComponent;
class UStaticMesh;
class UMaterialInterface;
class UAnastasisAtmosphereProfile;
class ULightComponent;

/**
 * ATMOSPHERE OWNER.
 *
 * The world's air, in code: sun, sky, sky light, height fog and exposure policy, read from
 * UAnastasisAtmosphereProfile and applied to whatever level embodies the world.
 *
 * Before this actor, lighting existed in exactly one place -- a rig hand-spawned by
 * tools/unreal/observe-slice.py into /Game/Anastasis/Maps/Lvl_AnastasisSlice. The PIE target
 * (/Game/FirstPerson/Lvl_FirstPerson) ran on the FirstPerson template's lighting, which no
 * one authored and nothing reproduces. Two levels, two unrelated looks, neither owned.
 *
 * ADOPT BEFORE SPAWN. Apply() looks for an existing DirectionalLight / SkyAtmosphere /
 * SkyLight / ExponentialHeightFog / unbound PostProcessVolume in the level and configures
 * THAT, spawning only what is missing. Two consequences, both deliberate:
 *   - the observation rig keeps working: its actors are adopted and, because the profile's
 *     code defaults are that rig's own values, written back identical (rig parity is a test,
 *     not a hope), so existing A/B captures stay comparable;
 *   - applying twice is idempotent -- the second pass adopts what the first one spawned
 *     instead of stacking a second sun. Anastasis.Atmosphere.Idempotence locks this.
 *
 * Nothing here touches AnastasisSim, worldgen, or the sealed terrain contract. Light is not
 * simulation truth; it is how already-decided truth is seen.
 */
UCLASS()
class AAnastasisWorldAtmosphere : public AActor
{
	GENERATED_BODY()

public:
	AAnastasisWorldAtmosphere();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Actual applied exposure, for temporal proofs (the sky state contains the target). */
	UFUNCTION(BlueprintPure, Category = "Anastasis|Atmosphere")
	double GetAppliedExposureEV() const { return AppliedExposureEV; }

	UFUNCTION(BlueprintPure, Category = "Anastasis|Atmosphere")
	double GetTargetExposureEV() const { return LastSky.ExposureEV100; }

	/** Follows the simulation clock while the sky clock is active (DAY_NIGHT_WEATHER_001). */
	virtual void Tick(float DeltaSeconds) override;

	/**
	 * anastasis.Sky.Clock: 1 (default) the sky follows the simulation's hour, season and
	 * weather; 0 the profile's fixed sun angles (the observation rig).
	 */
	static bool IsSkyClockEnabledByCVar();

	/**
	 * The simulation instant the sky shows, and the seed it reads the weather with: the live
	 * simulation's in a game world, AnastasisSkyClock::InitialSimTime and the canonical seed
	 * where no simulation runs (the editor). anastasis.Sky.Day / anastasis.Sky.Hour pin a day
	 * or an hour for captures -- they move the SKY, never the simulation.
	 */
	double ResolveSkySimTime(uint32& OutSeed) const;

	/** The sky as last written by Apply() or Tick(). Meaningful only while the clock is active. */
	const AnastasisSkyClock::FSkyState& GetLastSkyState() const { return LastSky; }
	bool IsSkyClockActive() const { return bSkyClockActive; }

	/** Diagnostic: first visible meteor instant on an evening, with clear weather and the current seed. -1 if absent. */
	UFUNCTION(BlueprintCallable, Category="Anastasis|Atmosphere|Diagnostics")
	double GetFirstCosmicMeteorHour(int32 EveningDay) const;

	/**
	 * anastasis.Atmosphere: 1 (default) applies the profile, 0 leaves the level's own
	 * lighting untouched. The escape hatch for anyone who needs to compare against the
	 * pre-ATMOSPHERE_001 image without editing data.
	 */
	static bool IsEnabledByCVar();

	/**
	 * anastasis.Atmosphere.Realism: 1 (default) applies the realism layer of the profile
	 * (ENV_REALISM_001), 0 restores the engine defaults of every property that layer owns and
	 * removes its moon and clouds. Read on every Apply(), so the A/B needs no restart.
	 */
	static bool IsRealismEnabledByCVar();

	/**
	 * Tag carried by the moon. The sun is adopted as "the first DirectionalLight in the level",
	 * so without it the second Apply() would adopt the moon as the sun and light the world at
	 * 0.3 lux. Every sun lookup skips it.
	 */
	static const FName MoonTag;

	/** Tag carried by a cloud layer this actor created: only those are removed with the realism layer off. */
	static const FName RealismCloudTag;

	/**
	 * Resolve the profile and write it into the level. Returns false only when the profile
	 * says bEnabled=false (the level's own lighting is then left strictly alone).
	 */
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "Anastasis|Atmosphere")
	bool Apply();

	/**
	 * Places the wetness-driven mist pockets (ATMOSPHERE_002).
	 *
	 * SEPARATE FROM Apply() ON PURPOSE: the global atmosphere is a property of the level and
	 * can be set before anything exists, but mist is read from the world's own tiles, so it
	 * needs AAnastasisWorldEmbodiment to have embodied first. Rather than hide that ordering
	 * inside BeginPlay, the game mode calls this after spawning the embodiment, and this
	 * function finds the embodiment itself so an editor button or a script works too.
	 *
	 * Returns the number of pockets placed. 0 with no embodiment is a logged refusal, not a
	 * crash: a world with no tiles has no wet valleys to speak of.
	 */
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "Anastasis|Atmosphere")
	int32 ApplyMist();

	/** Pockets placed by the last ApplyMist(). */
	int32 GetMistVolumeCount() const { return MistVolumes.Num(); }

	/** One line, the same shape the other WorldView diagnostics use. Written by Apply(). */
	const FString& GetLastSummary() const { return LastSummary; }

	/** Actors touched by the last Apply(), for the probe and for tests. Null before Apply(). */
	ADirectionalLight* GetSun() const { return Sun; }
	ASkyAtmosphere* GetSkyAtmosphere() const { return SkyAtmosphere; }
	ASkyLight* GetSkyLight() const { return SkyLight; }
	AExponentialHeightFog* GetFog() const { return Fog; }
	APostProcessVolume* GetExposureVolume() const { return ExposureVolume; }
	ADirectionalLight* GetMoon() const { return Moon; }
	AVolumetricCloud* GetCloud() const { return Cloud; }

	/** RAIN_001. The streaks, once rain has been shown (null before); hidden when it stops. */
	UInstancedStaticMeshComponent* GetRainStreaks() const { return RainStreaks; }

	/** RAIN_001. The share of streaks lit at the last UpdateSky (0 = none drawn). */
	double GetRainAmount() const { return LastRainAmount; }

	/** Whether the last Apply() ran the realism layer (profile switch AND CVar). */
	bool WasRealismApplied() const { return bRealismApplied; }

	/**
	 * EYE_PLANE_001. The first 7 m stay outside the volumetric volume (a trunk, a reed).
	 * The volume then reaches about 120 m, so a shore near 40 m sits in air that has a cost
	 * and a ridge keeps its shape. AIR_RELIEF_002 reduces the previous 3.5 amplification
	 * to 1.5 to lighten the shared volumetric veil while retaining local wetness mist.
	 * Direct art adjustment requested by Alexandre; this value has not been visually observed.
	 * The exponential fog's own start, max opacity and cutoff do not reach the volume.
	 */
	static constexpr float EyePlaneStartUU = 700.0f;
	static constexpr float EyePlaneNearFadeUU = 1800.0f;
	static constexpr float EyePlaneVolumetricDistanceUU = 12000.0f;
	static constexpr float EyePlaneMaxOpacity = 0.48f;
	static constexpr float EyePlaneCutoffUU = 0.0f;
	static constexpr float EyePlaneAerialGain = 1.15f;
	static constexpr float EyePlaneAerialCap = 12.0f;
	static constexpr float EyePlaneExtinctionGain = 1.5f;

	/** True when the last Apply()/Tick handed forward shading to the moon (ATMOSPHERE_COHERENCE_001). */
	bool IsMoonLeadingForward() const { return bMoonLeadsForward; }

	/** Directional lights in the level that are neither the adopted sun nor the moon, at the last Apply(). */
	int32 GetExtraDirectionalLightCount() const { return ExtraDirectionalLights; }

	/** How many of the five rig actors Apply() had to create because the level had none. */
	int32 GetSpawnedCount() const { return SpawnedCount; }

	/** How many it found already in the level and reconfigured. */
	int32 GetAdoptedCount() const { return AdoptedCount; }

	/**
	 * Destroys only the actors this instance created, never an adopted one. Spawned actors
	 * are RF_Transient so they can never be saved into a level, but a long editor session
	 * (or an automation pass) still has to be able to put the world back as it found it.
	 */
	void DestroySpawnedActors();

protected:
	UPROPERTY()
	TObjectPtr<ADirectionalLight> Sun;

	UPROPERTY()
	TObjectPtr<ASkyAtmosphere> SkyAtmosphere;

	UPROPERTY()
	TObjectPtr<ASkyLight> SkyLight;

	UPROPERTY()
	TObjectPtr<AExponentialHeightFog> Fog;

	UPROPERTY()
	TObjectPtr<APostProcessVolume> ExposureVolume;

	UPROPERTY()
	TObjectPtr<ADirectionalLight> Moon;

	UPROPERTY()
	TObjectPtr<AVolumetricCloud> Cloud;

	/**
	 * The realism layer, on or off. Off is not "do nothing": it writes the engine component
	 * defaults back into every property the layer owns and removes the moon and the cloud layer
	 * it created, so that switching it off in a live session returns the exact pre-realism image.
	 */
	void ApplyRealism(const UAnastasisAtmosphereProfile& Profile, bool bOn, const FRotator& SunRotation, const FRotator& MoonRotation);

	/**
	 * EYE_PLANE_001. The realism aerial scale is tuned for a ridge a kilometre away, so at 40 m
	 * an olive, a pebble and a wall still arrive at the same contrast. With anastasis.Depth.EyePlane
	 * on, the first 7 m stay clear (a trunk or a reed in the near frame) and colour contrast falls
	 * toward the air after that. FogMaxOpacity and the cutoff keep a ridge a shape, not a white-out.
	 * Volumetric fog ignores start, max opacity and cutoff: the extinction scale is what reaches it.
	 */
	void ApplyEyePlane(const UAnastasisAtmosphereProfile& Profile, bool bRealismOn);

	bool bRealismApplied = false;

	/**
	 * ATMOSPHERE_COHERENCE_001. The renderer takes ONE directional light for forward shading,
	 * translucency, single layer water and volumetric fog (highest ForwardShadingPriority). Sun
	 * and moon both at the default 0 made it warn and fall back to brightness --
	 * the 75000 lux sun, even set, even shadowless. This gives the lead to the moon exactly when
	 * the sun is below the horizon (the shadows' own predicate), and scales the sun's
	 * VolumetricScatteringIntensity by SunFogScattering. Writes only what changed.
	 */
	void ArbitrateDirectionalLights(bool bMoonLeads, double SunFogScattering);

	bool bMoonLeadsForward = false;
	int32 ExtraDirectionalLights = 0;

	/**
	 * Writes one sky instant: sun and moon orientation and shadows, pinned exposure, cloud
	 * coverage and fog density from the weather. Called by Apply() and, while the clock is
	 * active, every Tick. Logs a line when the village phase changes (or when bForceLog).
	 * AdaptSeconds > 0 applies the exposure policy (Passage: slow dark adaptation, immediate
	 * bright-light protection; legacy: symmetric rate); 0 snaps to the hour (Apply, captures).
	 */
	void UpdateSky(const UAnastasisAtmosphereProfile& Profile, bool bForceLog, float AdaptSeconds = 0.0f);

	/**
	 * RAIN_001. Shows the simulation's rain (or anastasis.Sky.Rain when pinned) as streaks around
	 * the camera: creates the instanced component the first time rain is visible, hides it when
	 * the rain stops, and writes amount and wind on its dynamic material. Called by UpdateSky.
	 * Missing assets (tools/unreal/rain-material.ps1 not run) are logged once and leave no rain.
	 */
	void UpdateRain(const AnastasisSkyClock::FSkyState& Sky, bool bWeatherDrivesSky);

	/** Map-independent night art. The dome is transient and reads only the sky clock. */
	void UpdateCosmicSky(bool bForceLog);

	UPROPERTY(VisibleAnywhere, Category="Atmosphere|Cosmic Sky")
	TObjectPtr<UStaticMeshComponent> CosmicDome;

	UPROPERTY(EditDefaultsOnly, Category="Atmosphere|Cosmic Sky")
	TSoftObjectPtr<UStaticMesh> CosmicDomeMesh = TSoftObjectPtr<UStaticMesh>(
		FSoftObjectPath(TEXT("/Engine/EngineSky/SM_SkySphere.SM_SkySphere")));

	UPROPERTY(EditDefaultsOnly, Category="Atmosphere|Cosmic Sky")
	TSoftObjectPtr<UMaterialInterface> CosmicSkyBaseMaterial = TSoftObjectPtr<UMaterialInterface>(
		FSoftObjectPath(TEXT("/Game/Anastasis/Celestial/M_AnastasisCosmicSky.M_AnastasisCosmicSky")));

	UPROPERTY()
	TObjectPtr<class UMaterialInstanceDynamic> CosmicSkyInstance;

	uint32 LastSkySeed = 12345u;
	int32 LastCosmicEvening = -1;
	bool bCosmicAssetsMissingLogged = false;

	UPROPERTY()
	TObjectPtr<UInstancedStaticMeshComponent> RainStreaks;

	UPROPERTY()
	TObjectPtr<class UMaterialInstanceDynamic> RainMaterialInstance;

	double LastRainAmount = 0.0;
	bool bRainAssetsMissingLogged = false;

	/** The exposure actually on screen, which lags the hour's target while the eye adapts. */
	double AppliedExposureEV = 0.0;
	bool bHasAppliedExposure = false;

	/** Remember adopted-light settings so disabling the passage is reversible. */
	struct FPassageLight
	{
		TWeakObjectPtr<ULightComponent> Component;
		float Diffuse = 1.0f;
		float Specular = 1.0f;
		float Indirect = 1.0f;
		float Fog = 1.0f;
		bool bScaledFog = false;
		void Restore();
		void Write(ULightComponent* Light, float Weight, bool bScaleFog);
	};
	FPassageLight PassageSun;
	FPassageLight PassageMoon;
	void RestorePassage();

    /** Shared material weather contract. Serialized soft reference retains it in cooked builds. */
    UPROPERTY(EditDefaultsOnly, Category="Atmosphere")
    TSoftObjectPtr<class UMaterialParameterCollection> WeatherParameters = TSoftObjectPtr<UMaterialParameterCollection>(FSoftObjectPath(TEXT("/Game/Anastasis/Materials/MPC_AnastasisWeather.MPC_AnastasisWeather")));

    UPROPERTY()
    TObjectPtr<class UMaterialParameterCollection> WeatherCollection;
    bool bWeatherCollectionLoadAttempted = false;

	/** Coverage lives on a material parameter: the engine instance is shared, so this actor drives its own dynamic instance. */
	UPROPERTY()
	TObjectPtr<class UMaterialInstanceDynamic> CloudMaterialInstance;

	/** Each pocket's ATMOSPHERE_002 extinction, parallel to MistVolumes: the clock scales from it, never compounds. */
	TArray<float> MistBaseExtinction;
	float LastMistFactor = -1.0f;

	AnastasisSkyClock::FSkyState LastSky;
	bool bSkyClockActive = false;
	FString LastLoggedPhase;
	float LastExposureWritten = TNumericLimits<float>::Lowest();

	/**
	 * Adopt what the level already has, create only what is missing, and record which is
	 * which. The difference is what makes Apply() idempotent and reversible.
	 */
	template <typename ActorType>
	ActorType* AdoptOrSpawn(const FVector& Location, const FRotator& Rotation, const struct FActorSpawnParameters& Params);

	/** Exactly what this instance created, in creation order. Adopted actors are never in here. */
	UPROPERTY()
	TArray<TObjectPtr<AActor>> SpawnedActors;

	/**
	 * The mist pockets, kept apart from SpawnedActors because they are rebuilt wholesale:
	 * re-applying mist destroys the previous field rather than adopting it. Adoption is right
	 * for the five singleton rig actors, wrong here — a second pass would otherwise stack a
	 * new fog bank on every old one and double the extinction with nothing in the world
	 * having changed.
	 */
	UPROPERTY()
	TArray<TObjectPtr<class ALocalFogVolume>> MistVolumes;

	int32 SpawnedCount = 0;
	int32 AdoptedCount = 0;
	FString LastSummary;
};
