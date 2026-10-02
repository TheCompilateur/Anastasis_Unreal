# HANDOFF: sim-rng-001

## MISSION

Version réduite, décidée avec « Simulateur IV Kingdoms migration phase 3 » :
1. exposer le flux partagé `sim.rng` du village C++ et son état (pour `save.rng`) ;
2. porter `goalNoise` en fonction pure, prouvée au bit près ;
3. MESURER dans la référence (`anastasis-ref-p3` = `fee66ae`) chaque tirage `sim.rng` du scénario
   `endurance` — tick, habitant, site, nombre par décision, et le premier tirage du tick 32.

Aucune ligne de la table de décision n'est branchée. La raison est mesurée, voir plus bas. La suite
est perception-explore-001.

## FILES_OWNED

- `Source/AnastasisSim/Public/Ai/AnastasisGoalNoise.h`, `Private/Ai/AnastasisGoalNoise.cpp` (nouveaux)
- `Source/AnastasisSim/Private/Tests/AnastasisGoalNoiseTests.cpp` (nouveau)
- `Source/AnastasisSim/Private/Tests/AnastasisGoalNoiseVectors.inl` (nouveau, généré)
- `Source/AnastasisSim/Public/Village/AnastasisVillage.h` : `GetSimRngState` / `SetSimRngState` à côté de
  `SetRngSeed`, et le texte des écarts n°1 et n°16 complété (rien de retiré de l'existant)
- `tools/migration/rng-trace-lib.mjs`, `trace-sim-rng.mjs`, `gen-goal-noise-vectors.mjs` (nouveaux)
- `docs/migration/phase3/P3_RNG_RELEVE.md` + `P3_RNG_RELEVE_600.csv` (600 ticks),
  `P3_RNG_RELEVE_JOUR.md` + `P3_RNG_RELEVE_5400.csv` (un jour)
- `tools/migration/ported-functions.mjs` (`goalNoise` dans `npc.js`), `Source/AnastasisSim/PORTAGE.md`,
  `docs/migration/phase2/P2_INVENTAIRE_JS.md` (régénéré)
- `docs/unreal/handoffs/sim-rng-001.md`

Non touchés : `Harness/*`, `AnastasisJsSave`, `AnastasisHarnessTrace`, le bloc des besoins de
`UpdateNpc`, `FNpc.Phenotype`, `FNpc.Conditioning`, les scénarios, `masks.mjs`,
`known-expected-failures.txt`, les `.inl` existants.

## COMMIT

Le commit qui porte cette fiche sur `agent/sim-rng-001`.

## LE FLUX — ce qu'il remplace, ce qui tire dessus

- Accesseurs : **`AnastasisVillage::FVillage::GetSimRngState()`** et **`SetSimRngState(uint32)`**. C'est ce
  qu'il faut brancher sur `save.rng` dans le lecteur (`sim.rng.setState(data.rng)`, save.js l. 168).
- Ils ne remplacent rien : `VillageRng` EST déjà `this.rng = makeRng(this.seed)` (simulation.js l. 1022).
  L'hôte le sème avec la graine de la simulation (`FAnastasisSimulation::Reset` / `ResetFromWorld` →
  `SetRngSeed(Seed)`), comme la référence. Il manquait seulement de pouvoir lire et poser son état.
  Cet usage n'est pas changé, et aucun test `Village.*` ne bouge.
- Ce qui tire dessus côté C++ aujourd'hui : `createInformResourceSpotActs` (speechActs.js l. 62), dans
  `FVillage::ExchangeSpotRumors` et `SocializeWithCompanion`. C'est tout.

## SITES DE TIRAGE `sim.rng` — branchés / non branchés

Mesurés sur le scénario `endurance` (1 jour, 5 400 ticks : 1 854 tirages, 104 décisions) :

| Site (référence) | Tirages / jour | C++ |
| --- | ---: | --- |
| `goalNoise` npc.js:491, table `adultScores` (14 par décision) | 1 456 | **porté, non branché** (`AnastasisGoalNoise`) |
| `exploreTarget` memory.js:669/670 (via `failureTargetBiasMap` > `failureCauseForGoal`, ligne explore, AVANT la table ; et `redirectAfterFailure`) | 254 | non porté → perception-explore-001 |
| `updateNpc` npc.js:893, `sim.rng() < chance` (seulement si l'habitant a une cible) | 75 | non branché (écart n°2 : pas de reconsidération) |
| `rollCraftMiss` craftMiss.js:79 (`act` > `progressTendWork`) | 43 | non branché (rate de coup, écart n°11) |
| `createInformResourceSpotActs` speechActs.js:62 | 10 | **branché** (`VillageRng`, rumeurs) |
| `tellPerson` socialMemory.js:449 | 8 | non porté |
| `assignDayIntent` dayIntent.js:305 | 5 | non porté (intention du jour) |
| `maybeChatOnHaul` npc.js:5505 (`deliver`) | 2 | non porté |
| `refreshColonyStockReport` colonyStockReport.js:149 | 1 | non porté |

Sites demandés et non atteints par le scénario : npc.js:4096 (`confront`, il faut un rival à 2,2 cases)
et npc.js:4462 (`begForFood`, il faut un donneur à moins de 5 cases et pas de refus de principe). Aucun
des deux n'est atteint par une boucle C++ portée : hors mission.

`ensureNeeds(npc, sim.rng)` (npc.js:813) : il ne tire que si un mètre manque (`== null`), dans l'ordre
social, leisure, hygiene, thirst, health. Il ne tire jamais dans le scénario. Documenté, pas branché.

Premier tirage au tick 32 (où `rng` et `buildings` divergent) : `npc-2`, état avant 2576143622,
`exploreTarget src/ai/memory.js:669`, chemin `updateNpc > … > chooseGoal > scoreGoals > adultScores >
failureTargetBiasMap > failureCauseForGoal > exploreTarget`. 16 tirages à ce tick : exploreTarget x2,
goalNoise x14.

## MEC

- BUILD : `tools\unreal\anastasis-unreal.ps1 build` → `BUILD::PASS`.
- Relevé : `node tools/migration/trace-sim-rng.mjs -ref <clone> -ticks 600`, puis
  `-ticks 5400 -md docs/migration/phase3/P3_RNG_RELEVE_JOUR.md`.
- Vecteurs : `node tools/migration/gen-goal-noise-vectors.mjs -ref <clone>` : 70 vecteurs purs, 34 appels
  `goalNoise` lus dans la source, et 104 décisions mesurées (1 456 bruits, tous contigus après 2 à 8
  tirages d'`exploreTarget`).
- TESTS cibles : `report-tests.ps1 -Filter "Anastasis.Sim.Parite.BruitDeBut+Anastasis.Sim.Village.FluxSimRng"`
  → PASS 2, KNOWN_EXPECTED_FAILURE 0, FAIL 0 ; 1 717 valeurs comparées, 104 décisions mesurées rejouées,
  0 écart.
- MUTATION (posée, testée, retirée) : `relax` et `drink` interverties dans la table C++ → **détectée**
  (`decision tick 32 npc-2 : la table tire drink (l. 1118) en position 1, la référence tirait la ligne 1116`).

## PROOFS

PROOFS: (aucune)

La preuve est faite par des tests d'automation (`Parite.BruitDeBut`, `Village.FluxSimRng`), que la suite
du lot rejoue.

## SCN

Aucune scène, aucun asset.

## PLY

Sans objet.

## INTEGRATION_RISK

- `AnastasisVillage.h` : deux accesseurs en ligne et du commentaire. Fusion texte possible avec
  needs-wiring-001 (écart n°19, champs de `FNpc`), à des endroits différents du fichier.
- `PORTAGE.md`, `ported-functions.mjs`, `P2_INVENTAIRE_JS.md` : inventaire régénéré en dernier, sur la
  version de `main`.

## STOP

- Ne revendique aucun recul de divergence : rien n'est branché dans la décision.
- `exploreTarget`, `npc.mind.cells`, intention du jour, ambition, reconsidération (l. 893), rate de coup :
  non portés.
