// Meteo commune — portage de `src/sim/weather.js` (et `fieldSeasonFromDay` de
// `src/sim/fieldCrops.js`, dont elle depend).
//
// Le ciel est une VERITE de simulation, pas un effet : la reference le calcule a
// partir de la graine et du jour, la simulation le lit (`weather.rain > 0.2`
// ralentit les travailleurs dehors, `weatherGoalBias` pousse a s'abriter), et le
// rendu ne fait que le montrer. Porte ici pour que le ciel d'Unreal et le village
// lisent le meme etat, au bit pres.
//
// Porte, et prouve par vecteurs (`Anastasis.Sim.Parite.Meteo`) :
//   weatherAt (saison, roll journalier, vent, front intrajournalier, eclaircie,
//   pluie, neige, givre), sampleCoverFront, coverLobeAt, winterSnowAt,
//   winterFrostAt, weatherWetnessAt, weatherHumidityAt, fieldSeasonFromDay.
//
// PAS porte : windGustAt / sampleWindField / packWindView (rafales en temps REEL,
// pas en temps de simulation : c'est de la presentation), readWindFromView,
// calmWindField. Le biais de climat de terre (`climate`) est porte comme
// parametre optionnel ; la simulation de reference ne le passe pas.

#pragma once

#include "CoreMinimal.h"

#include <limits>

namespace AnastasisWeather
{
	/** The reference's `dayFrac == null`: a NaN hour means "no hour", exactly as `!Number.isFinite(dayFrac)`. */
	inline constexpr double NoHour = std::numeric_limits<double>::quiet_NaN();

	/** `FIELD_SEASONS`, dans l'ordre de la reference. */
	enum class ESeason : uint8
	{
		Spring,
		Summer,
		Autumn,
		Winter,
	};

	/** `FIELD_SEASON_DAYS` / `FIELD_YEAR_DAYS`. */
	inline constexpr double SeasonDays = 30.0;
	inline constexpr double YearDays = 120.0;

	/** Identifiant de la reference ("spring", "summer"...). */
	ANASTASISSIM_API const TCHAR* SeasonId(ESeason Season);

	/** `fieldSeasonFromDay(day)` — jour 1 = premier jour du printemps ; jour <= 0 ou NaN = ete. */
	ANASTASISSIM_API ESeason FieldSeasonFromDay(double Day);

	/** `WEATHER_SEASON_BIAS[season]`. */
	struct FSeasonBias
	{
		double CoverBias = 0.0;
		double RainBias = 0.0;
		double WindBias = 0.0;
		double SwingMul = 1.0;
		double ClearingMul = 1.0;
	};

	ANASTASISSIM_API FSeasonBias SeasonClimateBias(ESeason Season);

	/** Biais de terre optionnel (`climate`) : champs absents = 0, comme `climate.coverBias || 0`. */
	struct FClimateBias
	{
		double CoverBias = 0.0;
		double RainBias = 0.0;
		double WindBias = 0.0;
	};

	/** Retour de `weatherAt`. `bHasDayFrac` faux reproduit l'appel sans heure (`dayFrac == null`). */
	struct FWeather
	{
		double Cover = 0.0;
		double CoverBase = 0.0;
		double Rain = 0.0;
		double Snow = 0.0;
		double Frost = 0.0;
		double Precip = 0.0;
		double Clearing = 0.0;
		double Peak = 0.0;
		ESeason Season = ESeason::Summer;
		double Wind = 0.0;
		/** Radians : direction vers laquelle le vent POUSSE, stable sur la journee. */
		double WindDir = 0.0;
		double DirX = 1.0;
		double DirZ = 0.0;
	};

	/** Retour de `sampleCoverFront`. */
	struct FCoverFront
	{
		double Cover = 0.0;
		double CoverBase = 0.0;
		double Peak = 0.0;
		double Swing = 0.0;
		double Lobe = 0.5;
		double Clearing = 0.0;
		double Rain = 0.0;
	};

	/** `coverLobeAt(dayFrac, peak, sigma)`. */
	ANASTASISSIM_API double CoverLobeAt(double DayFrac, double Peak, double Sigma = 0.11);

	/** `winterSnowAt(season, cover, rain)`. */
	ANASTASISSIM_API double WinterSnowAt(ESeason Season, double Cover, double Rain);

	/** `winterFrostAt(season, cover, snow, dayFrac, clearing)` ; `DayFrac` NaN = `null`. */
	ANASTASISSIM_API double WinterFrostAt(ESeason Season, double Cover, double Snow, double DayFrac, double Clearing);

	/** `sampleCoverFront(coverBase, seed, day, dayFrac, opts)` ; `DayFrac` NaN = `null`. */
	ANASTASISSIM_API FCoverFront SampleCoverFront(double CoverBase, uint32 Seed, double Day, double DayFrac,
		double SwingMul = 1.0, double ClearingMul = 1.0);

	/**
	 * `weatherAt(seed, day, climate, dayFrac)`.
	 * `Day` est flottant comme dans la reference (`1 + time / DAY_LENGTH`) et floore dedans.
	 * `DayFrac` NaN reproduit l'appel sans heure : c'est celui de la simulation
	 * (`simulation.js`), la couverture vaut alors la base journaliere.
	 */
	ANASTASISSIM_API FWeather WeatherAt(uint32 Seed, double Day, const FClimateBias* Climate = nullptr,
		double DayFrac = NoHour);

	/** `weatherWetnessAt(weather)` — humidite visuelle partagee (sol, routes, batiments). */
	ANASTASISSIM_API double WeatherWetnessAt(const FWeather& Weather);

	/** `weatherHumidityAt(weather)` — air encore lourd apres la pluie. */
	ANASTASISSIM_API double WeatherHumidityAt(const FWeather& Weather);
}
