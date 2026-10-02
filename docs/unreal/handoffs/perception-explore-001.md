# HANDOFF: perception-explore-001

## MISSION

Commandée par « Simulateur IV Kingdoms migration phase 3 », découpage approuvé le 2026-10-01 :
- porter la mémoire des régions (`npc.mind.cells`, écrite par `perceive`), `exploreTarget` et
  `randomWalkTarget` ;
- brancher, dans la décision adulte du village, les tirages `sim.rng` dans l'ordre de la référence : la
  préparation (`failureTargetBiasMap` > `exploreTarget`), puis les 14 `goalNoise` et leurs 3
  conditionnels.

Ce qui n'est pas branché (l. 893, rate de coup, intention du jour) est déclaré dans les écarts. La branche
est posée sur `agent/sim-rng-001` (3b14d27, rebasée sur `main` 2388bb1), qui n'est pas encore dans `main`.

## FILES_OWNED

- `Source/AnastasisSim/Public/World/AnastasisExplore.h`, `Private/World/AnastasisExplore.cpp` (nouveaux)
- `Source/AnastasisSim/Public/Village/AnastasisVillage.h` : `FNpc::KnownCells` / `CellCount`,
  `FDecisionTrace::ExploreDraws` / `NoiseDraws` / `RowNoise`, `FVillage::ChooseGoalNow`, et en privé
  `ExploreTargetFor`, `ExploreWorld`, `NoiseConditionHolds` ; un argument `Noise` sur `SocialRowScore`,
  `WorkRowScore` et `BuildRowScore`
- `Source/AnastasisSim/Private/Village/AnastasisVillage.cpp` : `Perceive` (markCell), `ChooseGoal` (tirages),
  `RedirectAfterFailure` (exploreTarget si le but raté est `explore`), lignes de score (bruit)
- `Source/AnastasisSim/Private/Tests/AnastasisExploreTests.cpp`, `AnastasisExploreVectors.inl` (nouveaux)
- Tests adaptés au bruit, formule exacte conservée : `AnastasisVillageSimTests.cpp` (ligne drink),
  `AnastasisVillageBuildTests.cpp` (ligne build), `AnastasisVillageGatherTests.cpp` (ligne gatherFood)
- `tools/migration/gen-explore-vectors.mjs` (nouveau), `tools/migration/rng-trace-lib.mjs` (photo de
  l'habitant à chaque décision ; le relevé est inchangé, vérifié ligne à ligne)
- `Source/AnastasisSim/ECARTS.md` (n° 1, 2, 11 ; n° 24 nouveau, n° 20 à 23 pris par player-goals-001), `PORTAGE.md`,
  `tools/migration/ported-functions.mjs`, `docs/migration/phase2/P2_INVENTAIRE_JS.md` (régénéré)

Non touchés : `Harness/*`, `AnastasisJsSave`, `AnastasisHarnessTrace`, le début d'`UpdateNpc` (cadence) et le
bloc des besoins, `FNpc.Phenotype` / `Conditioning` / `Lifestyle`, les scénarios, `masks.mjs`,
`known-expected-failures.txt`, les `.inl` existants.

## COMMIT

Le commit qui porte cette fiche sur `agent/perception-explore-001`.

## POUR LE LECTEUR — `mind.cells`

- JS : `npc.mind.cells` est un objet `{ "<index>": 1, … }` ; `npc.mind.cellCount` est un nombre.
- `index = Math.floor(y / 8) * Math.ceil(sim.w / 8) + Math.floor(x / 8)`, sur les coordonnées
  ARRONDIES de l'habitant (`Math.floor(npc.x)`). Une clé hors du monde est supprimée par
  `repairMindReferences`.
- C++ : **`FNpc::KnownCells`** (`TSet<int32>`, les index) et **`FNpc::CellCount`** (`int32`).
- Projection : un objet JS à clés entières s'itère dans l'ordre NUMÉRIQUE croissant. Projeter
  `KnownCells` triées croissantes, valeur `1`.
- Jamais oubliées ; `perceive` en marque une à chaque balayage (porte `scanInterval`).

## TIRAGES `sim.rng` DE LA DÉCISION — branchés / non branchés

Ordre dans `chooseGoal` (adulte) d'après le relevé (`docs/migration/phase3/P3_RNG_RELEVE_JOUR.md`) :

| Étape (référence) | C++ |
| --- | --- |
| `ensureDayIntent` > `assignDayIntent` (+ cap si `explore`) | **non** : écart n° 24 (5 par jour, au changement de jour) |
| `prepareAdultGoalContext` > `assignAmbition` si pas d'ambition | **non** : écart n° 24 |
| `spatialRiskBiasMap` > `recallOrSearch` > `exploreTarget` | **non** : 0 tirage sur un jour, écart n° 24 |
| `failureTargetBiasMap` > `failureCauseForGoal("explore")` > `intentExploreHint` \|\| `exploreTarget` | **oui** pour `exploreTarget` (`ExploreTargetFor`) ; `intentExploreHint` non porté (n° 24) |
| table : 14 `goalNoise` inconditionnels + 3 conditionnels (`helpFarm`, `craft`, `visitFamily`) | **oui** (`Ai/AnastasisGoalNoise.h`) ; famille : toujours faux, la famille n'est pas portée (n° 8) |
| `assignTarget` > `recallOrSearch` / `exploreTarget` (gather, explore) | **non** : ne tire pas dans le scénario ; `GatherTarget` sans gisement vaque (n° 11) |

Hors décision :
- l. 893 : **non**, écart n° 2, avec lifestyle-wiring-001 ;
- `rollCraftMiss` : **non**, écart n° 11 ;
- `redirectAfterFailure` > `exploreTarget` : **oui**, quand le but raté est `explore` ; le C++ ne porte pas ce
  but, donc ce tirage n'arrive pas aujourd'hui ;
- `tellPerson`, `maybeChatOnHaul`, `refreshColonyStockReport` : **non**, inchangé.

Le bruit d'une ligne NON portée (plancher 42) est tiré mais pas ajouté : ajouté à un score inventé, il
faisait gagner `observer` au hasard (`Village.Endurance` le voyait). Les lignes portées (drink, relax,
socialize, shelterRain, build sur chantier, gatherFood du fermier) reçoivent leur bruit à la place que la
référence lui donne dans la somme.

## MEC

- BUILD : `tools\unreal\anastasis-unreal.ps1 build` → `BUILD::PASS`.
- Vecteurs : `node tools/migration/gen-explore-vectors.mjs -ref <clone anastasis-ref-p3>` : 188 cas
  `exploreTarget` (64 en promenade, 4 au centre du village), 48 `randomWalkTarget`, 104 décisions mesurées
  (toutes contiguës ; 5 contiennent aussi `assignDayIntent` ou `refreshColonyStockReport`, avant le bloc porté).
- TESTS (`report-tests.ps1 -Filter Anastasis.Sim`), après rebase sur `main` 2388bb1 (needs-wiring-001 compris) :
  **PASS 115, KNOWN_EXPECTED_FAILURE 2** (`Parite.Fbm`, `Parite.SemantiqueJs`), **FAIL 0**, 117/117.
  - `Anastasis.Sim.Parite.Exploration` : 1 185 valeurs, 0 écart.
  - `Anastasis.Sim.Village.TiragesDecision` : les 104 décisions mesurées sont reprises sur le village du
    harnais (scénario `endurance` relu, photo de l'habitant posée, flux posé) et rejouées par la décision
    C++ : 0 fausse. Même nombre de tirages d'`exploreTarget`, même nombre de bruits, même état du flux après.
  - `Anastasis.Sim.Parite.BruitDeBut` (sim-rng-001) : toujours 0 écart.
  - Premier run : 5 FAIL. Trois tests de ligne (drink, build, gatherFood) recalculaient la formule sans
    bruit : formule mise à jour, exacte. `Village.Endurance` (« chaque jour quelqu'un mange ») : le bruit
    ajouté au plancher 42, corrigé (voir plus haut). `Parite.Recolte` : erreur de pilote audio
    (`LogAudioMixer`), passé au run suivant sans changement.
- MUTATION (posée, testée, retirée) : `exploreTarget` ignore les régions connues (`if (false && KnownCells…)`)
  → **détectée** par les deux tests : `Parite.Exploration` (`exploreTarget[1] moitie : 14 tirages, attendu 22`)
  et `Village.TiragesDecision` (`tick 453 npc-3 : exploreTarget 6 / 8`).

## ECARTS

- modifié : n° 1 — bruits branchés (préparation `exploreTarget` puis table), plancher 42 et `observer`
  restent ; le bruit d'une ligne non portée est tiré, pas ajouté.
- modifié : n° 2 — le tirage l. 893 est décrit (75 par jour, premier au tick 165) ; fermeture :
  lifestyle-wiring-001 pour le tirage, goal-noise-001 pour le collant.
- modifié : n° 11 — `exploreTarget` porté, pas encore appelé par `GatherTarget` ; `rollCraftMiss` non tiré
  (43 par jour).
- ouvert : n° 24 — Intention du jour, ambition, cibles de risque : leurs tirages ne sont pas faits
  (A_FERMER, goals-day-intent, goals-resources-001).

## PROOFS

PROOFS: (aucune)

Preuve par tests d'automation (`Parite.Exploration`, `Village.TiragesDecision`), rejoués par la suite du lot.

## SCN

Aucune scène, aucun asset.

## PLY

Sans objet.

## INTEGRATION_RISK

- Dépend de `agent/sim-rng-001` (3b14d27) : à verser après elle, ou dans le même lot.
- `AnastasisVillage.h/.cpp` : `ChooseGoal`, `Perceive`, les lignes de score et `FNpc` (deux champs après
  `LastScan`). Fusion probable avec lifestyle-wiring et needs-wiring (autres endroits de `UpdateNpc` et de
  `FNpc`).
- Le comportement du village change : les lignes portées reçoivent leur bruit, donc des décisions
  basculent. La suite `Anastasis.Sim` est verte.

## STOP

- Ne revendique pas la parité du harnais au-delà du tick 165 : l. 893 et rate de coup non tirés.
- `intentExploreHint`, `assignDayIntent`, `assignAmbition`, `spatialRiskBiasMap`, l'exploration de récolte : non
  portés.
