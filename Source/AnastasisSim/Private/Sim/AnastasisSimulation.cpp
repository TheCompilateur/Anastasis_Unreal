#include "Sim/AnastasisSimulation.h"

#include "Core/AnastasisJsNumeric.h"
#include "Core/AnastasisSimClock.h"
#include "Core/AnastasisStateDigest.h"

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
	// `readSimWeather` lit `sim.seed` : les habitants voient le ciel que le rendu montre.
	Village.SetWeatherSeed(Seed);
	Accumulator = 0.0;
	// ecart n°38 : un monde neuf n'a pas de dehors tant qu'un hote n'en charge pas.
	Geo.Unload();
}

void FAnastasisSimulation::ResetFromWorld(uint32 SeedValue, AnastasisWorld::FWorld&& InWorld, double InTime, int32 InDay)
{
	bBootDeferred = false;
	Seed = SeedValue;
	World = MoveTemp(InWorld);
	Village.Bind(World);
	Time = InTime;
	Day = InDay;
	NewDayCount = 0;
	DeferredRemaining = 0;
	DeferredJobs.Reset();
	LastRegrownFields = 0;
	Village.SetRngSeed(Seed);
	Village.SetWeatherSeed(Seed);
	Accumulator = 0.0;
	// ecart n°38 : un monde neuf n'a pas de dehors tant qu'un hote n'en charge pas.
	Geo.Unload();
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

int32 FAnastasisSimulation::AdmitGeoMigration()
{
	// ecart n°38 : l'adaptateur local. Le monde exterieur cree le groupe, le village l'instancie ;
	// ensuite ces habitants sont des habitants comme les autres.
	int32 Created = 0;
	for (const int32 BatchIndex : Geo.PendingMigration())
	{
		const AnastasisGeo::FMigrationBatch& Batch = Geo.GetMigrationBatches()[BatchIndex];
		const double Angle = GeoArrivalAngleStep * static_cast<double>(BatchIndex);
		const TArray<FString> Ids = Village.IsBound()
			? Village.AdmitExternalArrivals(Batch.Persons, GeoArrivalRadius, Angle)
			: TArray<FString>();
		Created += Ids.Num();
		Geo.RecordAdmission(BatchIndex, Ids);
	}
	return Created;
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

uint64 FAnastasisSimulation::StateDigest() const
{
	// Le monde ne change pas apres la generation (seul le harnais le reecrit) ; il entre quand meme,
	// en entier, par une empreinte d'octets : une ecriture dans une tuile se verrait.
	AnastasisDigest::FFnv1a64 WorldHash;
	const auto Mix = [&WorldHash](const auto& Value)
	{
		WorldHash.Bytes(reinterpret_cast<const uint8*>(&Value), static_cast<int32>(sizeof(Value)));
	};
	Mix(World.W);
	Mix(World.H);
	for (const AnastasisWorld::FTile& Tile : World.Tiles)
	{
		Mix(Tile.X); Mix(Tile.Y); Mix(Tile.Type); Mix(Tile.Resource); Mix(Tile.Amount); Mix(Tile.Alt);
		Mix(Tile.Shade); Mix(Tile.Shore); Mix(Tile.Wetness); Mix(Tile.FlowX); Mix(Tile.FlowZ); Mix(Tile.FlowAmt);
		Mix(Tile.CropId); Mix(Tile.Fertility); Mix(Tile.ForestMargin); Mix(Tile.bHasForestMargin);
	}

	AnastasisDigest::FStateWriter Out;
	Out.BeginObject();
	Out.Key(TEXT("bootDeferred")).Bool(bBootDeferred);
	Out.Key(TEXT("seed")).Number(Seed);
	Out.Key(TEXT("time")).Number(Time);
	Out.Key(TEXT("day")).Number(Day);
	Out.Key(TEXT("newDayCount")).Number(NewDayCount);
	Out.Key(TEXT("deferredRemaining")).Number(DeferredRemaining);
	Out.Key(TEXT("deferredJobs")).BeginArray(DeferredJobs.Num());
	for (const int32 Job : DeferredJobs) Out.Number(Job);
	Out.EndArray();
	Out.Key(TEXT("lastRegrownFields")).Number(LastRegrownFields);
	Out.Key(TEXT("accumulator")).Number(Accumulator);
	Out.Key(TEXT("world")).String(AnastasisDigest::ToHex(WorldHash.Hash));
	Out.Key(TEXT("village")).String(AnastasisDigest::ToHex(Village.StateDigest()));
	// Le monde exterieur (geopolitical-world-001) : tout son etat vivant passe par SaveState, que son
	// Digest hache ; le scenario charge n'y est pas, il se recharge depuis sa source.
	Out.Key(TEXT("geoLoaded")).Bool(Geo.IsLoaded());
	Out.Key(TEXT("geo")).String(AnastasisDigest::ToHex(Geo.Digest()));
	Out.EndObject();
	return Out.Digest();
}

void FAnastasisSimulation::OnNewDay(bool bDefer)
{
	++NewDayCount;
	// Section critique de minuit, « eco + logements » : de la reference n'est porte
	// que `assignSheltersDaily` (les sans-toit recoivent un lit). Achats de maison,
	// agrandissements, loyers : economie, non portee.
	Village.AssignSheltersDaily();
	// `updateReputationDaily` (player-minimal-001) : seule l'oisivete y est portee ; un habitant
	// jamais oisif reste a 50, au bit pres.
	Village.UpdateReputationDaily();
	// ecart n°38 (geopolitical-world-001) : le monde exterieur, s'il est charge, avance jusqu'a ce
	// jour, puis ses groupes d'arrivants deviennent des habitants. Decharge : rien, au bit pres.
	if (Geo.IsLoaded())
	{
		Geo.AdvanceToDay(Day);
		AdmitGeoMigration();
	}
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
	else if (Job == DayJobLifeDaily)
	{
		// `updateLifeDaily` : ni `agePopulation` ni les tirages de mortalite ne sont portes (ecart n°28).
		Village.UpdateMortalityDaily();
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
