#include "Work/AnastasisGather.h"

#include "Core/AnastasisJsNumeric.h"
#include "Core/AnastasisSimMath.h"
#include "Village/AnastasisVillage.h"

namespace AnastasisGather
{
	namespace
	{
		const FTrait Traits[TraitCount] = {
			{ TEXT("batisseur"), 1.4, 0.75, 1.1, 0.8 },
			{ TEXT("marchand"), 0.8, 1.5, 0.9, 0.9 },
			{ TEXT("ranger"), 0.75, 0.85, 1.2, 1.5 },
			{ TEXT("gardien"), 1.0, 0.8, 1.0, 1.0 },
			{ TEXT("artisan"), 1.25, 1.05, 0.85, 0.85 },
			{ TEXT("soigneur"), 0.85, 1.0, 1.05, 0.9 },
		};

		// `JOBS.<id>.priority` de metiers/catalog.js.
		const TCHAR* const FarmerPriority[] = {
			TEXT("eat"), TEXT("rest"), TEXT("gatherFood"), TEXT("deliver"), TEXT("sell"), TEXT("buy"),
			TEXT("socialize"), TEXT("relax"), TEXT("visitFamily"), TEXT("build"), TEXT("explore"),
		};
		const TCHAR* const WoodPriority[] = { TEXT("eat"), TEXT("rest"), TEXT("gatherWood"), TEXT("deliver"), TEXT("sell"), TEXT("buy"), TEXT("maintain"), TEXT("build"), TEXT("socialize"), TEXT("relax"), TEXT("visitFamily"), TEXT("explore") };
		const TCHAR* const SettlerPriority[] = {
			TEXT("eat"), TEXT("rest"), TEXT("gatherWood"), TEXT("gatherStone"), TEXT("gatherFood"), TEXT("deliver"),
			TEXT("sell"), TEXT("buy"), TEXT("build"), TEXT("socialize"), TEXT("relax"), TEXT("visitFamily"), TEXT("explore"),
		};

		const TCHAR* const BuilderPriority[] = {
			TEXT("eat"), TEXT("rest"), TEXT("build"), TEXT("buy"), TEXT("maintain"), TEXT("gatherWood"),
			TEXT("gatherStone"), TEXT("sell"), TEXT("socialize"), TEXT("relax"), TEXT("visitFamily"), TEXT("explore"),
		};

		template <int32 N>
		int32 RankIn(const TCHAR* const (&List)[N], const FString& Goal)
		{
			for (int32 I = 0; I < N; ++I)
			{
				if (Goal == List[I]) return I;
			}
			return INDEX_NONE;
		}

		// `FIELD_WORK_POSTS`.
		const double FieldPosts[FieldPostCount][2] = {
			{ 0.26, 0.26 }, { 0.74, 0.26 }, { 0.26, 0.74 }, { 0.74, 0.74 },
			{ 0.50, 0.20 }, { 0.50, 0.80 }, { 0.20, 0.50 }, { 0.80, 0.50 },
		};

		// `FIELD_SEASON_YIELD[season].gather`, printemps -> hiver.
		const double SeasonGather[4] = { 0.95, 1.1, 1.55, 0.85 };
		// `FIELD_SEASON_YIELD[saison].tend` : printemps, ete, automne, hiver.
		const double SeasonTend[4] = { 1.35, 1.5, 1.1, 0.55 };

		bool StartsWithGather(const FString& Goal)
		{
			return Goal.StartsWith(TEXT("gather"), ESearchCase::CaseSensitive);
		}

		/** `risingWillTier(value, urgeAt, criticalAt)`. */
		int32 RisingWillTier(double Value, double UrgeAt, double CriticalAt)
		{
			if (Value >= (UrgeAt + CriticalAt) / 2.0) return 2;
			if (Value >= UrgeAt) return 1;
			return 0;
		}

		/** `hashText(text, channel)` de fieldWorkPosts.js. */
		uint32 FieldPostHash(const FString& Text, uint32 Channel)
		{
			uint32 Hash = Channel * 2654435761u;
			for (const TCHAR C : Text)
			{
				Hash ^= static_cast<uint32>(C);
				Hash = AnastasisJs::Imul(Hash, 16777619u);
			}
			return Hash ^ (Hash >> 16);
		}
	}

	const FTrait& TraitAt(int32 Index)
	{
		return Traits[FMath::Clamp(Index, 0, TraitCount - 1)];
	}

	double JobPriority(const FString& JobId, const FString& Goal)
	{
		int32 Rank = INDEX_NONE;
		if (JobId == JobFarmer) Rank = RankIn(FarmerPriority, Goal);
		else if (JobId == TEXT("woodcutter")) Rank = RankIn(WoodPriority, Goal);
		else if (JobId == JobSettler) Rank = RankIn(SettlerPriority, Goal);
		else if (JobId == TEXT("builder")) Rank = RankIn(BuilderPriority, Goal);
		return Rank < 0 ? 0.0 : FMath::Max(0.0, 18.0 - Rank * 2.5);
	}

	double JobTraitBiasGather(const FString& JobId)
	{
		return JobId == TEXT("woodcutter") ? 1.35 : JobId == JobFarmer ? 1.25 : 1.0;
	}

	double PresumedNoise(const FString& NpcId, const FString& Resource)
	{
		const FString Key = NpcId + TEXT(":") + Resource;
		uint32 H = 2166136261u;
		for (const TCHAR C : Key)
		{
			H ^= static_cast<uint32>(C);
			H = AnastasisJs::Imul(H, 16777619u);
		}
		const double Unit = static_cast<double>(H % 1000u) / 1000.0;
		return 0.85 + Unit * 0.3;
	}

	double BelievedFoodPresumed(const FString& NpcId)
	{
		return PresumedFoodStock * PresumedNoise(NpcId, TEXT("food"));
	}

	double ResourceScoreFood(int32 ActorCount, double Believed, const FString& JobId, double TraitGather, int32 InventoryFood, double Hunger)
	{
		// `Math.max(FLOOR, Math.ceil((sim.actors.length || 1) * 0.9 * FOOD_RESERVE_DAYS))`.
		const double Count = ActorCount > 0 ? static_cast<double>(ActorCount) : 1.0;
		const double Target = FMath::Max(FoodTargetFloor, FMath::CeilToDouble(Count * 0.9 * FoodReserveDays));
		const double BelievedGap = FMath::Max(0.0, Target - Believed);
		// constructionResourcePressure(food) : 0 sans chantier de colonie (aucune branche ne pousse la nourriture).
		const double Construction = 0.0;
		const double MarketPressure = BelievedGap * 0.32;
		double JobFit = 0.0;
		if (JobId == JobFarmer) JobFit = 16.0;
		const double LowStockBonus = BelievedGap * TraitGather * JobTraitBiasGather(JobId);
		const double BellyPanic = InventoryFood <= 0 && Hunger > 38.0 ? (Hunger - 30.0) * 0.55 : 0.0;
		// planBias, collectiveGoalBias, colonySiteGatherBias : 0 sans ambition ni colonie.
		return LowStockBonus + MarketPressure + Construction + JobFit + BellyPanic + JobPriority(JobId, GoalGatherFood);
	}

	double DeliveryScoreDepot(const FString& JobId, int32 DepotLoad)
	{
		if (DepotLoad <= 0) return 0.0;
		return 48.0 + DepotLoad * 5.0 + JobPriority(JobId, GoalDeliver);
	}

	double CompletionBias(const FString& Goal, int32 Load, int32 DepotLoad, const FString& SessionGoal, bool bNeedsCritical)
	{
		if (Goal.IsEmpty()) return 0.0;
		double Bias = 0.0;
		const bool bHeavy = Load >= HeavyLoadAt || DepotLoad >= 4;

		if (Load > 0)
		{
			if (Goal == GoalDeliver && DepotLoad > 0)
			{
				Bias += HaulCompletion + DepotLoad * 4 + (bHeavy ? 10 : 0);
			}
			else if (Goal == TEXT("sell") && DepotLoad <= 0)
			{
				Bias += HaulCompletion + Load * 3 + (bHeavy ? 8 : 0);
			}
			else if (Goal == GoalDeliver && DepotLoad <= 0 && Load > 0)
			{
				Bias += HaulCompletion * 0.35;
			}
			// storeOutputLoad : pas de produit fini porte.
			if (bHeavy && (StartsWithGather(Goal) || Goal == TEXT("explore") || Goal == TEXT("craft")
				|| Goal == TEXT("build") || Goal == TEXT("socialize") || Goal == TEXT("maintain")))
			{
				Bias -= HaulAbandonPenalty;
			}
		}

		if (!SessionGoal.IsEmpty() && !bNeedsCritical)
		{
			if (Goal == SessionGoal)
			{
				Bias += SessionCompletion;
			}
			else if (Goal == TEXT("eat") || Goal == TEXT("eatTogether") || Goal == TEXT("rest") || Goal == TEXT("relieve") || Goal == TEXT("drink"))
			{
				// Les urgences passent.
			}
			else if ((Goal == GoalDeliver || Goal == TEXT("sell")) && Load > 0)
			{
				// Vider le sac prime sur finir le coup.
			}
			else if (StartsWithGather(Goal) || Goal == TEXT("craft") || Goal == TEXT("build")
				|| Goal == TEXT("explore") || Goal == TEXT("socialize") || Goal == TEXT("maintain"))
			{
				Bias -= SessionAbandonPenalty;
			}
		}
		return Bias;
	}

	double TraitGoalBias(const FTrait& T, const FString& Goal)
	{
		if (Goal.IsEmpty()) return 0.0;
		const double Push = TraitPush;
		// `(Number(v) || 1) - 1` : aucun trait du catalogue ne vaut 0.
		auto D = [Push](double V) { return (V - 1.0) * Push; };
		const FString Label = T.Label;
		if (Goal == TEXT("build")) return D(T.Build) * 1.15;
		if (Goal == TEXT("craft") || Goal == TEXT("maintain"))
		{
			double V = D(T.Build) * 0.75;
			if (Label == TEXT("artisan")) V += Push * 0.35;
			return V;
		}
		if (Goal == TEXT("sell") || Goal == TEXT("buy")) return D(T.Trade);
		if (Goal == GoalDeliver || Goal == TEXT("haulJob")) return D(T.Trade) * 0.55 + D(T.Gather) * 0.3;
		if (StartsWithGather(Goal) || Goal == TEXT("helpFarm")) return D(T.Gather);
		if (Goal == TEXT("explore")) return D(T.Explore);
		if (Goal == TEXT("socialize") || Goal == TEXT("visitFamily"))
		{
			double V = D(T.Trade) * 0.4;
			if (Label == TEXT("soigneur")) V += Push * 0.45;
			if (Label == TEXT("gardien")) V += Push * 0.2;
			return V;
		}
		if (Goal == TEXT("confront")) return D(T.Build) * 0.35 - D(T.Trade) * 0.15;
		if (Goal == TEXT("play") || Goal == TEXT("study") || Goal == TEXT("apprentice")) return D(T.Explore) * 0.2 + D(T.Build) * 0.15;
		return 0.0;
	}

	double TintedGatherSkill(const FTrait& Trait)
	{
		// createNpc : `skills.gather *= 0.88 + (trait.gather || 1) * 0.12`, puis createSkills borne.
		return 1.0 * (0.88 + Trait.Gather * 0.12);
	}

	double TintedTradeSkill(const FTrait& Trait)
	{
		return 1.0 * (0.88 + Trait.Trade * 0.12);
	}

	double GranaryWorkplaceGoalBias(const FString& JobId, const FString& Goal, int32 FoodCarried)
	{
		if (Goal.IsEmpty() || JobId.IsEmpty() || JobId == JobSettler) return 0.0;
		// Pas d'atelier a intrant local pour les metiers portes ; grenier = depot de nourriture.
		if (Goal == GoalGatherFood) return 22.0;
		if (Goal == TEXT("helpFarm")) return 14.0;
		if (Goal == GoalDeliver && FoodCarried > 0) return 14.0;
		// `workCommutePos(npc, goal)` au grenier : entretien et apprentissage toujours ;
		// livraison aussi, le grenier portant `storage` au catalogue.
		if (Goal == TEXT("maintain") || Goal == GoalDeliver || Goal == TEXT("apprentice")) return 9.0;
		return 0.0;
	}

	double WorkWillFactor(const AnastasisNeeds::FNeeds& N)
	{
		using namespace AnastasisNeeds::Constants;
		if (AnastasisVillage::NeedsCritical(N)) return 0.0;
		int32 Worst = RisingWillTier(N.Hunger, HungerUrge, HungerCritical);
		if (Worst < 2) Worst = FMath::Max(Worst, RisingWillTier(N.Thirst, ThirstUrge, ThirstCritical));
		if (Worst < 2) Worst = FMath::Max(Worst, RisingWillTier(100.0 - N.Energy, FatigueUrge, FatigueCritical));
		if (Worst < 2) Worst = FMath::Max(Worst, RisingWillTier(100.0 - N.Social, LonelyUrge, LonelyCritical));
		if (Worst < 2) Worst = FMath::Max(Worst, RisingWillTier(100.0 - N.Leisure, BoredUrge, BoredCritical));
		if (Worst < 2) Worst = FMath::Max(Worst, RisingWillTier(100.0 - N.Hygiene, HygieneUrge, HygieneCritical));
		if (Worst < 2) Worst = FMath::Max(Worst, RisingWillTier(HealthUrge - N.Health, 0.0, HealthUrge - HealthCritical));
		const double Morale = N.Morale;
		if (Worst < 2 && Morale < (MoraleUrge + MoraleCritical) / 2.0) Worst = 2;
		else if (Worst < 1 && Morale < MoraleUrge) Worst = 1;
		static const double TierValues[3] = { 1.0, 0.7, 0.45 };
		return TierValues[Worst];
	}

	double MoralEffectiveWork(const AnastasisNeeds::FNeeds& Needs, int32 MarketFood, int32 Day)
	{
		const double Personal = WorkWillFactor(Needs);
		double Famine = 1.0;
		if (MarketFood < 20) Famine = 0.88;
		else if (MarketFood < 60) Famine = 0.95;
		const double Cold = FieldSeasonFromDay(Day) == 3 ? 0.86 : 1.0;
		const double Grief = 1.0;
		// `sim.colony?.morale ?? 50` : 50 est entre 45 et 70, facteur 1.
		const double Colony = 1.0;
		const double WorkMul = AnastasisMath::Clamp(Famine * Cold * Grief * Colony, 0.5, 1.2);
		return Personal * WorkMul;
	}

	double MoralSocialMul(double Morale, int32 MarketFood, int32 Day)
	{
		using namespace AnastasisNeeds::Constants;
		// `famine` et `cold` de moralPressure, comme pour effectiveWork.
		double Famine = 1.0;
		if (MarketFood < 20) Famine = 0.88;
		else if (MarketFood < 60) Famine = 0.95;
		const double Cold = FieldSeasonFromDay(Day) == 3 ? 0.86 : 1.0;
		double Social = 1.0;
		if (Morale < MoraleUrge) Social *= 1.12;
		if (Morale < MoraleCritical) Social *= 1.08;
		// Deuil (`griefRole === "actor"`) : non porte.
		if (Famine < 0.9) Social *= 0.92;
		if (Cold < 1.0) Social *= 0.94;
		return AnastasisMath::Clamp(Social, 0.5, 1.2);
	}

	bool MealPathBlocked(double Hunger, int32 InventoryFood, double BelievedFood)
	{
		if (InventoryFood > 0) return false;
		if (Hunger < AnastasisNeeds::Constants::HungerUrge) return false;
		if (BelievedFood < 12.0) return true;
		if (Hunger >= AnastasisNeeds::Constants::HungerCritical && BelievedFood < 40.0) return true;
		// Echec de repas recent (`mind.failures.goals.eat`) : non porte.
		return false;
	}

	double SurvivalWorkFactor(const FString& Goal, double WorkFactor, bool bMealBlocked)
	{
		if (bMealBlocked && (Goal == GoalGatherFood || Goal == TEXT("helpFarm") || Goal == TEXT("buy")))
		{
			return FMath::Max(WorkFactor, 1.0) * 2.2;
		}
		return WorkFactor;
	}

	bool ShouldHaulGatherLoad(int32 Load)
	{
		return Load > HaulLoadAbove;
	}

	double CraftFatigueT(int32 SwingsDone, double Energy)
	{
		constexpr int32 Onset = 4;
		constexpr int32 FullAt = 14;
		const double Span = static_cast<double>(FMath::Max(1, FullAt - Onset));
		double FromSwings = 0.0;
		if (SwingsDone > Onset)
		{
			FromSwings = FMath::Min(1.0, (SwingsDone - Onset) / Span);
		}
		const double E = FMath::IsFinite(Energy) ? Energy : 100.0;
		double FromEnergy = 0.0;
		constexpr double EnergySoft = 42.0;
		if (E < EnergySoft)
		{
			FromEnergy = FMath::Min(0.45, (EnergySoft - E) / EnergySoft);
		}
		return FMath::Max(0.0, FMath::Min(1.0, FromSwings * 0.82 + FromEnergy * 0.55 + FromSwings * FromEnergy * 0.25));
	}

	double CraftFatiguePeriodMul(int32 SwingsDone, double Energy)
	{
		return 1.0 + CraftFatigueT(SwingsDone, Energy) * (1.28 - 1.0);
	}

	double SwingPeriodFarm(double Skill, int32 SwingsDone, double Energy)
	{
		const double S = AnastasisJs::NumberOr(Skill, 0.7);
		double Period = FMath::Max(FarmMinSwingPeriod, FarmBaseSwingPeriod - S * FarmSkillPeriodFactor);
		Period *= CraftFatiguePeriodMul(SwingsDone, Energy);
		// techniqueWorkPeriodMultiplier : 1 sans technique.
		return Period;
	}

	int32 YieldPerSwingFarm(double Skill)
	{
		// `(Number(npc.skill) || 0.7) + bestCraftMastery * bonus` : aucune technique, + 0.
		const double S = AnastasisJs::NumberOr(Skill, 0.7) + 0.0;
		return S >= 1.35 ? 3 : 2;
	}

	int32 FieldSeasonFromDay(int32 Day)
	{
		if (!(Day > 0)) return 1;
		return ((Day - 1) % 120) / 30 % 4;
	}

	int32 FieldSeasonGatherAmount(int32 Base, int32 Day)
	{
		const double Mul = SeasonGather[FieldSeasonFromDay(Day)];
		return FMath::Max(1, static_cast<int32>(AnastasisJs::Round(static_cast<double>(Base) * Mul)));
	}

	double SwingPeriodTend(double Skill, int32 SwingsDone, double Energy)
	{
		const double S = AnastasisJs::NumberOr(Skill, 0.7);
		double Period = FMath::Max(TendMinSwingPeriod, TendBaseSwingPeriod - S * TendSkillPeriodFactor);
		Period *= CraftFatiguePeriodMul(SwingsDone, Energy);
		// techniqueWorkPeriodMultiplier : 1 sans technique.
		return Period;
	}

	int32 FieldSeasonTendAmount(int32 Base, int32 Day)
	{
		const double Mul = SeasonTend[FieldSeasonFromDay(Day)];
		return FMath::Max(1, static_cast<int32>(AnastasisJs::Round(static_cast<double>(Base) * Mul)));
	}

	int32 PreferredFieldPostIndex(const FString& NpcId, int32 TileX, int32 TileY)
	{
		const FString Id = NpcId.IsEmpty() ? FString(TEXT("npc")) : NpcId;
		const FString Text = FString::Printf(TEXT("%s:%d,%d"), *Id, TileX, TileY);
		return static_cast<int32>(FieldPostHash(Text, 41u) % static_cast<uint32>(FieldPostCount));
	}

	void FieldPostWorld(int32 TileX, int32 TileY, int32 Index, double& OutX, double& OutY)
	{
		const double* Post = FieldPosts[((Index % FieldPostCount) + FieldPostCount) % FieldPostCount];
		OutX = TileX + Post[0];
		OutY = TileY + Post[1];
	}

	int32 NearestFieldPostIndex(int32 TileX, int32 TileY, double X, double Y)
	{
		int32 Best = 0;
		double BestD = TNumericLimits<double>::Max();
		bool bAny = false;
		for (int32 I = 0; I < FieldPostCount; ++I)
		{
			const double D = AnastasisMath::JsHypot(X - (TileX + FieldPosts[I][0]), Y - (TileY + FieldPosts[I][1]));
			if (!bAny || D < BestD)
			{
				bAny = true;
				BestD = D;
				Best = I;
			}
		}
		return Best;
	}

	int32 FieldWorkPostIndex(int32 Preferred, uint32 ClaimedMask)
	{
		if ((ClaimedMask & (1u << Preferred)) == 0) return Preferred;
		for (int32 I = 0; I < FieldPostCount; ++I)
		{
			const int32 J = (Preferred + I) % FieldPostCount;
			if ((ClaimedMask & (1u << J)) == 0) return J;
		}
		return Preferred;
	}

	double NeutralLearnFactor()
	{
		// `clamp(0.75 + n.esprit * 0.28, 0.7, 1.35)`, esprit 1.
		return AnastasisMath::Clamp(0.75 + 1.0 * 0.28, 0.7, 1.35);
	}

	void GainDomainSkill(double& InOutSkill, double& InOutDomainSkill, double Amount)
	{
		if (!(Amount > 0.0)) return;
		const double Gained = Amount * NeutralLearnFactor();
		InOutSkill = FMath::Min(SkillCap, AnastasisJs::NumberOrZero(InOutSkill) + Gained);
		const double Base = InOutDomainSkill != 0.0 && !FMath::IsNaN(InOutDomainSkill) ? InOutDomainSkill : SkillFloor;
		InOutDomainSkill = FMath::Min(SkillCap, Base + Gained * 1.15);
	}

	double SkillGoalBias(double DomainSkill)
	{
		const double V = DomainSkill != 0.0 && !FMath::IsNaN(DomainSkill) ? DomainSkill : 1.0;
		return (V - 1.0) * 8.0;
	}
}
