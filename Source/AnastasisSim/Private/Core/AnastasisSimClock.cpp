#include "Core/AnastasisSimClock.h"

namespace AnastasisSimClock
{
	FStepPlan StepPlan(double Scale, double InFixedDt)
	{
		// `Math.max(1, Number(scale) || 1)`: 0 et NaN retombent sur 1, pas sur 0.
		const double S = FMath::Max(1.0, AnastasisJs::NumberOr(Scale, 1.0));
		const double Dt = FMath::Max(1e-6, AnastasisJs::NumberOr(InFixedDt, FixedDt));

		if (S <= 1.0)
		{
			return FStepPlan{ Dt, 1, Dt, 1.0 };
		}

		// Fat step pour 2x/5x/10x: 1 tick CPU, temps sim xN.
		// Evite le piege "2 ticks x FixedDt" ou le budget mur nivelle tout a 1x.
		const double StepMult = FMath::Min(S, MaxStepMult);
		const double StepDt = Dt * StepMult;
		const double Wanted = (Dt * S) / StepDt;
		// Math.round de JS = arrondi au plus proche, moitie vers +Infini.
		// Wanted est toujours positif ici, donc FMath::RoundToDouble (moitie
		// loin de zero) coincide exactement.
		const int32 TargetSteps = FMath::Min(2, FMath::Max(1,
			static_cast<int32>(FMath::RoundToDouble(Wanted))));
		const double TimePerFrame = StepDt * static_cast<double>(TargetSteps);

		return FStepPlan{ StepDt, TargetSteps, TimePerFrame, TimePerFrame / Dt };
	}

	double WallBudgetMs(double Scale, double WallFrameMs, bool bWorldBuilding)
	{
		const FStepPlan Plan = StepPlan(Scale);

		double Ms = FMath::Min(
			WallBudgetMsCap,
			static_cast<double>(Plan.TargetSteps) * WallTickCostMs + 2.0);

		// Plafond dur a 45% d'une frame 60 Hz: la sim ne mange jamais la moitie
		// du budget d'affichage, meme en fat step.
		Ms = FMath::Min(Ms, (1000.0 / 60.0) * 0.45);

		if (AnastasisJs::NumberOrZero(WallFrameMs) > WallHeavyFrameMs)
		{
			Ms *= 0.65;
		}
		if (bWorldBuilding)
		{
			Ms *= 0.45;
		}
		return FMath::Max(4.0, Ms);
	}

	int32 MinStepsBeforeBudget(double /*Scale*/)
	{
		// 1 tick garanti (le plan nominal est deja 1 fat step a 2x/5x/10x).
		return 1;
	}

	double KeepLagSeconds(double Scale, double InFixedDt)
	{
		return StepPlan(Scale, InFixedDt).TimePerFrame;
	}

	FFrameDelta FrameDelta(double WallMs, double MaxDt, double MinWallMs, double MaxWallMs)
	{
		FFrameDelta Out;
		Out.WallMs = WallMs;
		Out.LastWallFrameMs = FMath::Min(FMath::Max(WallMs, MinWallMs), MaxWallMs);
		Out.Dt = FMath::Min(WallMs / 1000.0, MaxDt);
		return Out;
	}
}
