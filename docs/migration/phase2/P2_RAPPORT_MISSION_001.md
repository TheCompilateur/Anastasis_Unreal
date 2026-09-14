# ANASTASIS_SIM_CPP_TRANSITION_001 — rapport

Discipline de claim : `OBS` observé · `EVD` mesuré · `INF` déduit · `DEC` décidé · `UNK` inconnu.
Une déduction n'est pas une mesure, et rien ici ne prétend l'être.

---

## 1. PROVENANCE

| | |
|---|---|
| Dépôt Git | `C:/dev/ANASTASIS_UNREAL/.git` (worktree partagé) |
| Racine de travail | `C:/dev/ANASTASIS_WORKTREES/multi-agent-control-001` |
| Branche | `agent/multi-agent-control-001` |
| HEAD final | `e531cc4` |
| Worktree | **propre** |
| Position | 4 commits devant la base, `main` a avancé de **20 commits** pendant la session |
| Référence JS | `C:/dev/Jeux IV Kingdoms` @ `fee66ae` — **lue seulement, jamais écrite** |

**Projet Unreal canonique, prouvé et non supposé** : `Anastasis_UnrealV2.uproject`,
`EngineAssociation "5.8"`, UE 5.8.2. Modules compilés : `AnastasisSim` (simulation pure,
dépendances `Core` + `CoreUObject` seulement) et `Anastasis_UnrealV2` (jeu, WorldView).
`AGENTS.md` interdit le développement dans la racine canonique ; tout ce travail vit dans
le worktree.

### Activité concurrente `OBS`

Sept worktrees agents actifs. Deux faits qui comptent :

- **`main` +20 commits** : végétation, assets, tessellation du relief. `git log HEAD..main --
  Source/AnastasisSim tools/migration` est **vide** — aucune collision textuelle avec ce
  travail. Un merge antérieur de cette branche est déjà dans le trunk.
- **`agent/sim-tick-day`** (`bbd46cc`) : collision **sémantique**, pas textuelle. Détail en §4.

Six processus Unreal (4 éditeurs, 2 `UnrealEditor-Cmd`) tournaient en fin de session, lancés
par d'autres agents. Non tués — ils ne m'appartiennent pas. Conséquence directe en §10.

Aucun `reset --hard`, aucun `checkout .`, aucun nettoyage de travail concurrent. Le seul
`git checkout --` de la session a restauré **mon propre** fichier muté.

---

## 2. CARTE DU SIMULATEUR

Mesurée, pas estimée : `tools/migration/inventory-js-sim.mjs` classe les **234 modules** du
noyau (`src/sim`, `src/life`, `src/ai`, `src/lang`, `src/runtime`) par graphe d'imports et
atteignabilité depuis `src/main.js`. Rapport complet : `P2_INVENTAIRE_JS.md`.

| | fichiers | lignes | dont code |
|---|---:|---:|---:|
| À porter | 198 | 79 659 | **63 492** |
| À générer (données) | 4 | 2 101 | 1 278 |
| À jeter (navigateur, présentation, barils, non atteint) | 24 | 5 055 | 3 909 |
| Déjà porté | 8 | 2 347 | 1 805 |

Chantiers du reste à porter, par vague de dépendance :

| Vague | Chantier | fichiers | code | owner JS principal |
|---|---|---:|---:|---|
| 2 | navigation | 9 | 1 491 | `navService.js` |
| 3 | budget et LOD | 2 | 394 | `logicalLod.js` |
| 4 | état et sauvegarde | 3 | 778 | `save.js` |
| 5 | noyau de boucle | 9 | 14 013 | `simulation.js` (7 196) |
| 5 | société et institutions | 8 | 6 629 | `collectivePriorities.js` |
| 5 | urbanisme | 14 | 3 954 | `urban/intent.js` |
| 5 | économie et travail | 18 | 3 656 | `craftWork.js` |
| 5 | transport et logistique | 13 | 3 350 | `transport/delivery.js` |
| 5 | chronique et mémoire | 11 | 2 483 | `villageChronicle.js` |
| 5 | règne animal | 7 | 1 031 | `animaux/updateAnimals.js` |
| 6 | personne et famille | 33 | 7 520 | `life/lifeScenes.js` |
| 6 | cognition | 33 | 7 281 | `ai/memory.js` |
| 6 | parole et narration | 15 | 4 547 | `life/talk.js` |
| 6 | rites et culture | 14 | 4 072 | `life/kosmos1204…` |
| 6 | langue | 9 | 2 293 | `lang/lexicon.js` |

Frontière du simulateur, déterminée et non supposée : `src/render3d` est exclu par décision
(`AGENTS.md`), et la mesure confirme que le noyau ne l'importe jamais. En revanche
`src/runtime` contient surtout des **filets de sécurité navigateur** — `faultShield`,
`flightRecorder`, `watchdog`, `crashReport`, `postMortemUi` — qui ne sont pas de la
simulation et ne se traduisent pas.

---

## 3. CARTE DE MIGRATION JS → C++

Statuts : `JS_ONLY` · `PARTIAL_PARITY` · `PARITY_TESTED` · `UNMAPPED`.

| Responsabilité sémantique | Source JS | C++ actuel | Statut |
|---|---|---|---|
| RNG déterministe | `sim/rng.js` | `Core/AnastasisRng` | **PARITY_TESTED** |
| Math JS-exacte (hypot, hash) | `sim/util.js` | `Core/AnastasisSimMath` | **PARITY_TESTED** |
| Sémantique ECMA-262 (`>>>0`…) | — | `Core/AnastasisJsNumeric` | PARTIAL_PARITY (`ToUint32(1e21)`) |
| Horloge et plan de pas | `runtime/simClock.js` | `Core/AnastasisSimClock` | **PARITY_TESTED** |
| Grille spatiale | `sim/spatialGrid.js` | `Core/AnastasisSpatialGrid` | **PARITY_TESTED** |
| Bruit, fbm | `sim/world.js` | `World/AnastasisWorldNoise` | PARTIAL_PARITY (1 vecteur, ~2.5 ulp) |
| Génération du monde | `sim/world.js` | `World/AnastasisWorld` | **PARITY_TESTED** |
| Hydrologie | `sim/hydrology.js` | `World/AnastasisHydrology` | **PARITY_TESTED** |
| Archétypes de monde | `sim/worldArchetypes.js` | `World/AnastasisWorldArchetype` | PARTIAL (knobs sim seuls) |
| Coûts de terrain, blocage au pied | `sim/navGrid.js` (couche terrain) | `World/AnastasisNavGrid` | **PARITY_TESTED** |
| A\* | `sim/pathfinding.js` | `World/AnastasisPathfinding` | **PARITY_TESTED** |
| Budget de simulation (causal) | `sim/simulationBudget.js` | `Core/AnastasisSimBudget` | **PARITY_TESTED** |
| Empreinte d'état canonique | `sim/save.js` (projection) | `Core/AnastasisStateDigest` | **PARITY_TESTED** |
| Table d'entités (ordre JS) | `simulation.js` (`actors`) | `World/AnastasisEntityTable` | testé, sans vecteurs JS |
| Horloge jour / rollover | `simulation.js` (tick) | `Sim/AnastasisSimulation` *(branche concurrente)* | hors de cette branche |
| Cache et file de chemins | `sim/navService.js` | — | UNMAPPED |
| Genèse (site, fondateurs, camp) | `simulation.js` ctor | — | **UNMAPPED — bloque le harnais** |
| Acteurs, besoins, économie, vie, IA | `sim/`, `life/`, `ai/` | — | JS_ONLY |

**Aucun nom similaire n'a été pris pour une équivalence.** Chaque `PARITY_TESTED` ci-dessus
pointe un test d'automation nommé, et chaque test relit un `.inl` généré depuis la référence
exécutée.

---

## 4. MODÈLE D'AUTORITÉ ET DE TICK

`OBS` La simulation est **hybride, à temporalités imbriquées** — réponse D/E, pas A :

```
FRAME navigateur (requestAnimationFrame)
  └─ PumpFrame : frameDeltaSeconds + simStepPlan → N ticks
       └─ TICK  dt = 1/30 s de simulation
            ├─ time += dt ;  day = 1 + floor(time / 90)
            ├─ FRONTIÈRE MINUIT — critique : éco, logements, pouls
            ├─ FRONTIÈRE MINUIT — différé : clio, vie, métiers, bétail, immigration
            │     (file à quota, DAY_DEFERRED_JOBS_PER_TICK = 2)
            └─ CADENCE PNJ : near 60 Hz · medium 10 Hz · far 1 Hz · invisible 0.25 Hz
```

`DAY_LENGTH = 90` secondes de simulation ⇒ **2 700 ticks/jour** à dt = 1/30. `EVD`

Le point important : **la cadence PNJ n'est pas le tick**. Un habitant lointain est simulé
par bouffées avec un `dt` accumulé, plafonné à 2.5 s (6 s s'il est invisible). Transposer
mécaniquement les fréquences JS dans un `Tick` Unreal détruirait cette structure.

### SIM_STATE(t) — où vit réellement l'état

`serialize(sim)` de `src/sim/save.js` **fait autorité** et non une sélection reconstruite :
35 sections, exhaustives par construction. Ses commentaires documentent les divergences déjà
combattues — le `trafficTimer` qui décide de la formation des routes donc du terrain, le
cache A\* dont l'absence change la trajectoire dès la première requête, la portion de repas
qui reste réservée après rechargement.

### Collision sémantique avec `agent/sim-tick-day` `OBS`

Cette branche a déjà porté l'hôte temporel : `Source/AnastasisSim/Public/Sim/AnastasisSimulation.h`
— `Reset(seed)`, `Tick(dt)`, `PumpFrame`, rollover jour, file différée, `DayLength = 90`.
C'est **le propriétaire de SIM_STATE(t)**, et c'est la tranche que j'allais recommander.

Pas de collision textuelle (`Sim/` contre mes `Core/` et `World/`). Mais elle expose
`TileFingerprint()`, une empreinte écrite à la main — **là où `Core/AnastasisStateDigest`
existe déjà et est prouvé identique à la spécification JS.** Deux empreintes incompatibles
dans le même module ne se compareront jamais à une trace JS.

`DEC` Point de rencontre recommandé : `FAnastasisSimulation` émet sa projection par
`AnastasisDigest::FStateWriter`, et `TileFingerprint()` disparaît. C'est une ligne de code
et cela branche le harnais différentiel sur le premier état C++ existant.

---

## 5. RISQUES DE DÉTERMINISME

Tous constatés dans le code, aucun spéculatif.

| Risque | Sévérité | État |
|---|---|---|
| Pression de budget dérivée du temps mur | **PORT_BLOCKER** (historique) | Corrigé dans la référence le 10/08/2026, preuve datée : pop 6 vs 5, trésor 328 vs 321. **Ne jamais recâbler `FApp::GetDeltaTime` sur `Pressure`.** Non porté côté C++ : impossible à réintroduire par accident |
| Ordre du tableau d'acteurs observable (`splice`) | **HIGH** | `RemoveAtSwap` interdit dans `AnastasisEntityTable`, testé sur 4 éléments |
| Vue caméra décidant la cadence PNJ | **HIGH** (bancs) | Mesuré côté JS : 109 477 vs 108 586 ticks au jour 40. Mitigé par vue épinglée — le concept devra être porté avec la vue |
| `Math.hypot` V8 (Kahan) vs `sqrt(dx²+dy²)` | **MEDIUM** | `JsHypot` porté. Trois vecteurs franchissent un rayon (near/medium/far vs medium/far/invisible) |
| Stockage `Float32Array` (`moveCost`, altitudes) | **MEDIUM** | `TArray<float>` délibéré : un `double` serait plus précis **donc faux**. Vecteur forêt `0x3ff99999a0000000` |
| Ordre des clés d'objet JS | **MEDIUM** | Tri par unité de code UTF-16 dans l'empreinte, des deux côtés |
| Compteurs de module (`logs[].id` = `vlog-N`) | **MEDIUM** | Découvert par le harnais à son premier essai. Contrainte : **une trace = un processus neuf** |
| `fbm` — 1 ulp (~2.5) via `sin` | **LOW** | `KNOWN_EXPECTED_FAILURE` au registre. Stockage tuile f32+`round3`, cartes identiques |
| `ToUint32(1e21)` | **LOW** | `KNOWN_EXPECTED_FAILURE`. Hors worldgen |
| `Number(v) \|\| 1` — `0` est *falsy* | **LOW** | `AnastasisJs::NumberOr` |

`DEC` L'égalité bit-à-bit **est** exigée ici, et la justification est écrite dans
`PORTAGE.md` : `Math.floor(hash2d(...) * n)` bascule de catégorie au bord — ce sont des
arbres qui changent de place ; sur `dist < radius`, c'est une décision de PNJ qui bascule.

---

## 6. SÉLECTION DE TRANCHE — trois candidats évalués

| | Valeur | Dépendances | Complexité | Testabilité | Risque | Débloque |
|---|---|---|---|---|---|---|
| **A. Navigation (terrain + A\*)** | haute — tout déplacement | worldgen seul ✓ | moyenne | **excellente** (chemin point par point) | faible | destination, transport, crowdNav |
| **B. Budget causal** | moyenne — gate qui pense | `util.js` ✓ | faible | excellente (scalaires) | faible | cadence des acteurs |
| **C. Genèse** | **maximale** — allume le harnais | site, fondateurs, camp, index | **élevée** | faible tant qu'incomplète | élevé | tout l'état |

`DEC` **A puis B.** C n'a pas été retenu : gros, séquentiel, mauvais candidat multi-agent —
et un autre agent l'a effectivement entamé par le tick. Ni A ni B n'était « le plus facile » :
A a été choisi parce que l'A\* est l'endroit où une divergence est *invisible* (deux chemins
de même coût) et donc où la preuve vaut le plus.

---

## 7. IMPLÉMENTATION

Simulation pure, aucune dépendance `Engine`, aucun `UObject`, aucun `AActor`, aucun Blueprint.

**Portage**
`Public/World/AnastasisNavGrid.h` · `Private/World/AnastasisNavGrid.cpp`
`Public/World/AnastasisPathfinding.h` · `Private/World/AnastasisPathfinding.cpp`
`Public/Core/AnastasisSimBudget.h` · `Private/Core/AnastasisSimBudget.cpp`
`Public/World/AnastasisEntityTable.h`

**Contrat de parité et preuve**
`Public/Core/AnastasisStateDigest.h` · `Private/Core/AnastasisStateDigest.cpp`
`tools/migration/state-digest.mjs` — **la spécification**, et le hacheur du harnais
`tools/migration/emit-state-digests.mjs` · `compare-digests.mjs` · `selftest-harness.mjs`
`tools/migration/parity-kit.mjs` · `gen-parity.mjs` · `parity/simulation-budget.mjs`
`tools/migration/gen-nav-vectors.mjs` · `gen-digest-vectors.mjs`
`tools/migration/inventory-js-sim.mjs`

**Décisions d'architecture**
`P2_MODELE_DONNEES.md` — tableau de structures (pas SoA : 151 champs par habitant, dont 40
objets imbriqués), références par identifiant (ni pointeur, ni index), format de sauvegarde
natif Unreal, le format JS lu par le **harnais** et jamais promis au joueur.

**Le harness n'est pas factice.** `emit-state-digests.mjs` instancie la vraie `Simulation`,
appelle le vrai `tick`, et projette le vrai `serialize`. `gen-nav-vectors.mjs` appelle le vrai
`findPath` sur un vrai `generateWorld`. Aucune logique testée n'est reconstruite dans le test.

---

## 8. PREUVE DE PARITÉ

`EVD` Suite complète mesurée sur build frais, éditeurs fermés :
**55 PASS · 4 KNOWN_EXPECTED_FAILURE (tous au registre) · 0 FAIL.**
Suite `Anastasis.Sim` en fin de session : **19 PASS · 2 KEF · 0 FAIL**.

| Enveloppe | Couverture | Verdict |
|---|---|---|
| Navigation — chemins | 3 graines × 64×64, 32 chemins, **553 points**, motif binaire | **EXACT** |
| Navigation — coûts | 24 cases : `moveCost`, `tileTraversalCost`, `footBlockedAt` | **EXACT** |
| Budget causal | 119 vecteurs : palier, multiplicateurs, intervalle, bande | **EXACT** |
| Empreinte JS↔C++ | 32 vecteurs, scalaires et composés | **EXACT** |
| Harnais, autotest | 4 cas / 4 | **EXACT** |
| `fbm` | 1 vecteur sur N | **DIVERGENT_EXPLAINED** (~2.5 ulp, `sin` fdlibm) |
| `ToUint32(1e21)` | 1 cas | **DIVERGENT_EXPLAINED** (portage entier) |

**Aucune tolérance n'a été élargie pour verdir un test.** Le seul `1e-6` du code est
celui de la référence (`accum + 1e-6 < interval`) et il est documenté.

### Les vecteurs mordent-ils ? Vérifié par mutation

| Mutation | Détectée | Ce que ça a appris |
|---|---|---|
| Échanger deux voisins de l'A\* | **non** | L'ordre des voisins **ne change aucun chemin** : le comparateur est un ordre total. **J'avais écrit le contraire** dans l'en-tête — corrigé, et il cite désormais la mesure |
| Retirer le départage par `h` | oui | C'est lui qui rend le chemin reproductible |
| `MoveCost` en `double` | oui | Le stockage f32 est bien testé |
| `JsHypot` → `sqrt` naïf | `UNK` — voir §10 | |

Trois erreurs corrigées en ma défaveur, pas en celle du portage : un commentaire faux ; une
assertion de test trop stricte (l'epsilon de la référence) ; un `UTF8_TO_TCHAR` en
initialiseur statique rendant des chaînes vides, qui accusait le portage.

---

## 9. CE QUI EST VRAI

- Quatre responsabilités possèdent un candidat C++ **dont la parité est démontrée dans
  l'enveloppe du §8** : coûts de terrain, A\*, budget causal, empreinte d'état.
- L'A\* rend le **même chemin**, point par point, motif binaire compris — pas un chemin de
  même longueur.
- Le harnais différentiel rend le premier tick divergent, la section, puis l'élément ; il est
  prouvé sur des divergences dont la réponse était connue, dont **un ulp retrouvé au bon tick**.
- L'empreinte C++ rend les mêmes bits que la spécification JS : une divergence future
  accusera la simulation, pas le hacheur.
- La route demandée existe et a été parcourue deux fois de bout en bout :
  `JS AUTHORITY → SEMANTIC CONTRACT → C++ IMPLEMENTATION → DIFFERENTIAL PROOF`.

---

## 10. CE QUI N'EST PAS PROUVÉ

- `UNK` **La mutation `JsHypot` → `sqrt` naïf n'a pas pu être mesurée.** Quatre éditeurs
  Unreal d'autres agents tenaient Live Coding. `INF` Côté JS, les trois entrées ajoutées
  rendent `near/medium/far` avec `hypot` et `medium/far/invisible` avec `sqrt` : le test
  échouerait. **C'est une déduction, pas une mesure.**
- `UNK` La compilation effective des 3 derniers vecteurs (119 au lieu de 116) n'a pas été
  reconfirmée par un build frais, pour la même raison. Les 19 PASS ont été mesurés sur
  l'arbre tel que commité.
- `UNK` **Aucune trajectoire d'état complète JS↔C++ n'a été comparée.** Le harnais est
  complet d'un côté et prouvé des deux, mais le C++ n'a pas d'état à projeter tant que la
  genèse n'est pas portée. Aucune tranche n'est donc `CPP_AUTHORITY_CANDIDATE`.
- `UNK` `AnastasisEntityTable` est testé contre la *sémantique* de `splice`, pas contre des
  vecteurs JS.
- `UNK` Rien n'est prouvé sur les 198 modules non portés — ni sur la genèse, ni sur les
  acteurs, ni sur l'économie, ni sur la vie sociale.
- `UNK` Aucune mesure de performance. Le SoA est reporté sans l'avoir chiffré.

Le JavaScript **reste l'autorité comportementale** sur l'intégralité du simulateur.

---

## 11. TROIS TRANCHES SUIVANTES, DANS L'ORDRE CAUSAL

1. **Souder l'empreinte à l'hôte de `agent/sim-tick-day`.** `FAnastasisSimulation` projette
   par `FStateWriter`, `TileFingerprint()` disparaît, l'émetteur Unreal écrit le même JSONL.
   Le harnais compare alors sa première trajectoire réelle — sur `time`, `day`, `seed`, le
   terrain. Petit, et c'est le seul geste qui allume l'instrument.
2. **La genèse** — `resetWorldBase` → site, fondateurs, camp, index. Grosse et séquentielle,
   mais chaque section qu'elle ajoute devient immédiatement comparable grâce à (1). La
   couverture du harnais (*n* sections sur 35) devient la mesure d'avancement de la migration.
3. **`navService.js`** — cache et file de requêtes A\*, clé `navVersion`. Le premier système
   à **état** de la navigation, et sa présence change les trajectoires (le cache sert un
   chemin approximatif) : c'est écrit noir sur blanc dans `save.js`.

`logicalLod.js`, `crowdNav.js` et les points d'accès attendent leurs systèmes — les points
d'accès dépendent de `urban/intent.js`, chantier urbanisme.

---

## 12. ÉTAT D'ARRÊT

| | |
|---|---|
| Branche | `agent/multi-agent-control-001` @ `e531cc4`, **worktree propre** |
| Commits de la session | 4 — harnais, modèle de données, navigation, budget + atelier |
| Intégration | **non faite** — `agent-worktree.ps1 integrate` demande le rôle d'intégrateur et une racine quiescente ; six processus Unreal d'autres agents tournaient |
| Dépôt JS | **non modifié** — vérifié |
| Build en attente | une passe pour lever les deux `UNK` du §10, dès que Live Coding est libre |
| Reste | 194 modules, ~62 000 lignes de code |
