#include "World/AnastasisWeather.h"

#include "Core/AnastasisJsNumeric.h"

namespace AnastasisWeather
{
namespace
{
	/**
	 * `hash(a, b)` de weather.js. PAS le hash2d d'util.js : ici le second produit
	 * (`h * 1274126177`) est une multiplication de DOUBLES suivie de `>>> 0`, pas un
	 * Math.imul. Au-dela de 2^53 le double arrondit, et c'est cet arrondi que la
	 * reference a fige dans ses meteos : le reproduire, pas le "corriger".
	 */
	double Hash(double A, double B)
	{
		uint32 H = AnastasisJs::ToUint32(A * 374761393.0 + B * 668265263.0);
		H = H ^ (H >> 13);
		H = AnastasisJs::ToUint32(static_cast<double>(H) * 1274126177.0);
		return static_cast<double>(H) / 4294967295.0;
	}

	/** `clamp01` : NaN traverse, comme en JS (aucune des deux comparaisons n'est vraie). */
	double Clamp01(double V)
	{
		return V < 0.0 ? 0.0 : V > 1.0 ? 1.0 : V;
	}

	/** Math.max / Math.min a deux arguments, NaN contagieux comme en JS. */
	double JsMax(double A, double B)
	{
		return (FMath::IsNaN(A) || FMath::IsNaN(B)) ? std::numeric_limits<double>::quiet_NaN() : (A > B ? A : B);
	}

	double JsMin(double A, double B)
	{
		return (FMath::IsNaN(A) || FMath::IsNaN(B)) ? std::numeric_limits<double>::quiet_NaN() : (A < B ? A : B);
	}

	double Smoothstep(double Edge0, double Edge1, double X)
	{
		const double T = Clamp01((X - Edge0) / JsMax(1e-6, Edge1 - Edge0));
		return T * T * (3.0 - 2.0 * T);
	}

	/** `((x % 1) + 1) % 1` : le `%` de JS est fmod (signe du dividende). */
	double Wrap01(double X)
	{
		return FMath::Fmod(FMath::Fmod(X, 1.0) + 1.0, 1.0);
	}

	bool HasHour(double DayFrac)
	{
		// `dayFrac == null || !Number.isFinite(dayFrac)` : NaN porte le `null`.
		return FMath::IsFinite(DayFrac);
	}
}

const TCHAR* SeasonId(ESeason Season)
{
	switch (Season)
	{
	case ESeason::Spring: return TEXT("spring");
	case ESeason::Summer: return TEXT("summer");
	case ESeason::Autumn: return TEXT("autumn");
	case ESeason::Winter: return TEXT("winter");
	}
	return TEXT("summer");
}

ESeason FieldSeasonFromDay(double Day)
{
	if (!(Day > 0.0))
	{
		return ESeason::Summer;
	}
	const double Index = FMath::Fmod(AnastasisJs::Floor(FMath::Fmod(Day - 1.0, YearDays) / SeasonDays), 4.0);
	// 0 < Day < 1 donne -1 en JS, donc `FIELD_SEASONS[-1]` = undefined, que tout
	// consommateur de la reference lit comme l'ete (`WEATHER_SEASON_BIAS[x] || summer`,
	// `season !== "winter"`). weatherAt floore le jour avant : ce cas ne l'atteint pas.
	switch (static_cast<int32>(Index))
	{
	case 0: return ESeason::Spring;
	case 1: return ESeason::Summer;
	case 2: return ESeason::Autumn;
	case 3: return ESeason::Winter;
	default: return ESeason::Summer;
	}
}

FSeasonBias SeasonClimateBias(ESeason Season)
{
	FSeasonBias B;
	switch (Season)
	{
	case ESeason::Spring:
		B.CoverBias = 0.05; B.RainBias = 0.10; B.WindBias = 0.04; B.SwingMul = 1.12; B.ClearingMul = 1.15;
		break;
	case ESeason::Summer:
		B.CoverBias = -0.07; B.RainBias = -0.06; B.WindBias = -0.05; B.SwingMul = 0.88; B.ClearingMul = 0.85;
		break;
	case ESeason::Autumn:
		B.CoverBias = 0.09; B.RainBias = 0.14; B.WindBias = 0.20; B.SwingMul = 1.22; B.ClearingMul = 1.25;
		break;
	case ESeason::Winter:
		B.CoverBias = 0.12; B.RainBias = 0.04; B.WindBias = 0.18; B.SwingMul = 0.82; B.ClearingMul = 0.70;
		break;
	}
	return B;
}

double CoverLobeAt(double DayFrac, double Peak, double Sigma)
{
	const double T = Wrap01(DayFrac);
	const double P = Wrap01(Peak);
	double D = FMath::Abs(T - P);
	if (D > 0.5)
	{
		D = 1.0 - D;
	}
	const double S = JsMax(0.04, Sigma);
	return FMath::Exp(-(D * D) / (2.0 * S * S));
}

double WinterSnowAt(ESeason Season, double Cover, double Rain)
{
	if (Season != ESeason::Winter)
	{
		return 0.0;
	}
	const double C = Clamp01(Cover);
	const double R = Clamp01(Rain);
	const double Flurry = JsMax(0.0, (C - 0.52) / 0.48) * 0.48;
	const double Storm = R * 0.94;
	return Clamp01(JsMax(Flurry, Storm));
}

double WinterFrostAt(ESeason Season, double Cover, double Snow, double DayFrac, double Clearing)
{
	if (Season != ESeason::Winter)
	{
		return 0.0;
	}
	const double C = Clamp01(Cover);
	const double Sn = Clamp01(Snow);
	const double Cl = Clamp01(Clearing);
	double Frost = 0.18 + (1.0 - C) * 0.30 + Sn * 0.40 + Cl * 0.14;
	if (!HasHour(DayFrac))
	{
		Frost += 0.10;
	}
	else
	{
		const double T = Wrap01(DayFrac);
		const double Dawn = Smoothstep(0.12, 0.22, T) * (1.0 - Smoothstep(0.32, 0.44, T));
		const double LateNight = (T > 0.88 || T < 0.14) ? 0.28 : 0.0;
		Frost += Dawn * 0.38 + LateNight * 0.18;
	}
	return Clamp01(Frost);
}

FCoverFront SampleCoverFront(double CoverBase, uint32 Seed, double Day, double DayFrac, double SwingMulIn, double ClearingMulIn)
{
	const double D = AnastasisJs::Floor(Day);
	const double S = static_cast<double>(Seed);
	const double Base = Clamp01(CoverBase);
	const double Peak = Hash(S, D + 3307.0);
	const double Swing = 0.10 + Hash(S, D + 4413.0) * 0.48;
	const double SwingMul = JsMax(0.5, JsMin(1.6, SwingMulIn));
	const double ClearingMul = JsMax(0.4, JsMin(1.6, ClearingMulIn));
	// Jours tres clairs : petite respiration. Jours charges : vrai front.
	const double Amp = Swing * (0.28 + Base * 0.85) * SwingMul;

	FCoverFront F;
	F.CoverBase = Base;
	F.Peak = Peak;
	F.Swing = Amp;
	if (!HasHour(DayFrac))
	{
		F.Cover = Base;
		F.Lobe = 0.5;
		F.Clearing = 0.0;
		F.Rain = JsMax(0.0, (Base - 0.72) / 0.28);
		return F;
	}

	const double T = Wrap01(DayFrac);
	const double Lobe = CoverLobeAt(T, Peak, 0.10 + Hash(S, D + 5521.0) * 0.06);
	// Sous le lobe la couverture monte ; hors lobe elle redescend sous la base.
	const double Cover = Clamp01(Base + Amp * (Lobe * 1.2 - 0.42));
	const double Rain = JsMax(0.0, (Cover - 0.72) / 0.28);
	// Eclaircie : jour orageux, APRES le pic, et seulement quand la pluie a redescendu.
	const double StormDay = Smoothstep(0.55, 0.82, Base);
	double Delta = T - Peak;
	if (Delta < -0.5)
	{
		Delta += 1.0;
	}
	if (Delta > 0.5)
	{
		Delta -= 1.0;
	}
	const double AfterPeak = Smoothstep(0.08, 0.16, Delta) * (1.0 - Smoothstep(0.28, 0.40, Delta));
	const double RainFade = 1.0 - Smoothstep(0.05, 0.32, Rain);

	F.Cover = Cover;
	F.Lobe = Lobe;
	F.Clearing = Clamp01(StormDay * AfterPeak * RainFade * (0.55 + Amp * 0.55) * ClearingMul);
	F.Rain = Rain;
	return F;
}

FWeather WeatherAt(uint32 Seed, double Day, const FClimateBias* Climate, double DayFrac)
{
	const double D = AnastasisJs::Floor(Day);
	const double S = static_cast<double>(Seed);
	const ESeason Season = FieldSeasonFromDay(D);
	const FSeasonBias Seasonal = SeasonClimateBias(Season);

	const double Roll = Hash(S, D);
	// Plus de jours clairs que de jours couverts : une colonie sous la pluie
	// permanente serait juste terne.
	double CoverBase = Roll < 0.45 ? Roll * 0.5 : Roll < 0.8 ? 0.35 + Roll * 0.35 : 0.72 + (Roll - 0.8) * 1.4;

	// Vent independant (baie calme le plus souvent, rafales rares).
	const double WindRoll = Hash(S, D + 917.0);
	double Wind = WindRoll < 0.55
		? WindRoll * 0.35
		: WindRoll < 0.85
			? 0.25 + (WindRoll - 0.55) * 1.1
			: 0.55 + (WindRoll - 0.85) * 2.2;
	// Direction du souffle (vers ou le vent POUSSE), stable sur la journee.
	const double DirRoll = Hash(S, D + 1403.0);
	const double WindDir = DirRoll * UE_DOUBLE_PI * 2.0;

	// Saison d'abord, puis terre (climate) — soft, clamp final.
	CoverBase += Seasonal.CoverBias;
	Wind += Seasonal.WindBias;
	if (Climate)
	{
		CoverBase += Climate->CoverBias;
		Wind += Climate->CoverBias * 0.15 + Climate->WindBias;
	}
	CoverBase = JsMin(1.0, JsMax(0.0, CoverBase));
	Wind = JsMin(1.0, JsMax(0.0, Wind));

	const FCoverFront Front = SampleCoverFront(CoverBase, Seed, D, DayFrac, Seasonal.SwingMul, Seasonal.ClearingMul);
	double Rain = Front.Rain;
	const double RainBias = Seasonal.RainBias + (Climate ? Climate->RainBias : 0.0);
	if (RainBias != 0.0)
	{
		Rain = JsMin(1.0, JsMax(0.0, Rain + RainBias * 0.35));
	}
	const double Snow = WinterSnowAt(Season, Front.Cover, Rain);
	// Hiver : la precipitation physique reste (humidite), les stries de pluie
	// visibles sont presque eteintes au profit des flocons.
	const double RainOut = Season == ESeason::Winter ? Clamp01(Rain * (1.0 - Snow * 0.88) * 0.22) : Rain;
	const double Frost = WinterFrostAt(Season, Front.Cover, Snow, DayFrac, Front.Clearing);

	FWeather W;
	W.Cover = Front.Cover;
	W.CoverBase = Front.CoverBase;
	W.Rain = RainOut;
	W.Snow = Snow;
	W.Frost = Frost;
	W.Precip = Rain;
	W.Clearing = Front.Clearing;
	W.Peak = Front.Peak;
	W.Season = Season;
	W.Wind = Wind;
	W.WindDir = WindDir;
	W.DirX = FMath::Cos(WindDir);
	W.DirZ = FMath::Sin(WindDir);
	return W;
}

double WeatherWetnessAt(const FWeather& Weather)
{
	const double Rain = Clamp01(Weather.Rain);
	const double Snow = Clamp01(Weather.Snow);
	const double Cover = Clamp01(Weather.Cover);
	const double Clearing = Clamp01(Weather.Clearing);
	const double Precip = Clamp01(Weather.Precip);
	// Neige : fonte legere, jamais une flaque de pluie.
	const double SnowWet = JsMax(Snow * 0.32, Precip * Snow * 0.22);
	const double Active = JsMin(1.0, Rain * 0.95 + Cover * Rain * 0.2 + SnowWet);
	const double Residual = Clearing * 0.44 * (1.0 - JsMax(Rain, Snow) * 0.88);
	return JsMin(1.0, Active + Residual);
}

double WeatherHumidityAt(const FWeather& Weather)
{
	const double Rain = Clamp01(Weather.Rain);
	const double Snow = Clamp01(Weather.Snow);
	const double Cover = Clamp01(Weather.Cover);
	const double Clearing = Clamp01(Weather.Clearing);
	return JsMax(JsMax(Rain * 0.7, Snow * 0.38), JsMax(Clearing * 0.52, Cover * 0.22));
}
}
