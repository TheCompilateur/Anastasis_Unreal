# HANDOFF: micro-ecologie-001

## MISSION

Micro-structure écologique sur le terrain déjà forgé : berges en poches irrégulières,
prairie en taches, lisière et sous-bois en groupes, teinte de sol locale. Pas de
reconstruction du relief. Fiche : `docs/unreal/MICRO_ECOLOGY_001.md`.

## FILES_OWNED

- `Source/Anastasis_UnrealV2/WorldView/AnastasisMicroEcology.h`
- `Source/Anastasis_UnrealV2/WorldView/AnastasisMicroEcology.cpp`
- `Source/Anastasis_UnrealV2/WorldView/AnastasisMicroEcologyTests.cpp`
- `Source/Anastasis_UnrealV2/WorldView/AnastasisWorldEmbodiment.h` (appel, CVars, HISM)
- `Source/Anastasis_UnrealV2/WorldView/AnastasisWorldEmbodiment.cpp`
- `docs/unreal/MICRO_ECOLOGY_001.md`
- `docs/unreal/handoffs/micro-ecologie-001.md`

## COMMIT

ce commit

## MEC

- BUILD: PASS (worktree, après rebase sur main le portail `finish` rejoue le build)
- TESTS: `report-tests.ps1 -Filter Anastasis.MicroEcology` → PASS 7 / KNOWN_EXPECTED_FAILURE 0 / FAIL 0, lanceur exit 0
  - Determinism, BankPockets (propre 5, rocheux 32, boueux 25, végétalisé 18, dérive 5, désaccord 0,39)
  - MeadowClusters (pierres 11, buissons 3, sol nu 74, sec 26, humide 1)
  - ForestEdge (buissons 14, gaulis 14, secteurs vides 2/6, sous-bois 10 dont 3 vides)
  - SlopeAndClearing, SoilTint, Rejects
- COMMANDS:
  - `tools\unreal\anastasis-unreal.ps1 build` → `BUILD::PASS`
  - incarnation : `ANASTASIS_MICRO_ECOLOGY instances=19993 components=1196 missing_meshes=0 tinted=8537 truncated=1 plan_ms=613 total_ms=956`

## SCN

Pas de capture avant/après. A/B prévu sur `anastasis.Dressing.MicroEcology` et
`anastasis.MicroEcology.Soil`.

## PLY

NOT_IMPLEMENTED (hors mission).

## INTEGRATION_RISK

- `AnastasisWorldEmbodiment` est le seul fichier partagé. `main` y a ajouté
  `PlaceUnderstory` (FOREST_TERRAIN_P3) et la forêt macro pose déjà une lisière en
  dégradé (P2, canopée / secondaire, toujours pas de strate `Young`). Cette passe
  s'ajoute après `PlaceGroundCover` ; elle ne remplace ni l'understory ni la grammaire
  forestière.
- Empilement visuel possible : buissons et gaulis de lisière (`EdgeBush`, `EdgeSapling`)
  à côté des ronces et du maquis. Couper `anastasis.Dressing.MicroEcology` si le bord
  de forêt double. Les berges (galets, roseaux, vase, bois flotté) et le bois mort
  n'ont pas d'équivalent dans l'understory.
- Plafond 20 000 instances déjà atteint sur le monde entier (`truncated=1`). 1 196
  composants HISM. Coût GPU par frame non mesuré.
- Teinte de sommet après celle de l'herbe. Ne pas régénérer `M_AnastasisGround`.

## STOP

Ne revendique pas : micro-relief, matériau de sol, PNJ, atmosphère, ciel, forêt macro,
maquis P3, textures photo du sol. Pas de graphe PCG (il n'en existe pas).
