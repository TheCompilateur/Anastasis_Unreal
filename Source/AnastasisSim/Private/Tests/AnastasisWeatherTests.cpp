#include "Misc/AutomationTest.h"

#include "World/AnastasisWeather.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace AnastasisWeatherParity
{
	namespace Vecteurs
	{
#include "AnastasisWeatherVectors.inl"
	}

	double FromBits(uint64 Bits)
	{
		double Value;
		FMemory::Memcpy(&Value, &Bits, sizeof(Value));
		return Value;
	}

	uint64 ToBits(double Value)
	{
		uint64 Bits;
		FMemory::Memcpy(&Bits, &Value, sizeof(Bits));
		return Bits;
	}

	/** Distance en ULP entre deux doubles finis de meme signe ; INT64_MAX sinon. */
	int64 UlpDistance(double A, double B)
	{
		if (ToBits(A) == ToBits(B))
		{
			return 0;
		}
		if (!FMath::IsFinite(A) || !FMath::IsFinite(B) || (A < 0.0) != (B < 0.0))
		{
			return (A == B) ? 0 : MAX_int64;
		}
		const int64 IA = static_cast<int64>(ToBits(FMath::Abs(A)));
		const int64 IB = static_cast<int64>(ToBits(FMath::Abs(B)));
		return FMath::Abs(IA - IB);
	}

	/**
	 * Compare un champ. Tout est exige au bit pres -- y compris ce qui traverse Math.exp,
	 * que le portage calcule avec l'exp de fdlibm, celle de V8 (AnastasisJs::Exp). Seuls
	 * `dirX` / `dirZ` (`bLibm`) passent par Math.cos / Math.sin, que V8 ne prend pas au CRT
	 * MSVC : 4 ULP y sont toleres ET comptes, meme cause que la divergence connue de
	 * `Parite.Fbm`. Mesure du 2026-09-30 : 39 valeurs a 1 ULP, toutes sur dirX / dirZ.
	 * Premier run, avec l'exp du CRT : 2 echecs a 6 et 8 ULP sur `cover` et `rain` (une
	 * soustraction de grandeurs voisines amplifie l'ulp d'exp) -- d'ou le portage de fdlibm.
	 */
	struct FTally
	{
		int32 Failures = 0;
		int32 Compared = 0;
		int32 NotBitExact = 0;
		int64 MaxUlp = 0;
	};

	constexpr int64 LibmUlpTolerance = 4;

	template <typename VectorType>
	void CheckWeather(FAutomationTestBase& Test, FTally& T, const TCHAR* Case, int32 I, const VectorType& V,
		const AnastasisWeather::FWeather& W)
	{
		auto Field = [&](const TCHAR* Name, double Got, uint64 ExpectedBits, bool bLibm)
		{
			++T.Compared;
			const double Expected = FromBits(ExpectedBits);
			const int64 Ulp = UlpDistance(Got, Expected);
			if (Ulp == 0)
			{
				return;
			}
			++T.NotBitExact;
			T.MaxUlp = FMath::Max(T.MaxUlp, Ulp);
			if (!bLibm || Ulp > LibmUlpTolerance)
			{
				++T.Failures;
				Test.AddError(FString::Printf(TEXT("%s[%d].%s : %.17g attendu %.17g (%lld ulp)"),
					Case, I, Name, Got, Expected, Ulp));
			}
		};

		Field(TEXT("cover"), W.Cover, V.AttenduCoverBits, false);
		Field(TEXT("coverBase"), W.CoverBase, V.AttenduCoverBaseBits, false);
		Field(TEXT("rain"), W.Rain, V.AttenduRainBits, false);
		Field(TEXT("snow"), W.Snow, V.AttenduSnowBits, false);
		Field(TEXT("frost"), W.Frost, V.AttenduFrostBits, false);
		Field(TEXT("precip"), W.Precip, V.AttenduPrecipBits, false);
		Field(TEXT("clearing"), W.Clearing, V.AttenduClearingBits, false);
		Field(TEXT("peak"), W.Peak, V.AttenduPeakBits, false);
		Field(TEXT("wind"), W.Wind, V.AttenduWindBits, false);
		Field(TEXT("windDir"), W.WindDir, V.AttenduWindDirBits, false);
		Field(TEXT("dirX"), W.DirX, V.AttenduDirXBits, true);
		Field(TEXT("dirZ"), W.DirZ, V.AttenduDirZBits, true);

		++T.Compared;
		const FString Season = AnastasisWeather::SeasonId(W.Season);
		const FString ExpectedSeason = UTF8_TO_TCHAR(V.AttenduSeason);
		if (Season != ExpectedSeason)
		{
			++T.Failures;
			Test.AddError(FString::Printf(TEXT("%s[%d].season : %s attendu %s"), Case, I, *Season, *ExpectedSeason));
		}
	}

	uint32 SeedOf(uint64 Bits)
	{
		// Les graines sont declarees en double (au-dela de 2^31) ; la reference fait `seed >>> 0`.
		return static_cast<uint32>(FromBits(Bits));
	}
}

/**
 * La meteo commune (weather.js), comparee a la reference EXECUTEE.
 * Vecteurs : tools/migration/parity/weather.mjs (1112).
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisParityWeatherTest,
	"Anastasis.Sim.Parite.Meteo",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisParityWeatherTest::RunTest(const FString&)
{
	using namespace AnastasisWeatherParity::Vecteurs;
	using AnastasisWeatherParity::CheckWeather;
	using AnastasisWeatherParity::FromBits;
	using AnastasisWeatherParity::LibmUlpTolerance;
	using AnastasisWeatherParity::SeedOf;
	using AnastasisWeatherParity::UlpDistance;
	AnastasisWeatherParity::FTally T;

	for (int32 I = 0; I < UE_ARRAY_COUNT(WeatherAtHourVectors); ++I)
	{
		const auto& V = WeatherAtHourVectors[I];
		CheckWeather(*this, T, TEXT("WeatherAtHour"), I, V,
			AnastasisWeather::WeatherAt(SeedOf(V.A0Bits), FromBits(V.A1Bits), nullptr, FromBits(V.A2Bits)));
	}
	for (int32 I = 0; I < UE_ARRAY_COUNT(WeatherAtDailyVectors); ++I)
	{
		const auto& V = WeatherAtDailyVectors[I];
		CheckWeather(*this, T, TEXT("WeatherAtDaily"), I, V,
			AnastasisWeather::WeatherAt(SeedOf(V.A0Bits), FromBits(V.A1Bits)));
	}
	for (int32 I = 0; I < UE_ARRAY_COUNT(WeatherAtClimateVectors); ++I)
	{
		const auto& V = WeatherAtClimateVectors[I];
		AnastasisWeather::FClimateBias Climate;
		Climate.CoverBias = FromBits(V.A3Bits);
		Climate.RainBias = FromBits(V.A4Bits);
		Climate.WindBias = FromBits(V.A5Bits);
		CheckWeather(*this, T, TEXT("WeatherAtClimate"), I, V,
			AnastasisWeather::WeatherAt(SeedOf(V.A0Bits), FromBits(V.A1Bits), &Climate, FromBits(V.A2Bits)));
	}
	for (int32 I = 0; I < UE_ARRAY_COUNT(WetnessHumidityVectors); ++I)
	{
		const auto& V = WetnessHumidityVectors[I];
		const AnastasisWeather::FWeather W =
			AnastasisWeather::WeatherAt(SeedOf(V.A0Bits), FromBits(V.A1Bits), nullptr, FromBits(V.A2Bits));
		const TCHAR* Names[2] = { TEXT("wetness"), TEXT("humidity") };
		const double Gots[2] = { AnastasisWeather::WeatherWetnessAt(W), AnastasisWeather::WeatherHumidityAt(W) };
		const uint64 Expecteds[2] = { V.AttenduWetnessBits, V.AttenduHumidityBits };
		for (int32 K = 0; K < 2; ++K)
		{
			const TCHAR* Name = Names[K];
			const double Got = Gots[K];
			const uint64 Bits = Expecteds[K];
			++T.Compared;
			const int64 Ulp = UlpDistance(Got, FromBits(Bits));
			if (Ulp != 0)
			{
				++T.NotBitExact;
				T.MaxUlp = FMath::Max(T.MaxUlp, Ulp);
			}
			if (Ulp != 0)
			{
				++T.Failures;
				AddError(FString::Printf(TEXT("WetnessHumidity[%d].%s : %.17g attendu %.17g (%lld ulp)"),
					I, Name, Got, FromBits(Bits), Ulp));
			}
		}
	}

	// Le nombre de valeurs non bit-exactes est une mesure, pas un detail : il est ecrit.
	AddInfo(FString::Printf(TEXT("ANASTASIS_WEATHER_PARITY compared=%d not_bit_exact=%d max_ulp=%lld failures=%d"),
		T.Compared, T.NotBitExact, T.MaxUlp, T.Failures));
	return T.Failures == 0;
}

/**
 * Proprietes que la reference garantit et que le rendu va consommer : toutes les
 * sorties bornees dans [0,1], la neige et le givre seulement en hiver, la meme entree
 * rend la meme meteo, et les quatre saisons tombent aux jours 1, 31, 61, 91.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisWeatherInvariantsTest,
	"Anastasis.Sim.Meteo.Invariants",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisWeatherInvariantsTest::RunTest(const FString&)
{
	using namespace AnastasisWeather;
	using AnastasisWeatherParity::ToBits;
	TestEqual(TEXT("day 1 is spring"), FString(SeasonId(FieldSeasonFromDay(1))), FString(TEXT("spring")));
	TestEqual(TEXT("day 31 is summer"), FString(SeasonId(FieldSeasonFromDay(31))), FString(TEXT("summer")));
	TestEqual(TEXT("day 61 is autumn"), FString(SeasonId(FieldSeasonFromDay(61))), FString(TEXT("autumn")));
	TestEqual(TEXT("day 91 is winter"), FString(SeasonId(FieldSeasonFromDay(91))), FString(TEXT("winter")));
	TestEqual(TEXT("day 121 is spring again"), FString(SeasonId(FieldSeasonFromDay(121))), FString(TEXT("spring")));

	int32 Bad = 0;
	for (int32 Day = 1; Day <= 240; ++Day)
	{
		for (int32 Step = 0; Step < 24; ++Step)
		{
			const double Frac = Step / 24.0;
			const FWeather W = WeatherAt(12345u, Day, nullptr, Frac);
			const bool bBounded =
				W.Cover >= 0.0 && W.Cover <= 1.0 && W.Rain >= 0.0 && W.Rain <= 1.0 &&
				W.Snow >= 0.0 && W.Snow <= 1.0 && W.Frost >= 0.0 && W.Frost <= 1.0 &&
				W.Clearing >= 0.0 && W.Clearing <= 1.0 && W.Wind >= 0.0 && W.Wind <= 1.0;
			const bool bWinterOnly = W.Season == ESeason::Winter || (W.Snow == 0.0 && W.Frost == 0.0);
			const FWeather Again = WeatherAt(12345u, Day, nullptr, Frac);
			// Champ par champ, au bit pres : un memcmp lirait aussi l'octet de bourrage apres Season.
			const bool bDeterministic =
				ToBits(W.Cover) == ToBits(Again.Cover) && ToBits(W.Rain) == ToBits(Again.Rain) &&
				ToBits(W.Snow) == ToBits(Again.Snow) && ToBits(W.Frost) == ToBits(Again.Frost) &&
				ToBits(W.Clearing) == ToBits(Again.Clearing) && ToBits(W.Wind) == ToBits(Again.Wind) &&
				ToBits(W.WindDir) == ToBits(Again.WindDir) && W.Season == Again.Season;
			if (!bBounded || !bWinterOnly || !bDeterministic)
			{
				if (++Bad <= 5)
				{
					AddError(FString::Printf(TEXT("day=%d frac=%.3f bounded=%d winter_only=%d deterministic=%d"),
						Day, Frac, bBounded, bWinterOnly, bDeterministic));
				}
			}
		}
	}
	TestEqual(TEXT("invariant violations over two years, hourly"), Bad, 0);
	return true;
}

#endif
