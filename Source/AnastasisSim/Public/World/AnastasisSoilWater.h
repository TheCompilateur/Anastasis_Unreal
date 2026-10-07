#pragma once

#include "CoreMinimal.h"

// SOIL_WATER_BUDGET_001: a dimensionless experimental field reservoir.
// Rain in weather.js is an intensity index, not measured precipitation. These
// coefficients are game parameters; no millimetres or soil chemistry are claimed.
namespace AnastasisSoilWater
{
	struct FStep
	{
		double Before = 0.0;
		double RainIn = 0.0;
		double Evaporation = 0.0;
		double Drainage = 0.0;
		double Overflow = 0.0;
		double After = 0.0;
		bool bValid = false;
	};

	inline FStep Advance(double Stored, double RainIndex, double EvaporationIndex)
	{
		FStep Out;
		if (!FMath::IsFinite(Stored) || !FMath::IsFinite(RainIndex) || !FMath::IsFinite(EvaporationIndex)
			|| Stored < 0.0 || Stored > 1.0 || RainIndex < 0.0 || RainIndex > 1.0
			|| EvaporationIndex < 0.0 || EvaporationIndex > 1.0) return Out;
		Out.Before = Stored;
		Out.RainIn = RainIndex * 0.30;
		double Water = Stored + Out.RainIn;
		Out.Evaporation = FMath::Min(Water, EvaporationIndex * 0.06);
		Water -= Out.Evaporation;
		Out.Drainage = FMath::Min(Water, FMath::Max(0.0, Water - 0.70) * 0.25);
		Water -= Out.Drainage;
		Out.Overflow = FMath::Max(0.0, Water - 1.0);
		Out.After = Water - Out.Overflow;
		Out.bValid = true;
		return Out;
	}

	inline double GrowthFactor(double Stored)
	{
		if (!FMath::IsFinite(Stored) || Stored < 0.0 || Stored > 1.0) return 1.0;
		return FMath::Clamp(1.10 - 0.40 * FMath::Abs(Stored - 0.55) / 0.55, 0.70, 1.10);
	}
}
