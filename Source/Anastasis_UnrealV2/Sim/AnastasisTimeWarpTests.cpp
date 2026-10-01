#include "Misc/AutomationTest.h"

#include "Core/AnastasisSimClock.h"
#include "Sim/AnastasisSimulation.h"
#include "Sim/AnastasisTimeWarp.h"
#include "WorldView/AnastasisWorldView.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr uint32 kWarpSeed = 12345u;
	constexpr double kWarpDay = FAnastasisSimulation::DayLength;

	void ResetWarpReference(FAnastasisSimulation& Sim)
	{
		Sim.Reset(kWarpSeed, AnastasisWorldView::ReferenceWidth, AnastasisWorldView::ReferenceHeight);
	}

	uint64 WarpTimeBits(double Value)
	{
		uint64 Out = 0;
		FMemory::Memcpy(&Out, &Value, sizeof(double));
		return Out;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisTimeWarpPresets,
	"Anastasis.Sim.TimeWarp.Presets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisTimeWarpPresets::RunTest(const FString&)
{
	using namespace AnastasisTimeWarp;
	TestEqual(TEXT("x1 -> x2"), StepPreset(1.0, +1), 2.0);
	TestEqual(TEXT("x2 -> x1"), StepPreset(2.0, -1), 1.0);
	TestEqual(TEXT("between presets, faster"), StepPreset(3.0, +1), 4.0);
	TestEqual(TEXT("between presets, slower"), StepPreset(3.0, -1), 2.0);
	TestEqual(TEXT("top stays"), StepPreset(128.0, +1), 128.0);
	TestEqual(TEXT("bottom stays"), StepPreset(0.25, -1), 0.25);
	TestEqual(TEXT("pause, faster resumes at x1"), StepPreset(0.0, +1), 1.0);
	TestEqual(TEXT("above the presets, slower falls to the top one"), StepPreset(500.0, -1), 128.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisTimeWarpParseAdvance,
	"Anastasis.Sim.TimeWarp.ParseAdvance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisTimeWarpParseAdvance::RunTest(const FString&)
{
	using namespace AnastasisTimeWarp;
	const double Now = kWarpDay * 0.42; // time0 de la reference, ~10:05
	double S = 0.0;
	FString Err;

	TestTrue(TEXT("45"), ParseAdvance(TEXT("45"), Now, kWarpDay, S, Err));
	TestEqual(TEXT("45 -> 45 s"), S, 45.0);
	TestTrue(TEXT("45s"), ParseAdvance(TEXT("45s"), Now, kWarpDay, S, Err));
	TestEqual(TEXT("45s -> 45 s"), S, 45.0);
	TestTrue(TEXT("3d"), ParseAdvance(TEXT("3d"), Now, kWarpDay, S, Err));
	TestEqual(TEXT("3d -> 3 days"), S, 3.0 * kWarpDay);
	TestTrue(TEXT("6h"), ParseAdvance(TEXT("6h"), Now, kWarpDay, S, Err));
	TestEqual(TEXT("6h -> a quarter day"), S, kWarpDay / 4.0);
	TestTrue(TEXT("@22"), ParseAdvance(TEXT("@22"), Now, kWarpDay, S, Err));
	TestEqual(TEXT("@22 from 0.42 -> same day"), S, (22.0 / 24.0 - 0.42) * kWarpDay, 1e-9);
	TestTrue(TEXT("@10"), ParseAdvance(TEXT("@10"), Now, kWarpDay, S, Err));
	TestEqual(TEXT("@10 already passed -> tomorrow"), S, (10.0 / 24.0 + 1.0 - 0.42) * kWarpDay, 1e-9);
	TestTrue(TEXT("@6:30"), ParseAdvance(TEXT("@6:30"), Now, kWarpDay, S, Err));
	TestEqual(TEXT("@6:30 -> tomorrow morning"), S, (6.5 / 24.0 + 1.0 - 0.42) * kWarpDay, 1e-9);
	TestTrue(TEXT("@22 on day 5 uses the time of day"), ParseAdvance(TEXT("@22"), Now + 4.0 * kWarpDay, kWarpDay, S, Err));
	TestEqual(TEXT("@22 on day 5"), S, (22.0 / 24.0 - 0.42) * kWarpDay, 1e-6);

	TestFalse(TEXT("empty refused"), ParseAdvance(TEXT(""), Now, kWarpDay, S, Err));
	TestFalse(TEXT("text refused"), ParseAdvance(TEXT("night"), Now, kWarpDay, S, Err));
	TestFalse(TEXT("negative refused"), ParseAdvance(TEXT("-5"), Now, kWarpDay, S, Err));
	TestFalse(TEXT("zero refused"), ParseAdvance(TEXT("0d"), Now, kWarpDay, S, Err));
	TestFalse(TEXT("hour 25 refused"), ParseAdvance(TEXT("@25"), Now, kWarpDay, S, Err));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisTimeWarpAdvance,
	"Anastasis.Sim.TimeWarp.Advance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisTimeWarpAdvance::RunTest(const FString&)
{
	FAnastasisSimulation Sim;
	TestEqual(TEXT("advance before reset is a no-op"), AnastasisTimeWarp::Advance(Sim, kWarpDay), 0);

	ResetWarpReference(Sim);
	const double T0 = Sim.GetTime();
	const uint64 Frozen = Sim.TileFingerprint();
	const int32 Steps = AnastasisTimeWarp::Advance(Sim, kWarpDay);
	TestEqual(TEXT("one day = 540 steps of 1/6 s"), Steps, 540);
	TestEqual(TEXT("exactly one day later"), Sim.GetTime(), T0 + kWarpDay, 1e-9);
	TestEqual(TEXT("day 2"), Sim.GetDay(), 2);
	TestEqual(TEXT("one onNewDay"), Sim.GetNewDayCount(), 1);
	TestEqual(TEXT("world still frozen"), Sim.TileFingerprint(), Frozen);

	// Le dernier pas est raccourci : la duree demandee tombe juste, sans arrondi au pas.
	const double T1 = Sim.GetTime();
	TestEqual(TEXT("0.05 s -> one short step"), AnastasisTimeWarp::Advance(Sim, 0.05), 1);
	TestEqual(TEXT("0.05 s exactly"), Sim.GetTime(), T1 + 0.05, 1e-9);

	// Une semaine d'un coup : sept minuits, chacun vu.
	AnastasisTimeWarp::Advance(Sim, 7.0 * kWarpDay);
	TestEqual(TEXT("a week later: day 9"), Sim.GetDay(), 9);
	TestEqual(TEXT("eight onNewDay"), Sim.GetNewDayCount(), 8);

	// Deterministe : un jumeau qui avance pareil tombe sur les memes bits.
	FAnastasisSimulation Twin;
	ResetWarpReference(Twin);
	AnastasisTimeWarp::Advance(Twin, kWarpDay);
	AnastasisTimeWarp::Advance(Twin, 0.05);
	AnastasisTimeWarp::Advance(Twin, 7.0 * kWarpDay);
	TestEqual(TEXT("twin time bits"), WarpTimeBits(Twin.GetTime()), WarpTimeBits(Sim.GetTime()));
	TestEqual(TEXT("twin newDays"), Twin.GetNewDayCount(), Sim.GetNewDayCount());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisTimeWarpPump,
	"Anastasis.Sim.TimeWarp.Pump",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisTimeWarpPump::RunTest(const FString&)
{
	const double Frame = 1.0 / 60.0;

	// x8 : 10 s reelles donnent 80 s simulees, a un pas pres (8/60 s).
	{
		FAnastasisSimulation Sim;
		ResetWarpReference(Sim);
		AnastasisTimeWarp::FWarpPump Pump;
		const double T0 = Sim.GetTime();
		int32 MaxSteps = 0;
		for (int32 I = 0; I < 600; ++I)
		{
			MaxSteps = FMath::Max(MaxSteps, Pump.Pump(Sim, Frame, 8.0, 0.0).Steps);
		}
		// Un pas de 8/60 s rentre pile dans une frame : l'arrondi peut en laisser un en attente.
		TestEqual(TEXT("x8 for 10 s -> 80 s simulated"), Sim.GetTime() - T0, 80.0, 2.0 * 8.0 / 60.0);
		TestTrue(TEXT("x8 is one fat step a frame, not eight fine ones"), MaxSteps >= 1 && MaxSteps <= 2);
	}

	// x1000 : le pas reste a 10 x FixedDt, ce sont les pas par frame qui montent.
	{
		FAnastasisSimulation Sim;
		ResetWarpReference(Sim);
		AnastasisTimeWarp::FWarpPump Pump;
		const double T0 = Sim.GetTime();
		int32 MaxSteps = 0;
		for (int32 I = 0; I < 60; ++I)
		{
			MaxSteps = FMath::Max(MaxSteps, Pump.Pump(Sim, Frame, 1000.0, 0.0).Steps);
		}
		const double StepDt = AnastasisSimClock::FixedDt * AnastasisSimClock::MaxStepMult;
		TestEqual(TEXT("x1000 for 1 s -> 1000 s simulated"), Sim.GetTime() - T0, 1000.0, StepDt + 1e-9);
		TestTrue(TEXT("x1000 runs ~100 steps a frame"), MaxSteps >= 99 && MaxSteps <= 101);
		TestTrue(TEXT("x1000 crossed eleven midnights"), Sim.GetNewDayCount() >= 11);
	}

	// Pause : rien ne bouge, l'accumulateur non plus.
	{
		FAnastasisSimulation Sim;
		ResetWarpReference(Sim);
		AnastasisTimeWarp::FWarpPump Pump;
		const double T0 = Sim.GetTime();
		for (int32 I = 0; I < 60; ++I)
		{
			Pump.Pump(Sim, Frame, 0.0, 0.0);
		}
		TestEqual(TEXT("pause: time frozen"), Sim.GetTime(), T0);
		TestEqual(TEXT("pause: nothing accumulated"), Pump.Accumulator, 0.0);
	}

	// Budget depasse : un pas par frame au moins, le retard est abandonne, la coupe signalee.
	{
		FAnastasisSimulation Sim;
		ResetWarpReference(Sim);
		AnastasisTimeWarp::FWarpPump Pump;
		const AnastasisTimeWarp::FPumpResult R = Pump.Pump(Sim, 0.1, 1000.0, 1e-9);
		const double StepDt = AnastasisSimClock::FixedDt * AnastasisSimClock::MaxStepMult;
		TestEqual(TEXT("budget: one step still runs"), R.Steps, 1);
		TestTrue(TEXT("budget: cut reported"), R.bBudgetCut);
		TestTrue(TEXT("budget: backlog dropped"), Pump.Accumulator <= StepDt);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisTimeWarpWitness,
	"Anastasis.Sim.TimeWarp.Witness",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisTimeWarpWitness::RunTest(const FString&)
{
	using AnastasisTimeWarp::FWitness;
	const double Infinite = TNumericLimits<double>::Max();

	FWitness Normal;
	Normal.Observe(10.0 * kWarpDay, 1.0, kWarpDay);
	TestEqual(TEXT("x1: fully present"), Normal.Presence, 1.0);
	TestEqual(TEXT("x1: never idle"), Normal.IdleSeconds, 0.0);

	FWitness Slow;
	Slow.Observe(kWarpDay, 0.25, kWarpDay);
	TestEqual(TEXT("slow motion is not idle"), Slow.IdleSeconds, 0.0);

	// Le PNJ qui regarde le joueur avancer le temps une semaine : un type qui ne fait rien.
	FWitness Week;
	Week.Observe(7.0 * kWarpDay, Infinite, kWarpDay);
	TestEqual(TEXT("a skipped week: 7 idle days"), Week.IdleDays(kWarpDay), 7.0, 1e-9);
	TestEqual(TEXT("a skipped week: presence e^-3.5"), Week.Presence, FMath::Exp(-3.5), 1e-9);
	TestTrue(TEXT("a skipped week: nearly invisible"), Week.Presence < 0.05);

	// Puis un jour joue normalement : il revient, l'oisivete reste.
	Week.Observe(kWarpDay, 1.0, kWarpDay);
	TestEqual(TEXT("one day back: presence recovers"), Week.Presence, 1.0 - (1.0 - FMath::Exp(-3.5)) * FMath::Exp(-1.0), 1e-9);
	TestEqual(TEXT("one day back: idle days stay"), Week.IdleDays(kWarpDay), 7.0, 1e-9);

	// x2 : la moitie du temps simule est oisive.
	FWitness Double;
	Double.Observe(2.0 * kWarpDay, 2.0, kWarpDay);
	TestEqual(TEXT("x2 for two days: one idle day"), Double.IdleDays(kWarpDay), 1.0, 1e-9);

	// Progressif : plus on accelere, moins on est vu, a temps simule egal.
	FWitness X4;
	FWitness X64;
	X4.Observe(3.0 * kWarpDay, 4.0, kWarpDay);
	X64.Observe(3.0 * kWarpDay, 64.0, kWarpDay);
	TestTrue(TEXT("x64 fades more than x4"), X64.Presence < X4.Presence);
	return true;
}

#endif
