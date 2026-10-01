# HANDOFF: forest-terrain-p5

## MISSION

Plus d'oliviers et de platanes. Mesure de la carte reelle apres integration (forest-terrain-yhszr2) :
`olive=23 plane_tree=61` pour 6 556 arbres d'essence (`holm_oak=2806 black_pine=2459`).

## FILES_OWNED

- Source/Anastasis_UnrealV2/WorldView/AnastasisPresentationResolver.h / .cpp (`FTreeSite::bOpenGround`,
  aptitudes Olive et PlaneTree)
- Source/Anastasis_UnrealV2/WorldView/AnastasisPresentationResolverTests.cpp (`TreeZoning`, 3 assertions)
- Source/Anastasis_UnrealV2/WorldView/AnastasisWorldEmbodiment.cpp (`Site.bOpenGround = P.bLone`)
- Source/Anastasis_UnrealV2/WorldView/AnastasisEcologicalDressing.cpp (`LoneDensity`, `GalleryDensity`)

## Ce qui change

| | Avant | Apres |
|---|---|---|
| Olivier, altitude relative | 0-35 % (fondu 12-35 %) | 0-55 % (fondu 25-55 %) : oleastre jusqu'a mi-versant |
| Olivier, pente | < 22 deg | < 28 deg |
| Olivier, arbre isole du pre | x1 | x3 (olivier plante) |
| Platane, rive | des Riparian 0.2, plein a 0.6 | des 0.1, plein a 0.4 ; altitude jusqu'a 75 % ; poids 2.4 -> 3.2 |
| Arbres isoles / galerie de berge | 0.013 / 0.035 par candidate | 0.018 / 0.07 |

Replique Python : olivier 0 -> 38 % des tirages en bas-versant boise sec, 65 % pour un arbre isole de pre ;
platane 39-55 % sur une berge (Riparian 0.3-0.5). Gradient : pire saut 5.1 % par % de relief (< 8 %),
sept essences atteintes, sommets toujours > 98 % sapin + pin noir.

## COMMIT

BRANCH_HEAD (`claude/anastasis-forest-terrain-yhszr2`, repartie de main d31a356)

## MEC

- BUILD: NOT_RUN (conteneur Linux). `tools\unreal\anastasis-unreal.ps1 build`, puis
  `report-tests.ps1 -Filter "Anastasis"`. Aucun asset a regenerer.
- Tests touches : `Anastasis.Presentation.TreeZoning` (+3). A surveiller : `Ecology.MacroForestRenderedHabitat`
  (prairie : au plus un arbre isole pour vingt tuiles ; replique ~125 pour 9 216), `ForestEdgesAndOpenings`.
- A relire : `ANASTASIS_TREE_TAXA` (olive, plane_tree), `ANASTASIS_FOREST_OPEN lone_trees=`.

### Mesure sur le poste (worktree forest-terrain-p5, main 83f12cd + sim-scenario fusionnes)

- BUILD: PASS (`anastasis-unreal.ps1 build`, unity complet, premier build du worktree).
- TESTS: `report-tests.ps1 -Filter "Anastasis"` -> PASS 223, KNOWN_EXPECTED_FAILURE 4 (Fbm, SemantiqueJs,
  deux AnastasisInspect), FAIL 0, 227 annonces. `TreeZoning`, `TreeSpecies`, `MacroForestRenderedHabitat`,
  `MacroForestCanonicalRelief`, `ForestEdgesAndOpenings` : Success.
- Carte canonique (`altitude_span_uu=6592`) :
  `ANASTASIS_TREE_TAXA aleppo_pine=342 cypress=91 holm_oak=2218 olive=226 plane_tree=119 black_pine=1949
  greek_fir=496 untagged=0 real_height=5441`, `ANASTASIS_FOREST_OPEN lone_trees=540 of=5450`.
  Olivier 23 -> 226 (x9.8), platane 61 -> 119 (x2).
- A noter, non explique : le total d'arbres d'essence passe de 6 550 (mesure forest-terrain-yhszr2) a
  5 441, alors que P5 ne fait qu'ajouter des arbres isoles et de galerie. Le main fusionne a change
  entre les deux mesures ; la cause n'est pas cherchee ici.

## SCN / PLY

NOT_ATTEMPTED / UNKNOWN.
