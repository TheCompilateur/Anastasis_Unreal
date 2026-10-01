# HANDOFF: river-look-001

## MISSION

RIVER_LOOK_001 : rendre lisible le courant, la profondeur et le lit de la rivière
déjà calculée par le drainage, sans recréer le réseau ni toucher l'atmosphère,
les PNJ ou la micro-écologie. Voir `docs/unreal/RIVER_LOOK_001.md`.

## FILES_OWNED

- `tools/unreal/water-look.py` — source d'autorité de `M_AnastasisWater`
- `Content/Anastasis/Materials/M_AnastasisWater.uasset` (LFS, régénéré)
- `Source/Anastasis_UnrealV2/WorldView/AnastasisDrainage.h` / `.cpp` — UV1 courbure, UV3 profondeur et pente, teinte du fond immergé
- `Source/Anastasis_UnrealV2/WorldView/AnastasisWorldEmbodiment.cpp` — les rubans portent UV1 et UV3
- `Source/Anastasis_UnrealV2/WorldView/AnastasisDrainageTests.cpp` — canaux du ruban
- `docs/unreal/RIVER_LOOK_001.md`, cette fiche
- `docs/visual/river-look-001/` — caméras et planches avant / après

## COMMIT

BRANCH_HEAD

## MEC

- BUILD: `BUILD::PASS` (`tools\unreal\anastasis-unreal.ps1 build`, worktree `agent/river-look-001`)
- MATERIAL: `tools\unreal\water-look.ps1 -Rebuild` → `WATER_MATERIAL::PASS`, `WATER_MATERIAL_SLW_COUNT 1`
- TESTS: `TESTS::PASS` — `report-tests.ps1 -Filter Anastasis.Terrain.Drainage.WaterLook` (1 PASS, 0 FAIL, 0 KNOWN_EXPECTED_FAILURE)
- VISUAL: mêmes caméras, `anastasis.Sky.Hour 11`, brouillard remis. `compare.py` avant → retake2 :
  - E étroite : 18,30 % des pixels > 16/255 (bruit de capture ~3,6 %)
  - F large : 11,35 %
  - G peu profonde : 15,49 %
  - H profonde, vue haute : 0,73 % — sous le bruit, la surface lointaine ne change pas
- COMMANDS:
  - `tools\unreal\anastasis-unreal.ps1 build`
  - `tools\unreal\water-look.ps1 -Rebuild`
  - `tools\unreal\hydro-network-capture.ps1 -Label avant -States 1 -Shots docs\visual\river-look-001\shots.json -PreCmds "ShowFlag.Fog 1;ShowFlag.VolumetricFog 1;anastasis.Terrain.WaterLook 1;anastasis.Sky.Hour 11"`
  - même commande `-Label retake2 -Shots docs\visual\river-look-001\shots-retake.json` après le matériau adouci
  - `tools\unreal\report-tests.ps1 -Filter Anastasis.Terrain.Drainage.WaterLook`

## SCN

Le réseau seed 12345 reste cohérent sur les captures : uphill=0, narrowing=0,
confluence_narrower=0, dangling=0, isolated=0, lakes_without_role=0,
bank_containment=0.980, 19 rivières, largeur 4,5–42 m.

## PLY

UNKNOWN — PLAYER reste NOT_IMPLEMENTED.

## INTEGRATION_RISK

- `AnastasisDrainage.cpp` et `AnastasisWorldEmbodiment.cpp` sont des fichiers chauds (berge, sol, monde). Le diff est local : `BuildRiverRibbons`, la teinte du fond WaterLook, un appel `CreateMeshSection`.
- `M_AnastasisWater.uasset` est en LFS. Une régénération qui laisse deux nœuds Single Layer Water fait tomber l'eau sur le damier en SM6. Le script compte les nœuds et refuse d'en garder plus d'un.
- Les lacs (UV2 nul) reprennent les cinq vagues d'avant. Un courant plus faible que 0,012 en longueur d'UV2 serait lu comme un lac.

## STOP

- Pas d'étranglement local de largeur : le test du réseau l'interdit vers l'aval.
- Pas de Niagara sur les rochers, pas de caustiques, pas de plugin Water.
- Pas de coucher ni de nuit rephotographiés : l'heure épinglée est 11, et le matériau n'encode pas l'heure.
- Le coin noir au premier plan de la vue étroite et les arêtes de ruban vues d'en haut sont la géométrie des rubans, déjà là avant ce matériau.
- Coût GPU du shader non mesuré en instructions.
