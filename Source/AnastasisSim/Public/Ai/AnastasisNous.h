// Noûs — la decision algorithmique du cycle faim (`src/ai/algorithmic/*`).
//
// ACTIVE PAR DEFAUT dans la reference (`ALGORITHMIC_NPC_V1 = true`, flags.js:7 ;
// rien dans src n'ecrit `sim.flags`). Decision prise le 2026-09-29 : porter ce
// que le jeu execute reellement, pas le chemin classique.
//
// Porte ici, pur, et prouve par vecteurs (`Anastasis.Sim.Parite.Nous`) :
//   decision.js    makeDecision (bornes), normalizeUtilityScore
//   hungerUtility  hungerUrgency, scoreHungerCandidates (7 candidats), tri,
//                  selectCandidateWithCooldown, decisionToNpcGoal,
//                  decisionSourceBuildingId
//   inertia.js     evaluateInertia
//   scheduler.js   decisionIntervalSeconds (bande « near »), stagger
//
// L'etat (reservations, action faim, pont vers la table de buts) vit dans le
// village : Village/AnastasisVillage.h.

#pragma once

#include "CoreMinimal.h"

namespace AnastasisNous
{
	/** `HUNGER_UTILITY`. */
	inline constexpr double ScoreCeiling = 140.0;
	inline constexpr double WorkBaseline = 28.0;
	inline constexpr double SleepBaseline = 18.0;
	inline constexpr double WaitBaseline = 6.0;

	/** `INERTIA`. */
	inline constexpr double MinCommitSeconds = 2.5;
	inline constexpr double SwitchMargin = 0.08;
	inline constexpr double InterruptCost = 0.12;
	inline constexpr double FailureCooldownSeconds = 4.0;
	inline constexpr double UrgencyInterruptAt = 0.85;

	/** `ALGO_SCHEDULER` (bande « near » : tout habitant porte est proche). */
	inline constexpr double DecideDefaultSeconds = 2.2;
	inline constexpr double DecideCriticalSeconds = 0.55;
	inline constexpr double ReservationSweepSeconds = 2.5;

	/** `BRIDGE`. */
	inline constexpr double BridgeScoreScale = 72.0;
	inline constexpr double BridgeUrgencyExtra = 36.0;
	inline constexpr double BridgeInertiaKeepBonus = 28.0;
	inline constexpr double BridgeFailureEatPenalty = 40.0;
	inline constexpr double BridgeFailureGatherBoost = 22.0;
	inline constexpr double BridgeSeekGatherBoost = 18.0;
	inline constexpr double BridgeWorkSuppressEat = 16.0;

	/** `DecisionRecord` — les champs que le pont et la reservation lisent. */
	struct FDecision
	{
		FString Type = TEXT("idle");
		/** Vide = null. */
		FString TargetId;
		double Score = 0.0;
		double Urgency = 0.0;
		FString Reason = TEXT("unspecified");
		double CreatedAt = 0.0;
		double ExpectedDuration = 0.0;
		// metadata
		double Raw = 0.0;
		FString SourceBuildingId;
		/** NaN = null (trajet inconnu). */
		double TravelSeconds = 0.0;

		bool IsValid() const { return !Type.IsEmpty(); }
	};

	/** Contexte de perception faim (`perceiveFoodContext`) — jamais omniscient. */
	struct FFoodContext
	{
		double Hunger = 0.0;
		int32 InventoryFood = 0;
		double BelievedFood = 0.0;
		/** Vide = aucune source connue. */
		FString BestSourceBuildingId;
		/** NaN = null. */
		double BestSourceDistance = 0.0;
		double BestSourceEstimated = 0.0;
		double BestSourceConfidence = 0.0;
		double Certainty = 0.0;
		bool bDangerNear = false;
		int32 Gold = 0;
	};

	/** Ce que scoreHungerCandidates lit de l'habitant, hors contexte. */
	struct FHungerSubject
	{
		double Energy = 70.0;
		double Speed = 3.2;
		double Skill = 0.0;
		FString WorkplaceId;
		FString HomeId;
	};

	/** Resultat de scoreHungerCandidates. */
	struct FScored
	{
		/** Candidats tries : score desc, puis urgence desc (tri stable). */
		TArray<FDecision> Candidates;
		FDecision Best;
		bool bHasBest = false;
		TArray<FString> Excluded;
	};

	/** `clamp01` de decision.js : non fini -> 0. */
	ANASTASISSIM_API double Clamp01(double Value);

	/** `hungerUrgency`. */
	ANASTASISSIM_API double HungerUrgency(double Hunger);

	/** `scoreHungerCandidates` avec un contexte fourni. ExcludeTypes = types en cooldown. */
	ANASTASISSIM_API FScored ScoreHungerCandidates(const FFoodContext& Context, const FHungerSubject& Subject, double Now, const TArray<FString>& ExcludeTypes);

	/** `evaluateInertia` ; rend Keep et la raison. */
	ANASTASISSIM_API bool EvaluateInertia(const FDecision* Current, const FDecision* Candidate, double ElapsedSeconds, double FailureCooldownLeft, double DangerUrgency, FString& OutReason);

	/** `decisionToNpcGoal` ; vide = null. */
	ANASTASISSIM_API FString DecisionToNpcGoal(const FDecision& Decision);

	/** `decisionSourceBuildingId` ; vide = null. */
	ANASTASISSIM_API FString DecisionSourceBuildingId(const FDecision& Decision);

	/** `isUrgentInterrupt`. */
	inline bool IsUrgentInterrupt(double Urgency) { return Clamp01(Urgency) >= UrgencyInterruptAt; }

	/** `decisionIntervalSeconds` (bande near, sans trait de prudence : non porte). */
	ANASTASISSIM_API double DecisionIntervalSeconds(bool bCritical);

	/** `deterministicAlgoStagger(npc, interval)` de npc.js. */
	ANASTASISSIM_API double DeterministicAlgoStagger(const FString& Id, double Interval);
}
