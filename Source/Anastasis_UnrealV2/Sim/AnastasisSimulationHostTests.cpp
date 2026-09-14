#include "Misc/AutomationTest.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Sim/AnastasisSimulationSubsystem.h"
#include "WorldView/AnastasisWorldView.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr uint32 kSeed = 12345u;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisSimTickHostPumps,
	"Anastasis.Sim.Tick.HostPumps",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisSimTickHostPumps::RunTest(const FString&)
{
	UWorld* Fresh = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("scratch game world"), Fresh))
	{
		return false;
	}

	FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
	Context.SetCurrentWorld(Fresh);

	UAnastasisSimulationSubsystem* Host = Fresh->GetSubsystem<UAnastasisSimulationSubsystem>();
	if (!TestNotNull(TEXT("simulation subsystem"), Host))
	{
		GEngine->DestroyWorldContext(Fresh);
		Fresh->DestroyWorld(false);
		return false;
	}

	Host->ResetCanonical(kSeed);
	FAnastasisSimulation& Sim = Host->GetSimulation();
	TestEqual(TEXT("PIE host uses WorldView seed"), Sim.GetSeed(), kSeed);
	TestEqual(TEXT("canonical tiles"), Sim.GetWorld().Tiles.Num(), AnastasisWorldView::ReferenceWidth * AnastasisWorldView::ReferenceHeight);
	TestEqual(TEXT("starts day 1"), Sim.GetDay(), 1);

	const uint64 Frozen = Sim.TileFingerprint();
	int32 Frames = 0;
	while (Sim.GetDay() == 1)
	{
		Sim.PumpFrame(1.0 / 60.0, 10.0);
		++Frames;
		if (Frames > 2000)
		{
			AddError(TEXT("host never crossed midnight"));
			GEngine->DestroyWorldContext(Fresh);
			Fresh->DestroyWorld(false);
			return false;
		}
	}

	TestEqual(TEXT("day changed in the game-world host"), Sim.GetDay(), 2);
	TestEqual(TEXT("one onNewDay"), Sim.GetNewDayCount(), 1);
	TestEqual(TEXT("world still frozen"), Sim.TileFingerprint(), Frozen);

	FAnastasisSimulation Twin;
	Twin.Reset(kSeed, AnastasisWorldView::ReferenceWidth, AnastasisWorldView::ReferenceHeight);
	for (int32 Frame = 0; Frame < Frames; ++Frame)
	{
		Twin.PumpFrame(1.0 / 60.0, 10.0);
	}
	TestEqual(TEXT("host matches headless twin day"), Sim.GetDay(), Twin.GetDay());
	TestEqual(TEXT("host matches headless twin newDays"), Sim.GetNewDayCount(), Twin.GetNewDayCount());

	uint64 HostTime = 0;
	uint64 TwinTime = 0;
	const double HostTimeValue = Sim.GetTime();
	const double TwinTimeValue = Twin.GetTime();
	FMemory::Memcpy(&HostTime, &HostTimeValue, sizeof(double));
	FMemory::Memcpy(&TwinTime, &TwinTimeValue, sizeof(double));
	TestEqual(TEXT("host matches headless twin time bits"), HostTime, TwinTime);

	GEngine->DestroyWorldContext(Fresh);
	Fresh->DestroyWorld(false);
	return true;
}

#endif
