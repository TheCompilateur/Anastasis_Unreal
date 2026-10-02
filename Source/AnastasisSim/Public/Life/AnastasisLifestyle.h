// Mode de vie d'un habitant — portage de `src/sim/lifestyle.js`.
//
// Six facons de vivre (leve-tot, noctambule, travailleur acharne, flaneur, parent
// present, habitue de taverne), tirees une fois, stables. Elles penchent les choix
// (`LifestyleBias`), la vitesse de marche (`LifestyleTravelFactor`), la duree d'une
// activite a l'interieur (`LifestyleIndoorDuration`), la destination
// (`LifestyleTargetBuilding`), la memoire des lieux (`LifestyleNotePlaceUse`), et
// tiennent un score de regularite jour par jour (`LifestyleDailyUpdate`).
//
// MODULE SEUL : ni `FNpc` ni `UpdateNpc` ne le lisent encore.
//
// --- CE QUI DECIDE --------------------------------------------------------------
//
// `ensureLifestyle` TIRE dans le flux qu'on lui donne si l'habitant n'a pas de mode
// de vie (ou un identifiant inconnu) : un tirage, `rng() * total`. Dans `updateNpc`
// il passe AVANT la cadence, avec `sim.rng` — donc a chaque tick, meme quand
// l'habitant ne pense pas. Sans flux (`rng` null), la reference puise dans
// `fallbackRng`, un flux GLOBAL partage (`GetAnastasisFallbackRng`). Les fonctions
// d'ici prennent donc un `FAnastasisRng*` : nullptr = le flux de secours, comme le JS.
//
// L'ordre des identifiants (`Object.keys(LIFESTYLES)`) decide du tirage : la roue
// les parcourt dans cet ordre. Il est copie tel quel dans `ELifestyle`.
//
// La phase du mode de vie n'est PAS la phase du village : `dawn` y devient
// `morning` et `afternoon` devient `day` (`dayPhase`).
//
// Un argument `DayFrac` remplace `typeof sim?.dayFrac === "function" ? sim.dayFrac() : 0.5` :
// l'appelant passe 0,5 quand il n'a pas d'horloge, comme la reference.
//
// --- CE QUI N'EST PAS ICI ---------------------------------------------------------
//
// - `lifestyleLabel`, `lifestyleColor` : presentation (texte et couleur affiches).
//   La table des descripteurs est portee (`LifestyleForId`), leurs deux formatages non.
// - `sim.buildingAccessPoint` : `LifestyleTargetBuilding` rend le BATIMENT choisi ;
//   l'appelant calcule le point d'acces exactement quand la reference l'appelle, une
//   fois, et seulement si un batiment est rendu.

#pragma once

#include "CoreMinimal.h"

struct FAnastasisRng;

namespace AnastasisLifestyle
{
	/** `Object.keys(LIFESTYLES)`, dans l'ordre de la reference : la roue du tirage le suit. */
	enum class ELifestyle : uint8
	{
		EarlyBird = 0,
		NightOwl,
		Workhorse,
		Wanderer,
		FamilyFirst,
		TavernRegular,
		Count
	};

	inline constexpr int32 NumLifestyles = static_cast<int32>(ELifestyle::Count);

	/** Identifiant JS (`"earlyBird"`...). */
	ANASTASISSIM_API const TCHAR* LifestyleId(ELifestyle Lifestyle);

	/** Identifiant JS -> mode de vie ; false si inconnu (`!LIFESTYLES[id]`). */
	ANASTASISSIM_API bool LifestyleFromId(const FString& Id, ELifestyle& Out);

	/** Une entree de `LIFESTYLES`. */
	struct FLifestyleInfo
	{
		const TCHAR* Label;
		const TCHAR* Short;
		const TCHAR* Color;
		const TCHAR* Marker;
	};

	/** `lifestyleForId(id)` — un identifiant inconnu rend le flaneur. */
	ANASTASISSIM_API const FLifestyleInfo& LifestyleForId(const FString& Id);

	/** `npc.lifestyle`, au format de la sauvegarde JS. */
	struct FLifestyle
	{
		/** Identifiant JS, tel qu'il est sauve. */
		FString Id;
		double SinceDay = 1.0;
		double RhythmScore = 0.0;
		double LastNotedDay = 0.0;
	};

	/** Phase du mode de vie (`dayPhase`) : celle du village, aube et apres-midi renommees. */
	enum class EDayPhase : uint8
	{
		Night,
		Morning,
		Midday,
		Day,
		Evening,
	};

	/** `dayPhase(frac)`. */
	ANASTASISSIM_API EDayPhase DayPhase(double Frac);

	/** Identifiant JS de la phase (`"night"`, `"morning"`, `"midday"`, `"day"`, `"evening"`). */
	ANASTASISSIM_API const TCHAR* DayPhaseId(EDayPhase Phase);

	/**
	 * Ce que le module lit sur l'habitant. Chaine vide = champ absent ou `null`.
	 */
	struct FLifestyleSubject
	{
		/** `npc.lifeStage` : "child", "teen", "elder"... */
		FString LifeStage;
		bool bApprenticing = false;
		FString JobId;
		/** `npc.trait?.explore || 0` etc. */
		double TraitExplore = 0.0;
		double TraitBuild = 0.0;
		double TraitTrade = 0.0;
		FString FamilyId;
		FString PartnerId;
		int32 ChildCount = 0;
		/** `npc.home` — son identifiant ; vide = sans maison. */
		FString HomeId;
		/** `npc.skill || 0`. */
		double Skill = 0.0;
		/** `npc.energy || 100`. */
		double Energy = 100.0;
		FString Goal;
		/** `npc.target` non nul. */
		bool bHasTarget = false;
		/** `npc.placeMemory?.favoriteBuildingId`. */
		FString FavoriteBuildingId;
	};

	/**
	 * `assignLifestyle(rng, npc, preferred)` — UN tirage dans `Rng` (nullptr = flux de
	 * secours). `Preferred` vide ou inconnu = sans preference.
	 */
	ANASTASISSIM_API FLifestyle AssignLifestyle(FAnastasisRng* Rng, const FLifestyleSubject& Npc, const FString& Preferred = FString());

	/**
	 * `ensureLifestyle(npc, rng)` — tire si le mode de vie manque ou porte un
	 * identifiant inconnu, sinon le rend tel quel. Les `??=` de la reference
	 * (champs absents d'une vieille sauvegarde) sont l'affaire du lecteur : un
	 * `FLifestyle` a toujours ses quatre champs.
	 */
	ANASTASISSIM_API FLifestyle& EnsureLifestyle(TOptional<FLifestyle>& Lifestyle, const FLifestyleSubject& Npc, FAnastasisRng* Rng);

	/** `lifestyleBias(sim, npc, goal)` — points de score, pour un but candidat. */
	ANASTASISSIM_API double LifestyleBias(
		TOptional<FLifestyle>& Lifestyle,
		const FLifestyleSubject& Npc,
		FAnastasisRng* SimRng,
		double DayFrac,
		const FString& Goal);

	/** `lifestyleTravelFactor(sim, npc)` — multiplicateur de vitesse de marche. */
	ANASTASISSIM_API double LifestyleTravelFactor(
		TOptional<FLifestyle>& Lifestyle,
		const FLifestyleSubject& Npc,
		FAnastasisRng* SimRng,
		double DayFrac);

	/**
	 * `lifestyleIndoorDuration(npc, goal, base)`. La reference appelle `ensureLifestyle`
	 * SANS flux : un habitant sans mode de vie tire alors dans le flux de secours.
	 */
	ANASTASISSIM_API double LifestyleIndoorDuration(
		TOptional<FLifestyle>& Lifestyle,
		const FLifestyleSubject& Npc,
		const FString& Goal,
		double Base);

	/** Le monde que `lifestyleTarget` interroge. */
	class ANASTASISSIM_API ILifestyleWorld
	{
	public:
		virtual ~ILifestyleWorld() = default;
		/**
		 * `sim.buildings.find(b => b.type === "tavern" && b.progress >= 1)` —
		 * la PREMIERE dans l'ordre des batiments. Vide = aucune.
		 */
		virtual FString FirstCompletedTavernId() const = 0;
		/** `sim.buildingById?.(id)` non nul. */
		virtual bool HasBuilding(const FString& Id) const = 0;
	};

	/**
	 * `lifestyleTarget(sim, npc, goal, fallback)` — rend l'identifiant du batiment
	 * dont la reference prendrait le point d'acces, ou une chaine vide pour garder
	 * `fallback`.
	 */
	ANASTASISSIM_API FString LifestyleTargetBuilding(
		TOptional<FLifestyle>& Lifestyle,
		const FLifestyleSubject& Npc,
		FAnastasisRng* SimRng,
		const ILifestyleWorld& World,
		const FString& Goal);

	/**
	 * Une entree de `actor.placeMemory.buildings[key]`, reduite a ce que
	 * `lifestyleNotePlaceUse` lit et ecrit.
	 */
	struct FPlaceUseEntry
	{
		/** `entry.buildingId` ; vide = absent. */
		FString BuildingId;
		double Work = 0.0;
		double Social = 0.0;
		double Home = 0.0;
		/** `entry.lifestyle` : objet JS, donc ordonne par premiere insertion. */
		TArray<TPair<FString, double>> Lifestyle;
	};

	/**
	 * `lifestyleNotePlaceUse(actor, kind, entry, amount)`. Sans flux, comme
	 * `lifestyleIndoorDuration`.
	 *
	 * Copie fidele d'une bizarrerie : `actor.home?.id === entry.buildingId` est
	 * VRAI quand l'habitant n'a pas de maison et que l'entree n'a pas de batiment
	 * (`undefined === undefined`). Les deux chaines vides le reproduisent.
	 */
	ANASTASISSIM_API void LifestyleNotePlaceUse(
		TOptional<FLifestyle>& Lifestyle,
		const FLifestyleSubject& Actor,
		const FString& Kind,
		FPlaceUseEntry& Entry,
		double Amount = 1.0);

	/**
	 * `lifestyleDailyUpdate(sim, npc)` — une fois par jour au plus (`lastNotedDay`),
	 * le score de regularite monte de 1 si l'habitant vit selon son mode a cet
	 * instant, descend de 0,2 sinon.
	 */
	ANASTASISSIM_API void LifestyleDailyUpdate(
		TOptional<FLifestyle>& Lifestyle,
		const FLifestyleSubject& Npc,
		FAnastasisRng* SimRng,
		double Day,
		double DayFrac);
}
