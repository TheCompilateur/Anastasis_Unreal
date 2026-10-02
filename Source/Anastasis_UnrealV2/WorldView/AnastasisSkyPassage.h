#pragma once

#include "CoreMinimal.h"

namespace AnastasisSkyPassage
{
	/** Presentation envelopes, not astronomical irradiance. Atmosphere keeps its two physical lights. */
	struct FRelay
	{
		double Sun = 1.0;
		double Moon = 0.0;
		double Twilight = 0.0;
	};

	inline double Aperture(const double Elevation, const double FullElevation)
	{
		const double T = FMath::Clamp(Elevation / FMath::Max(0.01, FullElevation), 0.0, 1.0);
		return T * T * (3.0 - 2.0 * T);
	}

	/** Both surface lights reach zero at the forward-light handover. The sky carries the interval. */
	inline FRelay Resolve(const double SunElevation, const double MoonElevation,
		const double SunFullElevation, const double MoonFullElevation)
	{
		FRelay Out;
		Out.Sun = Aperture(SunElevation, SunFullElevation);
		Out.Moon = Aperture(-SunElevation, MoonFullElevation) * Aperture(MoonElevation, MoonFullElevation);
		Out.Twilight = 1.0 - FMath::Clamp(Out.Sun + Out.Moon, 0.0, 1.0);
		return Out;
	}

	/**
	 * Dark adaptation may lag; protection from increasing light may not. EV >= target means
	 * the camera never amplifies the scene above the calibrated exposure for the current sun.
	 * This bounds exposure mismatch, NOT rendered luminance or temporal renderer caches.
	 */
	inline double Exposure(const double Current, const double Target, const double DeltaSeconds,
		const double DarkAdaptationRate)
	{
		return FMath::Max(Target, Current - FMath::Max(0.0, DarkAdaptationRate) * FMath::Clamp(DeltaSeconds, 0.0, 0.1));
	}
}
