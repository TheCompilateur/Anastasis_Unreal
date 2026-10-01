# HANDOFF: water-look-claude-01

## MISSION

WATER_LOOK_001 : que l'eau se lise comme de l'eau et plus comme de la peinture — matériau Single
Layer Water, rubans de rivière lisses, berges marquées, couleurs du fond et de la rive. Aucune
modification de la structure du réseau (HYDRO_NETWORK_001). Voir `docs/unreal/WATER_LOOK_001.md`.

## FILES_OWNED

- `tools/unreal/water-look.ps1` / `.py` (nouveaux) — source d'autorité de `M_AnastasisWater`, index dans `AGENTS.md`
- `Content/Anastasis/Materials/M_AnastasisWater.uasset` (nouveau, LFS)
- `Source/Anastasis_UnrealV2/WorldView/AnastasisDrainage.h` / `.cpp` — `FParams::bWaterLook`, `FNetwork::LakeWaterTriangles`, `FWaterRibbons`, `BuildRiverRibbons`, talus droit, recoloration, plafond d'humidité ; tout est derrière `bWaterLook` (faux par défaut)
- `Source/Anastasis_UnrealV2/WorldView/AnastasisDrainageTests.cpp` — `Anastasis.Terrain.Drainage.WaterLook`
- `Source/Anastasis_UnrealV2/WorldView/AnastasisWorldEmbodiment.h` / `.cpp` — CVar `anastasis.Terrain.WaterLook`, section 2 (rubans), `ResolveWaterLookMaterial`
- `docs/unreal/WATER_LOOK_001.md`, cette fiche, `docs/visual/water-look-001/`

## COMMIT

BRANCH_HEAD

## MEC

- BUILD: `BUILD::PASS` (`tools\unreal\anastasis-unreal.ps1 build`, worktree rebasé sur `main` 998537f, code final)
- TESTS: `TESTS::PASS` — 182 PASS / 4 KNOWN_EXPECTED_FAILURE / 0 FAIL, 186 annoncés
  (`tools\unreal\report-tests.ps1`), dont :
  - PASS `Anastasis.Terrain.Drainage.WaterLook` (nouveau) : contrôles du réseau à 0, containment 0.980,
    19 rubans / 2 654 triangles, aucun retourné, 21 868 triangles de nappe, `blue_dry=0`, `blue_bed=0`
  - PASS `Anastasis.Terrain.Drainage.Network` / `.OriginalForms` / `.KeepsAuthoredRivers` /
    `.DeterminismAndSnapshot` (chemin d'avant, inchangé)
  - Ce test a d'abord échoué (12 triangles retournés dans les coudes, 2 800 sommets secs encore bleus) :
    les deux défauts sont corrigés dans le code, pas dans le test.
- MATERIAL: `tools\unreal\water-look.ps1 -Rebuild` → `WATER_MATERIAL::PASS` (carte `/Engine/Maps/Entry`,
  câblage complet, aucun échec après `WATER_MATERIAL_COMPILE`). L'asset commité est celui des captures.
- VISUAL: `docs/visual/water-look-001/` — 4 vues avant/après, même build, `hydro-network-capture.ps1`
  avec `-PreCmds "anastasis.Terrain.WaterLook 0|1"`.
- COMMANDS:
  - `tools\unreal\anastasis-unreal.ps1 build`
  - `tools\unreal\report-tests.ps1`
  - `tools\unreal\water-look.ps1 -Rebuild`
  - `tools\unreal\hydro-network-capture.ps1 -Label final_on -States "1" -Shots <showcase_shots.json> -PreCmds "anastasis.Terrain.WaterLook 1;ShowFlag.Fog 1;ShowFlag.VolumetricFog 1"`

## RISQUES

- `anastasis.Terrain.WaterLook 0` rend exactement l'eau d'avant : tout le code nouveau est derrière
  `FParams::bWaterLook`, faux par défaut, que seule l'incarnation lève depuis la CVar. Les tests
  existants du drainage tournent donc sur le chemin d'avant ; `Drainage.WaterLook` couvre le nouveau.
- `M_AnastasisWater` absent → repli sur `M_AnastasisShoreWater` (log `ANASTASIS_WATER_LOOK material=`).
- Single Layer Water a un coût de rendu propre (passe dédiée) ; non mesuré ici.

## SUITE POSSIBLE

Écume aux confluences et aux ruptures de pente, roseaux de rive, cascades : non commencés.
