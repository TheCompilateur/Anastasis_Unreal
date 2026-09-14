#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AnastasisWorldAtmosphere.generated.h"

class ADirectionalLight;
class AExponentialHeightFog;
class APostProcessVolume;
class ASkyAtmosphere;
class ASkyLight;

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

	/**
	 * anastasis.Atmosphere: 1 (default) applies the profile, 0 leaves the level's own
	 * lighting untouched. The escape hatch for anyone who needs to compare against the
	 * pre-ATMOSPHERE_001 image without editing data.
	 */
	static bool IsEnabledByCVar();

	/**
	 * Resolve the profile and write it into the level. Returns false only when the profile
	 * says bEnabled=false (the level's own lighting is then left strictly alone).
	 */
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "Anastasis|Atmosphere")
	bool Apply();

	/** One line, the same shape the other WorldView diagnostics use. Written by Apply(). */
	const FString& GetLastSummary() const { return LastSummary; }

	/** Actors touched by the last Apply(), for the probe and for tests. Null before Apply(). */
	ADirectionalLight* GetSun() const { return Sun; }
	ASkyAtmosphere* GetSkyAtmosphere() const { return SkyAtmosphere; }
	ASkyLight* GetSkyLight() const { return SkyLight; }
	AExponentialHeightFog* GetFog() const { return Fog; }
	APostProcessVolume* GetExposureVolume() const { return ExposureVolume; }

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

	/**
	 * Adopt what the level already has, create only what is missing, and record which is
	 * which. The difference is what makes Apply() idempotent and reversible.
	 */
	template <typename ActorType>
	ActorType* AdoptOrSpawn(const FVector& Location, const FRotator& Rotation, const struct FActorSpawnParameters& Params);

	/** Exactly what this instance created, in creation order. Adopted actors are never in here. */
	UPROPERTY()
	TArray<TObjectPtr<AActor>> SpawnedActors;

	int32 SpawnedCount = 0;
	int32 AdoptedCount = 0;
	FString LastSummary;
};
