// Reconsideration d'un but en cours — mission reconsider-001.
//
// La reference, une fois par pensee (`src/sim/npc.js`, `updateNpc`) :
//
//   const reconsider = phaseReconsiderChance(sim, npc, thinkDt, needsReconsiderChance(npc, thinkDt));
//   const chance = committedReconsiderChance(sim, npc, reconsider);
//   if (!npc.target || sim.rng() < chance) chooseGoal(sim, npc);
//
// UN tirage du flux partage, seulement si l'habitant a une cible, et avant `chooseGoal`.
//
// Ce qui decide, mesure (`docs/migration/phase3/P3_RECONSIDERATION.md`, 75 tirages sur 75
// expliques par ces fonctions) :
//
// - `updateNpc` synchronise la phase a CHAQUE tick avant la pensee (`syncVillagePhase`,
//   l. 874). Le `syncVillagePhase` interne de `phaseReconsiderChance` rend donc toujours
//   false sur ce chemin : sa branche 0,92 n'est jamais prise. Le portage ne la porte pas
//   (`PhaseReconsiderChance` est la branche sans bascule) : branche morte, fonction reduite.
// - La phase est PERSONNELLE (`villagePhaseFor`) : le mode de vie decale l'horloge
//   (`personalFrac`). Un habitant sans mode de vie vit sur la phase du village.

#pragma once

#include "CoreMinimal.h"
#include "Life/AnastasisLifestyle.h"
#include "Life/AnastasisNeeds.h"
#include "Life/AnastasisVillageRhythm.h"

namespace AnastasisReconsider
{
	/** `GOAL_AI` : fenetres de la reconsideration. */
	inline constexpr double PhaseAdaptSeconds = 2.5;
	inline constexpr double CommitSeconds = 7.0;
	inline constexpr double CommitReconsiderScale = 0.42;

	/**
	 * `personalFrac(sim, npc)` : `dayFracOf(sim)`, decale de +0,04 (leve-tot) ou +0,96
	 * (noctambule), modulo 1. Sans mode de vie (`Lifestyle` vide) : la fraction du village.
	 */
	ANASTASISSIM_API double PersonalFrac(double DayFrac, const TOptional<AnastasisLifestyle::FLifestyle>& Lifestyle);

	/** `villagePhaseFor(sim, npc)` = `villagePhase(personalFrac(sim, npc))`. */
	ANASTASISSIM_API AnastasisRhythm::EPhase PersonalPhase(double DayFrac, const TOptional<AnastasisLifestyle::FLifestyle>& Lifestyle);

	/** `needsReconsiderChance(npc, dt)` (`life/needs.js`). */
	ANASTASISSIM_API double NeedsReconsiderChance(bool bCritical, const FString& Goal, double Dt);

	/**
	 * `phaseReconsiderChance(sim, npc, dt, base)` sur le chemin de `updateNpc` (sans bascule :
	 * voir l'en-tete) : midi, soir, nuit et aube relevent la chance a `dt * 0,55`.
	 */
	ANASTASISSIM_API double PhaseReconsiderChance(AnastasisRhythm::EPhase PersonalPhaseNow, double Dt, double BaseChance);

	/** Ce que `committedReconsiderChance` lit sur l'habitant. */
	struct FCommitSubject
	{
		bool bCritical = false;
		bool bHasTarget = false;
		FString Goal;
		/** `npc.phaseChangedAt` ; vide = `null`. */
		TOptional<double> PhaseChangedAt;
		/** `npc.goalSince || 0`. */
		double GoalSince = 0.0;
		/** `shiftShields(sim, npc)`. */
		bool bShiftShields = false;
	};

	/** `committedReconsiderChance(sim, npc, baseChance)`. */
	ANASTASISSIM_API double CommittedReconsiderChance(const FCommitSubject& Npc, double Now, double BaseChance);

	// --- Le collant de but (`goalStickinessBonus`, npc.js) --------------------------------------
	//
	// `commitGoalChoice` ajoute ce bonus a la ligne du but EN COURS, avant Noûs et le tri : sans lui,
	// une reconsideration tiree au milieu d'une cueillette fait basculer le fermier vers la livraison
	// a sac 4 (la reference le garde au champ : 18 + 12 de session).

	/** `GOAL_AI` : le collant. */
	inline constexpr double Stickiness = 18.0;
	inline constexpr double PhaseStickinessScale = 0.22;
	inline constexpr double LoadedBonus = 16.0;
	inline constexpr double CraftSessionBonus = 12.0;
	inline constexpr double TraitStickMin = 0.72;
	inline constexpr double TraitStickMax = 1.48;

	/** `CRAFT_GOALS` (craftWork.js) : les buts d'une session de travail. */
	ANASTASISSIM_API bool IsCraftGoal(const FString& Goal);

	/**
	 * `criticalReliefGoals(npc, sim).has(goal)` : le but soulage-t-il un besoin critique ?
	 * `helpFarm` (ferme comptee ou planifiee) n'est pas un but porte : la branche est omise.
	 */
	ANASTASISSIM_API bool IsCriticalReliefGoal(const AnastasisNeeds::FNeeds& Needs, int32 InventoryFood, const FString& Goal);

	/** `traitStickinessScale(npc, goal)` sur les affinites du trait (build, trade, gather, explore). */
	ANASTASISSIM_API double TraitStickinessScale(double Build, double Trade, double Gather, double Explore, const FString& Goal);

	/** Ce que `goalStickinessBonus` lit sur l'habitant. */
	struct FStickSubject
	{
		FString CurrentGoal;
		bool bCritical = false;
		/** `criticalReliefGoals(npc, sim).has(npc.goal)`. */
		bool bCurrentRelievesCritical = false;
		TOptional<double> PhaseChangedAt;
		double TraitBuild = 1.0;
		double TraitTrade = 1.0;
		double TraitGather = 1.0;
		double TraitExplore = 1.0;
		/** `natureStickBonus(npc, goal)` : 0 pour une nature sans qualite ni defaut. */
		double NatureStick = 0.0;
		/** `npc.workSession` present. */
		bool bWorkSession = false;
		/** `craftBatchCommitment(npc)` : le lot de sciage acquis, 0..1. */
		double CraftBatchCommitment = 0.0;
		/** `inventoryLoad(npc)` : toute la cargaison marchande. */
		int32 InventoryLoad = 0;
	};

	/** `goalStickinessBonus(sim, npc, goal)`. `haulJob` n'est pas un but porte. */
	ANASTASISSIM_API double GoalStickinessBonus(const FStickSubject& Npc, double Now, const FString& Goal);
}
