#include "Ai/AnastasisNous.h"

#include "Life/AnastasisNeeds.h"

namespace AnastasisNous
{
	namespace
	{
		/** `normalizeUtilityScore(raw, ceiling)`. */
		double Normalize(double Raw)
		{
			return Clamp01(Raw / FMath::Max(1.0, ScoreCeiling));
		}

		FDecision Make(const TCHAR* Type, const FString& TargetId, double Raw, double Urgency,
			const TCHAR* Reason, double Now, double ExpectedDuration)
		{
			FDecision D;
			D.Type = Type;
			D.TargetId = TargetId;
			D.Score = Normalize(Raw);
			D.Urgency = Clamp01(Urgency);
			D.Reason = Reason;
			D.CreatedAt = Now;
			D.ExpectedDuration = FMath::Max(0.0, ExpectedDuration);
			D.Raw = Raw;
			return D;
		}
	}

	double Clamp01(double Value)
	{
		if (!FMath::IsFinite(Value)) return 0.0;
		return FMath::Max(0.0, FMath::Min(1.0, Value));
	}

	double HungerUrgency(double Hunger)
	{
		using namespace AnastasisNeeds::Constants;
		if (Hunger < HungerUrge) return Clamp01(Hunger / FMath::Max(1.0, HungerUrge) * 0.35);
		if (Hunger < HungerCritical)
		{
			return Clamp01(0.35 + (Hunger - HungerUrge) / FMath::Max(1.0, HungerCritical - HungerUrge) * 0.4);
		}
		return Clamp01(0.75 + (Hunger - HungerCritical) / 40.0 * 0.25);
	}

	FScored ScoreHungerCandidates(const FFoodContext& C, const FHungerSubject& S, double Now, const TArray<FString>& ExcludeTypes)
	{
		const double Urgency = HungerUrgency(C.Hunger);
		const double Fatigue = FMath::Max(0.0, 100.0 - S.Energy);
		TArray<FDecision> Candidates;

		// Manger depuis l'inventaire.
		{
			const double Accessible = C.InventoryFood > 0 ? 1.0 : 0.0;
			const double Raw = Accessible * (C.Hunger * 1.15 + 20.0);
			FDecision D = Make(TEXT("eat"), TEXT("inventory"), Raw, Urgency,
				Accessible > 0.0 ? TEXT("hunger_inventory_ready") : TEXT("no_inventory_food"),
				Now, AnastasisNeeds::Constants::EatDuration);
			D.TravelSeconds = 0.0;
			Candidates.Add(D);
		}

		// Chercher une source connue precise (batiment).
		{
			const FString& SourceId = C.BestSourceBuildingId;
			const bool bKnown = !SourceId.IsEmpty() && C.BestSourceEstimated > 0.0 && C.Certainty >= 0.35;
			const double Distance = C.BestSourceDistance;
			const bool bHasDistance = !FMath::IsNaN(Distance);
			// `estimateTravelSeconds` : distance / max(1.4, speed || 3.2).
			const double Speed = S.Speed != 0.0 && !FMath::IsNaN(S.Speed) ? S.Speed : 3.2;
			const double TravelSeconds = FMath::IsFinite(Distance) ? Distance / FMath::Max(1.4, Speed) : NAN;
			const double DistancePenalty = bHasDistance ? FMath::Min(0.35, Distance / 40.0) : 0.15;
			const double DangerPenalty = C.bDangerNear ? 0.4 : 0.0;
			const double Availability = bKnown ? Clamp01(C.Certainty - DistancePenalty - DangerPenalty) : 0.0;
			const double Raw = Availability * (C.Hunger * 1.05 + 16.0) * (1.0 - DistancePenalty);
			const TCHAR* Reason = SourceId.IsEmpty() ? TEXT("no_known_building_source")
				: C.BestSourceEstimated <= 0.0 ? TEXT("known_source_believed_empty")
				: C.bDangerNear ? TEXT("food_known_but_danger")
				: TEXT("hunger_high_food_known_and_accessible");
			// `12 + (travelSeconds || 20)` : null et 0 retombent sur 20.
			const double Travel = FMath::IsNaN(TravelSeconds) || TravelSeconds == 0.0 ? 20.0 : TravelSeconds;
			FDecision D = Make(TEXT("seek_food"), SourceId, Raw, Urgency, Reason, Now, 12.0 + Travel);
			D.SourceBuildingId = SourceId;
			D.TravelSeconds = TravelSeconds;
			Candidates.Add(D);
		}

		// Acheter si or + croyance marche (pas de stock vrai).
		{
			const bool bCanBuy = C.Gold >= 2 && C.BelievedFood > 0.0;
			const double Raw = bCanBuy ? (C.Hunger * 0.7 + 12.0) : 0.0;
			FDecision D = Make(TEXT("buy_food"), TEXT("market"), Raw, Urgency * 0.9,
				bCanBuy ? TEXT("hunger_market_solvable") : TEXT("cannot_buy"), Now, 18.0);
			Candidates.Add(D);
		}

		{
			const double WorkDrive = FMath::Max(0.0, WorkBaseline - Urgency * 40.0 + (S.Skill * 2.0));
			FDecision D = Make(TEXT("work"), S.WorkplaceId, WorkDrive, 1.0 - Urgency,
				Urgency > 0.7 ? TEXT("work_suppressed_by_hunger") : TEXT("work_available"), Now, 30.0);
			Candidates.Add(D);
		}

		{
			const double RestNeed = Fatigue > 40.0 ? Fatigue * 0.55 : SleepBaseline * 0.4;
			FDecision D = Make(TEXT("sleep"), S.HomeId, RestNeed, Fatigue / 100.0,
				Fatigue > 55.0 ? TEXT("fatigue_high") : TEXT("rest_low_priority"), Now,
				AnastasisNeeds::Constants::SleepDuration);
			Candidates.Add(D);
		}

		{
			const double FleeRaw = C.bDangerNear ? 90.0 + Urgency * 20.0 : 0.0;
			FDecision D = Make(TEXT("flee"), FString(), FleeRaw, C.bDangerNear ? 0.95 : 0.0,
				C.bDangerNear ? TEXT("danger_near") : TEXT("no_danger"), Now, 8.0);
			Candidates.Add(D);
		}

		{
			const double WaitRaw = WaitBaseline * (1.0 - Urgency);
			FDecision D = Make(TEXT("wait"), FString(), WaitRaw, 0.05, TEXT("intentional_idle"), Now, 5.0);
			Candidates.Add(D);
		}

		FScored Out;
		Out.Candidates = Candidates;
		// `sort((a, b) => b.score - a.score || b.urgency - a.urgency)` : tri stable.
		Out.Candidates.StableSort([](const FDecision& A, const FDecision& B)
		{
			if (A.Score != B.Score) return A.Score > B.Score;
			return A.Urgency > B.Urgency;
		});
		Out.Excluded = ExcludeTypes;

		// `selectCandidateWithCooldown`.
		if (ExcludeTypes.Num() == 0)
		{
			if (Out.Candidates.Num() > 0)
			{
				Out.Best = Out.Candidates[0];
				Out.bHasBest = true;
			}
			return Out;
		}
		for (const FDecision& D : Out.Candidates)
		{
			if (!ExcludeTypes.Contains(D.Type))
			{
				Out.Best = D;
				Out.bHasBest = true;
				return Out;
			}
			if (D.Type == TEXT("flee") && C.bDangerNear && D.Urgency >= 0.85)
			{
				Out.Best = D;
				Out.bHasBest = true;
				return Out;
			}
		}
		for (const FDecision& D : Out.Candidates)
		{
			if (D.Type == TEXT("wait"))
			{
				Out.Best = D;
				Out.bHasBest = true;
				break;
			}
		}
		return Out;
	}

	bool EvaluateInertia(const FDecision* Current, const FDecision* Candidate, double ElapsedSeconds,
		double FailureCooldownLeft, double DangerUrgency, FString& OutReason)
	{
		if (!Current) { OutReason = TEXT("no_current"); return false; }
		if (!Candidate) { OutReason = TEXT("no_candidate"); return true; }
		const double Urgency = FMath::Max(Clamp01(Candidate->Urgency), Clamp01(DangerUrgency));
		if (Urgency >= UrgencyInterruptAt && Candidate->Type != Current->Type)
		{
			OutReason = TEXT("urgency_interrupt");
			return false;
		}
		if (FailureCooldownLeft > 0.0 && Candidate->Type == Current->Type)
		{
			OutReason = TEXT("failure_cooldown_same_action");
			return false;
		}
		if (ElapsedSeconds < MinCommitSeconds && Urgency < UrgencyInterruptAt)
		{
			OutReason = TEXT("min_commit");
			return true;
		}
		const double Needed = Current->Score + SwitchMargin + InterruptCost;
		if (Candidate->Score < Needed)
		{
			OutReason = TEXT("switch_margin");
			return true;
		}
		OutReason = TEXT("candidate_better");
		return false;
	}

	FString DecisionToNpcGoal(const FDecision& D)
	{
		if (D.Type == TEXT("eat") || D.Type == TEXT("seek_food")) return TEXT("eat");
		if (D.Type == TEXT("buy_food")) return TEXT("buy");
		if (D.Type == TEXT("sleep")) return TEXT("rest");
		// work, flee (FLEE_BRIDGE.legacyGoal = null), wait, idle -> null.
		return FString();
	}

	FString DecisionSourceBuildingId(const FDecision& D)
	{
		if (!D.SourceBuildingId.IsEmpty()) return D.SourceBuildingId;
		if (D.Type == TEXT("seek_food")) return D.TargetId;
		return FString();
	}

	double DecisionIntervalSeconds(bool bCritical)
	{
		if (bCritical) return DecideCriticalSeconds;
		// Bande near ; aucun habitant de la reference ne porte `prudence`.
		return FMath::Max(1.0, FMath::Min(5.0, DecideDefaultSeconds));
	}

	double DeterministicAlgoStagger(const FString& Id, double Interval)
	{
		uint32 H = 2166136261u;
		for (const TCHAR C : Id)
		{
			H ^= static_cast<uint32>(C);
			H *= 16777619u;
		}
		return (static_cast<double>(H % 1000u) / 1000.0) * FMath::Max(0.05, Interval != 0.0 ? Interval : 1.0);
	}
}
