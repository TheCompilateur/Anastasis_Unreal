#include "Life/AnastasisNeeds.h"

#include "Core/AnastasisSimMath.h"

namespace AnastasisNeeds
{
	namespace
	{
		/** `(npc.morale || 50)`. */
		double MoraleOr50(double Morale)
		{
			return Morale != 0.0 && !FMath::IsNaN(Morale) ? Morale : 50.0;
		}
	}

	using namespace Constants;
	using AnastasisMath::Clamp;

	double UrgeScore(double Pressure, double UrgeAt, double CriticalAt)
	{
		if (Pressure <= 0.0) return 0.0;
		if (Pressure < UrgeAt * 0.55) return Pressure * 0.25;
		if (Pressure < UrgeAt) return 8.0 + Pressure * 0.55;
		if (Pressure < CriticalAt) return 42.0 + (Pressure - UrgeAt) * 2.15;
		return 105.0 + (Pressure - CriticalAt) * 2.8;
	}

	FNeedGoalScores NeedGoalScores(const FNeeds& Needs, int32 CompletedWells, int32 CompletedTaverns)
	{
		const double Fatigue = 100.0 - Needs.Energy;
		const double Lonely = 100.0 - Needs.Social;
		const double Bored = 100.0 - Needs.Leisure;

		FNeedGoalScores S;
		S.Eat = UrgeScore(Needs.Hunger, HungerUrge, HungerCritical);
		S.Rest = UrgeScore(Fatigue, FatigueUrge, FatigueCritical);
		if (Needs.Health <= HealthUrge)
		{
			S.Eat += (HealthUrge - Needs.Health) * 1.1;
			S.Rest += (HealthUrge - Needs.Health) * 0.55;
		}
		if (Needs.Health <= HealthCritical)
		{
			S.Eat += 48.0;
			S.Rest += 28.0;
		}

		S.Socialize = UrgeScore(Lonely, LonelyUrge, LonelyCritical) + (CompletedTaverns > 0 ? 6.0 : 0.0);
		S.Relax = UrgeScore(Bored, BoredUrge, BoredCritical);
		const double Filthy = 100.0 - Needs.Hygiene;
		S.Relieve = UrgeScore(Filthy, HygieneUrge, HygieneCritical);
		S.Drink = UrgeScore(Needs.Thirst, ThirstUrge, ThirstCritical);
		// « Un puits construit rend la soif plus tractable (on sait ou aller). »
		if (CompletedWells > 0) S.Drink += 4.0;
		if (Needs.Health <= HealthUrge) S.Drink += (HealthUrge - Needs.Health) * 1.1;
		if (Needs.Health <= HealthCritical) S.Drink += 48.0;

		const double Morale = Needs.Morale; // `npc.morale ?? 50` : jamais nul ici.
		if (Morale < MoraleUrge)
		{
			const double Despair = MoraleUrge - Morale;
			S.Socialize += 14.0 + Despair * 2.4;
			S.Relax += 10.0 + Despair * 1.7;
		}
		if (Morale < MoraleCritical)
		{
			S.Socialize += 44.0;
			S.Relax += 32.0;
		}

		// Preseance vitale : place APRES le bonus de desespoir, comme la reference.
		if (Needs.Thirst >= ParchedAt)
		{
			const double Unpaid = FMath::Max(FMath::Max(S.Rest, S.Socialize), FMath::Max(S.Relax, S.Relieve));
			if (S.Drink <= Unpaid) S.Drink = Unpaid + LethalThirstMargin;
		}
		return S;
	}

	void TickNeeds(FNeeds& N, double Dt, bool bDrinking, bool bWorking)
	{
		// Genome et conditionnement neutres : voir l'en-tete.
		constexpr double HydrationMul = 1.0;
		constexpr double MetabolicMul = 1.0;
		constexpr double FatigueAdaptationMul = 1.0;

		if (bDrinking)
		{
			N.Thirst = Clamp(N.Thirst - Dt * DrinkingThirstFall, 0.0, 100.0);
			N.Hunger = Clamp(N.Hunger + Dt * HungerRise * 0.35 * MetabolicMul, 0.0, 100.0);
			N.Energy = Clamp(N.Energy - Dt * EnergyFall * 0.15 * MetabolicMul * FatigueAdaptationMul, 0.0, 100.0);
		}
		else
		{
			N.Hunger = Clamp(N.Hunger + Dt * HungerRise * MetabolicMul, 0.0, 100.0);
			N.Thirst = Clamp(N.Thirst + Dt * (ThirstRise + (bWorking ? 0.12 : 0.0)) * HydrationMul, 0.0, 100.0);
			N.Energy = Clamp(N.Energy - Dt * EnergyFall * MetabolicMul * FatigueAdaptationMul, 0.0, 100.0);
			N.Social = Clamp(N.Social - Dt * (SocialFall + (bWorking ? WorkSocialExtra : 0.0)), 0.0, 100.0);
			N.Leisure = Clamp(N.Leisure - Dt * (LeisureFall + (bWorking ? WorkLeisureExtra : 0.0)), 0.0, 100.0);
			N.Hygiene = Clamp(N.Hygiene - Dt * HygieneFall, 0.0, 100.0);
		}

		TickVitality(N, Dt);
	}

	void TickNeedsRestInside(FNeeds& N, double Dt, bool bNight, double SleepQuality)
	{
		constexpr double HydrationMul = 1.0;
		constexpr double MetabolicMul = 1.0;
		constexpr double FatigueRecoveryMul = 1.0;
		constexpr double RecoveryConditioningMul = 1.0;

		const double Rate = bNight ? SleepEnergyGain : NapEnergyGain;
		N.Energy = Clamp(N.Energy + Dt * Rate * FatigueRecoveryMul * RecoveryConditioningMul, 0.0, 100.0);
		N.Hunger = Clamp(N.Hunger + Dt * HungerRise * 0.35 * MetabolicMul, 0.0, 100.0);
		N.Thirst = Clamp(N.Thirst + Dt * ThirstRise * 0.4 * HydrationMul, 0.0, 100.0);
		N.Social = Clamp(N.Social - Dt * SocialFall * 0.25, 0.0, 100.0);
		N.Leisure = Clamp(N.Leisure + Dt * 1.1, 0.0, 100.0);
		N.Hygiene = Clamp(N.Hygiene - Dt * HygieneFall * 0.2, 0.0, 100.0);
		if (N.Hunger < StarvingAt && N.Thirst < ParchedAt)
		{
			N.Health = Clamp(N.Health + Dt * HealthSleepRestore * SleepQuality, 0.0, 100.0);
		}

		TickVitality(N, Dt);
	}

	void TickNeedsEatInside(FNeeds& N, double Dt)
	{
		constexpr double HydrationMul = 1.0;
		constexpr double MetabolicMul = 1.0;
		constexpr double FatigueAdaptationMul = 1.0;
		N.Hunger = Clamp(N.Hunger - Dt * EatHungerFall, 0.0, 100.0);
		N.Thirst = Clamp(N.Thirst + Dt * ThirstRise * 0.3 * HydrationMul, 0.0, 100.0);
		N.Energy = Clamp(N.Energy - Dt * EnergyFall * 0.25 * MetabolicMul * FatigueAdaptationMul, 0.0, 100.0);
		TickVitality(N, Dt);
	}

	void TickNeedsSocialize(FNeeds& N, double Dt, bool bInsideGoal)
	{
		constexpr double HydrationMul = 1.0;
		constexpr double MetabolicMul = 1.0;
		constexpr double FatigueAdaptationMul = 1.0;
		N.Social = Clamp(N.Social + Dt * TalkSocialGain * (bInsideGoal ? 1.0 : 0.45), 0.0, 100.0);
		N.Hunger = Clamp(N.Hunger + Dt * HungerRise * 0.7 * MetabolicMul, 0.0, 100.0);
		N.Thirst = Clamp(N.Thirst + Dt * ThirstRise * 0.75 * HydrationMul, 0.0, 100.0);
		N.Energy = Clamp(N.Energy - Dt * EnergyFall * 0.55 * MetabolicMul * FatigueAdaptationMul, 0.0, 100.0);
		N.Leisure = Clamp(N.Leisure - Dt * LeisureFall * 0.4, 0.0, 100.0);
		TickVitality(N, Dt);
	}

	void TickNeedsRelax(FNeeds& N, double Dt)
	{
		constexpr double HydrationMul = 1.0;
		constexpr double MetabolicMul = 1.0;
		N.Leisure = Clamp(N.Leisure + Dt * RelaxLeisureGain, 0.0, 100.0);
		N.Energy = Clamp(N.Energy + Dt * RelaxEnergyGain, 0.0, 100.0);
		N.Hunger = Clamp(N.Hunger + Dt * HungerRise * 0.55 * MetabolicMul, 0.0, 100.0);
		N.Thirst = Clamp(N.Thirst + Dt * ThirstRise * 0.55 * HydrationMul, 0.0, 100.0);
		N.Social = Clamp(N.Social - Dt * SocialFall * 0.35, 0.0, 100.0);
		TickVitality(N, Dt);
	}

	void SatisfySocial(FNeeds& N, double Amount, bool bIndoor)
	{
		const double Gain = bIndoor ? Amount * 0.45 : Amount;
		N.Social = Clamp(N.Social + Gain, 0.0, 100.0);
		N.Morale = Clamp(MoraleOr50(N.Morale) + SocialMorale, 0.0, 100.0);
		N.Leisure = Clamp(N.Leisure + (bIndoor ? 3.0 : 6.0), 0.0, 100.0);
	}

	void SatisfyRelax(FNeeds& N, bool bIndoor)
	{
		if (bIndoor)
		{
			constexpr double Target = 78.0;
			if (N.Leisure < Target)
			{
				N.Leisure = Clamp(N.Leisure + (Target - N.Leisure) * 0.55, 0.0, 100.0);
			}
			N.Energy = Clamp(N.Energy + 6.0, 0.0, 100.0);
		}
		else
		{
			N.Leisure = Clamp(N.Leisure + LeisureRelief, 0.0, 100.0);
			N.Energy = Clamp(N.Energy + 12.0, 0.0, 100.0);
		}
		N.Morale = Clamp(MoraleOr50(N.Morale) + LeisureMorale, 0.0, 100.0);
	}

	void TickVitality(FNeeds& N, double Dt)
	{
		const bool bStarving = N.Hunger >= StarvingAt;
		const bool bParched = N.Thirst >= ParchedAt;
		const bool bExhausted = N.Energy <= ExhaustedAt;
		if (bStarving)
		{
			N.Health = Clamp(N.Health - Dt * HealthLossStarving, 0.0, 100.0);
		}
		else if (bParched)
		{
			N.Health = Clamp(N.Health - Dt * HealthLossParched, 0.0, 100.0);
		}
		else if (bExhausted)
		{
			N.Health = Clamp(N.Health - Dt * HealthLossExhausted, 0.0, 100.0);
		}
		else if (N.Hunger < 50.0 && N.Thirst < 50.0 && N.Energy > 35.0 && N.Health < 100.0)
		{
			N.Health = Clamp(N.Health + Dt * HealthGain, 0.0, 100.0);
		}
	}

	double SleepQuality(const FString& InsideBuildingId, const FString& HomeId, const FString& ShelterId)
	{
		if (!InsideBuildingId.IsEmpty())
		{
			if (!HomeId.IsEmpty() && HomeId == InsideBuildingId) return Domestic::HomeRestBonus;
			if (!ShelterId.IsEmpty() && ShelterId == InsideBuildingId) return Domestic::ShelterRestFactor;
		}
		// Les trois replis de la reference valent tous outdoorRestFactor.
		return Domestic::OutdoorRestFactor;
	}

	void SatisfyRest(FNeeds& N, bool bNight, double Quality, bool bIndoor, bool bAtHome)
	{
		// Interieur : l'energie est deja remontee par tickNeeds ; on plafonne vers
		// une cible selon la qualite du lit, sans re-ajouter sleepRelief.
		if (bIndoor)
		{
			const double Target = bNight
				? Clamp(58.0 + 38.0 * Quality, 0.0, 100.0)
				: Clamp(42.0 + 28.0 * Quality, 0.0, 100.0);
			if (N.Energy < Target)
			{
				N.Energy = Clamp(N.Energy + (Target - N.Energy) * 0.7, 0.0, 100.0);
			}
			N.Leisure = Clamp(N.Leisure + (bNight ? 10.0 : 5.0) * Quality, 0.0, 100.0);
		}
		else
		{
			const double Base = bNight ? SleepRelief : NapRelief;
			N.Energy = Clamp(N.Energy + Base * Quality * 0.9, 0.0, 100.0);
			N.Leisure = Clamp(N.Leisure + (bNight ? 14.0 : 7.0) * Quality, 0.0, 100.0);
		}
		// `3 + DOMESTIC.homeMorale` (2).
		if (bNight && bAtHome) N.Morale = Clamp(MoraleOr50(N.Morale) + 3.0 + Domestic::HomeMorale, 0.0, 100.0);
		else if (bNight && Quality < 0.5) N.Morale = Clamp(MoraleOr50(N.Morale) - 4.0, 0.0, 100.0);
		else if (bNight) N.Morale = Clamp(MoraleOr50(N.Morale) + 1.0, 0.0, 100.0);
	}

	void SatisfyEat(FNeeds& N, bool bIndoor, bool bAtHome, double Amount)
	{
		// Interieur : tickNeeds a deja baisse la faim pendant le repas. On finalise
		// vers un ventre raisonnable + bonus foyer, sans re-soustraire eatRelief entier.
		if (bIndoor)
		{
			const double Target = bAtHome ? 10.0 : 18.0;
			if (N.Hunger > Target)
			{
				N.Hunger = Clamp(N.Hunger - (N.Hunger - Target) * 0.75, 0.0, 100.0);
			}
			N.Morale = Clamp(MoraleOr50(N.Morale) + (bAtHome ? 5.0 : 3.0), 0.0, 100.0);
			N.Leisure = Clamp(N.Leisure + (bAtHome ? 6.0 : 3.0), 0.0, 100.0);
			// `Math.ceil(NEEDS.healthEatRestore * 0.55)` = ceil(5.5) = 6.
			N.Health = Clamp(N.Health + FMath::CeilToDouble(HealthEatRestore * 0.55) + (bAtHome ? 2.0 : 0.0), 0.0, 100.0);
		}
		else
		{
			N.Hunger = Clamp(N.Hunger - Amount - (bAtHome ? Domestic::HomeEatBonus : 0.0), 0.0, 100.0);
			N.Morale = Clamp(MoraleOr50(N.Morale) + (bAtHome ? 6.0 : 4.0), 0.0, 100.0);
			N.Leisure = Clamp(N.Leisure + (bAtHome ? 8.0 : 4.0), 0.0, 100.0);
			N.Health = Clamp(N.Health + HealthEatRestore + (bAtHome ? 4.0 : 0.0), 0.0, 100.0);
		}
		if (bAtHome) N.Hygiene = Clamp(N.Hygiene + 4.0, 0.0, 100.0);
	}

	void SatisfyDrink(FNeeds& N, double Amount)
	{
		N.Thirst = Clamp(N.Thirst - Amount, 0.0, 100.0);
		N.Hygiene = Clamp(N.Hygiene + DrinkHygiene, 0.0, 100.0);
		// `(npc.morale || 50)` : 0 est falsy en JS, un moral nul repart de 50.
		const double Morale = N.Morale != 0.0 && !FMath::IsNaN(N.Morale) ? N.Morale : 50.0;
		N.Morale = Clamp(Morale + DrinkMorale, 0.0, 100.0);
		N.Health = Clamp(N.Health + DrinkHealth, 0.0, 100.0);
	}
}
