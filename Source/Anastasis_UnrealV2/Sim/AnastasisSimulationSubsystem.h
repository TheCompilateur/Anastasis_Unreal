#pragma once

#include "CoreMinimal.h"
#include "Sim/AnastasisSimulation.h"
#include "Kismet/BlueprintFunctionLibrary.h"
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

	/**
	 * La maison (mission house-rest-001) : deux maisons pres de (TileX, TileY),
	 * la premiere a un proprietaire, la seconde libre ; `NpcCount` habitants,
	 * dont un proprietaire et les autres sans toit, abrites par
	 * `assignSheltersDaily`. Rend l'identifiant de la maison possedee.
	 */
	FString SeedFirstHouse(int32 NpcCount, int32 TileX, int32 TileY);

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

/**
 * Lecteurs de debug pour les scripts de preuve Python (tools/unreal/*-pie.py).
 * Python ne sait pas atteindre un sous-systeme de monde : USubsystemBlueprintLibrary
 * est reservee aux noeuds Blueprint internes. Lecture seule, rien ne decide ici.
 */
UCLASS()
class UAnastasisSimulationDebugLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Temps de simulation (s) de l'hote de ce monde, -1 sans hote. */
	UFUNCTION(BlueprintPure, Category = "Anastasis|Debug", meta = (WorldContext = "WorldContextObject"))
	static double GetSimulationTime(const UObject* WorldContextObject);

	/** Phase commune du village ("night", "dawn"...), vide sans hote. */
	UFUNCTION(BlueprintPure, Category = "Anastasis|Debug", meta = (WorldContext = "WorldContextObject"))
	static FString GetVillagePhase(const UObject* WorldContextObject);

	/** Habitants dedans ce batiment (`npc.inside`), -1 sans hote. */
	UFUNCTION(BlueprintPure, Category = "Anastasis|Debug", meta = (WorldContext = "WorldContextObject"))
	static int32 CountInside(const UObject* WorldContextObject, const FString& BuildingId);
};
