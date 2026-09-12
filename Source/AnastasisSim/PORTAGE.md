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

1. Une sauvegarde JS doit pouvoir se rejouer dans Unreal et donner le même village.
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

Écarts assumés, documentés dans les en-têtes :

- La grille spatiale stocke des **index** `int32`, pas des références d'acteurs :
  en C++ un pointeur vers un élément de `TArray` meurt au premier realloc.
- `startGameLoop` (boucle `requestAnimationFrame`) n'est pas porté — c'est le
  `Tick` d'Unreal.
- `simSpeedRenderGate3d` / `applySimSpeedRenderGate3d` ne sont pas portés : ce sont
  des réglages three.js (ombres, particules, pixel ratio). L'équivalent Unreal
  appartient à la couche de présentation.

### Suite proposée — dans cet ordre

L'ordre suit les dépendances réelles, pas l'intérêt du gameplay. Chaque étape doit
arriver avec ses vecteurs de parité avant qu'on empile la suivante.

1. **Génération du monde** — portée (Phase 1). `Anastasis.Sim.Parite.Monde` PASS 2026-09-11. `Fbm` 1 vecteur ~2.5 ulp (voir `docs/migration/phase1/P1_HANDOFF.md`).
2. **Navigation** — `src/sim/navGrid.js`, `pathfinding.js`. L'ordre d'exploration
   de l'A* doit être identique : deux chemins de même coût, et les PNJ ne prennent
   pas la même rue.
3. **Budget et LOD logique** — `src/sim/simulationBudget.js`, `logicalLod.js`.
   Attention : la pression est déclarée `deterministic-only` côté JS ; ne jamais la
   dériver du temps mur réel sous peine de rendre la simulation non reproductible.
4. **État du monde et sauvegarde** — `src/sim/save.js`, `saveStore.js`,
   `world.js` (structures). C'est ici qu'on décide si Unreal relit les sauvegardes
   JS existantes ou repart d'un format propre — décision à prendre explicitement.
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
