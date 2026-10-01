# HANDOFF: hydra-forge-001

## MISSION

Première grammaire hydrologique visuelle : trois archétypes (torrent de
montagne, rivière de vallée, entrée dans un lac) projetés depuis les champs
déjà produits par le simulateur (FlowAmt, FlowX/Z, Shore, Wetness, Alt).
Présentation seulement. Le lot est prêt à être intégré.

## FILES_OWNED

- Source/Anastasis_UnrealV2/WorldView/AnastasisHydrologyDressing.h
- Source/Anastasis_UnrealV2/WorldView/AnastasisHydrologyDressing.cpp
- Source/Anastasis_UnrealV2/WorldView/AnastasisHydrologyDressingTests.cpp
- Source/Anastasis_UnrealV2/WorldView/AnastasisWorldEmbodiment.h
- Source/Anastasis_UnrealV2/WorldView/AnastasisWorldEmbodiment.cpp
- tools/unreal/hydra-capture.py
- tools/unreal/hydra-capture.ps1
- docs/unreal/HYDRA_FORGE_001.md
- docs/unreal/handoffs/hydra-forge-001.md
- docs/visual/hydra-001/

## COMMIT

PENDING

## MEC

- BUILD: `BUILD::PASS` (`tools\unreal\anastasis-unreal.ps1 build` dans le worktree)
- TESTS:
  - PASS `Anastasis.Hydrology.CanonicalArchetypes`
  - PASS `Anastasis.Hydrology.Determinism`
  - PASS `Anastasis.Hydrology.DoesNotMutateSnapshot`
  - PASS `Anastasis.Hydrology.GeometryContract`
  - PASS `Anastasis.Hydrology.RejectsInvalid`
  - PASS `Anastasis.Hydrology.UsesSimFlowGate`
  - PASS `Anastasis.Terrain.DressingOnGround` (non-régression HISM)
  - PASS `Anastasis.Terrain.Contract`
  - PASS `Anastasis.Ecology.DeterminismAndAnchoring`
  - PASS `Anastasis.Ecology.EdgeAndConditioning`
  - PASS `Anastasis.Ecology.RejectInvalidInput`
- COMMANDS:
  - `tools\unreal\anastasis-unreal.ps1 build`
  - `UnrealEditor-Cmd.exe ... -nullrhi -ExecCmds="Automation RunTests Anastasis.Hydrology;Quit"`
  - `tools\unreal\hydra-capture.ps1`

## SCN

Les trois sites existent sur le monde canonique seed=12345 (meilleur exemple
intérieur, pas le barycentre) :

- torrent=(6350, 1050, 275) tiles=177 relief=0.210 width=0.311
- valley=(5350, 1550, 275) tiles=99 relief=0.076 width=0.787
- inflow=(8150, 1450, 275) tiles=113
- flowing=389 still=809 rocks=99 reeds=23 foam=69 flow_tris=778 bank_tris=2286

CVar `anastasis.Dressing.Hydrology` (défaut 1). Sections proc mesh 2/3/4.
Log `ANASTASIS_HYDROLOGY enabled=0` quand la CVar est à 0.

## PLY

PARTIAL. Une première série HighResShot a été observée (torrent / valley /
inflow, aérien, hauteur humaine) : les rubans et berges se lisent, les
premiers cadrages visaient un barycentre et les vues humaines plongeaient
sous le maille. Le code a depuis :

- choisi le meilleur chenal intérieur comme cible caméra ;
- raccourci les berges sur pente forte et plafonné le snap (plus de losanges
  collés aux falaises) ;
- posé les roches de torrent dans le chenal, pas sur la paroi ;
- remonté la caméra humaine au-dessus de l'eau.

La recapture disque a ensuite échoué : HighResShot ne rend pas tant qu'un
autre éditeur tient le GPU, et `filename=` est irrégulier. Relancer
`tools\unreal\hydra-capture.ps1` sur un GPU libre. Les PNG ne sont pas
revendiqués comme preuve scellée.

## PLY

À juger sur les captures `docs/visual/hydra-001/` (torrent / valley / inflow,
avant/après, aérien, oblique, hauteur humaine).

## INTEGRATION_RISK

- `AnastasisWorldEmbodiment.cpp/.h` est un fichier chaud. Hook minimal :
  `ApplyHydrologyDressing` après `PlaceDressing`. Pas de nouveau HISM
  (DressingOnGround continue d'ignorer les sections proc mesh).
- `agent/hydrology-surface` possède `AnastasisTerrainSurface` / `TileColor`.
  Cette mission ne les touche pas.
- `claude/anastasis-shoreline-grammar-e384a1` possède la nappe (section 1) et
  `M_AnastasisShoreWater`. Cette mission n'y touche pas : elle ajoute les
  sections 2/3/4 par-dessus la nappe plate existante.
- Pas de plugin Water Unreal : le sol est un ProceduralMesh, pas un Landscape.
- Le simulateur n'est pas modifié. FlowAmt < 0.06 reste de l'eau stagnante.

## STOP

- Ne revendique pas l'intégration canonique.
- Ne revendique pas un nouveau moteur hydrologique, ni un changement de
  worldgen, ni PLAYER, ni le plugin Water.
- Ne revendique pas la peinture de sol (Wetness/Shore en vertex color) :
  c'est hydrology-surface.
- Ne revendique pas le shader de nappe de rive : c'est shoreline-grammar.
