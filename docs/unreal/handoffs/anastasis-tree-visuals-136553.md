# HANDOFF: anastasis-tree-visuals-136553

## MISSION

Remplacer les cones de proxy de la foret par une grammaire d'arbres construite --
silhouette, stature, espece, ombrage et cout -- sans toucher au terrain, a la
simulation, ni a la distribution ecologique.

## FILES_OWNED

- Source/Anastasis_UnrealV2/WorldView/AnastasisPresentationRegistry.{h,cpp}
- Source/Anastasis_UnrealV2/WorldView/AnastasisPresentationResolver.{h,cpp}
- Source/Anastasis_UnrealV2/WorldView/AnastasisPresentationResolverTests.cpp
- Source/Anastasis_UnrealV2/WorldView/AnastasisWorldEmbodiment.cpp
- Content/Anastasis/Vegetation/SM_Tree_*.uasset (8 silhouettes + alias)
- Content/Anastasis/Materials/M_AnastasisVegetation.uasset
- Content/Anastasis/Materials/M_AnastasisBark.uasset
- Content/Anastasis/Presentation/DA_AnastasisPresentation.uasset (entree FOREST seule)
- tools/unreal/create_tree_asset.py
- tools/unreal/set_tree_grammar.py
- tools/unreal/capture-tree-lineup.{py,ps1}
- tools/unreal/measure-tree-cost.{py,ps1}
- docs/visual/tree-form-001/**

Partages, touches avec precaution (voir INTEGRATION_RISK) :

- tools/unreal/set_presentation_meshes.py (ligne FOREST retiree)
- tools/unreal/presentation-registry.py (graine FOREST seule)
- tools/unreal/observe-slice.py + capture-slice.ps1 (ajout additif de camera)

## COMMIT

BRANCH_HEAD

Passes: `3a98b8b` stature, `e2f7382` espece + ombrage, `13b1e3a` deux materiaux,
BRANCH_HEAD cout. Les trois premieres sont deja dans `main`.

## MEC

- BUILD: PASS (`Anastasis_UnrealV2Editor Win64 Development`)
- TESTS: PASS 63 / KNOWN_EXPECTED_FAILURE 4 (les 4 du registre) / FAIL 0
- INVARIANTS SCELLES:
  - `DRESSING_ON_GROUND ground_error=0.000000000` -- les instances reposent
    exactement sur le sol rendu, y compris sur le terrain tessellé de `ea87acc`
  - `TREE_PIVOT variants_checked=8` -- tous les meshes couvrent Z=[-50,+50]
  - `TREE_SLOTS variants_checked=8` -- 2 slots declares, 2 slots nommes
- MESURE DE COUT: 886 instances, 182 490 triangles poses, 206/instance,
  9 composants HISM, 17 draw calls (borne haute), **0 LOD sur 9 meshes**
- COMMANDS:
  - `tools\unreal\anastasis-unreal.ps1 build`
  - `tools\unreal\report-tests.ps1`
  - `tools\unreal\measure-tree-cost.ps1`
  - `UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script=tools/unreal/create_tree_asset.py`
  - `UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script=tools/unreal/set_tree_grammar.py`

## SCN

PASS. Captures au banc scelle `Lvl_AnastasisSlice` (soleil 75000 lux, EV100 fige),
seed 12345, monde 96x96, mode surface -- meme camera avant/apres :

- `docs/visual/tree-form-001/A_close_before.png` etat initial (terrain pre-forge)
- `docs/visual/tree-form-001/C_close_after.png` stature + espece (terrain post-forge)
- `docs/visual/tree-form-001/D_shading_after.png` + ombrage deux faces
- `docs/visual/tree-form-001/E_bark_slot_after.png` + bois/feuillage separes
- `docs/visual/tree-form-001/B_stature_board.png` planche neutre, 8 silhouettes

## PLY

NOT_ATTEMPTED. Aucune preuve joueur : `PLAYER` reste `NOT_IMPLEMENTED` au niveau
projet, et cette mission n'a pas de mandat pour l'ouvrir. Les arbres n'ont jamais
ete vus depuis une camera au sol ni traverses par un pawn.

## INTEGRATION_RISK

- **Le piege non-unity a mordu sur ce tronc, et il remordra.** `3961b6d` ne compilait
  pas sur un arbre PROPRE (`DeepWater` local masquant un global, C4459). Verifie ici
  en compilant `main` seul, detache, dans un worktree neuf ; corrige independamment
  sur le tronc par `d47d5ea`, documente par `1661ea5`. Rien a faire de plus, mais la
  lecon vaut pour toute integration future : un worktree sale ne prouve pas
  l'assemblage, parce qu'UBT sort du unity les fichiers que `git status` voit sales.
- **`AnastasisWorldEmbodiment.cpp` est un fichier chaud.** `ea87acc`
  (terrain-forge) l'a modifie en meme temps que moi. Les deux passes etaient
  disjointes, mais tout agent qui y touche doit recompiler l'assemblage.
- **`set_presentation_meshes.py` ne doit plus cibler FOREST.** Il n'ecrit que
  `variants[0].mesh` ; le relancer sur Forest ecraserait la variante understory
  par le mesh generique et casserait la grammaire en silence. Autorite: `set_tree_grammar.py`.
- **Convention de pivot non negociable.** Deux formules de lift coexistent dans
  `PlaceDressing` (heritee `0.5*EngineBasicShapeSize*Scale`, ecologique
  `-MeshBounds.Min.Z*Scale`). Elles ne coincident que si chaque mesh couvre
  Z=[-50,+50]. Un mesh hors convention fait flotter les arbres dans UN SEUL des
  deux chemins. Verifie par le generateur et par `TREE_PIVOT`.
- **`enable_recompute_normals` doit rester `False`** dans les options de build du
  generateur : a `True` il jette en silence les normales fractionnees.
- **Aucun LOD.** Sans consequence a 886 instances, facteur limitant vers 20 000
  (4,1 M triangles soumis a toute distance). Chiffre, pas suppose.
- **Le gradient d'espece n'a qu'un seul seed de preuve** (12345). Ses constantes
  sont reglees sur la distribution mesuree de ce monde.
- **La graine `RUIN` de `presentation-registry.py` pointe encore sur
  `/Engine/BasicShapes/Cylinder.Cylinder`** alors que l'asset reel est
  `SM_Ruin_Generic_01`. Derive preexistante, chantier d'un autre agent : signalee,
  pas corrigee.

## STOP

Cette mission ne revendique PAS :

- le terrain, l'hydrologie, les materiaux de sol (`M_AnastasisSlice`)
- la simulation (`Source/AnastasisSim/` intact)
- la distribution ecologique (`AnastasisEcologicalDressing` intact -- 886 instances
  avant, 886 apres, aux memes endroits)
- l'archetype `Ruin`
- la hauteur litterale des arbres : 6,3 m au maximum contre 10-25 m pour un
  conifere pontique reel, borne par la densite de la distribution scellee
- toute preuve joueur
