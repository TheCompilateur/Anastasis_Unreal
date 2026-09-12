#pragma once

#include "CoreMinimal.h"
#include "Core/AnastasisJsNumeric.h"
#include "Core/AnastasisSimMath.h"

/**
 * Bruit worldgen — src/sim/world.js valueNoise / smoothNoise / fbm.
 * Les altitudes sont ensuite stockees en Float32 comme le JS.
 * `Anastasis.Sim.Parite.Libm` : sin/log/pow/tanh CRT = V8 sur les echantillons.
 * `Anastasis.Sim.Parite.Fbm` : un vecteur double (~2.5 ulp) ; `Monde` reste
 * identique car les tuiles passent par f32 + round3.
 */
namespace AnastasisWorldNoise
{
	inline double ValueNoise(double X, double Y, double Seed)
	{
		const double N = AnastasisJs::Sin(X * 127.1 + Y * 311.7 + Seed * 0.017) * 43758.5453;
		return N - AnastasisJs::Floor(N);
	}

	inline double SmoothNoise(double X, double Y, double Seed)
	{
		const double IX = AnastasisJs::Floor(X);
		const double IY = AnastasisJs::Floor(Y);
		const double FX = X - IX;
		const double FY = Y - IY;
		const double A = ValueNoise(IX, IY, Seed);
		const double B = ValueNoise(IX + 1.0, IY, Seed);
		const double C = ValueNoise(IX, IY + 1.0, Seed);
		const double D = ValueNoise(IX + 1.0, IY + 1.0, Seed);
		const double UX = FX * FX * (3.0 - 2.0 * FX);
		const double UY = FY * FY * (3.0 - 2.0 * FY);
		return AnastasisMath::Lerp(AnastasisMath::Lerp(A, B, UX), AnastasisMath::Lerp(C, D, UX), UY);
	}

	inline double Fbm(double X, double Y, double Seed)
	{
		double Value = 0.0;
		double Amp = 0.55;
		double Freq = 1.0;
		for (int32 I = 0; I < 4; ++I)
		{
			Value += SmoothNoise(X * Freq, Y * Freq, Seed) * Amp;
			Amp *= 0.5;
			Freq *= 2.0;
		}
		return Value;
	}
}
