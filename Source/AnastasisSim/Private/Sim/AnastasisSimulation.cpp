#include "Sim/AnastasisSimulation.h"

#include "Core/AnastasisJsNumeric.h"
#include "Core/AnastasisSimClock.h"
#include "Core/AnastasisStateArchive.h"
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

int32 FAnastasisSimulation::ApplyWaterMask(const TArray<uint8>& Water)
{
	// ecart n°51 : l'eau suit le reseau ; seul un monde neuf, sans habitants, la recoit.
	if (Water.Num() != World.Tiles.Num() || !Village.GetActors().IsEmpty() || !Village.GetBuildings().IsEmpty()) return -1;
	const int32 Changed = AnastasisWorld::RestampWater(World, Water);
	if (Changed > 0) Village.Bind(World);
	return Changed;
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
		// ecart n°53 : ce qu'ils fuient (le recit du choc, a defaut son nom) et d'ou ils viennent.
		FString Cause;
		if (const AnastasisGeo::FShockDef* Shock = Geo.GetShocks().FindByPredicate([&Batch](const AnastasisGeo::FShockDef& S) { return S.Id == Batch.CauseId; }))
		{
			Cause = Shock->Telling.IsEmpty() ? Shock->Label : Shock->Telling;
		}
		FString Origin = Batch.OriginNode;
		if (const AnastasisGeo::FNode* Node = Geo.GetScenario().FindNode(Batch.OriginNode))
		{
			// « Paipert (Bayburt), ville caravaniere » -> « Paipert » : le nom que l'on dit.
			Origin = Node->Label;
			for (const TCHAR* Cut : { TEXT(" ("), TEXT(" :"), TEXT(",") })
			{
				const int32 At = Origin.Find(Cut);
				if (At > 0) Origin.LeftInline(At);
			}
			Origin.RemoveFromStart(TEXT("Bandon de "));
		}
		const TArray<FString> Ids = Village.IsBound()
			? Village.AdmitExternalArrivals(Batch.Persons, GeoArrivalRadius, Angle, Cause, Origin, Day)
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
	AnastasisDigest::FStateWriter Out;
	AnastasisArchive::FStateArchive Ar = AnastasisArchive::FStateArchive::ForHash(Out);
	FString GeoScenarioId;
	FString GeoJson;
	// Le hachage ne fait que lire (Core/AnastasisStateArchive.h).
	const_cast<FAnastasisSimulation*>(this)->ArchiveState(Ar, GeoScenarioId, GeoJson);
	return Out.Digest();
}

void FAnastasisSimulation::ArchiveState(AnastasisArchive::FStateArchive& Ar, FString& GeoScenarioId, FString& GeoJson)
{
	Ar.BeginObject();
	Ar.Key(TEXT("bootDeferred")).Bool(bBootDeferred);
	Ar.Key(TEXT("seed")).Number(Seed);
	Ar.Key(TEXT("time")).Number(Time);
	Ar.Key(TEXT("day")).Number(Day);
	Ar.Key(TEXT("newDayCount")).Number(NewDayCount);
	Ar.Key(TEXT("deferredRemaining")).Number(DeferredRemaining);
	Ar.Key(TEXT("deferredJobs"));
	AnastasisArchive::VisitArray(Ar, DeferredJobs, [](AnastasisArchive::FStateArchive& A, int32& Job) { A.Number(Job); });
	Ar.Key(TEXT("lastRegrownFields")).Number(LastRegrownFields);
	Ar.Key(TEXT("accumulator")).Number(Accumulator);

	Ar.Key(TEXT("world"));
	if (Ar.IsHashing())
	{
		// Le monde ne change pas apres la generation (seul le harnais et le reseau d'eau le reecrivent) ; il
		// entre quand meme, en entier, par une empreinte d'octets : une ecriture dans une tuile se verrait.
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
		FString Hex = AnastasisDigest::ToHex(WorldHash.Hash);
		Ar.String(Hex);
	}
	else
	{
		// Sauve : chaque tuile, champ par champ. Relu : sur le monde que Reset a regenere a la meme taille
		// (la generation peut changer entre deux versions du jeu ; la sauvegarde, elle, garde ses tuiles).
		Ar.BeginObject();
		int32 W = World.W;
		int32 H = World.H;
		Ar.Key(TEXT("w")).Number(W);
		Ar.Key(TEXT("h")).Number(H);
		Ar.Expect(W, World.W, TEXT("largeur du monde"));
		Ar.Expect(H, World.H, TEXT("hauteur du monde"));
		Ar.Key(TEXT("tiles"));
		AnastasisArchive::VisitArray(Ar, World.Tiles, [](AnastasisArchive::FStateArchive& A, AnastasisWorld::FTile& Tile)
		{
			AnastasisArchive::VisitTile(A, Tile);
		});
		Ar.Expect(World.Tiles.Num(), World.W * World.H, TEXT("tuiles du monde"));
		Ar.EndObject();
	}

	Ar.Key(TEXT("village"));
	if (Ar.IsHashing())
	{
		FString Hex = AnastasisDigest::ToHex(Village.StateDigest());
		Ar.String(Hex);
	}
	else
	{
		Village.ArchiveState(Ar);
	}

	// Le monde exterieur (geopolitical-world-001) : tout son etat vivant passe par SaveState, que son
	// Digest hache ; le scenario charge n'y est pas, il se recharge depuis sa source.
	bool bGeoLoaded = Geo.IsLoaded();
	Ar.Key(TEXT("geoLoaded")).Bool(bGeoLoaded);
	Ar.Key(TEXT("geo"));
	if (Ar.IsHashing())
	{
		FString Hex = AnastasisDigest::ToHex(Geo.Digest());
		Ar.String(Hex);
	}
	else
	{
		if (Ar.IsSaving())
		{
			GeoScenarioId = bGeoLoaded ? Geo.GetScenario().Id : FString();
			GeoJson = bGeoLoaded ? Geo.SaveState() : FString();
		}
		Ar.BeginObject();
		Ar.Key(TEXT("scenario")).String(GeoScenarioId);
		Ar.Key(TEXT("state")).String(GeoJson);
		Ar.EndObject();
		if (Ar.IsLoading() && Ar.Ok() && !bGeoLoaded && !GeoJson.IsEmpty())
		{
			Ar.Fail(TEXT("etat exterieur present sans monde exterieur charge"));
		}
		if (Ar.IsLoading()) GeoScenarioId = bGeoLoaded ? GeoScenarioId : FString();
	}
	Ar.EndObject();
}

namespace
{
	// "ANSV" : ANastasis SaVe.
	constexpr uint8 SaveMagic[4] = { 'A', 'N', 'S', 'V' };

	void ArchiveHeader(AnastasisArchive::FStateArchive& Ar, FAnastasisSimulation::FSaveHeader& H)
	{
		Ar.BeginObject();
		Ar.Key(TEXT("version")).Number(H.Version);
		Ar.Key(TEXT("seed")).Number(H.Seed);
		Ar.Key(TEXT("width")).Number(H.Width);
		Ar.Key(TEXT("height")).Number(H.Height);
		Ar.Key(TEXT("time")).Number(H.Time);
		Ar.Key(TEXT("day")).Number(H.Day);
		Ar.Key(TEXT("geoLoaded")).Bool(H.bGeoLoaded);
		Ar.Key(TEXT("geoScenario")).String(H.GeoScenarioId);
		Ar.EndObject();
	}

	/** Lit magie + en-tete ; rend la position du corps, ou INDEX_NONE. */
	int32 ReadHeaderAt(const TArray<uint8>& Bytes, FAnastasisSimulation::FSaveHeader& OutHeader, FString& OutError)
	{
		if (Bytes.Num() < 4 || FMemory::Memcmp(Bytes.GetData(), SaveMagic, 4) != 0)
		{
			OutError = TEXT("pas une sauvegarde ANASTASIS (signature ANSV absente)");
			return INDEX_NONE;
		}
		AnastasisArchive::FStateArchive Ar = AnastasisArchive::FStateArchive::ForLoad(Bytes, 4);
		ArchiveHeader(Ar, OutHeader);
		if (!Ar.Ok())
		{
			OutError = FString::Printf(TEXT("en-tete illisible : %s"), *Ar.GetError());
			return INDEX_NONE;
		}
		if (OutHeader.Version != FAnastasisSimulation::SaveFormatVersion)
		{
			OutError = FString::Printf(TEXT("format %d, ce jeu lit le format %d"), OutHeader.Version, FAnastasisSimulation::SaveFormatVersion);
			return INDEX_NONE;
		}
		if (OutHeader.Width <= 0 || OutHeader.Height <= 0 || OutHeader.Width > 4096 || OutHeader.Height > 4096)
		{
			OutError = FString::Printf(TEXT("monde de %d x %d"), OutHeader.Width, OutHeader.Height);
			return INDEX_NONE;
		}
		return Ar.Tell();
	}
}

// ecart n°45 : format propre (la reference sauve par serialize(sim), un JSON partiel), produit par le
// parcours de StateDigest.
void FAnastasisSimulation::SaveState(TArray<uint8>& OutBytes) const
{
	OutBytes.Reset();
	OutBytes.Append(SaveMagic, 4);
	FSaveHeader Header;
	Header.Version = SaveFormatVersion;
	Header.Seed = Seed;
	Header.Width = World.W;
	Header.Height = World.H;
	Header.Time = Time;
	Header.Day = Day;
	Header.bGeoLoaded = Geo.IsLoaded();
	Header.GeoScenarioId = Geo.IsLoaded() ? Geo.GetScenario().Id : FString();
	AnastasisArchive::FStateArchive Ar = AnastasisArchive::FStateArchive::ForSave(OutBytes);
	ArchiveHeader(Ar, Header);
	FString GeoScenarioId;
	FString GeoJson;
	// La sauvegarde ne fait que lire l'etat (le parcours n'ecrit qu'en lecture d'archive).
	const_cast<FAnastasisSimulation*>(this)->ArchiveState(Ar, GeoScenarioId, GeoJson);
}

bool FAnastasisSimulation::ReadSaveHeader(const TArray<uint8>& Bytes, FSaveHeader& OutHeader, FString& OutError)
{
	return ReadHeaderAt(Bytes, OutHeader, OutError) != INDEX_NONE;
}

bool FAnastasisSimulation::LoadState(const TArray<uint8>& Bytes, FString& OutError, const AnastasisGeo::FScenario* GeoScenario)
{
	// Une premiere lecture complete sur une simulation d'essai : un fichier refuse ne touche pas celle-ci.
	{
		TUniquePtr<FAnastasisSimulation> Probe = MakeUnique<FAnastasisSimulation>();
		if (!Probe->LoadStateInto(Bytes, OutError, GeoScenario))
		{
			return false;
		}
	}
	return LoadStateInto(Bytes, OutError, GeoScenario);
}

bool FAnastasisSimulation::LoadStateInto(const TArray<uint8>& Bytes, FString& OutError, const AnastasisGeo::FScenario* GeoScenario)
{
	FSaveHeader Header;
	const int32 Body = ReadHeaderAt(Bytes, Header, OutError);
	if (Body == INDEX_NONE)
	{
		return false;
	}
	if (Header.bGeoLoaded && (!GeoScenario || GeoScenario->Id != Header.GeoScenarioId))
	{
		OutError = FString::Printf(TEXT("la sauvegarde a un monde exterieur (scenario « %s ») : %s"), *Header.GeoScenarioId,
			GeoScenario ? *FString::Printf(TEXT("scenario « %s » fourni"), *GeoScenario->Id) : TEXT("aucun scenario fourni"));
		return false;
	}

	// Reset regenere le monde a la meme taille et lie le village ; tout le reste est relu par-dessus.
	Reset(Header.Seed, Header.Width, Header.Height);
	AnastasisArchive::FStateArchive Ar = AnastasisArchive::FStateArchive::ForLoad(Bytes, Body);
	FString GeoScenarioId;
	FString GeoJson;
	ArchiveState(Ar, GeoScenarioId, GeoJson);
	if (Ar.Ok() && !Ar.AtEnd())
	{
		Ar.Fail(FString::Printf(TEXT("%d octets en trop apres l'etat"), Bytes.Num() - Ar.Tell()));
	}
	if (!Ar.Ok())
	{
		OutError = Ar.GetError();
		return false;
	}
	if (Seed != Header.Seed)
	{
		OutError = FString::Printf(TEXT("graine %u dans l'etat, %u dans l'en-tete"), Seed, Header.Seed);
		return false;
	}
	Village.AfterStateLoaded();

	Geo.Unload();
	if (!GeoScenarioId.IsEmpty())
	{
		TArray<FString> Errors;
		if (!Geo.LoadState(*GeoScenario, GeoJson, Errors))
		{
			OutError = FString::Printf(TEXT("monde exterieur : %s"), *FString::Join(Errors, TEXT(" ; ")));
			return false;
		}
	}
	return true;
}

void FAnastasisSimulation::OnNewDay(bool bDefer)
{
	++NewDayCount;
	// `decayPassageTrafficDaily(this)` : la reference l'appelle avant l'economie et les logements.
	Village.DecayTrafficDaily();
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
	else if (Job == DayJobCollective)
	{
		// `updateCollectivePrioritiesDaily` n'est pas porte (ecart n°27) ; a sa place, le village decide de ses
		// batiments communs, seulement s'il grandit (ecart n°52).
		Village.UpdateCommonBuildingsDaily(Day);
	}
	else if (Job == DayJobRoadEvolution)
	{
		LastRoadsBuilt = Village.UpdateRoadEvolutionDaily(Day);
	}
	else if (Job == DayJobLifeDaily)
	{
		// `updateLifeDaily` : ni `agePopulation` ni les tirages de mortalite ne sont portes (ecart n°28).
		Village.UpdateMortalityDaily();
		// `runLifeRelationsPhase` -> `applyEpisodeFeelings` : le souvenir a le dernier mot (ecart n°47 : ni
		// rencontres ni frictions avant lui).
		Village.ApplyEpisodeFeelingsDaily();
		// ecart n°53 (EXTENSION) : les arrivants de la veille passent au conseil des chefs de famille.
		Village.UpdateArrivalCouncilDaily(Geo.IsLoaded() ? Geo.GetVillageExposure().Pressure[static_cast<int32>(AnastasisGeo::EPressure::Insecurity)] : 0.0, Day);
		// ecart n°48 (EXTENSION) : les familles sans maison decident de batir, et vont demander de l'aide.
		Village.UpdateFamilyHousesDaily();
	}
	else if (Job == DayJobMemory)
	{
		// `memory` : forgetStale + forgetStalePeople + fadeEpisodes pour chacun (ecart n°47 pour le detail).
		Village.ForgetStaleDaily(Day);
		Village.FadeEpisodesDaily(Day);
	}

}

void FAnastasisSimulation::RunLandRegen()
{
	// `sim.regrowFieldsDaily(); sim.regrowForestDaily(); syncAllYardsFromStock(sim);`
	// La foret : `regrowWoodTile` rend toujours false dans la reference (fee66ae),
	// rien ne repousse. Les cours de stock sont une vue, pas de l'etat.
	LastRegrownFields = Village.RegrowFieldsDaily(Day);
}
