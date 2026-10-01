#include "WorldView/AnastasisSkyClock.h"

#include "Life/AnastasisVillageRhythm.h"
#include "WorldView/AnastasisAtmosphereProfile.h"
#include "WorldView/AnastasisAtmosphereResolver.h"

namespace AnastasisSkyClock
{

/** Smoothstep of X between Lo and Hi, clamped. Named for the unity build: WorldView has several SmoothSteps. */
static double SkyClockSmoothStep(const double Lo, const double Hi, const double X)
{
	const double T = FMath::Clamp((X - Lo) / FMath::Max(1e-3, Hi - Lo), 0.0, 1.0);
	return T * T * (3.0 - 2.0 * T);
}

double SimTimeFor(const double Day, const double Hours)
{
	return (FMath::Max(1.0, FMath::FloorToDouble(Day)) - 1.0) * DayLengthSeconds
		+ FMath::Fmod(FMath::Max(0.0, Hours), 24.0) / 24.0 * DayLengthSeconds;
}

double DeclinationForDay(const double Day, const double MaxDeclinationDegrees)
{
	// fieldCrops.js: a 120-day year of four 30-day seasons, day 1 = first day of spring.
	// Astronomically, spring starts at the equinox and summer at the solstice, so day 1 is
	// the equinox (0), day 31 the summer solstice (+max), day 91 the winter one (-max).
	const double DayInYear = FMath::Fmod(FMath::Max(0.0, FMath::FloorToDouble(Day) - 1.0), AnastasisWeather::YearDays);
	return MaxDeclinationDegrees * FMath::Sin(2.0 * UE_DOUBLE_PI * DayInYear / AnastasisWeather::YearDays);
}

double ExposureForSunElevation(const double ElevationDegrees, const double DayEV100, const double NightEV100,
	const double LowElevationDegrees, const double HighElevationDegrees)
{
	const double Span = FMath::Max(1e-3, HighElevationDegrees - LowElevationDegrees);
	const double T = FMath::Clamp((ElevationDegrees - LowElevationDegrees) / Span, 0.0, 1.0);
	const double S = T * T * (3.0 - 2.0 * T);
	return NightEV100 + (DayEV100 - NightEV100) * S;
}

FSkyState Evaluate(const UAnastasisAtmosphereProfile& Profile, const double SimTime, const uint32 Seed)
{
	FSkyState State;
	const double Time = FMath::Max(0.0, SimTime);

	// One clock: the fraction the village's rhythm reads, not a recomputation of it.
	State.DayFrac = AnastasisRhythm::DayFracOf(Time);
	State.Day = 1.0 + FMath::FloorToDouble(Time / DayLengthSeconds);
	State.Hours = State.DayFrac * 24.0;
	State.VillagePhase = AnastasisRhythm::PhaseId(AnastasisRhythm::VillagePhase(State.DayFrac));
	State.DeclinationDegrees = DeclinationForDay(State.Day, Profile.MaxDeclinationDegrees);

	State.SunRotation = AnastasisAtmosphere::SunRotationForTimeOfDay(State.Hours, Profile.LatitudeDegrees, State.DeclinationDegrees);
	// Full moon: opposite hour angle, opposite declination (see ResolveMoonRotation).
	State.MoonRotation = AnastasisAtmosphere::SunRotationForTimeOfDay(
		FMath::Fmod(State.Hours + 12.0, 24.0), Profile.LatitudeDegrees, -State.DeclinationDegrees);
	State.SunElevationDegrees = ElevationOf(State.SunRotation);

	State.ExposureEV100 = ExposureForSunElevation(State.SunElevationDegrees, Profile.ExposureEV100,
		Profile.NightExposureEV100, Profile.NightElevationDegrees, Profile.DayElevationDegrees);
	// The same curve, as a 0..1 daylight factor: night vision fades in exactly as the exposure drops.
	State.Daylight = ExposureForSunElevation(State.SunElevationDegrees, 1.0, 0.0,
		Profile.NightElevationDegrees, Profile.DayElevationDegrees);
	State.Twilight = TwilightFor(Profile, State.SunElevationDegrees);
	// Saturation: night vision on the exposure curve, then a calmer grade while the sun grazes
	// the horizon (ENV_REALISM_002). Both factors are exactly 1 at day.
	State.ColorSaturation = FMath::Lerp(static_cast<double>(Profile.NightColorSaturation), 1.0, State.Daylight)
		* TwilightScale(State.Twilight, Profile.TwilightColorSaturation);
	// The moon's white point only once the moon is what lights the land: on the exposure
	// curve it was already 4540 K at -6 deg, a blue shift over a pink twilight -- magenta.
	const double MoonWhite = 1.0 - SkyClockSmoothStep(Profile.NightElevationDegrees, Profile.NightWhiteBalanceStartDegrees,
		State.SunElevationDegrees);
	State.WhiteTemp = FMath::Lerp(6500.0, static_cast<double>(Profile.NightWhiteTemp), MoonWhite);

	// The renderer's call in the reference: with the hour, so a front can build and clear within a day.
	State.Weather = AnastasisWeather::WeatherAt(Seed, State.Day, nullptr, State.DayFrac);
	State.Humidity = AnastasisWeather::WeatherHumidityAt(State.Weather);
	State.Wetness = AnastasisWeather::WeatherWetnessAt(State.Weather);
	return State;
}

double CloudCoverageFor(const UAnastasisAtmosphereProfile& Profile, const double Cover)
{
	return FMath::Lerp(static_cast<double>(Profile.CloudCoverageClear), static_cast<double>(Profile.CloudCoverageOvercast),
		FMath::Clamp(Cover, 0.0, 1.0));
}

double FogDensityScaleFor(const UAnastasisAtmosphereProfile& Profile, const double Humidity)
{
	return 1.0 + Profile.FogHumidityGain * FMath::Clamp(Humidity, 0.0, 1.0);
}

double FogInscatteringScaleFor(const double ExposureEV100, const double DayEV100)
{
	return FMath::Pow(2.0, FMath::Min(0.0, ExposureEV100 - DayEV100));
}

double MistFactorFor(const UAnastasisAtmosphereProfile& Profile, const FSkyState& State)
{
	const double Span = FMath::Max(1e-3, static_cast<double>(Profile.MistBurnOffElevationDegrees));
	const double T = FMath::Clamp(State.SunElevationDegrees / Span, 0.0, 1.0);
	const double BurnOff = T * T * (3.0 - 2.0 * T);
	const double Diurnal = FMath::Lerp(1.0, static_cast<double>(Profile.MistMiddayFactor), BurnOff);
	const double Humid = 1.0 + FMath::Clamp(State.Humidity, 0.0, 1.0);
	const double Wind = 1.0 - 0.6 * FMath::Clamp(State.Weather.Wind, 0.0, 1.0);
	// Thinner while the sun grazes the horizon: at full strength, white pockets lit by a
	// reddened sun were the pinkest thing in the dawn frame (ENV_REALISM_002).
	const double Twilight = TwilightScale(State.Twilight, Profile.TwilightMistScale);
	return FMath::Clamp(Diurnal * Humid * Wind * Twilight, 0.0, 2.0);
}

double TwilightFor(const UAnastasisAtmosphereProfile& Profile, const double SunElevationDegrees)
{
	const double Rise = SkyClockSmoothStep(Profile.TwilightStartDegrees, Profile.TwilightFullLowDegrees, SunElevationDegrees);
	const double Fall = 1.0 - SkyClockSmoothStep(Profile.TwilightFullHighDegrees, Profile.TwilightEndDegrees, SunElevationDegrees);
	return FMath::Clamp(Rise * Fall, 0.0, 1.0);
}

FSkyState WithoutTwilight(const UAnastasisAtmosphereProfile& Profile, const FSkyState& State)
{
	FSkyState Out = State;
	Out.Twilight = 0.0;
	Out.ColorSaturation = FMath::Lerp(static_cast<double>(Profile.NightColorSaturation), 1.0, State.Daylight);
	Out.WhiteTemp = FMath::Lerp(static_cast<double>(Profile.NightWhiteTemp), 6500.0, State.Daylight);
	return Out;
}

double TwilightScale(const double Twilight, const double FullTwilightScale)
{
	return FMath::Lerp(1.0, FullTwilightScale, FMath::Clamp(Twilight, 0.0, 1.0));
}

double GroundWetnessFor(const UAnastasisAtmosphereProfile& Profile, const FSkyState& State, const double PinnedWetness)
{
	if (!Profile.bWeatherWetsGround)
	{
		return 0.0;
	}
	if (PinnedWetness >= 0.0)
	{
		return FMath::Clamp(PinnedWetness, 0.0, 1.0);
	}
	return FMath::IsFinite(State.Wetness) ? FMath::Clamp(State.Wetness, 0.0, 1.0) : 0.0;
}

}
