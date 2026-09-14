#pragma once

#include "CoreMinimal.h"
#include "World/AnastasisWorld.h"

/**
 * Hote de simulation — port du tick temporel de src/sim/simulation.js.
 *
 * Porte dans cette tranche, et rien d'autre :
 *   time / day / dayFrac / tick / onNewDay({ defer }) / file jour vide
 *   generateWorld une fois au Reset, puis le monde est fige
 *
 * PAS porte (reste NOT_IMPLEMENTED, volontaire) :
 *   acteurs, besoins, economie, nav, onNewDay critique (spoilage, life, clio)
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

	/** Soft cap jobs / tick. Identique a DAY_DEFERRED_JOBS_PER_TICK. File encore vide. */
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
	double GetAccumulator() const { return Accumulator; }

	const AnastasisWorld::FWorld& GetWorld() const { return World; }

	/** Empreinte du terrain fige : type + bits d'altitude, dans l'ordre des tuiles. */
	uint64 TileFingerprint() const;

private:
	void OnNewDay(bool bDefer);
	void ProcessDayDeferred(int32 MaxJobs);

	bool bBootDeferred = true;
	uint32 Seed = 0;
	double Time = 0.0;
	int32 Day = 0;
	int32 NewDayCount = 0;
	int32 DeferredRemaining = 0;
	double Accumulator = 0.0;
	AnastasisWorld::FWorld World;
};
