#include "Ai/AnastasisSpatialRisk.h"

#include "Core/AnastasisSimMath.h"
#include "Life/AnastasisNeeds.h"
#include "Life/AnastasisWeatherBehavior.h"

#include <limits>

namespace AnastasisSpatialRisk
{
	namespace
	{
		constexpr double Infinity = std::numeric_limits<double>::infinity();

		/** `clamp` de util.js : `Math.max(min, Math.min(max, v))`. */
		double JsClamp(double V, double Min, double Max)
		{
			return FMath::Max(Min, FMath::Min(Max, V));
		}

		const TCHAR* const GoalList[] = {
			TEXT("gatherWood"), TEXT("gatherStone"), TEXT("gatherFood"), TEXT("helpFarm"),
			TEXT("build"), TEXT("explore"), TEXT("maintain"), TEXT("aidHousehold"),
		};
	}

	TConstArrayView<const TCHAR*> Goals()
	{
		return MakeArrayView(GoalList);
	}

	double SecondsUntilThreshold(double Current, double Critical, double RisePerSecond)
	{
		if (!FMath::IsFinite(Current) || !FMath::IsFinite(Critical) || !(RisePerSecond > 0.0)) return Infinity;
		if (Current >= Critical) return 0.0;
		return FMath::Max(0.0, (Critical - Current) / RisePerSecond - Forecast::ReturnSlackSeconds);
	}

	FNeedBudget CriticalNeedBudget(double Hunger, double Thirst, double Energy)
	{
		namespace N = AnastasisNeeds::Constants;
		FNeedBudget Out;
		Out.Hunger = SecondsUntilThreshold(Hunger, N::HungerCritical, N::HungerRise + 0.06);
		Out.Thirst = SecondsUntilThreshold(Thirst, N::ThirstCritical, N::ThirstRise + 0.12);
		Out.Rest = SecondsUntilThreshold(100.0 - Energy, N::FatigueCritical, N::EnergyFall);
		return Out;
	}

	double SecondsUntilNight(double DayFrac)
	{
		// `civilHourOf` : `(((frac % 1) + 1) % 1) * 24` — `%` de JS = fmod (signe du dividende).
		const double Hour = FMath::Fmod(FMath::Fmod(DayFrac, 1.0) + 1.0, 1.0) * 24.0;
		if (Hour >= 21.0 || Hour < 5.0) return 0.0;
		return ((21.0 - Hour) / 24.0) * 90.0 - Forecast::ReturnSlackSeconds;
	}

	double RainReturnBudget(double Rain)
	{
		namespace S = AnastasisWeatherBehavior::Shelter;
		if (Rain >= S::RainHeavy) return 0.0;
		if (Rain < Forecast::RainCautionThreshold) return Infinity;
		const double T = JsClamp((Rain - Forecast::RainCautionThreshold)
			/ FMath::Max(0.01, S::RainHeavy - Forecast::RainCautionThreshold), 0.0, 1.0);
		return Forecast::RainReturnSeconds * (1.0 - T);
	}

	double RouteSpeed(double Speed)
	{
		// `npc.speed || 3.4` : 0 et NaN retombent sur 3,4.
		return FMath::Max(1.6, (Speed != 0.0 && !FMath::IsNaN(Speed)) ? Speed : 3.4);
	}

	double RouteSeconds(double NpcX, double NpcY, double Speed, const FVector2D& Target, const TOptional<FVector2D>& Safe)
	{
		const double V = RouteSpeed(Speed);
		const double Outward = AnastasisMath::Dist(NpcX, NpcY, Target.X, Target.Y) / V;
		const double Back = Safe.IsSet() ? AnastasisMath::Dist(Target.X, Target.Y, Safe->X, Safe->Y) / V : 0.0;
		return Outward + Back;
	}

	double BudgetOverrun(double Route, double Available)
	{
		if (!(Route > 0.0)) return 0.0;
		if (!FMath::IsFinite(Available)) return 0.0;
		if (Available <= 0.0) return JsClamp(Route / 6.0, 0.0, 1.8);
		if (Route <= Available) return 0.0;
		return JsClamp((Route - Available) / FMath::Max(4.0, Available * 0.55), 0.0, 1.8);
	}

	double GoalPressure(const FRiskInputs& In, const FVector2D& Target)
	{
		const FNeedBudget Need = CriticalNeedBudget(In.Hunger, In.Thirst, In.Energy);
		const double Night = SecondsUntilNight(In.DayFrac);
		const double RainBudget = RainReturnBudget(In.Rain);
		auto Route = [&In, &Target](const TOptional<FVector2D>& Safe)
		{
			return RouteSeconds(In.NpcX, In.NpcY, In.Speed, Target, Safe);
		};
		double Pressure = 0.0;
		Pressure = FMath::Max(Pressure, BudgetOverrun(Route(In.EatSafe), Need.Hunger));
		Pressure = FMath::Max(Pressure, BudgetOverrun(Route(In.DrinkSafe), Need.Thirst));
		Pressure = FMath::Max(Pressure, BudgetOverrun(Route(In.RestSafe), Need.Rest));
		Pressure = FMath::Max(Pressure, BudgetOverrun(Route(In.RestSafe), Night));
		if (RainBudget > 0.0)
		{
			Pressure = FMath::Max(Pressure, BudgetOverrun(Route(In.ShelterSafe), RainBudget));
		}
		return Pressure;
	}

	double GoalBias(const FString& Goal, double Pressure)
	{
		const bool bHeavy = Goal == TEXT("explore") || Goal == TEXT("gatherWood") || Goal == TEXT("gatherStone")
			|| Goal == TEXT("gatherFood");
		const double Scale = bHeavy ? Forecast::RouteHeavyPenaltyScale : Forecast::RoutePenaltyScale;
		return -(Pressure * Scale);
	}

	TArray<TPair<FString, double>> BiasMap(const FRiskInputs& In,
		TFunctionRef<TOptional<FVector2D>(const FString& Goal)> TargetFor)
	{
		TArray<TPair<FString, double>> Map;
		for (const TCHAR* GoalName : Goals())
		{
			const FString Goal = GoalName;
			const TOptional<FVector2D> Target = TargetFor(Goal);
			if (!Target.IsSet()) continue;
			const double Pressure = GoalPressure(In, *Target);
			if (Pressure <= 0.0) continue;
			Map.Add(TPair<FString, double>(Goal, GoalBias(Goal, Pressure)));
		}
		return Map;
	}

	namespace
	{
		const TCHAR* const HeavyWorkGoals[] = {
			TEXT("gatherWood"), TEXT("gatherStone"), TEXT("gatherFood"), TEXT("helpFarm"), TEXT("build"),
			TEXT("deliver"), TEXT("fetchInput"), TEXT("haulJob"), TEXT("haulCart"), TEXT("explore"),
		};
		const TCHAR* const LightWorkGoals[] = { TEXT("craft"), TEXT("maintain"), TEXT("sell"), TEXT("closeWorkplace") };

		/** `SURVIVAL_PRODUCTION_GOALS`. */
		bool IsSurvivalProductionGoal(const FString& Goal)
		{
			return Goal == TEXT("gatherFood") || Goal == TEXT("helpFarm") || Goal == TEXT("buy");
		}

		/** `bias[goal] = value` : la cle garde sa place si elle existe, sinon elle s'ajoute en fin. */
		void SetBias(TArray<TPair<FString, double>>& Map, const FString& Goal, double Value)
		{
			for (TPair<FString, double>& Entry : Map)
			{
				if (Entry.Key == Goal)
				{
					Entry.Value = Value;
					return;
				}
			}
			Map.Add(TPair<FString, double>(Goal, Value));
		}
	}

	double BiasOf(const TArray<TPair<FString, double>>& Map, const FString& Goal)
	{
		for (const TPair<FString, double>& Entry : Map)
		{
			if (Entry.Key == Goal) return Entry.Value;
		}
		return 0.0;
	}

	double TravelPressureToTarget(double NpcX, double NpcY, double Speed, const TOptional<FVector2D>& Target, double Horizon)
	{
		if (!Target.IsSet()) return 0.0;
		const double TravelSeconds = AnastasisMath::Dist(NpcX, NpcY, Target->X, Target->Y) / RouteSpeed(Speed);
		const double Excess = TravelSeconds - Survival::TravelSafeSeconds;
		if (Excess <= 0.0) return 0.0;
		return JsClamp(Excess / FMath::Max(4.0, Horizon - Survival::TravelSafeSeconds), 0.0, 1.4);
	}

	FProjectedNeeds ProjectedNeedPressure(double Hunger, double Thirst, double Energy, double Seconds)
	{
		namespace N = AnastasisNeeds::Constants;
		FProjectedNeeds Out;
		Out.Hunger = JsClamp(Hunger + Seconds * N::HungerRise, 0.0, 100.0);
		Out.Thirst = JsClamp(Thirst + Seconds * (N::ThirstRise + 0.12), 0.0, 100.0);
		Out.Fatigue = JsClamp((100.0 - Energy) + Seconds * N::EnergyFall, 0.0, 100.0);
		return Out;
	}

	double ForecastNeedBias(double Projected, double UrgeAt, double CriticalAt, double TravelPressure, double Scale)
	{
		const double PrecriticalAt = FMath::Max(UrgeAt, CriticalAt - Survival::PrecriticalMargin);
		if (Projected < UrgeAt && TravelPressure <= 0.0) return 0.0;
		const double Warning = FMath::Max(0.0, Projected - UrgeAt) * 0.16;
		const double Precritical = FMath::Max(0.0, Projected - PrecriticalAt) * Scale;
		return Warning + Precritical * (1.0 + TravelPressure * 0.9);
	}

	TArray<TPair<FString, double>> SurvivalForecastBias(const FForecastInputs& In)
	{
		namespace N = AnastasisNeeds::Constants;
		// `ensureNeeds(npc, sim.rng)` : les metres existent toujours ici, aucun tirage.
		const double Horizon = Survival::HorizonSeconds;
		const FProjectedNeeds Projected = ProjectedNeedPressure(In.Hunger, In.Thirst, In.Energy, Horizon);
		const double EatTravel = TravelPressureToTarget(In.NpcX, In.NpcY, In.Speed, In.EatTarget, Horizon);
		const double DrinkTravel = TravelPressureToTarget(In.NpcX, In.NpcY, In.Speed, In.DrinkTarget, Horizon);
		const double RestTravel = TravelPressureToTarget(In.NpcX, In.NpcY, In.Speed, In.RestTarget, Horizon);
		const double Eat = ForecastNeedBias(Projected.Hunger, N::HungerUrge, N::HungerCritical, EatTravel, Survival::EatScale);
		const double Drink = ForecastNeedBias(Projected.Thirst, N::ThirstUrge, N::ThirstCritical, DrinkTravel, Survival::DrinkScale);
		const double Rest = ForecastNeedBias(Projected.Fatigue, N::FatigueUrge, N::FatigueCritical, RestTravel, Survival::RestScale);
		const double TotalRisk = Eat * 0.45 + Drink * 0.65 + Rest * 0.5;
		TArray<TPair<FString, double>> Bias;
		if (TotalRisk <= 0.01) return Bias;

		SetBias(Bias, TEXT("eat"), Eat);
		SetBias(Bias, TEXT("drink"), Drink);
		SetBias(Bias, TEXT("rest"), Rest);
		if (In.InventoryFood <= 0)
		{
			SetBias(Bias, TEXT("buy"), Eat * 0.24);
			SetBias(Bias, TEXT("sell"), -Eat * 0.12);
		}
		else
		{
			SetBias(Bias, TEXT("eatTogether"), Eat * 0.18);
		}
		if (Drink > 0.0) SetBias(Bias, TEXT("shelterRain"), -Drink * 0.08);

		for (const TCHAR* GoalName : HeavyWorkGoals)
		{
			const FString Goal = GoalName;
			if (In.bMealBlocked && IsSurvivalProductionGoal(Goal)) continue;
			SetBias(Bias, Goal, BiasOf(Bias, Goal) - TotalRisk * Survival::HeavyWorkPenaltyScale);
		}
		for (const TCHAR* GoalName : LightWorkGoals)
		{
			const FString Goal = GoalName;
			SetBias(Bias, Goal, BiasOf(Bias, Goal) - TotalRisk * Survival::WorkPenaltyScale);
		}
		// Pivot : sans repas possible, la faim pousse la recolte / l'achat.
		if (In.bMealBlocked && Eat > 0.0)
		{
			SetBias(Bias, TEXT("gatherFood"), BiasOf(Bias, TEXT("gatherFood")) + Eat * 0.62);
			SetBias(Bias, TEXT("helpFarm"), BiasOf(Bias, TEXT("helpFarm")) + Eat * 0.42);
			SetBias(Bias, TEXT("buy"), BiasOf(Bias, TEXT("buy")) + Eat * 0.38);
		}
		return Bias;
	}
}
