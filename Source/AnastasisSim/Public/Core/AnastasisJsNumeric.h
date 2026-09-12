#pragma once

#include "CoreMinimal.h"

/**
 * Semantique numerique JavaScript, reproduite a l'identique.
 *
 * POURQUOI CE FICHIER EXISTE. Le simulateur de reference est ecrit en JS: tout
 * nombre y est un double IEEE754, et les operateurs binaires (`|0`, `>>> 0`,
 * `^`, `<<`, Math.imul) commencent par CONVERTIR ce double en entier 32 bits
 * selon ECMA-262. Un portage "raisonnable" en C++ ecrit `(uint32)(x * 374761393)`
 * et croit avoir traduit — mais si x vaut 3.5, JS calcule 1311664875.5 en double
 * PUIS tronque, alors que le C++ naif a deja perdu la partie fractionnaire ou
 * deborde en UB. Le hash bascule de categorie, et ce sont des arbres, des roches
 * et des herbes qui se deplacent (cf. le commentaire de hash2d dans util.js).
 *
 * Regle: tout endroit ou le JS ecrit `>>> 0` ou `| 0` sur une expression qui
 * n'est pas deja un entier 32 bits sur passe par ToUint32 / ToInt32 ici.
 */
namespace AnastasisJs
{
	/** 2^32, en double exact. */
	inline constexpr double UInt32Modulo = 4294967296.0;

	/**
	 * ECMA-262 ToUint32 (l'operateur `>>> 0` de JavaScript).
	 * NaN, +-Inf et +-0 donnent 0; sinon troncature vers zero puis modulo 2^32.
	 */
	inline uint32 ToUint32(double Value)
	{
		if (!FMath::IsFinite(Value) || Value == 0.0)
		{
			return 0u;
		}
		// Troncature vers zero (ToIntegerOrInfinity), puis repliage modulo 2^32.
		const double Truncated = FMath::TruncToDouble(Value);
		double Wrapped = FMath::Fmod(Truncated, UInt32Modulo);
		if (Wrapped < 0.0)
		{
			Wrapped += UInt32Modulo;
		}
		return static_cast<uint32>(Wrapped);
	}

	/** ECMA-262 ToInt32 (l'operateur `| 0`). Meme repliage, lu en signe. */
	inline int32 ToInt32(double Value)
	{
		return static_cast<int32>(ToUint32(Value));
	}

	/**
	 * Math.imul: produit 32 bits signe, tronque, rendu ici en uint32.
	 * En C++ le produit d'uint32 enroule deja modulo 2^32 sans UB — c'est
	 * exactement le comportement voulu. La fonction existe pour que le portage
	 * reste lisible en regard du JS d'origine.
	 */
	inline uint32 Imul(uint32 A, uint32 B)
	{
		return A * B;
	}

	/** Decalage logique a droite (`>>>`), sur une valeur deja en 32 bits. */
	inline uint32 Shru(uint32 Value, uint32 Bits)
	{
		return Value >> Bits;
	}

	/**
	 * Coercition `Number(v) || 0` appliquee a un double deja parse.
	 * Seul NaN retombe a 0: +-Inf reste truthy cote JS et se fera borner par le
	 * clamp appelant. Voir clamp01Coerce dans src/sim/util.js.
	 */
	inline double NumberOrZero(double Value)
	{
		return FMath::IsNaN(Value) ? 0.0 : Value;
	}

	/**
	 * `Number(v) || Fallback` pour les gardes du type `Number(scale) || 1`.
	 * JS retombe sur Fallback pour NaN **et pour 0** (0 est falsy). Cette
	 * distinction compte: simStepPlan(0) rend 1x, pas une division par zero.
	 */
	inline double NumberOr(double Value, double Fallback)
	{
		return (FMath::IsNaN(Value) || Value == 0.0) ? Fallback : Value;
	}

	/**
	 * Math.round ECMA-262: nearest integer, ties toward +Infinity.
	 * std::round fait "away from zero" (Math.round(-1.5) JS=-1, C++=-2).
	 */
	inline double Round(double Value)
	{
		if (!FMath::IsFinite(Value) || Value == 0.0)
		{
			return Value;
		}
		if (Value < 0.0 && Value >= -0.5)
		{
			return -0.0;
		}
		return FMath::FloorToDouble(Value + 0.5);
	}

	/** Math.floor — vers -Inf. */
	inline double Floor(double Value)
	{
		return FMath::FloorToDouble(Value);
	}

	inline double Round3(double Value)
	{
		return Round(Value * 1000.0) / 1000.0;
	}

	/** Stockage Float32Array JS: le calcul est en double, la case est f32. */
	inline float StoreF32(double Value)
	{
		return static_cast<float>(Value);
	}

	inline double LoadF32(float Value)
	{
		return static_cast<double>(Value);
	}

	/**
	 * Math.sin / log / pow / tanh. CRT MSVC n'est pas norme-identique a V8.
	 * Les tests monde mesurent l'ecart; remplacer ici si la parite casse.
	 */
	inline double Sin(double Value) { return FMath::Sin(Value); }
	inline double Log(double Value) { return FMath::Loge(Value); }
	inline double Pow(double Base, double Exp) { return FMath::Pow(Base, Exp); }
	inline double Tanh(double Value) { return FMath::Tanh(Value); }
}
