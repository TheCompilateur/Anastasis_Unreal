// GENERE AUTOMATIQUEMENT - ne pas editer a la main.
// Source: tools/migration/gen-planner-catalog.mjs (reference anastasis-ref-p3)
// Inclus par AnastasisPlanner.cpp, dans namespace AnastasisPlanner::Catalog.

// clang-format off

static const TCHAR* const MarketResources[] = { TEXT("wood"), TEXT("stone"), TEXT("food"), TEXT("tools"), TEXT("wool"), TEXT("milk"), TEXT("leather"), TEXT("meat"), TEXT("eggs"), TEXT("cloth"), TEXT("planks") };
static const FNamedValue MarketCap[] = { { TEXT("wood"), 220.0 }, { TEXT("stone"), 220.0 }, { TEXT("food"), 320.0 }, { TEXT("tools"), 120.0 }, { TEXT("wool"), 120.0 }, { TEXT("milk"), 90.0 }, { TEXT("leather"), 80.0 }, { TEXT("meat"), 160.0 }, { TEXT("eggs"), 120.0 }, { TEXT("cloth"), 80.0 }, { TEXT("planks"), 160.0 } };

static const FNamedValue BuildingCosts[] = {
	{ TEXT("wood"), 36.0 },
	{ TEXT("stone"), 18.0 },
	{ TEXT("wood"), 24.0 },
	{ TEXT("stone"), 8.0 },
	{ TEXT("wood"), 42.0 },
	{ TEXT("stone"), 18.0 },
	{ TEXT("wood"), 48.0 },
	{ TEXT("stone"), 36.0 },
	{ TEXT("wood"), 16.0 },
	{ TEXT("stone"), 4.0 },
	{ TEXT("wood"), 22.0 },
	{ TEXT("stone"), 6.0 },
	{ TEXT("wood"), 34.0 },
	{ TEXT("stone"), 18.0 },
	{ TEXT("wood"), 26.0 },
	{ TEXT("stone"), 16.0 },
	{ TEXT("wood"), 22.0 },
	{ TEXT("stone"), 8.0 },
	{ TEXT("wood"), 34.0 },
	{ TEXT("stone"), 18.0 },
	{ TEXT("wood"), 20.0 },
	{ TEXT("stone"), 10.0 },
	{ TEXT("wood"), 14.0 },
	{ TEXT("stone"), 4.0 },
	{ TEXT("wood"), 26.0 },
	{ TEXT("stone"), 20.0 },
	{ TEXT("wood"), 24.0 },
	{ TEXT("stone"), 18.0 },
	{ TEXT("wood"), 22.0 },
	{ TEXT("stone"), 14.0 },
	{ TEXT("wood"), 10.0 },
	{ TEXT("stone"), 2.0 },
	{ TEXT("wood"), 14.0 },
	{ TEXT("stone"), 8.0 },
	{ TEXT("wood"), 18.0 },
	{ TEXT("stone"), 4.0 },
	{ TEXT("wood"), 32.0 },
	{ TEXT("stone"), 18.0 },
	{ TEXT("wood"), 24.0 },
	{ TEXT("stone"), 34.0 },
	{ TEXT("wood"), 10.0 },
	{ TEXT("stone"), 18.0 },
	{ TEXT("wood"), 40.0 },
	{ TEXT("stone"), 48.0 },
	{ TEXT("wood"), 36.0 },
	{ TEXT("stone"), 64.0 },
	{ TEXT("wood"), 44.0 },
	{ TEXT("stone"), 32.0 },
	{ TEXT("wood"), 28.0 },
	{ TEXT("stone"), 12.0 },
	{ TEXT("wood"), 32.0 },
	{ TEXT("stone"), 16.0 },
	{ TEXT("wood"), 20.0 },
	{ TEXT("stone"), 24.0 },
	{ TEXT("wood"), 18.0 },
	{ TEXT("stone"), 8.0 },
	{ TEXT("wood"), 44.0 },
	{ TEXT("stone"), 42.0 },
	{ TEXT("wood"), 28.0 },
	{ TEXT("stone"), 24.0 },
	{ TEXT("wood"), 38.0 },
	{ TEXT("stone"), 36.0 },
	{ TEXT("wood"), 30.0 },
	{ TEXT("stone"), 22.0 },
	{ TEXT("wood"), 28.0 },
	{ TEXT("stone"), 16.0 },
	{ TEXT("wood"), 20.0 },
	{ TEXT("stone"), 12.0 },
	{ TEXT("wood"), 30.0 },
	{ TEXT("stone"), 18.0 }
};
static const FNamedValue BuildingStorage[] = {
	{ TEXT("food"), 36.0 },
	{ TEXT("wood"), 48.0 },
	{ TEXT("stone"), 32.0 },
	{ TEXT("tools"), 10.0 },
	{ TEXT("planks"), 18.0 },
	{ TEXT("food"), 120.0 },
	{ TEXT("wood"), 120.0 },
	{ TEXT("stone"), 120.0 },
	{ TEXT("tools"), 60.0 }
};
/** Type, housing, security, postes (jobs.length), cout [debut, n], stockage [debut, n], logement (data.housing vrai). */
static const FBuildingSpec Buildings[] = {
	{ TEXT("camp"), 0.0, 0.0, 3, 0, 2, 0, 5, false },
	{ TEXT("house"), 3.0, 0.0, 0, 2, 2, 5, 0, true },
	{ TEXT("dormitory"), 8.0, 0.0, 1, 4, 2, 5, 0, true },
	{ TEXT("manor"), 5.0, 0.0, 1, 6, 2, 5, 0, true },
	{ TEXT("farm"), 0.0, 0.0, 1, 8, 2, 5, 0, false },
	{ TEXT("fishery"), 0.0, 0.0, 1, 10, 2, 5, 0, false },
	{ TEXT("mill"), 0.0, 0.0, 2, 12, 2, 5, 0, false },
	{ TEXT("granary"), 0.0, 0.0, 3, 14, 2, 5, 1, false },
	{ TEXT("sheepfold"), 0.0, 0.0, 2, 16, 2, 6, 0, false },
	{ TEXT("stable"), 0.0, 0.0, 2, 18, 2, 6, 0, false },
	{ TEXT("piggery"), 0.0, 0.0, 2, 20, 2, 6, 0, false },
	{ TEXT("chickencoop"), 0.0, 0.0, 2, 22, 2, 6, 0, false },
	{ TEXT("bakery"), 0.0, 0.0, 1, 24, 2, 6, 0, false },
	{ TEXT("dairy"), 0.0, 0.0, 1, 26, 2, 6, 0, false },
	{ TEXT("butcher"), 0.0, 0.0, 1, 28, 2, 6, 0, false },
	{ TEXT("lumbercamp"), 0.0, 0.0, 1, 30, 2, 6, 0, false },
	{ TEXT("sawmill"), 0.0, 0.0, 1, 32, 2, 6, 0, false },
	{ TEXT("quarry"), 0.0, 0.0, 1, 34, 2, 6, 0, false },
	{ TEXT("tavern"), 0.0, 0.0, 1, 36, 2, 6, 0, false },
	{ TEXT("chapel"), 0.0, 0.0, 1, 38, 2, 6, 0, false },
	{ TEXT("well"), 0.0, 0.0, 1, 40, 2, 6, 0, false },
	{ TEXT("townhall"), 0.0, 0.0, 1, 42, 2, 6, 0, false },
	{ TEXT("temple"), 0.0, 0.0, 1, 44, 2, 6, 0, false },
	{ TEXT("guildhall"), 0.0, 0.0, 1, 46, 2, 6, 0, false },
	{ TEXT("dock"), 0.0, 0.0, 2, 48, 2, 6, 0, false },
	{ TEXT("lodge"), 0.0, 0.0, 1, 50, 2, 6, 0, false },
	{ TEXT("watchtower"), 0.0, 10.0, 1, 52, 2, 6, 0, false },
	{ TEXT("wall"), 0.0, 8.0, 1, 54, 2, 6, 0, false },
	{ TEXT("barracks"), 0.0, 24.0, 1, 56, 2, 6, 0, false },
	{ TEXT("workshop"), 0.0, 0.0, 1, 58, 2, 6, 0, false },
	{ TEXT("forge"), 0.0, 0.0, 1, 60, 2, 6, 0, false },
	{ TEXT("tannery"), 0.0, 0.0, 1, 62, 2, 6, 0, false },
	{ TEXT("weaver"), 0.0, 0.0, 1, 64, 2, 6, 0, false },
	{ TEXT("market"), 0.0, 0.0, 2, 66, 2, 6, 0, false },
	{ TEXT("warehouse"), 0.0, 0.0, 3, 68, 2, 6, 3, false }
};

/** Ressource autorisee et plafond (`profile.cap[res] ?? MARKET.cap[res] ?? 0`). */
static const FNamedValue ProfileAllow[] = {
	{ TEXT("wood"), 200.0 },
	{ TEXT("planks"), 80.0 },
	{ TEXT("wood"), 160.0 },
	{ TEXT("planks"), 120.0 },
	{ TEXT("stone"), 200.0 },
	{ TEXT("food"), 80.0 },
	{ TEXT("food"), 60.0 },
	{ TEXT("food"), 100.0 },
	{ TEXT("food"), 300.0 },
	{ TEXT("wood"), 200.0 },
	{ TEXT("stone"), 200.0 },
	{ TEXT("tools"), 80.0 },
	{ TEXT("planks"), 120.0 },
	{ TEXT("cloth"), 60.0 },
	{ TEXT("leather"), 40.0 },
	{ TEXT("wool"), 60.0 },
	{ TEXT("milk"), 40.0 },
	{ TEXT("meat"), 40.0 },
	{ TEXT("wood"), 220.0 },
	{ TEXT("stone"), 220.0 },
	{ TEXT("food"), 320.0 },
	{ TEXT("tools"), 120.0 },
	{ TEXT("wool"), 120.0 },
	{ TEXT("milk"), 90.0 },
	{ TEXT("leather"), 80.0 },
	{ TEXT("meat"), 160.0 },
	{ TEXT("eggs"), 120.0 },
	{ TEXT("cloth"), 80.0 },
	{ TEXT("planks"), 160.0 },
	{ TEXT("wood"), 80.0 },
	{ TEXT("stone"), 60.0 },
	{ TEXT("food"), 80.0 },
	{ TEXT("tools"), 20.0 },
	{ TEXT("planks"), 40.0 },
	{ TEXT("wood"), 40.0 },
	{ TEXT("tools"), 40.0 },
	{ TEXT("planks"), 24.0 },
	{ TEXT("wood"), 40.0 },
	{ TEXT("tools"), 50.0 },
	{ TEXT("stone"), 20.0 },
	{ TEXT("leather"), 40.0 },
	{ TEXT("cloth"), 30.0 },
	{ TEXT("wool"), 40.0 },
	{ TEXT("cloth"), 40.0 },
	{ TEXT("food"), 60.0 },
	{ TEXT("milk"), 40.0 },
	{ TEXT("food"), 30.0 },
	{ TEXT("meat"), 50.0 },
	{ TEXT("food"), 30.0 },
	{ TEXT("leather"), 20.0 },
	{ TEXT("wool"), 40.0 },
	{ TEXT("food"), 20.0 },
	{ TEXT("milk"), 40.0 },
	{ TEXT("food"), 20.0 },
	{ TEXT("leather"), 20.0 },
	{ TEXT("meat"), 40.0 },
	{ TEXT("food"), 20.0 },
	{ TEXT("eggs"), 40.0 },
	{ TEXT("food"), 20.0 },
	{ TEXT("wood"), 40.0 },
	{ TEXT("stone"), 30.0 },
	{ TEXT("planks"), 24.0 },
	{ TEXT("tools"), 16.0 },
	{ TEXT("wood"), 80.0 },
	{ TEXT("stone"), 60.0 },
	{ TEXT("planks"), 60.0 },
	{ TEXT("tools"), 20.0 }
};
static const FDepotProfile DepotProfiles[] = {
	{ TEXT("lumbercamp"), 0, 2 },
	{ TEXT("sawmill"), 2, 2 },
	{ TEXT("quarry"), 4, 1 },
	{ TEXT("farm"), 5, 1 },
	{ TEXT("fishery"), 6, 1 },
	{ TEXT("mill"), 7, 1 },
	{ TEXT("granary"), 8, 1 },
	{ TEXT("warehouse"), 9, 9 },
	{ TEXT("market"), 18, 11 },
	{ TEXT("camp"), 29, 5 },
	{ TEXT("workshop"), 34, 3 },
	{ TEXT("forge"), 37, 3 },
	{ TEXT("tannery"), 40, 2 },
	{ TEXT("weaver"), 42, 2 },
	{ TEXT("bakery"), 44, 1 },
	{ TEXT("dairy"), 45, 2 },
	{ TEXT("butcher"), 47, 3 },
	{ TEXT("sheepfold"), 50, 2 },
	{ TEXT("stable"), 52, 3 },
	{ TEXT("piggery"), 55, 2 },
	{ TEXT("chickencoop"), 57, 2 },
	{ TEXT("lodge"), 59, 4 }
};
static const FDepotProfile SiteProfile = { TEXT(""), 63, 4 };

static const FConsumeOrder ConsumeOrder[] = {
	{ TEXT("food"), TEXT("granary,mill,bakery,farm,fishery,market,camp,warehouse") },
	{ TEXT("wood"), TEXT("warehouse,lumbercamp,sawmill,lodge,workshop,forge,market,camp") },
	{ TEXT("stone"), TEXT("warehouse,quarry,lodge,forge,market,camp") },
	{ TEXT("tools"), TEXT("warehouse,workshop,forge,lodge,market,camp") },
	{ TEXT("planks"), TEXT("warehouse,sawmill,lumbercamp,lodge,workshop,market,camp") },
	{ TEXT("cloth"), TEXT("warehouse,weaver,tannery,market") },
	{ TEXT("leather"), TEXT("warehouse,tannery,stable,butcher,market") },
	{ TEXT("wool"), TEXT("warehouse,sheepfold,weaver,market") },
	{ TEXT("milk"), TEXT("warehouse,stable,dairy,market") },
	{ TEXT("meat"), TEXT("warehouse,piggery,butcher,market") },
	{ TEXT("eggs"), TEXT("warehouse,chickencoop,market") }
};

static const FNamedValue CharterValues[] = {
	{ TEXT("build"), 12.0 },
	{ TEXT("gatherWood"), 5.0 },
	{ TEXT("house"), 30.0 },
	{ TEXT("dormitory"), 14.0 },
	{ TEXT("sawmill"), 10.0 },
	{ TEXT("builder"), 0.7 },
	{ TEXT("woodcutter"), 0.35 },
	{ TEXT("gatherFood"), 12.0 },
	{ TEXT("craft"), 3.0 },
	{ TEXT("farm"), 28.0 },
	{ TEXT("fishery"), 14.0 },
	{ TEXT("granary"), 12.0 },
	{ TEXT("mill"), 10.0 },
	{ TEXT("farmer"), 0.75 },
	{ TEXT("fisherman"), 0.45 },
	{ TEXT("gatherWood"), 11.0 },
	{ TEXT("build"), 6.0 },
	{ TEXT("house"), 12.0 },
	{ TEXT("lumbercamp"), 16.0 },
	{ TEXT("sawmill"), 14.0 },
	{ TEXT("woodcutter"), 0.8 },
	{ TEXT("builder"), 0.4 },
	{ TEXT("craft"), 12.0 },
	{ TEXT("gatherStone"), 4.0 },
	{ TEXT("forge"), 30.0 },
	{ TEXT("workshop"), 16.0 },
	{ TEXT("quarry"), 10.0 },
	{ TEXT("blacksmith"), 0.8 },
	{ TEXT("artisan"), 0.5 },
	{ TEXT("quarryman"), 0.3 }
};
/** Theme : goalBias [debut, n], buildingBoosts [debut, n], jobBoosts [debut, n] dans CharterValues. */
static const FCharterTheme CharterThemes[] = {
	{ TEXT("house"), 0, 2, 2, 3, 5, 2 },
	{ TEXT("food"), 7, 2, 9, 4, 13, 2 },
	{ TEXT("clear"), 15, 2, 17, 3, 20, 2 },
	{ TEXT("forge"), 22, 2, 24, 3, 27, 3 }
};

/** `jobForId(id).traitBias.gather` (`Number(...) || 0`). */
static const FNamedValue JobTraitBiasGather[] = { { TEXT("settler"), 1.0 }, { TEXT("builder"), 1.05 }, { TEXT("woodcutter"), 1.35 }, { TEXT("quarryman"), 1.48 }, { TEXT("farmer"), 1.25 }, { TEXT("fisherman"), 1.32 }, { TEXT("herder"), 1.18 }, { TEXT("baker"), 1.1 }, { TEXT("cheesemaker"), 1.05 }, { TEXT("butcher"), 1.15 }, { TEXT("merchant"), 0.78 }, { TEXT("artisan"), 0.95 }, { TEXT("blacksmith"), 1.05 }, { TEXT("tanner"), 0.95 }, { TEXT("weaver"), 0.9 }, { TEXT("guard"), 0.85 }, { TEXT("innkeeper"), 0.95 }, { TEXT("priest"), 0.8 }, { TEXT("steward"), 0.95 }, { TEXT("porter"), 1.2 } };

inline constexpr double CollectiveAvgWindow = 3.0;
inline constexpr double CollectiveHistoryDays = 12.0;
inline constexpr double CollectiveHysteresis = 8.0;
inline constexpr double CollectiveStabilityDays = 2.0;
inline constexpr double CollectiveDecayPerDay = 12.0;
inline constexpr double CollectiveRisePerDay = 18.0;
inline constexpr double CollectiveBandLow = 25.0;
inline constexpr double CollectiveBandElevated = 45.0;
inline constexpr double CollectiveBandHigh = 65.0;
inline constexpr double CollectiveBandCritical = 80.0;
inline constexpr double CollectiveImmigrationFloor = 0.35;
inline constexpr double CollectiveImmigrationCeil = 1.15;
inline constexpr double CollectiveJobBoostMax = 2.4;
inline constexpr double CollectiveGoalBiasMax = 22.0;
inline constexpr double CollectiveBuildingPickMin = 15.0;
inline constexpr double CollectiveMaxActiveSites = 2.0;
inline constexpr double CollectiveBootstrapWoodFloor = 165.0;
inline constexpr double CollectiveReadySiteBuildFloor = 165.0;
inline constexpr double CollectiveReadySiteDebtQuantum = 42.0;
inline constexpr double CollectiveSpineSoftMargin = 12.0;
inline constexpr double CollectiveBuilderPerPop = 16.0;
inline constexpr double CollectiveActiveSiteBuilderBonus = 1.0;
inline constexpr double CollectiveActiveSiteBuildBias = 6.0;
inline constexpr double CollectiveActiveSiteHaulBias = 3.0;
inline constexpr double CollectiveCrisisSuspendAt = 70.0;
inline constexpr double CollectiveFoodHoldDays = 8.0;
inline constexpr double CollectiveSiteStallDays = 4.0;
inline constexpr double CollectiveTrafficHot = 8.0;
inline constexpr double CollectiveSecurityPerPop = 0.22;
inline constexpr double CollectiveThreatRadius = 14.0;
inline constexpr double CollectiveSecurityLatentCap = 24.0;
inline constexpr double CollectiveHubPullMissed = 16.0;
inline constexpr double CollectiveHubPullHaulLoad = 4.0;
inline constexpr double CollectiveHubPullTransportCap = 55.0;
inline constexpr double CollectiveDailyFocusMin = 25.0;
inline constexpr double CollectiveDailyFocusFloor = 32.0;
inline constexpr double CollectiveCraftBootstrapToolsBand = 45.0;
inline constexpr double CollectiveToolsVacuumStock = 1.0;
inline constexpr double CollectiveToolsVacuumFocusBoost = 42.0;
inline constexpr double CollectiveCraftIdleToolsStock = 8.0;
inline constexpr double CollectiveCraftIdleFocusBoost = 38.0;
inline constexpr double CollectiveCraftProofCraftFloor = 40.0;
inline constexpr double ReportNearRadius = 11.0;
inline constexpr double ReportFarRadius = 26.0;
inline constexpr double ReportDistanceHalfLife = 12.0;
inline constexpr double ReportFarWeight = 0.12;
inline constexpr double ReportDoubtAfterDays = 3.0;
inline constexpr double ReportRumorChance = 0.14;
inline constexpr double ReportRumorSkewMin = 0.14;
inline constexpr double ReportRumorSkewMax = 0.28;
inline constexpr double ReportClampMin = 0.48;
inline constexpr double ReportClampMax = 1.55;
inline constexpr double CharterMinDays = 2.0;
inline constexpr double CharterMaxDays = 4.0;
inline constexpr double CharterDefaultDays = 3.0;
inline constexpr double CharterReplaceMinInfluence = 1.0;
inline constexpr double CharterRallyMorale = 3.0;
inline constexpr double CharterGrumbleMorale = 1.0;
inline constexpr double CharterGoalBiasCap = 14.0;
inline constexpr double CharterBuildingBoostCap = 32.0;
inline constexpr double CharterJobBoostCap = 0.85;
inline constexpr double CharterBandElevated = 40.0;
inline constexpr double ForestDeepCrown = 0.55;
inline constexpr double ForestMinClearing = 0.02;
inline constexpr double ForestFloorDeep = 10.0;
inline constexpr double ForestFloorStand = 4.0;
inline constexpr double ForestDepleteStumpAmount = 3.0;
inline constexpr double ForestDepleteClearingBoost = 0.22;
inline constexpr double ForestDepleteCrownMul = 0.55;
inline constexpr double ForestWoodCapForest = 44.0;
inline constexpr double ForestWoodCapScrub = 18.0;
inline constexpr double ForestRegenPerDay = 2.0;
inline constexpr double ForestDailyChance = 0.4;
inline constexpr double ForestPromoteScrubAt = 16.0;
inline constexpr double ForestPromoteChance = 0.32;
inline constexpr double ForestPromoteMaxClearing = 0.42;
inline constexpr double ForestDensityOk = 0.28;
inline constexpr double ForestDensityWarn = 0.16;
inline constexpr double ForestDensityScarce = 0.08;
inline constexpr double ForestGatherBiasWarn = -10.0;
inline constexpr double ForestGatherBiasScarce = -24.0;
inline constexpr double ForestGatherBiasCritical = -36.0;
inline constexpr double ForestWoodcutterNeedMulWarn = 0.55;
inline constexpr double ForestWoodcutterNeedMulScarce = 0.28;
inline constexpr double ForestWoodcutterNeedMulCritical = 0.12;
inline constexpr double ForestJobBoostPenaltyWarn = -0.7;
inline constexpr double ForestJobBoostPenaltyScarce = -1.35;
inline constexpr double ForestJobBoostPenaltyCritical = -2.1;
inline constexpr double ForestDayIntentWoodPenaltyWarn = -10.0;
inline constexpr double ForestDayIntentWoodPenaltyScarce = -20.0;
inline constexpr double ForestDayIntentWoodPenaltyCritical = -28.0;
inline constexpr double ForestCareersDemandMulWarn = 0.55;
inline constexpr double ForestCareersDemandMulScarce = 0.3;
inline constexpr double ForestCareersDemandMulCritical = 0.15;
inline constexpr double ColonizationBaseTilesPerDay = 1.0;
inline constexpr double ColonizationMaxTilesPerDay = 2.0;
inline constexpr double ColonizationWindowDays = 4.0;
inline constexpr double ColonizationWindowCap = 8.0;
inline constexpr double ColonizationBandMin = 3.0;
inline constexpr double ColonizationBandMax = 16.0;
inline constexpr double ColonizationMaxCrownDeep = 0.55;
inline constexpr double ColonizationMinClearingHint = 0.02;
inline constexpr double ColonizationPressureHousing = 24.0;
inline constexpr double ColonizationPressureForest = 20.0;
inline constexpr double ColonizationPressureActive = 15.0;
inline constexpr double ColonizationGrassPadBonus = 2.4;
inline constexpr double ColonizationFieldPadBonus = 2.0;
inline constexpr double ColonizationForestHousingPenalty = 1.6;
inline constexpr double ColonizationSiteBlockedNeed = 30.0;
inline constexpr double ColonizationHotPadDays = 2.0;
inline constexpr double ColonizationHotPadHoldDays = 5.0;
inline constexpr double ColonizationHotPadSpotBonus = 5.8;
inline constexpr double ColonizationHotPadBuildBias = 14.0;
inline constexpr double ColonizationBuildOrderBias = 34.0;
inline constexpr double ColonizationLisiereOrderGatherBias = 38.0;
inline constexpr double ColonizationLisiereOrderDeliverBias = 30.0;
inline constexpr double ColonizationFinishAmount = 18.0;
inline constexpr double ColonizationHighPressure = 24.0;
inline constexpr double ColonizationOrderSlots = 2.0;
inline constexpr double ColonizationBuildOrderSlots = 1.0;
inline constexpr double ColonizationExpansionFromClear = 10.0;
inline constexpr double ColonizationExpansionHardCap = 26.0;
inline constexpr double ColonizationExpansionStep = 1.0;
inline constexpr double WorkshopWoodPerDay = 5.0;
inline constexpr double WorkshopToolsPerDay = 3.0;
inline constexpr double WorkshopPerPopulation = 15.0;
inline constexpr double CostGrowthFreePerType = 2.0;
inline constexpr double CostGrowthPerExistingOfType = 0.012;
inline constexpr double CostGrowthMaxMultiplier = 2.2;
inline constexpr double PlankBuildWoodShare = 0.4;
inline constexpr double PlankBuildMinWoodCost = 4.0;
inline constexpr int32 ConstructionPieceTotal = 22;
static const double HousePhaseCapacity[] = { 3.0, 4.0, 5.0, 6.0, 7.0, 8.0 };
static const FNamedValue CollectiveReserveCap[] = { { TEXT("wood"), 80.0 }, { TEXT("stone"), 50.0 }, { TEXT("planks"), 40.0 }, { TEXT("tools"), 20.0 }, { TEXT("food"), 0.0 } };
static const TCHAR* const CollectiveSecondaryTypes[] = { TEXT("tavern"), TEXT("chapel"), TEXT("temple"), TEXT("manor"), TEXT("guildhall"), TEXT("dock"), TEXT("townhall"), TEXT("well"), TEXT("wall"), TEXT("barracks"), TEXT("lodge") };
static const FNamedValue ReportPresumed[] = { { TEXT("wood"), 40.0 }, { TEXT("stone"), 40.0 }, { TEXT("food"), 40.0 }, { TEXT("tools"), 40.0 }, { TEXT("planks"), 20.0 } };
static const TCHAR* const ReportResources[] = { TEXT("wood"), TEXT("stone"), TEXT("food"), TEXT("tools"), TEXT("planks") };
static const FFocusFloor DailyFocusFloor[] = {
	{ TEXT("food"), TEXT("gatherFood"), 32.0 },
	{ TEXT("food"), TEXT("helpFarm"), 22.0 },
	{ TEXT("food"), TEXT("deliver"), 12.0 },
	{ TEXT("food"), TEXT("haulJob"), 12.0 },
	{ TEXT("housing"), TEXT("build"), 32.0 },
	{ TEXT("housing"), TEXT("gatherWood"), 14.0 },
	{ TEXT("housing"), TEXT("deliver"), 10.0 },
	{ TEXT("tools"), TEXT("craft"), 32.0 },
	{ TEXT("tools"), TEXT("fetchInput"), 26.0 },
	{ TEXT("tools"), TEXT("buy"), 16.0 },
	{ TEXT("tools"), TEXT("maintain"), 10.0 },
	{ TEXT("stone"), TEXT("gatherStone"), 32.0 },
	{ TEXT("stone"), TEXT("deliver"), 12.0 },
	{ TEXT("stone"), TEXT("haulJob"), 12.0 }
};

