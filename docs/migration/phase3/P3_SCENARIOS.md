# Scénarios et masques du harnais différentiel

Mission `sim-scenario-001`, jalon A de `P3_PLAN.md` (§2, §3). Le harnais
(`P2_HARNAIS_DIFFERENTIEL.md`) comparait deux exécutions de la **genèse complète** de la référence :
fondateurs romains, sagas, économie, Kosmos. Le C++ ne sait pas construire cet état. Un scénario
donne aux deux côtés un **état de départ commun et réduit**, et un **périmètre déclaré** : ce qui
est comparé, ce qui est masqué, ce qui ne peut pas l'être.

Discipline de claim : `OBS` observé · `EVD` mesuré · `INF` déduit · `DEC` décidé · `UNK` inconnu.

Référence : tag `anastasis-ref-p3` = `fee66ae` (`REFERENCE_JS.md`). Tous les outils prennent
`-ref <checkout propre du tag>` ; avec un scénario, l'émetteur **refuse** une référence modifiée ou
un autre commit que celui du scénario.

---

## 1. Un scénario est une sauvegarde JS `DEC`

Décision 2 de `P2_MODELE_DONNEES.md` : le harnais lit l'état JS. Un scénario est donc la sortie de
`serialize` (`src/sim/save.js`), plus ce qu'il faut pour le rejouer et le juger. Le C++ la lira
(`sim-state-reader-001`) ; l'émetteur JS la charge par `deserialize`.

```bash
node tools/migration/build-scenario.mjs -ref <tag> -scenario endurance -out tools/migration/scenarios/endurance.json
```

La construction n'utilise que l'API publique de la référence :

1. `new Simulation({ deferred: true, seed })` puis `resetWorldBase(seed, { pickSite: false })` :
   le monde de la graine, sans site, sans habitant, sans faune, sans camp ;
2. `serialize` puis `deserialize` : le monde **tel qu'une sauvegarde le décrit** (`deserialize` pose
   la couronne du village autour de `settlement` et retire les champs de la clairière) ;
3. la recette (`tools/migration/scenarios/<nom>.mjs`) : `addBuilding`, `spawnNpc`, `findPath`… ;
4. `serialize` → le fichier.

**Point fixe.** L'état écrit doit se recharger puis se resauver à l'identique, sinon le tick 0 de
l'émetteur (qui charge par `deserialize`) ne serait pas l'état du fichier, et le lecteur C++ serait
jugé contre un état que personne n'a écrit. Le constructeur itère l'aller-retour jusqu'au point fixe
(refus après 4). Pour `endurance` `EVD` : 2 allers-retours ; au premier, `actors` et `buildings`
changent parce que `deserialize` complète des valeurs par défaut — `trafficTimer: 0` et
`inside: null` sur chaque habitant, `yardFood: 0` sur le grenier. Le fichier porte l'état complété.

### Le format

| Champ | |
|---|---|
| `kind`, `format` | `"anastasis-scenario"`, `1` |
| `name` | nom de la recette |
| `reference` | `{ tag, commit, modifie }` du dépôt JS de construction |
| `seed`, `dt` | graine et pas de temps de la trace |
| `vue` | format 2 : `{ x, y }`, la vue du budget épinglée des deux côtés (`pinSimulationView`) ; `endurance` : le `settlement`, (54, 57). Sans elle, la vue reste en (0, 0) et le village est simulé « de loin » (bande *far*, 1 Hz). Voir `P3_PREMIER_RAPPORT.md`, rapport 2 |
| `dayDeferred` | `"tick"` : 2 travaux de minuit par tick (`processDayDeferred`), comme le jeu et le C++ ; `"flush"` : file vidée après chaque tick (trace sans scénario) |
| `sections` | **périmètre** : sections de `serialize` que le harnais juge |
| `masques` | identifiants du registre `tools/migration/scenarios/masks.mjs` |
| `masquesDetail` | par masque : technique, cible, ce que le C++ porte, mission qui le retirera, **tirages `sim.rng`** mesurés |
| `etatVide`, `nonMasquables` | ce qui est neutralisé par l'état, ce qui ne peut pas l'être |
| `auditRng` | tirages par étape du tick, masques levés puis posés |
| `pointFixe`, `recette` | l'aller-retour, et les positions et paramètres obtenus |
| `empreinte` | FNV-1a 64 (hacheur du harnais, `state-digest.mjs`) de `format, name, seed, dt, dayDeferred, sections, masques, save` |
| `save` | la sauvegarde JS |

L'émetteur recalcule l'empreinte au chargement et **refuse** un fichier modifié à la main ou périmé.

## 2. Le scénario `endurance`

Recette du test C++ `Anastasis.Sim.Village.Endurance` (`AnastasisVillageEnduranceTests.cpp`) :
graine 12345, champ généré le plus proche du centre ; grenier à Chebyshev 3-4, puits à 5-8, maison
à 5-9, chacun sur la première case libre dont une porte rejoint le champ ; deux fermiers au grenier
(faim 10, 15), trois sans-métier (faim 20, 30, 40), à la première porte du grenier ; énergie 85,
social 80, loisir 80, hygiène 80, soif 10, santé 95, moral 60 ; `dt = 1/60` ; départ à
`DAY_LENGTH × 0.42` du jour 1.

**Une différence imposée, pas choisie `OBS`.** Le C++ génère un monde 96 × 96
(`Sim.Reset(12345u, 96, 96)`). Une sauvegarde JS ne peut pas porter cette taille : `deserialize`
**lève** si `(w, h)` n'est pas `resolveWorldExtent(graine)`, soit **108 × 114** pour 12345. Le
scénario vit donc sur le monde que la sauvegarde décrit (graine, couronne, `tileDiff`), et le
« centre » est celui de ce monde. C'est ce monde que `sim-state-reader-001` devra reconstruire ; le
test C++ d'endurance, lui, reste sur son monde.

Ce que la recette a posé `EVD` (`recette` du fichier) :

| | |
|---|---|
| Monde | 108 × 114, centre (54, 57) ; `tileDiff` : 590 cases |
| Champ | (50, 62), 11 portions, `fruit` |
| Grenier `building-0` | (47, 59), 3 portes ; première porte (48.5, 59.5) |
| Puits `building-1` | (48, 57), 4 portes |
| Maison `building-2` | (49, 57), 2 portes |
| `npc-0`, `npc-1` | fermiers, poste `building-0` (vérifié à la construction) |
| `npc-2` à `npc-4` | sans-métier |

Périmètre : `seed, rng, w, h, time, day, tileDiff, buildings, actors, mealReservations` — ce que
le premier lecteur C++ projettera (`P3_PLAN.md` §3). Empreinte `a7c317b02da7be0f` au format 1 ;
**`52f66b01c0766137` au format 2** (vue épinglée, budget-cadence-001). Les chiffres d'audit
ci-dessous sont ceux du format 1 (vue en (0, 0)) ; le fichier `endurance.json` porte ceux du format 2.

## 3. Les masques

### Trois techniques, et pourquoi trois

Le plan laissait la question ouverte (`P3_PLAN.md` §9) : les systèmes du tick sont-ils masquables
de l'extérieur ? Réponse `OBS` : **la moitié seulement, par une méthode**.

| Technique | Quand | Comment |
|---|---|---|
| `methode` | le système est une méthode de `Simulation` (`onNewDay`, `separateCrowdedActors`) | propriété propre de l'instance : elle masque celle du prototype, le tick de référence l'appelle sans le savoir |
| `tick-recompose` | le système est une **fonction importée** par `simulation.js` (`tickTransport`, `updateAnimalsTick`, `tickSagas`, `tickLogicalVillageLod`, `resolveUneBouchePlus`…) : une liaison d'import ES ne se remplace pas | `sim.tick` remplacé par `tickRecompose` (`masks.mjs`) : la même suite d'étapes, dans le même ordre, appelées depuis les mêmes exports de la référence ; une étape masquée est sautée |
| `file-de-minuit` | un des 17 travaux de `enqueueDayDeferred` | on appelle l'`enqueueDayDeferred` de la référence, puis on remplace le `run` des travaux masqués. Le travail garde sa place : le C++ tient aussi la file à 17, 2 par tick. Si la file de la référence n'a plus ces 17 travaux dans cet ordre, l'émetteur lève au lieu de masquer à côté |

`tickRecompose` n'est légitime que s'il **est** le tick de référence hors des étapes masquées. C'est
le cas 10 de l'autotest : sans masque, tick recomposé contre tick de référence, 3 jours, empreinte
globale (les 35 sections) comparée à chaque tick — **identiques** (§6). Seule différence voulue : les
spans `obs` (chronométrage de débogage, état de module) ne sont pas ouverts.

### Les 24 masques d'`endurance`

Tirages `sim.rng` : mesurés sur 3 jours (16 200 ticks), scénario chargé, masques **levés**
(`auditRng.sansMasques`) — ce que le système tirerait s'il tournait. Avec les masques, ils tombent
tous à 0 `EVD`.

| Masque | Technique | Le C++ porte | Retiré par | Tirages / 3 j |
|---|---|---|---|---:|
| `kosmos` | tick-recompose | non | `sagas-kosmos-001` | 0 |
| `sagas` | tick-recompose | non | `sagas-kosmos-001` | 0 |
| `urbanIntent` | tick-recompose | non | `urban-001` | 0 |
| `lodLogique` | tick-recompose | non | `nav-service-001` | 0 |
| `separationFoule` | methode | non | `nav-service-001` | 0 |
| `animaux` | tick-recompose | non | `animals-001` | 0 |
| `transport` | tick-recompose | non | `transport-001` / `goals-haul-001` | 0 |
| `economieJournaliere` | methode | `assignSheltersDaily` seulement | `day-critical-001` | **36** |
| `minuit.collective` | file-de-minuit | non | `day-deferred-001` | 0 |
| `minuit.socialOrders` | file-de-minuit | non | `day-deferred-001` | 0 |
| `minuit.colonyDoctrine` | file-de-minuit | non | `day-deferred-001` | 0 |
| `minuit.growthChapter` | file-de-minuit | non | `day-deferred-001` | 0 |
| `minuit.founderCharter` | file-de-minuit | non | `day-deferred-001` | 0 |
| `minuit.colonySites` | file-de-minuit | non | `day-deferred-001` | 0 |
| `minuit.transportProjects` | file-de-minuit | non | `day-deferred-001` | 0 |
| `minuit.roadEvolution` | file-de-minuit | non | `day-deferred-001` | 0 |
| `minuit.watchPosts` | file-de-minuit | non | `day-deferred-001` | 0 |
| `minuit.lifeDaily` | file-de-minuit | non | `day-deferred-001` | **48** |
| `minuit.careers` | file-de-minuit | non | `day-deferred-001` | **10** |
| `minuit.founders` | file-de-minuit | non | `day-deferred-001` | 0 |
| `minuit.romanCouncils` | file-de-minuit | non | `day-deferred-001` | 0 |
| `minuit.memory.partiel` | file-de-minuit | `forgetStale` + `forgetStalePeople` | `day-deferred-001` | **16** |
| `minuit.animalsDaily` | file-de-minuit | non | `animals-001` | 0 |
| `minuit.immigration` | file-de-minuit | non | `day-deferred-001` | **3** |

`economieJournaliere` remplace `onNewDay` par ce que le C++ porte : remise à zéro des rations du
jour, `assignSheltersDaily`, `enqueueDayDeferred` (et le `flush` si `defer` est faux). Disparaissent :
`ensureWorldSagas`, visiteurs du marché, déclin du trafic, production, pourriture, exports,
`rebuildMarketAggregate`, entretien, registre de nourriture, pénuries, achats et agrandissements de
maison, `ensureWorkplacesDaily`, routes, loyers, dividendes, moral, transformation, circulation,
journal du jour, `fadeVillageLogs`, arrivée du prêtre. Un seul masque pour toute la section : elle
est faite d'appels en ligne (pourriture, exports, dividendes) qu'aucune méthode ne couvre. Le
découper est le travail de `day-critical-001`.

`minuit.memory.partiel` garde la part portée (`forgetStale`, `forgetStalePeople` pour chaque
habitant) et masque `fadeEpisodes`, `updateAmbitionsDaily`, `updateDayIntentsDaily` — qui tiraient
les 16 tirages.

**Cinq masques suppriment des tirages : divergence attendue tant qu'ils ne sont pas portés `DEC`.**
Avec les masques, l'audit ne voit plus qu'**un** consommateur de `sim.rng` : `updateNpc`
(5 946 tirages sur 3 jours, masques posés ; 5 593 masques levés, sur une autre trajectoire).

### L'ordre des tirages des systèmes restants

Un masque **retire** des appels ; il n'en ajoute ni n'en déplace aucun. `tickRecompose` reprend
l'ordre du tick de référence, une méthode masquée est appelée là où l'original l'était, un travail
de minuit masqué garde son rang. Les tirages des systèmes restants arrivent donc dans le même ordre
d'appel ; leurs **valeurs** changent dès qu'un système masqué aurait tiré avant eux — c'est la
divergence attendue ci-dessus, pas un défaut du masque. `INF` pour l'ordre (par construction), `EVD`
pour l'absence d'effet hors masque (cas 10).

### Neutralisé par l'état, sans masque

| Système | État |
|---|---|
| animaux | `sim.animals = []` (et `minuit.animalsDaily` masqué n'en ajoute pas) |
| transport | aucune charrette, aucun travail de transport au départ |
| Kosmos | aucune instance `life.kosmos1204.lot0.pending` |
| LOD logique | 5 habitants, sous `LOGICAL_LOD.minPopulation` (18) |
| village | pas de camp, pas de marché, pas de structure de village |

Ces systèmes sont **aussi** masqués : l'état vide garantit qu'ils n'agiraient pas aujourd'hui, le
masque garantit qu'ils n'agiront pas demain si la trajectoire change.

### Non masquables `OBS`

| Système | Pourquoi |
|---|---|
| `navService` : `beginNavTick` / `processNavQueue`, file A*, budget, cache (`navCache` est dans la sauvegarde) | le masquer arrête toute marche : aucune requête de chemin n'est plus servie. Socle de `nav-service-001` ; d'ici là, `actors` diverge dès le premier pas (écarts n° 4 et 5) |
| ce qu'appelle `updateNpc` et que le C++ ne porte pas : table complète, `goalNoise` et reconsidération (`sim.rng`), achat / vente, épisodes, texte des répliques, rumeurs hors gisements, marché de l'emploi interne | `updateNpc` **est** la boucle comparée ; le remplacer, ce serait écrire une autre simulation. Écarts n° 1 à 18, jalon B |
| identifiants du journal (`logs[].id`) | compteur de module, pas état de `sim` : une trace = un processus neuf ; `logs` hors périmètre |

Aucun système n'a été neutralisé en modifiant le dépôt JS.

## 4. L'émetteur et le comparateur

```bash
node tools/migration/emit-state-digests.mjs -ref <tag> -scenario tools/migration/scenarios/endurance.json -days 3 -out a.jsonl
node tools/migration/compare-digests.mjs a.jsonl b.jsonl
node tools/migration/compare-digests.mjs a.jsonl b.jsonl -sections actors,buildings
```

**Émetteur** (`-scenario`) : graine, `dt`, mode de la file de minuit et masques viennent du
scénario. L'en-tête porte `scenario: { name, empreinte, masques, sections }`, le mode du tick
(`reference` / `recompose`), et la provenance de la référence (`refCommit`, `refTag`, `refModifie`).
Options d'instrument : `-sans-masques`, `-tick reference|recompose`, `-audit-rng`,
`-perturb <tick> -perturb-path <chemin>`. Sans `-scenario`, il se comporte comme avant (genèse
complète, file vidée à chaque tick).

**Comparateur** : refuse (sortie 3) deux traces dont le scénario, son empreinte, ses masques ou son
périmètre diffèrent, et une trace de scénario face à une trace sans scénario. Juge le périmètre du
scénario, ou la partie que `-sections` en retient ; refuse une section demandée hors périmètre ou
absente des traces. Le rapport liste les sections jugées et les sections **ignorées, sans verdict** :
une section ignorée qui diverge n'est jamais annoncée. L'empreinte globale `g` (tout l'état) ne sert
que quand tout est jugé.

Une trace d'endurance de 3 jours, une empreinte à chaque tick : ~6 min, 18 Mo `EVD`.

## 5. Ce que le scénario ne dit pas encore

- **Il ne juge rien côté C++.** Il n'y a ni lecteur (`sim-state-reader-001`) ni émetteur Unreal
  (`sim-digest-emitter-001`). Ce document prouve l'instrument JS contre lui-même.
- **Le premier tick divergent JS / Unreal sera très tôt `INF`** : `navService` n'est pas masquable,
  et `updateNpc` consomme `sim.rng` dès le premier tick dans la référence, pas dans le C++ (écarts
  n° 1 et 16). C'est attendu : l'indicateur du jalon A est le périmètre, pas encore les jours.
- L'audit des tirages attribue chaque tirage à l'étape du tick en cours (étiquettes posées par
  `tickRecompose`) ; un module qui aurait gardé une référence à la fonction `rng` d'origine tirerait
  sans être compté. Le comptage pourrait donc sous-estimer, jamais changer la trajectoire : la
  fonction enveloppée tire dans le même générateur.

## 6. L'autotest, rapport réel

```bash
node tools/migration/selftest-harness.mjs -ref <tag> -scenario-days 3 -jobs 7
```

Mesuré le 2026-10-01 `EVD`, référence = clone frais du tag, `-scenario-days 3` (16 200 ticks à
`dt = 1/60`, une empreinte à chaque tick), 7 traces de scénario en parallèle, 8 min 25 s au total :

```
PASS  1. meme graine, deux traces -> aucune divergence
      attendu: null   obtenu: null
PASS  2. un ulp au tick 0 -> divergence annoncee au tick 0
      attendu: 0   obtenu: 0
PASS  3. un ulp sur actors[0].x au tick 40 -> divergence annoncee au tick 40
      attendu: 40   obtenu: 40
PASS  4. graines differentes -> refus de comparer
      attendu: false   obtenu: false
      (7 traces de scenario en 486 s, 7 a la fois)
PASS  5. endurance, masques actifs, 3 jours, deux traces -> aucune divergence
      attendu: true   obtenu: true   (16201 ticks, 10 sections jugees, 24 masques ; empreinte globale : identique)
PASS  6. endurance, masques actifs, un ulp sur actors[0].x au tick 300 -> divergence annoncee au tick 300
      attendu: 300   obtenu: 300
PASS  7. trace sans masque face a une trace masquee -> refus de comparer
      attendu: false   obtenu: false   (masques: 24 vs 0 ; seulement dans A : animaux, economieJournaliere, kosmos, lodLogique, minuit.animalsDaily, minuit.careers, minuit.collective, minuit.colonyDoctrine, minuit.colonySites, minuit.founderCharter, minuit.founders, minuit.growthChapter, minuit.immigration, minuit.lifeDaily, minuit.memory.partiel, minuit.roadEvolution, minuit.romanCouncils, minuit.socialOrders, minuit.transportProjects, minuit.watchPosts, sagas, separationFoule, transport, urbanIntent)
PASS  8. colony.treasury perturbee au tick 300, hors -sections actors,buildings -> non annoncee
      attendu: true   obtenu: true   (colony diverge reellement au tick 300 ; rapport : premierDivergent=null, colony ignoree=true)
PASS  9. -sections hors du perimetre du scenario (colony) -> refus de conclure
      attendu: false   obtenu: false   (colony : hors du perimetre du scenario endurance (seed, rng, w, h, time, day, tileDiff, buildings, actors, mealReservations))
PASS  10. sans masque, tick recompose contre tick de reference, 3 jours -> aucune divergence, sur tout l'etat
      attendu: true   obtenu: true   (16201 ticks ; perimetre : identique ; empreinte globale : identique)
OK — 10 cas sur 10.
```

Le cas 10 franchit trois minuits : la file de minuit (17 travaux, 2 par tick) et `onNewDay` y
passent, sans masque, par le tick recomposé comme par celui de la référence.

Et le rapport lisible de deux traces `endurance` indépendantes (deux processus, 3 jours) :

```bash
node tools/migration/emit-state-digests.mjs -ref <tag> -scenario tools/migration/scenarios/endurance.json -days 3 -out a.jsonl
node tools/migration/emit-state-digests.mjs -ref <tag> -scenario tools/migration/scenarios/endurance.json -days 3 -out b.jsonl
node tools/migration/compare-digests.mjs a.jsonl b.jsonl
```

```
RAPPORT DE DIVERGENCE
=====================

A : a.jsonl
    source=js graine=12345 dt=0.016666666666666666 ticks=16200 every=1 ref=fee66ae
    scenario=endurance empreinte=a7c317b02da7be0f masques=24
B : b.jsonl
    source=js graine=12345 dt=0.016666666666666666 ticks=16200 every=1 ref=fee66ae
    scenario=endurance empreinte=a7c317b02da7be0f masques=24

Ticks compares : 16201 (de 0 a 16200)
Masques du scenario (24) : kosmos, sagas, urbanIntent, lodLogique, separationFoule, animaux, transport, economieJournaliere, minuit.collective, minuit.socialOrders, minuit.colonyDoctrine, minuit.growthChapter, minuit.founderCharter, minuit.colonySites, minuit.transportProjects, minuit.roadEvolution, minuit.watchPosts, minuit.lifeDaily, minuit.careers, minuit.founders, minuit.romanCouncils, minuit.memory.partiel, minuit.animalsDaily, minuit.immigration
Sections jugees (10) [perimetre du scenario] : seed, rng, w, h, time, day, tileDiff, buildings, actors, mealReservations
Sections ignorees (25), sans verdict : animalState, animals, colony, districtLandmarks, economy, game, life, logs, market, navCache, navVersion, nextBuildingId, nextId, player, playerPersonId, playerRng, playerTalkIntent, playerTalkOutcome, roadEvents, sagas, save, settlement, traffic, transport, transportPhase

IDENTIQUES sur les 16201 ticks compares.
10 sections jugees, aucune n'a devie.
```

## 7. Les fichiers

| Fichier | Rôle |
|---|---|
| `tools/migration/build-scenario.mjs` | constructeur : recette → sauvegarde, point fixe, audit des tirages |
| `tools/migration/scenarios/endurance.mjs` | recette `endurance` |
| `tools/migration/scenarios/endurance.json` | le scénario construit (empreinte `a7c317b02da7be0f`) |
| `tools/migration/scenarios/masks.mjs` | registre des masques, `tickRecompose`, application, compteur de tirages |
| `tools/migration/scenarios/scenario-format.mjs` | format, empreinte, chargement vérifié, provenance de la référence |
| `tools/migration/emit-state-digests.mjs` | `-scenario`, `-sans-masques`, `-tick`, `-audit-rng`, `-perturb-path` |
| `tools/migration/compare-digests.mjs` | `-sections`, refus scénario / masques / périmètre |
| `tools/migration/selftest-harness.mjs` | cas 5 à 10 |
