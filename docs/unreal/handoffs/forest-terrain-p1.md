# HANDOFF: forest-terrain-p1

## MISSION

Phase P1 de la mission foret/terrain : remplacer la flore boreale (epiceas, sapins, hetres
« boules ») par une flore de Grece byzantine zonee par l'altitude du relief EXISTANT, a des
proportions et des hauteurs reelles, avec une variation d'echelle, de rotation et de teinte par
arbre. Sans toucher au village, au simulateur, ni a la carte.

## FILES_OWNED

- Source/Anastasis_UnrealV2/WorldView/AnastasisPresentationRegistry.h / .cpp (enum
  `EAnastasisTreeSpecies`, champs `Species` et `HeightRangeM` de variante, essences dans le repli code)
- Source/Anastasis_UnrealV2/WorldView/AnastasisPresentationResolver.h / .cpp (`FTreeSite`,
  `SpeciesSuitability`, `SelectTreeSpecies`, `FamilyOfSpecies`, `SpeciesName`, pool par essence)
- Source/Anastasis_UnrealV2/WorldView/AnastasisPresentationResolverTests.cpp (2 tests)
- Source/Anastasis_UnrealV2/WorldView/AnastasisEcologicalDressing.h / .cpp (`FPlacement::Maturity`)
- Source/Anastasis_UnrealV2/WorldView/AnastasisWorldEmbodiment.cpp (boucle foret, CVar
  `anastasis.Dressing.TreeSpecies`, donnees par instance, log `ANASTASIS_TREE_TAXA`)
- tools/unreal/create_tree_asset.py (21 meshes, materiau de feuillage)
- tools/unreal/set_tree_grammar.py (entree FOREST)
- tools/unreal/capture-tree-lineup.py / .ps1 (`-Set species`)
- AGENTS.md (index tools/unreal, trois lignes)

## Ce qui change

| Defaut releve | Correctif |
|---|---|
| Sapins et arbres « boules », aspect boreal | 7 essences : pin d'Alep, cypres, chene vert, olivier, platane d'Orient en bas ; pin noir et sapin de Cephalonie en haut |
| Troncs longs et nus, couronnes petites | Recettes aux rapports reels (tableau dans `create_tree_asset.py`), rayon de tronc reel (plus le facteur 0.55). Mesure hors moteur de la recette : fut nu pin d'Alep 44-46 %, chene vert 19-31 %, olivier w/h 1.04-1.07, platane 0.71-0.78, cypres 0.15-0.18 |
| Echelles incoherentes | Hauteur = plage reelle de l'essence (m) x maturite de l'arbre, au lieu de trois enveloppes multipliees. Platane 17-24 m, pin noir 15-23, sapin 14-22, cypres 12-20, pin d'Alep 11-18, chene vert 8-14, olivier 4.5-8 |
| Sapins tous identiques | 3 formes par essence (graine par nom), largeur de couronne +-12 % par arbre independante de la hauteur, lacet aleatoire |
| Teinte uniforme | 2 flottants par instance : secheresse du site (couronne vers l'olive paille loin de l'eau) et ecart de valeur +-12 %. Nuls hors foret : les acteurs poses a la main rendent comme avant |
| Vent identique pour tous (4 uu) | Offset calcule en espace local puis transforme : proportionnel a la taille, croissant avec la hauteur dans la couronne |

**Zonage.** Altitude RELATIVE au relief rendu (`(Z - eau) / (Zmax - eau)`), pente rendue, riviere
rendue (`RiparianAt`), plus `Wetness` et `Shade` de la simulation. Aptitudes douces, bandes
chevauchantes, tirage par hachage du site. Le platane n'existe qu'au bord de l'eau, le sapin
prefere les versants frais (`Shade < 0`), le cypres les pentes rocheuses. Verifie hors moteur sur
une copie exacte des fonctions : ecart maximal de 3.9 % de composition par 1 % de relief, et les
sept essences sont atteintes.

**Compatibilite.** `Species = Any` et `HeightRangeM = (0,0)` par defaut : un registre ancien
rend exactement comme avant. La grille pontique reste dans l'entree comme repli non etiquete :
une essence dont le mesh n'est pas encore genere retombe sur elle (warning
`ANASTASIS_PRESENTATION_MISSING_SPECIES_MESH`). `anastasis.Dressing.TreeSpecies 0` rend la
grammaire pontique d'origine : memes troncs, memes positions (A/B).

## COMMIT

BRANCH_HEAD (`claude/anastasis-forest-terrain-yhszr2`)

## MEC

- BUILD: **NOT_RUN** (conteneur Linux sans Unreal). Ordre sur le poste :
  1. `tools\unreal\anastasis-unreal.ps1 build`
  2. editeur dedie : `py tools/unreal/create_tree_asset.py` -> `RESULT::PASS meshes=30 species_meshes=21`,
     et la ligne `MATERIAL ... tint=per_instance ... wind=local_height` (un `WARN` y dit quel repli a joue)
  3. `UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script=tools/unreal/set_tree_grammar.py`
     -> `VERIFY ... especes=8 attendu=8 ... -> OK`, puis second run : `deja conforme`
  4. `tools\unreal\report-tests.ps1 -Filter "Anastasis.Presentation"` puis `-Filter "Anastasis"`
- Nouveaux tests : `Anastasis.Presentation.TreeSpecies` (pool par essence, repli, familles),
  `Anastasis.Presentation.TreeZoning` (zonage, gradient, determinisme, NaN).
- Tests a surveiller : `Anastasis.Presentation.TreePivotConvention` (21 meshes nouveaux, Z = [-50,+50] par
  normalisation), `TreeMaterialSlots` (2 slots), `Anastasis.Terrain.HumanGeography.CollisionAndDressing`
  (plafond 30 m : max theorique 27.6 m, platane emergent), `Anastasis.Ecology.*` (ScaleMultiplier
  inchange, bit a bit).
- Log a lire : `ANASTASIS_TREE_TAXA` (repartition reelle par essence, `untagged` doit etre 0 apres
  les etapes 2-3), `ANASTASIS_TREE_STATURE height_uu=[min,max]`.

## SCN

NOT_ATTEMPTED. A faire :
- `tools\unreal\capture-tree-lineup.ps1 -Out species_01.png -Set species` (et `-Shape 02`, `03`)
- A/B monde : `anastasis.Dressing.TreeSpecies 0/1` aux memes cameras (`capture-macro-forest.py`,
  `capture-forest-walk.py`).

## PLY

UNKNOWN.

## INTEGRATION_RISK

- API Python non verifiee sur ce build (le script le signale par WARN et se replie) :
  `MaterialExpressionPerInstanceCustomData`, `MaterialExpressionLocalPosition`,
  `MaterialExpressionTransform` et ses enums `MaterialVectorCoordTransform(Source)`.
- `AltitudeSpan` vient du pic le plus haut rendu : si le relief a un seul sommet isole, les
  etages superieurs seront rares. A lire dans `ANASTASIS_TREE_TAXA` avant de retoucher les bandes
  de `SpeciesSuitability`.
- Hors perimetre P1, toujours pontiques : `AnastasisPlaces` (« vieux chene » = mesh de hetre,
  vieille foret en sapins) et les 127 acteurs WD01 sauves dans `Lvl_AnastasisSlice`.
- Pas encore traite (P2/P3) : lisieres sur la grille de tuiles, densite saturee, strate
  arbustive, rochers.
