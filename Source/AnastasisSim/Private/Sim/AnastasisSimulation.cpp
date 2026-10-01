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
	DeferredJobs.Reset();
	LastRegrownFields = 0;
	// Le flux de tirages du village (rumeurs) suit la graine du monde.
	Village.SetRngSeed(Seed);
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
	// Section critique de minuit, « eco + logements » : de la reference n'est porte
	// que `assignSheltersDaily` (les sans-toit recoivent un lit). Achats de maison,
	// agrandissements, loyers : economie, non portee.
	Village.AssignSheltersDaily();
	// `enqueueDayDeferred` : les 17 travaux, ajoutes derriere un eventuel reliquat.
	// Portes : `landRegen` (regen du sol) et `memory` (oubli). Collectif, ordres, doctrine,
	// chapitres, chartes, sites, transports, routes, guets, vie, carrieres, fondateurs,
	// conseils, betail, immigration : NOT_IMPLEMENTED, ils ne font que tenir leur rang.
	for (int32 Job = 0; Job < DayDeferredJobCount; ++Job)
	{
		DeferredJobs.Add(Job);
	}
	DeferredRemaining = DeferredJobs.Num();
	if (!bDefer)
	{
		// `flushDayDeferred` : tout, tout de suite.
		ProcessDayDeferred(DeferredJobs.Num());
	}
}

void FAnastasisSimulation::ProcessDayDeferred(int32 MaxJobs)
{
	// DETERMINISME JS: budget en NOMBRE de jobs, jamais en duree.
	int32 Done = 0;
	while (Done < MaxJobs && DeferredJobs.Num() > 0)
	{
		const int32 Job = DeferredJobs[0];
		DeferredJobs.RemoveAt(0);
		RunDayJob(Job);
		++Done;
	}
	DeferredRemaining = DeferredJobs.Num();
}

void FAnastasisSimulation::RunDayJob(int32 Job)
{
	if (Job == DayJobLandRegen)
	{
		RunLandRegen();
	}
	else if (Job == DayJobMemory)
	{
		// `memory` : forgetStale + forgetStalePeople pour chacun (fadeEpisodes : non porte).
		Village.ForgetStaleDaily(Day);
	}
}

void FAnastasisSimulation::RunLandRegen()
{
	// `sim.regrowFieldsDaily(); sim.regrowForestDaily(); syncAllYardsFromStock(sim);`
	// La foret : `regrowWoodTile` rend toujours false dans la reference (fee66ae),
	// rien ne repousse. Les cours de stock sont une vue, pas de l'etat.
	LastRegrownFields = Village.RegrowFieldsDaily(Day);
}
