#include "Life/AnastasisWorkShift.h"

#include "Core/AnastasisSimMath.h"
#include "Life/AnastasisVillageRhythm.h"

namespace AnastasisWorkShift
{
	const TCHAR* StateId(EState State)
	{
		switch (State)
		{
		case EState::OffDuty: return TEXT("OFF_DUTY");
		case EState::Commuting: return TEXT("COMMUTING");
		case EState::OnShift: return TEXT("ON_SHIFT");
		case EState::Break: return TEXT("BREAK");
		default: return TEXT("");
		}
	}

	bool IsShiftEntryGoal(const FString& Goal)
	{
		return Goal == TEXT("craft") || Goal == TEXT("build") || Goal == TEXT("maintain");
	}

	bool IsShiftWorkFamily(const FString& Goal)
	{
		return AnastasisRhythm::IsWorkGoal(Goal) || Goal == TEXT("fetchInput");
	}

	void NoteShiftGoalCommit(FWorkShift& Shift, const FString& Goal, double Now, bool bCritical, bool bEntry)
	{
		const bool bActive = Shift.State == EState::Commuting || Shift.State == EState::OnShift;
		if (bActive && IsShiftWorkFamily(Goal))
		{
			Shift.Goal = Goal;
			return;
		}
		if (bEntry)
		{
			// Un quart neuf : `npc.workShift = { state: COMMUTING, goal, startedAt, floorUntil }`.
			Shift.State = EState::Commuting;
			Shift.Goal = Goal;
			Shift.StartedAt = Now;
			Shift.FloorUntil = Now + FloorSeconds;
			return;
		}
		if (bActive)
		{
			Shift.State = bCritical ? EState::Break : EState::OffDuty;
			Shift.Goal.Reset();
		}
	}

	void NoteShiftArrival(FWorkShift& Shift)
	{
		if (Shift.State == EState::Commuting)
		{
			Shift.State = EState::OnShift;
		}
	}

	bool ShiftShields(const FWorkShift& Shift, const FString& Goal, double Now, bool bCritical)
	{
		if (Shift.State == EState::None) return false;
		if (Shift.State != EState::Commuting && Shift.State != EState::OnShift) return false;
		// `now >= (shift.floorUntil ?? 0)`.
		if (Now >= Shift.FloorUntil) return false;
		// Auto-desarmement : un but deja sorti de la famille travail ne protege rien.
		if (!IsShiftWorkFamily(Goal)) return false;
		// Le palier critique est LA porte de sortie.
		if (bCritical) return false;
		return true;
	}

	bool OpensExtractionShift(
		const FString& Goal,
		const FString& WorkplaceType,
		double WorkplaceProgress,
		double WorkplaceX,
		double WorkplaceY,
		bool bHasTarget,
		double TargetX,
		double TargetY)
	{
		// `GATHER_GOAL_RESOURCE[goal]`.
		FString Resource;
		if (Goal == TEXT("gatherWood")) Resource = TEXT("wood");
		else if (Goal == TEXT("gatherStone")) Resource = TEXT("stone");
		else if (Goal == TEXT("gatherFood")) Resource = TEXT("food");
		else return false;
		// `extractionPostFor` : poste acheve, dont la ressource brute produite est celle-ci.
		if (WorkplaceType.IsEmpty() || WorkplaceProgress < 1.0) return false;
		FString PostResource;
		if (WorkplaceType == TEXT("farm") || WorkplaceType == TEXT("fishery")) PostResource = TEXT("food");
		else if (WorkplaceType == TEXT("lumbercamp")) PostResource = TEXT("wood");
		else if (WorkplaceType == TEXT("quarry")) PostResource = TEXT("stone");
		if (PostResource != Resource) return false;
		// `withinCourt(post, target.x, target.y, EXTRACTION_COURT.arriveSlack)`.
		if (!bHasTarget || !FMath::IsFinite(TargetX) || !FMath::IsFinite(TargetY)) return false;
		return AnastasisMath::JsHypot(TargetX - (WorkplaceX + 0.5), TargetY - (WorkplaceY + 0.5)) <= 3.5 + 0.75;
	}
}
