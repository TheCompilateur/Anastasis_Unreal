# HANDOFF: ecotone-forge-001

## MISSION

Ajouter la couche naturelle 0,2–3 m (écotones : lisière, sous-bois, rive, pied de pierre) par une bibliothèque de 12 meshes et un dressing déterministe, sans toucher à la topologie ni à la grammaire d'arbres.

## FILES_OWNED

- Source/Anastasis_UnrealV2/WorldView/AnastasisEcotoneDressing.h
- Source/Anastasis_UnrealV2/WorldView/AnastasisEcotoneDressing.cpp
- Source/Anastasis_UnrealV2/WorldView/AnastasisEcotoneDressingTests.cpp
- Source/Anastasis_UnrealV2/WorldView/AnastasisWorldEmbodiment.h
- Source/Anastasis_UnrealV2/WorldView/AnastasisWorldEmbodiment.cpp
- Source/Anastasis_UnrealV2/WorldView/AnastasisTerrainSurfaceTests.cpp
- tools/unreal/create_ecotone_assets.py
- tools/unreal/create-ecotone-assets.ps1
- tools/unreal/capture-ecotone.py
- tools/unreal/capture-ecotone-boards.py
- tools/unreal/capture-ecotone.ps1
- Content/Anastasis/Ecotone/SM_Ecotone_*.uasset
- Content/Anastasis/Materials/M_AnastasisStone.uasset
- docs/unreal/handoffs/ecotone-forge-001.md
- docs/unreal/handoffs/ecotone-forge-001/lineup.png
- docs/unreal/handoffs/ecotone-forge-001/A_macro.png
- docs/unreal/handoffs/ecotone-forge-001/B_macro.png

## COMMIT

BRANCH_HEAD

## MEC

- BUILD: PASS (`tools\unreal\anastasis-unreal.ps1 build` dans ce worktree)
- TESTS:
  - PASS `Anastasis.Ecotone.DeterminismAndAnchoring`
  - PASS `Anastasis.Ecotone.ThreeContexts`
  - PASS `Anastasis.Ecotone.RejectInvalidInput`
  - PASS `Anastasis.Terrain.DressingOnGround` (EcotoneDressing y est explicitement éteint)
- RUNTIME (EmbodyCanonical seed=12345, monde 96x96) :
  - `ANASTASIS_ECOTONE placed=2271 planned=2271 companions=348 understory=202 edge=229 shore=1092 rock=748 missing_meshes=0 ungrounded=0`
- COMMANDS:
  - `tools\unreal\anastasis-unreal.ps1 build`
  - `tools\unreal\create-ecotone-assets.ps1`
  - `tools\unreal\report-tests.ps1 -Filter Anastasis.Ecotone`
  - `tools\unreal\capture-ecotone.ps1`

## SCN

PARTIAL. Les 12 meshes existent, le dressing les pose dans le monde (2271 instances, 4 contextes, 0 mesh manquant). Captures A/B macro du 96x96 : `docs/unreal/handoffs/ecotone-forge-001/A_macro.png` (écotone off) et `B_macro.png` (on). À cette distance le delta 0,2–3 m est à peine lisible — c'est attendu, ce n'est pas l'échelle de la couche. La planche d'assets : `lineup.png`.

Les vues in-world à hauteur d'homme n'ont pas été obtenues : l'hôte faisait tourner plusieurs UnrealEditor en parallèle (OOM pagefile, puis HighResShot qui n'écrit plus). Caméras déjà calculées, non photographiées :

- forest (7827, 4774, 911)
- shore (1316, 5815, 756)
- rock (534, 2167, 1179)

## PLY

PARTIAL. La planche `lineup.png` montre les 12 silhouettes aux tailles réelles (souche ~0,6 m, roseaux ~1,8 m, tronc couché ~2,5 m). La lecture in-world à 1,6 m n'est pas photographiée. Le placement causal (souche→semis, bloc→fragments+herbe, rive→roseaux+bois) est dans `Build` + companions, mesuré (`companions=348`), pas vu.

## INTEGRATION_RISK

- **Collision chaude avec `agent/lithos_forge_001`** : même trio `AnastasisWorldEmbodiment.cpp/.h` + `AnastasisTerrainSurfaceTests.cpp`. Lithos ajoute `AnastasisGeologicalDressing` (parois, éboulis, affleurements). Cette mission ajoute `AnastasisEcotoneDressing` (0,2–3 m). L'intégrateur devra fusionner les deux crochets `Place*Dressing`, pas choisir l'un contre l'autre.
- `AnastasisWorldEmbodiment.cpp` est déjà un fichier chaud (forêt, hydrologie, terrain-forge). Le crochet ici est un bloc isolé + CVar `anastasis.Dressing.Ecotone`.
- Ne touche pas `DA_AnastasisPresentation` ni les meshes d'arbres.
- RockCluster / BuriedBlock sont des fragments de pied, pas les masses de Lithos. Recouvrement possible au pied de pente une fois les deux intégrés — à arbitrer (densité, pas de géométrie).
- `Anastasis.Terrain.DressingOnGround` désactive `EcotoneDressing` : ce test scelle le lift 100 uu des primitives.

## STOP

- Pas de refonte du terrain, de l'eau, de la canopée.
- PLAYER reste NOT_IMPLEMENTED.
- Ce lot n'est **pas** intégré à `main`. Il est prêt à l'être.
- Les captures à hauteur d'homme in-world restent à faire sur un hôte quiescé (un seul éditeur).
