// Rythme de Valmire — portage de `src/life/villageRhythm.js` (phases et biais).
//
// Les besoins disent CE QU'un habitant ressent ; le rythme dit QUAND le village
// penche vers le travail, le repas ou le sommeil. C'est lui, pas la fatigue
// seule, qui couche les habitants la nuit : `phaseBias(rest)` vaut ~+91 pour
// qui a un toit, la ou `needs.rest` d'un habitant repose vaut 0.
//
// Porte, et prouve par vecteurs (`Anastasis.Sim.Parite.Rythme`) :
//   villagePhase (heure civile -> phase), isNightPhase, phaseBias (tous les buts).
//
// Mode de vie (lifestyle) non porte : `personalFrac` vaut la fraction du jour
// commune, `isNocturnalWanderer` est faux (ni noctambule, ni voleur, ni rancune :
// ces etats n'existent pas encore dans le portage). Pas de gardes, pas d'enfants.

#pragma once

#include "CoreMinimal.h"

namespace AnastasisRhythm
{
	/** `DAY_LENGTH` — secondes de simulation par jour. */
	inline constexpr double DayLength = 90.0;

	enum class EPhase : uint8
	{
		Night,
		Dawn,
		Morning,
		Midday,
		Afternoon,
		Evening,
	};

	/** Identifiant de la reference ("night", "dawn"...). */
	ANASTASISSIM_API const TCHAR* PhaseId(EPhase Phase);

	/** `PHASES[id].work` — facteur de travail de la phase. */
	ANASTASISSIM_API double PhaseWork(EPhase Phase);

	/**
	 * `phaseWorkFactor(sim, npc)` a phase PERSONNELLE donnee (`villagePhaseFor`) : garde la nuit 1,15,
	 * aubergiste le soir et la nuit 0,85, puis les modes de vie (bourreau de travail, noctambule, matinal).
	 * Identifiants vides = absents.
	 */
	ANASTASISSIM_API double PhaseWorkFactor(EPhase PersonalPhase, const FString& JobId, const FString& LifestyleId);

	/** `dayFracOf(sim)` = `(time % DAY_LENGTH) / DAY_LENGTH`. */
	ANASTASISSIM_API double DayFracOf(double Time);

	/** `villagePhase(frac)` — heure civile = frac * 24, nuit 21h-5h. */
	ANASTASISSIM_API EPhase VillagePhase(double Frac);

	/** `isNightPhase(sim)` — sur la phase COMMUNE, jamais la phase personnelle. */
	ANASTASISSIM_API bool IsNightPhase(double Time);

	/** Ce que `phaseBias` lit de l'habitant. */
	struct FPhaseSubject
	{
		/** `npc.home || npc.shelter`. */
		bool bHasHomeOrShelter = false;
		double Energy = 100.0;
		double Hunger = 0.0;
		bool bGuard = false;
		bool bYoung = false;
		bool bHasFamily = false;
		/** Metier `innkeeper` ou `merchant` : le soir ne penalise pas leur travail. */
		bool bEveningTrade = false;
		bool bNocturnalWanderer = false;
	};

	/** `WORK_GOALS` de villageRhythm.js. */
	ANASTASISSIM_API bool IsWorkGoal(const FString& Goal);

	/** `LIFE_GOALS` de villageRhythm.js. */
	ANASTASISSIM_API bool IsLifeGoal(const FString& Goal);

	/** `phaseBias(sim, npc, goal)` a phase donnee (celle de `villagePhaseFor`). */
	ANASTASISSIM_API double PhaseBias(EPhase Phase, const FPhaseSubject& Subject, const FString& Goal);
}
