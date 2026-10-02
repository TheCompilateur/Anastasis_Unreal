// CE QUI EST PORTE, FONCTION PAR FONCTION — declaration lue par l'inventaire.
//
// `Source/AnastasisSim/PORTAGE.md` dit, mission par mission, quelles fonctions JS
// ont un equivalent C++. Les tranches verticales (puits -> chantier) ont porte des
// PARTIES de modules: `npc.js` n'est pas porte parce que `deliver` l'est. Cette
// table recopie les tableaux de PORTAGE.md pour que l'inventaire distingue
// porte / partiel / a porter et compte les lignes qui restent.
//
// Une entree par module JS:
//   cpp        fichier(s) C++ qui portent le module
//   source     section de PORTAGE.md d'ou vient la ligne
//   entier     PORTAGE.md cite le module sans restriction (couches 0-3, ou un
//              fichier nomme sans liste de fonctions). Ce n'est qu'une
//              revendication: le module n'est classe "porte" que si CHAQUE
//              fonction est retrouvee dans le C++ (voir `citation`).
//   fonctions  fonctions que PORTAGE.md nomme comme portees
//   reduites   fonctions portees en partie seulement (une branche, un cas):
//              nommees, mais leurs lignes comptent dans le reste
//   hors       fonctions que PORTAGE.md ecarte du portage (observation,
//              reglages three.js): ni portees, ni a porter
//   alias      nom JS -> nom C++, quand le C++ a renomme
//   citation   (defaut vrai) ajouter aux fonctions portees celles que le C++
//              retrouve. Module `entier` ou sans liste: le C++ NOMME la fonction
//              (nom JS, PascalCase ou alias) sur une ligne qui ne dit pas "non
//              porte". Module avec liste: le C++ a une FONCTION de ce nom
//              (PascalCase suivi d'une parenthese). Faux pour simulation.js et
//              npc.js: leurs noms sont cites partout en commentaire, souvent
//              pour dire ce que la reference fait et que le portage ne fait pas.
//
// Ne pas ajouter une fonction ici parce qu'un test passe: l'ajouter quand
// PORTAGE.md la declare. La table se corrige avec PORTAGE.md, dans le meme
// commit que la mission qui porte.

export const PORTAGE_DECLARE = [
  // --- Couche 0, socle deterministe -----------------------------------------
  { module: "src/sim/rng.js", cpp: "Core/AnastasisRng", source: "couche 0", entier: true },
  { module: "src/sim/util.js", cpp: "Core/AnastasisSimMath", source: "couche 0", entier: true },
  {
    module: "src/sim/spatialGrid.js", cpp: "Core/AnastasisSpatialGrid", source: "couche 0", entier: true,
    alias: { packSpatialCellKey: "PackCellKey", buildActorGrid: "Rebuild", recycleGridCells: "ResetKeepingMemory", acquireBucket: "ResetKeepingMemory" },
  },
  {
    module: "src/runtime/simClock.js", cpp: "Core/AnastasisSimClock", source: "couche 0", entier: true,
    alias: { simStepPlan: "StepPlan", simWallBudgetMs: "WallBudgetMs", simMinStepsBeforeBudget: "MinStepsBeforeBudget", simKeepLagSeconds: "KeepLagSeconds" },
    hors: { simSpeedRenderGate3d: "reglage three.js (PORTAGE.md)", applySimSpeedRenderGate3d: "reglage three.js (PORTAGE.md)" },
  },

  // --- Couche 1, generation du monde -----------------------------------------
  { module: "src/sim/world.js", cpp: "World/AnastasisWorld (+ WorldNoise)", source: "couche 1", entier: true },
  {
    module: "src/sim/worldArchetypes.js", cpp: "World/AnastasisWorldArchetype", source: "couche 1 (knobs sim seuls)",
    fonctions: ["pickWorldArchetypeId", "archetypeJitter"],
    reduites: ["resolveWorldArchetype"],
    alias: { pickWorldArchetypeId: "PickId", resolveWorldArchetype: "Resolve" },
  },
  { module: "src/sim/hydrology.js", cpp: "World/AnastasisHydrology", source: "couche 1", entier: true, alias: { applyHydrology: "Apply", MinHeap: "FMinHeap" } },
  {
    module: "src/sim/fieldCrops.js", cpp: "World/AnastasisWorld, Work/AnastasisFields, Work/AnastasisGather",
    source: "couche 1 ; gather-deliver-001 ; field-regrow-001 ; village-weather-001", entier: true,
    fonctions: ["pickFieldCropId", "fieldSeasonFromDay", "fieldSeasonRegenAmount", "rotateFieldCropId", "ensureFieldCropReady"],
  },

  // --- Couche 2, navigation ----------------------------------------------------
  {
    module: "src/sim/navGrid.js", cpp: "World/AnastasisNavGrid, World/AnastasisNavService",
    source: "couche 2 (couche terrain) ; puits (seuils) ; nav-service-001 (metriques, anneau de trace, cle de cible)",
    fonctions: ["footBlockedAt", "createNavMetrics", "recordNavTransition", "navTraceSnapshot", "navigationTargetKey"],
    reduites: ["ensureNavigation", "syncNavigationFromActor"],
    alias: { createNavMetrics: "FNavMetrics", navTraceSnapshot: "TraceSnapshot" },
  },
  { module: "src/sim/pathfinding.js", cpp: "World/AnastasisPathfinding", source: "couche 2", entier: true, alias: { MinHeap: "FMinHeap", constructor: "FMinHeap" } },
  {
    module: "src/sim/navService.js", cpp: "World/AnastasisNavService", source: "couche 2 (nav-service-001, module seul)", entier: true,
    alias: { createNavService: "FNavService", ensureNavService: "FNavService", clonePath: "FNavCacheEntry", resolveJobActor: "FindLiveAgent" },
  },

  // --- Couche 3, budget (noyau causal) ---------------------------------------
  {
    module: "src/sim/simulationBudget.js", cpp: "Core/AnastasisSimBudget", source: "couche 3 (noyau causal)",
    fonctions: ["simulationPressureTier", "simulationBudgetMultipliers", "classifyNpcSimulationBand", "npcSimulationIntervalForBand", "consumeNpcSimulationCadence", "createSimulationBudgetDirector", "setSimulationPressure", "pinSimulationView", "unpinSimulationView"],
    alias: { simulationPressureTier: "PressureTier", simulationBudgetMultipliers: "Multipliers", classifyNpcSimulationBand: "ClassifyNpcBand", npcSimulationIntervalForBand: "IntervalForBand", consumeNpcSimulationCadence: "ConsumeCadence", createSimulationBudgetDirector: "FDirector", setSimulationPressure: "FDirector", pinSimulationView: "FDirector", unpinSimulationView: "FDirector" },
    hors: { ema: "chronometre, observation seule", noteSimulationBudgetFrame: "chronometre, observation seule", resetSimulationBudgetStats: "chronometre, observation seule", formatSimulationBudgetHud: "HUD" },
  },

  // --- Tranches verticales -----------------------------------------------------
  {
    module: "src/life/needs.js", cpp: "Life/AnastasisNeeds (+ Work/AnastasisGather)",
    source: "puits ; maison ; grenier ; gather-deliver ; social-relax ; needs-factors-001 ; reconsider-001",
    fonctions: ["needsReconsiderChance", "urgeScore", "needGoalScores", "tickNeeds", "tickVitality", "satisfyDrink", "satisfyRest", "satisfyEat", "satisfySocial", "satisfyRelax", "workWillFactor",
      "hydrationLossFactor", "metabolicDemandFactor", "fatigueRecoveryFactor", "fatigueAdaptationFactor", "recoveryConditioningFactor", "needsCritical"],
    alias: { needsCritical: "AreNeedsCritical" },
  },
  {
    module: "src/life/genome.js", cpp: "Life/AnastasisGenome", source: "needs-factors-001",
    fonctions: ["hashString", "deriveGenomeSeed", "createGenome", "mutateAllele", "recombineGenome", "derivePhenotype", "clamp01",
      "hydrationLossMultiplierFromRetention", "heatDissipationEfficiencyFromRetention", "metabolicDemandMultiplierFromEfficiency",
      "metabolicPeakRecoveryMultiplierFromEfficiency", "fatigueRecoveryMultiplierFromRecovery", "fatigueRecoveryStrainCostFromRecovery",
      "ensureGenome", "genomeFingerprint"],
    alias: { clamp01: "GenomeClamp01" },
  },
  { module: "src/life/conditioning.js", cpp: "Life/AnastasisConditioning", source: "needs-factors-001", entier: true },
  {
    module: "src/life/villageRhythm.js", cpp: "Life/AnastasisVillageRhythm, Village/AnastasisVillage",
    source: "maison ; puits ; grenier ; social-relax ; reconsider-001",
    fonctions: ["villagePhase", "isNightPhase", "phaseBias", "nearestWell", "nearestHousing", "mealPlace", "rhythmTarget",
      "personalFrac", "villagePhaseFor", "phaseReconsiderChance", "syncVillagePhase"],
  },
  {
    module: "src/life/workShift.js", cpp: "Life/AnastasisWorkShift, Village/AnastasisVillage", source: "reconsider-001",
    fonctions: ["noteShiftGoalCommit", "noteShiftArrival", "shiftShields", "shiftEntryCommitted"],
    alias: { shiftEntryCommitted: "IsShiftEntryGoal" },
    hors: {
      __setWorkShiftModeForBench: "bascule de banc A/B, aucun code runtime ne l'appelle",
      shiftOf: "accesseur nul-sur : FNpc::WorkShift existe toujours (etat None)",
    },
  },
  {
    module: "src/sim/metiers/extractionPost.js", cpp: "Life/AnastasisWorkShift", source: "reconsider-001",
    fonctions: ["opensExtractionShift", "extractionPostFor", "withinCourt", "extractionResourceOfBuilding"],
    alias: { extractionPostFor: "OpensExtractionShift", withinCourt: "OpensExtractionShift", extractionResourceOfBuilding: "OpensExtractionShift" },
    hors: { __setExtractionPostModeForBench: "bascule de banc A/B, aucun code runtime ne l'appelle" },
  },
  {
    module: "src/life/domestic.js", cpp: "Life/AnastasisNeeds, Village/AnastasisVillage", source: "maison ; social-relax",
    fonctions: ["sleepQuality", "findOpenShelter", "countShelterOccupants", "assignSheltersDaily", "domesticTarget"],
  },
  {
    module: "src/sim/simulation.js", cpp: "Village/AnastasisVillage, Sim/AnastasisSimulation, Work/AnastasisFields, Work/AnastasisBuild, Life/AnastasisWeatherBehavior",
    source: "couche 2 ; puits ; maison ; gather-deliver ; field-regrow ; social-relax ; bonds-rumors ; village-weather ; build-001",
    citation: false,
    fonctions: [
      "blockedAt", "footBlockedAt", "tileTraversalCost", "randomWalkTarget",
      "addBuilding", "countBuildings", "localOccupancy", "accessPointNear", "drinkAccessPoint",
      "enterBuilding", "exitBuilding", "buildingForIndoorAction", "buildingNearActor",
      "resourceTileNear", "depleteTile", "regrowFieldsDaily", "regrowFieldTile", "socialPos",
      "workplaceAcceptsIndoorGoal",
      "countPlannedBuildings", "countBuildings", "activeConstructions", "activeConstruction", "constructionOpenSlots",
      "housingCapacity", "pendingHousingCapacity", "houseCapacity", "housePhase", "completedBuildingEntries", "marketCaps",
      "totalBuildingValue", "costMultiplier", "buildCost", "hasSawCapacity", "siteCanPlacePiece", "marketPos", "plannedMarketPos",
      "workersAtBuilding", "buildingNeedScore",
      "buildCost", "costMultiplier", "siteCanPlacePiece", "consumeSiteMaterials", "constructionAccessPoint", "workConstruction",
    ],
    reduites: ["tick", "moveActor", "nextWaypoint", "enqueueDayDeferred", "assignHomeToHousehold", "movementSpeedFactor"],
  },
  {
    module: "src/sim/npc.js", cpp: "Village/AnastasisVillage, Work/AnastasisGather, Life/AnastasisWeatherBehavior, Ai/AnastasisGoalNoise",
    source: "puits ; maison ; grenier ; gather-deliver ; social-relax ; bonds-rumors ; village-weather ; build-001 ; sim-rng-001 (goalNoise, fonction pure non branchee) ; reconsider-001 ; chat-on-haul-001",
    citation: false,
    fonctions: [
      "goalNoise", "committedReconsiderChance", "addGoalBias", "readCollectiveUrgencySnapshot", "collectiveUrgencyBiasMap",
      "reachedMoveTarget", "updateInside", "tryEnterIndoorAction", "redirectDomesticDoorFailure",
      "completionBias", "traitGoalBias", "jobPriority", "mealPathBlocked", "survivalWorkFactor",
      "shouldHaulGatherLoad", "progressCraftGather", "beginHaulToDepot", "applyGoalEligibility",
      "socialize", "holdTalkAct",
      "shelterRainDuration", "applyRainExposure", "shelterRainAccess", "performShelterRain",
      "progressBuildWork", "pickBuildSite",
    ],
    reduites: ["updateNpc", "act", "perform", "adultScores", "assignTarget", "resourceScore", "deliveryScore", "workplaceGoalBias", "deliver", "commitGoalChoice",
      "maybeChatOnHaul"],
  },
  { module: "src/ai/algorithmic/hungerUtility.js", cpp: "Ai/AnastasisNous", source: "grenier", entier: true },
  { module: "src/ai/algorithmic/inertia.js", cpp: "Ai/AnastasisNous", source: "grenier", entier: true },
  { module: "src/ai/algorithmic/scheduler.js", cpp: "Ai/AnastasisNous", source: "grenier", entier: true },
  { module: "src/ai/algorithmic/decision.js", cpp: "Ai/AnastasisNous", source: "grenier", entier: true },
  { module: "src/ai/algorithmic/hungerAction.js", cpp: "Village/AnastasisVillage", source: "grenier", entier: true },
  { module: "src/ai/algorithmic/runtime.js", cpp: "Village/AnastasisVillage", source: "grenier", entier: true },
  { module: "src/ai/algorithmic/bridge.js", cpp: "Village/AnastasisVillage", source: "grenier", entier: true },
  { module: "src/ai/algorithmic/mealReservation.js", cpp: "Village/AnastasisVillage", source: "grenier (lignes 1-377)" },
  {
    module: "src/sim/transport/stockLedger.js", cpp: "Village/AnastasisVillage, Village/AnastasisPlanner", source: "grenier (reserver / rendre / prelever) ; planner-module-001",
    fonctions: ["reserveStock", "releaseStock", "takeReserved",
      "profileForBuilding", "ensureBuildingStock", "availableStock", "physicalStock", "acceptsResource", "findDepotsForResource"],
  },
  {
    module: "src/sim/collectivePriorities.js", cpp: "Village/AnastasisPlanner", source: "planner-module-001 (module seul)",
    fonctions: ["countWorkers", "farmStaffingGap", "housingDeficit", "stampHouseVacant", "stampHouseOccupied", "countVacantHouses",
      "countHomelessForHome", "housingVacancySnapshot", "collectiveHydrationStress", "collectiveHousingSaturation", "collectiveAccessStress",
      "foodDaysLeft", "knownStock", "measureJobNeeds", "stateDailyFocusTools", "plannedCount", "unfinishedCount", "haulAccessibleStock",
      "readySiteServiceDebt", "hasWoodIndustry", "woodBootstrapNeeded", "woodBootstrapDraft", "isWoodBootstrapDraftee",
      "canCoverSpineSeed", "wantExtraFarmHardGate", "constructionSlotsFull", "countActiveConstructionSites", "exploitSpinePending",
      "bestPendingByScore", "anyPlanned", "villageAmenityPending", "villageCraftPending", "craftBootstrapPending",
      "earlySpineSitesOpen", "toolsVacuum", "completedToolsAtelier", "craftIdle", "villageHerdPending", "livePriorityLevels",
      "scoreBuildingProjects", "boostedBuildingScores", "collectiveBuildingNeedScore", "computeEffects", "applyDailyFocusToEffects",
      "liveEffects", "collectiveGoalBias", "collectiveGoalFloor", "isFoodRush"],
    reduites: ["ensureCollectivePriorities"],
  },
  {
    module: "src/sim/colonyStockReport.js", cpp: "Village/AnastasisPlanner", source: "planner-module-001",
    fonctions: ["hubOf", "trueStockMap", "weightForDistance", "emptyReport", "ensureColonyStockReport", "refreshColonyStockReport",
      "reportedColonyStock", "hubConsolidationPressure", "colonyStockBlindSpots"],
  },
  {
    module: "src/sim/founderCharter.js", cpp: "Village/AnastasisPlanner", source: "planner-module-001",
    fonctions: ["ensureFounderCharter", "liveFounderCharter", "expireFounderCharter", "applyFounderCharterToEffects"],
  },
  {
    module: "src/sim/forestSustain.js", cpp: "Village/AnastasisPlanner", source: "planner-module-001",
    fonctions: ["forestGatherBrake"],
  },
  {
    module: "src/sim/colonizationDoctrine.js", cpp: "Village/AnastasisPlanner", source: "planner-module-001",
    fonctions: ["colonizationBandRange", "frontierForestDensity"],
    reduites: ["ensureColonizationDoctrine"],
  },
  {
    module: "src/ai/memory.js", cpp: "Village/AnastasisVillage, Work/AnastasisGather, World/AnastasisExplore", source: "grenier ; gather-deliver ; bonds-rumors (n° 14) ; perception-explore-001",
    fonctions: ["believedStock", "presumedNoise", "perceive", "rememberSpot", "trimMemory", "forgetEmptied", "recallResource", "forgetStale",
      "markCell", "cellIndex", "exploreTarget", "tellSpots"],
  },
  { module: "src/ai/moralPressure.js", cpp: "Work/AnastasisGather", source: "gather-deliver ; social-relax", entier: true },
  {
    module: "src/sim/craftWork.js", cpp: "Work/AnastasisGather, Work/AnastasisBuild", source: "gather-deliver ; build-001 (profil build)",
    fonctions: ["swingPeriodFor", "yieldPerSwing", "ensureCraftSession"],
  },
  { module: "src/sim/craftFatigue.js", cpp: "Work/AnastasisGather", source: "gather-deliver ; chat-on-haul-001 (missMul)", entier: true },
  {
    module: "src/sim/craftMiss.js", cpp: "Work/AnastasisCraftMiss, Village/AnastasisVillage", source: "chat-on-haul-001",
    fonctions: ["craftMissChance", "craftMissKindFor", "canRollCraftMiss", "rollCraftMiss", "stampCraftMiss", "applyCraftMissRecovery"],
    alias: { craftMissKindFor: "MissKindFor", canRollCraftMiss: "CanRoll", craftMissChance: "MissChance", stampCraftMiss: "RollCraftMiss" },
    hors: { isCraftMissFresh: "fenetre du geste et des eclats (rendu), aucune lecture par la simulation" },
  },
  { module: "src/sim/fieldWorkPosts.js", cpp: "Work/AnastasisGather, Village/AnastasisVillage", source: "gather-deliver", entier: true, fonctions: ["fieldWorkTarget", "claimedFieldPosts"] },
  { module: "src/life/skills.js", cpp: "Work/AnastasisGather", source: "gather-deliver", entier: true },
  { module: "src/life/bonds.js", cpp: "Life/AnastasisBonds", source: "bonds-rumors", entier: true, fonctions: ["bondSocialTarget"] },
  { module: "src/life/talk.js", cpp: "Life/AnastasisBonds, Village/AnastasisVillage", source: "bonds-rumors (hash, portes, durees, tours, refus)", fonctions: ["recordTalk", "beginTalkSession", "advanceTalkTurn"] },
  { module: "src/ai/socialMemory.js", cpp: "Life/AnastasisBonds", source: "bonds-rumors", entier: true, fonctions: ["rememberedSocialTarget"] },
  {
    module: "src/sim/lifestyle.js", cpp: "Life/AnastasisLifestyle", source: "lifestyle-001 (module seul)", entier: true,
    alias: { lifestyleTarget: "LifestyleTargetBuilding" },
    hors: {
      lifestyleLabel: "presentation : `${life.label}: ${life.short}`, texte affiche (PORTAGE.md, lifestyle-001)",
      lifestyleColor: "presentation : couleur du marqueur 3D (PORTAGE.md, lifestyle-001)",
    },
  },
  { module: "src/life/moodlets.js", cpp: "Life/AnastasisBonds", source: "bonds-rumors (newFriend)" },
  { module: "src/life/speechActs.js", cpp: "Village/AnastasisVillage", source: "bonds-rumors", fonctions: ["createInformResourceSpotActs", "commitHearsayResourceSpot"] },
  {
    module: "src/sim/weather.js", cpp: "World/AnastasisWeather", source: "env-realism-001",
    fonctions: ["weatherAt", "sampleCoverFront", "coverLobeAt", "winterSnowAt", "winterFrostAt", "weatherWetnessAt", "weatherHumidityAt"],
  },
  {
    module: "src/ai/weatherGoalBias.js", cpp: "Life/AnastasisWeatherBehavior", source: "village-weather-001",
    fonctions: ["readSimWeather", "weatherGoalBiasFromState", "shouldSeekRainShelter", "shelterRainScore"],
  },
  { module: "src/sim/constructionPieces.js", cpp: "Work/AnastasisBuild", source: "build-001", entier: true },

  // --- Vague 4, etat et sauvegarde: le lecteur du harnais -------------------
  // Le format JS est LU (harnais), jamais ecrit par le jeu (P2_MODELE_DONNEES.md).
  {
    module: "src/sim/save.js", cpp: "Harness/AnastasisJsSave", source: "sim-state-reader-001 (lecteur du harnais)",
    citation: false,
    fonctions: ["tileDiff"],
    // serialize / deserialize / applyTileDiff / unpackActor : sections du perimetre
    // du scenario seulement, lignes de tileDiff au format courant seulement.
    reduites: ["serialize", "deserialize", "applyTileDiff", "unpackActor"],
  },
  {
    module: "src/sim/pristineWorld.js", cpp: "Harness/AnastasisJsSave (GenerateWorld)", source: "sim-state-reader-001",
    fonctions: ["pristineReference", "storePristineRef"],
  },
  { module: "src/sim/craftToolSwitch.js", cpp: "Work/AnastasisBuild, Village/AnastasisVillage", source: "build-001", entier: true },
];

// Mentions de PORTAGE.md qui ne se comptent pas en fonctions: des donnees, ou
// une fonction d'un module classe ailleurs. Le rapport les liste pour ne pas
// les perdre.
export const PORTAGE_HORS_COMPTE = [
  ["src/sim/content.js", "table TRAITS (donnee) — gather-deliver"],
  ["src/sim/metiers/catalog.js", "metiers farmer / settler (donnee, module a generer) — gather-deliver ; tables metiers — build-001"],
  ["src/runtime/gameLoop.js", "frameDeltaSeconds — couche 0 ; le module est jete (requestAnimationFrame)"],
];
