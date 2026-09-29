#pragma once

#include "CoreMinimal.h"
#include "Sim/AnastasisSimulation.h"
#include "Subsystems/WorldSubsystem.h"
#include "Village/AnastasisVillagePresentation.h"
#include "AnastasisSimulationSubsystem.generated.h"

/**
 * Pompe Unreal du tick de simulation. Possede FAnastasisSimulation.
 * Tick seulement dans les mondes game/PIE — pas l'editeur.
 */
UCLASS()
class UAnastasisSimulationSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override;
	virtual bool IsTickableInEditor() const override { return false; }

	void ResetCanonical(uint32 Seed);
	void LogStatus() const;

	/**
	 * Premier batiment (mission first-building-001) : pose un puits sur la case
	 * libre la plus proche de (TileX, TileY) et `NpcCount` habitants autour, a
	 * soifs echelonnees. Rend l'identifiant du puits, vide si rien n'a pu etre pose.
	 * Tout passe par la simulation ; la presentation suit au prochain Sync.
	 */
	FString SeedFirstWell(int32 NpcCount, int32 TileX, int32 TileY);

	/** Reflete les batiments de la simulation en acteurs. Appele a chaque Tick. */
	int32 SyncVillagePresentation();
	const FAnastasisVillagePresentation& GetVillagePresentation() const { return VillagePresentation; }

	FAnastasisSimulation& GetSimulation() { return Simulation; }
	const FAnastasisSimulation& GetSimulation() const { return Simulation; }

private:
	void DrawOverlay() const;
	void LogDayIfChanged();

	FAnastasisSimulation Simulation;
	FAnastasisVillagePresentation VillagePresentation;
	int32 LoggedDay = 0;
	/** True only after OnWorldBeginPlay. Tests ResetCanonical without the engine ticker. */
	bool bPumpFromEngineTick = false;
};
