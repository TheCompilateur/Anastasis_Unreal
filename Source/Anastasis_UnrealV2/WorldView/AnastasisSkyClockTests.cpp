#include "Misc/AutomationTest.h"

#include "Life/AnastasisVillageRhythm.h"
#include "World/AnastasisWeather.h"
#include "WorldView/AnastasisAtmosphereProfile.h"
#include "WorldView/AnastasisSkyClock.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace AnastasisSkyClockTest
{
	uint64 Bits(double Value)
	{
		uint64 Out;
		FMemory::Memcpy(&Out, &Value, sizeof(Out));
		return Out;
	}
}

/**
 * ONE clock, not two that happen to agree: the sky's hour and phase are the village's own
 * fraction (AnastasisRhythm::DayFracOf), at every instant of two simulated years.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisSkyClockOneClock, "Anastasis.Sky.Clock.OneClock", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnastasisSkyClockOneClock::RunTest(const FString&)
{
	using AnastasisSkyClockTest::Bits;
	const UAnastasisAtmosphereProfile* Profile = UAnastasisAtmosphereProfile::CreateCodeDefaults(GetTransientPackage());
	int32 Bad = 0;
	for (double Time = 0.0; Time < 240.0 * AnastasisSkyClock::DayLengthSeconds; Time += 0.37)
	{
		const AnastasisSkyClock::FSkyState S = AnastasisSkyClock::Evaluate(*Profile, Time, 12345u);
		const double Frac = AnastasisRhythm::DayFracOf(Time);
		const FString Phase = AnastasisRhythm::PhaseId(AnastasisRhythm::VillagePhase(Frac));
		if (Bits(S.DayFrac) != Bits(Frac) || Phase != S.VillagePhase
			|| S.Day != 1.0 + FMath::FloorToDouble(Time / AnastasisSkyClock::DayLengthSeconds))
		{
			if (++Bad <= 5)
			{
				AddError(FString::Printf(TEXT("t=%.2f sky frac=%.17g phase=%s / village frac=%.17g phase=%s"),
					Time, S.DayFrac, S.VillagePhase, Frac, *Phase));
			}
		}
	}
	TestEqual(TEXT("instants where the sky and the village disagree on the time"), Bad, 0);

	// The editor stand-in is the reference simulation's first frame (~10h), not an arbitrary noon.
	const AnastasisSkyClock::FSkyState First = AnastasisSkyClock::Evaluate(*Profile, AnastasisSkyClock::InitialSimTime, 12345u);
	TestEqual(TEXT("the first frame is day 1"), First.Day, 1.0);
	TestEqual(TEXT("the first frame is at 10.08h"), First.Hours, 0.42 * 24.0, 1e-9);

	// Pins are the inverse of the clock.
	for (const double Day : {1.0, 17.0, 95.0})
	{
		for (const double Hours : {0.0, 6.5, 12.0, 23.75})
		{
			const AnastasisSkyClock::FSkyState P = AnastasisSkyClock::Evaluate(*Profile, AnastasisSkyClock::SimTimeFor(Day, Hours), 12345u);
			TestEqual(FString::Printf(TEXT("pinned day %.0f"), Day), P.Day, Day);
			TestEqual(FString::Printf(TEXT("pinned hour %.2f on day %.0f"), Hours, Day), P.Hours, Hours, 1e-9);
		}
	}
	return true;
}

/**
 * The inconsistency this mission exists to remove: the village slept under a noon sun. Over a
 * whole year, every 15 simulated minutes: while the village's phase is "night" (21h-5h) the sun
 * is below the horizon or grazing it (< 6 deg, the summer dawn at 41 N rises before 5h); at
 * "midday" it stands high (> 20 deg, the winter noon is 25.6 deg); and the pinned exposure is
 * darker at every night instant than at every midday one.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisSkyClockVillageSleepsInTheDark, "Anastasis.Sky.Clock.VillageSleepsInTheDark", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnastasisSkyClockVillageSleepsInTheDark::RunTest(const FString&)
{
	const UAnastasisAtmosphereProfile* Profile = UAnastasisAtmosphereProfile::CreateCodeDefaults(GetTransientPackage());
	double MaxNightElevation = -90.0;
	double MinMiddayElevation = 90.0;
	double MaxNightEV = -100.0;
	double MinMiddayEV = 100.0;
	for (int32 Day = 1; Day <= 120; ++Day)
	{
		for (int32 Quarter = 0; Quarter < 96; ++Quarter)
		{
			const double Time = AnastasisSkyClock::SimTimeFor(Day, Quarter * 0.25);
			const AnastasisSkyClock::FSkyState S = AnastasisSkyClock::Evaluate(*Profile, Time, 12345u);
			const FString Phase = S.VillagePhase;
			if (Phase == TEXT("night"))
			{
				MaxNightElevation = FMath::Max(MaxNightElevation, S.SunElevationDegrees);
				MaxNightEV = FMath::Max(MaxNightEV, S.ExposureEV100);
				TestTrue(TEXT("night phase is also AnastasisRhythm::IsNightPhase"), AnastasisRhythm::IsNightPhase(Time));
			}
			else if (Phase == TEXT("midday"))
			{
				MinMiddayElevation = FMath::Min(MinMiddayElevation, S.SunElevationDegrees);
				MinMiddayEV = FMath::Min(MinMiddayEV, S.ExposureEV100);
			}
		}
	}
	AddInfo(FString::Printf(TEXT("ANASTASIS_SKY_YEAR max_night_sun_elev=%.2f min_midday_sun_elev=%.2f max_night_ev=%.2f min_midday_ev=%.2f"),
		MaxNightElevation, MinMiddayElevation, MaxNightEV, MinMiddayEV));
	TestTrue(TEXT("the village never sleeps under a risen sun (night phase: sun < 6 deg)"), MaxNightElevation < 6.0);
	TestTrue(TEXT("midday is always a high sun (> 20 deg)"), MinMiddayElevation > 20.0);
	TestTrue(TEXT("every night instant is exposed darker than every midday instant"), MaxNightEV < MinMiddayEV);
	return true;
}

/**
 * The season moves the sun, the way the 120-day year of fieldCrops.js says: day 1 equinox,
 * day 31 summer solstice, day 91 winter solstice. And exposure is monotonic in sun elevation,
 * pinned at the night and day values outside the twilight band.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisSkyClockSeasonsAndExposure, "Anastasis.Sky.Clock.SeasonsAndExposure", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnastasisSkyClockSeasonsAndExposure::RunTest(const FString&)
{
	const UAnastasisAtmosphereProfile* Profile = UAnastasisAtmosphereProfile::CreateCodeDefaults(GetTransientPackage());
	TestEqual(TEXT("day 1 is the equinox"), AnastasisSkyClock::DeclinationForDay(1.0), 0.0, 1e-12);
	TestEqual(TEXT("day 31 is the summer solstice"), AnastasisSkyClock::DeclinationForDay(31.0), 23.44, 1e-9);
	TestEqual(TEXT("day 91 is the winter solstice"), AnastasisSkyClock::DeclinationForDay(91.0), -23.44, 1e-9);
	TestEqual(TEXT("the year wraps at 120 days"), AnastasisSkyClock::DeclinationForDay(121.0), AnastasisSkyClock::DeclinationForDay(1.0), 1e-12);

	auto NoonElevation = [Profile](double Day)
	{
		return AnastasisSkyClock::Evaluate(*Profile, AnastasisSkyClock::SimTimeFor(Day, 12.0), 12345u).SunElevationDegrees;
	};
	TestEqual(TEXT("equinox noon at 41 N is 49 deg"), NoonElevation(1.0), 49.0, 0.01);
	TestTrue(TEXT("summer noon is higher than equinox noon"), NoonElevation(31.0) > NoonElevation(1.0));
	TestTrue(TEXT("winter noon is lower than equinox noon"), NoonElevation(91.0) < NoonElevation(1.0));

	const double Day = Profile->ExposureEV100;
	const double Night = Profile->NightExposureEV100;
	TestEqual(TEXT("deep night holds the night exposure"), AnastasisSkyClock::ExposureForSunElevation(-40.0, Day, Night, Profile->NightElevationDegrees, Profile->DayElevationDegrees), Night, 1e-12);
	TestEqual(TEXT("full day holds the day exposure"), AnastasisSkyClock::ExposureForSunElevation(60.0, Day, Night, Profile->NightElevationDegrees, Profile->DayElevationDegrees), Day, 1e-12);
	double Previous = -1000.0;
	bool bMonotonic = true;
	for (double E = -40.0; E <= 60.0; E += 0.25)
	{
		const double EV = AnastasisSkyClock::ExposureForSunElevation(E, Day, Night, Profile->NightElevationDegrees, Profile->DayElevationDegrees);
		bMonotonic &= EV >= Previous;
		Previous = EV;
	}
	TestTrue(TEXT("exposure never darkens as the sun rises"), bMonotonic);

	// Night vision: neutral by day, faded and white-balanced to the moon by night -- and a
	// moon-adapted white point, never a blue grade (it stays warmer than 4000 K).
	const AnastasisSkyClock::FSkyState Noon = AnastasisSkyClock::Evaluate(*Profile, AnastasisSkyClock::SimTimeFor(1.0, 12.0), 12345u);
	const AnastasisSkyClock::FSkyState Midnight = AnastasisSkyClock::Evaluate(*Profile, AnastasisSkyClock::SimTimeFor(1.0, 0.0), 12345u);
	TestEqual(TEXT("day: engine-neutral saturation"), Noon.ColorSaturation, 1.0, 1e-12);
	TestEqual(TEXT("day: engine-neutral white point"), Noon.WhiteTemp, 6500.0, 1e-9);
	TestEqual(TEXT("night: the profile's night saturation"), Midnight.ColorSaturation, static_cast<double>(Profile->NightColorSaturation), 1e-6);
	TestTrue(TEXT("night: colour fades"), Midnight.ColorSaturation < 0.6);
	TestTrue(TEXT("night: white point adapts toward the moon, not to a blue grade"),
		Midnight.WhiteTemp < 6500.0 && Midnight.WhiteTemp > 4000.0);
	return true;
}

/**
 * The sky's weather IS the simulation's: AnastasisWeather::WeatherAt with the sky's own day and
 * fraction, bit for bit -- not a presentation-side roll. And the weather actually reaches the
 * sky's parameters: cover moves the cloud coverage monotonically between the profile's clear and
 * overcast values, humidity thickens the fog and dry air leaves it at the profile's density.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisSkyClockWeatherIsTheSimulations, "Anastasis.Sky.Clock.WeatherIsTheSimulations", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnastasisSkyClockWeatherIsTheSimulations::RunTest(const FString&)
{
	using AnastasisSkyClockTest::Bits;
	const UAnastasisAtmosphereProfile* Profile = UAnastasisAtmosphereProfile::CreateCodeDefaults(GetTransientPackage());
	int32 Bad = 0;
	int32 RainyInstants = 0;
	for (double Time = 0.0; Time < 120.0 * AnastasisSkyClock::DayLengthSeconds; Time += 1.7)
	{
		const AnastasisSkyClock::FSkyState S = AnastasisSkyClock::Evaluate(*Profile, Time, 12345u);
		const AnastasisWeather::FWeather W = AnastasisWeather::WeatherAt(12345u, S.Day, nullptr, S.DayFrac);
		if (Bits(S.Weather.Cover) != Bits(W.Cover) || Bits(S.Weather.Rain) != Bits(W.Rain) || S.Weather.Season != W.Season
			|| Bits(S.Humidity) != Bits(AnastasisWeather::WeatherHumidityAt(W)))
		{
			++Bad;
		}
		RainyInstants += S.Weather.Rain > 0.2 ? 1 : 0;
	}
	TestEqual(TEXT("instants where the sky's weather differs from the simulation's"), Bad, 0);
	AddInfo(FString::Printf(TEXT("ANASTASIS_SKY_WEATHER rainy_instants_over_a_year=%d"), RainyInstants));
	TestTrue(TEXT("a year of seed 12345 has some rain (the model is not inert)"), RainyInstants > 0);

	TestEqual(TEXT("clear sky -> clear coverage"), AnastasisSkyClock::CloudCoverageFor(*Profile, 0.0), static_cast<double>(Profile->CloudCoverageClear), 1e-6);
	TestEqual(TEXT("overcast -> overcast coverage"), AnastasisSkyClock::CloudCoverageFor(*Profile, 1.0), static_cast<double>(Profile->CloudCoverageOvercast), 1e-6);
	TestTrue(TEXT("more cover, more coverage"), AnastasisSkyClock::CloudCoverageFor(*Profile, 0.7) > AnastasisSkyClock::CloudCoverageFor(*Profile, 0.3));
	TestEqual(TEXT("dry air leaves the fog at the profile density"), AnastasisSkyClock::FogDensityScaleFor(*Profile, 0.0), 1.0, 1e-12);
	TestTrue(TEXT("humid air thickens the fog"), AnastasisSkyClock::FogDensityScaleFor(*Profile, 0.7) > 1.0);
	return true;
}

/**
 * The two night defects of the first day/night capture (2026-09-30), locked as rules:
 *   - the fog's authored luminance follows the exposure, so at night it is (almost) gone
 *     instead of glowing 2^15 times too bright, and at day it is exactly the authored value;
 *   - the mist is radiation fog: full at night and dawn, burnt off under a high sun, thicker
 *     in humid air, thinner in wind.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisSkyClockFogAndMistFollowTheLight, "Anastasis.Sky.Clock.FogAndMistFollowTheLight", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnastasisSkyClockFogAndMistFollowTheLight::RunTest(const FString&)
{
	const UAnastasisAtmosphereProfile* Profile = UAnastasisAtmosphereProfile::CreateCodeDefaults(GetTransientPackage());
	const double DayEV = Profile->ExposureEV100;
	TestEqual(TEXT("day: authored fog luminance, unscaled"), AnastasisSkyClock::FogInscatteringScaleFor(DayEV, DayEV), 1.0, 1e-12);
	TestTrue(TEXT("night: fog luminance follows the 15-stop drop"),
		AnastasisSkyClock::FogInscatteringScaleFor(Profile->NightExposureEV100, DayEV) < 1e-4);
	TestEqual(TEXT("never brighter than authored"), AnastasisSkyClock::FogInscatteringScaleFor(DayEV + 3.0, DayEV), 1.0, 1e-12);

	auto At = [Profile](double Day, double Hours)
	{
		return AnastasisSkyClock::Evaluate(*Profile, AnastasisSkyClock::SimTimeFor(Day, Hours), 12345u);
	};
	// Calm, dry reference state, varied one factor at a time.
	AnastasisSkyClock::FSkyState Dawn = At(34.0, 5.5);
	AnastasisSkyClock::FSkyState Noon = At(34.0, 13.0);
	for (AnastasisSkyClock::FSkyState* S : {&Dawn, &Noon})
	{
		S->Humidity = 0.0;
		S->Weather.Wind = 0.0;
	}
	const double MistDawn = AnastasisSkyClock::MistFactorFor(*Profile, Dawn);
	const double MistNoon = AnastasisSkyClock::MistFactorFor(*Profile, Noon);
	AddInfo(FString::Printf(TEXT("ANASTASIS_SKY_MIST dawn=%.3f noon=%.3f"), MistDawn, MistNoon));
	TestTrue(TEXT("the sun burns the mist off: noon well below dawn"), MistNoon < 0.5 * MistDawn);
	TestEqual(TEXT("a high sun leaves the profile's midday trace"), MistNoon, static_cast<double>(Profile->MistMiddayFactor), 1e-6);

	AnastasisSkyClock::FSkyState Humid = Dawn;
	Humid.Humidity = 0.7;
	TestTrue(TEXT("humid air thickens the mist"), AnastasisSkyClock::MistFactorFor(*Profile, Humid) > MistDawn);
	AnastasisSkyClock::FSkyState Windy = Dawn;
	Windy.Weather.Wind = 0.8;
	TestTrue(TEXT("wind disperses the mist"), AnastasisSkyClock::MistFactorFor(*Profile, Windy) < MistDawn);
	return true;
}

/**
 * SKY_TRANSITIONS_001 -- the sunset "flash". Measured 2026-09-30 on Lvl_AnastasisSlice (day 1,
 * capture-sky sunset_before): with the first exposure curve the valley view's mean luma ROSE as
 * the sun set (141 at +2.26 deg, 183 at -1.13 deg), the overview's too (144 -> 192), then both
 * crashed (24 by -9.03 deg).
 *
 * The scene's own light per elevation, for the valley and the overview, is recovered from a
 * second sweep (sunset_after) as log2(mean luma) + the EV100 logged for that capture --
 * tonemapper ignored, an approximation stated. Replaying it against a curve predicts the
 * displayed log-luma of both views. Required of the current curve: no step brightens a view by
 * more than a quarter stop (the scatter of the measurement itself), and nothing after the first
 * frame is brighter than it. Required of the FIRST curve on the same data: that it fails --
 * otherwise this test would prove nothing.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisSkyClockSunsetOnlyDarkens, "Anastasis.Sky.Clock.SunsetOnlyDarkens", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnastasisSkyClockSunsetOnlyDarkens::RunTest(const FString&)
{
	const UAnastasisAtmosphereProfile* Profile = UAnastasisAtmosphereProfile::CreateCodeDefaults(GetTransientPackage());

	// (sun elevation deg, scene log-light: valley view, overview)
	struct FSample { double Elevation; double Valley; double Overview; };
	const FSample Sweep[] = {
		{ 4.52, 19.44, 19.10 }, { 3.39, 18.33, 18.31 }, { 2.26, 17.00, 17.34 }, { 1.13, 16.16, 16.44 },
		{ 0.00, 15.30, 15.52 }, { -1.13, 14.41, 14.49 }, { -2.26, 13.37, 13.48 }, { -3.39, 12.25, 12.40 },
		{ -4.52, 10.97, 11.18 }, { -5.65, 9.26, 9.23 }, { -6.78, 7.53, 7.74 }, { -7.91, 5.87, 6.27 },
		{ -9.03, 4.19, 4.18 },
	};
	constexpr double Tolerance = 0.25; // stops

	auto Darkens = [&Sweep, Tolerance](TFunctionRef<double(double)> CurveEV, FString* Trace)
	{
		bool bOk = true;
		for (const bool bValley : {true, false})
		{
			double First = 0.0, Previous = 0.0;
			for (int32 I = 0; I < UE_ARRAY_COUNT(Sweep); ++I)
			{
				const double Shown = (bValley ? Sweep[I].Valley : Sweep[I].Overview) - CurveEV(Sweep[I].Elevation);
				if (I == 0)
				{
					First = Shown;
				}
				else
				{
					bOk &= Shown <= Previous + Tolerance && Shown <= First + Tolerance;
				}
				Previous = Shown;
				if (Trace)
				{
					*Trace += FString::Printf(TEXT(" %s%.2f:%.0f"), bValley ? TEXT("v") : TEXT("o"), Sweep[I].Elevation, FMath::Pow(2.0, Shown));
				}
			}
		}
		return bOk;
	};

	FString Trace;
	const bool bNew = Darkens([Profile](double E) { return AnastasisSkyClock::ExposureForSunElevation(*Profile, E); }, &Trace);
	const bool bOld = Darkens([Profile](double E)
	{
		return AnastasisSkyClock::ExposureForSunElevation(E, Profile->ExposureEV100, Profile->NightExposureEV100,
			Profile->NightElevationDegrees, Profile->DayElevationDegrees);
	}, nullptr);
	AddInfo(TEXT("ANASTASIS_SKY_SUNSET predicted_luma") + Trace);
	TestFalse(TEXT("the replay reproduces the flash with the first curve"), bOld);
	TestTrue(TEXT("with the calibrated curve both views only darken through sunset"), bNew);

	// The curve itself: never darker by day than by night, day EV at a high sun, night floor below -9 deg.
	double Previous = -1e9;
	bool bMonotonic = true;
	for (double E = -30.0; E <= 60.0; E += 0.25)
	{
		const double EV = AnastasisSkyClock::ExposureForSunElevation(*Profile, E);
		bMonotonic &= EV >= Previous - 1e-9;
		Previous = EV;
	}
	TestTrue(TEXT("exposure never darkens as the sun rises"), bMonotonic);
	TestEqual(TEXT("high sun: the day exposure"), AnastasisSkyClock::ExposureForSunElevation(*Profile, 45.0), static_cast<double>(Profile->ExposureEV100), 1e-9);
	TestEqual(TEXT("deep night: the night floor"), AnastasisSkyClock::ExposureForSunElevation(*Profile, -30.0), static_cast<double>(Profile->NightExposureEV100), 1e-9);
	return true;
}

/**
 * Eye adaptation: a running sky never jumps. However fast anastasis.Sim.Speed makes the dusk,
 * the exposure on screen moves at most MaxExposureChangePerSecond per real second, and does
 * reach its target.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisSkyClockEyeAdaptation, "Anastasis.Sky.Clock.EyeAdaptation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnastasisSkyClockEyeAdaptation::RunTest(const FString&)
{
	const UAnastasisAtmosphereProfile* Profile = UAnastasisAtmosphereProfile::CreateCodeDefaults(GetTransientPackage());
	const double Rate = Profile->MaxExposureChangePerSecond;
	const double Dt = 1.0 / 60.0;
	double EV = 14.0;
	const double Target = -1.0;
	int32 Frames = 0;
	bool bBounded = true;
	while (FMath::Abs(EV - Target) > 1e-9 && Frames < 100000)
	{
		const double Next = AnastasisSkyClock::AdaptExposure(EV, Target, Dt, Rate);
		bBounded &= FMath::Abs(Next - EV) <= Rate * Dt + 1e-12;
		EV = Next;
		++Frames;
	}
	AddInfo(FString::Printf(TEXT("ANASTASIS_SKY_ADAPT day_to_night_seconds=%.2f"), Frames * Dt));
	TestTrue(TEXT("no frame moves the exposure faster than the eye adapts"), bBounded);
	TestEqual(TEXT("the target is reached"), EV, Target, 1e-9);
	TestTrue(TEXT("a full day-to-night change takes seconds, not a frame"), Frames * Dt >= 15.0 / Rate - 1e-6);
	TestEqual(TEXT("zero elapsed time does not move it"), AnastasisSkyClock::AdaptExposure(5.0, 10.0, 0.0, Rate), 5.0, 1e-12);
	return true;
}

#endif
