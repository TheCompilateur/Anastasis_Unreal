# HANDOFF: sim-digest-emitter-001

## MISSION

Phase 3, jalon A (`P3_PLAN.md` §3), dernière mission : l'émetteur Unreal. Reprendre l'état lu d'un
scénario dans l'hôte C++, le faire tourner, projeter l'état vivant sur les sections de `serialize`,
écrire la trace JSONL au format JS ; produire et archiver le **premier rapport réel** de premier tick
divergent JS / Unreal.

## FILES_OWNED

- `Source/AnastasisSim/Public/Harness/AnastasisHarnessTrace.h`, `Private/Harness/AnastasisHarnessTrace.cpp` (nouveaux)
- `Source/AnastasisSim/Private/Tests/AnastasisHarnessTraceTests.cpp` (nouveau)
- `Source/AnastasisSim/Public/Sim/AnastasisSimulation.h`, `Private/Sim/AnastasisSimulation.cpp` : `ResetFromWorld` (ajout)
- `Source/AnastasisSim/Public/Village/AnastasisVillage.h`, `Private/Village/AnastasisVillage.cpp` :
  `RestoreForHarness`, `GetLiveTiles`, `GetMealSeq` (ajouts, rien de modifié)
- `Source/AnastasisSim/Public/Core/AnastasisJson.h`, `Private/Core/AnastasisJson.cpp` : `Stringify` (ajout)
- `Source/AnastasisSim/Public/Harness/AnastasisJsSave.h`, `Private/Harness/AnastasisJsSave.cpp` : génération vierge en cache
- `tools/migration/emit-state-digests.mjs` (`-dump`), `tools/migration/diff-states.mjs` (nouveau)
- `docs/migration/phase3/P3_PREMIER_RAPPORT.md` (nouveau), `docs/migration/phase2/P2_HARNAIS_DIFFERENTIEL.md`, `Source/AnastasisSim/PORTAGE.md`
- `docs/unreal/handoffs/sim-digest-emitter-001.md`

## COMMIT

BRANCH_HEAD

## MEC

- `tools\unreal\anastasis-unreal.ps1 build` → `BUILD::PASS`.
- `ANASTASIS_HARNESS_TICKS=16200 ANASTASIS_HARNESS_DRILL=1 tools\unreal\report-tests.ps1 -Filter Anastasis.Sim.Harnais`
  → **3 PASS / 0 KNOWN_EXPECTED_FAILURE / 0 FAIL** (`Harnais.Json`, `Harnais.Lecture`, `Harnais.Trace`).
  `Harnais.Trace` : reprise (3 bâtiments, 5 habitants, horloge 37,8 jour 1) ; **tick 0 après reprise = empreintes de
  la référence sur les 10 sections** ; 16 201 échantillons ; deux exécutions de 300 ticks, même empreinte ; info :
  « sections changées entre le tick 0 et le tick 16200 : actors, buildings, day, mealReservations, tileDiff, time ».
- `node tools/migration/compare-digests.mjs js.jsonl Saved/HarnessTraces/endurance-unreal.jsonl` (JS : 3 jours,
  scénario `endurance`, clone du tag) → `PREMIER TICK DIVERGENT : 1`, section `actors` ; puis `buildings` et `rng` au
  tick 120, `mealReservations` 133, `tileDiff` 298 ; **`time`, `day`, `seed`, `w`, `h` identiques sur 16 201 ticks**.
  Deux productions de la trace C++ de 3 jours rendent le même rapport.
- `node tools/migration/diff-states.mjs js.tick1.json …/endurance-unreal.tick1.json` → 40 champs, tous dans `actors`,
  les mêmes pour les 5 habitants : 7 besoins (le JS ne les a pas avancés, le C++ si) et `_simBudgetAccum`. Cause : la
  cadence du budget (`consumeNpcSimulationCadence`, bande *far* à 1 Hz, vue en (0, 0)) n'est pas branchée dans
  `FVillage::UpdateActors`. Archivé : `docs/migration/phase3/P3_PREMIER_RAPPORT.md`.
- FINISH : voir le compte rendu de passation.

## SCN

Sans objet.

## PLY

Sans objet : rien de visible ne change.

## INTEGRATION_RISK

- `AnastasisVillage.h/.cpp` et `AnastasisSimulation.h/.cpp` : AJOUTS seulement (trois méthodes du village, une de
  l'hôte), aucune ligne existante modifiée. Fichiers chauds : un rebase peut demander de recaser l'ajout.
- `RestoreForHarness` pose les bâtiments avec leurs seuils SAUVEGARDÉS (le JS les persiste) au lieu de les recalculer.
- Le test écrit dans `Saved/HarnessTraces/` (ignoré par git). Durée par défaut 600 ticks ; 3 jours par
  `ANASTASIS_HARNESS_TICKS=16200`.

## STOP

- Ne revendique **aucune** parité de simulation : le rapport dit l'inverse dès le tick 1. Il revendique un
  instrument qui le dit, au bon tick, avec la bonne cause.
- Les champs non lus par le lecteur sont recopiés (figés) côté C++ ; `rng` est l'état lu, figé (pas de `sim.rng` C++).
- Pas de branchement de la cadence : c'est l'ordre de travail que le rapport donne (`DEC` à prendre, P3_PREMIER_RAPPORT).
- Pas de commandlet : la trace sort d'un test d'automation.
