// Repousse des champs — `Simulation.regrowFieldsDaily`, `regrowFieldTile`
// (src/sim/simulation.js) et `fieldSeasonRegenAmount`, `rotateFieldCropId`,
// `ensureFieldCropReady` (src/sim/fieldCrops.js), reference `fee66ae`.
//
// Chaque nuit, pour chaque champ sous le plafond de 37 : un tirage deterministe
// (x, y, jour) contre la chance de la saison ; s'il passe, le champ regagne
// `3 x regen de saison x fertilite` (au moins 1), et une jachere repart en
// culture (grain, legumes, fruits). Prouve par `Anastasis.Sim.Parite.Repousse`.
//
// Le village applique ce qui suit a l'etat VIVANT des tuiles (LiveTileAt) ;
// l'hote l'appelle en tete des travaux differes de minuit (`landRegen`).

#pragma once

#include "CoreMinimal.h"
#include "World/AnastasisWorld.h"

namespace AnastasisFields
{
	/** `Simulation.FIELD_FOOD_CAP`, `FIELD_REGEN_PER_DAY`. */
	inline constexpr int32 FieldFoodCap = 37;
	inline constexpr int32 FieldRegenPerDay = 3;

	/** `FIELD_SEASON_YIELD[saison].dailyChance`. */
	ANASTASISSIM_API double DailyChance(int32 Day);

	/** `fieldSeasonRegenAmount(base, day)` : max(0, round(base x regen)). */
	ANASTASISSIM_API int32 RegenAmount(int32 Base, int32 Day);

	/**
	 * Le tirage de `regrowFieldsDaily` : somme en double, ToUint32, xor-decalage,
	 * puis produit en DOUBLE (pas `Math.imul` : au-dela de 2^53 les bits bas
	 * s'arrondissent, et la reference le fait ainsi) ; pas de melange final.
	 */
	ANASTASISSIM_API double RegrowRoll(int32 X, int32 Y, int32 Day);

	/** `hash2d` de fieldCrops.js — meme produit en double, puis melange final. */
	ANASTASISSIM_API double FieldCropHash(int32 X, int32 Y, int32 Channel);

	/** `rotateFieldCropId("fallow", x, y, day)` : sortie de jachere. */
	ANASTASISSIM_API AnastasisWorld::ECropId CropAfterFallow(int32 X, int32 Y, int32 Day);

	/**
	 * `regrowFieldTile(tile, amount)` : rend true si la tuile a repousse.
	 * Day sert au tirage de culture (`ensureFieldCropReady(tile, this.day)`).
	 */
	ANASTASISSIM_API bool RegrowFieldTile(AnastasisWorld::FTile& Tile, int32 Amount, int32 Day);

	/** Le corps de boucle de `regrowFieldsDaily` pour UNE tuile. */
	ANASTASISSIM_API bool RegrowTileDaily(AnastasisWorld::FTile& Tile, int32 Day);
}
