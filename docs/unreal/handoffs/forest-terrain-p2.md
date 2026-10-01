# HANDOFF: forest-terrain-p2

## MISSION

Phase P2 de la mission foret/terrain : distribution par regles. Lisieres en degrade au lieu de
lignes droites sur la grille, densite variable, clairieres, arbres isoles et bosquets dans les
pres, galerie le long des rivieres. Mode macro seulement (le mode du jeu) ; le chemin par tuile
reste bit a bit celui d'avant.

## FILES_OWNED

- Source/Anastasis_UnrealV2/WorldView/AnastasisEcologicalDressing.h / .cpp
- Source/Anastasis_UnrealV2/WorldView/AnastasisEcologicalDressingTests.cpp
- Source/Anastasis_UnrealV2/WorldView/AnastasisHumanGeography.h / .cpp (`FSample::RoadWeight`, lecture seule)
- Source/Anastasis_UnrealV2/WorldView/AnastasisWorldEmbodiment.cpp (log `ANASTASIS_FOREST_OPEN`)

## Causes et correctifs

| Defaut | Cause | Correctif (constantes `P2::` dans le .cpp) |
|---|---|---|
| Lisieres droites | tests par tuile du candidat (`T.Type == Grass`, `T.Wetness`) : chaque regle tracait une ligne a la taille de la tuile | part d'habitat ponderee en cone sur 3 tuiles, autour d'un point deforme (+-0.45 tuile) ; herbe et humidite lues en bilineaire |
| Densite uniforme | 128 candidats par tuile contre ~16 places a 5 m : chaque tuile saturait | 2 candidats par cellule d'espacement (32 par tuile) : la probabilite redevient une densite ; peuplement 0.55-1 (bruit, 5 tuiles) |
| Pas de clairiere interne | seul le bruit de masse (14 tuiles) ouvrait | clairieres (bruit, 3.5 tuiles) jusqu'a -92 % |
| Lisiere de la meme hauteur que l'interieur | strate tiree sans lien au bord | lisiere, clairiere et peuplement clair recrutent plus de sous-canopee : la hauteur descend vers le bord |
| Prairie vide | vallees et prairie basse fermees (`Opening = 0`) | arbres isoles et bosquets (0.013 par candidat x bruit de bosquet), galerie de berge (0.035 x bande riveraine) ; memes reserves que la foret : bassin du village, lit de riviere, route du col (`RoadWeight`) ; jamais sur un champ ni une ruine |

Mesure sur une replique Python de `Build` (bord foret/champ droit, echelle 5, altitude), densite par
demi-tuile depuis le bord, rapportee a l'interieur :

```
avant  : 106 %  137 %  96 % ...   coupe pleine densite sur la grille
apres  :   7 %   35 %  79 % ...   lisiere de ~30 m
```

Interieur : coefficient de variation des blocs 3x3 tuiles 0.69, 120 blocs 2x2 vides (clairieres).
Prairie basse plate (echelle 1) : 90 arbres, tous isoles. (Chiffres apres la correction des graines
de bruit en P3 : avant elle, le bruit de clairiere etait correle au bruit de masse.) La foret totale baisse (~40 % sur le
fixture) : un peuplement mediterraneen est plus ouvert que la foret pontique saturee.

## COMMIT

BRANCH_HEAD (`claude/anastasis-forest-terrain-yhszr2`)

## MEC

- BUILD: **NOT_RUN** (conteneur Linux sans Unreal).
  `tools\unreal\anastasis-unreal.ps1 build` puis `tools\unreal\report-tests.ps1 -Filter "Anastasis.Ecology"`.
- Nouveau test : `Anastasis.Ecology.ForestEdgesAndOpenings` (lisiere clairsemee puis comblee, CV > 0.35,
  >= 10 clairieres 2x2). Seuils tires de la replique avec marge (7 % < 40 %, 0.69 > 0.35, 120 >= 10).
- Tests MODIFIES, sur mandat (« des arbres isoles dans les pres ») :
  - `MacroForestRenderedHabitat` : « flat low prairie stays open » (0 arbre) devient « aucune masse,
    seulement des arbres isoles, au plus un pour vingt tuiles, et au moins un ».
  - `MacroForestCanonicalRelief` : « valleys remain open » ne vaut plus que pour les arbres de foret
    (`!bLone`) ; ajout « aucun arbre sur la route du col » (`RoadWeight < 0.3`). Lit de riviere et
    bassin inchanges, pour tous.
- Log : `ANASTASIS_FOREST_OPEN lone_trees=N of=M`.

## SCN

NOT_ATTEMPTED. `capture-macro-forest.py` / `capture-forest-walk.py` et `ground-cover-capture.py`
(cameras prairie, lisiere) avant/apres.

## PLY

UNKNOWN.

## INTEGRATION_RISK

- `Anastasis.Ecology.MacroForestCanonicalRelief` exige « canopy dominates » : la part de canopee
  attendue est 0.5 + 0.42 x interieur moyen, donc > 50 % par construction, mais non mesuree sur le
  monde canonique.
- Le nombre d'arbres change : `ANASTASIS_ECOLOGY` / `ANASTASIS_TREE_TAXA`, et le cout GPU baisse.
- Hors perimetre : arbustes de lisiere de `AnastasisPlaces` (vieille foret), encore poses sur les
  bords de tuiles -- P3, avec la strate arbustive.
