# HANDOFF: budget-cadence-001

## MISSION

Fermer la cause du premier écart du rapport JS / Unreal (`P3_PREMIER_RAPPORT.md`) : brancher la cadence du
budget (`consumeNpcSimulationCadence`) dans la boucle des habitants C++, et — décision d'Alexandre, option B —
épingler la vue du budget sur le village dans le scénario.

## FILES_OWNED

- `Source/AnastasisSim/Public/Village/AnastasisVillage.h`, `Private/Village/AnastasisVillage.cpp` : `FNpc::SimBudgetAccum`
  et `bHasSimBudgetAccum`, `FVillage::SetSimulationView`, cadence en tête d'`UpdateNpc`, écart n° 5 réécrit
- `Source/AnastasisSim/Private/Tests/AnastasisVillageCadenceTests.cpp` (nouveau)
- `Source/AnastasisSim/*/Harness/AnastasisJsSave.*` (`_simBudgetAccum` lu et projeté), `AnastasisHarnessTrace.*` (`vue`)
- `Source/AnastasisSim/Private/Tests/AnastasisScenarioVectors.inl` (régénéré : seule l'empreinte change)
- `tools/migration/scenarios/scenario-format.mjs` (format 2), `endurance.mjs`, `endurance.json` (reconstruit), `masks.mjs`
- `tools/migration/build-scenario.mjs`, `emit-state-digests.mjs` (vue épinglée)
- `docs/migration/phase3/P3_PREMIER_RAPPORT.md` (rapport 2), `P3_SCENARIOS.md`
- `docs/unreal/handoffs/budget-cadence-001.md`

## COMMIT

BRANCH_HEAD

## MEC

- `node tools/migration/build-scenario.mjs -ref <tag> -scenario endurance …` → format 2, vue (54, 57), empreinte
  `52f66b01c0766137`, point fixe en 2 allers-retours ; audit 3 jours : masques levés `updateNpc` 10 244 tirages,
  masques posés 6 750 (tous dans `updateNpc`).
- `node tools/migration/gen-scenario-vectors.mjs -ref <tag>` → global du tick 0 inchangé (`47a2a2ffc0e5d98a`).
- `tools\unreal\anastasis-unreal.ps1 build` → `BUILD::PASS`.
- `ANASTASIS_HARNESS_TICKS=16200 tools\unreal\report-tests.ps1 -Filter Anastasis.Sim` → **94 PASS / 2 KNOWN_EXPECTED_FAILURE /
  0 FAIL**, 96 annoncés. Premier lancement : `TESTS::FAIL lanceur bloque` — démarrage de l'éditeur en 755 s (premier boot
  du worktree, shaders), aucun test lancé ; relancé sans changement. `Anastasis.Sim.Village.Cadence` : near tourne à
  chaque tick sans `_simBudgetAccum` ; medium attend 5 ticks puis tourne avec 0,1 s (faim 20,000000 → 20,058000),
  accumulateur à 0 ; sans vue, tourne dès le premier tick.
- Rapport JS / Unreal, 3 jours, vue épinglée : premier tick divergent **1** (`actors`) ; `buildings` et `rng` 32,
  `mealReservations` 133, `tileDiff` 257 ; `time`, `day` identiques sur 16 201 ticks. Forage au tick 1 (état C++
  reconstruit depuis le forage du rapport 1, empreinte vérifiée `8b3a4d5cb53808f9`) : 45 champs — besoins personnels
  (`hunger`, `thirst`, `energy`), `workTimer`, `aiThinkAt`, `villagePhase`, `placeMemory`, `lifestyle.lastNotedDay`.
  `health`, `hygiene`, `leisure`, `social` identiques.
- FINISH : voir le compte rendu de passation.

## SCN

Sans objet.

## PLY

Sans vue posée, le village du jeu est inchangé (écart n° 5, reste).

## INTEGRATION_RISK

- `AnastasisVillage.h/.cpp` : ajouts (champs, une méthode, un bloc en tête d'`UpdateNpc`) et le texte de l'écart n° 5.
  La session `labo-ecarts-001` tient un champ `jugement` sur la fiche n° 5 d'`ECARTS.md` : non touché ici.
- Le scénario passe au format 2 : un fichier de format 1 est refusé par `chargerScenario`.
- La cadence n'agit que si l'hôte pose une vue : le jeu (`Anastasis_UnrealV2`) n'en pose pas encore.

## STOP

- Ne revendique pas que le premier tick divergent a reculé : il reste 1, pour une autre cause (besoins personnels,
  champs non projetés). La mission retire une cause, elle ne rapproche pas encore les traces.
- Le forage C++ au tick 1 est reconstruit et vérifié par empreinte, pas réécrit par un nouveau run C++.
- Pression du budget toujours 0 ; pas de vue liée à la caméra du jeu.
