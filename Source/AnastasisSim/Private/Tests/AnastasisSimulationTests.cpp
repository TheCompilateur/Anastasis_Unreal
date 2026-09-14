#include "Misc/AutomationTest.h"

#include "Core/AnastasisSimClock.h"
#include "Sim/AnastasisSimulation.h"
#include "World/AnastasisWorld.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr uint32 kSeed = 12345u;
	constexpr int32 kW = 96;
	constexpr int32 kH = 96;

	uint64 TimeBits(double Value)
	{
		uint64 Bits = 0;
		FMemory::Memcpy(&Bits, &Value, sizeof(double));
		return Bits;
	}

	void ResetCanonical(FAnastasisSimulation& Sim)
	{
		Sim.Reset(kSeed, kW, kH);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisSimTickBootDeferred,
	"Anastasis.Sim.Tick.BootDeferred",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisSimTickBootDeferred::RunTest(const FString&)
{
	FAnastasisSimulation Sim;
	TestFalse(TEXT("ctor is bootDeferred"), Sim.IsRunning());
	TestEqual(TEXT("day before reset"), Sim.GetDay(), 0);

	Sim.Tick(AnastasisSimClock::FixedDt);
	TestEqual(TEXT("tick before reset is a no-op"), Sim.GetDay(), 0);
	TestEqual(TEXT("time before reset stays 0"), TimeBits(Sim.GetTime()), TimeBits(0.0));
	TestEqual(TEXT("pump before reset is a no-op"), Sim.PumpFrame(1.0 / 60.0, 10.0), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisSimTickInitialClock,
	"Anastasis.Sim.Tick.InitialClock",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisSimTickInitialClock::RunTest(const FString&)
{
	FAnastasisSimulation Sim;
	ResetCanonical(Sim);

	TestTrue(TEXT("running after reset"), Sim.IsRunning());
	TestEqual(TEXT("seed"), Sim.GetSeed(), kSeed);
	TestEqual(TEXT("day 1"), Sim.GetDay(), 1);
	TestEqual(TEXT("newDay count 0"), Sim.GetNewDayCount(), 0);
	TestEqual(
		TEXT("time = DAY_LENGTH * 0.42"),
		TimeBits(Sim.GetTime()),
		TimeBits(FAnastasisSimulation::DayLength * 0.42));
	TestEqual(
		TEXT("dayFrac = time / DAY_LENGTH before first midnight"),
		TimeBits(Sim.DayFrac()),
		TimeBits(Sim.GetTime() / FAnastasisSimulation::DayLength));
	TestEqual(TEXT("canonical 96x96"), Sim.GetWorld().Tiles.Num(), kW * kH);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisSimTickDayAdvance,
	"Anastasis.Sim.Tick.DayAdvance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisSimTickDayAdvance::RunTest(const FString&)
{
	FAnastasisSimulation Sim;
	ResetCanonical(Sim);

	const double Step = AnastasisSimClock::FixedDt;
	int32 Ticks = 0;
	double TimeBeforeCross = Sim.GetTime();
	while (Sim.GetDay() == 1)
	{
		TimeBeforeCross = Sim.GetTime();
		Sim.Tick(Step);
		++Ticks;
		if (Ticks > 20000)
		{
			AddError(TEXT("day never advanced"));
			return false;
		}
	}

	TestEqual(TEXT("crossed to day 2"), Sim.GetDay(), 2);
	TestEqual(TEXT("one onNewDay"), Sim.GetNewDayCount(), 1);
	TestTrue(TEXT("time reached DAY_LENGTH"), Sim.GetTime() >= FAnastasisSimulation::DayLength);
	TestTrue(TEXT("previous tick was still day 1"), TimeBeforeCross < FAnastasisSimulation::DayLength);
	TestEqual(TEXT("empty deferred queue"), Sim.GetDeferredRemaining(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisSimTickSkipDaysOneOnNewDay,
	"Anastasis.Sim.Tick.SkipDays",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisSimTickSkipDaysOneOnNewDay::RunTest(const FString&)
{
	FAnastasisSimulation Sim;
	ResetCanonical(Sim);

	// Un gros dt saute day 2: JS n'appelle onNewDay qu'une fois, avec day deja a 3.
	Sim.Tick(FAnastasisSimulation::DayLength * 2.0);
	TestEqual(TEXT("day jumped to 3"), Sim.GetDay(), 3);
	TestEqual(TEXT("still a single onNewDay"), Sim.GetNewDayCount(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisSimTickDeterminism,
	"Anastasis.Sim.Tick.Determinism",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisSimTickDeterminism::RunTest(const FString&)
{
	FAnastasisSimulation A;
	FAnastasisSimulation B;
	ResetCanonical(A);
	ResetCanonical(B);

	TestEqual(TEXT("same world at reset"), A.TileFingerprint(), B.TileFingerprint());

	const double Step = AnastasisSimClock::FixedDt;
	for (int32 i = 0; i < 4000; ++i)
	{
		A.Tick(Step);
		B.Tick(Step);
	}

	TestEqual(TEXT("same seed"), A.GetSeed(), B.GetSeed());
	TestEqual(TEXT("same day"), A.GetDay(), B.GetDay());
	TestEqual(TEXT("same newDay count"), A.GetNewDayCount(), B.GetNewDayCount());
	TestEqual(TEXT("same time bits"), TimeBits(A.GetTime()), TimeBits(B.GetTime()));
	TestEqual(TEXT("same world after ticks"), A.TileFingerprint(), B.TileFingerprint());
	TestTrue(TEXT("day advanced on both"), A.GetDay() >= 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisSimTickWorldFrozen,
	"Anastasis.Sim.Tick.WorldFrozen",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisSimTickWorldFrozen::RunTest(const FString&)
{
	FAnastasisSimulation Sim;
	ResetCanonical(Sim);

	const uint64 Before = Sim.TileFingerprint();
	const int32 TileCount = Sim.GetWorld().Tiles.Num();
	const AnastasisWorld::ETileType OriginType = Sim.GetWorld().Tiles[0].Type;
	const double OriginAlt = Sim.GetWorld().Tiles[0].Alt;

	for (int32 i = 0; i < 5400; ++i)
	{
		Sim.Tick(AnastasisSimClock::FixedDt);
	}

	TestEqual(TEXT("fingerprint frozen"), Sim.TileFingerprint(), Before);
	TestEqual(TEXT("tile count frozen"), Sim.GetWorld().Tiles.Num(), TileCount);
	TestEqual(TEXT("origin type frozen"), static_cast<int32>(Sim.GetWorld().Tiles[0].Type), static_cast<int32>(OriginType));
	TestEqual(TEXT("origin alt bits frozen"), TimeBits(Sim.GetWorld().Tiles[0].Alt), TimeBits(OriginAlt));
	TestTrue(TEXT("clock still moved"), Sim.GetDay() >= 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisSimTickPumpFrame,
	"Anastasis.Sim.Tick.PumpFrame",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisSimTickPumpFrame::RunTest(const FString&)
{
	FAnastasisSimulation A;
	FAnastasisSimulation B;
	ResetCanonical(A);
	ResetCanonical(B);

	// 10x, 1/60 s de mur par frame : un fat step de 10/60 par appel.
	int32 Frames = 0;
	while (A.GetDay() == 1)
	{
		A.PumpFrame(AnastasisSimClock::FixedDt, 10.0);
		B.PumpFrame(AnastasisSimClock::FixedDt, 10.0);
		++Frames;
		if (Frames > 2000)
		{
			AddError(TEXT("pump never crossed midnight"));
			return false;
		}
	}

	TestTrue(TEXT("midnight within a short 10x burst"), Frames > 300 && Frames < 400);
	TestEqual(TEXT("pumped day 2"), A.GetDay(), 2);
	TestEqual(TEXT("one onNewDay"), A.GetNewDayCount(), 1);
	TestEqual(TEXT("pump determinism day"), A.GetDay(), B.GetDay());
	TestEqual(TEXT("pump determinism time"), TimeBits(A.GetTime()), TimeBits(B.GetTime()));
	TestEqual(TEXT("pump determinism world"), A.TileFingerprint(), B.TileFingerprint());
	return true;
}

#endif
