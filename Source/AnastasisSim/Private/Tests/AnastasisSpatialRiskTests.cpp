#include "Misc/AutomationTest.h"

#include "Ai/AnastasisSpatialRisk.h"
#include "Life/AnastasisVillageRhythm.h"

#include <limits>

#if WITH_DEV_AUTOMATION_TESTS

namespace AnastasisSpatialRiskParity
{
	namespace Vecteurs
	{
#include "AnastasisSpatialRiskVectors.inl"
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

	/** Les scenes de tools/migration/parity/spatial-risk.mjs (SCENES), recopiees dans le meme ordre. */
	struct FScene
	{
		double X, Y, Speed;
		/** repos, manger, boire, abri ; absent = NaN. */
		double Safes[4][2];
		/** gatherWood, gatherStone, gatherFood, helpFarm, build, explore, maintain, aidHousehold ; absent = NaN. */
		double Targets[8][2];
	};
	constexpr double No = std::numeric_limits<double>::quiet_NaN();
	const FScene Scenes[] = {
		{ 50, 50, 4, { { 52, 50 }, { 60, 48 }, { 40, 50 }, { 51, 51 } },
			{ { 80, 90 }, { 10, 10 }, { 55, 52 }, { 70, 30 }, { No, No }, { 120, 120 }, { 49.5, 57.5 }, { 50.5, 57.5 } } },
		{ 12.25, 7.75, 0, { { No, No }, { 12, 8 }, { No, No }, { No, No } },
			{ { 12.25, 7.75 }, { No, No }, { 200, 3 }, { 13, 9 }, { 30, 30 }, { No, No }, { No, No }, { 100, 100 } } },
		{ 100, 20, 2.2, { { 30, 30 }, { No, No }, { 100, 22 }, { 5, 5 } },
			{ { 140, 25 }, { 101, 21 }, { 60, 80 }, { 99, 19 }, { 104, 20 }, { 150, 70 }, { 30.5, 30.5 }, { 100.5, 20.5 } } },
		{ 40, 40, 3.4, { { 41, 40 }, { 40, 41 }, { 39, 40 }, { No, No } },
			{ { 42, 40 }, { 40, 43 }, { 41, 41 }, { 38, 39 }, { 40.5, 40.5 }, { 44, 44 }, { 41.5, 40.5 }, { 40, 40 } } },
	};

	TOptional<FVector2D> PointOf(const double (&P)[2])
	{
		if (FMath::IsNaN(P[0])) return {};
		return FVector2D(P[0], P[1]);
	}
}

/**
 * Le risque spatial d'un but (`spatialRiskBiasMap`, npc.js), compare BIT A BIT a la reference
 * executee : budgets, trajet, depassement, et la carte entiere sur des replis et des cibles
 * fournis. Vecteurs : tools/migration/parity/spatial-risk.mjs.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisParitySpatialRiskTest,
	"Anastasis.Sim.Parite.RisqueSpatial",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisParitySpatialRiskTest::RunTest(const FString&)
{
	using namespace AnastasisSpatialRiskParity;
	using namespace AnastasisSpatialRiskParity::Vecteurs;
	namespace R = AnastasisSpatialRisk;
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

	for (int32 I = 0; I < UE_ARRAY_COUNT(SpatialRiskThresholdVectors); ++I)
	{
		const auto& V = SpatialRiskThresholdVectors[I];
		Exact(TEXT("SecondsUntilThreshold"), I, R::SecondsUntilThreshold(FromBits(V.A0Bits), FromBits(V.A1Bits), FromBits(V.A2Bits)), V.AttenduBits);
	}
	for (int32 I = 0; I < UE_ARRAY_COUNT(SpatialRiskNeedBudgetVectors); ++I)
	{
		const auto& V = SpatialRiskNeedBudgetVectors[I];
		const R::FNeedBudget B = R::CriticalNeedBudget(FromBits(V.A0Bits), FromBits(V.A1Bits), FromBits(V.A2Bits));
		Exact(TEXT("CriticalNeedBudget.hunger"), I, B.Hunger, V.AttenduHungerBits);
		Exact(TEXT("CriticalNeedBudget.thirst"), I, B.Thirst, V.AttenduThirstBits);
		Exact(TEXT("CriticalNeedBudget.rest"), I, B.Rest, V.AttenduRestBits);
	}
	for (int32 I = 0; I < UE_ARRAY_COUNT(SpatialRiskNightVectors); ++I)
	{
		const auto& V = SpatialRiskNightVectors[I];
		Exact(TEXT("SecondsUntilNight"), I, R::SecondsUntilNight(AnastasisRhythm::DayFracOf(FromBits(V.A0Bits))), V.AttenduBits);
	}
	for (int32 I = 0; I < UE_ARRAY_COUNT(SpatialRiskRainVectors); ++I)
	{
		const auto& V = SpatialRiskRainVectors[I];
		Exact(TEXT("RainReturnBudget"), I, R::RainReturnBudget(FromBits(V.A0Bits)), V.AttenduBits);
	}
	for (int32 I = 0; I < UE_ARRAY_COUNT(SpatialRiskRouteVectors); ++I)
	{
		const auto& V = SpatialRiskRouteVectors[I];
		TOptional<FVector2D> Safe;
		if (V.A5 != 0) Safe = FVector2D(FromBits(V.A6Bits), FromBits(V.A7Bits));
		Exact(TEXT("RouteSeconds"), I, R::RouteSeconds(FromBits(V.A0Bits), FromBits(V.A1Bits), FromBits(V.A2Bits),
			FVector2D(FromBits(V.A3Bits), FromBits(V.A4Bits)), Safe), V.AttenduBits);
	}
	for (int32 I = 0; I < UE_ARRAY_COUNT(SpatialRiskOverrunVectors); ++I)
	{
		const auto& V = SpatialRiskOverrunVectors[I];
		Exact(TEXT("BudgetOverrun"), I, R::BudgetOverrun(FromBits(V.A0Bits), FromBits(V.A1Bits)), V.AttenduBits);
	}

	for (int32 I = 0; I < UE_ARRAY_COUNT(SurvivalTravelVectors); ++I)
	{
		const auto& V = SurvivalTravelVectors[I];
		TOptional<FVector2D> Target;
		if (V.A3 != 0) Target = FVector2D(FromBits(V.A4Bits), FromBits(V.A5Bits));
		Exact(TEXT("TravelPressureToTarget"), I, R::TravelPressureToTarget(FromBits(V.A0Bits), FromBits(V.A1Bits),
			FromBits(V.A2Bits), Target, R::Survival::HorizonSeconds), V.AttenduBits);
	}

	int32 ForecastNonEmpty = 0;
	for (int32 I = 0; I < UE_ARRAY_COUNT(SurvivalForecastVectors); ++I)
	{
		const auto& V = SurvivalForecastVectors[I];
		const FScene& S = Scenes[V.A0];
		R::FForecastInputs In;
		In.NpcX = S.X;
		In.NpcY = S.Y;
		In.Speed = S.Speed;
		In.Hunger = FromBits(V.A1Bits);
		In.Thirst = FromBits(V.A2Bits);
		In.Energy = FromBits(V.A3Bits);
		In.InventoryFood = V.A4;
		In.bMealBlocked = V.A5 != 0;
		In.RestTarget = PointOf(S.Safes[0]);
		In.EatTarget = PointOf(S.Safes[1]);
		In.DrinkTarget = PointOf(S.Safes[2]);
		const TArray<TPair<FString, double>> Map = R::SurvivalForecastBias(In);

		TArray<FString> Keys;
		for (const TPair<FString, double>& Entry : Map) Keys.Add(Entry.Key);
		ForecastNonEmpty += Map.Num() > 0 ? 1 : 0;
		++Compared;
		const FString Expected = UTF8_TO_TCHAR(V.AttenduKeys);
		if (FString::Join(Keys, TEXT(",")) != Expected)
		{
			++Failures;
			AddError(FString::Printf(TEXT("SurvivalForecast.keys[%d] : %s attendu %s"), I, *FString::Join(Keys, TEXT(",")), *Expected));
		}
		const TPair<const TCHAR*, uint64> Expect[] = {
			{ TEXT("eat"), V.AttenduEatBits }, { TEXT("drink"), V.AttenduDrinkBits }, { TEXT("rest"), V.AttenduRestBits },
			{ TEXT("buy"), V.AttenduBuyBits }, { TEXT("sell"), V.AttenduSellBits }, { TEXT("eatTogether"), V.AttenduEatTogetherBits },
			{ TEXT("shelterRain"), V.AttenduShelterRainBits }, { TEXT("gatherWood"), V.AttenduGatherWoodBits },
			{ TEXT("gatherStone"), V.AttenduGatherStoneBits }, { TEXT("gatherFood"), V.AttenduGatherFoodBits },
			{ TEXT("helpFarm"), V.AttenduHelpFarmBits }, { TEXT("build"), V.AttenduBuildBits }, { TEXT("deliver"), V.AttenduDeliverBits },
			{ TEXT("fetchInput"), V.AttenduFetchInputBits }, { TEXT("haulJob"), V.AttenduHaulJobBits }, { TEXT("haulCart"), V.AttenduHaulCartBits },
			{ TEXT("explore"), V.AttenduExploreBits }, { TEXT("craft"), V.AttenduCraftBits }, { TEXT("maintain"), V.AttenduMaintainBits },
			{ TEXT("closeWorkplace"), V.AttenduCloseWorkplaceBits },
		};
		for (const TPair<const TCHAR*, uint64>& E : Expect)
		{
			Exact(*FString::Printf(TEXT("SurvivalForecast.%s"), E.Key), I, R::BiasOf(Map, E.Key), E.Value);
		}
	}

	int32 NonEmpty = 0;
	for (int32 I = 0; I < UE_ARRAY_COUNT(SpatialRiskBiasMapVectors); ++I)
	{
		const auto& V = SpatialRiskBiasMapVectors[I];
		const FScene& S = Scenes[V.A0];
		R::FRiskInputs In;
		In.NpcX = S.X;
		In.NpcY = S.Y;
		In.Speed = S.Speed;
		In.Hunger = FromBits(V.A1Bits);
		In.Thirst = FromBits(V.A2Bits);
		In.Energy = FromBits(V.A3Bits);
		In.DayFrac = AnastasisRhythm::DayFracOf(FromBits(V.A4Bits));
		In.Rain = FromBits(V.A5Bits);
		In.RestSafe = PointOf(S.Safes[0]);
		In.EatSafe = PointOf(S.Safes[1]);
		In.DrinkSafe = PointOf(S.Safes[2]);
		// `shelterRainAccess(sim, npc) || restSafe`.
		In.ShelterSafe = PointOf(S.Safes[3]);
		if (!In.ShelterSafe.IsSet()) In.ShelterSafe = In.RestSafe;

		const TConstArrayView<const TCHAR*> Goals = R::Goals();
		const TArray<TPair<FString, double>> Map = R::BiasMap(In, [&S, &Goals](const FString& Goal)
		{
			const int32 Index = Goals.IndexOfByPredicate([&Goal](const TCHAR* G) { return Goal == G; });
			return PointOf(S.Targets[Index]);
		});

		TArray<FString> Keys;
		for (const TPair<FString, double>& Entry : Map) Keys.Add(Entry.Key);
		NonEmpty += Map.Num() > 0 ? 1 : 0;
		++Compared;
		const FString Expected = UTF8_TO_TCHAR(V.AttenduKeys);
		if (FString::Join(Keys, TEXT(",")) != Expected)
		{
			++Failures;
			AddError(FString::Printf(TEXT("BiasMap.keys[%d] : %s attendu %s"), I, *FString::Join(Keys, TEXT(",")), *Expected));
		}
		const uint64 ExpectedBits[8] = { V.AttenduGatherWoodBits, V.AttenduGatherStoneBits, V.AttenduGatherFoodBits,
			V.AttenduHelpFarmBits, V.AttenduBuildBits, V.AttenduExploreBits, V.AttenduMaintainBits, V.AttenduAidHouseholdBits };
		for (int32 G = 0; G < Goals.Num(); ++G)
		{
			const TPair<FString, double>* Entry = Map.FindByPredicate([&](const TPair<FString, double>& E) { return E.Key == Goals[G]; });
			Exact(*FString::Printf(TEXT("BiasMap.%s"), Goals[G]), I, Entry ? Entry->Value : 0.0, ExpectedBits[G]);
		}
	}

	AddInfo(FString::Printf(TEXT("ANASTASIS_SPATIAL_RISK_PARITY compared=%d failures=%d maps=%d nonempty=%d forecasts=%d nonempty=%d"),
		Compared, Failures, static_cast<int32>(UE_ARRAY_COUNT(SpatialRiskBiasMapVectors)), NonEmpty,
		static_cast<int32>(UE_ARRAY_COUNT(SurvivalForecastVectors)), ForecastNonEmpty));
	return Failures == 0;
}

#endif
