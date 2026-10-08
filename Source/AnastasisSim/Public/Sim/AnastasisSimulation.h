#pragma once

#include "CoreMinimal.h"
#include "Geo/AnastasisGeo.h"
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

	/** Soft cap jobs / tick. Identique a DAY_DEFERRED_JOBS_PER_TICK. */
	static constexpr int32 DayDeferredJobsPerTick = 2;

	/**
	 * La file de minuit de la reference (`enqueueDayDeferred`), ses 17 travaux dans l'ordre.
	 * Portes : `landRegen` (0), `roadEvolution` (8, sentiers de desir, ecart n°42), `lifeDaily` (10, mortalite reduite) et `memory` (14, oubli quotidien). Les autres occupent leur
	 * place et leur part du budget sans rien faire : NOT_IMPLEMENTED.
	 */
	static constexpr int32 DayDeferredJobCount = 17;
	static constexpr int32 DayJobLandRegen = 0;
	/** `lifeDaily` (updateLifeDaily) : porte reduit a la mort certaine (ecart n°28). */
	static constexpr int32 DayJobLifeDaily = 10;
	/** `roadEvolution` (updateRoadEvolutionDaily) : sentiers de desir seuls, si l'hote les active (ecart n°42). */
	static constexpr int32 DayJobRoadEvolution = 8;
	static constexpr int32 DayJobMemory = 14;

	/** Sentiers poses par le dernier travail `roadEvolution` (ecart n°42). */
	int32 GetLastRoadsBuilt() const { return LastRoadsBuilt; }

	FAnastasisSimulation();

	/**
	 * Equivalent de resetWorldBase(seed) sans site / PNJ / cache save.
	 * Genere le monde une fois. Reinitialise l'horloge. Autorise Tick.
	 */
	void Reset(uint32 Seed, int32 Width, int32 Height);

	/**
	 * Harnais : le chemin de `deserialize`. Le monde est FOURNI (genere puis
	 * `tileDiff` applique, Harness/AnastasisJsSave.h), l'horloge est celle de la
	 * sauvegarde ; la file de minuit part vide (`_dayDeferred` n'est pas
	 * sauvegarde). Le village est lie a ce monde, vide : a peupler par
	 * `FVillage::RestoreForHarness`.
	 */
	void ResetFromWorld(uint32 Seed, AnastasisWorld::FWorld&& InWorld, double InTime, int32 InDay);

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

	/**
	 * EXTENSION — ecart n°38 (geopolitical-world-001). Le monde exterieur : vide tant qu'un hote n'a
	 * pas charge de scenario (`GetGeo().Load`). Il avance a chaque minuit, sur le jour de cette
	 * simulation ; Reset et ResetFromWorld le dechargent. Aucun scenario du harnais ne le charge.
	 */
	AnastasisGeo::FGeoWorld& GetGeo() { return Geo; }
	const AnastasisGeo::FGeoWorld& GetGeo() const { return Geo; }

	/**
	 * L'adaptateur local du monde exterieur : chaque groupe d'arrivants en attente devient des
	 * habitants (`FVillage::AdmitExternalArrivals`), et le monde exterieur en garde la trace. Appele
	 * a minuit ; un hote l'appelle aussi apres une injection. Rend le nombre d'habitants crees.
	 */
	int32 AdmitGeoMigration();

	/** Rayon (cases) de l'anneau ou se posent les arrivants du dehors. */
	static constexpr double GeoArrivalRadius = 7.0;
	/** Pas d'angle entre deux groupes successifs (rad) : deux groupes n'arrivent pas au meme endroit. */
	static constexpr double GeoArrivalAngleStep = 2.399963229728653;

	/**
	 * STATE_ORACLE_001 -- empreinte d'ETAT de la simulation : horloge, file de minuit, monde entier et
	 * FVillage::StateDigest(). L'oracle des tests de determinisme ; pas la parite JS.
	 */
	uint64 StateDigest() const;

private:
	void OnNewDay(bool bDefer);
	void ProcessDayDeferred(int32 MaxJobs);
	/** Tete de la file de minuit : `regrowFieldsDaily` (regrowForestDaily est sans effet dans la reference). */
	void RunLandRegen();
	/** Sentiers poses par le dernier travail `roadEvolution` (observation, hors digest). */
	int32 LastRoadsBuilt = 0;
	void RunDayJob(int32 Job);

	bool bBootDeferred = true;
	uint32 Seed = 0;
	double Time = 0.0;
	int32 Day = 0;
	int32 NewDayCount = 0;
	int32 DeferredRemaining = 0;
	/** Travaux en attente, par numero dans la file de la reference. */
	TArray<int32> DeferredJobs;
	int32 LastRegrownFields = 0;
	double Accumulator = 0.0;
	AnastasisWorld::FWorld World;
	/** Pointe sur `World` : l'hote n'est ni copie ni deplace (voir les declarations supprimees). */
	AnastasisVillage::FVillage Village;
	/** ecart n°38 : le monde exterieur, decharge par defaut. */
	AnastasisGeo::FGeoWorld Geo;

public:
	FAnastasisSimulation(const FAnastasisSimulation&) = delete;
	FAnastasisSimulation& operator=(const FAnastasisSimulation&) = delete;
};
