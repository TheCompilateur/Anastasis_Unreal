# HANDOFF: planner-wiring-001

## MISSION

Brancher le planificateur collectif (`Village/AnastasisPlanner`, porté seul par planner-module-001) sur le
village, et calculer la ligne `build` sans chantier, avec sa cible. C'est l'une des deux causes du premier
tick divergent du harnais (tick 32 : npc-2 choisit `build` dans la référence, sans chantier, avec un besoin
de 220 et un biais collectif de 20).

1. **La colonie** : le lecteur du harnais reprend `sim.colony` : moral, doctrine (`hotPads.length`,
   `expansionBonus`), priorités (niveaux, métiers, scores stockés, focus du jour, veille des chantiers),
   rapport de stock, charte, ainsi que `colony.treasury`, `market.stock` et `settlement.clearRadius`. Pour
   chaque bâtiment, il lit le stock complet (toutes les cases, dans l'ordre) et `vacantSinceDay`, et la
   projection les écrit.
2. **`CollectiveDecisionOf`** : sur une colonie, la vue du village (`BuildPlannerView`) passe à
   `AnastasisPlanner::DecisionFor`, puis les écritures du planificateur reviennent au village
   (`WritePlannerView`, dans l'ordre de sa fiche : vacance, stock, rapport, charte, caches). Le flux tiré
   est celui du village.
3. **La ligne `build`** (`buildScore`, npc.js l. 2733) est calculée pour tout adulte d'une colonie : besoin
   du planificateur, lisière, manques en attente, liquidité (trésor, salaire 15, fondation), traits,
   `builderFit`, `jobPriority`, biais collectif.
4. **La cible `build` sans chantier** : `bindBuildSite` puis `constructionAccessPoint`, sinon
   `marketAccessPoint` (accès au site du marché, build-decision-001).

## FILES_OWNED

- `Source/AnastasisSim/Public/Village/AnastasisVillage.h`, `Private/Village/AnastasisVillage.cpp` : `FBuilding::VacantSinceDay` / `PlannerStock`, `FCollectiveDecision::BuildingNeedScore` / `bHasColony` / `bBuildIdle` / `BuildNeedTerm`, membres de colonie, `RestoreColonyForHarness`, `BuildPlannerView`, `WritePlannerView`, `CollectiveDecisionOf` (non const), `BuildRowScore(…, Collective)`, la cible `build` dans `AssignTarget`
- `Source/AnastasisSim/Public/Work/AnastasisBuild.h`, `Private/Work/AnastasisBuild.cpp` : `BuildWage`, `Foundation*`, `BuildScoreFromNeed`
- `Source/AnastasisSim/Private/Harness/AnastasisJsSave.cpp` : stock complet et `vacantSinceDay`, lus et projetés
- `Source/AnastasisSim/Private/Harness/AnastasisHarnessTrace.cpp` : `ReadColony`, reprise de la colonie
- `Source/AnastasisSim/Private/Tests/AnastasisVillagePlannerWiringTests.cpp` (nouveau)
- `Source/AnastasisSim/ECARTS.md` (n° 27 modifié, n° 32 harnais)
- `Act` : `JsTravelActivity` (`travelActivity`, npc.js l. 3821)
- `docs/unreal/handoffs/planner-wiring-001.md`
- Reprend `agent/planner-module-001` (renfort, f4d42a8) et deux commits de build-decision-001 (afeb5b7, 8d6f1f9) : à verser après planner-module-001, ou avec.

## COMMIT

Voir `git log agent/planner-wiring-001`.

## MEC

- BUILD : `BUILD::PASS`.
- TESTS : `report-tests.ps1 -Filter Anastasis.Sim` → PASS 125, KNOWN_EXPECTED_FAILURE 2 (`Parite.Fbm`,
  `Parite.SemantiqueJs`), FAIL 0, 127/127 annoncés.
  - `Anastasis.Sim.Village.PlanificateurBranche` (nouveau) : sans colonie, décision vide (aucun biais, aucun
    plancher) ; avec la colonie d'`endurance` (moral 56, trésor 420, rapport de stock du jour 1), la décision vient
    du planificateur (biais par but, besoin de bâtir > 0, ligne `build` non nulle, terme `needFloor × 1` ≥ besoin),
    le flux n'est pas tiré et une deuxième lecture rend les mêmes valeurs ; sans chantier, un bâtisseur choisi
    vise l'accès au site du marché (source `market`).
  - `Parite.Planificateur` (planner-module-001) et tous les `Village.*` restent verts : leurs villages n'ont
    pas de colonie.
- HARNAIS (`endurance`, 16 200 ticks, forage au tick 32) :
  - avant : au tick 32, npc-2 restait `observer` (la référence choisit `build`, score 223,8) ;
  - après : npc-2 choisit **`build`**, cible **(58,5 ; 55,5)** (l'accès au site du marché), `goalSince` et
    `aiThinkAt` identiques à la référence ; activité en route `chantier` identique ;
  - premier tick divergent toujours **32** (`actors`, `buildings`), `rng` au tick 125 inchangé. Il reste 32 champs
    au tick 32, tous sur npc-2 sauf le puits :
    - ce qu'une **première pensée** écrit et que le C++ n'écrit pas : `goalExplain`, `streetDecision`, `workShift`,
      `hungerAction`, `_algoDebug`, `hesitationTimer` / `hesitationCooldown`, `nocturnalIntent`, `socialSeekId`,
      `buildBinding`, `mind.failures.{causes, goals, negative}`, `activitySince`. Le tick 32 est la toute première
      pensée du scénario : aucun habitant n'avait encore décidé ;
    - le **service de navigation** (`navigation.*`, `path`, `pathGoal`, `lastMoveDir`, `trafficTimer`, d'où un
      écart de 0,006 sur x/y) : n° 4 ;
    - `building-1` (puits) : un point d'accès que la référence retire, resource-targets-001 (renfort).
- Écart non déclaré trouvé par la mesure et corrigé : en route, le C++ écrivait « marche » pour tout but ; la
  référence écrit `travelActivity(goal)` (« chantier », « boit », « cherche »…). Porté dans `Act`.

## ECARTS

- modifié : n° 27 : le planificateur est branché sur une colonie reprise. Reste : le village du C++ sans colonie, la passe quotidienne non portée (cache `_effects` jamais vidé au changement de jour), `pickCollectiveBuilding` (essentiel lu faux), les biais de colonisation et de brief nuls, `liveHotPads` sans élagage, `findBuildSpot` non fourni, l'ordre d'appel (une fois avant la table), l'arrivée sans chantier (n° 18).
- modifié : n° 32 : branché, la divergence d'ordre toucherait `actors`.

## PROOFS

PROOFS: (aucune)

## SCN

`endurance`, inchangé.

## PLY

Sans objet.

## INTEGRATION_RISK

- Dépend de planner-module-001 (non versée) : verser les deux dans le même lot, planner-module-001 d'abord.
- `CollectiveDecisionOf` n'est plus `const`.
- resource-targets-001 (renfort) ajoute son bloc dans `ChooseGoal` avant `ApplyCollectivePass` : zone voisine,
  pas la même.
- Un village créé par le C++ (le jeu) n'a pas de colonie : son comportement ne change pas.

## STOP

- Ne revendique pas la passe quotidienne des priorités, `pickCollectiveBuilding`, l'ouverture de chantier
  (`tryOpenNewConstruction`, build-002), ni le premier tick divergent au-delà de ce que dit MEC.
