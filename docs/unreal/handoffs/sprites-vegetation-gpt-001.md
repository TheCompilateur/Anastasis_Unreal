# HANDOFF: sprites-vegetation-gpt-001

## MISSION

Transformer les treize sujets de vegetation d'une planche generee par GPT (arbres, arbustes, fougere,
prairie fleurie) en vrais maillages 3D, et placer les arbres dans le zonage de la foret.

Methode, sans modele d'IA image -> 3D : la silhouette de chaque sprite donne des tranches de couronne (volume
de revolution), le fut est mesure sur l'image, les branches partent de l'axe, et le feuillage est fait de
cartes texturees par des pastilles du sprite (atlas 8 x 4). Meme grammaire que les arbres du projet
(normalisation Z = [-50, +50], deux slots, trois LOD, normales spheriques) ; bois et cartes de
`create-tree-cards.py` / `create_tree_asset.py` reutilises.

- 13 maillages `SM_Gpt_<Espece>` + `M_AnastasisGptFoliage` + `T_GptFlora_Atlas` (`/Game/Anastasis/Vegetation/Gpt/`).
- Placement des arbres : cinq essences nouvelles dans l'enum (`DeciduousOak`, `Birch`, `ScotsPine`, `Willow`,
  `HorseChestnut`), zonees par altitude / pente / riviere ; `SM_Gpt_PinSombre` et `SM_Gpt_Cypres` sont une forme de
  plus de `BlackPine` et `Cypress`.

## FILES_OWNED

- `SourceArt/Vegetation/gpt/` (planche source, 13 PNG detoures, atlas, `gpt-flora.json`)
- `tools/unreal/gpt-flora-cut.py`, `gpt-flora-fit.py`, `gpt-flora-preview.py`, `gpt_flora_geometry.py`
- `tools/unreal/create-gpt-flora.ps1`, `create-gpt-flora.py`
- `tools/unreal/set_tree_grammar.py` (GPT_SPECIES), `tools/unreal/capture-tree-lineup.py` (`-Set gpt`, `-Set gpt_low`)
- `Source/Anastasis_UnrealV2/WorldView/AnastasisPresentationRegistry.{h,cpp}`, `AnastasisPresentationResolver.cpp`,
  `AnastasisPresentationResolverTests.cpp`, `AnastasisWorldEmbodiment.cpp`
- `Content/Anastasis/Vegetation/Gpt/`, `Content/Anastasis/Materials/M_AnastasisGptFoliage.uasset`,
  `Content/Anastasis/Presentation/DA_AnastasisPresentation.uasset`
- `docs/visual/sprites-vegetation-gpt-001/`, `AGENTS.md` (index)

## COMMIT

PENDING

## MEC

- BUILD: PASS (`anastasis-unreal.ps1 build`, worktree, apres les changements C++)
- TESTS: la suite sans rendu est rejouee par `finish` (verdict dans le message de passation) ; tests touches :
  `Anastasis.Presentation.TreeSpecies` (familles des cinq essences) et `Anastasis.Presentation.TreeZoning`
  (aptitudes des cinq essences, les sept d'origine toujours atteignables)
- MESURES:
  - `GPT_FLORA_ASSETS::PASS` : 13 maillages, LOD0 / LOD1 / LOD2 triangles = marronnier 3914/3568/816, bouleau 5406/4570/708,
    chene 3022/2822/684, pin sombre 4178/3698/708, pin sylvestre 3054/2802/684, cypres 2622/2472/656, genevrier 1968/1634/696,
    noisetier 1628/1340/656, saule 3724/3410/756, arbuste a baies 1332/1132/600, rhododendron 1256/1078/696,
    fougere 96/82/132, prairie 164/130/132 ; bornes Z = [-50, +50] pour les treize
  - `set_tree_grammar` : `VERIFY variants=36 attendu=36 ... especes=13 attendu=13 -> OK`, `RESULT::PASS`
  - IoU de silhouette de face contre le sprite (rendu logiciel) : marronnier 0,81, rhododendron 0,80, arbuste 0,76,
    saule 0,74, genevrier 0,71, bouleau 0,70, cypres 0,67, pin sombre 0,67, noisetier 0,66, prairie 0,61,
    chene 0,57, pin sylvestre 0,48, fougere 0,44 -- le chiffre dit la silhouette, pas la beaute
  - monde reel (`capture-slice.ps1 -Mode 1`, `ANASTASIS_TREE_TAXA_GPT`) avec les poids d'avant reglage : chene caducifolie 657,
    bouleau 370, pin sylvestre 983, saule 9, marronnier 3 sur 5 489 arbres ; poids du chene et du pin sylvestre baisses
    depuis (0,8 -> 0,6 et 0,9 -> 0,65), comptes non remesures
- COMMANDS:
  - `python tools/unreal/gpt-flora-cut.py` puis `gpt-flora-fit.py` puis `gpt-flora-preview.py` (hors editeur)
  - `tools\unreal\create-gpt-flora.ps1` (maillages, materiau, atlas, registre)
  - `tools\unreal\capture-tree-lineup.ps1 -Out <png> -Set gpt` et `-Set gpt_low`

## PROOFS

PROOFS: (aucune)

## SCN

Images dans `docs/visual/sprites-vegetation-gpt-001/` : `engine-arbres.png` et `engine-petits-sujets.png` (planches a hauteur reelle sous la
lumiere du banc), `monde-oblique.png` et `monde-lisiere.png` (le monde reel, saule en berge et pins au fond visibles),
`decoupe-controle.png` (decoupe), `preview-planche.png` (rendu logiciel). Verdict visuel : Alexandre.
Defauts vus et non corriges : cartes de feuillage des pins en plaques plates ; fut du chene en cone pale ; fougere
en rosette basse qui ne se lit pas comme une fougere ; rhododendron plus rouge que rose.

## PLY

UNKNOWN

## ECARTS

AUCUN — aucun fichier de `Source/AnastasisSim/` touche.

## INTEGRATION_RISK

- `DA_AnastasisPresentation.uasset` (binaire LFS) est reecrit : toute autre mission qui le modifie conflit.
  `set_tree_grammar.py` est idempotent : sur conflit, garder la version de `main` et relancer
  `create-gpt-flora.ps1`.
- La distribution reelle des essences change (cinq essences de plus dans `SelectTreeSpecies`).

## STOP

- Pas de verdict de beaute : celui d'Alexandre, a l'image.
- La face cachee des arbres est une hypothese (couronne de revolution), pas une mesure.
- Anachronismes pour 1204 pontique, a trancher par Alexandre : marronnier d'Inde (Aesculus, absent du Pont a
  cette date) et saule pleureur (forme ornementale tardive). Poids faible mis dans le zonage ; a retirer si besoin.
- Arbustes, fougere et prairie : maillages crees mais PAS places dans le monde (autre systeme : sous-bois /
  couverture au sol).
