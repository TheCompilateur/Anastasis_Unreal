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
		/** 0 in day and night, 1 while the sun grazes the horizon (TwilightFor). */
		double Twilight = 0.0;
		AnastasisWeather::FWeather Weather;
		double Humidity = 0.0;
		double Wetness = 0.0;
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
	 * Pinned exposure as a function of sun elevation: NightEV at and below LowElevation,
	 * DayEV at and above HighElevation, smoothstep between. Monotonic by construction.
	 */
	double ExposureForSunElevation(double ElevationDegrees, double DayEV100, double NightEV100,
		double LowElevationDegrees, double HighElevationDegrees);

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
	 * Strength of the wetness-driven mist pockets at this instant, as a multiplier of their
	 * ATMOSPHERE_002 extinction. Valley and river mist is radiation fog: it forms in the still,
	 * cool hours and burns off as the sun climbs. So:
	 *   - full strength with the sun at or below the horizon, MistMiddayFactor with it above
	 *     MistBurnOffElevationDegrees, smoothstep between (dawn and dusk are symmetric in sun
	 *     height, the evening mist comes back as the sun sets);
	 *   - humid air after rain thickens it (x (1 + humidity)), wind disperses it (x (1 - 0.6 wind)).
	 * The WHERE stays the simulation's wetness field; this only says WHEN and how much.
	 */
	double MistFactorFor(const UAnastasisAtmosphereProfile& Profile, const FSkyState& State);

	/**
	 * Twilight band for a sun elevation (ENV_REALISM_002): 0 below TwilightStartDegrees and above
	 * TwilightEndDegrees, 1 between TwilightFullLowDegrees and TwilightFullHighDegrees, smoothstep
	 * on both edges. Daytime and night images are untouched by construction.
	 */
	double TwilightFor(const UAnastasisAtmosphereProfile& Profile, double SunElevationDegrees);

	/**
	 * The same instant as DAY_NIGHT_WEATHER_001 rendered it: no twilight band, and saturation and
	 * white point both on the exposure curve. anastasis.Sky.Twilight 0, for the A/B.
	 */
	FSkyState WithoutTwilight(const UAnastasisAtmosphereProfile& Profile, const FSkyState& State);

	/** Lerp(1, FullTwilightScale, Twilight): how a twilight multiplier applies at this instant. */
	double TwilightScale(double Twilight, double FullTwilightScale);

	/**
	 * Ground rain wetness in [0,1] for M_AnastasisGround: the state's WeatherWetnessAt, or a
	 * pinned value (anastasis.Sky.GroundWetness >= 0) for captures. 0 when the profile says
	 * the weather does not wet the ground.
	 */
	double GroundWetnessFor(const UAnastasisAtmosphereProfile& Profile, const FSkyState& State, double PinnedWetness);
}
