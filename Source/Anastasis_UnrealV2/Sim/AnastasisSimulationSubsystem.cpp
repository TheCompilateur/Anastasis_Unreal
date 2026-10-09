#include "Sim/AnastasisSimulationSubsystem.h"
#include "Sim/AnastasisArrivals.h"
#include "Sim/AnastasisBuildingCapacity.h"

#include "Anastasis_UnrealV2.h"
#include "Stats/Stats.h"
#include "Core/AnastasisJsNumeric.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "Core/AnastasisSimClock.h"
#include "Village/AnastasisVillage.h"
#include "Village/AnastasisArchitecture.h"
#include "Work/AnastasisBuild.h"
#include "Work/AnastasisGather.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Village/AnastasisVillageBuilding.h"
#include "Village/AnastasisVillageInteractionSubsystem.h"
#include "Village/AnastasisVillagerVisual.h"
#include "WorldView/AnastasisPresentationResolver.h"
#include "WorldView/AnastasisWorldView.h"
#include "WorldView/AnastasisWorldEmbodiment.h"
#include "WorldView/AnastasisAnthropicSubsystem.h"
#include "WorldView/AnastasisWorldAtmosphere.h"
#include "Village/AnastasisBuildingMetabolism.h"
#include "EngineUtils.h"
#include "WorldView/AnastasisSettlementSurvey.h"
#include "WorldView/AnastasisPresentationRegistry.h"
#include "Village/AnastasisVillagerLooks.h"
#include "Sim/AnastasisDialogueLines.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "WorldView/AnastasisCanonicalGeography.h"
#include "HAL/PlatformTime.h"

// Default 1 since SKY_TRANSITIONS_001 (2026-09-30), the JS reference's realtime: one day = 90 s.
// At the former default of 10, now that the sky follows the simulation, a day lasted ~12 real
// seconds and the sun swept the sky at ~30 deg/s -- Alexandre: "on dirait la magie". Proofs that
// need a fast night set the speed themselves (gather-deliver-pie.py does: 'anastasis.Sim.Speed 10');
// those that only wait for night still find it within their 240 s timeout (night falls ~41 s in).
static TAutoConsoleVariable<float> CVarSimSpeed(
	TEXT("anastasis.Sim.Speed"),
	1.0f,
	TEXT("Simulation speed scale (JS 1/2/5/10). Default 1 = JS realtime, a 90 s day. Set 10 in a proof that needs midnight quickly."),
	ECVF_Default);

static TAutoConsoleVariable<float> CVarSimTimeScale(
	TEXT("anastasis.Sim.TimeScale"),
	0.0375f,
	TEXT("Simulated seconds fed per real second, before anastasis.Sim.Speed (VILLAGER_PNG_001, point 4). 1 = JS realtime: a 90 s day, villagers at 4 tiles/s = 80 m/s on 20 m tiles. Default 0.0375: villagers ~3 m/s, a day ~40 min. 0 freezes the simulation (Sim.Speed 0 does not: PumpFrame reads any speed below 1 as 1). Proof scripts that wait on simulated time set 1."),
	ECVF_Default);

// TIME_WARP_001. Multiplie tout le reste ; 1 reprend exactement l'ancien chemin (PumpFrame).
static TAutoConsoleVariable<float> CVarSimWarp(
	TEXT("anastasis.Sim.Warp"),
	1.0f,
	TEXT("Time warp for the player and for agents (TIME_WARP_001): multiplies simulated time after anastasis.Sim.TimeScale and anastasis.Sim.Speed. 1 = unchanged (the JS PumpFrame path), 0 = pause, up to 1000. Steps never exceed the reference's 10 x FixedDt; more steps run per frame, under anastasis.Sim.WarpBudgetMs. PIE keys: 8 faster, 9 slower (digit row or numpad), numpad +/-, Pause. Batch editors: -dpcvars=anastasis.Sim.Warp=64."),
	ECVF_Default);

static TAutoConsoleVariable<float> CVarSimWarpBudgetMs(
	TEXT("anastasis.Sim.WarpBudgetMs"),
	8.0f,
	TEXT("Wall-clock milliseconds the time warp may spend simulating per frame (TIME_WARP_001). Past it, the backlog is dropped and the overlay shows the warp actually reached. 0 = unlimited (headless proofs: the frame rate drops, simulated time does not)."),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarVillageDebug(
	TEXT("anastasis.Village.Debug"),
	1,
	TEXT("1 = draw the simulated village (buildings, access points, inhabitants, targets) in PIE."),
	ECVF_Default);

// ICEBERG_001 : ce que la maison montre de ses habitants. 0 = ancien rendu, 1 = projection de la
// simulation (defaut), 2 = TEMOIN FAUX (toutes les maisons allumees), pour comparer, jamais un reglage.
static TAutoConsoleVariable<int32> CVarVillageMetabolism(
	TEXT("anastasis.Village.Metabolism"),
	1,
	TEXT("ICEBERG_001: 0 = buildings show nothing of their occupants, 1 = hearth light from the simulation's occupancy (default), 2 = WRONG WITNESS control: every house lit regardless."),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarVillagePortraits(
	TEXT("anastasis.Village.Portraits"),
	1,
	TEXT("1 = draw each simulated villager as an animated 3D body. 0 = hide villager visuals; debug spheres stay under anastasis.Village.Debug. Legacy variable name."),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarVillageStartVillagers(
	TEXT("anastasis.Village.StartVillagers"),
	12,
	TEXT("Inhabitants placed around the first well when play begins (VILLAGER_PNG_001), so the game does not open on an empty world. 0 = empty village. The first explicit scenario command (FirstWell, FirstHouse, FirstGranary, FirstFarmer, FoodSupply) replaces this village."),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarVillageOpeningConstruction(
	TEXT("anastasis.Village.OpeningConstruction"), 1,
	TEXT("1 = give the initial village one funded house site and two existing builders. 0 = keep the initial village without a site for A/B."),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarVillageSiteSelection(
    TEXT("anastasis.Village.SiteSelection"), 1,
    TEXT("1 selects the opening village from terrain and navigable resource access (source: anastasis.Village.SiteSource). 0 retains legacy centre placement for comparison. Explicit scenarios and saves are not relocated."), ECVF_Default);

// SITE_FROM_SIM_001 -- GEO_MEASURE_001 measured the rendered-mesh survey moving the opening village by
// 0.5 to 0.7 km when a RENDER CVar changed (Drainage 0, HumanGeography 0). The simulation now chooses from
// its own tiles; the rendered survey is kept as an observation (water concordance, rendered slope).
// WATER_NETWORK_001 -- decision d'Alexandre (2026-10-08) : « l'eau doit respecter son reseau ». Au reset,
// la simulation prend l'eau du reseau de drainage canonique (AnastasisCanonicalGeography : graine seule,
// recette fixe, aucune CVar de rendu). Ecart n°51 dans AnastasisSim.
static TAutoConsoleVariable<int32> CVarSimWaterNetwork(
    TEXT("anastasis.Sim.WaterNetwork"), 1,
    TEXT("1 = the simulation's water follows the canonical drainage network (default; what the player sees at default render settings). 0 = the JS hydrology water (previous behaviour, A/B). Applied on reset."), ECVF_Default);

static TAutoConsoleVariable<int32> CVarVillageSiteSource(
    TEXT("anastasis.Village.SiteSource"), 1,
    TEXT("Opening site read from: 1 = the simulation's tiles (default; render CVars cannot move the village, the rendered survey is only reported), 0 = the rendered relief (previous behaviour, for A/B)."), ECVF_Default);

static TAutoConsoleVariable<int32> CVarVillageGrowth(
	TEXT("anastasis.Village.Growth"),
	1,
	TEXT("valmire-grows-001 (ecart n°50) : 1 = le village grandit de lui-meme (les habitants ouvrent maison, grenier ou puits quand il en manque, des arrivants viennent quand il y a de la place et de quoi manger) ; 0 = seuls l'hote et les scenarios ouvrent des chantiers. Lu au demarrage de la simulation."),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarVillageRoadEvolution(
	TEXT("anastasis.Village.RoadEvolution"),
	1,
	TEXT("settlement-morphogenesis-001 (ecart n°42) : 1 = une case foulee assez longtemps devient un sentier (sim.traffic -> Road, cout 0,86) ; 0 = le passage est compte mais ne marque pas le sol. Lu au demarrage de la simulation."),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarVillageRouteCost(
	TEXT("anastasis.Village.RouteCost"), 1,
	TEXT("1 = PNJ travel time pays the existing nav-grid terrain cost (road faster, wet grass slower). 0 = JS-reference uniform travel time. Does not alter path selection or player input."), ECVF_Default);

static TAutoConsoleVariable<int32> CVarSoilWaterBudget(
	TEXT("anastasis.Village.SoilWaterBudget"), 0,
	TEXT("Experimental field reservoir: 1 couples previous-day rain index to field regrowth; 0 keeps JS-reference fertility. Reset the world between A/B runs."), ECVF_Default);

static TAutoConsoleVariable<int32> CVarSimOverlay(
	TEXT("anastasis.Sim.Overlay"),
	1,
	TEXT("1 = draw day/time overlay in PIE. 0 = log only."),
	ECVF_Default);

// CHRONIQUE_VILLAGE_001. Observation seule : 0 ne change rien a la simulation, seulement au recit.
static TAutoConsoleVariable<int32> CVarChronicleEnabled(
	TEXT("anastasis.Chronicle.Enabled"),
	1,
	TEXT("CHRONIQUE_VILLAGE_001: 1 = the village chronicle reads the simulation every frame and every chunk of Anastasis.Sim.Advance; 0 = it stops reading (the simulation is unchanged either way). Anastasis.Chronicle.Write / Print to read it."),
	ECVF_Default);

// familles-feu-001 (ecart n°44). Le village du lancement : les fondateurs de Valmire, ou des anonymes.
static TAutoConsoleVariable<int32> CVarVillageFounders(
	TEXT("anastasis.Village.Founders"),
	1,
	TEXT("familles-feu-001: 1 = the start village is the four founding families of Valmire and the monk Arsenios (Content/Anastasis/Scenario/valmire-fondateurs.json), seeded on tiles that reach the well, with the first evening at the fire told in the chronicle; 0 = anastasis.Village.StartVillagers anonymous villagers, as before."),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarVillageCanopyRain(
	TEXT("anastasis.Village.CanopyRain"), 1,
	TEXT("1 = embodied tree crowns partly intercept rain exposure for villagers; 0 = reference exposure at every outdoor position."),
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

    // Subsystem begin play precedes actor terrain construction. Survey on the first ready tick.
    bPendingStartVillage = CVarVillageStartVillagers.GetValueOnGameThread() > 0;
    StartVillageWait = 0.0;
    SettlementSiteReport = bPendingStartVillage ? TEXT("{\"status\":\"pending\"}") : TEXT("{\"status\":\"disabled\"}");
}

void UAnastasisSimulationSubsystem::TryStartVillage(float DeltaTime)
{
    if (!bPendingStartVillage) return;
    auto& Village = Simulation.GetVillage();
    // Explicit scenario/load takes ownership before automatic opening: never relocate existing people.
    if (!Village.GetBuildings().IsEmpty() || !Village.GetActors().IsEmpty())
    {
        bPendingStartVillage = false;
        SettlementSiteReport = TEXT("{\"status\":\"existing_village_preserved\"}");
        return;
    }
    int32 X = FMath::FloorToInt32(Village.GetSettlement().X), Y = FMath::FloorToInt32(Village.GetSettlement().Y);
    if (CVarVillageSiteSelection.GetValueOnGameThread() != 0)
    {
        AnastasisSettlementSite::FInputs Rendered;
        FString Error;
        const double Began = FPlatformTime::Seconds();
        const bool bFromSimulation = AnastasisSettlementSurvey::SiteFromSimulation();
        const bool bRendered = AnastasisSettlementSurvey::Read(GetWorld(), Simulation.GetSeed(), Simulation.GetWorld(), Village, Rendered, Error);
        if (!bRendered)
        {
            // The rendered survey is awaited for 10 s either way (its observation feeds the water concordance).
            StartVillageWait += DeltaTime;
            if (StartVillageWait < 10.0 && Error == TEXT("terrain_not_ready")) return;
            if (!bFromSimulation)
            {
                bPendingStartVillage = false;
                SettlementSiteReport = FString::Printf(TEXT("{\"status\":\"unavailable\",\"error\":\"%s\"}"), *Error);
                UE_LOG(LogAnastasis_UnrealV2, Warning, TEXT("SETTLEMENT_SITE %s"), *SettlementSiteReport);
                return;
            }
            // From the simulation, a late or missing terrain no longer leaves the world without a village.
            UE_LOG(LogAnastasis_UnrealV2, Warning, TEXT("SETTLEMENT_SITE rendered survey unavailable (%s): choosing from the simulation, without observation"), *Error);
        }
        const AnastasisSettlementSite::FInputs In = AnastasisSettlementSurvey::SiteInputs(
            Rendered, bRendered, Simulation.GetSeed(), Simulation.GetWorld(), Village);
        const auto Report = AnastasisSettlementSite::Choose(In);
        SettlementSiteReport = AnastasisSettlementSite::ToJson(Report, In);
        UE_LOG(LogAnastasis_UnrealV2, Display, TEXT("SETTLEMENT_SITE survey_ms=%.3f %s"),
            (FPlatformTime::Seconds()-Began)*1000.0, *SettlementSiteReport);
        bPendingStartVillage = false;
        if (!Report.Best.bEligible) return; // No disguised fallback to the arbitrary centre.
        X = Report.Best.Index % In.W; Y = Report.Best.Index / In.W;
        Village.SetSettlement(X + 0.5, Y + 0.5);
    }
    else
    {
        bPendingStartVillage = false;
        SettlementSiteReport = TEXT("{\"status\":\"legacy\"}");
    }
    const int32 Count = CVarVillageStartVillagers.GetValueOnGameThread();
    if (Count <= 0) return;
    const FString WellId = SeedStartVillage(Count, X, Y);
    const auto* Well = Village.FindBuilding(WellId);
    if (!Well || (CVarVillageSiteSelection.GetValueOnGameThread()!=0 && (Well->X!=X || Well->Y!=Y)))
    {
        SettlementSiteReport = TEXT("{\"status\":\"spawn_mismatch\"}");
        UE_LOG(LogAnastasis_UnrealV2, Error, TEXT("SETTLEMENT_SITE spawn_mismatch"));
    }
    UE_LOG(LogAnastasis_UnrealV2, Display, TEXT("ANASTASIS_VILLAGE start village: %s + %d inhabitants site=(%d,%d)"),
        *WellId, Village.GetActors().Num(), X, Y);
}

FString UAnastasisSimulationSubsystem::SeedStartVillage(int32 NpcCount, int32 TileX, int32 TileY)
{
	// familles-feu-001 : le puits seul, puis les fondateurs a leur place ; sans scenario lisible, les anonymes.
	const AnastasisFounders::FScenario* Scenario = CVarVillageFounders.GetValueOnGameThread() != 0 ? AnastasisFounders::FScenario::Get() : nullptr;
	const FString WellId = SeedFirstWell(Scenario ? 0 : NpcCount, TileX, TileY);
	bStartVillage = !WellId.IsEmpty();
	if (bStartVillage)
	{
		if (Scenario) Founders = AnastasisFounders::Seed(Simulation, WellId, *Scenario);
		// opening-in-sim-001 : foyer, poste, chantier, batisseurs, porteur et embauches sont decides par la
		// simulation (FVillage::SeedOpeningVillage, ecart n°40) ; l'hote ne fait que le dire.
		LogOpeningReport(Simulation.GetVillage().SeedOpeningVillage(Simulation.GetDay(), CVarVillageOpeningConstruction.GetValueOnGameThread() != 0));
		if (Scenario) TellFounding(*Scenario);
		// valmire-grows-001 (ecart n°50) : le village du jeu grandit de lui-meme.
		Simulation.GetVillage().SetGrowthEnabled(CVarVillageGrowth.GetValueOnGameThread() != 0);
		if (Scenario) OpenValmireToTheWorld();
	}
	return WellId;
}

void UAnastasisSimulationSubsystem::TellFounding(const AnastasisFounders::FScenario& Scenario)
{
	if (Founders.IsEmpty()) return;
	// memoire-decisions-001 : ce qui se dira au feu est deja memoire quand la chronique s'ouvre (elle ne
	// raconte donc pas ces cent cinquante histoires entendues comme des rumeurs du premier jour).
	const int32 Memories = AnastasisFounders::RecordFireMemories(Simulation, Scenario, Founders, Simulation.GetSeed());
	// La chronique s'ouvre maintenant, sur le village tel qu'il est pose (metiers d'ouverture compris).
	Chronicle.Observe(Simulation);
	const int32 Day = Simulation.GetDay();
	const int32 Hour = FMath::Clamp(FMath::FloorToInt32(Simulation.DayFrac() * 24.0), 0, 23);
	for (int32 F = 0; F < Scenario.Families.Num(); ++F)
	{
		TArray<FString> Members;
		for (const AnastasisFounders::FFounder& Founder : Founders)
		{
			if (Founder.Family == F) Members.Add(Founder.NpcId);
		}
		Chronicle.AddNarration(Day, Hour, AnastasisChronicle::EKind::Founding, Members, AnastasisFounders::FamilyIntro(Scenario.Families[F]));
	}
	// Le premier soir, au feu (Bible §10.6) : le moine interroge, chaque famille repond.
	constexpr int32 EveningHour = 20;
	const TArray<AnastasisFounders::FLine> Scene = AnastasisFounders::FireScene(Scenario, Founders, Simulation.GetSeed(), AnastasisDialogue::FLibrary::Get());
	for (const AnastasisFounders::FLine& Line : Scene)
	{
		Chronicle.AddNarration(Day, EveningHour, AnastasisChronicle::EKind::Scene, { Line.SpeakerId },
			FString::Printf(TEXT("%s : « %s »"), *Chronicle.NameOf(Line.SpeakerId), *Line.Text));
	}
	UE_LOG(LogAnastasis_UnrealV2, Display, TEXT("ANASTASIS_FOUNDERS founding told families=%d founders=%d fire_lines=%d fire_memories=%d"),
		Scenario.Families.Num(), Founders.Num(), Scene.Num(), Memories);
}

void UAnastasisSimulationSubsystem::LogOpeningReport(const AnastasisVillage::FOpeningReport& Opening)
{
	// Memes lignes, dans le meme ordre, que lorsque l'hote decidait lui-meme (preuves PIE et A/B).
	if (Opening.bHousehold)
	{
		UE_LOG(LogAnastasis_UnrealV2, Display,
			TEXT("ANASTASIS_VILLAGE opening household npc=%s home=%s work=%s"),
			*Opening.ResidentId, Opening.HomeId.IsEmpty() ? TEXT("none") : *Opening.HomeId, Opening.WorkId.IsEmpty() ? TEXT("none") : *Opening.WorkId);
	}
	if (Opening.bConstructionTried)
	{
		if (!Opening.SiteId.IsEmpty())
		{
			FirstSiteId = Opening.SiteId;
			UE_LOG(LogAnastasis_UnrealV2, Display,
				TEXT("ANASTASIS_VILLAGE opening construction site=%s tile=(%d,%d) builders=%s courier=%s stock=%d wood %d stone"),
				*Opening.SiteId, Opening.SiteX, Opening.SiteY, *FString::Join(Opening.Builders, TEXT(",")), *Opening.CourierId, Opening.StockWood, Opening.StockStone);
		}
		else
		{
			UE_LOG(LogAnastasis_UnrealV2, Warning, TEXT("ANASTASIS_VILLAGE opening construction unavailable: no reachable site for any settler"));
		}
	}
	if (Opening.bWorkforce)
	{
		UE_LOG(LogAnastasis_UnrealV2, Display, TEXT("ANASTASIS_VILLAGE opening workforce granary=%s farmers=%s"),
			*Opening.WorkId, Opening.Farmers.IsEmpty() ? TEXT("none") : *FString::Join(Opening.Farmers, TEXT(",")));
	}
}

void UAnastasisSimulationSubsystem::LogOpeningHome()
{
	// La simulation attribue la maison d'ouverture (FVillage::AssignCompletedOpeningHome, ecart n°40) ;
	// l'hote ne fait que dire l'issue, une fois.
	const AnastasisVillage::FOpeningHomeOutcome& Outcome = Simulation.GetVillage().GetOpeningHome();
	using AnastasisVillage::EOpeningHomeStatus;
	if (Outcome.Status == EOpeningHomeStatus::None || Outcome.Status == EOpeningHomeStatus::Pending)
	{
		bOpeningHomeLogged = false;
		return;
	}
	if (bOpeningHomeLogged) return;
	bOpeningHomeLogged = true;
	if (Outcome.Status == EOpeningHomeStatus::Assigned)
	{
		UE_LOG(LogAnastasis_UnrealV2, Display, TEXT("ANASTASIS_VILLAGE opening home assigned npc=%s home=%s"), *Outcome.NpcId, *Outcome.SiteId);
	}
	else if (Outcome.Status == EOpeningHomeStatus::Unassigned)
	{
		UE_LOG(LogAnastasis_UnrealV2, Warning, TEXT("ANASTASIS_VILLAGE opening home unassigned site=%s: no reachable homeless builder"), *Outcome.SiteId);
	}
}

void UAnastasisSimulationSubsystem::ReplaceStartVillage()
{
	bPendingStartVillage = false;
	if (!bStartVillage)
	{
		return;
	}
	bStartVillage = false;
	UWorld* World = GetWorld();
	VillagePresentation.Clear(World ? World->GetSubsystem<UAnastasisVillageInteractionSubsystem>() : nullptr);
	ResetCanonical(Simulation.GetSeed());
	UE_LOG(LogAnastasis_UnrealV2, Display, TEXT("ANASTASIS_VILLAGE start village replaced by an explicit scenario"));
}

void UAnastasisSimulationSubsystem::Deinitialize()
{
	if (HelpPanel.IsValid())
	{
		if (UWorld* World = GetWorld())
		{
			if (UGameViewportClient* Viewport = World->GetGameViewport()) Viewport->RemoveViewportWidgetContent(HelpPanel.ToSharedRef());
		}
		HelpPanel.Reset();
	}
	// Le monde se defait : ses acteurs partent avec lui. On oublie seulement les liens.
	VillagePresentation = FAnastasisVillagePresentation();
	Super::Deinitialize();
}

void UAnastasisSimulationSubsystem::ResetCanonical(uint32 Seed)
{
	HelpFeedback.Reset();
	bHelpSceneActive = false;
	bHelpNotebookOpen = false;
	RainCanopyActor.Reset();
	if (auto* Anthropic = GetWorld()->GetSubsystem<UAnastasisAnthropicSubsystem>()) Anthropic->ResetPresentation();
	bPendingStartVillage = false;
	StartVillageWait = 0.0;
	SettlementSiteReport = TEXT("{\"status\":\"reset\"}");
	FirstSiteId.Reset();
	GeoScenarioPath.Reset();
	// Le grenier et le champ du dernier FirstFarmer : sans cette remise, get_gather_status lisait un
	// identifiant d'un village precedent (les ids `building-N` repartent de zero).
	FarmerGranaryId.Reset();
	FarmerField = FIntPoint(-1, -1);
	// Le verrou de la maison d'ouverture vit dans la simulation (ecart n°40) : Simulation.Reset le vide.
	bOpeningHomeLogged = false;
	Simulation.Reset(Seed, AnastasisWorldView::ReferenceWidth, AnastasisWorldView::ReferenceHeight);
	if (CVarSimWaterNetwork.GetValueOnGameThread() != 0)
	{
		const AnastasisCanonicalGeography::FGeography& Mask = AnastasisCanonicalGeography::Get(Seed);
		const int32 Changed = Mask.bValid ? Simulation.ApplyWaterMask(Mask.Water) : -1;
		UE_LOG(LogAnastasis_UnrealV2, Display, TEXT("ANASTASIS_WATER_NETWORK seed=%u valid=%d error=%s water_tiles=%d rivers=%d lakes=%d changed_tiles=%d compute_s=%.2f"),
			Seed, Mask.bValid ? 1 : 0, Mask.Error.IsEmpty() ? TEXT("-") : *Mask.Error, Mask.WaterTiles, Mask.Rivers, Mask.Lakes, Changed, Mask.Seconds);
	}
	// CHRONIQUE_VILLAGE_001 : une simulation neuve, une chronique neuve. Le nom provisoire d'un habitant
	// s'accorde au portrait que la presentation lui donnera (meme tirage que SyncVillagers).
	Chronicle.Reset(Seed);
	Notebook.Reset();
	Founders.Reset();
	Chronicle.SetDialogue(&AnastasisDialogue::FLibrary::Get());
	Chronicle.SetLookResolver([this](const AnastasisVillage::FNpc& Npc)
	{
		AnastasisChronicle::FPersonLook Look;
		// familles-feu-001 : le surnom d'un fondateur (« le Scribe ») vient du scenario.
		if (const AnastasisFounders::FScenario* Scenario = AnastasisFounders::FScenario::Get())
		{
			if (const AnastasisFounders::FFounder* Founder = Founders.FindByPredicate([&Npc](const AnastasisFounders::FFounder& F) { return F.NpcId == Npc.Id; }))
			{
				int32 Family = INDEX_NONE;
				if (const AnastasisFounders::FMember* Member = Scenario->FindMember(Founder->Key, Family)) Look.Byname = Member->Byname;
			}
		}
		const UAnastasisPresentationRegistry& Registry = AnastasisPresentation::GetRegistry();
		const TArray<int32> Pool = AnastasisVillagerLooks::VillagePool(Registry.Villagers, FName(*Npc.JobId));
		const int32 Index = AnastasisVillagerLooks::PickLook(Pool, Npc.Id);
		if (Registry.Villagers.IsValidIndex(Index))
		{
			const EAnastasisVillagerCategory Category = Registry.Villagers[Index].Category;
			Look.bKnown = true;
			Look.bFemale = Category == EAnastasisVillagerCategory::AdultFemale
				|| Category == EAnastasisVillagerCategory::ElderFemale
				|| Category == EAnastasisVillagerCategory::ChildFemale;
			Look.bElder = Category == EAnastasisVillagerCategory::ElderMale
				|| Category == EAnastasisVillagerCategory::ElderFemale;
		}
		return Look;
	});
	Simulation.GetVillage().SetTerrainTravelCostEnabled(CVarVillageRouteCost.GetValueOnGameThread() != 0);
	Simulation.GetVillage().SetSoilWaterEnabled(CVarSoilWaterBudget.GetValueOnGameThread() != 0);
	Simulation.GetVillage().SetRoadEvolutionEnabled(CVarVillageRoadEvolution.GetValueOnGameThread() != 0);
	// ecart n°46 : la biographie des batiments est observee par la simulation (save-history-001).
	Simulation.GetVillage().SetBiographyEnabled(true);
	// ecart n°50 : la croissance s'allume pour le village du lancement seulement (SeedStartVillage) ; les
	// scenarios de preuve (First*, FoodSupply, Hamlet) restent ceux qu'ils etaient.
	Simulation.GetVillage().SetGrowthEnabled(false);
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
	TryStartVillage(DeltaTime);
	EnsureHelpPanel();
	BindRainCanopy();
	if (!Simulation.IsRunning())
	{
		return;
	}

	const double Speed = static_cast<double>(CVarSimSpeed.GetValueOnGameThread());
	const double Warp = FMath::Clamp(static_cast<double>(CVarSimWarp.GetValueOnGameThread()), 0.0, AnastasisTimeWarp::MaxWarp);
	// Le temps simule avance plus lentement que le temps reel : sans cela un habitant (4 tuiles par
	// seconde simulee, une tuile = 20 m a l'ecran) traverse le pre a 80 m/s. Tout ralentit ensemble --
	// marche, besoins, jour, ciel -- la simulation reste fidele a elle-meme, seul le rythme change.
	const double SimWall = static_cast<double>(DeltaTime)
		* FMath::Clamp(static_cast<double>(CVarSimTimeScale.GetValueOnGameThread()), 0.0, 1.0);
	const double TimeBefore = Simulation.GetTime();
	Simulation.GetVillage().SetTerrainTravelCostEnabled(CVarVillageRouteCost.GetValueOnGameThread() != 0);
	Simulation.GetVillage().SetSoilWaterEnabled(CVarSoilWaterBudget.GetValueOnGameThread() != 0);
	Simulation.GetVillage().SetRoadEvolutionEnabled(CVarVillageRoadEvolution.GetValueOnGameThread() != 0);
	const double Multiplier = FMath::Max(1.0, AnastasisJs::NumberOr(Speed, 1.0)) * Warp;
	// player-minimal-001 : la direction du pawn conduit le corps incarne pendant les pas de cette frame.
	ApplyPlayerInput();

	int32 Steps = 0;
	double StepAlpha = 1.0;
	if (Warp == 1.0)
	{
		WarpPump.Reset();
		bWarpBudgetCut = false;
		// Miroir de l'accumulateur de PumpFrame (memes regles publiques d'AnastasisSimClock) : la fraction
		// du pas en cours sert a interpoler les cartes. Un pas fixe vaut 1/60 s simulee ; ralenti, il ne
		// tombe qu'une frame sur ~27 et les habitants sauteraient de 1,3 m a chaque pas.
		const AnastasisSimClock::FStepPlan Plan = AnastasisSimClock::StepPlan(Speed);
		PresentationAccumulator += FMath::Max(0.0, AnastasisSimClock::FrameDelta(SimWall * 1000.0).Dt)
			* FMath::Max(1.0, AnastasisJs::NumberOr(Speed, 1.0));
		Steps = Simulation.PumpFrame(SimWall, Speed);
		PresentationAccumulator = FMath::Clamp(PresentationAccumulator - Steps * Plan.StepDt, 0.0,
			Plan.StepDt * static_cast<double>(FMath::Max(1, Plan.TargetSteps)));
		StepAlpha = Plan.StepDt > 0.0 ? FMath::Clamp(PresentationAccumulator / Plan.StepDt, 0.0, 1.0) : 1.0;
	}
	else
	{
		// Voie acceleree (TIME_WARP_001) : memes pas que la reference, plus de pas par frame. Un accroc
		// de chargement est borne comme dans FrameDelta (0,1 s), sinon la premiere frame apres un hitch
		// a x64 teleporterait le village.
		PresentationAccumulator = 0.0;
		const AnastasisTimeWarp::FPumpResult Result = WarpPump.Pump(Simulation, FMath::Min(SimWall, 0.1), Multiplier,
			static_cast<double>(CVarSimWarpBudgetMs.GetValueOnGameThread()));
		Steps = Result.Steps;
		StepAlpha = Result.StepAlpha;
		bWarpBudgetCut = Result.bBudgetCut;
	}

	const double Advanced = Simulation.GetTime() - TimeBefore;
	ObservePlayerTime(Advanced, Multiplier);
	if (DeltaTime > 0.0f)
	{
		// Lisse sur ~1 s : a vitesse lente un pas ne tombe qu'une frame sur plusieurs, la valeur brute clignoterait.
		const double Alpha = FMath::Clamp(static_cast<double>(DeltaTime), 0.0, 1.0);
		EffectiveRate += (Advanced / static_cast<double>(DeltaTime) - EffectiveRate) * Alpha;
	}
	LogDayIfChanged();
	LogOpeningHome();
	ObserveChronicle();
	SyncVillagePresentation();
	VillagePresentation.SyncVillagers(
		Simulation.GetVillage(), Simulation.GetWorld(), GetWorld(),
		AnastasisPresentation::GetRegistry(), CVarVillagePortraits.GetValueOnGameThread() != 0,
		StepAlpha, Steps > 0);
	PlacePlayerPawn();
	if (CVarVillageDebug.GetValueOnGameThread() != 0)
	{
		FAnastasisVillagePresentation::DrawDebug(GetWorld(), Simulation.GetVillage(), Simulation.GetWorld());
		VillagePresentation.DrawSettlementDebug(GetWorld(), Simulation.GetVillage(), Simulation.GetWorld(), Simulation.GetDay());
	}
	DrawOverlay();
}

void UAnastasisSimulationSubsystem::BindRainCanopy()
{
	if (RainCanopyActor.IsValid()) return;
	for (TActorIterator<AAnastasisWorldEmbodiment> It(GetWorld()); It; ++It)
	{
		AAnastasisWorldEmbodiment* Actor = *It;
		const auto& Source = Actor->GetSnapshot();
		if (Source.Seed != Simulation.GetSeed() || Source.SourceW != Simulation.GetWorld().W
			|| Source.SourceH != Simulation.GetWorld().H || Source.SpatialScale <= 0.0) continue;
		RainCanopyActor = Actor;
		const TWeakObjectPtr<AAnastasisWorldEmbodiment> WeakActor(Actor);
		const uint32 Seed = Source.Seed;
		const int32 Width = Source.SourceW, Height = Source.SourceH;
		Simulation.GetVillage().SetRainCanopyCover([WeakActor, Seed, Width, Height](double X, double Y)
		{
			if (CVarVillageCanopyRain.GetValueOnGameThread() == 0 || !WeakActor.IsValid()) return 0.0;
			const auto& Current = WeakActor->GetSnapshot();
			if (Current.Seed != Seed || Current.SourceW != Width || Current.SourceH != Height
				|| Current.SpatialScale <= 0.0) return 0.0;
			const double Cell = AnastasisWorldView::TileWorldSize * Current.SpatialScale;
			return WeakActor->RainCanopyCoverAt(X * Cell, Y * Cell);
		});
		return;
	}
}

int32 UAnastasisSimulationSubsystem::SyncVillagePresentation()
{
	UWorld* World = GetWorld();
	UAnastasisVillageInteractionSubsystem* Rooms = World ? World->GetSubsystem<UAnastasisVillageInteractionSubsystem>() : nullptr;
	if (!Rooms || !Simulation.IsRunning())
	{
		return 0;
	}
	// La lumiere du jour que le joueur voit : celle du ciel, pas une seconde horloge. Sans ciel pilote
	// par l'horloge de simulation (editeur, test) : plein jour, aucun foyer allume.
	static TWeakObjectPtr<AAnastasisWorldAtmosphere> Atmosphere;
	if (!Atmosphere.IsValid() || Atmosphere->GetWorld() != World)
	{
		Atmosphere.Reset();
		for (TActorIterator<AAnastasisWorldAtmosphere> It(World); It; ++It)
		{
			Atmosphere = *It;
			break;
		}
	}
	const double Daylight = (Atmosphere.IsValid() && Atmosphere->IsSkyClockActive())
		? Atmosphere->GetLastSkyState().Daylight
		: 1.0;
	const int32 Changes = VillagePresentation.Sync(
		Simulation.GetVillage(), Simulation.GetWorld(), *Rooms, Daylight,
		AnastasisMetabolism::ModeFromInt(CVarVillageMetabolism.GetValueOnGameThread()),
		1 + static_cast<int32>(FMath::FloorToDouble(Simulation.GetTime() / AnastasisSkyClock::DayLengthSeconds)));
	VillagePresentation.SyncInteractions(Simulation.GetVillage(), *Rooms);
	return Changes;
}

FString UAnastasisSimulationSubsystem::SeedFirstWell(int32 NpcCount, int32 TileX, int32 TileY)
{
	ReplaceStartVillage();
	if (!Simulation.IsRunning())
	{
		return FString();
	}
	AnastasisVillage::FVillage& Village = Simulation.GetVillage();
	const AnastasisNav::FNavGrid& Nav = Village.GetNavGrid();

	auto FreeNeighbours = [&](int32 X, int32 Y)
	{
		int32 Count = 0;
		for (int32 DY = -1; DY <= 1; ++DY)
		{
			for (int32 DX = -1; DX <= 1; ++DX)
			{
				if ((DX || DY) && !Village.IsFootBlocked(X + DX + 0.5, Y + DY + 0.5)) ++Count;
			}
		}
		return Count;
	};

	// Anneaux croissants autour de la case demandee : le premier sol libre dont
	// au moins trois voisins le sont aussi (un puits sans seuil ne sert a rien).
	FString WellId;
	int32 WellX = 0;
	int32 WellY = 0;
	for (int32 Radius = 0; Radius <= 24 && WellId.IsEmpty(); ++Radius)
	{
		for (int32 DY = -Radius; DY <= Radius && WellId.IsEmpty(); ++DY)
		{
			for (int32 DX = -Radius; DX <= Radius && WellId.IsEmpty(); ++DX)
			{
				if (FMath::Max(FMath::Abs(DX), FMath::Abs(DY)) != Radius) continue;
				const int32 X = TileX + DX;
				const int32 Y = TileY + DY;
				if (X < 2 || Y < 2 || X > Nav.W - 3 || Y > Nav.H - 3) continue;
				if (Village.IsFootBlocked(X + 0.5, Y + 0.5) || FreeNeighbours(X, Y) < 3) continue;
				WellId = Village.AddBuilding(AnastasisVillage::WellType, X, Y, 1.0, Simulation.GetDay());
				WellX = X;
				WellY = Y;
			}
		}
	}
	if (WellId.IsEmpty())
	{
		UE_LOG(LogAnastasis_UnrealV2, Warning, TEXT("ANASTASIS_VILLAGE first well: no free tile near (%d,%d)"), TileX, TileY);
		return WellId;
	}

	// Habitants en couronne a ~7 cases, soifs echelonnees : le plus assoiffe part
	// tout de suite, les autres quand leur soif franchit le seuil.
	for (int32 K = 0; K < NpcCount; ++K)
	{
		const double Angle = 2.0 * UE_DOUBLE_PI * K / FMath::Max(1, NpcCount);
		const int32 CX = WellX + FMath::RoundToInt32(7.0 * FMath::Cos(Angle));
		const int32 CY = WellY + FMath::RoundToInt32(7.0 * FMath::Sin(Angle));
		bool bPlaced = false;
		for (int32 R = 0; R <= 6 && !bPlaced; ++R)
		{
			for (int32 DY = -R; DY <= R && !bPlaced; ++DY)
			{
				for (int32 DX = -R; DX <= R && !bPlaced; ++DX)
				{
					const int32 X = CX + DX;
					const int32 Y = CY + DY;
					if (FMath::Max(FMath::Abs(DX), FMath::Abs(DY)) != R || !Nav.IsInBounds(X, Y)) continue;
					if (Village.IsFootBlocked(X + 0.5, Y + 0.5)) continue;
					AnastasisNeeds::FNeeds Needs;
					Needs.Hunger = 10.0;
					Needs.Energy = 80.0;
					Needs.Social = 70.0;
					Needs.Leisure = 70.0;
					Needs.Hygiene = 60.0;
					// Le seuil de soif suit la phase (~40 la nuit et a midi, ~64 le matin) :
					// le plus assoiffe part a toute heure, les autres a leur tour.
					Needs.Thirst = FMath::Max(5.0, 80.0 - 12.0 * K);
					Needs.Health = 90.0;
					Needs.Morale = 55.0;
					Village.SpawnNpc(X + 0.5, Y + 0.5, Needs, 4.0);
					bPlaced = true;
				}
			}
		}
	}

	SyncVillagePresentation();
	UE_LOG(
		LogAnastasis_UnrealV2,
		Display,
		TEXT("ANASTASIS_VILLAGE first well %s at tile (%d,%d), %d inhabitants"),
		*WellId,
		WellX,
		WellY,
		Village.GetActors().Num());
	FAnastasisVillagePresentation::LogStatus(Village, Simulation.GetTime());
	return WellId;
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

void UAnastasisSimulationSubsystem::ObserveChronicle()
{
	if (CVarChronicleEnabled.GetValueOnGameThread() != 0)
	{
		Chronicle.Observe(Simulation);
		// memoire-decisions-001 : le carnet du joueur, aux memes passages ; les noms sont ceux de la chronique.
		Notebook.Observe(Simulation, AnastasisDialogue::FLibrary::Get(), [this](const FString& Id) { return Chronicle.NameOf(Id); });
	}
}

FString UAnastasisSimulationSubsystem::WriteNotebook(const FString& FileName) const
{
	const FString Name = FileName.IsEmpty()
		? FString::Printf(TEXT("carnet-%u-jour-%d.txt"), Simulation.GetSeed(), Simulation.GetDay())
		: FileName;
	const FString Path = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Chronicle"), Name));
	if (!FFileHelper::SaveStringToFile(Notebook.Render(), *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		UE_LOG(LogAnastasis_UnrealV2, Warning, TEXT("ANASTASIS_NOTEBOOK write failed path=%s"), *Path);
		return FString();
	}
	UE_LOG(LogAnastasis_UnrealV2, Display, TEXT("ANASTASIS_NOTEBOOK written path=%s %s"), *Path, *Notebook.StatusJson());
	return Path;
}

FString UAnastasisSimulationSubsystem::WriteChronicle(const FString& FileName) const
{
	const FString Name = FileName.IsEmpty()
		? FString::Printf(TEXT("chronique-%u-jour-%d.txt"), Simulation.GetSeed(), Simulation.GetDay())
		: FileName;
	const FString Path = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Chronicle"), Name));
	if (!FFileHelper::SaveStringToFile(Chronicle.Render(), *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		UE_LOG(LogAnastasis_UnrealV2, Warning, TEXT("ANASTASIS_CHRONICLE write failed path=%s"), *Path);
		return FString();
	}
	UE_LOG(LogAnastasis_UnrealV2, Display, TEXT("ANASTASIS_CHRONICLE written path=%s %s"), *Path, *Chronicle.StatusJson());
	return Path;
}

void UAnastasisSimulationSubsystem::LogDayIfChanged()
{
	// geopolitical-world-001 : les evenements du monde exterieur, meme sans changement de jour.
	LogGeoEvents();
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

	// TIME_WARP_001 : la vitesse demandee, celle obtenue (une machine chargee coupe), la duree d'un jour
	// a ce rythme. Ce que le village voit du joueur : DrawPlayerOverlay.
	const double Warp = static_cast<double>(CVarSimWarp.GetValueOnGameThread());
	if (Warp != 1.0)
	{
		const double DaySeconds = EffectiveRate > 1e-6 ? FAnastasisSimulation::DayLength / EffectiveRate : 0.0;
		const FString DayText = Warp <= 0.0 ? FString(TEXT("PAUSE"))
			: DaySeconds <= 0.0 ? FString(TEXT("-"))
			: DaySeconds >= 120.0 ? FString::Printf(TEXT("1 jour = %.0f min"), DaySeconds / 60.0)
			: FString::Printf(TEXT("1 jour = %.0f s"), DaySeconds);
		GEngine->AddOnScreenDebugMessage(
			0xA51A52,
			0.0f,
			FColor::Cyan,
			FString::Printf(
				TEXT("TEMPS  x%g%s  %s"),
				Warp,
				bWarpBudgetCut ? TEXT(" (machine saturee)") : TEXT(""),
				*DayText));
	}
	DrawPlayerOverlay();
}

int32 UAnastasisSimulationSubsystem::AdvanceBy(double Seconds)
{
	if (!Simulation.IsRunning() || !(Seconds > 0.0))
	{
		return 0;
	}
	const double From = Simulation.GetTime();
	Simulation.GetVillage().SetTerrainTravelCostEnabled(CVarVillageRouteCost.GetValueOnGameThread() != 0);
	Simulation.GetVillage().SetSoilWaterEnabled(CVarSoilWaterBudget.GetValueOnGameThread() != 0);
	Simulation.GetVillage().SetRoadEvolutionEnabled(CVarVillageRoadEvolution.GetValueOnGameThread() != 0);
	const int32 FromDay = Simulation.GetDay();
	const double Start = FPlatformTime::Seconds();
	// Par tranches de 15 s simulees (90 pas) : le temoin informe le village AU FIL du saut, et chaque
	// minuit franchi juge un joueur dont l'oisivete est a jour (player-minimal-001). Le joueur n'a rien
	// fait de tout ce temps : multiplicateur infini.
	int32 Steps = 0;
	double Left = Seconds;
	while (Left > 1e-9)
	{
		const double Chunk = FMath::Min(Left, FAnastasisSimulation::DayLength / 6.0);
		const double ChunkFrom = Simulation.GetTime();
		Steps += AnastasisTimeWarp::Advance(Simulation, Chunk);
		ObservePlayerTime(Simulation.GetTime() - ChunkFrom, TNumericLimits<double>::Max());
		// CHRONIQUE_VILLAGE_001 : une lecture toutes les quatre heures simulees, le saut garde son recit.
		ObserveChronicle();
		Left -= Chunk;
	}
	const double WallMs = (FPlatformTime::Seconds() - Start) * 1000.0;
	WarpPump.Reset();
	PresentationAccumulator = 0.0;
	LogDayIfChanged();
	SyncVillagePresentation();
	UWorld* World = GetWorld();
	VillagePresentation.SyncVillagers(
		Simulation.GetVillage(), Simulation.GetWorld(), World,
		AnastasisPresentation::GetRegistry(), CVarVillagePortraits.GetValueOnGameThread() != 0,
		1.0, true);
	PlacePlayerPawn();
	UE_LOG(LogAnastasis_UnrealV2, Display,
		TEXT("ANASTASIS_SIM advance from=%.4f to=%.4f day=%d->%d ticks=%d wallMs=%.1f presence=%.4f idleDays=%.3f"),
		From, Simulation.GetTime(), FromDay, Simulation.GetDay(), Steps, WallMs,
		Witness.Presence, Witness.IdleDays(FAnastasisSimulation::DayLength));
	return Steps;
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

// --- TIME_WARP_001 -------------------------------------------------------------------------------

namespace
{
	void SetWarp(double Warp, const TCHAR* Why)
	{
		if (IConsoleVariable* Var = IConsoleManager::Get().FindConsoleVariable(TEXT("anastasis.Sim.Warp")))
		{
			Var->Set(static_cast<float>(FMath::Clamp(Warp, 0.0, AnastasisTimeWarp::MaxWarp)), ECVF_SetByConsole);
		}
		UE_LOG(LogAnastasis_UnrealV2, Display, TEXT("ANASTASIS_SIM warp=%g (%s)"), Warp, Why);
	}

	/** La vitesse que Pause rend. Etat de la session, pas une CVar : il n'a de sens qu'en jeu. */
	double GWarpBeforePause = 1.0;
}

static FAutoConsoleCommand CmdAnastasisSimFaster(
	TEXT("Anastasis.Sim.Faster"),
	TEXT("Next time warp preset (x0.25 .. x128). From pause, resumes at x1. PIE: 8 (or numpad +)."),
	FConsoleCommandDelegate::CreateLambda([]()
	{
		SetWarp(AnastasisTimeWarp::StepPreset(CVarSimWarp.GetValueOnGameThread(), +1), TEXT("Faster"));
	}));

static FAutoConsoleCommand CmdAnastasisSimSlower(
	TEXT("Anastasis.Sim.Slower"),
	TEXT("Previous time warp preset (x128 .. x0.25). PIE: 9 (or numpad -)."),
	FConsoleCommandDelegate::CreateLambda([]()
	{
		SetWarp(AnastasisTimeWarp::StepPreset(CVarSimWarp.GetValueOnGameThread(), -1), TEXT("Slower"));
	}));

static FAutoConsoleCommand CmdAnastasisSimPause(
	TEXT("Anastasis.Sim.Pause"),
	TEXT("Toggles the simulation pause (anastasis.Sim.Warp 0), then back to the previous warp. PIE: Pause key."),
	FConsoleCommandDelegate::CreateLambda([]()
	{
		const double Current = CVarSimWarp.GetValueOnGameThread();
		if (Current > 0.0)
		{
			GWarpBeforePause = Current;
			SetWarp(0.0, TEXT("Pause"));
		}
		else
		{
			SetWarp(GWarpBeforePause > 0.0 ? GWarpBeforePause : 1.0, TEXT("Resume"));
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs CmdAnastasisChronicleWrite(
	TEXT("Anastasis.Chronicle.Write"),
	TEXT("Anastasis.Chronicle.Write [file.txt] - writes the village chronicle (French text) to Saved/Chronicle/ and logs ANASTASIS_CHRONICLE written path=... with a JSON summary (CHRONIQUE_VILLAGE_001)."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		const UAnastasisSimulationSubsystem* Host = World ? World->GetSubsystem<UAnastasisSimulationSubsystem>() : nullptr;
		if (!Host || !Host->GetSimulation().IsRunning())
		{
			UE_LOG(LogAnastasis_UnrealV2, Warning, TEXT("ANASTASIS_CHRONICLE no running simulation in this world (PIE only)"));
			return;
		}
		Host->WriteChronicle(Args.IsValidIndex(0) ? Args[0] : FString());
	}));

static FAutoConsoleCommandWithWorldAndArgs CmdAnastasisNotebookWrite(
	TEXT("Anastasis.Carnet.Write"),
	TEXT("Anastasis.Carnet.Write [file.txt] - writes the incarnated player's notebook (what they heard, from whom, version by version) to Saved/Chronicle/ and logs ANASTASIS_NOTEBOOK written path=... (memoire-decisions-001)."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		const UAnastasisSimulationSubsystem* Host = World ? World->GetSubsystem<UAnastasisSimulationSubsystem>() : nullptr;
		if (!Host || !Host->GetSimulation().IsRunning())
		{
			UE_LOG(LogAnastasis_UnrealV2, Warning, TEXT("ANASTASIS_NOTEBOOK no running simulation in this world (PIE only)"));
			return;
		}
		Host->WriteNotebook(Args.IsValidIndex(0) ? Args[0] : FString());
	}));

static FAutoConsoleCommandWithWorldAndArgs CmdAnastasisChroniclePrint(
	TEXT("Anastasis.Chronicle.Print"),
	TEXT("Anastasis.Chronicle.Print [days=1] - logs the last N days of the village chronicle, one ANASTASIS_CHRONICLE line each (CHRONIQUE_VILLAGE_001)."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		const UAnastasisSimulationSubsystem* Host = World ? World->GetSubsystem<UAnastasisSimulationSubsystem>() : nullptr;
		if (!Host || !Host->GetSimulation().IsRunning())
		{
			UE_LOG(LogAnastasis_UnrealV2, Warning, TEXT("ANASTASIS_CHRONICLE no running simulation in this world (PIE only)"));
			return;
		}
		const int32 Wanted = Args.IsValidIndex(0) ? FMath::Max(1, FCString::Atoi(*Args[0])) : 1;
		const AnastasisChronicle::FVillageChronicle& Chronicle = Host->GetChronicle();
		const int32 From = Host->GetSimulation().GetDay() - Wanted + 1;
		for (const AnastasisChronicle::FEntry& Entry : Chronicle.GetEntries())
		{
			if (Entry.Day < From) continue;
			UE_LOG(LogAnastasis_UnrealV2, Display, TEXT("ANASTASIS_CHRONICLE jour %d, %s : %s"),
				Entry.Day, *AnastasisChronicle::HourLabel(Entry.Hour), *Entry.Text);
		}
		UE_LOG(LogAnastasis_UnrealV2, Display, TEXT("ANASTASIS_CHRONICLE status %s"), *Chronicle.StatusJson());
	}));

static FAutoConsoleCommandWithWorldAndArgs CmdAnastasisSimAdvance(
	TEXT("Anastasis.Sim.Advance"),
	TEXT("Anastasis.Sim.Advance <45 | 45s | 6h | 3d | @22 | @6:30> - jumps the simulation forward NOW, in the reference's 10x steps "
		"(seconds, simulated hours, days, or until the next given hour of the simulated day). For agents and proofs; "
		"logs ANASTASIS_SIM advance. All of it counts as idle time for the village (TIME_WARP_001)."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		UAnastasisSimulationSubsystem* Host = World ? World->GetSubsystem<UAnastasisSimulationSubsystem>() : nullptr;
		if (!Host || !Host->GetSimulation().IsRunning())
		{
			UE_LOG(LogAnastasis_UnrealV2, Warning, TEXT("ANASTASIS_SIM advance: no running simulation in this world (PIE only)"));
			return;
		}
		double Seconds = 0.0;
		FString Error;
		if (!AnastasisTimeWarp::ParseAdvance(Args.IsValidIndex(0) ? Args[0] : FString(), Host->GetSimulation().GetTime(),
				FAnastasisSimulation::DayLength, Seconds, Error))
		{
			UE_LOG(LogAnastasis_UnrealV2, Warning, TEXT("ANASTASIS_SIM advance refused: %s"), *Error);
			return;
		}
		Host->AdvanceBy(Seconds);
	}));

static FAutoConsoleCommandWithWorld CmdAnastasisSimTimeStatus(
	TEXT("Anastasis.Sim.TimeStatus"),
	TEXT("Logs the time warp: requested warp, speed, time scale, achieved simulated seconds per real second, presence and idle days (TIME_WARP_001)."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		UE_LOG(LogAnastasis_UnrealV2, Display, TEXT("ANASTASIS_SIM time %s"), *UAnastasisSimulationDebugLibrary::GetTimeWarpStatus(World));
	}));

FString UAnastasisSimulationSubsystem::SeedFirstHouse(int32 NpcCount, int32 TileX, int32 TileY)
{
	ReplaceStartVillage();
	if (!Simulation.IsRunning())
	{
		return FString();
	}
	AnastasisVillage::FVillage& Village = Simulation.GetVillage();
	const AnastasisNav::FNavGrid& Nav = Village.GetNavGrid();

	// Premiere case libre, en anneaux, dont la porte vers le camp est libre aussi.
	auto PlaceHouseNear = [&](int32 CX, int32 CY, int32& OutX, int32& OutY)
	{
		for (int32 Radius = 0; Radius <= 24; ++Radius)
		{
			for (int32 DY = -Radius; DY <= Radius; ++DY)
			{
				for (int32 DX = -Radius; DX <= Radius; ++DX)
				{
					if (FMath::Max(FMath::Abs(DX), FMath::Abs(DY)) != Radius) continue;
					const int32 X = CX + DX;
					const int32 Y = CY + DY;
					if (X < 2 || Y < 2 || X > Nav.W - 3 || Y > Nav.H - 3) continue;
					if (Village.IsFootBlocked(X + 0.5, Y + 0.5)) continue;
					if (Village.IsFootBlocked(X + 1.5, Y + 0.5) && Village.IsFootBlocked(X - 0.5, Y + 0.5)) continue;
					const FString Id = Village.AddBuilding(AnastasisVillage::HouseType, X, Y, 1.0, Simulation.GetDay());
					if (!Id.IsEmpty())
					{
						OutX = X;
						OutY = Y;
						return Id;
					}
				}
			}
		}
		return FString();
	};

	int32 HX = 0;
	int32 HY = 0;
	int32 FX = 0;
	int32 FY = 0;
	const FString Owned = PlaceHouseNear(TileX, TileY, HX, HY);
	const FString Free = PlaceHouseNear(TileX + 6, TileY, FX, FY);
	if (Owned.IsEmpty())
	{
		UE_LOG(LogAnastasis_UnrealV2, Warning, TEXT("ANASTASIS_VILLAGE first house: no free tile near (%d,%d)"), TileX, TileY);
		return Owned;
	}

	// Habitants autour, fatigues a des degres divers.
	for (int32 K = 0; K < NpcCount; ++K)
	{
		const double Angle = 2.0 * UE_DOUBLE_PI * K / FMath::Max(1, NpcCount);
		const int32 CX = HX + FMath::RoundToInt32(6.0 * FMath::Cos(Angle));
		const int32 CY = HY + FMath::RoundToInt32(6.0 * FMath::Sin(Angle));
		bool bPlaced = false;
		for (int32 R = 0; R <= 6 && !bPlaced; ++R)
		{
			for (int32 DY = -R; DY <= R && !bPlaced; ++DY)
			{
				for (int32 DX = -R; DX <= R && !bPlaced; ++DX)
				{
					const int32 X = CX + DX;
					const int32 Y = CY + DY;
					if (FMath::Max(FMath::Abs(DX), FMath::Abs(DY)) != R || !Nav.IsInBounds(X, Y)) continue;
					if (Village.IsFootBlocked(X + 0.5, Y + 0.5)) continue;
					AnastasisNeeds::FNeeds Needs;
					Needs.Hunger = 10.0;
					Needs.Energy = FMath::Max(8.0, 70.0 - 20.0 * K);
					Needs.Social = 70.0;
					Needs.Leisure = 70.0;
					Needs.Hygiene = 60.0;
					Needs.Thirst = 10.0;
					Needs.Health = 90.0;
					Needs.Morale = 55.0;
					const FString Id = Village.SpawnNpc(X + 0.5, Y + 0.5, Needs, 4.0);
					if (K == 0)
					{
						Village.AssignHome(Id, Owned);
					}
					bPlaced = true;
				}
			}
		}
	}
	// Ce que ferait le prochain minuit : les sans-toit recoivent un lit.
	const int32 Sheltered = Village.AssignSheltersDaily();

	SyncVillagePresentation();
	UE_LOG(
		LogAnastasis_UnrealV2,
		Display,
		TEXT("ANASTASIS_VILLAGE first house %s at (%d,%d) owned, %s at (%d,%d) free, %d inhabitants, %d sheltered"),
		*Owned,
		HX,
		HY,
		Free.IsEmpty() ? TEXT("-") : *Free,
		FX,
		FY,
		Village.GetActors().Num(),
		Sheltered);
	FAnastasisVillagePresentation::LogStatus(Village, Simulation.GetTime());
	return Owned;
}

int32 UAnastasisSimulationSubsystem::SeedArchitectureHamlet(int32 Houses, int32 NpcCount, int32 TileX, int32 TileY)
{
	ReplaceStartVillage();
	if (!Simulation.IsRunning())
	{
		return 0;
	}
	// Un hameau remplace TOUT village precedent (scenario compris) : rejoue deux fois, il retombe sur les
	// memes ids et les memes cases -- l'A/B avant / apres se fait aux memes cameras.
	if (Simulation.GetVillage().GetBuildings().Num() > 0 || Simulation.GetVillage().GetActors().Num() > 0)
	{
		UWorld* World = GetWorld();
		VillagePresentation.Clear(World ? World->GetSubsystem<UAnastasisVillageInteractionSubsystem>() : nullptr);
		ResetCanonical(Simulation.GetSeed());
	}
	AnastasisVillage::FVillage& Village = Simulation.GetVillage();
	const AnastasisNav::FNavGrid& Nav = Village.GetNavGrid();
	auto FreeAround = [&](int32 X, int32 Y)
	{
		int32 Count = 0;
		for (int32 DY = -1; DY <= 1; ++DY)
		{
			for (int32 DX = -1; DX <= 1; ++DX)
			{
				if ((DX || DY) && !Village.IsFootBlocked(X + DX + 0.5, Y + DY + 0.5)) ++Count;
			}
		}
		return Count;
	};
	// Parcelles voisines permises : chaque maisonnee garde sa ruelle de 2 m (ARCH-09) et une case d'acces libre.
	auto Place = [&](const TCHAR* Type, int32 CX, int32 CY, int32& OutX, int32& OutY)
	{
		for (int32 Radius = 0; Radius <= 12; ++Radius)
		{
			for (int32 DY = -Radius; DY <= Radius; ++DY)
			{
				for (int32 DX = -Radius; DX <= Radius; ++DX)
				{
					if (FMath::Max(FMath::Abs(DX), FMath::Abs(DY)) != Radius) continue;
					const int32 X = CX + DX;
					const int32 Y = CY + DY;
					if (X < 2 || Y < 2 || X > Nav.W - 3 || Y > Nav.H - 3) continue;
					if (Village.IsFootBlocked(X + 0.5, Y + 0.5) || FreeAround(X, Y) < 4) continue;
					const FString Id = Village.AddBuilding(Type, X, Y, 1.0, Simulation.GetDay());
					if (!Id.IsEmpty())
					{
						OutX = X;
						OutY = Y;
						return Id;
					}
				}
			}
		}
		return FString();
	};
	int32 WX = TileX;
	int32 WY = TileY;
	const FString Well = Place(AnastasisVillage::WellType, TileX, TileY, WX, WY);
	if (Well.IsEmpty())
	{
		UE_LOG(LogAnastasis_UnrealV2, Warning, TEXT("ANASTASIS_ARCH hamlet: no free tile near (%d,%d)"), TileX, TileY);
		return 0;
	}
	// Grappe irreguliere autour du puits : pas une grille, pas un cercle parfait.
	static const FIntPoint Offsets[] = {{1, 1}, {-1, 1}, {2, 0}, {-1, -1}, {1, -2}, {-2, 0}, {2, 2}, {-2, 2}, {3, -1}, {0, 3}};
	TArray<FString> HouseIds;
	for (int32 K = 0; K < FMath::Clamp(Houses, 1, UE_ARRAY_COUNT(Offsets)); ++K)
	{
		int32 X = 0;
		int32 Y = 0;
		const FString Id = Place(AnastasisVillage::HouseType, WX + Offsets[K].X, WY + Offsets[K].Y, X, Y);
		if (!Id.IsEmpty()) HouseIds.Add(Id);
	}
	int32 GX = 0;
	int32 GY = 0;
	const FString Granary = Place(AnastasisVillage::GranaryType, WX + 2, WY - 2, GX, GY);
	if (!Granary.IsEmpty())
	{
		Village.CreditFood(Granary, 40);
	}
	for (int32 K = 0; K < NpcCount; ++K)
	{
		const double Angle = 2.0 * UE_DOUBLE_PI * K / FMath::Max(1, NpcCount);
		const int32 CX = WX + FMath::RoundToInt32(2.5 * FMath::Cos(Angle));
		const int32 CY = WY + FMath::RoundToInt32(2.5 * FMath::Sin(Angle));
		bool bPlaced = false;
		for (int32 R = 0; R <= 6 && !bPlaced; ++R)
		{
			for (int32 DY = -R; DY <= R && !bPlaced; ++DY)
			{
				for (int32 DX = -R; DX <= R && !bPlaced; ++DX)
				{
					const int32 X = CX + DX;
					const int32 Y = CY + DY;
					if (FMath::Max(FMath::Abs(DX), FMath::Abs(DY)) != R || !Nav.IsInBounds(X, Y)) continue;
					if (Village.IsFootBlocked(X + 0.5, Y + 0.5)) continue;
					AnastasisNeeds::FNeeds Needs;
					Needs.Hunger = 10.0 + 5.0 * (K % 4);
					Needs.Energy = 70.0;
					Needs.Social = 70.0;
					Needs.Leisure = 70.0;
					Needs.Hygiene = 60.0;
					Needs.Thirst = 10.0 + 6.0 * (K % 5);
					Needs.Health = 90.0;
					Needs.Morale = 55.0;
					const FString Id = Village.SpawnNpc(X + 0.5, Y + 0.5, Needs, 4.0);
					// settlement-morphogenesis-001 : des foyers de metiers differents, pour que la forme de leurs
					// maisons ait une cause. Le premier cultive pour le grenier, le deuxieme et le quatrieme batissent ;
					// les autres sont des colons sans metier propre.
					if (K == 0 && !Granary.IsEmpty())
					{
						Village.AssignWorkplace(Id, AnastasisGather::JobFarmer, Granary);
					}
					else if (K == 1 || K == 3)
					{
						Village.SetJob(Id, AnastasisBuild::JobBuilder);
					}
					if (HouseIds.IsValidIndex(K))
					{
						Village.AssignHome(Id, HouseIds[K]);
					}
					bPlaced = true;
				}
			}
		}
	}
	const int32 Sheltered = Village.AssignSheltersDaily();
	SyncVillagePresentation();
	UE_LOG(LogAnastasis_UnrealV2, Display,
		TEXT("ANASTASIS_ARCH hamlet well=%s at (%d,%d) houses=%d granary=%s inhabitants=%d sheltered=%d"),
		*Well, WX, WY, HouseIds.Num(), Granary.IsEmpty() ? TEXT("-") : *Granary, Village.GetActors().Num(), Sheltered);
	FAnastasisVillagePresentation::LogStatus(Village, Simulation.GetTime());
	LogArchitecture();
	return 1 + HouseIds.Num() + (Granary.IsEmpty() ? 0 : 1);
}

void UAnastasisSimulationSubsystem::LogSettlement() const
{
	const AnastasisVillage::FVillage& Village = Simulation.GetVillage();
	const int32 Day = Simulation.GetDay();
	VillagePresentation.GetLedger().Log(Day);
	// Le passage et les sentiers : combien, ou, depuis quand, a quel cout.
	double Total = 0.0;
	double Peak = 0.0;
	int32 Hot = 0;
	for (const float T : Village.GetTraffic())
	{
		Total += T;
		Peak = FMath::Max<double>(Peak, T);
		Hot += T >= AnastasisTraffic::DesireTraffic ? 1 : 0;
	}
	const int32 W = Simulation.GetWorld().W;
	TArray<int32> Keys;
	Village.GetRoads().GetKeys(Keys);
	Keys.Sort();
	FString Cells;
	for (const int32 K : Keys)
	{
		const AnastasisTraffic::FRoadTile& R = Village.GetRoads()[K];
		Cells += FString::Printf(TEXT(" (%d,%d)j%d/%.0f/c%.2f"), K % FMath::Max(1, W), K / FMath::Max(1, W), R.BuiltDay, R.TrafficAtBirth,
			AnastasisNav::MoveCostAt(Village.GetNavGrid(), K % FMath::Max(1, W), K / FMath::Max(1, W)));
	}
	UE_LOG(LogAnastasis_UnrealV2, Display,
		TEXT("ANASTASIS_SETTLEMENT traffic day=%d evolution=%d passages=%lld total=%.1f peak=%.1f hot_cells=%d efforts=%d roads=%d paths_segments=%d grass_hidden=%d roads_at=[%s]"),
		Day, Village.IsRoadEvolutionEnabled() ? 1 : 0, Village.GetPassageCount(), Total, Peak, Hot, Village.GetRoadEfforts().Num(),
		Village.GetRoads().Num(), VillagePresentation.GetPaths().NumSegments(), VillagePresentation.GetPaths().NumHiddenGrass(), *Cells.TrimStart());
}

void UAnastasisSimulationSubsystem::LogArchitecture() const
{
	const AnastasisVillage::FVillage& Village = Simulation.GetVillage();
	for (const AnastasisVillage::FBuilding& B : Village.GetBuildings())
	{
		AnastasisArchitecture::EVariant Variant;
		if (!AnastasisArchitecture::ChooseVariant(B.Type, B.HousePhase, B.Id, Variant)) continue;
		// La forme vient de la biographie (settlement-morphogenesis-001) ; le repli de phase sinon.
		if (const AnastasisSettlement::FBiography* Bio = VillagePresentation.GetLedger().Find(B.Id))
		{
			Variant = Bio->Program;
		}
		const AnastasisArchitecture::FBuildingRecord Record = AnastasisArchitecture::Describe(Village, B, Variant, Simulation.GetDay());
		FString Where = TEXT("-");
		if (const AAnastasisVillageBuilding* Actor = VillagePresentation.FindActor(B.Id))
		{
			const FVector L = Actor->GetActorLocation();
			Where = FString::Printf(TEXT("(%.0f,%.0f,%.0f) yaw=%.0f pad=%.0f arch=%d"), L.X, L.Y, L.Z,
				Actor->GetActorRotation().Yaw, Actor->GetPadOffset(), Actor->HasArchitecture() ? 1 : 0);
		}
		UE_LOG(LogAnastasis_UnrealV2, Display, TEXT("%s at=%s"), *AnastasisArchitecture::ToLogLine(Record), *Where);
	}
}

FString UAnastasisSimulationSubsystem::SeedFirstGranary(int32 NpcCount, int32 Food, int32 TileX, int32 TileY)
{
	ReplaceStartVillage();
	if (!Simulation.IsRunning())
	{
		return FString();
	}
	AnastasisVillage::FVillage& Village = Simulation.GetVillage();
	const AnastasisNav::FNavGrid& Nav = Village.GetNavGrid();

	FString GranaryId;
	int32 GX = 0;
	int32 GY = 0;
	for (int32 Radius = 0; Radius <= 24 && GranaryId.IsEmpty(); ++Radius)
	{
		for (int32 DY = -Radius; DY <= Radius && GranaryId.IsEmpty(); ++DY)
		{
			for (int32 DX = -Radius; DX <= Radius && GranaryId.IsEmpty(); ++DX)
			{
				if (FMath::Max(FMath::Abs(DX), FMath::Abs(DY)) != Radius) continue;
				const int32 X = TileX + DX;
				const int32 Y = TileY + DY;
				if (X < 2 || Y < 2 || X > Nav.W - 3 || Y > Nav.H - 3) continue;
				if (Village.IsFootBlocked(X + 0.5, Y + 0.5)) continue;
				if (Village.IsFootBlocked(X + 1.5, Y + 0.5) && Village.IsFootBlocked(X - 0.5, Y + 0.5)) continue;
				GranaryId = Village.AddBuilding(AnastasisVillage::GranaryType, X, Y, 1.0, Simulation.GetDay());
				GX = X;
				GY = Y;
			}
		}
	}
	if (GranaryId.IsEmpty())
	{
		UE_LOG(LogAnastasis_UnrealV2, Warning, TEXT("ANASTASIS_VILLAGE first granary: no free tile near (%d,%d)"), TileX, TileY);
		return GranaryId;
	}
	const int32 Stored = Village.CreditFood(GranaryId, Food);

	for (int32 K = 0; K < NpcCount; ++K)
	{
		const double Angle = 2.0 * UE_DOUBLE_PI * K / FMath::Max(1, NpcCount);
		const int32 CX = GX + FMath::RoundToInt32(5.0 * FMath::Cos(Angle));
		const int32 CY = GY + FMath::RoundToInt32(5.0 * FMath::Sin(Angle));
		bool bPlaced = false;
		for (int32 R = 0; R <= 2 && !bPlaced; ++R)
		{
			for (int32 DY = -R; DY <= R && !bPlaced; ++DY)
			{
				for (int32 DX = -R; DX <= R && !bPlaced; ++DX)
				{
					const int32 X = CX + DX;
					const int32 Y = CY + DY;
					if (FMath::Max(FMath::Abs(DX), FMath::Abs(DY)) != R || !Nav.IsInBounds(X, Y)) continue;
					if (Village.IsFootBlocked(X + 0.5, Y + 0.5)) continue;
					AnastasisNeeds::FNeeds Needs;
					Needs.Hunger = FMath::Max(10.0, 80.0 - 15.0 * K);
					Needs.Energy = 80.0;
					Needs.Social = 70.0;
					Needs.Leisure = 70.0;
					Needs.Hygiene = 60.0;
					Needs.Thirst = 10.0;
					Needs.Health = 90.0;
					Needs.Morale = 55.0;
					Village.SpawnNpc(X + 0.5, Y + 0.5, Needs, 4.0);
					bPlaced = true;
				}
			}
		}
	}

	SyncVillagePresentation();
	UE_LOG(
		LogAnastasis_UnrealV2,
		Display,
		TEXT("ANASTASIS_VILLAGE first granary %s at (%d,%d), food=%d, %d inhabitants"),
		*GranaryId,
		GX,
		GY,
		Stored,
		Village.GetActors().Num());
	FAnastasisVillagePresentation::LogStatus(Village, Simulation.GetTime());
	return GranaryId;
}

namespace
{
	UAnastasisSimulationSubsystem* VillageHost(UWorld* World)
	{
		UAnastasisSimulationSubsystem* Host = World ? World->GetSubsystem<UAnastasisSimulationSubsystem>() : nullptr;
		if (!Host || !Host->GetSimulation().IsRunning())
		{
			UE_LOG(LogAnastasis_UnrealV2, Warning, TEXT("ANASTASIS_VILLAGE: no running simulation in this world (PIE only)"));
			return nullptr;
		}
		return Host;
	}
}

static FAutoConsoleCommandWithWorldAndArgs CmdAnastasisVillageFirstWell(
	TEXT("Anastasis.Village.FirstWell"),
	TEXT("Anastasis.Village.FirstWell [NpcCount=4] [TileX] [TileY] - poses the first well in the simulation near a tile (default: settlement) and inhabitants around it."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (UAnastasisSimulationSubsystem* Host = VillageHost(World))
		{
			const AnastasisVillage::FPoint Settlement = Host->GetSimulation().GetVillage().GetSettlement();
			const int32 Count = Args.IsValidIndex(0) ? FCString::Atoi(*Args[0]) : 4;
			const int32 X = Args.IsValidIndex(1) ? FCString::Atoi(*Args[1]) : FMath::FloorToInt32(Settlement.X);
			const int32 Y = Args.IsValidIndex(2) ? FCString::Atoi(*Args[2]) : FMath::FloorToInt32(Settlement.Y);
			Host->SeedFirstWell(Count, X, Y);
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs CmdAnastasisVillageFirstHouse(
	TEXT("Anastasis.Village.FirstHouse"),
	TEXT("Anastasis.Village.FirstHouse [NpcCount=4] [TileX] [TileY] - poses an owned house and a free one near a tile (default: settlement), and inhabitants: one owner, the others sheltered."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (UAnastasisSimulationSubsystem* Host = VillageHost(World))
		{
			const AnastasisVillage::FPoint Settlement = Host->GetSimulation().GetVillage().GetSettlement();
			const int32 Count = Args.IsValidIndex(0) ? FCString::Atoi(*Args[0]) : 4;
			const int32 X = Args.IsValidIndex(1) ? FCString::Atoi(*Args[1]) : FMath::FloorToInt32(Settlement.X);
			const int32 Y = Args.IsValidIndex(2) ? FCString::Atoi(*Args[2]) : FMath::FloorToInt32(Settlement.Y);
			Host->SeedFirstHouse(Count, X, Y);
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs CmdAnastasisVillageFirstGranary(
	TEXT("Anastasis.Village.FirstGranary"),
	TEXT("Anastasis.Village.FirstGranary [NpcCount=4] [Food=12] [TileX] [TileY] - poses a granary filled with food near a tile (default: settlement) and homeless, hungry inhabitants who can see it."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (UAnastasisSimulationSubsystem* Host = VillageHost(World))
		{
			const AnastasisVillage::FPoint Settlement = Host->GetSimulation().GetVillage().GetSettlement();
			const int32 Count = Args.IsValidIndex(0) ? FCString::Atoi(*Args[0]) : 4;
			const int32 Food = Args.IsValidIndex(1) ? FCString::Atoi(*Args[1]) : 12;
			const int32 X = Args.IsValidIndex(2) ? FCString::Atoi(*Args[2]) : FMath::FloorToInt32(Settlement.X);
			const int32 Y = Args.IsValidIndex(3) ? FCString::Atoi(*Args[3]) : FMath::FloorToInt32(Settlement.Y);
			Host->SeedFirstGranary(Count, Food, X, Y);
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs CmdAnastasisVillageHamlet(
	TEXT("Anastasis.Village.Hamlet"),
	TEXT("Anastasis.Village.Hamlet [Houses=6] [NpcCount=10] [TileX] [TileY] - ARCHITECTURE_SCALE_001 : un puits, des maisons en grappe, un grenier et leurs habitants (remplace le village)."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (UAnastasisSimulationSubsystem* Host = VillageHost(World))
		{
			const AnastasisVillage::FPoint Settlement = Host->GetSimulation().GetVillage().GetSettlement();
			const int32 Houses = Args.IsValidIndex(0) ? FCString::Atoi(*Args[0]) : 6;
			const int32 Count = Args.IsValidIndex(1) ? FCString::Atoi(*Args[1]) : 10;
			const int32 X = Args.IsValidIndex(2) ? FCString::Atoi(*Args[2]) : FMath::FloorToInt32(Settlement.X);
			const int32 Y = Args.IsValidIndex(3) ? FCString::Atoi(*Args[3]) : FMath::FloorToInt32(Settlement.Y);
			Host->SeedArchitectureHamlet(Houses, Count, X, Y);
		}
	}));

static FAutoConsoleCommandWithWorld CmdAnastasisVillageSettlementReport(
	TEXT("Anastasis.Village.SettlementReport"),
	TEXT("SETTLEMENT_MORPHOGENESIS_001 : la biographie de chaque batiment (lignes ANASTASIS_SETTLEMENT bio) et l'etat du passage et des sentiers (ANASTASIS_SETTLEMENT traffic)."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if (UAnastasisSimulationSubsystem* Host = VillageHost(World))
		{
			Host->LogSettlement();
		}
	}));

static FAutoConsoleCommandWithWorld CmdAnastasisVillageArchitectureReport(
	TEXT("Anastasis.Village.ArchitectureReport"),
	TEXT("ARCHITECTURE_SCALE_001 : journalise la fiche de chaque batiment (archetype, capacite, occupants, stockage, etat, age, assise) -- lignes ANASTASIS_ARCH record."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if (UAnastasisSimulationSubsystem* Host = VillageHost(World))
		{
			Host->LogArchitecture();
		}
	}));

static FAutoConsoleCommandWithWorld CmdAnastasisVillageStatus(
	TEXT("Anastasis.Village.Status"),
	TEXT("Logs every simulated building (state, access points, users) and inhabitant (goal, needs, target, why)."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if (UAnastasisSimulationSubsystem* Host = VillageHost(World))
		{
			FAnastasisVillagePresentation::LogStatus(Host->GetSimulation().GetVillage(), Host->GetSimulation().GetTime());
		}
	}));

static FAutoConsoleCommandWithWorld CmdAnastasisVillageCapacities(
	TEXT("Anastasis.Village.Capacities"),
	TEXT("Logs a JSON snapshot of occupied housing, food capacity, active sites, actual work and needs."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if (const UAnastasisSimulationSubsystem* Host = VillageHost(World))
		{
			const FAnastasisSimulation& Sim = Host->GetSimulation();
			UE_LOG(LogAnastasis_UnrealV2, Display, TEXT("BUILDING_CAPACITIES %s"),
				*FAnastasisBuildingCapacitySnapshot::Capture(Sim.GetVillage(), Sim.GetTime()).ToJson());
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs CmdAnastasisVillageRemoveBuilding(
	TEXT("Anastasis.Village.RemoveBuilding"),
	TEXT("Anastasis.Village.RemoveBuilding <building-N> - removes a building from the simulation; its actor follows."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		UAnastasisSimulationSubsystem* Host = VillageHost(World);
		if (!Host || !Args.IsValidIndex(0))
		{
			return;
		}
		const bool bRemoved = Host->GetSimulation().GetVillage().RemoveBuilding(Args[0]);
		Host->SyncVillagePresentation();
		UE_LOG(LogAnastasis_UnrealV2, Display, TEXT("ANASTASIS_VILLAGE remove %s -> %s"), *Args[0], bRemoved ? TEXT("removed") : TEXT("unknown id"));
	}));

static FAutoConsoleCommandWithWorldAndArgs CmdAnastasisVillageRemoveNpc(
	TEXT("Anastasis.Village.RemoveNpc"),
	TEXT("Anastasis.Village.RemoveNpc <npc-N> - removes an inhabitant from the simulation, even mid-interaction."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		UAnastasisSimulationSubsystem* Host = VillageHost(World);
		if (!Host || !Args.IsValidIndex(0))
		{
			return;
		}
		const bool bRemoved = Host->GetSimulation().GetVillage().RemoveNpc(Args[0]);
		UE_LOG(LogAnastasis_UnrealV2, Display, TEXT("ANASTASIS_VILLAGE remove %s -> %s"), *Args[0], bRemoved ? TEXT("removed") : TEXT("unknown id"));
	}));

namespace
{
	const FAnastasisSimulation* DebugSimulation(const UObject* WorldContextObject)
	{
		const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
		const UAnastasisSimulationSubsystem* Host = World ? World->GetSubsystem<UAnastasisSimulationSubsystem>() : nullptr;
		return Host && Host->GetSimulation().IsRunning() ? &Host->GetSimulation() : nullptr;
	}
}

namespace
{
	const UAnastasisSimulationSubsystem* DebugHost(const UObject* WorldContextObject)
	{
		const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
		const UAnastasisSimulationSubsystem* Host = World ? World->GetSubsystem<UAnastasisSimulationSubsystem>() : nullptr;
		return Host && Host->GetSimulation().IsRunning() ? Host : nullptr;
	}
}

FString UAnastasisSimulationDebugLibrary::GetChronicleText(const UObject* WorldContextObject)
{
	const UAnastasisSimulationSubsystem* Host = DebugHost(WorldContextObject);
	return Host ? Host->GetChronicle().Render() : FString();
}

FString UAnastasisSimulationDebugLibrary::GetChronicleStatus(const UObject* WorldContextObject)
{
	const UAnastasisSimulationSubsystem* Host = DebugHost(WorldContextObject);
	return Host ? Host->GetChronicle().StatusJson() : FString();
}

FString UAnastasisSimulationDebugLibrary::GetNotebookText(const UObject* WorldContextObject)
{
	const UAnastasisSimulationSubsystem* Host = DebugHost(WorldContextObject);
	return Host ? Host->GetNotebook().Render() : FString();
}

FString UAnastasisSimulationDebugLibrary::GetArrivalsStatus(const UObject* WorldContextObject)
{
	const UAnastasisSimulationSubsystem* Host = DebugHost(WorldContextObject);
	return Host ? AnastasisArrivals::StatusJson(Host->GetSimulation()) : FString();
}

FString UAnastasisSimulationDebugLibrary::GetNotebookStatus(const UObject* WorldContextObject)
{
	const UAnastasisSimulationSubsystem* Host = DebugHost(WorldContextObject);
	return Host ? Host->GetNotebook().StatusJson() : FString();
}

FString UAnastasisSimulationDebugLibrary::WriteNotebook(const UObject* WorldContextObject, const FString& FileName)
{
	const UAnastasisSimulationSubsystem* Host = DebugHost(WorldContextObject);
	return Host ? Host->WriteNotebook(FileName) : FString();
}

FString UAnastasisSimulationDebugLibrary::WriteChronicle(const UObject* WorldContextObject, const FString& FileName)
{
	const UAnastasisSimulationSubsystem* Host = DebugHost(WorldContextObject);
	return Host ? Host->WriteChronicle(FileName) : FString();
}

double UAnastasisSimulationDebugLibrary::GetSimulationTime(const UObject* WorldContextObject)
{
	const FAnastasisSimulation* Sim = DebugSimulation(WorldContextObject);
	return Sim ? Sim->GetTime() : -1.0;
}

double UAnastasisSimulationDebugLibrary::GetRainCanopyCover(const UObject* WorldContextObject, double SimX, double SimY)
{
	const FAnastasisSimulation* Sim = DebugSimulation(WorldContextObject);
	return Sim ? Sim->GetVillage().GetRainCanopyCover(SimX, SimY) : 0.0;
}

FString UAnastasisSimulationDebugLibrary::GetVillagePhase(const UObject* WorldContextObject)
{
	const FAnastasisSimulation* Sim = DebugSimulation(WorldContextObject);
	return Sim ? FString(AnastasisRhythm::PhaseId(AnastasisRhythm::VillagePhase(AnastasisRhythm::DayFracOf(Sim->GetTime())))) : FString();
}

int32 UAnastasisSimulationDebugLibrary::CountInside(const UObject* WorldContextObject, const FString& BuildingId)
{
	const FAnastasisSimulation* Sim = DebugSimulation(WorldContextObject);
	return Sim ? Sim->GetVillage().InsideOf(BuildingId).Num() : -1;
}

FString UAnastasisSimulationDebugLibrary::GetNpcState(const UObject* WorldContextObject, const FString& NpcId)
{
	const FAnastasisSimulation* Sim = DebugSimulation(WorldContextObject);
	const AnastasisVillage::FNpc* N = Sim ? Sim->GetVillage().FindNpc(NpcId) : nullptr;
	if (!N)
	{
		return FString();
	}
	return FString::Printf(TEXT("%s|%s|%s|%s|%d"), *N->Goal, *N->Activity,
		N->Inside.bActive ? *N->Inside.BuildingId : TEXT("-"), N->Inside.bActive ? *N->Inside.Goal : TEXT("-"), N->SheltersTaken);
}

int32 UAnastasisSimulationDebugLibrary::GetFoodStock(const UObject* WorldContextObject, const FString& BuildingId)
{
	const FAnastasisSimulation* Sim = DebugSimulation(WorldContextObject);
	const AnastasisVillage::FBuilding* B = Sim ? Sim->GetVillage().FindBuilding(BuildingId) : nullptr;
	return B ? B->FoodPhysical : -1;
}

int32 UAnastasisSimulationDebugLibrary::CountMealsTaken(const UObject* WorldContextObject)
{
	const FAnastasisSimulation* Sim = DebugSimulation(WorldContextObject);
	if (!Sim) return -1;
	int32 Meals = 0;
	for (const AnastasisVillage::FNpc& N : Sim->GetVillage().GetActors()) Meals += N.MealsTaken;
	return Meals;
}

// One opt-in circuit. Uses an actual Food tile near the settlement, an empty
// granary and one healthy adult. Repeating the command cannot inject/refill food.
bool UAnastasisSimulationSubsystem::SeedFoodSupply()
{
	ReplaceStartVillage();
	using namespace AnastasisVillage;
	if (!Simulation.IsRunning()) return false;
	auto& V = Simulation.GetVillage();
	if (!V.GetFoodSources().IsEmpty() || !V.GetActors().IsEmpty() || !V.GetBuildings().IsEmpty())
	{
		UE_LOG(LogAnastasis_UnrealV2, Warning, TEXT("FOOD_SUPPLY refused: start in an empty village; existing state preserved"));
		return false;
	}
	const auto& W = Simulation.GetWorld();
	TArray<int32> Candidates;
	for (int32 I=0; I<W.Tiles.Num(); ++I)
	{
		const auto& T = W.Tiles[I];
		if (T.Resource == AnastasisWorld::EResource::Food && T.Amount > 0 && !V.IsFootBlocked(T.X+0.5,T.Y+0.5)) Candidates.Add(I);
	}
	const auto Center=V.GetSettlement();
	Candidates.StableSort([&](int32 A, int32 B)
	{
		const auto& X=W.Tiles[A]; const auto& Y=W.Tiles[B];
		return FMath::Square(X.X-Center.X)+FMath::Square(X.Y-Center.Y) < FMath::Square(Y.X-Center.X)+FMath::Square(Y.Y-Center.Y);
	});
	for (int32 I:Candidates)
	{
		const auto& T=W.Tiles[I];
		for (int32 DY=-4; DY<=4; ++DY) for (int32 DX=-4; DX<=4; ++DX)
		{
			if (FMath::Max(FMath::Abs(DX),FMath::Abs(DY))!=4) continue;
			const int32 X=T.X+DX,Y=T.Y+DY;
			if (V.IsFootBlocked(X+0.5,Y+0.5)) continue;
			const FString B=V.AddBuilding(GranaryType,X,Y);
			if(B.IsEmpty()) continue;
			bool bReachable=false;
			FPoint Start;
			const AnastasisPath::FWorldNavSource Nav(V.GetNavGrid(),W);
			for(const auto& Door:V.FindBuilding(B)->AccessPoints)
			{
				TArray<FPoint> Path;
				if(AnastasisPath::FindPath(Nav,Door,{T.X+0.5,T.Y+0.5},{},Path)) {Start=Door;bReachable=true;break;}
			}
			if(!bReachable) {V.RemoveBuilding(B);continue;}
			V.ActivateFoodSource(T.X,T.Y);
			V.SetSettlement(Start.X,Start.Y);
			AnastasisNeeds::FNeeds N; N.Hunger=10; N.Energy=95; N.Hygiene=95; N.Social=95; N.Leisure=95;
			V.SpawnNpc(Start.X,Start.Y,N);
			SyncVillagePresentation();
			UE_LOG(LogAnastasis_UnrealV2,Display,TEXT("FOOD_SUPPLY ready source=(%d,%d) initial=%d depot=%s empty=1"),T.X,T.Y,T.Amount,*B);
			FAnastasisVillagePresentation::LogStatus(V,Simulation.GetTime());
			return true;
		}
	}
	UE_LOG(LogAnastasis_UnrealV2,Warning,TEXT("FOOD_SUPPLY no reachable generated source"));
	return false;
}

static FAutoConsoleCommandWithWorld CmdAnastasisFoodSupply(
	TEXT("Anastasis.Village.FoodSupply"),
	TEXT("Start one finite food circuit in an empty village: generated resource, empty granary, autonomous inhabitant. No refill."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if(auto* Host=World ? World->GetSubsystem<UAnastasisSimulationSubsystem>() : nullptr) Host->SeedFoodSupply();
	}));

FString UAnastasisSimulationDebugLibrary::GetFoodSupplyStatus(const UObject* WorldContextObject)
{
	const auto* Sim=DebugSimulation(WorldContextObject);
	if(!Sim) return TEXT("{}");
	const auto& V=Sim->GetVillage();
	int32 Initial=0,Remaining=0,Bag=0,Stored=0,Meals=0,Gathered=0,Delivered=0;
	for(const auto& S:V.GetFoodSources()) {Initial+=S.Initial;Remaining+=S.Remaining;}
	for(const auto& B:V.GetBuildings()) Stored+=B.FoodPhysical;
	for(const auto& N:V.GetActors()) {Bag+=N.InventoryFood;Meals+=N.MealsTaken;Gathered+=N.GatheredFood;Delivered+=N.DeliveredFood;}
	const auto* First = V.GetActors().IsEmpty() ? nullptr : &V.GetActors()[0];
	return FString::Printf(TEXT("{\"x\":%.5f,\"y\":%.5f,\"time\":%.4f,\"initial\":%d,\"remaining\":%d,\"bag\":%d,\"stock\":%d,\"meals\":%d,\"gathered\":%d,\"delivered\":%d}"),First ? First->X : 0.0,First ? First->Y : 0.0,Sim->GetTime(),Initial,Remaining,Bag,Stored,Meals,Gathered,Delivered);
}

// Le fermier au grenier (gather-deliver-001). Un champ genere, un grenier vide a
// 3-4 cases dont un seuil atteint le champ, des fermiers embauches au seuil.
FString UAnastasisSimulationSubsystem::SeedFirstFarmer(int32 FarmerCount, int32 TileX, int32 TileY)
{
	ReplaceStartVillage();
	using namespace AnastasisVillage;
	if (!Simulation.IsRunning())
	{
		return FString();
	}
	FVillage& V = Simulation.GetVillage();
	const AnastasisWorld::FWorld& W = Simulation.GetWorld();
	TArray<int32> Fields;
	for (int32 I = 0; I < W.Tiles.Num(); ++I)
	{
		const AnastasisWorld::FTile Tile = V.LiveTileAt(W.Tiles[I].X, W.Tiles[I].Y);
		if (Tile.Resource == AnastasisWorld::EResource::Food && Tile.Amount > 0 && !V.IsFootBlocked(Tile.X + 0.5, Tile.Y + 0.5)) Fields.Add(I);
	}
	Fields.StableSort([&](int32 A, int32 B)
	{
		const AnastasisWorld::FTile& TA = W.Tiles[A];
		const AnastasisWorld::FTile& TB = W.Tiles[B];
		return FMath::Square(TA.X - TileX) + FMath::Square(TA.Y - TileY) < FMath::Square(TB.X - TileX) + FMath::Square(TB.Y - TileY);
	});
	const AnastasisPath::FWorldNavSource NavSource(V.GetNavGrid(), W);
	for (const int32 FieldIndex : Fields)
	{
		const AnastasisWorld::FTile& Field = W.Tiles[FieldIndex];
		for (int32 R = 3; R <= 4; ++R)
		{
			for (int32 DY = -R; DY <= R; ++DY)
			{
				for (int32 DX = -R; DX <= R; ++DX)
				{
					if (FMath::Max(FMath::Abs(DX), FMath::Abs(DY)) != R) continue;
					const int32 X = Field.X + DX;
					const int32 Y = Field.Y + DY;
					if (X < 2 || Y < 2 || X > W.W - 3 || Y > W.H - 3) continue;
					// Pas sur un champ : le grenier ne mange pas la recolte.
					if (V.LiveTileAt(X, Y).Resource != AnastasisWorld::EResource::None || V.IsFootBlocked(X + 0.5, Y + 0.5)) continue;
					const FString Granary = V.AddBuilding(GranaryType, X, Y, 1.0, Simulation.GetDay());
					if (Granary.IsEmpty()) continue;
					const FPoint* Door = nullptr;
					for (const FPoint& P : V.FindBuilding(Granary)->AccessPoints)
					{
						TArray<FPoint> Path;
						if (AnastasisPath::FindPath(NavSource, P, { Field.X + 0.5, Field.Y + 0.5 }, {}, Path))
						{
							Door = &P;
							break;
						}
					}
					if (!Door)
					{
						V.RemoveBuilding(Granary);
						continue;
					}
					const FPoint Start = *Door;
					for (int32 K = 0; K < FMath::Max(1, FarmerCount); ++K)
					{
						AnastasisNeeds::FNeeds N;
						N.Hunger = 10.0;
						N.Energy = 90.0;
						N.Social = 80.0;
						N.Leisure = 80.0;
						N.Hygiene = 80.0;
						N.Thirst = 5.0;
						N.Health = 95.0;
						N.Morale = 60.0;
						const FString Id = V.SpawnNpc(Start.X, Start.Y, N);
						V.AssignWorkplace(Id, AnastasisGather::JobFarmer, Granary);
					}
					FarmerGranaryId = Granary;
					FarmerField = FIntPoint(Field.X, Field.Y);
					SyncVillagePresentation();
					UE_LOG(LogAnastasis_UnrealV2, Display,
						TEXT("ANASTASIS_VILLAGE first farmer granary=%s at (%d,%d) field=(%d,%d) food=%d farmers=%d"),
						*Granary, X, Y, Field.X, Field.Y, V.LiveTileAt(Field.X, Field.Y).Amount, FMath::Max(1, FarmerCount));
					FAnastasisVillagePresentation::LogStatus(V, Simulation.GetTime());
					return Granary;
				}
			}
		}
	}
	UE_LOG(LogAnastasis_UnrealV2, Warning, TEXT("ANASTASIS_VILLAGE first farmer: no reachable generated field near (%d,%d)"), TileX, TileY);
	return FString();
}

static FAutoConsoleCommandWithWorldAndArgs CmdAnastasisVillageFirstFarmer(
	TEXT("Anastasis.Village.FirstFarmer"),
	TEXT("Anastasis.Village.FirstFarmer [FarmerCount=1] [TileX] [TileY] - an empty granary near the generated field closest to a tile (default: settlement), and farmers hired there who gather then deliver."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (UAnastasisSimulationSubsystem* Host = VillageHost(World))
		{
			const AnastasisVillage::FPoint Settlement = Host->GetSimulation().GetVillage().GetSettlement();
			const int32 Count = Args.IsValidIndex(0) ? FCString::Atoi(*Args[0]) : 1;
			const int32 X = Args.IsValidIndex(1) ? FCString::Atoi(*Args[1]) : FMath::FloorToInt32(Settlement.X);
			const int32 Y = Args.IsValidIndex(2) ? FCString::Atoi(*Args[2]) : FMath::FloorToInt32(Settlement.Y);
			Host->SeedFirstFarmer(Count, X, Y);
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs CmdAnastasisVillageForceWeather(
	TEXT("Anastasis.Village.ForceWeather"),
	TEXT("Anastasis.Village.ForceWeather <rain 0..1> [snow] [wind] [spring|summer|autumn|winter] | off - imposes what the inhabitants read "
		"(the reference's sim.forceWeather: a debug hook). Rain >= 0.48 is a storm: outdoor work is dropped for shelter. "
		"The SKY is not forced: it keeps the simulation's own weather."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		UAnastasisSimulationSubsystem* Host = VillageHost(World);
		if (!Host || !Args.IsValidIndex(0))
		{
			return;
		}
		AnastasisVillage::FVillage& Village = Host->GetSimulation().GetVillage();
		if (Args[0].Equals(TEXT("off"), ESearchCase::IgnoreCase))
		{
			Village.ClearForcedWeather();
			UE_LOG(LogAnastasis_UnrealV2, Display, TEXT("ANASTASIS_VILLAGE_WEATHER forced=0 (the simulation's weather again)"));
			return;
		}
		AnastasisWeatherBehavior::FSimWeather Weather;
		Weather.Rain = FMath::Clamp(FCString::Atod(*Args[0]), 0.0, 1.0);
		Weather.Snow = Args.IsValidIndex(1) ? FMath::Clamp(FCString::Atod(*Args[1]), 0.0, 1.0) : 0.0;
		Weather.Wind = Args.IsValidIndex(2) ? FMath::Clamp(FCString::Atod(*Args[2]), 0.0, 1.0) : 0.0;
		if (Args.IsValidIndex(3))
		{
			const FString S = Args[3].ToLower();
			Weather.Season = S == TEXT("spring") ? AnastasisWeather::ESeason::Spring
				: S == TEXT("autumn") ? AnastasisWeather::ESeason::Autumn
				: S == TEXT("winter") ? AnastasisWeather::ESeason::Winter
				: AnastasisWeather::ESeason::Summer;
		}
		Village.SetForcedWeather(Weather);
		UE_LOG(LogAnastasis_UnrealV2, Display, TEXT("ANASTASIS_VILLAGE_WEATHER forced=1 rain=%.3f snow=%.3f wind=%.3f season=%s storm=%d"),
			Weather.Rain, Weather.Snow, Weather.Wind, AnastasisWeather::SeasonId(Weather.Season),
			Weather.Rain >= AnastasisWeatherBehavior::Shelter::RainHeavy ? 1 : 0);
	}));

FString UAnastasisSimulationSubsystem::SeedFirstSite(const FString& Type, int32 BuilderCount, bool bDelivered, int32 TileX, int32 TileY)
{
	ReplaceStartVillage();
	using namespace AnastasisVillage;
	if (!Simulation.IsRunning())
	{
		return FString();
	}
	FVillage& V = Simulation.GetVillage();
	const AnastasisWorld::FWorld& W = Simulation.GetWorld();
	for (int32 R = 0; R <= 12; ++R)
	{
		for (int32 DY = -R; DY <= R; ++DY)
		{
			for (int32 DX = -R; DX <= R; ++DX)
			{
				if (FMath::Max(FMath::Abs(DX), FMath::Abs(DY)) != R) continue;
				const int32 X = TileX + DX;
				const int32 Y = TileY + DY;
				if (X < 2 || Y < 2 || X > W.W - 3 || Y > W.H - 3) continue;
				if (V.LiveTileAt(X, Y).Resource != AnastasisWorld::EResource::None || V.IsFootBlocked(X + 0.5, Y + 0.5)) continue;
				const FString Site = V.OpenSite(Type, X, Y, bDelivered);
				if (Site.IsEmpty()) continue;
				const FBuilding* Building = V.FindBuilding(Site);
				if (Building->AccessPoints.Num() == 0)
				{
					V.RemoveBuilding(Site);
					continue;
				}
				const FPoint Start = Building->AccessPoints[0];
				FString CourierId;
				for (int32 K = 0; K < FMath::Max(1, BuilderCount); ++K)
				{
					AnastasisNeeds::FNeeds N;
					N.Hunger = 10.0;
					N.Energy = 90.0;
					N.Social = 80.0;
					N.Leisure = 80.0;
					N.Hygiene = 80.0;
					N.Thirst = 5.0;
					N.Health = 95.0;
					N.Morale = 60.0;
					const FString Id = V.SpawnNpc(Start.X, Start.Y, N);
					V.SetJob(Id, AnastasisBuild::JobBuilder);
					if (!bDelivered && CourierId.IsEmpty()) CourierId = Id;
				}
				if (!CourierId.IsEmpty()) V.SetMaterialCourier(CourierId);
				FirstSiteId = Site;
				SyncVillagePresentation();
				UE_LOG(LogAnastasis_UnrealV2, Display,
					TEXT("ANASTASIS_VILLAGE first site %s type=%s at (%d,%d) delivered=%d wood=%d stone=%d builders=%d courier=%s"),
					*Site, *Type, X, Y, bDelivered ? 1 : 0, Building->Materials.NeedWood, Building->Materials.NeedStone, FMath::Max(1, BuilderCount), CourierId.IsEmpty() ? TEXT("none") : *CourierId);
				FAnastasisVillagePresentation::LogStatus(V, Simulation.GetTime());
				return Site;
			}
		}
	}
	UE_LOG(LogAnastasis_UnrealV2, Warning, TEXT("ANASTASIS_VILLAGE first site: no free tile for %s near (%d,%d)"), *Type, TileX, TileY);
	return FString();
}

static FAutoConsoleCommandWithWorldAndArgs CmdAnastasisVillageFirstSite(
	TEXT("Anastasis.Village.FirstSite"),
	TEXT("Anastasis.Village.FirstSite [Type=house] [BuilderCount=2] [Delivered=1] [TileX] [TileY] - opens a construction site (well, house, granary) near a tile (default: settlement), its estimate delivered on site, and builders who raise it piece by piece."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (UAnastasisSimulationSubsystem* Host = VillageHost(World))
		{
			const AnastasisVillage::FPoint Settlement = Host->GetSimulation().GetVillage().GetSettlement();
			const FString Type = Args.IsValidIndex(0) ? Args[0] : FString(AnastasisVillage::HouseType);
			const int32 Count = Args.IsValidIndex(1) ? FCString::Atoi(*Args[1]) : 2;
			const bool bDelivered = Args.IsValidIndex(2) ? FCString::Atoi(*Args[2]) != 0 : true;
			const int32 X = Args.IsValidIndex(3) ? FCString::Atoi(*Args[3]) : FMath::FloorToInt32(Settlement.X);
			const int32 Y = Args.IsValidIndex(4) ? FCString::Atoi(*Args[4]) : FMath::FloorToInt32(Settlement.Y);
			Host->SeedFirstSite(Type, Count, bDelivered, X, Y);
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs CmdAnastasisVillageDeliverSite(
	TEXT("Anastasis.Village.DeliverSite"),
	TEXT("Anastasis.Village.DeliverSite <BuildingId> <Wood> <Stone> - credits materials to a construction site's stock (the porters are not ported)."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		UAnastasisSimulationSubsystem* Host = VillageHost(World);
		if (!Host || Args.Num() < 3) return;
		const int32 In = Host->GetSimulation().GetVillage().CreditSiteMaterials(Args[0], FCString::Atoi(*Args[1]), FCString::Atoi(*Args[2]));
		UE_LOG(LogAnastasis_UnrealV2, Display, TEXT("ANASTASIS_VILLAGE deliver site %s: %d units in"), *Args[0], In);
		// Le stock livre se voit tout de suite, comme apres FirstSite : sans cela il attend le prochain Tick du
		// sous-systeme, et une frame lente (lot d'integration) laisse lire le chantier d'avant la livraison.
		Host->SyncVillagePresentation();
	}));

FString UAnastasisSimulationDebugLibrary::GetGatherStatus(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	const UAnastasisSimulationSubsystem* Host = World ? World->GetSubsystem<UAnastasisSimulationSubsystem>() : nullptr;
	if (!Host || !Host->GetSimulation().IsRunning()) return TEXT("{}");
	const FAnastasisSimulation& Sim = Host->GetSimulation();
	const AnastasisVillage::FVillage& V = Sim.GetVillage();
	const AnastasisWorld::FWorld& W = Sim.GetWorld();
	const AnastasisVillage::FBuilding* Granary = V.FindBuilding(Host->GetFarmerGranaryId());
	if (!Granary) return TEXT("{\"granary\":-1}");
	int32 Field = 0;
	for (const AnastasisWorld::FTile& Generated : W.Tiles)
	{
		const AnastasisWorld::FTile Tile = V.LiveTileAt(Generated.X, Generated.Y);
		if (Tile.Resource == AnastasisWorld::EResource::Food) Field += Tile.Amount;
	}
	int32 Bag = 0, Stock = 0, Meals = 0, Deliveries = 0;
	for (const AnastasisVillage::FBuilding& B : V.GetBuildings()) Stock += B.FoodPhysical;
	const AnastasisVillage::FNpc* Farmer = nullptr;
	for (const AnastasisVillage::FNpc& N : V.GetActors())
	{
		Bag += N.InventoryFood;
		Meals += N.MealsTaken;
		if (N.WorkplaceId == Granary->Id)
		{
			Deliveries += N.Deliveries;
			if (!Farmer) Farmer = &N;
		}
	}
	UWorld* PresentationWorld = const_cast<UWorld*>(World);
	const FVector G = FAnastasisVillagePresentation::SimToUnreal(W, Granary->X + 0.5, Granary->Y + 0.5, PresentationWorld);
	const FIntPoint FieldTile = Host->GetFarmerField();
	const FVector F = FAnastasisVillagePresentation::SimToUnreal(W, FieldTile.X + 0.5, FieldTile.Y + 0.5, PresentationWorld);
	const FVector N = Farmer ? FAnastasisVillagePresentation::SimToUnreal(W, Farmer->X, Farmer->Y, PresentationWorld) : G;
	return FString::Printf(
		TEXT("{\"time\":%.4f,\"granary\":%d,\"field\":%d,\"bag\":%d,\"stock\":%d,\"meals\":%d,\"deliveries\":%d,")
		TEXT("\"goal\":\"%s\",\"activity\":\"%s\",\"session\":%s,")
		TEXT("\"gx\":%.1f,\"gy\":%.1f,\"gz\":%.1f,\"fx\":%.1f,\"fy\":%.1f,\"fz\":%.1f,\"nx\":%.1f,\"ny\":%.1f,\"nz\":%.1f}"),
		Sim.GetTime(), Granary->FoodPhysical, Field, Bag, Stock, Meals, Deliveries,
		Farmer ? *Farmer->Goal : TEXT(""), Farmer ? *Farmer->Activity : TEXT(""),
		Farmer && Farmer->WorkSession.bActive ? TEXT("true") : TEXT("false"),
		G.X, G.Y, G.Z, F.X, F.Y, F.Z, N.X, N.Y, N.Z);
}

FString UAnastasisSimulationDebugLibrary::GetBuildStatus(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	const UAnastasisSimulationSubsystem* Host = World ? World->GetSubsystem<UAnastasisSimulationSubsystem>() : nullptr;
	if (!Host || !Host->GetSimulation().IsRunning()) return TEXT("{}");
	const FAnastasisSimulation& Sim = Host->GetSimulation();
	const AnastasisVillage::FVillage& V = Sim.GetVillage();
	const AnastasisWorld::FWorld& W = Sim.GetWorld();
	const AnastasisVillage::FBuilding* Site = V.FindBuilding(Host->GetFirstSiteId());
	if (!Site) return TEXT("{\"site\":false}");
	int32 ByNpcs = 0;
	const AnastasisVillage::FNpc* Builder = nullptr;
	int32 Building = 0;
	for (const AnastasisVillage::FNpc& N : V.GetActors())
	{
		ByNpcs += N.PiecesPlaced;
		if (N.Goal == AnastasisBuild::GoalBuild) ++Building;
		if (!Builder && N.JobId == AnastasisBuild::JobBuilder) Builder = &N;
	}
	UWorld* PresentationWorld = const_cast<UWorld*>(World);
	const FVector S = FAnastasisVillagePresentation::SimToUnreal(W, Site->X + 0.5, Site->Y + 0.5, PresentationWorld);
	const FVector N = Builder ? FAnastasisVillagePresentation::SimToUnreal(W, Builder->X, Builder->Y, PresentationWorld) : S;
	const AnastasisBuild::FSiteMaterials& M = Site->Materials;
	const AnastasisVillage::FNpc* Owner = Site->Owner.IsEmpty() ? nullptr : V.FindNpc(Site->Owner);
	const AnastasisVillage::FNpc* Courier = V.GetMaterialCourier().IsEmpty() ? nullptr : V.FindNpc(V.GetMaterialCourier());
	return FString::Printf(
		TEXT("{\"site\":true,\"time\":%.4f,\"type\":\"%s\",\"progress\":%.4f,\"pieces\":%d,\"completed\":%s,")
		TEXT("\"id\":\"%s\",\"owner\":\"%s\",\"ownerHome\":\"%s\",\"ownerRests\":%d,")
		TEXT("\"courier\":\"%s\",\"carry\":%d,\"materialsDelivered\":%d,\"sourceIndex\":%d,")
		TEXT("\"needWood\":%d,\"needStone\":%d,\"consumedWood\":%d,\"consumedStone\":%d,\"stockWood\":%d,\"stockStone\":%d,")
		TEXT("\"workers\":%d,\"byNpcs\":%d,\"building\":%d,\"goal\":\"%s\",\"activity\":\"%s\",\"session\":%s,")
		TEXT("\"sx\":%.1f,\"sy\":%.1f,\"sz\":%.1f,\"nx\":%.1f,\"ny\":%.1f,\"nz\":%.1f}"),
		Sim.GetTime(), *Site->Type, Site->Progress, Site->PiecesPlaced, Site->Progress >= 1.0 ? TEXT("true") : TEXT("false"),
		*Site->Id, *Site->Owner, Owner ? *Owner->HomeId : TEXT(""), Owner ? Owner->RestsTaken : 0,
		Courier ? *Courier->Id : TEXT(""), Courier ? Courier->MaterialCarry : 0,
		Courier ? Courier->MaterialsDelivered : 0, Courier ? Courier->MaterialSourceIndex : INDEX_NONE,
		M.NeedWood, M.NeedStone, M.ConsumedWood, M.ConsumedStone, M.StockWood, M.StockStone,
		Site->Workers.Num(), ByNpcs, Building,
		Builder ? *Builder->Goal : TEXT(""), Builder ? *Builder->Activity : TEXT(""),
		Builder && Builder->WorkSession.bActive ? TEXT("true") : TEXT("false"),
		S.X, S.Y, S.Z, N.X, N.Y, N.Z);
}

FString UAnastasisSimulationDebugLibrary::GetSettlementStatus(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	const UAnastasisSimulationSubsystem* Host = World ? World->GetSubsystem<UAnastasisSimulationSubsystem>() : nullptr;
	if (!Host || !Host->GetSimulation().IsRunning()) return TEXT("{}");
	const FAnastasisSimulation& Sim = Host->GetSimulation();
	const AnastasisVillage::FVillage& V = Sim.GetVillage();
	const AnastasisWorld::FWorld& SimWorld = Sim.GetWorld();
	const int32 W = FMath::Max(1, SimWorld.W);
	const int32 Day = Sim.GetDay();
	double Total = 0.0;
	double Peak = 0.0;
	int32 Hot = 0;
	for (const float T : V.GetTraffic())
	{
		Total += T;
		Peak = FMath::Max<double>(Peak, T);
		Hot += T >= AnastasisTraffic::DesireTraffic ? 1 : 0;
	}
	TArray<FString> Roads;
	for (const TPair<int32, AnastasisTraffic::FRoadTile>& R : V.GetRoads())
	{
		const int32 X = R.Key % W;
		const int32 Y = R.Key / W;
		const FVector P = FAnastasisVillagePresentation::SimToUnreal(SimWorld, X + 0.5, Y + 0.5, const_cast<UWorld*>(World));
		Roads.Add(FString::Printf(TEXT("{\"x\":%d,\"y\":%d,\"day\":%d,\"traffic_at_birth\":%.2f,\"traffic\":%.2f,\"cost\":%.4f,\"wx\":%.0f,\"wy\":%.0f,\"wz\":%.0f}"),
			X, Y, R.Value.BuiltDay, R.Value.TrafficAtBirth, V.TrafficAt(X, Y), AnastasisNav::MoveCostAt(V.GetNavGrid(), X, Y), P.X, P.Y, P.Z));
	}
	TArray<FString> Buildings;
	const AnastasisSettlement::FLedger& Ledger = Host->GetVillagePresentation().GetLedger();
	for (const AnastasisVillage::FBuilding& B : V.GetBuildings())
	{
		const AnastasisSettlement::FBiography* Bio = Ledger.Find(B.Id);
		const AAnastasisVillageBuilding* Actor = Host->GetVillagePresentation().FindActor(B.Id);
		const FVector L = Actor ? Actor->GetActorLocation() : FVector::ZeroVector;
		FString Mesh;
		if (Actor && Actor->GetBody() && Actor->GetBody()->GetStaticMesh()) Mesh = Actor->GetBody()->GetStaticMesh()->GetName();
		Buildings.Add(FString::Printf(
			TEXT("{\"id\":\"%s\",\"type\":\"%s\",\"progress\":%.2f,\"program\":\"%s\",\"fixed\":%s,\"founded\":%d,\"founder\":\"%s\",\"job\":\"%s\",")
			TEXT("\"household\":%d,\"owner\":\"%s\",\"occupants\":%d,\"peak\":%d,\"crowded_days\":%d,\"owner_changes\":%d,\"age\":%d,\"events\":%d,")
			TEXT("\"mesh\":\"%s\",\"variant\":\"%s\",\"weathering\":%.3f,\"neglect\":%.3f,\"pad\":%.1f,\"x\":%.0f,\"y\":%.0f,\"z\":%.0f,\"yaw\":%.1f,\"cause\":\"%s\"}"),
			*B.Id, *B.Type, B.Progress, Bio ? AnastasisArchitecture::Get(Bio->Program).Id : TEXT(""), Bio && Bio->bProgramFixed ? TEXT("true") : TEXT("false"),
			Bio ? Bio->FoundedDay : -1, Bio ? *Bio->Founder : TEXT(""), Bio ? *Bio->FounderJob : TEXT(""), Bio ? Bio->FounderHousehold : 0,
			*B.Owner, Bio ? Bio->Occupants : 0, Bio ? Bio->PeakOccupants : 0, Bio ? Bio->CrowdedDays : 0, Bio ? Bio->OwnerChanges : 0,
			Bio ? Bio->AgeDays(Day) : 0, Bio ? Bio->Events.Num() : 0, *Mesh,
			Actor && Actor->HasArchitecture() ? AnastasisArchitecture::Get(Actor->GetVariant()).Id : TEXT(""),
			Actor ? Actor->GetWeathering() : -1.0, Actor ? Actor->GetNeglect() : -1.0, Actor ? Actor->GetPadOffset() : 0.0,
			L.X, L.Y, L.Z, Actor ? Actor->GetActorRotation().Yaw : 0.0, Bio ? *Bio->ProgramCause.Replace(TEXT("\""), TEXT("'")) : TEXT("")));
	}
	const FAnastasisSettlementPaths& Paths = Host->GetVillagePresentation().GetPaths();
	return FString::Printf(
		TEXT("{\"day\":%d,\"time\":%.2f,\"evolution\":%s,\"passages\":%lld,\"traffic_total\":%.1f,\"traffic_peak\":%.1f,\"hot_cells\":%d,\"efforts\":%d,")
		TEXT("\"segments\":%d,\"grass_hidden\":%d,\"roads\":[%s],\"buildings\":[%s]}"),
		Day, Sim.GetTime(), V.IsRoadEvolutionEnabled() ? TEXT("true") : TEXT("false"), V.GetPassageCount(), Total, Peak, Hot, V.GetRoadEfforts().Num(),
		Paths.NumSegments(), Paths.NumHiddenGrass(), *FString::Join(Roads, TEXT(",")), *FString::Join(Buildings, TEXT(",")));
}

FString UAnastasisSimulationDebugLibrary::GetBuildingCapacityStatus(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	const UAnastasisSimulationSubsystem* Host = World ? World->GetSubsystem<UAnastasisSimulationSubsystem>() : nullptr;
	if (!Host || !Host->GetSimulation().IsRunning()) return TEXT("{}");
	const FAnastasisSimulation& Sim = Host->GetSimulation();
	return FAnastasisBuildingCapacitySnapshot::Capture(Sim.GetVillage(), Sim.GetTime()).ToJson();
}

FString UAnastasisSimulationDebugLibrary::GetVillagerCards(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	const UAnastasisSimulationSubsystem* Host = World ? World->GetSubsystem<UAnastasisSimulationSubsystem>() : nullptr;
	if (!Host || !Host->GetSimulation().IsRunning()) return TEXT("{}");
	const AnastasisVillage::FVillage& V = Host->GetSimulation().GetVillage();
	TArray<FString> Rows;
	for (const AnastasisVillage::FNpc& N : V.GetActors())
	{
		const AAnastasisVillagerVisual* Card = Host->GetVillagePresentation().FindVillager(N.Id);
		const FVector P = Card ? Card->GetActorLocation() : FVector::ZeroVector;
		Rows.Add(FString::Printf(
			TEXT("{\"npc\":\"%s\",\"job\":\"%s\",\"look\":\"%s\",\"x\":%.1f,\"y\":%.1f,\"z\":%.1f,\"hidden\":%s,\"mirrored\":%s,\"inside\":%s,")
			TEXT("\"has_body\":%s,\"body\":%s,\"speed\":%.1f,\"heading\":%.1f}"),
			*N.Id, *N.JobId, Card ? *Card->GetLookId().ToString() : TEXT(""), P.X, P.Y, P.Z,
			Card && Card->IsHidden() ? TEXT("true") : TEXT("false"),
			Card && Card->IsMirrored() ? TEXT("true") : TEXT("false"),
			N.Inside.bActive ? TEXT("true") : TEXT("false"),
			// VILLAGER_BODY_3D_001 : corps 3D monte, montre cette frame, vitesse (cm/s) et cap de son animation.
			Card && Card->HasBody() ? TEXT("true") : TEXT("false"),
			Card && Card->IsShowingBody() ? TEXT("true") : TEXT("false"),
			Card ? Card->GetBodySpeed() : 0.0f,
			Card ? Card->GetBodyHeading() : 0.0f));
	}
	return FString::Printf(TEXT("{\"npcs\":%d,\"cards\":%d,\"villagers\":[%s]}"),
		V.GetActors().Num(), Host->GetVillagePresentation().NumVillagers(), *FString::Join(Rows, TEXT(",")));
}

FString UAnastasisSimulationDebugLibrary::GetOpeningLifeStatus(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	const UAnastasisSimulationSubsystem* Host = World ? World->GetSubsystem<UAnastasisSimulationSubsystem>() : nullptr;
	if (!Host || !Host->GetSimulation().IsRunning() || Host->GetSimulation().GetVillage().GetActors().IsEmpty()) return TEXT("{}");
	const FAnastasisSimulation& Sim = Host->GetSimulation();
	const AnastasisVillage::FNpc& N = Sim.GetVillage().GetActors()[0];
	int32 Farmers = 0;
	int32 OtherFarmerDeliveries = 0;
	for (const AnastasisVillage::FNpc& Actor : Sim.GetVillage().GetActors())
	{
		if (Actor.JobId != AnastasisGather::JobFarmer) continue;
		++Farmers;
		if (Actor.Id != N.Id) OtherFarmerDeliveries += Actor.Deliveries;
	}
	return FString::Printf(
		TEXT("{\"time\":%.3f,\"id\":\"%s\",\"home\":\"%s\",\"work\":\"%s\",\"goal\":\"%s\",\"activity\":\"%s\",")
		TEXT("\"inside\":\"%s\",\"claimed\":%s,\"x\":%.3f,\"y\":%.3f,\"hunger\":%.2f,\"thirst\":%.2f,\"energy\":%.2f,")
		TEXT("\"drinks\":%d,\"rests\":%d,\"deliveries\":%d,\"farmers\":%d,\"otherFarmerDeliveries\":%d,\"failed_path\":%s}"),
		Sim.GetTime(), *N.Id, *N.HomeId, *N.WorkplaceId, *N.Goal, *N.Activity, *N.Inside.BuildingId,
		Host->GetVillagePresentation().HasInteractionClaim(N.Id) ? TEXT("true") : TEXT("false"),
		N.X, N.Y, N.Needs.Hunger, N.Needs.Thirst, N.Needs.Energy,
		N.DrinksTaken, N.RestsTaken, N.Deliveries, Farmers, OtherFarmerDeliveries, N.bPathFailed ? TEXT("true") : TEXT("false"));
}

FString UAnastasisSimulationDebugLibrary::GetTimeWarpStatus(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	const UAnastasisSimulationSubsystem* Host = World ? World->GetSubsystem<UAnastasisSimulationSubsystem>() : nullptr;
	if (!Host || !Host->GetSimulation().IsRunning()) return TEXT("{}");
	const FAnastasisSimulation& Sim = Host->GetSimulation();
	const AnastasisTimeWarp::FWitness& W = Host->GetWitness();
	return FString::Printf(
		TEXT("{\"time\":%.4f,\"day\":%d,\"warp\":%g,\"speed\":%g,\"timeScale\":%g,\"rate\":%.4f,\"budgetCut\":%s,\"presence\":%.4f,\"idleDays\":%.4f}"),
		Sim.GetTime(), Sim.GetDay(),
		CVarSimWarp.GetValueOnGameThread(), CVarSimSpeed.GetValueOnGameThread(), CVarSimTimeScale.GetValueOnGameThread(),
		Host->GetEffectiveRate(), Host->WasWarpBudgetCut() ? TEXT("true") : TEXT("false"),
		W.Presence, W.IdleDays(FAnastasisSimulation::DayLength));
}

FString UAnastasisSimulationDebugLibrary::GetSettlementSiteStatus(const UObject* WorldContextObject)
{
    const UWorld* W = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
    const auto* Host = W ? W->GetSubsystem<UAnastasisSimulationSubsystem>() : nullptr;
    return Host ? Host->GetSettlementSiteReport() : TEXT("{}");
}

FVector UAnastasisSimulationDebugLibrary::GetSettlementGroundPoint(const UObject* WorldContextObject, double SimX, double SimY)
{
    UWorld* W = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
    const auto* Host = W ? W->GetSubsystem<UAnastasisSimulationSubsystem>() : nullptr;
    return Host ? FAnastasisVillagePresentation::SimToUnreal(Host->GetSimulation().GetWorld(), SimX, SimY, W) : FVector::ZeroVector;
}
