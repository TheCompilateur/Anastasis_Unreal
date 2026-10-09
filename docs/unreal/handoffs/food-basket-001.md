# HANDOFF: food-basket-001

## MISSION

Alexandre : « Ok pour panier » (2026-10-09). Modeliser un panier 3D et le montrer dans la main du cultivateur quand la nourriture portee par la simulation (`FNpc::InventoryFood`) est positive. Le faire disparaitre quand le sac est vide, sans ecrire dans `FVillage`.

## FILES_OWNED

- `tools/unreal/create-food-basket.py` / `.ps1` et `Content/Anastasis/CarriedFood/SM_Food_Basket_01.uasset` : recette et mesh du panier.
- `Source/Anastasis_UnrealV2/Village/AnastasisVillagerVisual.h/.cpp` : composant et pose du panier pres de `hand_l`, CVar d'A/B et lecture de l'etat visuel.
- `Source/Anastasis_UnrealV2/Village/AnastasisVillagePresentation.cpp` : passage de `Npc.InventoryFood` au corps, une ligne.
- `tools/unreal/food-basket-pie.py`, `tools/unreal/proofs.txt`, `AGENTS.md` : preuve et index.
- `docs/unreal/handoffs/food-basket-001.md` : passation.

## COMMIT

PENDING

## MEC

- Premier build du worktree avant modification : PASS.
- Geometrie pure : `py -3 tools/unreal/create-food-basket.py` -> 2116 sommets, 3612 triangles, 48.58 x 37.37 x 48.85 cm, SHA256 `eab0e84b57d80f2833077ae67495af62dd99e254479a48e92e2963251adae962`.
- Index `Test-AnastasisToolsIndex` : Missing {}, Stale {}.
- Build apres modification : PASS (`anastasis-unreal.ps1 build`, Editor Win64 Development).
- Creation dans Unreal : `create-food-basket.ps1` -> `FOOD_BASKET::PASS`, Static Mesh sauve a `/Game/Anastasis/CarriedFood/SM_Food_Basket_01`, 3612 triangles, dimensions Unreal identiques a la recette. Asset `.uasset` suivi par Git LFS.
- Tests sans rendu : PENDING (portail `finish`).

## PROOFS

PROOFS: food-basket-pie

## SCN

- `editor-batch.ps1 -Proofs food-basket-pie` -> `PROOF::PASS food-basket-pie (90.7s)`, `EDITOR_BATCH::PASS 1/1`. Journal : `Saved/EditorBatch/20261009-194813/editor-batch.log` ; donnees : `Saved/FoodBasketEvidence/pie/food-basket-pie.json`.
- `npc-0` : sac 0/panier cache, puis sac 2/panier visible apres recolte reelle ; main gauche au-dessus de la base a 46.8 cm, ecart horizontal 0 cm ; apres livraison reelle, sac vide et panier cache. Collision du composant desactivee. A/B/A a temps simule fixe 40.6333, CVar `anastasis.Village.FoodBasket` 0/1/0.
- Captures inspectees : `01-basket-off.png`, `02-basket-on.png`, `03-basket-off-control.png` dans `Saved/FoodBasketEvidence/pie/`. Meme PNJ/camera, paniers visiblement absent/present/absent. `compare.py` : 9.54 % de pixels > 16 off/on contre 6.62 % off/off sur l'image complete ; sur la zone du panier `(980,440,1150,660)`, 49.91 % contre 3.53 %. La pluie et l'eau changent hors de cette zone.
- Verdict SCN borne : l'objet 3D est effectivement visible sous la main gauche et le changement se lit en gros plan. La prise place le fermier dans l'eau et a contre-jour ; elle ne valide pas sa lisibilite en marche normale sur une parcelle seche ni la qualite finale de la scene.

## PLY

UNKNOWN : aucune marche humaine au rythme normal n'a encore juge la lisibilite du panier.

## ECARTS

AUCUN — aucune modification de `Source/AnastasisSim/` ni de la simulation alimentaire.

## INTEGRATION_RISK

- `AnastasisVillagePresentation.cpp` et `AnastasisVillagerVisual.h/.cpp` sont des fichiers chauds. `dormir-couche-001` les declarait en `FILES_OWNED`, mais `agent-worktree.ps1 status` a classe sa branche `DEJA_DANS_MAIN` par contenu avant cette mission. Aucun acquiescement d'un autre agent n'est revendique.
- La recette cree un nouvel `.uasset` LFS ; le lot doit posseder ce binaire et rejouer `food-basket-pie`.
- Le panier est un signe binaire de nourriture portee ; son contenu modele n'est pas une jauge quantitative.

## STOP

- La preuve locale ou la passation ne valent pas integration dans `main`, ni verdict joueur.
- Pas de geste de recolte, de personnage remodele, de modification des stocks, de nouvelles cultures ou de grenier rempli.
