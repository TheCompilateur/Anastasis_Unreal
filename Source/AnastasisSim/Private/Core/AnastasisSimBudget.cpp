#include "Core/AnastasisSimBudget.h"

#include "Core/AnastasisSimMath.h"

namespace AnastasisBudget
{
	ETier PressureTier(double Pressure)
	{
		const double P = AnastasisMath::Clamp01Coerce(Pressure);
		if (P >= SeverePressure) return ETier::Severe;
		if (P >= LeanPressure) return ETier::Lean;
		return ETier::Normal;
	}

	FMultipliers Multipliers(double Pressure)
	{
		const double P = AnastasisMath::Clamp01Coerce(Pressure);
		FMultipliers Out;
		Out.PathBudgetMul = P >= SeverePressure ? 0.42 : (P >= LeanPressure ? 0.68 : 1.0);
		Out.AnimalTickMul = P >= SeverePressure ? 0.35 : (P >= LeanPressure ? 0.6 : 1.0);
		Out.TransportTickMul = P >= SeverePressure ? 0.55 : (P >= LeanPressure ? 0.75 : 1.0);
		return Out;
	}

	EBand ClassifyNpcBand(const FDirector& Director, const FNpcView& Npc)
	{
		if (Npc.bInside)
		{
			return EBand::Medium;
		}

		// Le JS teste `Number.isFinite` sur la vue et retombe sur `near` si elle
		// n'est pas exploitable. Une vue NaN arrive quand rien ne l'a encore
		// posee: tout le monde est alors proche, ce qui est le comportement sur.
		if (!FMath::IsFinite(Director.ViewX) || !FMath::IsFinite(Director.ViewY))
		{
			return EBand::Near;
		}

		// `Math.hypot`, pas `sqrt(dx*dx+dy*dy)`: au bord d'un rayon, l'ecart des
		// deux formules fait changer un PNJ de bande.
		const double D = AnastasisMath::JsHypot(Npc.X - Director.ViewX, Npc.Y - Director.ViewY);
		const double P = AnastasisMath::Clamp01Coerce(Director.Pressure);

		const double Near = Cadence::NearRadius * (1.0 - P * 0.28);
		const double Medium = Cadence::MediumRadius * (1.0 - P * 0.24);
		const double Far = Cadence::FarRadius * (1.0 - P * 0.18);

		if (D <= Near) return EBand::Near;
		if (D <= Medium) return EBand::Medium;
		if (D <= Far) return EBand::Far;
		return EBand::Invisible;
	}

	double IntervalForBand(double Pressure, EBand Band)
	{
		const double P = AnastasisMath::Clamp01Coerce(Pressure);
		switch (Band)
		{
		case EBand::Near:
			return 1.0 / Cadence::NearHz;
		case EBand::Medium:
			return (1.0 / Cadence::MediumHz) * (1.0 + P * 1.8);
		case EBand::Far:
			return (1.0 / Cadence::FarHz) * (1.0 + P * 3.0);
		default:
			return (1.0 / Cadence::InvisibleHz) * (1.0 + P * 2.0);
		}
	}

	FCadenceStep ConsumeCadence(
		const FDirector& Director,
		const FNpcView& Npc,
		double Dt,
		double& Accumulator,
		bool bCritical)
	{
		FCadenceStep Step;
		if (bCritical)
		{
			Step.bRun = true;
			Step.Dt = Dt;
			Step.Band = EBand::Near;
			return Step;
		}

		const EBand Band = ClassifyNpcBand(Director, Npc);
		Step.Band = Band;

		if (Band == EBand::Near)
		{
			Step.bRun = true;
			Step.Dt = Dt;
			return Step;
		}

		// `(Number(dt) || 0)` du JS: NaN et 0 retombent tous deux sur 0, d'ou
		// `NumberOr`-comme-zero plutot qu'un simple max.
		const double Ajout = FMath::Max(0.0, FMath::IsFinite(Dt) ? Dt : 0.0);
		Accumulator = (FMath::IsFinite(Accumulator) ? Accumulator : 0.0) + Ajout;

		const double Interval = IntervalForBand(Director.Pressure, Band);
		if (Accumulator + 1e-6 < Interval)
		{
			Step.bRun = false;
			Step.Dt = 0.0;
			return Step;
		}

		// Plafond du rattrapage: un PNJ invisible peut avaler jusqu'a six
		// secondes d'un coup, les autres deux et demie. Sans ce plafond, un
		// habitant longtemps hors vue rattraperait son retard en une seule
		// enjambee.
		const double Plafond = Band == EBand::Invisible ? 6.0 : 2.5;
		Step.Dt = FMath::Min(Accumulator, Plafond);
		Accumulator = 0.0;
		Step.bRun = true;
		Step.bSymbolic = Band == EBand::Invisible;
		return Step;
	}

	const TCHAR* TierName(ETier Tier)
	{
		switch (Tier)
		{
		case ETier::Severe: return TEXT("severe");
		case ETier::Lean: return TEXT("lean");
		default: return TEXT("normal");
		}
	}

	const TCHAR* BandName(EBand Band)
	{
		switch (Band)
		{
		case EBand::Near: return TEXT("near");
		case EBand::Medium: return TEXT("medium");
		case EBand::Far: return TEXT("far");
		default: return TEXT("invisible");
		}
	}

	EBand BandFromName(const FString& Name)
	{
		if (Name == TEXT("near")) return EBand::Near;
		if (Name == TEXT("medium")) return EBand::Medium;
		if (Name == TEXT("far")) return EBand::Far;
		return EBand::Invisible;
	}
}
