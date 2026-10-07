#pragma once

#include "CoreMinimal.h"

/**
 * NOX_001: a reversible night-light experiment, not a second sky actor.
 * The atmosphere remains the sole owner of the sun, moon and exposure volume.
 * MoonFraction is an authored observation control until a lunar calendar exists.
 */
namespace AnastasisNoxLighting
{
	struct FNight
	{
		float MoonLux = 0.0f;
		double ExposureEV100 = 0.0;
	};

	inline FNight Resolve(const int32 Profile, const float FullMoonLux, const double LegacyNightEV,
		const float MoonFraction, const float CloudCover)
	{
		if (Profile <= 0)
		{
			return {FullMoonLux, LegacyNightEV};
		}
		const double Phase = FMath::Clamp(static_cast<double>(MoonFraction), 0.0, 1.0);
		const double Cover = FMath::Clamp(static_cast<double>(CloudCover), 0.0, 1.0);
		// These three presets change the amount of actual incident light and the maximum
		// night adaptation. Daylight is never graded or attenuated by NOX.
		const double CloudExtinction = Profile == 2 ? 0.97 : (Profile == 3 ? 0.85 : 0.92);
		const double VisibleMoon = Phase * (1.0 - CloudExtinction * Cover);
		const double NightOffset = Profile == 2 ? 1.0 : (Profile == 3 ? 0.5 : 0.0);
		const double DarkStopGain = Profile == 2 ? 3.5 : (Profile == 3 ? 2.0 : 3.0);
		return {static_cast<float>(FMath::Max(0.0, static_cast<double>(FullMoonLux) * VisibleMoon)),
			LegacyNightEV + NightOffset + DarkStopGain * (1.0 - VisibleMoon)};
	}

	inline double ExposureForSunElevation(const double ExistingEV, const double NoxNightEV,
		const double SunElevationDegrees)
	{
		// Preserve the already calibrated sunset curve through -8 degrees. Only the
		// deep-night plateau is changed; smooth over the last four degrees.
		const double T = FMath::Clamp((-SunElevationDegrees - 8.0) / 4.0, 0.0, 1.0);
		const double Weight = T * T * (3.0 - 2.0 * T);
		return FMath::Lerp(ExistingEV, NoxNightEV, Weight);
	}
}
