// Le planificateur collectif — mission planner-module-001.
//
// Reference `src/sim/collectivePriorities.js` (tag anastasis-ref-p3), avec ce qu'il lit :
// `colonyStockReport.js` (rapport de stock, ses angles morts, sa consolidation, son
// rafraichissement paresseux et ses tirages `sim.rng`), `founderCharter.js` (effets de la charte,
// expiration), `forestSustain.js` / `colonizationDoctrine.js` (frein forestier), `stockLedger.js`
// (stock des batiments), les methodes de `Simulation` lues (capacite de logement, plafonds du
// marche, devis, creneaux de chantier, pose de piece…) et, dans `npc.js`,
// `collectiveUrgencyBiasMap` / `readCollectiveUrgencySnapshot`.
//
// Module SEUL : il ne lit ni n'ecrit FVillage. Il travaille sur une VUE explicite du village
// (`FPlannerVillage`), que l'hote construit depuis son etat, et dont il recopie les ECRITURES
// apres l'appel (voir `FPlannerWrites` et la fiche docs/unreal/handoffs/planner-module-001.md).
// La sortie par habitant est `FPlannerDecision` (les champs de `FCollectiveDecision`, plus le
// besoin de chantier `buildingNeedScore`).
//
// Frontiere : `findBuildSpot` (l'urbanisme) est une fonction fournie par l'hote. Le hasard passe
// par le flux partage de l'hote (`Rng`).

#pragma once

#include "CoreMinimal.h"

struct FAnastasisRng;

namespace AnastasisPlanner
{
	/** Un objet JS a cles chaines, dans l'ordre d'insertion (`Object.entries`). */
	struct ANASTASISSIM_API FOrderedMap
	{
		TArray<TPair<FString, double>> Items;

		const double* Find(const FString& Key) const;
		double* FindMutable(const FString& Key);
		/** `map[key] || 0`. */
		double Get(const FString& Key) const;
		bool Has(const FString& Key) const { return Find(Key) != nullptr; }
		/** `map[key] = value` : remplace en place, sinon ajoute en fin. */
		void Set(const FString& Key, double Value);
		/** `map[key] = (map[key] || 0) + delta`. */
		void Add(const FString& Key, double Delta);
	};

	/** `building.stock[res] = { physical, reserved }`. */
	struct FStockSlot
	{
		int32 Physical = 0;
		int32 Reserved = 0;
	};

	/** Un batiment, tel que le planificateur le lit (et l'ecrit : `vacantSinceDay`, `stock`). */
	struct ANASTASISSIM_API FPlannerBuilding
	{
		FString Id;
		FString Type;
		/** `building.progress` ; vide = `undefined` (`?? 1` le lit 1, `< 1` et `>= 1` le lisent faux). */
		TOptional<double> Progress;
		double X = 0.0;
		double Y = 0.0;
		/** `building.owner` ; vide = sans proprietaire. */
		FString Owner;
		/** `building.vacantSinceDay` ; vide = `null` / absent. ECRIT par le planificateur. */
		TOptional<double> VacantSinceDay;
		TOptional<double> CreatedDay;
		TOptional<double> HousePhase;
		/** `building.materialsNeeded` present (un chantier) ; ses entrees, dans l'ordre. */
		bool bHasMaterialsNeeded = false;
		FOrderedMap MaterialsNeeded;
		FOrderedMap MaterialsConsumed;
		int32 PiecesPlaced = 0;
		/** `building.stock` present ; ses cases, dans l'ordre. ECRIT par `ensureBuildingStock`. */
		bool bHasStock = false;
		TArray<TPair<FString, FStockSlot>> Stock;

		const FStockSlot* SlotOf(const FString& Resource) const;
	};

	/** Un habitant, tel que le planificateur le lit. */
	struct FPlannerActor
	{
		FString Id;
		FString LifeStage;
		FString JobId;
		/** `npc.alive !== false`. */
		bool bAlive = true;
		/** `npc.home`, `npc.shelter`, `npc.workplace.id` ; vide = absent. */
		FString HomeId;
		FString ShelterId;
		FString WorkplaceId;
		/** `Number(npc.trait?.gather) || 0`. */
		double TraitGather = 0.0;
		/** `npc.inventory.wood | 0`. */
		int32 InventoryWood = 0;
	};

	/** Une tuile, pour la densite de foret de la lisiere (`sim.tileAt`). */
	struct FPlannerTile
	{
		FString Type;
		FString Resource;
		double Amount = 0.0;
	};

	/** `colony.priorities.jobs[jobId]`. */
	struct FJobNeed
	{
		FString JobId;
		double Current = 0.0;
		double Needed = 0.0;
		double Need = 0.0;
		double Surplus = 0.0;
	};

	/** `colony.priorities.dailyFocus`. */
	struct FDailyFocus
	{
		FString Id;
		/** `focus.forced` (ex. « foundingFood ») ; vide si absent. */
		FString Forced;
	};

	/** Les effets collectifs du jour (`computeEffects` + charte + focus), memoises dans `_effects`. */
	struct ANASTASISSIM_API FCollectiveEffects
	{
		FOrderedMap JobBoosts;
		FOrderedMap BuildingBoosts;
		FOrderedMap Reserve;
		bool bSuspendSecondary = false;
		double ImmigrationMul = 1.0;
		bool bFoodRush = false;
		bool bFoodHold = false;
		FOrderedMap GoalBias;
		FOrderedMap GoalFloor;
		FString DailyFocus;
		bool bHubPull = false;
		FString CraftBootstrap;
		bool bCraftProof = false;
		bool bReadySiteStalled = false;
		double ReadySiteDebt = 0.0;
		FString CharterThemeId;
	};

	/** `colony.priorities` : ce que le planificateur lit et ce qu'il memoise. */
	struct ANASTASISSIM_API FCollectivePriorities
	{
		/** `priorities[type].level`, pour food, housing, tools, labor, transport, security. */
		FOrderedMap Levels;
		/** `jobs`, dans l'ordre (les neuf metiers suivis d'abord). */
		TArray<FJobNeed> Jobs;
		/** `buildingScores` STOCKES (la passe quotidienne) : lus par `liveEffects`. */
		FOrderedMap BuildingScores;
		TOptional<FDailyFocus> DailyFocus;
		/** `siteWatch.sites[id].stalledSinceDay` ; absent ou non fini = pas de dette. */
		TMap<FString, double> SiteStalledSinceDay;

		/** `_effects` : le cache de `liveEffects`. ECRIT. Vide = a calculer. */
		TOptional<FCollectiveEffects> Effects;
		/** `_woodDraftDay` / `_woodDraft` : le cache de la corvee de bois. ECRIT. */
		TOptional<int32> WoodDraftDay;
		TOptional<TArray<FString>> WoodDraft;
	};

	/** `colony.stockReport.rumor`. */
	struct FStockRumor
	{
		FString Resource;
		double Mul = 1.0;
		double UntilDay = 0.0;
		FString Cause;
	};

	/** `colony.stockReport.blind[i]`. */
	struct FStockBlind
	{
		FString Resource;
		double Missed = 0.0;
		FString BuildingType;
		FString BuildingId;
		double Dist = 0.0;
	};

	/** `colony.stockReport`. ECRIT par le rafraichissement paresseux. */
	struct FStockReport
	{
		bool bPresent = false;
		double Day = 0.0;
		double LastRefreshDay = -1.0;
		FOrderedMap Stock;
		TOptional<FStockRumor> Rumor;
		TArray<FStockBlind> Blind;
		double CertifiedNear = 0.0;
		double IgnoredFar = 0.0;
	};

	/** `colony.charter`. ECRIT (expiration : vide). */
	struct FCharterState
	{
		FString ThemeId;
		double UntilDay = 0.0;
	};

	/** `sim.colony`. */
	struct FColonyState
	{
		/** `colony.morale ?? 50` : vide = 50. */
		TOptional<double> Morale;
		/** `colony.doctrine.hotPads.length`, `colony.doctrine.expansionBonus`. */
		int32 DoctrineHotPads = 0;
		double DoctrineExpansionBonus = 0.0;
		FCollectivePriorities Priorities;
		FStockReport StockReport;
		TOptional<FCharterState> Charter;
	};

	/** `readCollectiveUrgencySnapshot(sim)` (npc.js l. 1607). */
	struct FUrgencySnapshot
	{
		bool bHydrationActive = false;
		double HydrationLevel = 0.0;
		double HydrationPlannedGap = 0.0;
		bool bHousingActive = false;
		double HousingLevel = 0.0;
		double HousingDeficit = 0.0;
		bool bHousingVacancyActive = false;
		bool bAccessActive = false;
		double AccessLevel = 0.0;
		FString AccessResource;
	};

	/** La VUE du village que le planificateur lit et ecrit. */
	struct ANASTASISSIM_API FPlannerVillage
	{
		/** `sim.day`, `sim.time`, `sim.w`, `sim.h`. */
		double Day = 1.0;
		double Time = 0.0;
		int32 W = 0;
		int32 H = 0;
		/** `sim.settlement` ; `clearRadius`, `marketDx` / `marketDy` vides = absents. */
		bool bHasSettlement = true;
		double SettlementX = 0.0;
		double SettlementY = 0.0;
		TOptional<double> SettlementClearRadius;
		TOptional<double> MarketDx;
		TOptional<double> MarketDy;
		/** `sim._marketPos` (pose quand un marche est bati) ; vide = `plannedMarketPos()`. */
		TOptional<FVector2D> MarketPosCache;

		TArray<FPlannerBuilding> Buildings;
		TArray<FPlannerActor> Actors;
		/** `sim.colony` ; faux = pas de colonie (les lectures rendent 0 / faux). */
		bool bHasColony = true;
		FColonyState Colony;
		/** `sim.market.stock`. */
		FOrderedMap MarketStock;
		/** `sim.archetype.scarceSeed.wood / .stone`. */
		double ScarceSeedWood = 0.0;
		double ScarceSeedStone = 0.0;

		/** `sim.tileAt(x, y)` ; rend faux hors carte. */
		TFunction<bool(int32 X, int32 Y, FPlannerTile& Out)> TileAt;
		/** `sim.findBuildSpot(type)` (frontiere urbaine) : vrai s'il existe un emplacement. Vide = absent. */
		TFunction<bool(const FString& Type)> FindBuildSpot;
		/** `sim.rng` : le flux partage. Seul le rafraichissement paresseux du rapport de stock tire. */
		FAnastasisRng* Rng = nullptr;

		/** `sim._npcCollectiveUrgency` : le cache de l'urgence, par seconde de jeu. ECRIT. */
		FString UrgencyBucket;
		TOptional<FUrgencySnapshot> UrgencyCache;

		int32 IndexOfActor(const FString& Id) const;
	};

	/** Ce que le planificateur dit a la decision d'UN habitant (`FCollectiveDecision` + le besoin de chantier). */
	struct FPlannerDecision
	{
		/** `collectiveGoalBias(sim, goal)` pour chaque but de `GOAL_BIAS_SOFT_KEY`. */
		TMap<FString, double> GoalBias;
		/** `collectiveGoalFloor(sim, goal)` : les planchers poses (les autres valent 0). */
		TMap<FString, double> GoalFloor;
		/** `collectiveUrgencyBiasMap(sim, npc)`. */
		TMap<FString, double> UrgencyBias;
		bool bWoodBootstrapDraftee = false;
		bool bFoodRush = false;
		int32 FarmStaffingGap = 0;
		/** `sim.buildingNeedScore()` = `collectiveBuildingNeedScore(sim)`. */
		double BuildingNeedScore = 0.0;
	};

	/** Les buts de `GOAL_BIAS_SOFT_KEY`, dans l'ordre de la reference. */
	ANASTASISSIM_API const TArray<FString>& GoalBiasGoals();

	// --- Simulation (methodes lues) ---------------------------------------------------------------
	ANASTASISSIM_API int32 CountPlannedBuildings(const FPlannerVillage& V, const FString& Type);
	ANASTASISSIM_API int32 CountBuildings(const FPlannerVillage& V, const FString& Type);
	ANASTASISSIM_API int32 ActiveConstructionCount(const FPlannerVillage& V, const FString& Type = FString());
	ANASTASISSIM_API int32 ConstructionOpenSlots(const FPlannerVillage& V);
	ANASTASISSIM_API double HousingCapacity(const FPlannerVillage& V);
	ANASTASISSIM_API double PendingHousingCapacity(const FPlannerVillage& V);
	ANASTASISSIM_API FOrderedMap MarketCaps(const FPlannerVillage& V);
	ANASTASISSIM_API double TotalBuildingValueSecurity(const FPlannerVillage& V);
	ANASTASISSIM_API FOrderedMap BuildCost(const FPlannerVillage& V, const FString& Type);
	/** `siteCanPlacePiece(building)` : ECRIT le stock du chantier (`ensureBuildingStock`). */
	ANASTASISSIM_API bool SiteCanPlacePiece(FPlannerBuilding& Building);
	ANASTASISSIM_API FVector2D MarketPos(const FPlannerVillage& V);

	// --- Stock des batiments (stockLedger.js) -------------------------------------------------------
	ANASTASISSIM_API void EnsureBuildingStock(FPlannerBuilding& Building);
	ANASTASISSIM_API int32 PhysicalStock(const FPlannerBuilding& Building, const FString& Resource);
	ANASTASISSIM_API int32 AvailableStock(const FPlannerBuilding& Building, const FString& Resource);
	ANASTASISSIM_API bool AcceptsResource(const FPlannerBuilding& Building, const FString& Resource);

	// --- Rapport de stock (colonyStockReport.js) ----------------------------------------------------
	/** `reportedColonyStock(sim)` : rafraichit (et TIRE) si le rapport n'est pas du jour. */
	ANASTASISSIM_API FOrderedMap ReportedColonyStock(FPlannerVillage& V);
	/** `refreshColonyStockReport(sim)`. */
	ANASTASISSIM_API void RefreshColonyStockReport(FPlannerVillage& V);

	// --- Frein forestier -----------------------------------------------------------------------------
	ANASTASISSIM_API double FrontierForestDensity(const FPlannerVillage& V);

	// --- Planificateur -------------------------------------------------------------------------------
	ANASTASISSIM_API int32 FarmStaffingGap(const FPlannerVillage& V);
	ANASTASISSIM_API TArray<FJobNeed> MeasureJobNeeds(FPlannerVillage& V);
	ANASTASISSIM_API FOrderedMap ScoreBuildingProjects(FPlannerVillage& V);
	ANASTASISSIM_API FOrderedMap BoostedBuildingScores(FPlannerVillage& V);
	ANASTASISSIM_API double CollectiveBuildingNeedScore(FPlannerVillage& V);
	ANASTASISSIM_API FString ExploitSpinePending(FPlannerVillage& V);
	ANASTASISSIM_API FString VillageAmenityPending(FPlannerVillage& V);
	ANASTASISSIM_API FString VillageCraftPending(FPlannerVillage& V);
	ANASTASISSIM_API FString VillageHerdPending(FPlannerVillage& V);
	ANASTASISSIM_API FString CraftBootstrapPending(FPlannerVillage& V);
	/** `liveEffects(sim)` : memoise dans `Colony.Priorities.Effects`. */
	ANASTASISSIM_API const FCollectiveEffects& LiveEffects(FPlannerVillage& V);
	ANASTASISSIM_API double CollectiveGoalBias(FPlannerVillage& V, const FString& Goal);
	ANASTASISSIM_API double CollectiveGoalFloor(FPlannerVillage& V, const FString& Goal);
	ANASTASISSIM_API bool IsFoodRush(FPlannerVillage& V);
	ANASTASISSIM_API bool IsWoodBootstrapDraftee(FPlannerVillage& V, const FString& ActorId);
	/** `readCollectiveUrgencySnapshot(sim)` : memoise par seconde de jeu (`sim._npcCollectiveUrgency`). */
	ANASTASISSIM_API const FUrgencySnapshot& CollectiveUrgencySnapshot(FPlannerVillage& V);
	ANASTASISSIM_API TMap<FString, double> CollectiveUrgencyBiasMap(FPlannerVillage& V, const FString& ActorId);

	/**
	 * Tout ce que la decision de l'habitant lit, dans l'ordre ou `adultScores` / `buildScore` /
	 * `collectiveUrgencyBiasMap` le lisent : besoin de chantier, biais et planchers, urgence, corvee.
	 */
	ANASTASISSIM_API FPlannerDecision DecisionFor(FPlannerVillage& V, const FString& ActorId);
}
