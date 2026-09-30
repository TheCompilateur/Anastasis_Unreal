#pragma once

#include "CoreMinimal.h"
#include "Village/AnastasisVillage.h"
#include "World/AnastasisWorld.h"

/**
 * Hote de simulation — port du tick temporel de src/sim/simulation.js.
 *
 * Porte dans cette tranche, et rien d'autre :
 *   time / day / dayFrac / tick / onNewDay({ defer }) / file jour vide
 *   generateWorld une fois au Reset, puis le monde est fige
 *   `for (const npc of this.actors) updateNpc(this, npc, dt)` — la boucle du
 *   puits (Village/AnastasisVillage.h, mission first-building-001)
 *
 * PAS porte (reste NOT_IMPLEMENTED, volontaire) :
 *   economie, onNewDay critique (spoilage, life, clio), LOD logique,
 *   file de navigation, foule, animaux, transport
 *
 * Autorite JS:
 *   DAY_LENGTH = 90
 *   time0 = DAY_LENGTH * 0.42   // ~10h, premiere image en jour
 *   day0  = 1
 *   tick: time += dt; day = 1 + floor(time / DAY_LENGTH); si ca change, onNewDay({defer:true})
 *   un seul onNewDay meme si dt saute plusieurs minuits
 */
class ANASTASISSIM_API FAnastasisSimulation
{
public:
	/** Secondes de simulation par jour. Identique a export const DAY_LENGTH. */
	static constexpr double DayLength = 90.0;

	/** Soft cap jobs / tick. Identique a DAY_DEFERRED_JOBS_PER_TICK. Seul `landRegen` est porte. */
	static constexpr int32 DayDeferredJobsPerTick = 2;

	FAnastasisSimulation();

	/**
	 * Equivalent de resetWorldBase(seed) sans site / PNJ / cache save.
	 * Genere le monde une fois. Reinitialise l'horloge. Autorise Tick.
	 */
	void Reset(uint32 Seed, int32 Width, int32 Height);

	/** No-op tant que Reset n'a pas ete appele (port de `if (this.bootDeferred) return`). */
	void Tick(double Dt);

	/**
	 * Un frame d'hote : FrameDelta + StepPlan + accumulateur `dt * scale`.
	 * Port du pump de src/main.js, sans coupe au chronometre (aucun job rng ici).
	 * Retourne le nombre de Tick consommes.
	 */
	int32 PumpFrame(double WallSeconds, double Speed = 1.0);

	/** (time % DAY_LENGTH) / DAY_LENGTH. */
	double DayFrac() const;

	bool IsRunning() const { return !bBootDeferred; }
	uint32 GetSeed() const { return Seed; }
	int32 GetDay() const { return Day; }
	double GetTime() const { return Time; }
	int32 GetNewDayCount() const { return NewDayCount; }
	int32 GetDeferredRemaining() const { return DeferredRemaining; }
	/** Tuiles de champ regarnies par le dernier `landRegen` (regrowFieldsDaily). */
	int32 GetLastRegrownFields() const { return LastRegrownFields; }
	double GetAccumulator() const { return Accumulator; }

	const AnastasisWorld::FWorld& GetWorld() const { return World; }

	/** Batiments et habitants. Lie au monde par Reset ; vide avant. */
	AnastasisVillage::FVillage& GetVillage() { return Village; }
	const AnastasisVillage::FVillage& GetVillage() const { return Village; }

	/** Empreinte du terrain fige : type + bits d'altitude, dans l'ordre des tuiles. */
	uint64 TileFingerprint() const;

private:
	void OnNewDay(bool bDefer);
	void ProcessDayDeferred(int32 MaxJobs);
	/** Tete de la file de minuit : `regrowFieldsDaily` (regrowForestDaily est sans effet dans la reference). */
	void RunLandRegen();

	bool bBootDeferred = true;
	uint32 Seed = 0;
	double Time = 0.0;
	int32 Day = 0;
	int32 NewDayCount = 0;
	int32 DeferredRemaining = 0;
	bool bLandRegenPending = false;
	int32 LastRegrownFields = 0;
	double Accumulator = 0.0;
	AnastasisWorld::FWorld World;
	/** Pointe sur `World` : l'hote n'est ni copie ni deplace (voir les declarations supprimees). */
	AnastasisVillage::FVillage Village;

public:
	FAnastasisSimulation(const FAnastasisSimulation&) = delete;
	FAnastasisSimulation& operator=(const FAnastasisSimulation&) = delete;
};
