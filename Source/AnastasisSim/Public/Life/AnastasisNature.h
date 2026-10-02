#pragma once

// La nature d'un habitant — `src/life/nature.js` (mission lifestyle-decision-001).
//
// Trois attributs (corps, esprit, coeur ; 1 = moyen), deux qualites et un defaut au plus. Ce module
// porte ce que la DECISION en lit : le penchant par but (`natureGoalBias`), le facteur de travail
// (`natureWorkFactor`) et le collant (`natureStickBonus`). Les fonctions lisent la nature telle que
// `ensureNature` la rend (`Normalized`). Le tirage d'une nature absente (`rollNature`, flux de
// secours), l'heritage, l'apprentissage et les penchants sociaux ne sont pas portes (ecart n°10).
//
// Parite : `Anastasis.Sim.Parite.Nature` (vecteurs de tools/migration/parity/nature.mjs).

#include "CoreMinimal.h"

namespace AnastasisNature
{
	/** `NATURE` : bornes des attributs, poussee des penchants. */
	inline constexpr double AttrMin = 0.72;
	inline constexpr double AttrMax = 1.38;
	inline constexpr double GoalPush = 10.0;
	inline constexpr int32 QualitySlots = 2;
	inline constexpr int32 FlawSlots = 1;

	/** `npc.nature`. */
	struct FNature
	{
		double Corps = 1.0;
		double Esprit = 1.0;
		double Coeur = 1.0;
		TArray<FString> Qualities;
		TArray<FString> Flaws;
	};

	/** Une qualite connue du catalogue (`QUALITIES`). */
	ANASTASISSIM_API bool IsQuality(const FString& Id);
	/** Un defaut connu du catalogue (`FLAWS`). */
	ANASTASISSIM_API bool IsFlaw(const FString& Id);

	/**
	 * `ensureNature(npc)` sur une nature presente : attributs `clamp(Number(x) || 1, 0,72, 1,38)`,
	 * qualites et defauts inconnus retires, puis tronques a deux qualites et un defaut.
	 */
	ANASTASISSIM_API FNature Normalized(const FNature& Nature);

	/** `natureGoalBias(npc, goal)` : penchants des qualites et defauts, signatures des attributs, x 10. */
	ANASTASISSIM_API double NatureGoalBias(const FNature& Nature, const FString& Goal);

	/** `natureWorkFactor(npc)` : `clamp(0,92 + corps x 0,08 + travail, 0,72, 1,22)`. */
	ANASTASISSIM_API double NatureWorkFactor(const FNature& Nature);

	/** `natureStickBonus(npc, goal)` : le but n'est pas lu. */
	ANASTASISSIM_API double NatureStickBonus(const FNature& Nature);
}
