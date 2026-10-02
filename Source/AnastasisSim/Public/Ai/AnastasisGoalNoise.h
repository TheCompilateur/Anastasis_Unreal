// Bruit de decision — `goalNoise` de `src/sim/npc.js` (mission sim-rng-001).
//
//   function goalNoise(sim, amp = 10) { return sim.rng() * amp * GOAL_AI.noiseScale; }
//
// UN tirage du flux partage `sim.rng` par appel, quelle que soit l'amplitude.
// L'ordre des operations est celui du JS : `(r * amp) * noiseScale`.
//
// PAS BRANCHE. Le village C++ ne tire pas encore ces bruits dans sa table de buts
// (ecart n°1, `AnastasisVillage.h`). Le releve mesure de la reference
// (`docs/migration/phase3/P3_RNG_RELEVE.md`) montre pourquoi on ne branche pas
// la table seule : avant ses quatorze bruits, `adultScores` tire deja
// `exploreTarget` (ligne `explore` de `failureTargetBiasMap`), un nombre de fois
// qui depend de la memoire des cases de l'habitant. Brancher la table sans lui
// coderait un faux ordre. La suite est la mission perception-explore-001.
//
// Ce qui est ici, pret pour ce branchement : la fonction, prouvee au bit pres
// (`Anastasis.Sim.Parite.BruitDeBut`), et la table des bruits de `adultScores`
// dans l'ordre des tirages, conditions comprises.

#pragma once

#include "CoreMinimal.h"

struct FAnastasisRng;

namespace AnastasisGoalNoise
{
	/** `GOAL_AI.noiseScale`. */
	inline constexpr double NoiseScale = 0.42;

	/** `goalNoise(sim, amp)` — un tirage de `Rng`. */
	ANASTASISSIM_API double GoalNoise(FAnastasisRng& Rng, double Amp = 10.0, double Scale = NoiseScale);

	/** Quand une ligne de la table tire son bruit. */
	enum class ENoiseCondition : uint8
	{
		/** Toujours. */
		Always,
		/** `helpFarmScore` : `if (!hasFarm) return 0;` avant le bruit (ferme, ou batiment `nourrir`). */
		HasFarm,
		/** `craftScore` : `countBuildingsWith(fabriquer || produces.tools) <= 0` sort avant le bruit. */
		HasCraftBuilding,
		/** `familyVisitScore` : maison, famille, et au moins un parent ; sinon `return 0` avant le bruit. */
		HasFamilyToVisit,
	};

	/** Un bruit de la table de `adultScores`, dans l'ordre des tirages. */
	struct FTableNoise
	{
		const TCHAR* Goal;
		double Amp;
		ENoiseCondition Condition;
		/** Ligne de `src/sim/npc.js` (tag anastasis-ref-p3) qui appelle `goalNoise`. */
		int32 SourceLine;
	};

	/**
	 * Les bruits de la table `adultScores` (`npc.js` l. 1112-1142), dans l'ordre ou
	 * ils tirent : celui des elements du tableau, puis, dans une ligne, l'ordre des
	 * termes. Quatorze inconditionnels, trois conditionnels.
	 *
	 * Ce sont les tirages de la TABLE seulement. Ceux de la preparation (avant la
	 * table : `failureTargetBiasMap` > `exploreTarget`, `spatialRiskBiasMap`,
	 * intention du jour, ambition) et d'apres (`assignTarget`) n'y sont pas.
	 */
	ANASTASISSIM_API TConstArrayView<FTableNoise> AdultTableNoises();
}
