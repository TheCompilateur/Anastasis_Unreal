#include "Sim/AnastasisTimeWarp.h"

#include "Core/AnastasisSimClock.h"
#include "HAL/PlatformTime.h"
#include "Sim/AnastasisSimulation.h"

namespace AnastasisTimeWarp
{
	double StepPreset(double Current, int32 Direction)
	{
		if (Current <= 0.0)
		{
			return 1.0;
		}
		if (Direction > 0)
		{
			for (const double P : Presets)
			{
				if (P > Current + 1e-9) return P;
			}
			return Presets[UE_ARRAY_COUNT(Presets) - 1];
		}
		for (int32 I = static_cast<int32>(UE_ARRAY_COUNT(Presets)) - 1; I >= 0; --I)
		{
			if (Presets[I] < Current - 1e-9) return Presets[I];
		}
		return Presets[0];
	}

	bool ParseAdvance(const FString& Arg, double Now, double DayLength, double& OutSeconds, FString& OutError)
	{
		OutSeconds = 0.0;
		FString S = Arg.TrimStartAndEnd().ToLower();
		if (S.IsEmpty() || DayLength <= 0.0)
		{
			OutError = TEXT("duree vide");
			return false;
		}

		if (S.StartsWith(TEXT("@")))
		{
			// Jusqu'a la prochaine heure du jour simule : "@22", "@6:30".
			const FString Clock = S.Mid(1);
			FString HourPart;
			FString MinutePart;
			if (!Clock.Split(TEXT(":"), &HourPart, &MinutePart))
			{
				HourPart = Clock;
			}
			if (!HourPart.IsNumeric() || (!MinutePart.IsEmpty() && !MinutePart.IsNumeric()))
			{
				OutError = FString::Printf(TEXT("heure illisible '%s' (attendu @HH ou @HH:MM)"), *Arg);
				return false;
			}
			const double Hours = FCString::Atod(*HourPart) + (MinutePart.IsEmpty() ? 0.0 : FCString::Atod(*MinutePart) / 60.0);
			if (Hours < 0.0 || Hours >= 24.0)
			{
				OutError = FString::Printf(TEXT("heure hors du jour '%s'"), *Arg);
				return false;
			}
			const double Current = FMath::Fmod(Now, DayLength) / DayLength;
			double Delta = Hours / 24.0 - Current;
			// Deja a cette heure (ou passee) : la prochaine, demain.
			if (Delta <= 1e-9)
			{
				Delta += 1.0;
			}
			OutSeconds = Delta * DayLength;
			return true;
		}

		double Unit = 1.0;
		if (S.EndsWith(TEXT("d")))
		{
			Unit = DayLength;
			S.LeftChopInline(1);
		}
		else if (S.EndsWith(TEXT("h")))
		{
			Unit = DayLength / 24.0;
			S.LeftChopInline(1);
		}
		else if (S.EndsWith(TEXT("s")))
		{
			S.LeftChopInline(1);
		}
		if (!S.IsNumeric())
		{
			OutError = FString::Printf(TEXT("duree illisible '%s' (attendu 45, 45s, 6h, 3d, @22)"), *Arg);
			return false;
		}
		OutSeconds = FCString::Atod(*S) * Unit;
		if (!(OutSeconds > 0.0))
		{
			OutError = FString::Printf(TEXT("duree nulle ou negative '%s'"), *Arg);
			return false;
		}
		return true;
	}

	double AdvanceStepDt()
	{
		return AnastasisSimClock::FixedDt * AnastasisSimClock::MaxStepMult;
	}

	int32 Advance(FAnastasisSimulation& Sim, double Seconds, int32 MaxSteps)
	{
		if (!Sim.IsRunning())
		{
			return 0;
		}
		const double Step = AdvanceStepDt();
		double Left = Seconds;
		int32 Steps = 0;
		while (Left > 1e-9 && Steps < MaxSteps)
		{
			const double Dt = FMath::Min(Step, Left);
			Sim.Tick(Dt);
			Left -= Dt;
			++Steps;
		}
		return Steps;
	}

	FPumpResult FWarpPump::Pump(FAnastasisSimulation& Sim, double SimWallSeconds, double Multiplier, double BudgetMs)
	{
		FPumpResult Out;
		// Le pas que la reference autorise a cette vitesse : jamais plus de 10 x FixedDt.
		const double StepDt = AnastasisSimClock::FixedDt
			* FMath::Clamp(Multiplier, 1.0, AnastasisSimClock::MaxStepMult);
		if (Multiplier > 0.0 && Sim.IsRunning())
		{
			Accumulator += FMath::Max(0.0, SimWallSeconds) * Multiplier;
			const double Start = FPlatformTime::Seconds();
			while (Accumulator >= StepDt)
			{
				// Toujours un pas au moins : une machine lente ralentit, elle ne gele pas.
				if (BudgetMs > 0.0 && Out.Steps > 0 && (FPlatformTime::Seconds() - Start) * 1000.0 >= BudgetMs)
				{
					Out.bBudgetCut = true;
					break;
				}
				Sim.Tick(StepDt);
				Accumulator -= StepDt;
				++Out.Steps;
			}
			if (Out.bBudgetCut)
			{
				// Le retard est abandonne, pas reporte : sinon la frame suivante coupe encore, et ainsi de suite.
				Accumulator = FMath::Min(Accumulator, StepDt);
			}
		}
		Out.StepAlpha = FMath::Clamp(Accumulator / StepDt, 0.0, 1.0);
		return Out;
	}

	void FWitness::Observe(double SimSeconds, double Multiplier, double DayLength)
	{
		if (!(SimSeconds > 0.0) || DayLength <= 0.0)
		{
			return;
		}
		const double IdleShare = Multiplier > 1.0 ? 1.0 - 1.0 / Multiplier : 0.0;
		const double Idle = SimSeconds * IdleShare;
		const double Active = SimSeconds - Idle;
		IdleSeconds += Idle;
		Presence *= FMath::Exp(-Idle / (FadeDays * DayLength));
		Presence = 1.0 - (1.0 - Presence) * FMath::Exp(-Active / (RecoverDays * DayLength));
		Presence = FMath::Clamp(Presence, 0.0, 1.0);
	}
}
