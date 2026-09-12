#pragma once

#include "CoreMinimal.h"
#include "Core/AnastasisJsNumeric.h"

#include <limits>

/**
 * Petites fonctions mathematiques partagees par la simulation.
 * Port de src/sim/util.js — meme noms, meme semantique, memes pieges.
 *
 * Tout est en `double`. Pas de `float`: la reference JS calcule en 64 bits, et
 * une simulation qui tourne des heures accumule l'ecart jusqu'a la divergence.
 */
namespace AnastasisMath
{
	inline double Clamp(double V, double Min, double Max)
	{
		// Math.max(min, Math.min(max, v)) — propage NaN comme le JS.
		return FMath::Max(Min, FMath::Min(Max, V));
	}

	/**
	 * Borne [0,1]. PROPAGE NaN, volontairement: un NaN qui traverse signale un
	 * bug de calcul en amont et on veut le voir. Si une entree douteuse doit
	 * retomber a 0 plutot que d'empoisonner le calcul, prendre Clamp01Coerce.
	 */
	inline double Clamp01(double V)
	{
		return FMath::Max(0.0, FMath::Min(1.0, V));
	}

	/**
	 * Meme borne, mais coerce: NaN devient 0. Les deux existent parce que la
	 * moitie du moteur veut voir passer un NaN et l'autre veut un facteur
	 * toujours utilisable (dosages, gains audio). Choisir au point d'appel:
	 * le nom dit lequel on tient.
	 */
	inline double Clamp01Coerce(double V)
	{
		return FMath::Max(0.0, FMath::Min(1.0, AnastasisJs::NumberOrZero(V)));
	}

	inline double Lerp(double A, double B, double T)
	{
		// a + (b - a) * t — surtout pas FMath::Lerp, qui n'a pas la meme
		// forme algebrique et donc pas le meme dernier bit.
		return A + (B - A) * T;
	}

	/**
	 * Transition douce entre deux bornes. L'epsilon 1e-4 protege du bord
	 * degenere Edge1 == Edge0, MAIS il n'est pas cosmetique: sur un ecart de
	 * bornes entre 1e-6 et 1e-4 il change le resultat (jusqu'a x69 face a un
	 * epsilon de 1e-6). Dix modules JS gardent volontairement une autre
	 * variante — pas d'epsilon, epsilon 1e-6, ou clamp coercitif. Ne pas les
	 * aligner sur celle-ci sans mesurer.
	 */
	inline double Smoothstep(double Edge0, double Edge1, double Value)
	{
		const double T = Clamp01((Value - Edge0) / FMath::Max(0.0001, Edge1 - Edge0));
		return T * T * (3.0 - 2.0 * T);
	}

	/**
	 * Meme courbe sur un T DEJA normalise. Nom distinct parce que Smoothstep(t)
	 * a un argument et Smoothstep(e0, e1, x) a trois ne sont pas la meme
	 * fonction: les confondre fait passer un Edge0 pour un T, en silence.
	 */
	inline double Smoothstep01(double T)
	{
		const double U = Clamp01(T);
		return U * U * (3.0 - 2.0 * U);
	}

	/**
	 * Math.hypot de V8, reproduit pas a pas (mise a l'echelle par le max +
	 * sommation compensee de Kahan). std::hypot et sqrt(dx*dx+dy*dy) donnent
	 * des resultats a un ulp pres DIFFERENTS: sur un test `Dist(a,b) < Radius`
	 * pile au bord, cet ulp fait basculer la branche. La sim compare des
	 * distances a longueur de tick — on garde l'algorithme de la reference.
	 */
	inline double JsHypot(double X, double Y)
	{
		// JS: un +-Infinity l'emporte, meme face a un NaN sur l'autre argument.
		// Un NaN seul, lui, se propage tout seul dans l'arithmetique qui suit.
		const bool bXInfinite = !FMath::IsFinite(X) && !FMath::IsNaN(X);
		const bool bYInfinite = !FMath::IsFinite(Y) && !FMath::IsNaN(Y);
		if (bXInfinite || bYInfinite)
		{
			return std::numeric_limits<double>::infinity();
		}

		const double AbsX = FMath::Abs(X);
		const double AbsY = FMath::Abs(Y);
		double Max = FMath::Max(AbsX, AbsY);
		if (Max == 0.0)
		{
			Max = 1.0;
		}

		double Sum = 0.0;
		double Compensation = 0.0;
		const double Terms[2] = { AbsX / Max, AbsY / Max };
		for (const double Term : Terms)
		{
			const double Summand = Term * Term - Compensation;
			const double Preliminary = Sum + Summand;
			Compensation = (Preliminary - Sum) - Summand;
			Sum = Preliminary;
		}
		return FMath::Sqrt(Sum) * Max;
	}

	/** Distance planaire entre deux points de simulation (plan XY, pas XZ). */
	inline double Dist(double AX, double AY, double BX, double BY)
	{
		return JsHypot(AX - BX, AY - BY);
	}

	/**
	 * Hash FNV-1a d'une chaine, ramene dans [0,1]. Comme Hash2d, il sert a fixer
	 * du procedural.
	 *
	 * Cote JS, deux modules gardent une variante qui fait String(text || "")
	 * d'abord; celle-ci JETTE sur null/undefined, volontairement — un
	 * identifiant absent est un bug qu'on veut voir. Le typage C++ rend ce
	 * garde-fou inutile ici: il n'y a pas de FString "absente", et la chaine
	 * vide est une entree legitime qui a sa propre valeur de hash.
	 *
	 * Itere sur les TCHAR, qui sont des unites UTF-16 sur les plateformes
	 * cibles — donc exactement ce que rend charCodeAt, surrogates compris.
	 */
	ANASTASISSIM_API double HashText(const FString& Text, int32 Channel = 0);

	/**
	 * Bruit procedural deterministe par tuile.
	 *
	 * DANGER (repris du JS, toujours vrai ici): neuf modules gardent une variante
	 * locale qui rend des valeurs differentes de 1,5e-5 a 3e-5 — l'une remplace
	 * le Math.imul par un `*`, une autre saute le melange final, une derniere est
	 * a base de sin. L'ecart est invisible a l'oeil MAIS un
	 * Math.floor(hash2d(...) * n) bascule de categorie au bord: ce sont des
	 * arbres, des roches et des herbes qui se deplacent. Ne migrer une variante
	 * vers cette fonction qu'apres avoir mesure l'egalite STRICTE.
	 *
	 * X et Y sont des doubles, pas des entiers: le JS accepte des coordonnees
	 * fractionnaires et la premiere ligne est une somme en double AVANT le
	 * repliage 32 bits. Passer par des int ici changerait les valeurs.
	 */
	ANASTASISSIM_API double Hash2d(double X, double Y, int32 Channel = 0);
}
