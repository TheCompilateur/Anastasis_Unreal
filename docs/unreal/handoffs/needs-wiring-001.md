# HANDOFF: needs-wiring-001

## MISSION

Brancher dans le village et le lecteur du harnais les facteurs de besoins par habitant portés par
`needs-factors-001` : le premier ordre de travail du rapport 2 (`P3_PREMIER_RAPPORT.md`). En plus, le
deuxième ordre, côté lecteur : projeter `aiThinkAt` et `villagePhase`, que le C++ tient déjà.

**Branche posée sur `agent/needs-factors-001` (09bab00), dont elle dépend. À verser dans le même lot
qu'elle, ou après.**

## FILES_OWNED

- `Source/AnastasisSim/Public/Village/AnastasisVillage.h` : `FNpc::Phenotype`, `FNpc::Conditioning`
- `Source/AnastasisSim/Private/Village/AnastasisVillage.cpp` : bloc des besoins de `UpdateNpc` ; `NeedsCritical` délègue à `AreNeedsCritical`
- `Source/AnastasisSim/Private/Harness/AnastasisJsSave.cpp` : lecture de phénotype, génome et conditionnement ; projection du conditionnement, d'`aiThinkAt` et de `villagePhase`
- `Source/AnastasisSim/Private/Tests/AnastasisVillageNeedsFactorsTests.cpp` (nouveau)
- `Source/AnastasisSim/PORTAGE.md` (paragraphe « Branché par needs-wiring-001 »)
- `docs/unreal/handoffs/needs-wiring-001.md`

## COMMIT

Voir `git log agent/needs-wiring-001`.

## MEC

- BUILD : `anastasis-unreal.ps1 build` → `BUILD::PASS` (adaptatif non-unity sur les fichiers touchés).
- TESTS : `report-tests.ps1 -Filter Anastasis.Sim` (avant la projection `aiThinkAt` / `villagePhase`) →
  PASS 110, KNOWN_EXPECTED_FAILURE 2 (`Parite.Fbm`, `Parite.SemantiqueJs`), FAIL 0, 112/112 annoncés.
  Après la projection : `-Filter Anastasis.Sim.Harnais` → PASS 3, KEF 0, FAIL 0.
  - `Anastasis.Sim.Village.BesoinsParHabitant` (nouveau) :
    - un habitant médian garde au bit près les mètres de `TickNeeds` sans facteurs ;
    - un phénotype personnel (faim ×1,1719…, soif ×0,9247…) donne au bit près `TickNeeds` avec ses facteurs ;
    - au travail et fatigué, le conditionnement avance comme `TickNeedsConditioning` sur les mètres d'après la branche.
  - Tous les `Village.*` existants restent verts : le village du jeu ne porte ni phénotype ni conditionnement.
- RAPPORT JS / Unreal (`endurance`, 16 200 ticks, vue `settlement`, référence = clone du tag `anastasis-ref-p3`) :
  - premier tick divergent toujours **1** (`actors`, B=`636f5c203e8cbda8`) ; `buildings` et `rng` au tick 32, `mealReservations` au 133, `tileDiff` au 257 (inchangés) ;
  - forage du tick 1 (`diff-states.mjs`) : **20 champs** au lieu de 45 au rapport 2. Les huit mètres, le conditionnement, `aiThinkAt` et `villagePhase` sont égaux à la référence pour les cinq habitants. Restent, par habitant :
    - `lifestyle.lastNotedDay` (1 contre 0) et `placeMemory` (`favoriteBuildingId`, `buildings.building-0`) : branchement de `lifestyle-001` ;
    - `workTimer` (dt contre 0) : chemin d'acte du but `observer` (`npc.workTimer += dt`, npc.js l. 3522), non porté.
- COMMANDS:
  - `$env:ANASTASIS_HARNESS_TICKS='16200'; $env:ANASTASIS_HARNESS_DRILL='1'; tools\unreal\report-tests.ps1 -Filter Anastasis.Sim`
  - `node tools/migration/emit-state-digests.mjs -ref <clone> -scenario tools/migration/scenarios/endurance.json -days 3 -out js.jsonl`
  - `node tools/migration/emit-state-digests.mjs -ref <clone> -scenario …/endurance.json -ticks 1 -dump 1 -dump-out js.tick1.json`
  - `node tools/migration/compare-digests.mjs js.jsonl Saved/HarnessTraces/endurance-unreal.jsonl`
  - `node tools/migration/diff-states.mjs js.tick1.json Saved/HarnessTraces/endurance-unreal.tick1.json`

## ECARTS

- modifié : n° 8 — `createNpc` sans `ensureGenome` ni `ensureConditioning` : un habitant créé par le C++ reste médian (facteurs 1, conditionnement immobile), maintenant que les besoins lisent phénotype et conditionnement. Fiche et marque `ecart n°8` dans `UpdateNpc` et `FNpc`.
- aucun ouvert, aucun fermé : les facteurs, le conditionnement et la lecture du harnais sont fidèles, prouvés par `Anastasis.Sim.Village.BesoinsParHabitant` et le forage du tick 1 du scénario `endurance` (mètres et conditionnement égaux à la référence).

## PROOFS

PROOFS: (aucune)

## SCN

`endurance` (empreinte `52f66b01c0766137`), inchangé.

## PLY

Sans objet.

## INTEGRATION_RISK

- Dépend de `needs-factors-001`, à verser avec elle ou après.
- `AnastasisVillage.h/.cpp` : `sim-rng-001` (renfort) touchera la décision du village ; je lui ai réservé le
  bloc des besoins d'`UpdateNpc` et les deux champs de `FNpc`. En cas de conflit de texte sur la liste des
  écarts, garder les deux.
- Ordre de la fin des besoins changé : les moodlets passent AVANT la pluie, comme dans la référence. Ils
  n'écrivent que le moral, la pluie l'énergie et la santé : aucun bit ne bouge (suite `Village.*` verte).

## STOP

- La divergence au tick 1 n'est PAS levée : il reste `lifestyle` et `placeMemory` (`lifestyle-001` à brancher) et `workTimer`.
- Les habitants créés par le C++ (jeu, scénarios `First*`) n'ont ni génome ni conditionnement : `ensureGenome` et `ensureConditioning` de `createNpc` ne sont pas branchés (écart n° 8).
- Pas de rapport 4 dans `P3_PREMIER_RAPPORT.md` : `sim-report-003` y ajoute le rapport 3 et n'est pas encore versée. La mesure est ici ; elle ira au rapport suivant.
