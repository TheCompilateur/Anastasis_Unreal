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
	/**
	 * Math.exp, bit pour bit : V8 n'appelle pas le CRT, il execute l'exp de fdlibm
	 * (e_exp.c, base/ieee754.cc). Le CRT MSVC en differe d'1 ulp sur certaines entrees,
	 * et weather.js le montre : `cover = base + amp * (lobe * 1.2 - 0.42)` soustrait deux
	 * grandeurs voisines et amplifie cet ulp en 8 sur un resultat de 0.0046. Reproduire
	 * l'algorithme, pas elargir la tolerance.
	 *
	 * Portage de fdlibm __ieee754_exp (Sun Microsystems, 1993 : "Permission to use, copy,
	 * modify, and distribute this software is freely granted, provided that this notice
	 * is preserved."), branches et constantes a l'identique ; la mise a l'echelle finale
	 * multiplie par 2^k comme V8 (exacte hors sous-normaux).
	 */
	inline double Exp(double X)
	{
		constexpr double One = 1.0;
		constexpr double Huge = 1.0e+300;
		constexpr double TwoM1000 = 9.33263618503218878990e-302;
		constexpr double OThreshold = 7.09782712893383973096e+02;
		constexpr double UThreshold = -7.45133219101941108420e+02;
		constexpr double Ln2Hi[2] = { 6.93147180369123816490e-01, -6.93147180369123816490e-01 };
		constexpr double Ln2Lo[2] = { 1.90821492927058770002e-10, -1.90821492927058770002e-10 };
		constexpr double HalF[2] = { 0.5, -0.5 };
		constexpr double InvLn2 = 1.44269504088896338700e+00;
		constexpr double P1 = 1.66666666666666019037e-01;
		constexpr double P2 = -2.77777777770155933842e-03;
		constexpr double P3 = 6.61375632143793436117e-05;
		constexpr double P4 = -1.65339022054652515390e-06;
		constexpr double P5 = 4.13813679705723846039e-08;

		auto HighWord = [](double V) { uint64 B; FMemory::Memcpy(&B, &V, 8); return static_cast<uint32>(B >> 32); };
		auto LowWord = [](double V) { uint64 B; FMemory::Memcpy(&B, &V, 8); return static_cast<uint32>(B); };
		auto FromWords = [](uint32 Hi, uint32 Lo) { const uint64 B = (static_cast<uint64>(Hi) << 32) | Lo; double V; FMemory::Memcpy(&V, &B, 8); return V; };

		double Hi = 0.0, Lo = 0.0;
		int32 K = 0;
		uint32 Hx = HighWord(X);
		const int32 Xsb = static_cast<int32>((Hx >> 31) & 1);
		Hx &= 0x7fffffffu;

		// Arguments non finis et hors plage.
		if (Hx >= 0x40862E42u)
		{
			if (Hx >= 0x7ff00000u)
			{
				if (((Hx & 0xfffffu) | LowWord(X)) != 0)
				{
					return X + X; // NaN
				}
				return Xsb == 0 ? X : 0.0; // exp(+-inf) = {inf, 0}
			}
			if (X > OThreshold)
			{
				return Huge * Huge;
			}
			if (X < UThreshold)
			{
				return TwoM1000 * TwoM1000;
			}
		}

		// Reduction d'argument.
		if (Hx > 0x3fd62e42u) // |x| > 0.5 ln2
		{
			if (Hx < 0x3FF0A2B2u) // et |x| < 1.5 ln2
			{
				// Cas particulier de V8 (base/ieee754.cc) : exp(1) rend E exact, le calcul
				// ci-dessous en rate le dernier bit.
				if (X == 1.0)
				{
					return 2.718281828459045;
				}
				Hi = X - Ln2Hi[Xsb];
				Lo = Ln2Lo[Xsb];
				K = 1 - Xsb - Xsb;
			}
			else
			{
				K = static_cast<int32>(InvLn2 * X + HalF[Xsb]);
				const double T = K;
				Hi = X - T * Ln2Hi[0]; // exact ici
				Lo = T * Ln2Lo[0];
			}
			X = Hi - Lo;
		}
		else if (Hx < 0x3e300000u) // |x| < 2^-28
		{
			if (Huge + X > One)
			{
				return One + X;
			}
		}
		else
		{
			K = 0;
		}

		// x est dans l'intervalle principal.
		const double T = X * X;
		const double C = X - T * (P1 + T * (P2 + T * (P3 + T * (P4 + T * P5))));
		if (K == 0)
		{
			return One - ((X * C) / (C - 2.0) - X);
		}
		const double Y = One - ((Lo - (X * C) / (2.0 - C)) - Hi);
		if (K >= -1021)
		{
			if (K == 1024)
			{
				return Y * 2.0 * FromWords(0x7fe00000u, 0u);
			}
			return Y * FromWords(static_cast<uint32>(0x3ff00000 + (K << 20)), 0u);
		}
		return Y * FromWords(static_cast<uint32>(0x3ff00000 + ((K + 1000) << 20)), 0u) * TwoM1000;
	}

	inline double Sin(double Value) { return FMath::Sin(Value); }
	inline double Log(double Value) { return FMath::Loge(Value); }
	inline double Pow(double Base, double Exp) { return FMath::Pow(Base, Exp); }
	inline double Tanh(double Value) { return FMath::Tanh(Value); }
}
