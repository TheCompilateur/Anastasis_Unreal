# Référence JS du portage — `anastasis-ref-p3`

Le portage C++ (`Source/AnastasisSim`) est jugé contre **un commit** du simulateur JS, pas contre
une copie de travail. Ce document dit lequel, comment l'obtenir n'importe où, et comment on en
change.

Discipline de claim (`P3_PLAN.md`) : `OBS` observé · `EVD` mesuré · `INF` déduit · `DEC` décidé.

## La référence

| | |
|---|---|
| Dépôt | `https://github.com/TheCompilateur/Jeux-IV-Kingdoms` |
| Commit | `fee66ae8b571f6f7bcbe6a61f9749d0a812b84e2` |
| Date du commit | 2026-08-30 04:28:43 -0400 |
| Message | `refactor(render3d): liberer le RIG, et casser la boucle qui le retenait` |
| Tag | **`anastasis-ref-p3`**, annoté (objet `c1145ba9baf6caafec7f537fb639d57a5b98b9b4`), message « Reference du portage Unreal, phase 3 » |
| Poussé | 2026-10-01, mission `js-ref-pin-001` — le tag seul, aucune branche |

C'est le commit que citent `PORTAGE.md`, les fiches des tranches verticales et les en-têtes des
vecteurs `.inl` (`Reference: … @ fee66ae`).

## L'obtenir

Depuis n'importe quelle machine, sans rien d'autre que git :

```bash
git clone --depth 1 --branch anastasis-ref-p3 https://github.com/TheCompilateur/Jeux-IV-Kingdoms.git jsref
git -C jsref rev-parse HEAD     # doit rendre fee66ae8b571f6f7bcbe6a61f9749d0a812b84e2
```

Depuis un clone existant, sans toucher à sa copie de travail :

```bash
git -C <clone> fetch origin tag anastasis-ref-p3
git -C <clone> worktree add --detach <dossier temporaire> anastasis-ref-p3
```

Puis passer ce dossier en `-ref` à tous les outils de `tools/migration/` :

```bash
node tools/migration/inventory-js-sim.mjs   -ref jsref -out docs/migration/phase2/P2_INVENTAIRE_JS.md
node tools/migration/gen-parity.mjs --tous  -ref jsref
node tools/migration/gen-nav-vectors.mjs    -ref jsref
node tools/migration/emit-state-digests.mjs -ref jsref -seed 33344 -days 1 -out a.jsonl
node tools/migration/selftest-harness.mjs   -ref jsref
```

**Vérifié le 2026-10-01 `EVD`** : clone superficiel frais du tag dans un dossier vide →
`HEAD = fee66ae8b571f6f7bcbe6a61f9749d0a812b84e2`, 0 fichier modifié, `src/sim/simulation.js` et
`src/sim/save.js` présents. `git ls-remote origin refs/tags/anastasis-ref-p3*` rend l'objet tag
`c1145ba…` et sa cible `fee66ae…`.

## Ce que la mission a trouvé en épinglant

**`fee66ae` était déjà sur GitHub, mais pas épinglé `OBS`.** `P3_PLAN.md` §8 dit que GitHub
s'arrête à `f461eda` : c'est vrai de `main` (`f461eda`, 2026-08-27). Mais la branche distante
`codex/p0-temporal-hud` pointait déjà `fee66ae`. Une branche bouge au prochain push ; un tag annoté
non. Le tag ne déplace aucune branche. `f461eda` n'est pas un ancêtre de `fee66ae` : les deux
divergent depuis `a4d63b0` (1 commit propre à `f461eda`, 5 à `fee66ae`).

**La copie de travail `C:\dev\Jeux IV Kingdoms` n'est PAS la référence `OBS`.** Elle est sur
`codex/p0-temporal-hud` à `fee66ae`, mais au 2026-10-01 elle porte 36 modifications non commitées
sous `src/`, dont 12 fichiers du noyau simulé (`simulation.js`, `npc.js`, `save.js`, `life.js`,
`transport/index.js`…) et un fichier non suivi, `src/sim/observability.js`. Deux d'entre elles
changent le comportement simulé, dans `npc.js` :

| Fonction | `fee66ae` | copie de travail |
|---|---|---|
| `shouldHaulGatherLoad` | `if (load > 9) return true` | `load > 11` |
| `deliver`, lot générique | `maxBatch … : 3` | `: 8` |

Le premier est déjà l'écart n° 13 de `Village/AnastasisVillage.h` (« non suivi ») ; le second ne
touche que la vente hors dépôt, non portée.

Or tous les outils de `tools/migration/` prennent cette copie par défaut (`-ref` absent). D'où :

- **L'inventaire du 2026-09-13 avait été tiré de la copie de travail `EVD`** : il comptait
  `sim/observability.js` (234 modules), absent de `fee66ae` (233). Il est régénéré contre le tag
  (ci-dessous), et l'outil **refuse** désormais une référence dont `src/` est modifié
  (`-allow-dirty` pour passer outre, et le rapport le dit en tête).
- **Les vecteurs de parité n'en ont pas souffert `EVD`.** Régénérés depuis le tag dans un dossier
  temporaire, sans toucher aux `.inl` du dépôt, puis comparés aux `.inl` commités (lignes
  `// Reference:` et `// Declaration:` exclues, fins de ligne ignorées) :

  | Générateur | Vecteurs | Contre `anastasis-ref-p3` |
  |---|---:|---|
  | `gen-parity.mjs` : `bonds` | 2 834 | identique |
  | `build` | 432 | identique |
  | `domestic` | 7 | identique |
  | `gather` | 2 395 | identique |
  | `needs` | 684 | identique |
  | `nous-inertia` | 60 | identique |
  | `nous` | 252 | identique |
  | `regrow` | 686 | identique |
  | `simulation-budget` | 119 | identique |
  | `village-rhythm` | 642 | identique |
  | `weather-behavior` | 2 724 | identique |
  | `weather` | 1 112 | identique |
  | `gen-nav-vectors.mjs` | 32 chemins, 24 cases | identique |

  Non vérifiés ici : `AnastasisParityVectors.inl` (couches 0-1, généré par
  `tools/unreal/gen-parity-vectors.mjs` **dans le dépôt JS**) et `gen-digest-vectors.mjs` (ne lit
  pas la référence : il vecteurise le format d'empreinte).

**Consigne `DEC`** : toujours passer `-ref` vers un checkout propre du tag. Ne jamais pointer la
copie de travail de quelqu'un, même « sur le bon commit ».

## L'inventaire contre cette référence

`docs/migration/phase2/P2_INVENTAIRE_JS.md`, régénéré le 2026-10-01 contre le tag `EVD` :

| | modules | lignes de code | dont à porter |
|---|---:|---:|---:|
| Porté | 9 | 1 284 | 0 |
| **Partiellement porté** | 31 | 22 671 | **17 379** |
| À porter | 166 | 41 342 | 41 342 |
| À générer (données) | 4 | 1 278 | 0 |
| À jeter | 23 | 3 847 | 0 |
| **Total** | 233 | 70 422 | **58 721** |

L'ancienne version ne connaissait que 8 modules « portés » (couches 0-1) et rangeait tout le reste
en « à porter ». Les tranches verticales (puits → chantier) ont porté des **parties** de modules :
`ported-functions.mjs` les recopie de `PORTAGE.md`, fonction par fonction, et l'inventaire compte
ce qui reste. Deux anciens « portés » sont devenus partiels, à juste titre : `worldArchetypes.js`
(réglages de simulation seuls) et `fieldCrops.js` (6 fonctions sur 14 sans équivalent C++), de
même que `spatialGrid.js` (l'index des animaux n'est pas porté).

## Changer de référence

**On change de référence par une mission dédiée qui régénère TOUS les vecteurs, jamais en
passant `DEC`.** Une mission qui « a besoin d'une fonction plus récente » ne bouge pas la
référence : elle s'arrête et le signale.

La mission de changement :

1. choisit le commit, le pousse sous un **nouveau** tag `anastasis-ref-p<N>` (on ne déplace
   jamais un tag existant ; pas de `--force`) ;
2. régénère **tous** les vecteurs contre lui — `gen-parity.mjs --tous`, `gen-nav-vectors.mjs`,
   `AnastasisParityVectors.inl` — et l'inventaire ;
3. fait passer la suite `Anastasis.Sim` ; chaque test qui tombe est soit un portage à refaire,
   soit une évolution de la référence à porter — jamais un vecteur à corriger à la main ;
4. met à jour ce document, `PORTAGE.md` et les en-têtes qui citent l'ancien commit ;
5. dit dans sa fiche quelles divergences la nouvelle référence introduit.

Entre deux, les écarts entre la référence et le dépôt JS vivant se **déclarent** (comme l'écart
n° 13), ils ne se suivent pas.
