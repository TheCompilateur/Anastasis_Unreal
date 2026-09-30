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
}
