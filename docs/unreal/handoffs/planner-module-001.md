# HANDOFF: planner-module-001

## MISSION

Commandée par « Simulateur IV Kingdoms migration phase 3 » (2026-10-02) : le planificateur collectif de la
référence (`src/sim/collectivePriorities.js`, tag `anastasis-ref-p3`), en MODULE SEUL, sur une vue explicite du
village. C'est la plus grosse pièce du tick 32 d'endurance : `npc-2` y choisit `build` sans chantier, avec
`buildingNeedScore` = 220 et `collectiveGoalBias(build)` = 20 ; au tick 196, `npc-0` prend `helpFarm` par le
plancher collectif de 28.

Une seule mission (et non deux) : `computeEffects` appelle les cinq « pending », dont `bestPendingByScore`
recalcule `scoreBuildingProjects` à vif, qui lit lui-même le rapport de stock (et ses tirages) — les effets ne se
séparent pas des scores (accord du coordinateur).

Porté, au bit :
- `collectivePriorities.js` : `measureJobNeeds`, `scoreBuildingProjects`, `boostedBuildingScores`,
  `collectiveBuildingNeedScore`, `computeEffects`, `applyDailyFocusToEffects`, `liveEffects` (et son cache
  `_effects`), `collectiveGoalBias`, `collectiveGoalFloor`, `isFoodRush`, `farmStaffingGap`, `exploitSpinePending`,
  `villageAmenityPending`, `villageCraftPending`, `villageHerdPending`, `craftBootstrapPending`,
  `bestPendingByScore`, `woodBootstrapNeeded` / `woodBootstrapDraft` / `isWoodBootstrapDraftee` (cache par jour),
  `readySiteServiceDebt`, logement (`housingDeficit`, `housingVacancySnapshot` et ses écritures), urgence
  (`collectiveHydrationStress`, `collectiveHousingSaturation`, `collectiveAccessStress`), `canCoverSpineSeed`
  (avec `phys` toujours 0 : `sim.totalColonyPhysical` n'existe pas), et les petites aides ;
- `colonyStockReport.js` : rapport, rafraîchissement paresseux AVEC ses tirages `sim.rng`, angles morts,
  consolidation ;
- `founderCharter.js` : `applyFounderCharterToEffects`, l'expiration (`ensureFounderCharter`) ;
- `forestSustain.js` / `colonizationDoctrine.js` : `forestGatherBrake`, `frontierForestDensity`,
  `colonizationBandRange` ;
- `transport/stockLedger.js` : `ensureBuildingStock`, stock physique / disponible, `findDepotsForResource` ;
- méthodes de `Simulation` lues : `countPlannedBuildings`, `countBuildings`, `activeConstructions`,
  `constructionOpenSlots`, `housingCapacity` / `pendingHousingCapacity` (phases de maison), `marketCaps`,
  `totalBuildingValue("security")`, `buildCost` (croissance, planches), `siteCanPlacePiece`, `marketPos` ;
- `npc.js` : `collectiveUrgencyBiasMap` et `readCollectiveUrgencySnapshot` (cache par seconde de jeu).

Pas porté (hors du chemin de la décision, ou quotidien) : `updateCollectivePrioritiesDaily`,
`measureCollectiveNeeds`, `pickDailyFocus`, `pickCollectiveBuilding`, la réallocation des métiers.

## FILES_OWNED

- `Source/AnastasisSim/Public/Village/AnastasisPlanner.h`, `Private/Village/AnastasisPlanner.cpp` (nouveaux)
- `Source/AnastasisSim/Private/Village/AnastasisPlannerCatalog.inl` (nouveau, GÉNÉRÉ par
  `tools/migration/gen-planner-catalog.mjs` : bâtiments, profils de dépôt, ordre de consommation, plafonds du
  marché, thèmes de charte, biais de métier, constantes)
- `Source/AnastasisSim/Private/Tests/AnastasisPlannerTests.cpp` (nouveau)
- `tools/migration/gen-planner-catalog.mjs`, `tools/migration/gen-planner-fixtures.mjs`,
  `tools/migration/planner/planner-fixtures.json` (nouveaux)
- `Source/AnastasisSim/ECARTS.md`, `PORTAGE.md`, `tools/migration/ported-functions.mjs`,
  `docs/migration/phase2/P2_INVENTAIRE_JS.md` (régénéré)

Non touchés : `FVillage` (ni champs ni décision), le lecteur, le harnais.

## COMMIT

Le commit qui porte cette fiche sur `agent/planner-module-001`.

## INTERFACE — pour colony-state-001 et la passe collective

Espace de noms `AnastasisPlanner` (`Village/AnastasisPlanner.h`).

La vue, `FPlannerVillage` :

| Champ | Source JS | Remarque |
|---|---|---|
| `Day`, `Time`, `W`, `H` | `sim.day`, `sim.time`, `sim.w`, `sim.h` | |
| `bHasSettlement`, `SettlementX/Y`, `SettlementClearRadius`, `MarketDx/Dy` | `sim.settlement` | optionnels = absents |
| `MarketPosCache` | `sim._marketPos` | posé quand un marché est bâti ; vide = `plannedMarketPos()` |
| `Buildings[]` (`FPlannerBuilding`) | `sim.buildings` | `Progress` optionnel (`undefined`), `Owner` (id, vide = sans), `VacantSinceDay`, `CreatedDay`, `HousePhase`, `MaterialsNeeded` / `MaterialsConsumed` (ordre des clés), `PiecesPlaced`, `Stock` (cases `{physical, reserved}`, ordre des clés) |
| `Actors[]` (`FPlannerActor`) | `sim.actors` | `LifeStage`, `JobId`, `bAlive` (`alive !== false`), `HomeId`, `ShelterId`, `WorkplaceId`, `TraitGather` (`Number(trait.gather) \|\| 0`), `InventoryWood` |
| `bHasColony`, `Colony` (`FColonyState`) | `sim.colony` | `Morale` (vide = 50), `DoctrineHotPads`, `DoctrineExpansionBonus`, `Priorities`, `StockReport`, `Charter` |
| `Colony.Priorities` (`FCollectivePriorities`) | `colony.priorities` | `Levels` (six types), `Jobs` (ordre de l'objet), `BuildingScores` STOCKÉS, `DailyFocus {Id, Forced}`, `SiteStalledSinceDay` (`siteWatch.sites[id].stalledSinceDay`), caches `Effects`, `WoodDraftDay` / `WoodDraft` |
| `Colony.StockReport` (`FStockReport`) | `colony.stockReport` | `Day`, `LastRefreshDay`, `Stock`, `Rumor`, `Blind[]`, `CertifiedNear`, `IgnoredFar` |
| `MarketStock` | `sim.market.stock` | ordre des clés |
| `ScarceSeedWood/Stone` | `sim.archetype.scarceSeed` | |
| `TileAt(x, y, out)` | `sim.tileAt` | type, ressource, quantité VIVANTE ; faux hors carte |
| `FindBuildSpot(type)` | `sim.findBuildSpot` | frontière urbaine ; lu seulement si le palier élevage propose la pêcherie |
| `Rng` | `sim.rng` | le flux partagé du village |
| `UrgencyBucket`, `UrgencyCache` | `sim._npcCollectiveUrgency` | cache |

La sortie : `DecisionFor(View, ActorId)` rend `FPlannerDecision` — `GoalBias` (les 15 buts de
`GOAL_BIAS_SOFT_KEY`), `GoalFloor` (les planchers posés), `UrgencyBias`, `bWoodBootstrapDraftee`, `bFoodRush`,
`FarmStaffingGap`, `BuildingNeedScore`. Mêmes champs que `FCollectiveDecision` (help-farm-001), plus
`BuildingNeedScore`.

## ÉCRITURES — ce que l'hôte recopie de la vue vers le village

Après CHAQUE appel (`DecisionFor` ou une fonction isolée), dans cet ordre :

1. `Buildings[i].VacantSinceDay` (maisons achevées : posé si vide et sans propriétaire, effacé si un
   propriétaire existe) — écrit à chaque `housingVacancySnapshot`, idempotent dans la journée ;
2. `Buildings[i].Stock` (`ensureBuildingStock` : cases du profil créées, `physical` / `reserved` bornés) —
   écrit par le rapport de stock et par `siteCanPlacePiece` ; idempotent ;
3. `Colony.StockReport` ET l'état du flux (`Rng`) — le rafraîchissement ne tire qu'**une fois par jour** (quand
   `LastRefreshDay != Day`) : 0, 1 ou 4 tirages (rumeur : test, amplitude, signe, durée) ;
4. `Colony.Charter` (vidée à l'expiration, qui vide aussi le cache des effets) — au plus une fois par charte ;
5. les caches : `Colony.Priorities.Effects` (`_effects`), `WoodDraftDay` / `WoodDraft` (par jour), `UrgencyBucket`
   / `UrgencyCache` (par seconde de jeu). Les recopier évite de recalculer ; ne pas les recopier change les
   résultats dès que l'état bouge dans la journée (la référence fige les effets au premier appel du jour).

Le cache des effets n'est vidé, dans la référence, que par la passe quotidienne et l'expiration de la charte :
l'hôte doit le vider au changement de jour (là où la référence fait `updateCollectivePrioritiesDaily`).

## MEC

- BUILD : `tools\unreal\anastasis-unreal.ps1 build` → `BUILD::PASS`.
- Catalogue : `node tools/migration/gen-planner-catalog.mjs -ref <clone>` → 35 bâtiments, 22 profils, 4 thèmes,
  132 constantes.
- Fixtures : `node tools/migration/gen-planner-fixtures.mjs -ref <clone>` → 20 variantes d'endurance
  (ticks 0, 32, 196 ; rapport périmé et dépôt lointain ; ferme sans fermier ; maisons vides et sans toit ;
  priorités hautes et scores stockés ; focus outils / vivres-fondation / pierre ; charte vivante et échue ;
  chantiers et corvée de bois ; chantier posable en dette ; industrie et aménités ; puits, hiver et disette ;
  rumeur tirée ; rumeur vivante ; sans puits ; marché bâti). La référence EXÉCUTE tout ;
  `collectiveUrgencyBiasMap` (non exportée) est évaluée depuis le texte de `npc.js`.
- Tick 196 d'endurance, référence et C++ : `build` 20, `gatherFood` 14, `gatherWood` 14, `helpFarm` 10 ;
  planchers `gatherFood` 40, `helpFarm` 28, `build` 24 ; `buildingNeedScore` 220.
- TESTS : `Anastasis.Sim.Parite.Planificateur` : 20 variantes, 100 décisions, 7 variantes où le flux tire,
  6 362 valeurs comparées, 0 écart. Suite `Anastasis.Sim` : À COMPLÉTER.
- MUTATIONS : À COMPLÉTER.

## ECARTS

- ouvert : n° 32 — Planificateur : la corvée de bois départage les égalités en ordre ordinal (A_TRANCHER) ;
  la référence trie par `localeCompare`, identique pour les identifiants `npc-N` de tous les scénarios.

Le reste du module est fidèle au bit, prouvé par `Anastasis.Sim.Parite.Planificateur` (20 variantes, 0 écart).
`findBuildSpot` est une frontière fournie par l'hôte, pas un écart.

## PROOFS

PROOFS: (aucune)

Module seul : preuve par `Anastasis.Sim.Parite.Planificateur`.

## SCN

Aucune scène, aucun asset.

## PLY

Sans objet.

## INTEGRATION_RISK

- Nouveaux fichiers seulement côté C++ (le module n'est appelé par rien) : aucun risque de comportement.
- Le catalogue est généré : le régénérer si la référence change (jamais à la main).

## STOP

- Ne branche rien dans `FVillage` : c'est la passe collective du coordinateur (colony-state-001).
- Le tri de la corvée de bois départage les égalités par ordre ordinal, et non par `localeCompare` (voir ECARTS).
- `findBuildSpot` reste une frontière : aucune fixture n'atteint la pêcherie du palier élevage.
