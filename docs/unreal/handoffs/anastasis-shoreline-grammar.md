# HANDOFF: anastasis-shoreline-grammar

## MISSION

Faire cesser le bord d'eau d'etre une coupure : la nappe d'eau lit desormais la
profondeur, la platitude de berge et le courant que le monde contenait deja, et
les rend en marge, eau peu profonde, eau franche.

## FILES_OWNED

- `Source/Anastasis_UnrealV2/WorldView/AnastasisTerrainSurface.h`
- `Source/Anastasis_UnrealV2/WorldView/AnastasisTerrainSurface.cpp`
- `Source/Anastasis_UnrealV2/WorldView/AnastasisTerrainSurfaceTests.cpp`
- `Source/Anastasis_UnrealV2/WorldView/AnastasisWorldEmbodiment.h` (section 1 seule)
- `Source/Anastasis_UnrealV2/WorldView/AnastasisWorldEmbodiment.cpp` (section 1 seule)
- `Content/Anastasis/Materials/M_AnastasisShoreWater.uasset`
- `tools/unreal/shore-water.py` / `.ps1`
- `tools/unreal/shore-capture.py` / `.ps1`
- `docs/unreal/SHORELINE_FORGE_001.md`
- `docs/visual/shoreline-001/`

## COMMIT

BRANCH_HEAD

## MEC

- BUILD: PASS (`main` a `4908e06`, editeur Win64 Development)
- TESTS: PASS 68 / KNOWN_EXPECTED_FAILURE 2 / FAIL 0 / TOTAL 70
  - les 2 connus sont `Anastasis.Sim.Parite.Fbm` et `...SemantiqueJs`, du registre,
    sans rapport avec cette mission
  - nouveau test bloquant : `Anastasis.Terrain.Shoreline`
- MARQUEURS SCELLES INCHANGES, caractere pour caractere :
  - `TERRAIN_CONTRACT vertices=1024 triangles=1922 max_error=0.000000000 boundary_edges=124`
  - `TERRAIN_SEMANTICS water_tiles=151 land_tiles=873 shore_tiles=158 water_quads=187`
  - `TERRAIN_EXTENT world=96x96 vertices=9216 triangles=18050 boundary_edges=380`
- MESURES SORTIES DU TEST (pas affirmees) :
  - `TERRAIN_SHORELINE_FORGED vertices=145161 submerged=16234 margin=12593 full_depth=3641 flowing=4056 depth_uu_max=614 span_uu=60`
  - `TERRAIN_SHORELINE_FORGED_DEPTHS p10=11.1 p50=41.2 p90=78.6`
  - `TERRAIN_SHORELINE_FORGED_FAMILIES soft=50 steep=482 flowing=112` (hors bordure)
- COMMANDS:
  - `& 'C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat' Anastasis_UnrealV2Editor Win64 Development -Project=<uproject> -WaitMutex -NoHotReloadFromIDE`
  - `UnrealEditor-Cmd.exe <uproject> -unattended -ExecCmds="Automation RunTests Anastasis" -testexit="Automation Test Queue Empty"`
  - `tools\unreal\shore-water.ps1 -Rebuild`
  - `tools\unreal\shore-capture.ps1 -Out <png> -View <vue> -Mode 0|1`

## SCN

PASS, partiel et borne.

Trois paires A/B dans `docs/visual/shoreline-001/`, prises a la meme camera au uu
pres sur le relief forge, seule `anastasis.Terrain.Shoreline` change :
`A_close` (gain fort), `B_mid` (gain reel, plus discret), `C_aerial` (gain net).

Le log prouve le levier : `channels=145161 water_vertices=145161 forged=1` en
mode 1, `channels=0` en mode 0, geometrie identique des deux cotes.

**NOT_ATTEMPTED** pour les variantes de rive (GATE 7) : mesurees, pas
photographiees. Voir INTEGRATION_RISK.

## PLY

NOT_ATTEMPTED. `PLAYER` reste `NOT_IMPLEMENTED` au projet ; aucune preuve joueur
n'est revendiquee, et la nappe n'a pas de collision.

## INTEGRATION_RISK

- **Fichier chaud : `AnastasisWorldEmbodiment.cpp`.** Cette mission n'y touche que
  la section 1 (nappe d'eau). GROUND_SURFACE_001 (`claude/anastasis-ground-materials-727cc2`,
  non integree) y touche la section 0. Les deux branches declarent la meme methode
  `ResolveWaterMaterial()` : la resolution est additive, garder le sol de l'une et
  le materiau d'eau de l'autre. **Integrer le sol d'abord.**
- **Dependance de reglage.** La marge de rive est calee contre un sol encore
  surexpose. Si GROUND_SURFACE_001 change l'albedo du sol, les teintes de
  `shore-water.py` seront a revoir.
- **`AnastasisTerrainForge` n'est pas modifie**, mais cette mission en depend :
  `Apply` remplace la geometrie entiere, donc `FillShorelineChannels` doit etre
  rappele apres. Si un futur chemin remplace encore la geometrie sans rappeler
  cette fonction, la nappe repasse a Depth=0 et devient invisible. Le test
  `Anastasis.Terrain.Shoreline` echoue dans ce cas — c'est sa raison d'etre.
- **Cout GPU non mesure.** La section 1 passe d'un materiau opaque a un
  translucide a eclairage par pixel, sur jusqu'a 42 168 triangles d'eau.

## STOP

Cette mission ne revendique pas :

- la lecture << limon >> de la marge. Elle rend un plateau d'eau peu profonde.
  Le relief immerge est peint en bleu par `TileColor`, qui appartient a
  GROUND_SURFACE_001 ; la corriger est une ligne, mais apres integration.
- la moindre bande ecologique (GATE 6). Aucune plante, aucune pierre. Le gain
  visible ne depend d'aucun decor.
- les variantes de rive en image (GATE 7). Les trois familles sont mesurees et le
  test les exige ; trois des quatre inconnues de cadrage ont ete resolues par la
  mesure (direction de l'eau, hauteur de degagement, exclusion des bordures). La
  quatrieme reste : le recul doit suivre la TAILLE du sujet, et les deux familles
  non-A de ce monde sont des chenaux etroits en ravin.
- un gain a hauteur d'oeil. En incidence rasante la marge se comprime.
- toute declaration sur l'etat de `main`.
