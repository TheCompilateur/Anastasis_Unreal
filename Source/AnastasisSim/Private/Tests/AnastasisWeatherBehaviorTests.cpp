#include "Misc/AutomationTest.h"

#include "Life/AnastasisWeatherBehavior.h"
#include "World/AnastasisWeather.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace AnastasisWeatherBehaviorParity
{
	namespace Vecteurs
	{
#include "AnastasisWeatherBehaviorVectors.inl"
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

	AnastasisWeather::ESeason SeasonFrom(const FString& Id)
	{
		using AnastasisWeather::ESeason;
		return Id == TEXT("spring") ? ESeason::Spring
			: Id == TEXT("autumn") ? ESeason::Autumn
			: Id == TEXT("winter") ? ESeason::Winter
			: ESeason::Summer;
	}
}

/**
 * Le comportement meteo des habitants (weatherGoalBias.js), compare BIT A BIT a la reference
 * executee. Vecteurs : tools/migration/parity/weather-behavior.mjs.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisParityWeatherBehaviorTest,
	"Anastasis.Sim.Parite.MeteoHabitants",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisParityWeatherBehaviorTest::RunTest(const FString&)
{
	using namespace AnastasisWeatherBehaviorParity::Vecteurs;
	using AnastasisWeatherBehaviorParity::FromBits;
	using AnastasisWeatherBehaviorParity::ToBits;
	using AnastasisWeatherBehaviorParity::SeasonFrom;
	namespace B = AnastasisWeatherBehavior;
	int32 Failures = 0;
	int32 Compared = 0;

	auto Exact = [&](const TCHAR* What, int32 I, double Got, uint64 ExpectedBits)
	{
		++Compared;
		if (ToBits(Got) != ExpectedBits)
		{
			if (++Failures <= 20)
			{
				AddError(FString::Printf(TEXT("%s[%d] : %.17g attendu %.17g"), What, I, Got, FromBits(ExpectedBits)));
			}
		}
	};

	for (int32 I = 0; I < UE_ARRAY_COUNT(ReadSimWeatherVectors); ++I)
	{
		const auto& V = ReadSimWeatherVectors[I];
		const B::FSimWeather W = B::ReadSimWeather(static_cast<uint32>(FromBits(V.A0Bits)), V.A1, FromBits(V.A2Bits));
		Exact(TEXT("ReadSimWeather.rain"), I, W.Rain, V.AttenduRainBits);
		Exact(TEXT("ReadSimWeather.snow"), I, W.Snow, V.AttenduSnowBits);
		Exact(TEXT("ReadSimWeather.wind"), I, W.Wind, V.AttenduWindBits);
		Exact(TEXT("ReadSimWeather.cover"), I, W.Cover, V.AttenduCoverBits);
		Exact(TEXT("ReadSimWeather.clearing"), I, W.Clearing, V.AttenduClearingBits);
		++Compared;
		if (FString(AnastasisWeather::SeasonId(W.Season)) != UTF8_TO_TCHAR(V.AttenduSeason))
		{
			++Failures;
			AddError(FString::Printf(TEXT("ReadSimWeather.season[%d] : %s attendu %s"), I, AnastasisWeather::SeasonId(W.Season), UTF8_TO_TCHAR(V.AttenduSeason)));
		}
	}

	for (int32 I = 0; I < UE_ARRAY_COUNT(WeatherGoalBiasVectors); ++I)
	{
		const auto& V = WeatherGoalBiasVectors[I];
		B::FSimWeather W;
		W.Rain = FromBits(V.A0Bits);
		W.Snow = FromBits(V.A1Bits);
		W.Wind = FromBits(V.A2Bits);
		W.Season = SeasonFrom(UTF8_TO_TCHAR(V.A3));
		Exact(TEXT("WeatherGoalBias"), I, B::WeatherGoalBias(W, UTF8_TO_TCHAR(V.A4), UTF8_TO_TCHAR(V.A5)), V.AttenduBits);
	}

	for (int32 I = 0; I < UE_ARRAY_COUNT(ShelterRainVectors); ++I)
	{
		const auto& V = ShelterRainVectors[I];
		const double Rain = FromBits(V.A0Bits);
		const bool bInside = V.A1 != 0;
		const FString Goal = UTF8_TO_TCHAR(V.A2);
		const FString Job = UTF8_TO_TCHAR(V.A3);
		const double Time = FromBits(V.A4Bits);
		const double Cooldown = FromBits(V.A5Bits);
		++Compared;
		if (B::ShouldSeekRainShelter(Rain, bInside, Goal, Job, Time, Cooldown) != (V.AttenduShould != 0))
		{
			++Failures;
			AddError(FString::Printf(TEXT("ShouldSeekRainShelter[%d] rain=%.3f inside=%d goal=%s job=%s"), I, Rain, bInside, *Goal, *Job));
		}
		Exact(TEXT("ShelterRainScore"), I, B::ShelterRainScore(Rain, bInside, Goal, Job, Time, Cooldown), V.AttenduScoreBits);
	}

	AddInfo(FString::Printf(TEXT("ANASTASIS_WEATHER_BEHAVIOR_PARITY compared=%d failures=%d"), Compared, Failures));
	return Failures == 0;
}

/**
 * Les fonctions que la reference n'exporte pas (le generateur de vecteurs ne peut pas les
 * appeler), testees contre leur formule transcrite et leurs bornes : duree d'abri,
 * exposition a l'orage, bloc pluie de la vitesse de marche.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisWeatherBehaviorFormulasTest,
	"Anastasis.Sim.MeteoHabitants.Formules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisWeatherBehaviorFormulasTest::RunTest(const FString&)
{
	namespace B = AnastasisWeatherBehavior;

	// shelterRainDuration : 22 s a l'orage naissant, 38 s au deluge ; une pluie nulle vaut l'orage naissant.
	TestEqual(TEXT("duration at rainHeavy"), B::ShelterRainDuration(0.48), 22.0, 1e-12);
	TestEqual(TEXT("duration at rain 0 (rain || rainHeavy)"), B::ShelterRainDuration(0.0), 22.0, 1e-12);
	TestEqual(TEXT("duration saturates at 0.93"), B::ShelterRainDuration(0.93), 38.0, 1e-12);
	TestEqual(TEXT("duration beyond saturation"), B::ShelterRainDuration(1.0), 38.0, 1e-12);
	TestEqual(TEXT("duration midway"), B::ShelterRainDuration(0.705), 30.0, 1e-9);

	// applyRainExposure.
	{
		double E = 70.0, H = 90.0;
		B::ApplyRainExposure(0.47, false, TEXT("gatherFood"), 1.0, E, H);
		TestEqual(TEXT("below heavy rain: no drain"), E, 70.0);
		B::ApplyRainExposure(0.6, false, TEXT("gatherFood"), 1.0, E, H);
		TestEqual(TEXT("heavy rain drains energy 2.4 * rain * dt"), E, 70.0 - 2.4 * 0.6 * 1.0, 1e-12);
		TestEqual(TEXT("health untouched below 0.7"), H, 90.0);
		B::ApplyRainExposure(0.8, false, TEXT("gatherFood"), 2.0, E, H);
		TestEqual(TEXT("health drains 0.28 * dt from 0.7"), H, 90.0 - 0.28 * 2.0, 1e-12);
		const double E2 = E, H2 = H;
		B::ApplyRainExposure(1.0, true, TEXT("gatherFood"), 5.0, E, H);
		B::ApplyRainExposure(1.0, false, B::GoalShelterRain, 5.0, E, H);
		B::ApplyRainExposure(1.0, false, TEXT("gatherFood"), 0.0, E, H);
		TestTrue(TEXT("inside, sheltering, or dt 0: no drain"), E == E2 && H == H2);
		double E3 = 1.0, H3 = 0.1;
		B::ApplyRainExposure(1.0, false, TEXT("explore"), 10.0, E3, H3);
		TestTrue(TEXT("clamped at 0"), E3 == 0.0 && H3 == 0.0);
		double OpenE = 70.0, OpenH = 90.0, CrownE = OpenE, CrownH = OpenH;
		B::ApplyRainExposure(0.9, false, TEXT("gatherFood"), 2.0, OpenE, OpenH);
		B::ApplyRainExposure(0.9, false, TEXT("gatherFood"), 2.0, CrownE, CrownH, 1.0);
		TestTrue(TEXT("crown reduces but does not cancel exposure"),
			CrownE > OpenE && CrownE < 70.0 && CrownH > OpenH && CrownH < 90.0);
	}

	// Bloc pluie de movementSpeedFactor.
	TestEqual(TEXT("dry: no effect"), B::RainSpeedFactor(0.2, TEXT("socialize"), TEXT("settler")), 1.0);
	TestEqual(TEXT("running for shelter"), B::RainSpeedFactor(0.5, B::GoalShelterRain, TEXT("farmer")), 1.0 + 0.5 * 0.28, 1e-12);
	TestEqual(TEXT("settler hurrying home to rest"), B::RainSpeedFactor(0.5, TEXT("rest"), TEXT("settler")), 1.0 + 0.5 * 0.18, 1e-12);
	TestEqual(TEXT("settler with no shelter goal slows"), B::RainSpeedFactor(0.5, TEXT("drink"), TEXT("settler")), 0.96);
	TestEqual(TEXT("farmer keeps his pace"), B::RainSpeedFactor(0.5, TEXT("rest"), TEXT("farmer")), 1.0);
	TestEqual(TEXT("anyone gathering keeps his pace"), B::RainSpeedFactor(0.5, TEXT("gatherFood"), TEXT("settler")), 1.0);
	TestEqual(TEXT("this block's outdoor set excludes woodcutter"), B::RainSpeedFactor(0.5, TEXT("drink"), TEXT("woodcutter")), 0.96);
	return true;
}

#endif
