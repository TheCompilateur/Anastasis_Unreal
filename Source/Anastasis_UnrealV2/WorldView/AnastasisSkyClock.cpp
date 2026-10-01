#include "WorldView/AnastasisSkyClock.h"

#include "Life/AnastasisVillageRhythm.h"
#include "WorldView/AnastasisAtmosphereProfile.h"
#include "WorldView/AnastasisAtmosphereResolver.h"

namespace AnastasisSkyClock
{

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

double ExposureForSunElevation(const UAnastasisAtmosphereProfile& Profile, const double ElevationDegrees)
{
	const TArray<FVector2D>& Keys = Profile.ExposureStopsBelowDay;
	double Stops = 0.0;
	if (Keys.Num() > 0)
	{
		if (ElevationDegrees <= Keys[0].X)
		{
			Stops = Keys[0].Y;
		}
		else if (ElevationDegrees >= Keys.Last().X)
		{
			Stops = Keys.Last().Y;
		}
		else
		{
			for (int32 I = 1; I < Keys.Num(); ++I)
			{
				if (ElevationDegrees <= Keys[I].X)
				{
					const double Span = FMath::Max(1e-9, Keys[I].X - Keys[I - 1].X);
					Stops = FMath::Lerp(Keys[I - 1].Y, Keys[I].Y, (ElevationDegrees - Keys[I - 1].X) / Span);
					break;
				}
			}
		}
	}
	return FMath::Max(static_cast<double>(Profile.NightExposureEV100), Profile.ExposureEV100 - Stops);
}

double AdaptExposure(const double Current, const double Target, const double DeltaSeconds, const double MaxPerSecond)
{
	const double Step = FMath::Max(0.0, MaxPerSecond * DeltaSeconds);
	return Current + FMath::Clamp(Target - Current, -Step, Step);
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

	State.ExposureEV100 = ExposureForSunElevation(Profile, State.SunElevationDegrees);
	// A 0..1 daylight factor for night vision, on the twilight band.
	State.Daylight = ExposureForSunElevation(State.SunElevationDegrees, 1.0, 0.0,
		Profile.NightElevationDegrees, Profile.DayElevationDegrees);
	State.ColorSaturation = FMath::Lerp(static_cast<double>(Profile.NightColorSaturation), 1.0, State.Daylight);
	State.WhiteTemp = FMath::Lerp(static_cast<double>(Profile.NightWhiteTemp), 6500.0, State.Daylight);

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
	return FMath::Clamp(Diurnal * Humid * Wind, 0.0, 2.0);
}

}
