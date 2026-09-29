// Besoins d'un habitant — portage partiel de `src/life/needs.js`.
//
// Porte, et prouve par vecteurs (`Anastasis.Sim.Parite.Besoins`) :
//   urgeScore, needGoalScores (les six buts de besoin, entiers),
//   tickNeeds (branche « boit » et branche par defaut) + tickVitality,
//   satisfyDrink.
//
// PAS porte, volontairement :
//   - les branches interieures de tickNeeds (rest / relieve / eat / socialize /
//     relax) : elles supposent `npc.inside`, donc l'entree dans un batiment,
//     donc le chantier domestique. Aucun habitant porte n'entre nulle part.
//   - tickMoodlets et tickConditioning, appeles en fin de tickNeeds. Ils
//     n'ecrivent aucun des huit metres ci-dessous au meme tick, mais
//     tickConditioning fait deriver `conditioning.fatigueAdaptation`, donc le
//     multiplicateur de fatigue des ticks SUIVANTS. Ici il vaut 1 (valeur
//     neutre 0,5 de la reference) : la parite tient pour un habitant sans
//     conditionnement acquis, pas au-dela.
//   - le genome (phenotype.*Multiplier) : 1 partout, comme un habitant median.

#pragma once

#include "CoreMinimal.h"

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

		inline constexpr double DrinkRelief = 62.0;
		inline constexpr double DrinkHygiene = 6.0;
		inline constexpr double DrinkMorale = 2.0;
		/** `npc.health + 4` dans satisfyDrink — litteral de la reference. */
		inline constexpr double DrinkHealth = 4.0;
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
	ANASTASISSIM_API void TickNeeds(FNeeds& Needs, double Dt, bool bDrinking, bool bWorking);

	/** `tickVitality` — un seul drain a la fois : famine > soif > epuisement. */
	ANASTASISSIM_API void TickVitality(FNeeds& Needs, double Dt);

	/** `satisfyDrink` — la gorgee finale, une fois l'acte accompli. */
	ANASTASISSIM_API void SatisfyDrink(FNeeds& Needs, double Amount = Constants::DrinkRelief);
}
