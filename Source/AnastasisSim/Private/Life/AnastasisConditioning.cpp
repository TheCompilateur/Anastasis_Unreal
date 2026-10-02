#include "Life/AnastasisConditioning.h"

#include "Core/AnastasisSimMath.h"

namespace AnastasisConditioning
{
	// `clamp01` de util.js : `Math.max(0, Math.min(1, v))`.
	using AnastasisMath::Clamp01;

	void TickConditioning(
		FConditioning& C,
		double Dt,
		bool bWorking,
		bool bResting,
		bool bOverworked,
		bool bFatigued)
	{
		// C1 — workConditioning : pratique generale du metier.
		if (bWorking && !bOverworked)
		{
			C.WorkConditioning = Clamp01(C.WorkConditioning + Dt * ConditioningRate * (1.0 - C.WorkConditioning));
		}
		else if (bResting)
		{
			C.WorkConditioning = Clamp01(C.WorkConditioning - Dt * ConditioningRate * C.WorkConditioning);
		}

		// C2 — fatigueAdaptation : il faut travailler DEJA fatigue pour s'adapter.
		const double Fa = C.FatigueAdaptation;
		if (bWorking && bFatigued && !bOverworked)
		{
			C.FatigueAdaptation = Clamp01(Fa + Dt * ConditioningRate * (1.0 - Fa));
		}
		else if (bResting)
		{
			C.FatigueAdaptation = Clamp01(Fa - Dt * ConditioningRate * Fa);
		}

		// C3 — recoveryConditioning : decline a l'INVERSE des deux autres.
		const double Rc = C.RecoveryConditioning;
		if (bResting)
		{
			C.RecoveryConditioning = Clamp01(Rc + Dt * ConditioningRate * (1.0 - Rc));
		}
		else if (bWorking)
		{
			C.RecoveryConditioning = Clamp01(Rc - Dt * ConditioningRate * Rc);
		}
	}

	double FatigueAdaptationEnergyFallMultiplier(double Value)
	{
		const double V = Clamp01(Value);
		return 1.0 - (V - 0.5) * ConditioningEffectAmplitude;
	}

	double RecoveryConditioningEnergyGainMultiplier(double Value)
	{
		const double V = Clamp01(Value);
		return 1.0 + (V - 0.5) * ConditioningEffectAmplitude;
	}
}
