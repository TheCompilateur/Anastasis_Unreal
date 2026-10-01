// Comportement meteo des habitants — portage de `src/ai/weatherGoalBias.js`, du
// bloc pluie de `movementSpeedFactor` (`src/sim/simulation.js`) et de
// `applyRainExposure` / `shelterRainDuration` / `performShelterRain` (`src/sim/npc.js`).
//
// La meteo elle-meme est `AnastasisWeather` (weather.js, en parite). Ici : ce qu'un
// habitant en fait. Sous la pluie on explore moins, on cueille moins, on prefere
// l'atelier, le repos, la compagnie ; l'hiver pousse vers la table et l'abri, l'automne
// vers la recolte. Sous l'orage (pluie >= 0,48), un travailleur dehors lache son
// travail pour un toit, y reste 22 a 38 s, recupere, puis reprend.
//
// Porte, et prouve par vecteurs (`Anastasis.Sim.Parite.MeteoHabitants`) :
//   readSimWeather (hors forceWeather), weatherGoalBiasFromState,
//   shouldSeekRainShelter, shelterRainScore.
// Porte et teste contre la formule transcrite (fonctions NON exportees de la
// reference, que le generateur de vecteurs ne peut pas appeler) :
//   shelterRainDuration, applyRainExposure, le bloc pluie de movementSpeedFactor,
//   la recuperation sous l'auvent d'updateInside, performShelterRain.
//
// Rien ici ne touche un habitant : fonctions pures, le village les applique.

#pragma once

#include "CoreMinimal.h"
#include "World/AnastasisWeather.h"

namespace AnastasisWeatherBehavior
{
	/** Le but « s'abriter de la pluie ». */
	inline const TCHAR* const GoalShelterRain = TEXT("shelterRain");

	/** `WEATHER_GOAL`. */
	namespace GoalWeights
	{
		inline constexpr double RainThreshold = 0.22;
		inline constexpr double SnowThreshold = 0.28;
		inline constexpr double WindThreshold = 0.62;
		inline constexpr double RainExplore = -18.0;
		inline constexpr double RainGather = -11.0;
		inline constexpr double RainBuild = -9.0;
		inline constexpr double RainCraft = 13.0;
		inline constexpr double RainRest = 11.0;
		inline constexpr double RainSocial = 8.0;
		inline constexpr double RainMaintain = 6.0;
		inline constexpr double SnowExplore = -14.0;
		inline constexpr double WinterFood = 15.0;
		inline constexpr double WinterRest = 7.0;
		inline constexpr double WinterBuy = 6.0;
		inline constexpr double AutumnFood = 11.0;
		inline constexpr double AutumnDeliver = 7.0;
		inline constexpr double AutumnSell = 5.0;
		inline constexpr double SpringExplore = 7.0;
		inline constexpr double SpringGatherWood = 5.0;
		inline constexpr double WindExplore = -8.0;
		inline constexpr double WindBuild = -5.0;
	}

	/** `SHELTER_RAIN`. */
	namespace Shelter
	{
		inline constexpr double RainHeavy = 0.48;
		inline constexpr double MinSeconds = 22.0;
		inline constexpr double MaxSeconds = 38.0;
		inline constexpr double EnergyDrainPerSec = 2.4;
		inline constexpr double HealthDrainPerSec = 0.28;
		inline constexpr double EnergyRecover = 14.0;
		/** `performShelterRain` : `shelterCooldownUntil = time + 18`. */
		inline constexpr double CooldownSeconds = 18.0;
		/** `updateInside` : sous l'auvent, `energyDrainPerSec * 0.55 * dt` de recuperation. */
		inline constexpr double InsideRecoverFactor = 0.55;
		/** `applyRainExposure` : la sante ne baisse qu'au-dela de cette pluie. */
		inline constexpr double HealthDrainRain = 0.7;
	}

	/** Ce que `readSimWeather` rend : l'etat du ciel tel que l'habitant le lit. */
	struct FSimWeather
	{
		double Rain = 0.0;
		double Snow = 0.0;
		double Wind = 0.0;
		double Cover = 0.0;
		AnastasisWeather::ESeason Season = AnastasisWeather::ESeason::Summer;
		double Clearing = 0.0;
	};

	/**
	 * `readSimWeather(sim)` sans `forceWeather` : `weatherAt(seed >>> 0, sim.day || 1, null, dayFrac)`
	 * avec `dayFrac = ((time % 90) + 90) % 90 / 90` — CETTE formule, pas `dayFracOf` : les deux
	 * different au dernier bit, et c'est la meteo de la reference qu'on reproduit.
	 */
	ANASTASISSIM_API FSimWeather ReadSimWeather(uint32 Seed, int32 Day, double Time);

	/** `isOutdoorWorker(npc)` : farmer, herder, fisherman, woodcutter, quarryman, guard, butcher. */
	ANASTASISSIM_API bool IsOutdoorWorker(const FString& JobId);

	/** `RAIN_EXPOSED_GOALS`. */
	ANASTASISSIM_API bool IsRainExposedGoal(const FString& Goal);

	/** `weatherGoalBiasFromState(weather, npc, goal)` — biais doux, 0 sans signal. */
	ANASTASISSIM_API double WeatherGoalBias(const FSimWeather& Weather, const FString& JobId, const FString& Goal);

	/**
	 * `shouldSeekRainShelter(sim, npc)`. `CooldownUntil` < 0 = `shelterCooldownUntil` absent.
	 * Dehors, orage, hors delai de grace, et (but expose ou metier d'exterieur).
	 */
	ANASTASISSIM_API bool ShouldSeekRainShelter(double Rain, bool bInside, const FString& Goal, const FString& JobId,
		double Time, double CooldownUntil);

	/** `shelterRainScore(sim, npc)` : 0 sans raison de s'abriter, sinon 24 + pluie * 42 + exposition + metier. */
	ANASTASISSIM_API double ShelterRainScore(double Rain, bool bInside, const FString& Goal, const FString& JobId,
		double Time, double CooldownUntil);

	/** `shelterRainDuration` : 22 s a l'orage naissant, 38 s au deluge. `rain || rainHeavy`. */
	ANASTASISSIM_API double ShelterRainDuration(double Rain);

	/** `applyRainExposure` : dehors sous l'orage, l'energie fond ; au-dela de 0,7 la sante aussi. */
	ANASTASISSIM_API void ApplyRainExposure(double Rain, bool bInside, const FString& Goal, double Dt,
		double& InOutEnergy, double& InOutHealth);

	/**
	 * Le bloc pluie de `movementSpeedFactor`, SEUL (le reste du facteur n'est pas porte) :
	 * avec `weatherAt(seed, 1 + time / DAY_LENGTH)` — la meteo JOURNALIERE, sans heure.
	 * Pluie > 0,2 : on presse le pas vers l'abri (x 1 + pluie * 0,28), vers un but d'abri
	 * (x 1 + pluie * 0,18), sinon on ralentit un peu (x 0,96) ; le travailleur d'exterieur
	 * (ICI : farmer, herder, fisherman, butcher, guard, ou un but `gather*`) garde son pas.
	 */
	ANASTASISSIM_API double RainSpeedFactor(double DailyRain, const FString& Goal, const FString& JobId);
}
