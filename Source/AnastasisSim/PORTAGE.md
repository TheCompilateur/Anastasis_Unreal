# Portage du simulateur JS vers Unreal — module `AnastasisSim`

Source de référence : `C:\dev\Jeux IV Kingdoms`, dossiers `src/sim`, `src/runtime`,
`src/life`, `src/ai`. Le rendu (`src/render3d`) n'est **pas** porté : il est écrit
pour three.js et sera remplacé par le rendu Unreal.

## Règle du module

`AnastasisSim` ne dépend que de `Core` et `CoreUObject`. Pas d'`Engine`, pas
d'acteur, pas de rendu. La simulation doit tourner en headless — tests
d'automation, rejeu de bug, serveur — exactement comme en jeu. Tout ce qui touche
à l'affichage vit dans le module `Anastasis_UnrealV2`.

Corollaire : le module de jeu peut dépendre de `AnastasisSim`, jamais l'inverse.

## Le contrat : parité bit à bit, pas « à peu près »

Le simulateur JS est la référence. Le portage doit rendre **exactement** les mêmes
doubles, pas des valeurs proches. Deux raisons concrètes :

1. Un état JS doit pouvoir se rejouer dans Unreal et donner le même village. C'est le
   **harnais** qui relit cet état, pas le jeu : le format de sauvegarde joueur est natif
   Unreal, décision prise dans `docs/migration/phase2/P2_MODELE_DONNEES.md`.
2. Un bug reproduit avec une graine doit se reproduire des deux côtés.

Un écart d'un ulp n'est pas cosmétique : `Math.floor(hash2d(...) * n)` bascule de
catégorie au bord, et ce sont des arbres, des roches et des herbes qui changent de
place. Sur une comparaison `dist < radius`, c'est une décision de PNJ qui bascule.

### Comment la parité est vérifiée

`tools/unreal/gen-parity-vectors.mjs` (dans le dépôt JS) importe les modules de
référence **tels quels** et émet `Private/Tests/AnastasisParityVectors.inl` : des
vecteurs d'entrée/sortie où tous les doubles sont donnés par leur motif binaire —
un littéral décimal perdrait le dernier bit, et c'est ce bit qu'on teste.

```bash
cd "C:/dev/Jeux IV Kingdoms" && node tools/unreal/gen-parity-vectors.mjs
```

Les tests `Anastasis.Sim.Parite.*` comparent bit à bit. **Ne jamais corriger un
vecteur à la main pour faire passer un test** : soit le portage a dévié, soit la
référence JS a changé et il faut regénérer.

## Pièges rencontrés (à relire avant de porter la suite)

**La sémantique numérique de JavaScript.** Tout nombre JS est un double ; les
opérateurs binaires (`>>> 0`, `| 0`, `^`, `<<`, `Math.imul`) convertissent d'abord
ce double en entier 32 bits selon ECMA-262. Un portage « raisonnable » écrit
`(uint32)(x * 374761393)` et croit avoir traduit — mais si `x` vaut 3.5, JS calcule
`1311664875.5` en double **puis** tronque. `AnastasisJs::ToUint32` reproduit cette
conversion ; l'utiliser partout où le JS écrit `>>> 0` sur autre chose qu'un entier
32 bits déjà formé.

**`Number(v) || 1`.** En JS, `0` est *falsy* : `Number(0) || 1` vaut 1. D'où
`AnastasisJs::NumberOr`, qui retombe sur le défaut pour NaN **et** pour 0. Sans
cette subtilité, `StepPlan(0)` diviserait par zéro au lieu de rendre 1x.

**`Math.hypot`.** V8 l'implémente par mise à l'échelle sur le max puis sommation
compensée de Kahan. `std::hypot` et `sqrt(dx*dx+dy*dy)` donnent des résultats
voisins mais différents. `AnastasisMath::JsHypot` reproduit l'algorithme de V8.

**`float` interdit.** La référence calcule en 64 bits. Une simulation qui tourne
des heures accumule l'écart jusqu'à la divergence. Tout est en `double`, y compris
les positions.

**Pas de `FMath::Lerp`, pas de `FRandomStream`.** `FMath::Lerp` n'a pas la même
forme algébrique que `a + (b - a) * t`, donc pas le même dernier bit.
`FRandomStream` est un LCG différent de mulberry32 : il produirait une suite valide
mais *autre*, et toute sauvegarde JS deviendrait injouable.

## État du portage

### Fait — couche 0, socle déterministe

| Unreal | Source JS |
| --- | --- |
| `Core/AnastasisJsNumeric.h` | (nouveau — sémantique ECMA-262) |
| `Core/AnastasisRng.h/.cpp` | `src/sim/rng.js` |
| `Core/AnastasisSimMath.h/.cpp` | `src/sim/util.js` |
| `Core/AnastasisSimClock.h/.cpp` | `src/runtime/simClock.js`, `frameDeltaSeconds` de `src/runtime/gameLoop.js` |
| `Core/AnastasisSpatialGrid.h/.cpp` | `src/sim/spatialGrid.js` |

### Fait — couche 1, génération du monde (Phase 1)

| Unreal | Source JS |
| --- | --- |
| `World/AnastasisWorldNoise.h` | `valueNoise` / `smoothNoise` / `fbm` de `src/sim/world.js` |
| `World/AnastasisWorldArchetype.h/.cpp` | knobs sim de `src/sim/worldArchetypes.js` (pas air/forest/wardrobe) |
| `World/AnastasisHydrology.h/.cpp` | `src/sim/hydrology.js` |
| `World/AnastasisWorld.h/.cpp` | `generateWorld`, plafond forêt, `pickFieldCropId` |

Tests mesurés 2026-09-11 (`UnrealEditor-Cmd -nullrhi`) : `Monde` / `Archetype` / `Culture` / `Libm` **PASS**. `Fbm` **FAIL** 1 vecteur (~2.5 ulp sur `fbm(0.1, 0.2, 12345)`) — le stockage tuile est f32+`round3`, les cartes vecteurs restent identiques. Si le bit-exact double de `fbm` devient requis : remplacer `AnastasisJs::Sin` (fdlibm), jamais les vecteurs `.inl`. `SemantiqueJs::ToUint32(1e21)` échoue (couche 0, hors worldgen).

### Fait — couche 2, navigation (terrain + A*)

| Unreal | Source JS |
| --- | --- |
| `World/AnastasisNavGrid.h/.cpp` | couche terrain de `src/sim/navGrid.js` + `blockedAt` / `footBlockedAt` / `tileTraversalCost` de `simulation.js` |
| `World/AnastasisPathfinding.h/.cpp` | `src/sim/pathfinding.js` |

Tests mesurés 2026-09-13 : `Parite.NavCout` / `Parite.NavChemin` / `Parite.NavInvariants` **PASS**
(suite `Anastasis.Sim` : 17 PASS, 2 KNOWN_EXPECTED_FAILURE, 0 FAIL). Vecteurs générés par
`tools/migration/gen-nav-vectors.mjs` — 32 chemins comparés point par point, motif binaire
compris, plus 24 cases de coût.

**Ce que la mutation a appris.** Trois mutations posées exprès pour vérifier que les vecteurs
mordent :

| Mutation | Détectée | Ce que ça dit |
| --- | --- | --- |
| échanger deux voisins dans `NEIGHBORS` | **non** | l'ordre des voisins ne change aucun chemin : le comparateur du tas est un ordre total, la suite des `pop` ne dépend pas de l'ordre des poussées |
| retirer le départage par `h` du comparateur | oui (`NavChemin`) | c'est **lui** qui rend le chemin reproductible |
| stocker `MoveCost` en `double` au lieu de `float` | oui (`NavCout`) | la référence stocke en `Float32Array` : un `double` est plus précis, donc faux |

La première a corrigé un commentaire que j'avais écrit faux dans `AnastasisPathfinding.h`.

Pas encore porté de cette couche : `navService.js` (cache et file de requêtes), `crowdNav.js`,
les points d'accès des bâtiments (dépendent de `src/sim/urban/intent.js`, chantier urbanisme),
les métriques et l'anneau de trace. Ils suivront leurs systèmes.

### Fait — couche 3, budget de simulation (noyau causal)

| Unreal | Source JS |
| --- | --- |
| `Core/AnastasisSimBudget.h/.cpp` | noyau causal de `src/sim/simulationBudget.js` |

**C'est la référence qui trace la ligne du portage.** Son en-tête porte une loi datée du
10/08/2026 — « la machine peut changer la vitesse à laquelle le monde est calculé, elle ne
doit pas changer le monde qui est calculé » — née d'un bug mesuré : `pressure` était une
moyenne mobile du temps mur, et la pression décide quels PNJ pensent. Même graine, mêmes
ticks : population 6 contre 5, trésor 328 contre 321.

Depuis, tout ce qui touche au chronomètre (`ema`, `noteSimulationBudgetFrame`,
`frameWallMs`, `simMs`, `observedTier`, le HUD) est déclaré observation seule et ne décide
plus rien. Ce n'est donc pas du ressort d'un module dont le contrat est le déterminisme :
non porté, à refaire dans la couche de présentation. Reste ici ce qui décide : palier,
multiplicateurs, bande de cadence, intervalle.

`logicalLod.js`, l'autre moitié de la vague 3, n'est pas porté : il agrège l'état du
village (habitants, bâtiments) et suivra ses systèmes.

### L'atelier de vecteurs — déclarer au lieu d'écrire

Trois modules portés, trois générateurs écrits à la main : à ce rythme, 198 modules
valent 198 générateurs. `tools/migration/parity-kit.mjs` rend la preuve déclarative pour
les fonctions à arguments et retours scalaires — on dit quelle fonction appeler et sur
quelles entrées, l'atelier exécute la référence et émet le `.inl`.

```bash
node tools/migration/gen-parity.mjs simulation-budget.mjs   # ou --tous
```

Les fonctions à état — un A*, un hacheur, une boucle de tick — gardent un générateur dédié :
leur difficulté est ailleurs que dans la plomberie.

Deux pièges payés une fois, corrigés dans l'atelier pour tous les modules à venir :

- **`UTF8_TO_TCHAR` dans un initialiseur statique rend un pointeur pendouillant.** L'objet
  de conversion est un temporaire ; la table ne gardait que des chaînes vides, et les
  vecteurs accusaient le portage en comparant `""` à `"normal"`. Les tables portent
  désormais des octets UTF-8, convertis au point d'usage.
- **Une claim non testée est une claim fausse.** L'en-tête affirmait que `Math.hypot`
  compte face à un `sqrt(dx²+dy²)`. Vérification : sur les 17 entrées de la batterie, deux
  faisaient diverger les bits et **aucune ne changeait de bande**. Trois entrées ont été
  cherchées exprès, où `hypot` rend exactement le rayon et `sqrt` le double juste
  au-dessus — near contre medium, medium contre far, far contre invisible.

Écarts assumés, documentés dans les en-têtes :

- La grille spatiale stocke des **index** `int32`, pas des références d'acteurs :
  en C++ un pointeur vers un élément de `TArray` meurt au premier realloc.
- `startGameLoop` (boucle `requestAnimationFrame`) n'est pas porté — c'est le
  `Tick` d'Unreal.
- `simSpeedRenderGate3d` / `applySimSpeedRenderGate3d` ne sont pas portés : ce sont
  des réglages three.js (ombres, particules, pixel ratio). L'équivalent Unreal
  appartient à la couche de présentation.

### Ce qu'il reste — l'inventaire

`docs/migration/phase2/P2_INVENTAIRE_JS.md` classe les 234 modules du noyau JS en
**porter / générer / jeter**, avec leur vague et leur chantier. Il se régénère, il ne
s'édite pas :

```bash
node tools/migration/inventory-js-sim.mjs -out docs/migration/phase2/P2_INVENTAIRE_JS.md
```

Au 2026-09-13 (référence `fee66ae`) : 198 modules à porter — **63 492 lignes de code**,
commentaires et lignes vides déduits — 4 tables à générer, 24 modules à ne pas porter.

### Comment la parité se vérifiera au-delà des fonctions pures

Les vecteurs bit à bit ne montent pas jusqu'à `simulation.js` : on ne fabrique pas un
vecteur pour « le tick de minuit ». Au-delà de la couche 1, la preuve est un **harnais
différentiel** — même graine, une empreinte de l'état à chaque tick des deux côtés, et un
rapport qui nomme le premier tick divergent et la section fautive.

`docs/migration/phase2/P2_HARNAIS_DIFFERENTIEL.md`. L'empreinte C++
(`Core/AnastasisStateDigest.h`) est déjà prouvée identique à la spécification JS
(`Anastasis.Sim.Empreinte.*`) ; l'émetteur Unreal arrivera avec la couche 4, quand il y
aura un état à décrire.

### Suite proposée — dans cet ordre

L'ordre suit les dépendances réelles, pas l'intérêt du gameplay. Chaque étape doit
arriver avec ses vecteurs de parité avant qu'on empile la suivante.

1. **Génération du monde** — portée (Phase 1). `Anastasis.Sim.Parite.Monde` PASS 2026-09-11. `Fbm` 1 vecteur ~2.5 ulp (voir `docs/migration/phase1/P1_HANDOFF.md`).
2. **Navigation** — terrain et A* portés (couche 2 ci-dessus). Restent `navService.js`,
   `crowdNav.js`, et les points d'accès une fois `urban/intent.js` porté.
3. **Budget et LOD logique** — noyau causal du budget porté (couche 3 ci-dessus).
   Reste `logicalLod.js`, qui agrège l'état du village et suivra ses systèmes.
4. **État du monde et sauvegarde** — `src/sim/save.js`, `world.js` (structures).
   La décision est prise : `docs/migration/phase2/P2_MODELE_DONNEES.md`. Tableau de
   structures et non SoA, table ordonnée à la sémantique JS (`World/AnastasisEntityTable.h`),
   références par identifiant, format de sauvegarde natif Unreal — le format JS est lu par
   le harnais, jamais écrit, et ne promet rien au joueur. `saveStore.js` n'est pas porté
   (`localStorage`).
5. **Boucle de simulation** — `src/sim/simulation.js` (8 663 lignes). À découper,
   pas à traduire d'un bloc.
6. **Vie et IA** — `src/life/*` (66 fichiers), `src/ai/*` (39 fichiers). La masse
   du contenu, mais la couche la moins piégeuse : c'est de la logique de haut
   niveau posée sur le socle ci-dessus.

Non porté volontairement pour l'instant : `src/runtime/faultShield.js`,
`flightRecorder.js`, `watchdog.js`, `crashReport.js`, `postMortemUi.js`. Ce sont
des filets de sécurité conçus autour de contraintes navigateur ; Unreal a ses
propres équivalents et le choix est à refaire, pas à traduire.

## Lancer les tests

L'éditeur doit être fermé (Live Coding verrouille les DLL) :

```bash
"C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" Anastasis_UnrealV2Editor Win64 Development -Project="C:\Users\alex_\OneDrive\Documents\Unreal Projects\Anastasis_UnrealV2\Anastasis_UnrealV2.uproject" -WaitMutex
```

```bash
"C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "C:\Users\alex_\OneDrive\Documents\Unreal Projects\Anastasis_UnrealV2\Anastasis_UnrealV2.uproject" -ExecCmds="Automation RunTests Anastasis.Sim.Parite; Quit" -unattended -nopause -nullrhi -nosplash -log
```
