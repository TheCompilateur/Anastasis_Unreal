# Premier rapport JS / Unreal — scénario `endurance`

Mission `sim-digest-emitter-001`, fin du jalon A (`P3_PLAN.md` §3) : le harnais compare pour la
première fois une trace de la **référence JS** et une trace du **C++**, tick par tick, sur le même
état de départ. Ce document archive le rapport tel qu'il est sorti, et ce qu'il ordonne.

Discipline de claim : `OBS` observé · `EVD` mesuré · `INF` déduit · `DEC` décidé.

## Comment il a été produit

```bash
# JS — référence = clone propre du tag anastasis-ref-p3 (REFERENCE_JS.md)
node tools/migration/emit-state-digests.mjs -ref <tag> -scenario tools/migration/scenarios/endurance.json -days 3 -out js.jsonl

# Unreal — test d'automation, trace dans Saved/HarnessTraces/endurance-unreal.jsonl
#   ANASTASIS_HARNESS_TICKS=16200 ANASTASIS_HARNESS_DRILL=1
tools\unreal\report-tests.ps1 -Filter Anastasis.Sim.Harnais

node tools/migration/compare-digests.mjs js.jsonl Saved/HarnessTraces/endurance-unreal.jsonl
```

Côté Unreal (`Harness/AnastasisHarnessTrace.h`) : le scénario est lu (`Harness/AnastasisJsSave.h`), repris
par l'hôte — monde fourni et horloge de la sauvegarde (`FAnastasisSimulation::ResetFromWorld`), bâtiments
avec leurs identifiants et leurs seuils sauvegardés, habitants, registre des repas
(`FVillage::RestoreForHarness`) — puis tourne. À chaque tick, l'état **vivant** du C++ est projeté sur
les 10 sections du périmètre et haché comme `digestState`. L'en-tête porte le scénario, son empreinte
et ses 24 masques tels que le fichier les donne : côté JS ce sont des systèmes neutralisés, côté C++
des systèmes qui n'existent pas.

Préalable prouvé par `Anastasis.Sim.Harnais.Trace` : **au tick 0, après reprise par l'hôte, les 10
sections ont l'empreinte de la référence** — la reprise ne perd rien de ce qui est lu. Deux exécutions
C++ rendent les mêmes bits.

## Le rapport `EVD`

3 jours (16 200 ticks à `dt = 1/60`), une empreinte à chaque tick, 2026-10-01 :

```
RAPPORT DE DIVERGENCE
=====================

A : js-endurance-3j.jsonl
    source=js graine=12345 dt=0.016666666666666666 ticks=16200 every=1 ref=fee66ae
    scenario=endurance empreinte=a7c317b02da7be0f masques=24
B : Saved/HarnessTraces/endurance-unreal.jsonl
    source=unreal graine=12345 dt=0.016666666666666666 ticks=16200 every=1 ref=fee66ae
    scenario=endurance empreinte=a7c317b02da7be0f masques=24

Ticks compares : 16201 (de 0 a 16200)
Sections jugees (10) [perimetre du scenario] : seed, rng, w, h, time, day, tileDiff, buildings, actors, mealReservations
Sections ignorees (25), sans verdict : animalState, animals, colony, districtLandmarks, economy, …

PREMIER TICK DIVERGENT : 1  (jour 1, temps 37.81666666666666)
La divergence persiste jusqu'a la fin de la trace.

Sections divergentes a ce tick (1) :
  actors                 A=c6f14a5e4d750a09  B=8b3a4d5cb53808f9

Premiere divergence par section — l'ordre est l'ordre de travail :
  tick        1  actors
  tick      120  buildings
  tick      120  rng
  tick      133  mealReservations
  tick      298  tileDiff

Sections jugees restees identiques sur toute la trace (5) : day, h, seed, time, w
```

**L'horloge est en parité** : `time` et `day` identiques sur les 16 201 ticks, trois minuits compris.

## Le forage au tick 1 `EVD`

```bash
node tools/migration/emit-state-digests.mjs -ref <tag> -scenario …/endurance.json -ticks 1 -dump 1 -dump-out js.tick1.json
node tools/migration/diff-states.mjs js.tick1.json Saved/HarnessTraces/endurance-unreal.tick1.json
```

40 champs diffèrent, tous dans `actors`, et ce sont les mêmes pour les cinq habitants :

| Champ | JS (A) | Unreal (B) |
|---|---|---|
| `hunger` (npc-0) | 10 | 10.009666666666666 |
| `thirst` | 10 | 10.008 |
| `energy` | 85 | 84.99133333333333 |
| `social` / `leisure` / `hygiene` | 80 / 80 / 80 | 79.9957 / 79.997 / 79.996 |
| `health` | 95 | 95.00233333333334 |
| `_simBudgetAccum` | 0.016666666666666666 | (absent) |

Au premier pas, **le JS n'a pas fait évoluer les besoins** ; il a seulement accumulé `dt` dans
`_simBudgetAccum`. Le C++ les a avancés d'un pas.

## Ce que le rapport ordonne

**Cause `OBS` (src/sim/npc.js, `updateNpc`)** : la première instruction est
`consumeNpcSimulationCadence(sim, npc, dt)` ; tant que la cadence ne rend pas `run`, l'habitant ne fait
rien d'autre. La cadence dépend de la **bande** de l'habitant, c'est-à-dire de sa distance à la vue du
budget (`simulationBudget.view`). Dans le harnais, sans caméra, la vue est à `(0, 0)` ; le village est à
environ 76 cases : bande *far* (rayon 84), **1 Hz**. Les habitants JS pensent une fois par seconde de
simulation, avec un `dt` accumulé ; les habitants C++ à chaque tick.

Le C++ a porté ce mécanisme (`Core/AnastasisSimBudget.h` : `ClassifyNpcBand`, `IntervalForBand`,
`ConsumeCadence`, en parité bit à bit, couche 3) mais **ne le branche pas** dans `FVillage::UpdateActors`.

**Premier ordre de travail `DEC` à prendre** : brancher `ConsumeCadence` en tête de la mise à jour d'un
habitant, avec `_simBudgetAccum` dans `FNpc`, la vue du budget et sa bande — et ajouter
`_simBudgetAccum` aux champs lus par le lecteur. C'est du ressort de `nav-service-001` (vagues 2 et 3 :
`logicalLod`, budget) ou d'une mission courte dédiée, avant elle. Deux autres choses à savoir :

- `ensureLifestyle`, `ensureNeeds`, `ensureCulture` sont appelés AVANT la cadence, avec `sim.rng` :
  à vérifier au moment du branchement (premier tirage `rng` au tick 120 côté JS `OBS`).
- La vue `(0, 0)` est un artefact du harnais (pas de caméra), pas un choix de la référence. La référence
  la pose depuis la caméra du joueur. `INF` : un scénario pourrait épingler la vue (`pinSimulationView`) —
  mais `simulationBudget` n'est pas dans `serialize`, donc ce serait un masque ou un champ de scénario.
  À trancher avec le branchement, pas avant.

Les sections suivantes (`buildings` et `rng` au tick 120, `mealReservations` au tick 133, `tileDiff` au
tick 298) se jugeront **après** : tant que `actors` diverge au tick 1, elles divergent par conséquence.

## Ce que ce rapport ne dit pas

- Que le reste du portage diverge ou non : tout ce qui suit le tick 1 est masqué par la cadence.
- Les champs d'habitant que le lecteur ne lit pas (`mind`, `relations`, `deeds`…) sont **recopiés** du
  départ côté C++ : figés. Ils divergeront dès que le JS les fait bouger ; ce n'est pas une divergence
  de la simulation C++ mais une absence de lecture (STOP de `sim-state-reader-001`).
- `rng` côté C++ est l'état lu, figé : le C++ n'a pas de flux `sim.rng` (écart n° 16). La section ne
  convergera qu'avec `goal-noise-001`.

---

## Rapport 2 — la cadence branchée, la vue épinglée sur le village (budget-cadence-001)

Décision d'Alexandre (2026-10-01) : **la vue du budget est épinglée sur le village**, le cas du jeu
(option B). Le scénario passe au format 2 avec un champ `vue` = `settlement` de la sauvegarde,
(54, 57), compris dans son empreinte (`52f66b01c0766137`) et appliqué des deux côtés :
`pinSimulationView` après `deserialize` côté JS, `FVillage::SetSimulationView` côté C++. La cadence est
branchée en tête de `FVillage::UpdateNpc` dès qu'une vue est posée (écart n° 5 réécrit : le reste est
« sans vue »).

Mesure indépendante, session `labo-ecarts-001` (`docs/migration/ecarts/n05.md`, N = 40 répliques)
`EVD` : avec la vue en (0, 0), la référence elle-même se comporte autrement (épisodes `helpFarm`
46 → 11 s, temps dedans 0,38 → 0,55) parce qu'en bande *far* elle reconsidère ses buts 8 fois moins ;
vue épinglée sur le village, elle rejoint le profil d'un village regardé. Le choix B est le bon cas.

`Anastasis.Sim` : 94 PASS / 2 KNOWN_EXPECTED_FAILURE / 0 FAIL (dont `Anastasis.Sim.Village.Cadence` :
near à chaque tick ; medium accumule `_simBudgetAccum` et tourne au 6e tick avec 0,1 s ; sans vue,
inchangé).

### Le rapport `EVD`

```
PREMIER TICK DIVERGENT : 1  (jour 1, temps 37.81666666666666)
Sections divergentes a ce tick (1) :
  actors                 A=4cc03c8b776700c0  B=8b3a4d5cb53808f9
Premiere divergence par section — l'ordre est l'ordre de travail :
  tick        1  actors
  tick       32  buildings
  tick       32  rng
  tick      133  mealReservations
  tick      257  tileDiff
Sections jugees restees identiques sur toute la trace (5) : day, h, seed, time, w
```

Le premier tick divergent reste **1**, mais ce n'est plus la cadence. L'empreinte C++ au tick 1 est la
même qu'au rapport 1 (les habitants sont en *near* : ils tournaient déjà à chaque tick) ; c'est le JS
qui a changé : ses habitants tournent eux aussi au tick 1.

### Le forage au tick 1 `EVD`

L'état C++ au tick 1 a été reconstruit depuis le forage du rapport 1 (même empreinte : reconstruction
vérifiée, `8b3a4d5cb53808f9`). 45 champs diffèrent, en deux familles :

| Famille | Champs | Cause `INF` |
|---|---|---|
| Besoins **personnels** | `hunger`, `thirst`, `energy` (par ex. npc-0 : faim 10,011328 contre 10,009667) | `tickNeeds` applique à chaque habitant `metabolicDemandFactor`, `hydrationLossFactor`, `fatigueRecoveryFactor` (génome, mode de vie, conditionnement), que `needs.js` garde « à porter » dans l'inventaire. `health`, `hygiene`, `leisure`, `social` sont, eux, **identiques**. |
| Champs non tenus ou non projetés | `workTimer` (JS += dt, C++ 0) ; `aiThinkAt`, `villagePhase` (tenus par le C++, non projetés par le lecteur) ; `placeMemory` ; `lifestyle.lastNotedDay` | Lecteur et portage incomplets (STOP de `sim-state-reader-001`). |

**Ordres de travail, dans l'ordre** :
1. Les facteurs personnels des besoins (`needs.js` : `metabolicDemandFactor`, `hydrationLossFactor`,
   `fatigueRecoveryFactor`, `fatigueAdaptationFactor`, `recoveryConditioningFactor`) avec les champs qu'ils
   lisent (génome, mode de vie, conditionnement) — par le lecteur d'abord, le portage ensuite.
2. Le lecteur : projeter `aiThinkAt` et `villagePhase` (le C++ les tient déjà), suivre `workTimer`.
3. `placeMemory` et `lifestyleDailyUpdate` : petits, mais à chaque tick.
