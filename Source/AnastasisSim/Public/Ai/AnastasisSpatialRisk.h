// Risque spatial d'un but — `spatialRiskBiasMap` de `src/sim/npc.js` (mission resource-targets-001).
//
// Avant de choisir, l'habitant mesure chaque but lointain : l'aller jusqu'a la cible, puis le retour
// vers ce qui le sauve (manger, boire, dormir, s'abriter), contre le temps qui lui reste avant que la
// faim, la soif, la fatigue, la nuit ou l'averse ne deviennent critiques. Un trajet qui depasse son
// budget penalise la ligne du but (`score.spatial_risk` d'adultScores).
//
// Ici, la partie pure, prouvee au bit pres (`Anastasis.Sim.Parite.RisqueSpatial`) : les budgets, le
// trajet, le depassement et leur combinaison. Les cibles (le gisement, la parcelle, le chantier...) et
// les replis surs viennent du village (`FVillage::SpatialRiskBiasMap`), qui les fournit dans l'ordre
// de la reference : leurs appels ecrivent (le filtre paresseux des seuils), l'ordre compte.

#pragma once

#include "CoreMinimal.h"

namespace AnastasisSpatialRisk
{
	/** `SURVIVAL_FORECAST` — seulement ce que ce portage lit. */
	namespace Forecast
	{
		inline constexpr double RoutePenaltyScale = 28.0;
		inline constexpr double RouteHeavyPenaltyScale = 38.0;
		inline constexpr double ReturnSlackSeconds = 6.0;
		inline constexpr double RainCautionThreshold = 0.32;
		inline constexpr double RainReturnSeconds = 18.0;
	}

	/** `SPATIAL_RISK_GOALS`, dans l'ordre d'insertion du `Set` (celui de la boucle). */
	ANASTASISSIM_API TConstArrayView<const TCHAR*> Goals();

	/** `secondsUntilThreshold(current, critical, risePerSecond)`. */
	ANASTASISSIM_API double SecondsUntilThreshold(double Current, double Critical, double RisePerSecond);

	/** `criticalNeedBudget(npc)` : secondes avant la faim, la soif et la fatigue critiques. */
	struct FNeedBudget
	{
		double Hunger = 0.0;
		double Thirst = 0.0;
		double Rest = 0.0;
	};
	ANASTASISSIM_API FNeedBudget CriticalNeedBudget(double Hunger, double Thirst, double Energy);

	/** `secondsUntilNight(sim)`, sur `dayFracOf(sim)` (heure civile = frac * 24, nuit des 21 h). */
	ANASTASISSIM_API double SecondsUntilNight(double DayFrac);

	/** `rainReturnBudget(weather)` : Infinity sous le seuil de prudence, 0 sous l'orage. */
	ANASTASISSIM_API double RainReturnBudget(double Rain);

	/** `Math.max(1.6, npc.speed || 3.4)`. */
	ANASTASISSIM_API double RouteSpeed(double Speed);

	/** `routeSeconds(npc, target, safeTarget)` : aller, puis retour vers le repli s'il existe. */
	ANASTASISSIM_API double RouteSeconds(double NpcX, double NpcY, double Speed, const FVector2D& Target,
		const TOptional<FVector2D>& Safe);

	/** `budgetOverrun(route, available)`, borne a 1,8. */
	ANASTASISSIM_API double BudgetOverrun(double Route, double Available);

	/** Ce que la boucle lit de l'habitant, du ciel et des replis surs (calcules AVANT les cibles). */
	struct FRiskInputs
	{
		double NpcX = 0.0;
		double NpcY = 0.0;
		double Speed = 0.0;
		double Hunger = 0.0;
		double Thirst = 0.0;
		double Energy = 0.0;
		double DayFrac = 0.0;
		double Rain = 0.0;
		TOptional<FVector2D> RestSafe;
		TOptional<FVector2D> EatSafe;
		TOptional<FVector2D> DrinkSafe;
		/** `shelterRainAccess(sim, npc) || restSafe` : l'appelant fait le repli. */
		TOptional<FVector2D> ShelterSafe;
	};

	/** La pression d'un but dont la cible existe : le pire depassement des cinq budgets. */
	ANASTASISSIM_API double GoalPressure(const FRiskInputs& In, const FVector2D& Target);

	/** `-(pressure * scale)`, 38 pour explore et les trois cueillettes, 28 sinon. */
	ANASTASISSIM_API double GoalBias(const FString& Goal, double Pressure);

	/**
	 * `spatialRiskBiasMap` une fois les replis connus : pour chaque but de `Goals()`, dans l'ordre,
	 * `TargetFor(goal)` (rien = but saute), puis la pression ; seules les pressions > 0 entrent dans
	 * la carte, dans l'ordre de la boucle.
	 */
	ANASTASISSIM_API TArray<TPair<FString, double>> BiasMap(const FRiskInputs& In,
		TFunctionRef<TOptional<FVector2D>(const FString& Goal)> TargetFor);

	// --- Prevision de survie — `survivalForecastBias` (npc.js), calculee juste avant le risque spatial,
	// sur les memes replis (manger, boire, dormir) et dans le meme ordre d'appel.

	namespace Survival
	{
		inline constexpr double HorizonSeconds = 20.0;
		inline constexpr double PrecriticalMargin = 10.0;
		inline constexpr double TravelSafeSeconds = 5.0;
		inline constexpr double EatScale = 2.1;
		inline constexpr double DrinkScale = 2.35;
		inline constexpr double RestScale = 2.05;
		inline constexpr double WorkPenaltyScale = 0.32;
		inline constexpr double HeavyWorkPenaltyScale = 0.52;
	}

	/** `travelPressureToTarget(npc, target, horizon)` : 0 sans cible, borne a 1,4. */
	ANASTASISSIM_API double TravelPressureToTarget(double NpcX, double NpcY, double Speed,
		const TOptional<FVector2D>& Target, double Horizon);

	/** `projectedNeedPressure(npc, seconds)` : faim, soif, fatigue dans `seconds`. */
	struct FProjectedNeeds
	{
		double Hunger = 0.0;
		double Thirst = 0.0;
		double Fatigue = 0.0;
	};
	ANASTASISSIM_API FProjectedNeeds ProjectedNeedPressure(double Hunger, double Thirst, double Energy, double Seconds);

	/** `forecastNeedBias(projected, urgeAt, criticalAt, travelPressure, scale)`. */
	ANASTASISSIM_API double ForecastNeedBias(double Projected, double UrgeAt, double CriticalAt, double TravelPressure,
		double Scale);

	struct FForecastInputs
	{
		double NpcX = 0.0;
		double NpcY = 0.0;
		double Speed = 0.0;
		double Hunger = 0.0;
		double Thirst = 0.0;
		double Energy = 0.0;
		/** `npc.inventory.food | 0`. */
		int32 InventoryFood = 0;
		/** `mealPathBlocked(sim, npc)`. */
		bool bMealBlocked = false;
		/** `forecastEatTarget`, `forecastDrinkTarget`, `forecastRestTarget`. */
		TOptional<FVector2D> EatTarget;
		TOptional<FVector2D> DrinkTarget;
		TOptional<FVector2D> RestTarget;
	};

	/**
	 * `survivalForecastBias` une fois les replis connus : la carte des biais par but, dans l'ordre
	 * d'insertion de la reference (cles a 0 comprises), vide si le risque total est <= 0,01.
	 */
	ANASTASISSIM_API TArray<TPair<FString, double>> SurvivalForecastBias(const FForecastInputs& In);

	/** La valeur d'une carte pour un but, `map[goal] || 0`. */
	ANASTASISSIM_API double BiasOf(const TArray<TPair<FString, double>>& Map, const FString& Goal);
}
