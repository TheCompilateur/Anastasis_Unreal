// Besoins d'un habitant — portage partiel de `src/life/needs.js`.
//
// Porte, et prouve par vecteurs (`Anastasis.Sim.Parite.Besoins`,
// `Anastasis.Sim.Parite.BesoinsFacteurs`) :
//   urgeScore, needGoalScores (les six buts de besoin, entiers),
//   tickNeeds (toutes ses branches : boit, interieur rest / relieve / eat,
//   socialize, relax, defaut) + tickVitality, satisfyDrink, satisfyRest,
//   satisfyEat, satisfySocial, satisfyRelax, needsCritical ;
//   les cinq facteurs PAR HABITANT (mission needs-factors-001) :
//   hydrationLossFactor, metabolicDemandFactor, fatigueRecoveryFactor
//   (phenotype, voir Life/AnastasisGenome.h), fatigueAdaptationFactor,
//   recoveryConditioningFactor (conditionnement, Life/AnastasisConditioning.h),
//   et l'appel de tickConditioning en fin de tickNeeds.
//
// LES FACTEURS SONT UN ARGUMENT, PAS UN ETAT. Chaque branche prend un
// `FNeedFactors` dont le defaut vaut 1 partout : un habitant median, sans
// conditionnement acquis. Multiplier par 1,0 est exact, donc tout appelant qui
// n'en passe pas garde ses bits — c'est voulu, le village ne les branche pas
// encore.
//
// L'ORDRE DE FIN DE tickNeeds compte, et reste a l'appelant :
//   1. la branche (+ tickVitality), avec les facteurs lus AVANT le tick ;
//   2. tickMoodlets (non porte) — il peut ecrire `morale` ;
//   3. tickConditioning, dont la porte `overworked` lit needsCritical APRES 1 et 2
//      (`TickNeedsConditioning`).
// Les facteurs du tick suivant se relisent donc sur le conditionnement deja avance.
//
// PAS porte, volontairement :
//   - tickMoodlets : sans moodlet actif, il ne fait rien.
//   - ensureNeeds : il tire sur `sim.rng` pour les metres absents ; c'est
//     l'affaire du lecteur et du village, pas des fonctions pures.

#pragma once

#include "CoreMinimal.h"
#include "Life/AnastasisConditioning.h"
#include "Life/AnastasisGenome.h"

namespace AnastasisNeeds
{
	/** Constantes `NEEDS` de la reference — seulement celles que ce portage lit. */
	namespace Constants
	{
		inline constexpr double HungerRise = 0.58;
		inline constexpr double EnergyFall = 0.52;
		inline constexpr double SocialFall = 0.26;
		inline constexpr double LeisureFall = 0.18;
		inline constexpr double WorkSocialExtra = 0.14;
		inline constexpr double WorkLeisureExtra = 0.22;
		inline constexpr double HygieneFall = 0.24;
		inline constexpr double ThirstRise = 0.48;

		/** Soif etanchee par seconde passee au point d'eau (litteral de tickNeeds). */
		inline constexpr double DrinkingThirstFall = 9.5;

		inline constexpr double StarvingAt = 85.0;
		inline constexpr double ExhaustedAt = 12.0;
		inline constexpr double ParchedAt = 88.0;
		inline constexpr double HealthLossStarving = 0.40;
		inline constexpr double HealthLossExhausted = 0.11;
		inline constexpr double HealthLossParched = 0.28;
		inline constexpr double HealthGain = 0.14;
		inline constexpr double HealthUrge = 42.0;
		inline constexpr double HealthCritical = 28.0;
		inline constexpr double LethalThirstMargin = 8.0;

		inline constexpr double MoraleUrge = 38.0;
		inline constexpr double MoraleCritical = 22.0;

		inline constexpr double HungerUrge = 36.0;
		inline constexpr double HungerCritical = 58.0;
		inline constexpr double ThirstUrge = 40.0;
		inline constexpr double ThirstCritical = 68.0;
		inline constexpr double FatigueUrge = 32.0;
		inline constexpr double FatigueCritical = 52.0;
		inline constexpr double LonelyUrge = 38.0;
		inline constexpr double LonelyCritical = 65.0;
		inline constexpr double BoredUrge = 40.0;
		inline constexpr double BoredCritical = 68.0;
		inline constexpr double HygieneUrge = 38.0;
		inline constexpr double HygieneCritical = 66.0;

		inline constexpr double SleepEnergyGain = 6.5;
		inline constexpr double NapEnergyGain = 3.2;
		inline constexpr double HealthSleepRestore = 0.08;
		inline constexpr double SleepRelief = 82.0;
		inline constexpr double NapRelief = 42.0;
		inline constexpr double SleepDuration = 11.5;
		inline constexpr double NapDuration = 4.2;

		inline constexpr double EatHungerFall = 2.4;
		inline constexpr double EatRelief = 58.0;
		inline constexpr double EatDuration = 2.1;
		inline constexpr double HealthEatRestore = 10.0;

		inline constexpr double DrinkRelief = 62.0;
		inline constexpr double DrinkHygiene = 6.0;
		inline constexpr double DrinkMorale = 2.0;
		/** `npc.health + 4` dans satisfyDrink — litteral de la reference. */
		inline constexpr double DrinkHealth = 4.0;

		/** Socialiser et souffler (`NEEDS`). */
		inline constexpr double TalkSocialGain = 3.8;
		inline constexpr double RelaxLeisureGain = 4.2;
		inline constexpr double RelaxEnergyGain = 0.9;
		inline constexpr double SocialRelief = 38.0;
		inline constexpr double SocialAmbient = 14.0;
		inline constexpr double SocialMorale = 4.0;
		inline constexpr double LeisureRelief = 46.0;
		inline constexpr double LeisureMorale = 5.0;
		inline constexpr double SocialDuration = 3.6;
		inline constexpr double RelaxDuration = 4.8;
	}

	/**
	 * Les huit metres d'un habitant, 0..100.
	 *
	 * Morale n'est PAS avancee par tickNeeds. Elle a deux lectures differentes
	 * dans la reference, et les deux sont reproduites a leur point d'usage :
	 * `npc.morale ?? 50` (needGoalScores) et `npc.morale || 50` (satisfyDrink,
	 * ou un moral de 0 remonte donc a 52 — c'est la reference).
	 */
	struct FNeeds
	{
		double Hunger = 0.0;
		double Energy = 70.0;
		double Social = 70.0;
		double Leisure = 70.0;
		double Hygiene = 70.0;
		double Thirst = 0.0;
		double Health = 90.0;
		double Morale = 50.0;
	};

	/** Scores bruts des six buts de besoin, avant la table de decision. */
	struct FNeedGoalScores
	{
		double Eat = 0.0;
		double Rest = 0.0;
		double Socialize = 0.0;
		double Relax = 0.0;
		double Relieve = 0.0;
		double Drink = 0.0;
	};

	/**
	 * Les cinq multiplicateurs propres a un habitant, lus au DEBUT de tickNeeds.
	 * Defaut : 1 partout (phenotype median, conditionnement neutre).
	 */
	struct FNeedFactors
	{
		/** `hydrationLossFactor` — la soif qui MONTE. */
		double Hydration = 1.0;
		/** `metabolicDemandFactor` — la faim qui monte ET l'energie depensee. */
		double Metabolic = 1.0;
		/** `fatigueRecoveryFactor` — le gain continu d'energie en dormant. */
		double FatigueRecovery = 1.0;
		/** `fatigueAdaptationFactor` — l'energie depensee, en plus de Metabolic. */
		double FatigueAdaptation = 1.0;
		/** `recoveryConditioningFactor` — le gain en dormant, en plus de FatigueRecovery. */
		double RecoveryConditioning = 1.0;
	};

	/** `hydrationLossFactor(npc)` = `npc.phenotype?.hydrationLossMultiplier ?? 1`. */
	ANASTASISSIM_API double HydrationLossFactor(const AnastasisGenome::FPhenotype* Phenotype);
	/** `metabolicDemandFactor(npc)` = `npc.phenotype?.metabolicDemandMultiplier ?? 1`. */
	ANASTASISSIM_API double MetabolicDemandFactor(const AnastasisGenome::FPhenotype* Phenotype);
	/** `fatigueRecoveryFactor(npc)` = `npc.phenotype?.fatigueRecoveryMultiplier ?? 1`. */
	ANASTASISSIM_API double FatigueRecoveryFactor(const AnastasisGenome::FPhenotype* Phenotype);
	/** `fatigueAdaptationFactor(npc)` — `npc.conditioning?.fatigueAdaptation`, absent = 0,5. */
	ANASTASISSIM_API double FatigueAdaptationFactor(const AnastasisConditioning::FConditioning* Conditioning);
	/** `recoveryConditioningFactor(npc)` — `npc.conditioning?.recoveryConditioning`, absent = 0,5. */
	ANASTASISSIM_API double RecoveryConditioningFactor(const AnastasisConditioning::FConditioning* Conditioning);

	/** Les cinq d'un coup. nullptr = champ absent sur l'habitant JS. */
	ANASTASISSIM_API FNeedFactors NeedFactorsFor(
		const AnastasisGenome::FPhenotype* Phenotype,
		const AnastasisConditioning::FConditioning* Conditioning);

	/**
	 * `needsCritical(npc)` — un metre au-dela de son seuil critique, sante basse
	 * ou moral (`?? 50`) sous moraleCritical. Le `ensureNeeds` de tete n'est pas
	 * repris : les metres d'un FNeeds existent toujours.
	 */
	ANASTASISSIM_API bool AreNeedsCritical(const FNeeds& Needs);
	// Pas `NeedsCritical` : AnastasisVillage porte deja une fonction de ce nom sur FNeeds,
	// et la recherche dependante des arguments rendrait ses appels ambigus.

	/**
	 * La fin de `tickNeeds` : `tickConditioning` avec les drapeaux de la reference.
	 *   working    = WORK_GOALS.has(npc.goal)  — dedans OU dehors, contrairement a
	 *                bWorking de TickNeeds qui exclut l'interieur ;
	 *   resting    = npc.goal === "rest" ;
	 *   overworked = needsCritical(npc), sur les metres APRES le tick et les moodlets ;
	 *   fatigued   = 100 - energy >= fatigueUrge.
	 */
	ANASTASISSIM_API void TickNeedsConditioning(
		AnastasisConditioning::FConditioning& Conditioning,
		const FNeeds& NeedsAfterTick,
		double Dt,
		bool bWorkGoal,
		bool bRestGoal);

	/** `urgeScore` — discret sous le seuil, deborde les metiers au-dela. */
	ANASTASISSIM_API double UrgeScore(double Pressure, double UrgeAt, double CriticalAt);

	/**
	 * `needGoalScores`. Les deux lectures du monde de la reference
	 * (`sim.countBuildings("tavern")`, `sim.countBuildings("well")`) arrivent en
	 * arguments : ce module ne connait pas le village.
	 */
	ANASTASISSIM_API FNeedGoalScores NeedGoalScores(const FNeeds& Needs, int32 CompletedWells, int32 CompletedTaverns);

	/**
	 * `tickNeeds`, hors interieur, puis `tickVitality`.
	 *
	 * bDrinking = `npc.goal === "drink" && !npc.inside && atDrinkSpot(sim, npc)`.
	 * bWorking  = but de travail et pas a l'interieur.
	 * Le test d'eau reste a l'appelant : il lit les tuiles et les puits.
	 */
	ANASTASISSIM_API void TickNeeds(FNeeds& Needs, double Dt, bool bDrinking, bool bWorking, const FNeedFactors& Factors = FNeedFactors());

	/**
	 * `tickNeeds`, branche interieure `rest` (`insideGoal === "rest"`), puis `tickVitality`.
	 * bNight = `isNightPhase(sim)` ; SleepQuality = `sleepQuality(npc)` (domestic.js).
	 */
	ANASTASISSIM_API void TickNeedsRestInside(FNeeds& Needs, double Dt, bool bNight, double SleepQuality, const FNeedFactors& Factors = FNeedFactors());

	/** `tickNeeds`, branche interieure `eat` / `eatTogether`, puis `tickVitality`. */
	ANASTASISSIM_API void TickNeedsEatInside(FNeeds& Needs, double Dt, const FNeedFactors& Factors = FNeedFactors());

	/** `tickNeeds`, branche interieure `relieve`, puis `tickVitality`. */
	ANASTASISSIM_API void TickNeedsRelieveInside(FNeeds& Needs, double Dt, const FNeedFactors& Factors = FNeedFactors());

	/** `tickVitality` — un seul drain a la fois : famine > soif > epuisement. */
	/**
	 * `tickNeeds`, branche `socialize` : dedans (`insideGoal`), ou dehors tant que le
	 * but est `socialize` (gain x 0,45). Ni hygiene ni repos ne bougent ici.
	 */
	ANASTASISSIM_API void TickNeedsSocialize(FNeeds& N, double Dt, bool bInsideGoal, const FNeedFactors& Factors = FNeedFactors());

	/** `tickNeeds`, branche `relax` : dedans ou dehors, meme branche. */
	ANASTASISSIM_API void TickNeedsRelax(FNeeds& N, double Dt, const FNeedFactors& Factors = FNeedFactors());

	/** `satisfySocial(npc, amount)` : dedans, gain x 0,45 et loisir +3 au lieu de +6. */
	ANASTASISSIM_API void SatisfySocial(FNeeds& N, double Amount, bool bIndoor);

	/** `satisfyRelax(npc)` : dedans vers 78 de loisir et +6 d'energie, dehors +46 et +12. */
	ANASTASISSIM_API void SatisfyRelax(FNeeds& N, bool bIndoor);

	ANASTASISSIM_API void TickVitality(FNeeds& Needs, double Dt);

	/**
	 * `satisfyRest` — la fin d'un repos. bNight = nuit et pas garde ; bIndoor =
	 * `npc.inside` ; bAtHome = `isAtHome(npc)` (a l'interieur de son foyer ou abri).
	 */
	ANASTASISSIM_API void SatisfyRest(FNeeds& Needs, bool bNight, double SleepQuality, bool bIndoor, bool bAtHome);

	/** `DOMESTIC` de life/domestic.js — qualite du repos selon le lieu. */
	namespace Domestic
	{
		inline constexpr double HomeEnterRadius = 3.6;
		inline constexpr double OutdoorRestFactor = 0.42;
		inline constexpr double ShelterRestFactor = 0.78;
		inline constexpr double HomeRestBonus = 1.12;
		inline constexpr double HomeMorale = 2.0;
		inline constexpr double HomeEatBonus = 8.0;
	}

	/**
	 * `sleepQuality(npc)` — ne lit que des identifiants. InsideBuildingId vide =
	 * dehors ; HomeId / ShelterId vides = sans toit / sans abri.
	 */
	ANASTASISSIM_API double SleepQuality(const FString& InsideBuildingId, const FString& HomeId, const FString& ShelterId);

	/**
	 * `satisfyEat` — le repas acheve. bIndoor = `npc.inside` ; bAtHome = `isAtHome(npc)`.
	 * `starvingDays` (remis a 0 par la reference) n'est pas porte : la famine ne l'est pas.
	 */
	ANASTASISSIM_API void SatisfyEat(FNeeds& Needs, bool bIndoor, bool bAtHome, double Amount = Constants::EatRelief);

	/** `satisfyDrink` — la gorgee finale, une fois l'acte accompli. */
	ANASTASISSIM_API void SatisfyDrink(FNeeds& Needs, double Amount = Constants::DrinkRelief);
}
