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
class UAnastasisAtmosphereProfile;

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

	/** Whether the last Apply() ran the realism layer (profile switch AND CVar). */
	bool WasRealismApplied() const { return bRealismApplied; }

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

	bool bRealismApplied = false;

	/**
	 * Writes one sky instant: sun and moon orientation and shadows, pinned exposure, cloud
	 * coverage and fog density from the weather. Called by Apply() and, while the clock is
	 * active, every Tick. Logs a line when the village phase changes (or when bForceLog).
	 */
	void UpdateSky(const UAnastasisAtmosphereProfile& Profile, bool bForceLog);

	/** Coverage lives on a material parameter: the engine instance is shared, so this actor drives its own dynamic instance. */
	UPROPERTY()
	TObjectPtr<class UMaterialInstanceDynamic> CloudMaterialInstance;

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
