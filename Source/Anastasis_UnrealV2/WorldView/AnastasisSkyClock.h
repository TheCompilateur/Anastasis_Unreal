#pragma once

#include "CoreMinimal.h"
#include "World/AnastasisWeather.h"

class UAnastasisAtmosphereProfile;

/**
 * SKY CLOCK (DAY_NIGHT_WEATHER_001).
 *
 * Until this file the village and the sky lived on two different clocks. The simulation
 * turns: FAnastasisSimulation::DayLength = 90 s, the first frame is ~10h, and
 * AnastasisRhythm puts the village to bed from 21h to 5h. The sun, meanwhile, was pinned at
 * pitch -38 by the atmosphere profile: villagers went to sleep under a noon sky, and nothing
 * in Unreal knew whether it rained, although the reference simulation (weather.js, now
 * AnastasisWeather) decides it every day.
 *
 * This is the pure translation from simulation time to sky, with no actor and no engine
 * state, so every claim it makes is a test:
 *   - the hour IS AnastasisRhythm::DayFracOf(time) * 24, the very fraction that decides
 *     "night" for the village -- one clock, not two that happen to agree;
 *   - the season moves the sun: a 120-day year (fieldCrops.js), day 1 = spring equinox,
 *     day 31 = summer solstice, declination +-23.44;
 *   - the weather is AnastasisWeather::WeatherAt(seed, day, null, dayFrac), the call the
 *     reference's renderer makes, so the sky shows the same front the simulation reads;
 *   - exposure is a deterministic function of sun elevation, not auto-exposure: two
 *     captures at the same hour stay comparable, which is why the project pins exposure.
 *
 * Nothing here writes simulation state. The sky reads the clock; it never sets it.
 */
namespace AnastasisSkyClock
{
	/** Same constant as FAnastasisSimulation::DayLength and AnastasisRhythm::DayLength. */
	inline constexpr double DayLengthSeconds = 90.0;

	/** The reference simulation's first frame: `time0 = DAY_LENGTH * 0.42`. The editor, which has no clock, shows this instant. */
	inline constexpr double InitialSimTime = DayLengthSeconds * 0.42;

	struct FSkyState
	{
		/** `1 + floor(time / DAY_LENGTH)`, as the simulation counts days. */
		double Day = 1.0;
		/** [0,1): AnastasisRhythm::DayFracOf(time). */
		double DayFrac = 0.0;
		/** Local solar hours, DayFrac * 24. */
		double Hours = 0.0;
		double DeclinationDegrees = 0.0;
		FRotator SunRotation = FRotator::ZeroRotator;
		FRotator MoonRotation = FRotator::ZeroRotator;
		/** Degrees above the horizon (negative below). */
		double SunElevationDegrees = 0.0;
		double ExposureEV100 = 14.0;
		/** 0 at night, 1 in full day: the smoothstep in sun elevation that exposure also follows. */
		double Daylight = 1.0;
		/** Post-process colour saturation and white-balance temperature for night vision (1 and 6500 K in daylight). */
		double ColorSaturation = 1.0;
		double WhiteTemp = 6500.0;
		AnastasisWeather::FWeather Weather;
		double Humidity = 0.0;
		double Wetness = 0.0;
		/**
		 * The weather the SKY shows: the simulation's humidity, wind, cover and rain, cross-faded over
		 * the profile's WeatherBlendHours around midnight (see SkyWeatherAt). Weather above stays
		 * the simulation's own, bit for bit; fog, mist, clouds and rain read these.
		 */
		double SkyHumidity = 0.0;
		double SkyWind = 0.0;
		double SkyCover = 0.0;
		/** Visual rain on the same midnight blend as the clouds; never changes Weather.Rain. */
		double SkyRain = 0.0;
		/** 0..1 share of the sun's light the fog may scatter (SunFogScatteringFor). */
		double SunFogScattering = 1.0;
		/** True when the moon, not the sun, is the forward-shading light (MoonLeadsForwardShading). */
		bool bMoonLeadsForward = false;
		/** Village phase id ("night", "dawn"...), from the same fraction. */
		const TCHAR* VillagePhase = TEXT("");
	};

	/** Sim time of (Day, Hours): the inverse of the clock, for pinned captures and tests. */
	double SimTimeFor(double Day, double Hours);

	/** Solar declination for a simulation day: 23.44 * sin(2pi * ((day - 1) mod 120) / 120). */
	double DeclinationForDay(double Day, double MaxDeclinationDegrees = 23.44);

	/** Elevation, in degrees, of a light whose rotation points along its rays. */
	inline double ElevationOf(const FRotator& LightRotation) { return -LightRotation.Pitch; }

	/**
	 * Smoothstep band in sun elevation: NightValue at and below LowElevation, DayValue at and
	 * above HighElevation. Since SKY_TRANSITIONS_001 it drives night vision only (the 0..1
	 * daylight factor); exposure uses the calibrated overload below.
	 */
	double ExposureForSunElevation(double ElevationDegrees, double DayEV100, double NightEV100,
		double LowElevationDegrees, double HighElevationDegrees);

	/**
	 * The pinned exposure for a sun elevation: day EV minus the profile's ExposureStopsBelowDay
	 * (linear between keys, flat beyond), floored at the night EV.
	 */
	double ExposureForSunElevation(const UAnastasisAtmosphereProfile& Profile, double ElevationDegrees);

	/** One step of eye adaptation toward Target: at most MaxPerSecond * DeltaSeconds. */
	double AdaptExposure(double Current, double Target, double DeltaSeconds, double MaxPerSecond);

	/** The whole sky at one simulation instant. Deterministic in (Profile, SimTime, Seed). */
	FSkyState Evaluate(const UAnastasisAtmosphereProfile& Profile, double SimTime, uint32 Seed);

	/** Cloud material coverage for a weather cover in [0,1]: a linear map between the profile's clear and overcast values. */
	double CloudCoverageFor(const UAnastasisAtmosphereProfile& Profile, double Cover);

	/** Global fog density multiplier for a humidity in [0,1]. 1 in dry air. */
	double FogDensityScaleFor(const UAnastasisAtmosphereProfile& Profile, double Humidity);

	/**
	 * Scale of the height fog's authored inscattering LUMINANCE at this exposure.
	 *
	 * FogInscatteringColor is an absolute luminance (cd/m2) authored against the day's EV100.
	 * Left constant, it is negligible at noon and a glowing white band at night: exposed at
	 * EV -1 it lands 2^15 times brighter. First day/night capture, 2026-09-30, showed exactly
	 * that. The light the exposure follows is the light the fog scatters: 2^(EV - DayEV).
	 */
	double FogInscatteringScaleFor(double ExposureEV100, double DayEV100);

	/**
	 * ATMOSPHERE_COHERENCE_001. The renderer lights forward shading, translucency, single layer
	 * water and volumetric fog with ONE directional light: the highest ForwardShadingPriority,
	 * brightness as tie-break. Sun and moon both sat at the engine default 0, so the renderer
	 * warned ("multiple directional lights are competing...") whenever both were rendered and its fallback
	 * kept the 75000 lux sun -- below the horizon, with its shadows switched off -- as the light
	 * of the night's fog and water. The rule is the one the shadows already follow: the sun
	 * leads while it is up, the moon once it has set. Same predicate as IsBelowHorizon.
	 */
	inline bool MoonLeadsForwardShading(const double SunElevationDegrees) { return SunElevationDegrees < 0.0; }

	/** ForwardShadingPriority of the leading light and of the other one. Higher wins. */
	inline constexpr int32 ForwardPriorityLead = 1;
	inline constexpr int32 ForwardPriorityFollow = 0;

	/**
	 * Share of the sun's light the fog scatters (the sun's VolumetricScatteringIntensity): 0 with
	 * the sun at or under the horizon -- it lights nothing through the planet, and fog lit by it
	 * there is the glowing bank of the first captures -- smoothstep to 1 at the profile's
	 * SunFogScatterFullElevationDegrees. Continuous, so a sunset dims the fog instead of switching it.
	 */
	double SunFogScatteringFor(const UAnastasisAtmosphereProfile& Profile, double SunElevationDegrees);

	/** Humidity, wind and cover as the sky shows them. */
	struct FSkyWeather
	{
		double Humidity = 0.0;
		double Wind = 0.0;
		double Cover = 0.0;
		double Rain = 0.0;
	};

	/**
	 * The simulation's weather at (Day, DayFrac), made continuous in time for the sky.
	 *
	 * AnastasisWeather rolls a new base and a new wind every simulation day: at 00:00 humidity
	 * (so fog density) and wind (so mist) jumped in one frame -- a fog bank appearing at the
	 * stroke of midnight. Within BlendHours of midnight the sky cross-fades between the day
	 * ending (taken at its last instant) and the day starting (at its first), with a smoothstep
	 * that is exactly one half on both sides of midnight: continuous, deterministic in
	 * (Seed, Day, DayFrac), and equal to the simulation's weather outside the window. Day 1 has
	 * no previous day to fade from. BlendHours <= 0 returns the raw weather.
	 */
	FSkyWeather SkyWeatherAt(uint32 Seed, double Day, double DayFrac, double BlendHours);

	/**
	 * Local exposure highlight contrast for this daylight factor: the project's own value (the
	 * r.DefaultFeature.LocalExposure.HighlightContrastScale passed as DayValue) in full day, the
	 * profile's TwilightHighlightContrastScale with the sun gone, linear in Daylight between.
	 * Compresses the bright dusk sky toward the dark ground without touching the pinned EV.
	 */
	double HighlightContrastFor(const UAnastasisAtmosphereProfile& Profile, double Daylight, double DayValue);

	/**
	 * Strength of the wetness-driven mist pockets at this instant, as a multiplier of their
	 * ATMOSPHERE_002 extinction. Valley and river mist is radiation fog: it forms in the still,
	 * cool hours and burns off as the sun climbs. So:
	 *   - full strength with the sun at or below the horizon, MistMiddayFactor with it above
	 *     MistBurnOffElevationDegrees, smoothstep between (dawn and dusk are symmetric in sun
	 *     height, the evening mist comes back as the sun sets);
	 *   - humid air after rain thickens it (x (1 + humidity)), wind disperses it (x (1 - 0.6 wind)),
	 *     both read from the sky's cross-faded weather (SkyHumidity, SkyWind).
	 * The WHERE stays the simulation's wetness field; this only says WHEN and how much.
	 */
	double MistFactorFor(const UAnastasisAtmosphereProfile& Profile, const FSkyState& State);
}
