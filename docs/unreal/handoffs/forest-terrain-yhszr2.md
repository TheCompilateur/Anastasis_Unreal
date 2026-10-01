# HANDOFF: forest-terrain-yhszr2

## MISSION

Passer sur le poste la branche `claude/anastasis-forest-terrain-yhszr2` (PR #1, phases P0 a P4,
ecrites sans Unreal) : compiler, regenerer les assets qu'elle decrit, faire passer la suite
`Anastasis`, verser. Le detail de chaque phase reste dans `forest-terrain-p0.md` a `p4.md`.

## FILES_OWNED

- Source/Anastasis_UnrealV2/WorldView/AnastasisEcologicalDressingTests.cpp (`FPlan` qualifie)
- Source/Anastasis_UnrealV2/WorldView/AnastasisWorldEmbodiment.cpp (`PlaceUnderstory` : `UnderPlan`)
- Content/Anastasis/Vegetation/SM_Tree_* (21 essences + 9 pontiques regeneres), SM_Shrub_* (12)
- Content/Anastasis/Materials/M_AnastasisVegetation, M_AnastasisBark, M_AnastasisRock, M_AnastasisGrass
- Content/Anastasis/Materials/M_AnastasisGround, MI_AnastasisGround (regeneres apres fusion de main)
- Content/Anastasis/GroundCover/SM_Grass_* (5)
- Content/Anastasis/Presentation/DA_AnastasisPresentation (entree FOREST)
- docs/unreal/handoffs/forest-terrain-yhszr2.md

## COMMIT

BRANCH_HEAD (`agent/forest-terrain-yhszr2`, pousse sur `claude/anastasis-forest-terrain-yhszr2`)

## MEC

- BUILD: PASS. Le premier build echouait sur deux erreurs MSVC que le conteneur Linux n'avait pas vues :
  - `AnastasisEcologicalDressingTests.cpp(272)` C2872 `FPlan` ambigu (`AnastasisWorldView::FPlan`
    contre `AnastasisEcologicalDressing::FPlan`, les deux `using namespace`) ;
  - `AnastasisWorldEmbodiment.cpp(776)` C4458 : le `US::FPlan Plan` local masquait `AAnastasisWorldEmbodiment::Plan`.
  Apres correction `BUILD::PASS`, puis de nouveau `BUILD::PASS` apres fusion de main.
- ASSETS (editeur dedie sur `/Engine/Maps/Entry`, lance par `Start-AnastasisEditor`) :
  - `create_tree_asset.py` -> `RESULT::PASS meshes=42 species_meshes=21 shrub_meshes=12`,
    `MATERIAL ... tint=per_instance ... wind=local_height` (aucun repli WARN), `ROCK_MATERIAL saved`.
  - `set_tree_grammar.py` -> `VERIFY variants=29 attendu=29 statures=5 familles=2 especes=8 attendu=8 -> OK` ;
    second process : `FOREST entry[0] deja conforme`, meme VERIFY.
  - `ground-material.ps1 -Rebuild` -> `GROUND_MATERIAL::PASS`, 159 expressions, `TEXTURES_READY count=8`,
    991 instructions pixel, 10 samplers ; aucun `Failed to compile` dans le log.
    Note : `ground-material.ps1` ecrase `ANASTASIS_GROUND_REBUILD` depuis son switch ; la variable
    d'environnement seule ne regenere rien, il faut `-Rebuild`.
  - `create-ground-cover.ps1` -> `GROUND_COVER_ASSETS::PASS` (MeadowTall [1738, 620, 67], MeadowShort [1200, 339, 75]).
- TESTS (`tools\unreal\report-tests.ps1 -Filter "Anastasis"`, avant fusion de main) :
  - PASS 197, KNOWN_EXPECTED_FAILURE 4 (Fbm, SemantiqueJs, deux AnastasisInspect, registre), FAIL 0,
    201 annonces par le lanceur, `TESTS::PASS`.
  - Nouveaux tests verts : `Anastasis.Presentation.TreeSpecies`, `.TreeZoning`,
    `Anastasis.Ecology.ForestEdgesAndOpenings`, `Anastasis.Understory.SlopeAltitudeAndReserves`,
    `.EdgesRiversAndSpecies`. Surveilles verts : `TreePivotConvention`, `TreeMaterialSlots`,
    `MacroForestRenderedHabitat`, `MacroForestCanonicalRelief`, `HumanGeography.RiverAndOutlet`,
    `.CollisionAndDressing`, `Drainage.WaterLook`, `Terrain.Semantics`, `HydrologyGradient`.
  - Le run de `finish` apres fusion fait foi pour l'etat verse.
- LOG (monde canonique, sous automatisation) :
  - `ANASTASIS_TREE_TAXA aleppo_pine=479 cypress=129 holm_oak=2806 olive=23 plane_tree=61 black_pine=2459
    greek_fir=593 untagged=0 altitude_span_uu=6592`
  - `ANASTASIS_UNDERSTORY lentisk=2398 kermes_oak=5539 broom=6136 bramble=6233 rock=5511 placed=25817
    missing_meshes=0 truncated=0 plan_ms=170.3`
  - `FOREST_EDGE columns=[8 43 97 135 106 147] cv=0.69 empty_2x2=120 lone=56` (seuils du test atteints)

## SCN

NOT_ATTEMPTED. Aucune capture : les A/B conseilles par P0-P4 (`capture-tree-lineup.ps1 -Set species`,
`capture-forest-walk.py`, `ground-cover-capture.py`, `capture-horizon.ps1`) restent a faire.

## PLY

UNKNOWN. Collision des rochers non eprouvee en marchant.

## INTEGRATION_RISK

- Fusion de main (ATMOSPHERE_COHERENCE_001, GROUND_TEXTURE_001) sans conflit textuel. Seul chevauchement
  reel : `ground-material.py` ; les blocs P4 (pans regionaux, terra rossa) teintent la couleur avant
  que la texture de detail de GROUND_TEXTURE_001 la module. `M_AnastasisGround` / `MI_AnastasisGround`
  sont regeneres depuis le script fusionne : toute mission qui regenere le sol depuis un script sans
  P4 effacera P4.
- Olivier rare (23) et platane rare (61) sur la carte reelle : a lire en image avant de retoucher les
  bandes de `SpeciesSuitability`.
- 25 817 instances de sous-bois et 6 550 arbres d'essence sur la carte : cout GPU non mesure.

## STOP

Ne revendique aucun rendu : aucune image n'a ete regardee. Ne revendique pas que l'atmosphere
s'accorde a la nouvelle palette (P4 l'exclut). PLAYER non touche.
