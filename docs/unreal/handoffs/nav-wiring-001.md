# HANDOFF: nav-wiring-001

## MISSION

Commandée par « Simulateur IV Kingdoms migration phase 3 » (2026-10-02) sous le nom nav-service-001, renommée
nav-wiring-001 : ce nom désigne déjà, dans `main`, le portage du service en module seul (7e978ff), et sa fiche
`nav-service-001.md` reste intacte. Objet : brancher le service de navigation sur le village et porter le pas,
pour la partie marche de l'écart n° 4.

Au tick 32 d'endurance, npc-2 a dans la référence un chemin demandé au service
(`navigation.{path, pathIndex, targetKey, requestedAt, navVersion, doorQueueRole…}`, `path`, `pathGoal`,
`lastMoveDir`, `trafficTimer`) et le pas qui en découle. Le C++ marchait avec son ancien A* direct, et le lecteur
ne relisait ni ne projetait rien de la navigation.

Relevé d'abord : `tools/migration/trace-nav.mjs` sur la référence instrumentée, résultat dans
`docs/migration/phase3/P3_NAV_RELEVE.md`. Sur endurance, 16 200 ticks :
- `requestPath` : 217 A* immédiats, 83 chemins servis par le cache (dès le tick 114), aucune mise en file ;
- `steerAroundBlock` et `resolveStuckActor` : jamais atteints ;
- file de porte : rôle solo 19 942 fois, waiter 182 (dès le tick 114), leader 67 ;
- hésitation tirée 149 fois (dès le tick 118) ; `recordPassage` 394 fois ;
- facteur de vitesse ≠ 1 sur 18 814 pas. Au tick 32, c'est `lifestyleTravelFactor` qui vaut 0,86 (npc-2 est
  noctambule), d'où l'écart de 0,006 sur x / y.

Porté :
- le service (`World/AnastasisNavService`) branché. `FVillage` en est l'hôte (`INavServiceHost`) : il remplit la
  vue de l'habitant avant l'appel et la recopie à la sortie. `beginNavTick`, puis `processNavQueue` avant et après
  la boucle des habitants. Le cache est vidé à l'ajout d'un bâtiment ;
- `nextWaypoint` par `requestPath` (cache exact et de zone, A* sous budget, file, `awaitingPath`) ;
  `steerAroundBlock` ;
- `moveActor` : hésitation (le `hashText01` local de simulation.js, graines 29 / 31), `lastMoveDir`, file de porte
  (`resolveDoorQueue`, `doorQueueWaypoint`, ralentissement 0,55 du waiter), facteur de vitesse de l'état porté
  (énergie, faim, santé, moral, hésitation, pluie, pulsation d'épuisement, foule près de la cible, mode de vie),
  anti-blocage avec `stuckTicks`, `trafficTimer` et `recordPassage` ;
- `resolveStuckActor` fidèle (vide `navigation.path`, remet `targetKey` à l'autre seuil) ; le garde-fou
  `pathFailStreak >= 3` après `moveActor` (npc.js l. 3427 et 3471) ;
- les sites qui jettent le chemin, alignés sur la référence (`npc.path = null ; pathCooldown = …`, sans toucher
  `navigation.targetKey` ni `navVersion`) : entrée dans un bâtiment, redirections après échec, livraison ;
- `navigation.path` / `pathIndex` tenus en COPIE, recopiés seulement par `syncNavigationFromActor`, comme dans la
  référence ;
- lecteur et projection : `path`, `pathGoal`, `pathFailStreak`, `lastMoveDir`, `trafficTimer`, `hesitation*`,
  `navigation.*` ; reprise du harnais : `navVersion` et `navCache` de la sauvegarde.

## FILES_OWNED

- `Source/AnastasisSim/Private/Village/AnastasisVillageNav.cpp` (nouveau : la marche)
- `Source/AnastasisSim/Private/Tests/AnastasisVillageNavTests.cpp` (nouveau)
- `Source/AnastasisSim/Public/Village/AnastasisVillage.h` (champs de navigation de `FNpc`, `FVillage` hôte du service)
- `Source/AnastasisSim/Private/Village/AnastasisVillage.cpp` : `Bind`, `AddBuilding`, `UpdateActors`, `Act`
  (garde-fou `pathFailStreak`), `ResolveStuckActor`, sites qui jettent le chemin ; anciens `NextWaypoint` /
  `MoveActor` retirés
- `Source/AnastasisSim/Public/World/AnastasisNavService.h`, `Private/World/AnastasisNavService.cpp`
  (`FNavAgent::ApplyCount`)
- `Source/AnastasisSim/Private/Harness/AnastasisJsSave.cpp` (bloc actors : navigation),
  `Private/Harness/AnastasisHarnessTrace.cpp` (`Restore` : `navVersion`, `navCache`)
- `tools/migration/trace-nav.mjs`, `docs/migration/phase3/P3_NAV_RELEVE.md` (nouveaux)
- `Source/AnastasisSim/ECARTS.md`, `PORTAGE.md`, `tools/migration/ported-functions.mjs`,
  `docs/migration/phase2/P2_INVENTAIRE_JS.md` (régénéré)

## COMMIT

Les commits de `agent/nav-wiring-001`, posés sur `agent/spatial-risk-test-001` (22e679e, chaîne rebasée sur main da7c771).

## MEC

- BUILD : `tools\unreal\anastasis-unreal.ps1 build` → `BUILD::PASS`.
- Suite `Anastasis.Sim` avant le test village : 127 PASS, 2 KNOWN_EXPECTED_FAILURE, 0 FAIL. Suite complète : voir
  `finish -Prove`.
- `Anastasis.Sim.Village.Navigation` : trois habitants vers le même seuil du puits.
  - Rôles leader / waiter / waiter, rangs 0 / 1 / 2.
  - Le leader avance de 0,0667 vers le seuil, le dernier waiter de 0,0243.
  - Le premier waiter, déjà à son point d'attente, reste sur place.
  - Pour les trois : clé de cible, version de navigation, copie du chemin.
- MUTATION (posée, build, `Village.Navigation`, retirée) : file de porte coupée (`if (false && IsDoorLike() …)`)
  → détectée : `le plus proche passe : attendu "leader", obtenu "solo"`, `rang du deuxieme : attendu 1, obtenu 0`,
  `le premier waiter reste a son point d'attente` faux.
- Harnais (endurance, 600 ticks, `compare-digests.mjs` contre la référence) : `buildings` 125, `rng` 125,
  `tileDiff` 257, `mealReservations` 320. Forage du tick 32 (`diff-states.mjs`) : tous les champs de navigation
  et le pas de npc-2 sont identiques. Restent `navigation.destBuildingId` (voir ECARTS) et les champs de la première
  pensée (premiere-pensee-001).

## PROOFS

PROOFS: (aucune)

## SCN

Aucune scène, aucun asset. La marche en jeu passe par le même code : un habitant suit désormais un chemin du
service, attend à une porte encombrée et hésite aux virages.

## PLY

Sans objet.

## ECARTS

- modifié : n° 4 — la marche est portée. Restent :
  - `navigation.destBuildingId` : dernier écrivain non porté (`failureTargetBiasMap`), et remise à zéro au commit.
    Fermeture : failure-target-001 ;
  - `separateCrowdedActors` (masqué) et le verrou du seuil domestique en route ;
  - famille, routes et quartiers valent 1 dans le facteur de vitesse ;
  - risque d'un écart au dernier bit sur `atan2` / `cos` / `sin` dans la file de porte.
- modifié : n° 29 — `MoveActor` vit désormais dans `Village/AnastasisVillageNav.cpp` ; le coût de terrain du mode jeu
  (route-cost-001) y est reporté à l'identique, le mode référence inchangé.
- hérités de la base (`agent/spatial-risk-test-001`, posée sur lifestyle-decision-001 et premiere-pensee-001),
  non modifiés ici : n° 32, n° 33, n° 34, n° 35, n° 36.

## INTEGRATION_RISK

- Dépend de `agent/spatial-risk-test-001` → `agent/resource-targets-001` → `agent/planner-wiring-001` : à verser
  après elles.
- premiere-pensee-001 (coordinateur) touche aussi `AnastasisJsSave.cpp` (fonctions séparées) et
  `AnastasisVillage.cpp` (`CommitGoal`, `AssignTarget`, `SetActivity`). Zones disjointes, mais un rebase est
  probable. failure-target-001 se posera sur cette branche.
- La marche change partout : les tests de scénario qui comptaient des secondes de trajet peuvent bouger. La suite
  `Anastasis.Sim` passe.

## STOP

- Pas `failureTargetBiasMap` ni le `Reset` de `DestBuildingId` dans `CommitGoal` (failure-target-001).
- Pas `separateCrowdedActors` (masqué), ni le verrou du seuil domestique en route.
- Pas de métriques de navigation ni d'anneau de trace dans le village (preuve seulement, sans effet).
