// Le quart de travail — portage de `src/life/workShift.js` (mission reconsider-001).
//
// Une machine a etats par habitant, pilotee par deux evenements :
//
//   OFF_DUTY --commit craft/build/maintain (ou recolte a la cour de son poste)--> COMMUTING
//   COMMUTING --arrivee (ensureWorkSession)--> ON_SHIFT
//   COMMUTING/ON_SHIFT --commit hors famille travail, critique--> BREAK
//   COMMUTING/ON_SHIFT --commit hors famille travail, non critique--> OFF_DUTY
//
// Pendant le PLANCHER (15 s sim, la plus courte phase de travail du rythme), le bouclier
// `shiftShields` tient : seul le palier critique (`needsCritical`) peut faire sortir.
// Il amortit la reconsideration (`committedReconsiderChance`) et verrouille le commit
// (un but de travail sous plancher ne bascule pas hors famille travail).
//
// Aucun tirage. `npc.workShift` N'EST PAS sauvegarde (save.js) : un habitant charge n'a pas
// de quart, des deux cotes du harnais. L'etat absent est `State == None`.

#pragma once

#include "CoreMinimal.h"

namespace AnastasisWorkShift
{
	/** `PHASE_HOUR_START.afternoon` / `.evening` (life/villageRhythm.js). */
	inline constexpr double AfternoonHour = 13.5;
	inline constexpr double EveningHour = 17.5;
	/** `DAY_SECONDS` — jour civil en secondes sim. */
	inline constexpr double DaySeconds = 90.0;
	/** `WORK_SHIFT.floorSeconds` = `((evening - afternoon) / 24) * 90`. */
	inline constexpr double FloorSeconds = ((EveningHour - AfternoonHour) / 24.0) * DaySeconds;

	/** `SHIFT_STATES` ; `None` = `npc.workShift` absent. */
	enum class EState : uint8
	{
		None,
		OffDuty,
		Commuting,
		OnShift,
		Break,
	};

	/** Identifiant JS de l'etat (`"COMMUTING"`...), vide pour `None`. */
	ANASTASISSIM_API const TCHAR* StateId(EState State);

	/** `npc.workShift`. */
	struct FWorkShift
	{
		EState State = EState::None;
		/** Vide = `null`. */
		FString Goal;
		double StartedAt = 0.0;
		double FloorUntil = 0.0;
	};

	/** `SHIFT_ENTRY_GOALS`. */
	ANASTASISSIM_API bool IsShiftEntryGoal(const FString& Goal);

	/** `SHIFT_WORK_FAMILY` = `WORK_GOALS` + `fetchInput`. */
	ANASTASISSIM_API bool IsShiftWorkFamily(const FString& Goal);

	/**
	 * `noteShiftGoalCommit(sim, npc)` — au commit d'un but. `bEntry` = `shiftEntryCommitted(npc, goal)` :
	 * un but d'atelier, ou une recolte ancree a la cour de son poste d'extraction
	 * (`AnastasisWorkShift::OpensExtractionShift`).
	 */
	ANASTASISSIM_API void NoteShiftGoalCommit(FWorkShift& Shift, const FString& Goal, double Now, bool bCritical, bool bEntry);

	/** `noteShiftArrival(sim, npc)` — COMMUTING -> ON_SHIFT a la creation d'une session de travail. */
	ANASTASISSIM_API void NoteShiftArrival(FWorkShift& Shift);

	/** `shiftShields(sim, npc)`. */
	ANASTASISSIM_API bool ShiftShields(const FWorkShift& Shift, const FString& Goal, double Now, bool bCritical);

	/**
	 * `opensExtractionShift(npc, goal)` : une recolte dont la cible est dans la cour (3,5 + 0,75)
	 * du poste d'extraction ACHEVE de l'habitant, pour la ressource de ce but. Postes :
	 * `farm`, `fishery` (nourriture), `lumbercamp` (bois), `quarry` (pierre) — role `production`
	 * du catalogue. Le grenier n'en est pas un.
	 */
	ANASTASISSIM_API bool OpensExtractionShift(
		const FString& Goal,
		const FString& WorkplaceType,
		double WorkplaceProgress,
		double WorkplaceX,
		double WorkplaceY,
		bool bHasTarget,
		double TargetX,
		double TargetY);
}
