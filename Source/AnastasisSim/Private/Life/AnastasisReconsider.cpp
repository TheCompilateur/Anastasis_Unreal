#include "Life/AnastasisReconsider.h"

#include "Core/AnastasisJsNumeric.h"

#include <cmath>

namespace AnastasisReconsider
{
	double PersonalFrac(double DayFrac, const TOptional<AnastasisLifestyle::FLifestyle>& Lifestyle)
	{
		double Frac = DayFrac;
		if (!Lifestyle.IsSet())
		{
			return Frac;
		}
		// `(frac + 0.04) % 1` : le `%` de JS est le `fmod` de C sur des doubles finis.
		if (Lifestyle->Id == TEXT("earlyBird")) Frac = std::fmod(Frac + 0.04, 1.0);
		if (Lifestyle->Id == TEXT("nightOwl")) Frac = std::fmod(Frac + 0.96, 1.0);
		return Frac;
	}

	AnastasisRhythm::EPhase PersonalPhase(double DayFrac, const TOptional<AnastasisLifestyle::FLifestyle>& Lifestyle)
	{
		return AnastasisRhythm::VillagePhase(PersonalFrac(DayFrac, Lifestyle));
	}

	double NeedsReconsiderChance(bool bCritical, const FString& Goal, double Dt)
	{
		if (!bCritical) return Dt * 0.18;
		if (AnastasisRhythm::IsWorkGoal(Goal) || Goal == TEXT("explore")) return Dt * 1.35;
		return Dt * 0.35;
	}

	double PhaseReconsiderChance(AnastasisRhythm::EPhase PersonalPhaseNow, double Dt, double BaseChance)
	{
		using P = AnastasisRhythm::EPhase;
		if (PersonalPhaseNow == P::Midday || PersonalPhaseNow == P::Evening
			|| PersonalPhaseNow == P::Night || PersonalPhaseNow == P::Dawn)
		{
			return FMath::Max(BaseChance, Dt * 0.55);
		}
		return BaseChance;
	}

	double CommittedReconsiderChance(const FCommitSubject& Npc, double Now, double BaseChance)
	{
		if (!FMath::IsFinite(BaseChance)) return BaseChance;
		if (Npc.bCritical) return BaseChance;
		if (!Npc.bHasTarget || Npc.Goal.IsEmpty() || Npc.Goal == TEXT("observer")) return BaseChance;
		const bool bPhaseFlip = Npc.PhaseChangedAt.IsSet() && (Now - Npc.PhaseChangedAt.GetValue()) < PhaseAdaptSeconds;
		if (bPhaseFlip) return BaseChance;
		const double Age = Now - Npc.GoalSince;
		if (Age >= 0.0 && Age <= CommitSeconds)
		{
			return BaseChance * CommitReconsiderScale;
		}
		// LOT C : l'amortissement s'etend a tout le plancher du quart.
		if (Npc.bShiftShields)
		{
			return BaseChance * CommitReconsiderScale;
		}
		return BaseChance;
	}

	bool IsCraftGoal(const FString& Goal)
	{
		return Goal == TEXT("gatherWood") || Goal == TEXT("gatherFood") || Goal == TEXT("helpFarm")
			|| Goal == TEXT("gatherStone") || Goal == TEXT("build") || Goal == TEXT("craft") || Goal == TEXT("maintain");
	}

	bool IsCriticalReliefGoal(const AnastasisNeeds::FNeeds& Needs, int32 InventoryFood, const FString& Goal)
	{
		namespace C = AnastasisNeeds::Constants;
		const bool bNoRation = InventoryFood <= 0;
		if (Needs.Hunger >= C::HungerCritical)
		{
			if (Goal == TEXT("eat") || Goal == TEXT("eatTogether")) return true;
			if (bNoRation && (Goal == TEXT("buy") || Goal == TEXT("gatherFood"))) return true;
		}
		if (Needs.Energy <= 100.0 - C::FatigueCritical && Goal == TEXT("rest")) return true;
		if (Needs.Social <= 100.0 - C::LonelyCritical
			&& (Goal == TEXT("socialize") || Goal == TEXT("visitFamily") || Goal == TEXT("play"))) return true;
		if (Needs.Leisure <= 100.0 - C::BoredCritical && (Goal == TEXT("relax") || Goal == TEXT("play"))) return true;
		if (Needs.Hygiene <= 100.0 - C::HygieneCritical && Goal == TEXT("relieve")) return true;
		if (Needs.Thirst >= C::ThirstCritical && Goal == TEXT("drink")) return true;
		if (Needs.Health <= C::HealthCritical)
		{
			if (Goal == TEXT("eat") || Goal == TEXT("eatTogether") || Goal == TEXT("rest") || Goal == TEXT("drink")) return true;
			if (bNoRation && (Goal == TEXT("buy") || Goal == TEXT("gatherFood"))) return true;
		}
		// `(npc.morale ?? 50) < moraleCritical` : compagnie ou souffler cassent le collant du travail.
		if (Needs.Morale < C::MoraleCritical
			&& (Goal == TEXT("socialize") || Goal == TEXT("visitFamily") || Goal == TEXT("relax"))) return true;
		return false;
	}

	double TraitStickinessScale(double Build, double Trade, double Gather, double Explore, const FString& Goal)
	{
		if (Goal.IsEmpty()) return 1.0;
		double Affinity = 1.0;
		if (Goal == TEXT("explore")) Affinity = Explore;
		else if (Goal == TEXT("build") || Goal == TEXT("craft") || Goal == TEXT("maintain")) Affinity = Build;
		else if (Goal == TEXT("sell") || Goal == TEXT("buy") || Goal == TEXT("deliver") || Goal == TEXT("haulJob")) Affinity = Trade;
		else if (Goal.StartsWith(TEXT("gather"), ESearchCase::CaseSensitive) || Goal == TEXT("helpFarm")) Affinity = Gather;
		// `Number(t.trade) || 1` : un trade nul vaut 1.
		else if (Goal == TEXT("socialize")) Affinity = 0.85 + (Trade != 0.0 && !FMath::IsNaN(Trade) ? Trade : 1.0) * 0.15;
		else return 1.0;
		return FMath::Clamp(0.55 + Affinity * 0.45, TraitStickMin, TraitStickMax);
	}

	double GoalStickinessBonus(const FStickSubject& Npc, double Now, const FString& Goal)
	{
		if (Npc.CurrentGoal.IsEmpty() || Npc.CurrentGoal == TEXT("observer") || Goal != Npc.CurrentGoal) return 0.0;
		if (Npc.bCritical && !Npc.bCurrentRelievesCritical) return 0.0;
		const bool bPhaseFlip = Npc.PhaseChangedAt.IsSet() && (Now - Npc.PhaseChangedAt.GetValue()) < PhaseAdaptSeconds;
		double Bonus = Stickiness * (bPhaseFlip ? PhaseStickinessScale : 1.0);
		Bonus *= TraitStickinessScale(Npc.TraitBuild, Npc.TraitTrade, Npc.TraitGather, Npc.TraitExplore, Goal);
		Bonus *= 1.0 + Npc.NatureStick;
		if (IsCraftGoal(Goal) && Npc.bWorkSession) Bonus += CraftSessionBonus;
		// `(GOAL_AI.haulCompletion || 20) * acquis` : haulCompletion = 28.
		if (IsCraftGoal(Goal) && Npc.CraftBatchCommitment > 0.0) Bonus += 28.0 * Npc.CraftBatchCommitment;
		if ((Goal == TEXT("deliver") || Goal == TEXT("sell") || Goal == TEXT("haulJob")) && Npc.InventoryLoad > 0)
		{
			Bonus += LoadedBonus;
		}
		return Bonus;
	}
}
