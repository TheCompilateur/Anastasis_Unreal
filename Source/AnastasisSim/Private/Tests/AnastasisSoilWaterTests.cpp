#include "Misc/AutomationTest.h"
#include "Village/AnastasisVillage.h"
#include "Work/AnastasisFields.h"
#include "World/AnastasisSoilWater.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace AnastasisSoilWaterTest
{
	AnastasisWorld::FWorld MakeWorld()
	{
		AnastasisWorld::FWorld W;
		W.W = W.H = 16;
		W.Tiles.SetNum(16 * 16);
		for (int32 Y = 0; Y < 16; ++Y) for (int32 X = 0; X < 16; ++X)
		{
			auto& T = W.Tiles[Y * 16 + X];
			T.X = X; T.Y = Y; T.Type = AnastasisWorld::ETileType::Grass;
			T.Alt = 0.5; T.Wetness = 0.2;
		}
		auto& Field = W.Tiles[8 * 16 + 8];
		Field.Type = AnastasisWorld::ETileType::Field;
		Field.Resource = AnastasisWorld::EResource::Food;
		Field.Amount = 0;
		Field.Fertility = 1.0;
		Field.CropId = AnastasisWorld::ECropId::Grain;
		return W;
	}

	struct FResult { int32 Food = 0; double Stored = -1.0; bool bHasWater = false; uint64 Digest = 0; };
	FResult Run(double Rain, bool bEnabled, int32 Day)
	{
		const AnastasisWorld::FWorld World = MakeWorld();
		AnastasisVillage::FVillage Village;
		Village.Bind(World);
		Village.SetSoilWaterEnabled(bEnabled);
		AnastasisWeatherBehavior::FSimWeather Weather;
		Weather.Rain = Rain;
		Village.SetForcedWeather(Weather);
		Village.RegrowFieldsDaily(Day);
		FResult Result;
		Result.Food = Village.LiveTileAt(8, 8).Amount;
		Result.bHasWater = Village.GetSoilWaterAt(8, 8, Result.Stored);
		Result.Digest = Village.Digest();
		return Result;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisSoilWaterBalanceTest,
	"Anastasis.Sim.SoilWater.Balance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnastasisSoilWaterBalanceTest::RunTest(const FString&)
{
	using namespace AnastasisSoilWater;
	double Dry = 0.2, Wet = 0.2;
	for (int32 Day = 0; Day < 20; ++Day)
	{
		const FStep A = Advance(Dry, 0.0, 0.7);
		const FStep B = Advance(Wet, 1.0, 0.7);
		if (!TestTrue(TEXT("valid steps"), A.bValid && B.bValid)) return false;
		TestTrue(TEXT("dry balance"), FMath::IsNearlyEqual(A.Before + A.RainIn,
			A.After + A.Evaporation + A.Drainage + A.Overflow, 1e-12));
		TestTrue(TEXT("wet balance"), FMath::IsNearlyEqual(B.Before + B.RainIn,
			B.After + B.Evaporation + B.Drainage + B.Overflow, 1e-12));
		Dry = A.After; Wet = B.After;
		TestTrue(TEXT("bounded reservoir"), Dry >= 0.0 && Dry <= 1.0 && Wet >= 0.0 && Wet <= 1.0);
	}
	TestTrue(TEXT("rain changes stored water"), Wet > Dry);
	TestFalse(TEXT("nonfinite rain rejected"), Advance(0.5, NAN, 0.7).bValid);
	TestFalse(TEXT("out of range storage rejected"), Advance(1.1, 0.5, 0.7).bValid);
	AddInfo(FString::Printf(TEXT("SOIL_WATER_BALANCE dry=%.6f wet=%.6f"), Dry, Wet));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisSoilWaterRegrowthTest,
	"Anastasis.Sim.SoilWater.FieldRegrowth",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnastasisSoilWaterRegrowthTest::RunTest(const FString&)
{
	using namespace AnastasisSoilWaterTest;
	int32 Day = 2;
	while (Day < 30 && AnastasisFields::RegrowRoll(8, 8, Day) > AnastasisFields::DailyChance(Day)) ++Day;
	if (!TestTrue(TEXT("fixture: eligible spring day"), Day < 30)) return false;
	const FResult OffDry = Run(0.0, false, Day);
	const FResult OffWet = Run(1.0, false, Day);
	const FResult Dry = Run(0.0, true, Day);
	const FResult Wet = Run(1.0, true, Day);
	const FResult WetAgain = Run(1.0, true, Day);
	TestEqual(TEXT("disabled: same food"), OffDry.Food, OffWet.Food);
	TestEqual(TEXT("disabled: same digest"), OffDry.Digest, OffWet.Digest);
	TestFalse(TEXT("disabled: no reservoir"), OffDry.bHasWater || OffWet.bHasWater);
	TestTrue(TEXT("enabled: both reservoirs"), Dry.bHasWater && Wet.bHasWater);
	TestTrue(TEXT("enabled: rain increases storage"), Wet.Stored > Dry.Stored);
	TestTrue(TEXT("enabled: rain increases this field's regrowth"), Wet.Food > Dry.Food);
	TestEqual(TEXT("same rain: same food"), Wet.Food, WetAgain.Food);
	TestEqual(TEXT("same rain: same digest"), Wet.Digest, WetAgain.Digest);
	AddInfo(FString::Printf(TEXT("SOIL_WATER_FIELD day=%d off=%d dry=%d wet=%d storage_dry=%.6f storage_wet=%.6f"),
		Day, OffDry.Food, Dry.Food, Wet.Food, Dry.Stored, Wet.Stored));
	return true;
}
#endif
