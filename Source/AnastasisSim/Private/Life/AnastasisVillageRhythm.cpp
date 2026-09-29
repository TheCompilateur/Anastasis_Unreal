#include "Life/AnastasisVillageRhythm.h"

namespace AnastasisRhythm
{
	namespace
	{
		// `PHASE_HOUR_START`.
		constexpr double DawnStart = 5.0;
		constexpr double MorningStart = 7.0;
		constexpr double MiddayStart = 11.5;
		constexpr double AfternoonStart = 13.5;
		constexpr double EveningStart = 17.5;
		constexpr double NightStart = 21.0;

		bool InList(const FString& Goal, std::initializer_list<const TCHAR*> List)
		{
			for (const TCHAR* Item : List)
			{
				if (Goal == Item) return true;
			}
			return false;
		}

		/** `PHASES[id].focus`. */
		bool IsFocus(EPhase Phase, const FString& Goal)
		{
			switch (Phase)
			{
			case EPhase::Night: return InList(Goal, { TEXT("rest") });
			case EPhase::Dawn: return InList(Goal, { TEXT("rest"), TEXT("eat"), TEXT("drink"), TEXT("relieve") });
			case EPhase::Morning:
			case EPhase::Afternoon:
				return InList(Goal, { TEXT("gatherWood"), TEXT("gatherStone"), TEXT("gatherFood"), TEXT("build"),
					TEXT("craft"), TEXT("deliver"), TEXT("sell"), TEXT("buy"), TEXT("maintain") });
			case EPhase::Midday: return InList(Goal, { TEXT("eat"), TEXT("drink"), TEXT("relax"), TEXT("socialize") });
			case EPhase::Evening: return InList(Goal, { TEXT("socialize"), TEXT("visitFamily"), TEXT("relax"), TEXT("eat"), TEXT("relieve") });
			}
			return false;
		}

		/** `npc?.energy || 100` : 0 est falsy, un habitant a energie nulle compte 100. */
		double EnergyOr100(double Energy)
		{
			return Energy != 0.0 && !FMath::IsNaN(Energy) ? Energy : 100.0;
		}
	}

	const TCHAR* PhaseId(EPhase Phase)
	{
		switch (Phase)
		{
		case EPhase::Night: return TEXT("night");
		case EPhase::Dawn: return TEXT("dawn");
		case EPhase::Morning: return TEXT("morning");
		case EPhase::Midday: return TEXT("midday");
		case EPhase::Afternoon: return TEXT("afternoon");
		case EPhase::Evening: return TEXT("evening");
		}
		return TEXT("night");
	}

	double PhaseWork(EPhase Phase)
	{
		switch (Phase)
		{
		case EPhase::Night: return 0.08;
		case EPhase::Dawn: return 0.45;
		case EPhase::Morning: return 1.05;
		case EPhase::Midday: return 0.28;
		case EPhase::Afternoon: return 1.0;
		case EPhase::Evening: return 0.22;
		}
		return 1.0;
	}

	double DayFracOf(double Time)
	{
		// `%` de JavaScript sur des doubles = fmod (signe du dividende).
		return FMath::Fmod(Time, DayLength) / DayLength;
	}

	EPhase VillagePhase(double Frac)
	{
		const double Hour = FMath::Fmod(FMath::Fmod(Frac, 1.0) + 1.0, 1.0) * 24.0;
		if (Hour >= NightStart || Hour < DawnStart) return EPhase::Night;
		if (Hour < MorningStart) return EPhase::Dawn;
		if (Hour < MiddayStart) return EPhase::Morning;
		if (Hour < AfternoonStart) return EPhase::Midday;
		if (Hour < EveningStart) return EPhase::Afternoon;
		return EPhase::Evening;
	}

	bool IsNightPhase(double Time)
	{
		return VillagePhase(DayFracOf(Time)) == EPhase::Night;
	}

	bool IsWorkGoal(const FString& Goal)
	{
		return InList(Goal, { TEXT("gatherWood"), TEXT("gatherStone"), TEXT("gatherFood"), TEXT("build"), TEXT("craft"),
			TEXT("maintain"), TEXT("deliver"), TEXT("sell"), TEXT("buy"), TEXT("helpFarm"), TEXT("apprentice"), TEXT("study") });
	}

	bool IsLifeGoal(const FString& Goal)
	{
		return InList(Goal, { TEXT("eat"), TEXT("eatTogether"), TEXT("drink"), TEXT("rest"), TEXT("relax"),
			TEXT("relieve"), TEXT("socialize"), TEXT("visitFamily"), TEXT("play") });
	}

	double PhaseBias(EPhase Phase, const FPhaseSubject& S, const FString& Goal)
	{
		const double Work = PhaseWork(Phase);
		const bool bWork = IsWorkGoal(Goal);
		double Score = 0.0;

		if (IsFocus(Phase, Goal)) Score += 26.0;
		if (bWork) Score += (Work - 0.55) * 36.0;
		if (IsLifeGoal(Goal) && Work < 0.5) Score += (0.55 - Work) * 28.0;

		if (Phase == EPhase::Night)
		{
			if (S.bGuard)
			{
				if (Goal == TEXT("maintain")) Score += 40.0;
				if (Goal == TEXT("rest")) Score -= 28.0;
				if (Goal == TEXT("socialize") || Goal == TEXT("relax") || Goal == TEXT("explore")) Score -= 20.0;
			}
			else
			{
				if (Goal == TEXT("rest")) Score += S.bHasHomeOrShelter ? 52.0 : 30.0;
				if (Goal == TEXT("relieve")) Score += S.bHasHomeOrShelter ? 22.0 : 8.0;
				if (Goal == TEXT("eat") && S.Hunger > 40.0) Score += 12.0;
				if (bWork || Goal == TEXT("explore")) Score -= 42.0;
				if (Goal == TEXT("socialize") || Goal == TEXT("relax")) Score -= 28.0;
				if (S.bNocturnalWanderer)
				{
					if (Goal == TEXT("explore")) Score += 58.0;
					if (Goal == TEXT("confront")) Score += 48.0;
					if (Goal == TEXT("socialize")) Score += 14.0;
					if (Goal == TEXT("rest")) Score -= 30.0;
				}
			}
		}

		if (Phase == EPhase::Dawn)
		{
			if (Goal == TEXT("rest")) Score += 10.0;
			if (Goal == TEXT("eat")) Score += 18.0;
			if (Goal == TEXT("drink")) Score += 16.0;
			if (Goal == TEXT("relieve")) Score += 14.0;
			if (Goal == TEXT("explore")) Score -= 8.0;
		}

		if (Phase == EPhase::Morning)
		{
			if (bWork && !S.bGuard) Score += 18.0;
			if (Goal == TEXT("rest") && EnergyOr100(S.Energy) > 45.0) Score -= 16.0;
			if (Goal == TEXT("socialize") || Goal == TEXT("relax")) Score -= 8.0;
		}

		if (Phase == EPhase::Midday)
		{
			if (Goal == TEXT("eat")) Score += 42.0;
			if (Goal == TEXT("drink")) Score += 36.0;
			if (Goal == TEXT("relieve")) Score += 18.0;
			if (Goal == TEXT("relax")) Score += 24.0;
			if (Goal == TEXT("socialize")) Score += 16.0;
			if (Goal == TEXT("play") && S.bYoung) Score += 20.0;
			if (bWork) Score -= 22.0;
		}

		if (Phase == EPhase::Afternoon)
		{
			if (bWork) Score += 14.0;
			if (Goal == TEXT("eat") && S.Hunger < 35.0) Score -= 10.0;
		}

		if (Phase == EPhase::Evening)
		{
			if (Goal == TEXT("socialize")) Score += 34.0;
			if (Goal == TEXT("visitFamily")) Score += S.bHasFamily ? 30.0 : 8.0;
			if (Goal == TEXT("relax")) Score += 22.0;
			if (Goal == TEXT("eat")) Score += 12.0;
			if (Goal == TEXT("play") && S.bYoung) Score += 14.0;
			if (Goal == TEXT("rest") && EnergyOr100(S.Energy) < 40.0) Score += 10.0;
			if (bWork && !S.bEveningTrade) Score -= 24.0;
			if (Goal == TEXT("explore")) Score -= 10.0;
		}

		return Score;
	}
}
