#include "Sim/AnastasisSimulationSubsystem.h"

#include "Anastasis_UnrealV2.h"
#include "Stats/Stats.h"
#include "Core/AnastasisJsNumeric.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "WorldView/AnastasisWorldView.h"

static TAutoConsoleVariable<float> CVarSimSpeed(
	TEXT("anastasis.Sim.Speed"),
	10.0f,
	TEXT("Simulation speed scale (JS 1/2/5/10). Default 10 so midnight is visible in a short PIE. Set 1 for JS realtime."),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarSimOverlay(
	TEXT("anastasis.Sim.Overlay"),
	1,
	TEXT("1 = draw day/time overlay in PIE. 0 = log only."),
	ECVF_Default);

namespace
{
	uint32 SeedFromCVar()
	{
		if (IConsoleVariable* Var = IConsoleManager::Get().FindConsoleVariable(TEXT("anastasis.WorldView.Seed")))
		{
			return static_cast<uint32>(Var->GetInt());
		}
		return AnastasisWorldView::ReferenceSeed;
	}
}

bool UAnastasisSimulationSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld();
}

void UAnastasisSimulationSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	ResetCanonical(SeedFromCVar());
	bPumpFromEngineTick = true;
}

void UAnastasisSimulationSubsystem::ResetCanonical(uint32 Seed)
{
	Simulation.Reset(Seed, AnastasisWorldView::ReferenceWidth, AnastasisWorldView::ReferenceHeight);
	LoggedDay = Simulation.GetDay();
	UE_LOG(
		LogAnastasis_UnrealV2,
		Display,
		TEXT("ANASTASIS_SIM reset seed=%u day=%d time=%.17g tiles=%d speed=%.4g"),
		Simulation.GetSeed(),
		Simulation.GetDay(),
		Simulation.GetTime(),
		Simulation.GetWorld().Tiles.Num(),
		CVarSimSpeed.GetValueOnGameThread());
}

void UAnastasisSimulationSubsystem::Tick(float DeltaTime)
{
	if (!Simulation.IsRunning())
	{
		return;
	}

	const double Speed = static_cast<double>(CVarSimSpeed.GetValueOnGameThread());
	Simulation.PumpFrame(static_cast<double>(DeltaTime), Speed);
	LogDayIfChanged();
	DrawOverlay();
}

TStatId UAnastasisSimulationSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UAnastasisSimulationSubsystem, STATGROUP_Tickables);
}

bool UAnastasisSimulationSubsystem::IsTickable() const
{
	const UWorld* World = GetWorld();
	return bPumpFromEngineTick
		&& !IsTemplate()
		&& World
		&& World->IsGameWorld()
		&& Simulation.IsRunning();
}

void UAnastasisSimulationSubsystem::LogStatus() const
{
	UE_LOG(
		LogAnastasis_UnrealV2,
		Display,
		TEXT("ANASTASIS_SIM status seed=%u day=%d time=%.17g frac=%.17g newDays=%d tiles=%d running=%d"),
		Simulation.GetSeed(),
		Simulation.GetDay(),
		Simulation.GetTime(),
		Simulation.DayFrac(),
		Simulation.GetNewDayCount(),
		Simulation.GetWorld().Tiles.Num(),
		Simulation.IsRunning() ? 1 : 0);
}

void UAnastasisSimulationSubsystem::LogDayIfChanged()
{
	if (Simulation.GetDay() == LoggedDay)
	{
		return;
	}

	LoggedDay = Simulation.GetDay();
	UE_LOG(
		LogAnastasis_UnrealV2,
		Display,
		TEXT("ANASTASIS_SIM_DAY seed=%u day=%d time=%.17g newDays=%d"),
		Simulation.GetSeed(),
		Simulation.GetDay(),
		Simulation.GetTime(),
		Simulation.GetNewDayCount());
}

void UAnastasisSimulationSubsystem::DrawOverlay() const
{
	if (CVarSimOverlay.GetValueOnGameThread() == 0 || !GEngine)
	{
		return;
	}

	const double Hours = Simulation.DayFrac() * 24.0;
	const int32 Hour = static_cast<int32>(AnastasisJs::Floor(Hours));
	const int32 Minute = static_cast<int32>(AnastasisJs::Floor((Hours - static_cast<double>(Hour)) * 60.0));

	GEngine->AddOnScreenDebugMessage(
		0xA51A51,
		0.0f,
		FColor::Cyan,
		FString::Printf(
			TEXT("ANASTASIS  Jour %d  %02d:%02d  t=%.3f  seed=%u"),
			Simulation.GetDay(),
			Hour,
			Minute,
			Simulation.GetTime(),
			Simulation.GetSeed()));
}

static FAutoConsoleCommandWithWorld CmdAnastasisSimStatus(
	TEXT("Anastasis.Sim.Status"),
	TEXT("Logs the live simulation clock: seed, day, time, dayFrac, onNewDay count."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if (!World)
		{
			return;
		}
		if (UAnastasisSimulationSubsystem* Host = World->GetSubsystem<UAnastasisSimulationSubsystem>())
		{
			Host->LogStatus();
		}
		else
		{
			UE_LOG(LogAnastasis_UnrealV2, Warning, TEXT("ANASTASIS_SIM status: no subsystem in this world"));
		}
	}));
