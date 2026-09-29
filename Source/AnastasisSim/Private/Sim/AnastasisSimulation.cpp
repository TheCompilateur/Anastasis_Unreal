#include "Sim/AnastasisSimulation.h"

#include "Core/AnastasisJsNumeric.h"
#include "Core/AnastasisSimClock.h"

FAnastasisSimulation::FAnastasisSimulation() = default;

void FAnastasisSimulation::Reset(uint32 SeedValue, int32 Width, int32 Height)
{
	bBootDeferred = false;
	Seed = SeedValue;
	World = AnastasisWorld::GenerateWorld(Seed, Width, Height);
	Village.Bind(World);
	// JS: this.time = DAY_LENGTH * 0.42; this.day = 1;
	Time = DayLength * 0.42;
	Day = 1;
	NewDayCount = 0;
	DeferredRemaining = 0;
	Accumulator = 0.0;
}

void FAnastasisSimulation::Tick(double Dt)
{
	if (bBootDeferred)
	{
		return;
	}

	Time += Dt;
	const int32 NewDay = 1 + static_cast<int32>(AnastasisJs::Floor(Time / DayLength));
	if (NewDay != Day)
	{
		Day = NewDay;
		OnNewDay(/*bDefer=*/true);
	}

	ProcessDayDeferred(DayDeferredJobsPerTick);

	// JS: le LOD logique peut sortir ici (non porte). Puis :
	// `for (const npc of this.actors) updateNpc(this, npc, dt);`
	Village.UpdateActors(Time, Dt);
}

int32 FAnastasisSimulation::PumpFrame(double WallSeconds, double Speed)
{
	if (bBootDeferred)
	{
		return 0;
	}

	const AnastasisSimClock::FFrameDelta Frame = AnastasisSimClock::FrameDelta(WallSeconds * 1000.0);
	const AnastasisSimClock::FStepPlan Plan = AnastasisSimClock::StepPlan(Speed);
	const double Scale = FMath::Max(1.0, AnastasisJs::NumberOr(Speed, 1.0));

	Accumulator += FMath::Max(0.0, Frame.Dt) * Scale;

	int32 Steps = 0;
	const int32 StepCap = FMath::Max(1, Plan.TargetSteps);
	while (Accumulator >= Plan.StepDt && Steps < StepCap)
	{
		Tick(Plan.StepDt);
		Accumulator -= Plan.StepDt;
		++Steps;
	}

	const double MaxLag = Plan.StepDt * static_cast<double>(StepCap);
	if (Accumulator > MaxLag)
	{
		Accumulator = MaxLag;
	}
	return Steps;
}

double FAnastasisSimulation::DayFrac() const
{
	return FMath::Fmod(Time, DayLength) / DayLength;
}

uint64 FAnastasisSimulation::TileFingerprint() const
{
	uint64 Hash = 14695981039346656037ull;
	for (const AnastasisWorld::FTile& Tile : World.Tiles)
	{
		Hash ^= static_cast<uint64>(static_cast<uint8>(Tile.Type));
		Hash *= 1099511628211ull;
		uint64 AltBits = 0;
		FMemory::Memcpy(&AltBits, &Tile.Alt, sizeof(double));
		Hash ^= AltBits;
		Hash *= 1099511628211ull;
	}
	return Hash;
}

void FAnastasisSimulation::OnNewDay(bool bDefer)
{
	++NewDayCount;
	// Eco / life / clio : NOT_IMPLEMENTED. La file reste vide, que defer soit
	// true (tick runtime) ou false (appel direct / verifies).
	if (!bDefer)
	{
		DeferredRemaining = 0;
	}
}

void FAnastasisSimulation::ProcessDayDeferred(int32 /*MaxJobs*/)
{
	// DETERMINISME JS: budget en NOMBRE de jobs, jamais en duree. File vide.
	DeferredRemaining = 0;
}
