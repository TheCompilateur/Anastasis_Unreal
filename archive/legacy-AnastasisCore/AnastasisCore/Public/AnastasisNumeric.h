#pragma once

#include "CoreMinimal.h"

/**
 * Coercitions numeriques du contrat ANASTASIS, portees a l'identique.
 *
 * Source : `anastasis-world-contract.js:8-16` et `anastasis-kernel.js:22-32`.
 *
 * POURQUOI CES FONCTIONS EXISTENT AU LIEU D'UN CAST DIRECT.
 * Le contrat JS ne fait jamais confiance a une valeur entrante : chaque champ
 * traverse `finiteNumber` / `stringOrNull` avant d'entrer dans l'instantane,
 * de sorte qu'un `undefined` ou un `NaN` cote simulation devienne 0 plutot
 * que de contaminer tout ce qui lit ce champ plus loin. Cote C++ le typage
 * enleve une partie du risque, mais PAS quand la valeur arrive de JSON : un
 * champ absent ou un `null` traverse exactement le meme chemin. Ces fonctions
 * sont donc le point ou l'invariant reste verifiable, et non une precaution
 * ceremoniale.
 *
 * Tout ecart de comportement avec la version JS est une divergence de parite
 * [SCN], pas un detail d'implementation : les modifier demande de mesurer,
 * pas de raisonner.
 */
namespace Anastasis
{
	/** `finiteNumber(value, fallback)` — anastasis-world-contract.js:8. */
	FORCEINLINE double FiniteNumber(double Value, double Fallback = 0.0)
	{
		return FMath::IsFinite(Value) ? Value : Fallback;
	}

	/** `Math.max(0, Math.floor(Number(value) || 0))` sur un compteur entier. */
	FORCEINLINE int32 NonNegativeInt(double Value, double Fallback = 0.0)
	{
		const double Floored = FMath::FloorToDouble(FiniteNumber(Value, Fallback));
		const double Clamped = FMath::Clamp(Floored, 0.0, static_cast<double>(MAX_int32));
		return static_cast<int32>(Clamped);
	}

	/** `Math.min(1, Math.max(0, v))` — borne de progression et de fraction de jour. */
	FORCEINLINE double Clamp01(double Value)
	{
		return FMath::Min(1.0, FMath::Max(0.0, Value));
	}

	/**
	 * `Math.round` de JavaScript : la moitie va vers +Inf.
	 *
	 * `FMath::RoundToInt` arrondit la moitie EN S'ELOIGNANT DE ZERO, ce qui
	 * diverge sur les negatifs (-0.5 donne 0 en JS, -1 avec FMath). Les seuls
	 * appelants actuels passent des valeurs positives, ou les deux coincident,
	 * mais la fonction porte la semantique JS pour que le prochain appelant
	 * n'ait pas a redecouvrir l'ecart.
	 */
	FORCEINLINE double JsRound(double Value)
	{
		return FMath::FloorToDouble(Value + 0.5);
	}

	/**
	 * `Math.hypot(x, y)`.
	 *
	 * RISQUE DE PARITE CONNU, NON MESURE. `Math.hypot` de V8 met les operandes
	 * a l'echelle pour eviter overflow et underflow ; ce `Sqrt` direct ne le
	 * fait pas. Les deux different d'au plus quelques ulp sur les magnitudes
	 * du monde (coordonnees de tuiles, vecteurs de deplacement unitaires), ce
	 * qui est sans effet visible mais N'EST PAS une egalite bit a bit. Si une
	 * porte [SCN] compare un jour des positions au bit pres, c'est ici qu'il
	 * faudra regarder avant d'accuser la simulation.
	 */
	FORCEINLINE double Hypot(double X, double Y)
	{
		return FMath::Sqrt(X * X + Y * Y);
	}
}
