// Le coup rate — mission chat-on-haul-001.
//
// Reference `src/sim/craftMiss.js` (tag anastasis-ref-p3) : un geste de metier rate rarement
// (~1 sur 12 a competence 1), sans rendement, et la reprise est un peu plus lente. Le tirage
// est fait dans le flux PARTAGE (`sim.rng`), seulement si la porte (`canRollCraftMiss`) l'ouvre
// et que la chance est positive.
//
// Generique : le profil (`craftId`) dit si le geste est manquable et de quelle sorte. Le village
// l'appelle pour les boucles portees (cueillette du fermier `farm`, chantier `build`) ; les autres
// profils (`chop`, `quarry`, `tend`…) n'ont plus qu'a appeler `FVillage::RollCraftMiss`.
//
// Pas porte : la maitrise des techniques (`bestCraftMastery`, life/techniques.js) — aucun habitant
// C++ n'a de technique, la reference lit 0 pour un habitant sans `techniques` (ecart n°10). Elle
// reste un parametre ici, prouve par les vecteurs.

#pragma once

#include "CoreMinimal.h"

namespace AnastasisCraftMiss
{
	/** `CRAFT_MISS`. */
	inline constexpr double BaseChance = 1.0 / 12.0;
	inline constexpr double Cooldown = 9.5;
	inline constexpr double Ttl = 1.35;
	inline constexpr double RecoveryMul = 1.38;
	inline constexpr double MasteryGuard = 1.6;

	/** `craftMissKindFor(craftId)` : "glance", "slip", "whiff", ou vide si le geste ne rate jamais. */
	ANASTASISSIM_API FString MissKindFor(const FString& CraftId);

	/** `craftFatigueMissMul(npc)` = `craftFatigueOf(npc).missMul`, sur la session en cours. */
	ANASTASISSIM_API double FatigueMissMul(int32 SwingsDone, double Energy);

	/**
	 * `craftMissChance(npc, craftId)` : `(baseChance / (soft * hand)) * missMul`, avec
	 * `soft = 1 + max(0, skill - 1) * 0.55` (`Number(skill) || 1`) et `hand = 1 + mastery * 1.6`.
	 */
	ANASTASISSIM_API double MissChance(const FString& CraftId, double Skill, double Mastery, int32 SwingsDone, double Energy);

	/**
	 * `canRollCraftMiss(sim, npc, craftId)` : geste manquable, et les deux horloges du dernier rate
	 * (`npc.craftMissAt`, `npc.craftMiss.at`, absentes = 0) a plus de 9,5 s.
	 */
	ANASTASISSIM_API bool CanRoll(const FString& CraftId, double Now, double CraftMissAt, double LastMissAt);
}
