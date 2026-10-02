# Le planificateur collectif — carte du portage (build-decision-001)

Pourquoi : la ligne `build` de la décision (`buildScore`, `npc.js` l. 2733) lit le planificateur de bâtiments
du village (`src/sim/collectivePriorities.js`, 3 846 lignes). Au tick 32 du scénario `endurance`, `npc-2`
choisit `build` sans chantier ouvert (relevé `tools/migration/trace-build-score.mjs`) :

| Terme de `buildScore` | Valeur au tick 32 | Source |
|---|---|---|
| `need` = `sim.buildingNeedScore()` | 220 (plafond) | `collectiveBuildingNeedScore` : somme des scores **vivants** de `boostedBuildingScores` |
| `liquidity` | 1 (trésor 420 ≥ `BUILD_WAGE` 15) | `colony.treasury` |
| `trait.build × job.traitBias.build` | 1 (npc-2, colon) | habitant |
| `collectiveGoalBias(sim, "build")` | 20 | `liveEffects` : effets calculés sur les scores **stockés** (`priorities.buildingScores`, vides dans la sauvegarde) |
| `planBias(npc, "build")` | 0 (26 pour npc-4) | `ai/ambitions.js` |
| `colonySiteBuildBias` | 0 (16 plus tard dans la journée) | `colonySite.js` |
| cible sans chantier | `accessPointNear(plannedMarketPos())` = (58,5 ; 55,5) | `simulation.js` l. 5852 |

Relevé du graphe d'appels (agent de lecture, 2026-10-02), lignes de la référence `anastasis-ref-p3` :

## Ce qui n'est pas pur

- **Hasard caché** : `refreshColonyStockReport` (`colonyStockReport.js` l. 149-165) tire 0 à 4 fois dans
  `sim.rng` quand le rapport n'est pas du jour (`reportedColonyStock` / `hubConsolidationPressure`, paresseux).
  En régime normal, la passe quotidienne (`updateCollectivePrioritiesDaily`, CP l. 2904) l'a déjà fait.
  `findBuildSpot` (SIM l. 7848) tire aussi, atteint seulement par `villageHerdPending` si le meilleur choix est `fishery`.
- **Écritures** : `housingVacancySnapshot` écrit `building.vacantSinceDay` ; `ensureColonyStockReport` et le
  rafraîchissement écrivent `colony.stockReport` et `building.stock` ; `liveEffects` met `priorities._effects`
  en cache ; `ensureFounderCharter` peut expirer la charte (`colony.charter = null`, `_effects = null`).
- `sim.totalColonyPhysical` n'existe pas : `phys` vaut toujours 0 dans `canCoverSpineSeed` (bizarrerie à copier).

## Volume

| Bloc | Lignes (commentaires compris) |
|---|---|
| `collectivePriorities.js`, fonctions | ≈ 1 450 (`scoreBuildingProjects` 437, `computeEffects` 346, `measureJobNeeds` 81, `exploitSpinePending` 72, `applyDailyFocusToEffects` 65, `ensureCollectivePriorities` + création ≈ 107, ~30 petites ≈ 310) |
| `collectivePriorities.js`, constantes (`COLLECTIVE`, `BUILD_CANDIDATES`, `DAILY_FOCUS_FLOOR`, `GOAL_BIAS_SOFT_KEY`…) | ≈ 210 |
| `colonyStockReport.js` | ≈ 230 |
| `founderCharter.js` | ≈ 120 |
| `forestSustain.js` + `colonizationDoctrine.js` | ≈ 140 |
| `transport/stockLedger.js` | ≈ 75 |
| méthodes de `Simulation` (`housingCapacity`, `pendingHousingCapacity`, `marketCaps`, `totalBuildingValue`, `buildCost`, `constructionOpenSlots`, `siteCanPlacePiece`…) | ≈ 165 |
| **Total** | ≈ 2 400, soit ≈ 1 600 de code |

`findBuildSpot` et son sous-graphe urbain restent une frontière (fonction injectée, ou écart pour `fishery`).

## Données lues

- Colonie : `actors.length`, `day` (souvent `|0`), `colony.morale`, `colony.treasury`, `colony.doctrine.hotPads` /
  `.expansionBonus`, `colony.priorities` (`.priorities[t].level`, `.dailyFocus`, `.jobs`, `.buildingScores`,
  `.siteWatch`), `colony.stockReport`, `colony.charter`, `market.stock` (food, wood, stone, planks, tools,
  leather, wool), `archetype.scarceSeed`, `settlement` (x, y, clearRadius), tuiles (`frontierForestDensity`).
- Bâtiments : `type`, `progress` (souvent `?? 1`), `id`, `owner`, `vacantSinceDay`, `materialsNeeded`,
  `materialsConsumed`, `piecesPlaced`, `stock`, `housePhase`, `x`, `y`.
- Habitants : `lifeStage`, `jobId`, `alive`, `home`, `shelter`, `workplace.id`.

## Découpage

1. `planner-state-001` : l'état de la colonie dans le C++ (trésor, priorités, rapport de stock, charte,
   doctrine, moral, stock du marché, archétype) et son lecteur ; `ensureCollectivePriorities`,
   `liveEffects` / `computeEffects` / `applyDailyFocusToEffects` / `applyFounderCharterToEffects` →
   `collectiveGoalBias` (le +20).
2. `planner-scores-001` : `measureJobNeeds`, `scoreBuildingProjects`, les cinq « pending », logement, stock
   connu, `buildCost` et les méthodes de `Simulation` → `buildingNeedScore` (le 220).
3. `build-decision-001` : la ligne `build` de `ChooseGoal` et sa cible sans chantier.

Preuve de chaque étape : vecteurs tirés de la référence sur des états échantillonnés le long d'`endurance`
(`gen-parity.mjs`), puis le harnais.
