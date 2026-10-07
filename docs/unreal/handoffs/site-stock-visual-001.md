# HANDOFF: site-stock-visual-001

## MISSION

Faire apparaitre les livraisons de bois et de pierre au chantier effectivement ouvert par la simulation. Les lots suivent `Materials.StockWood/StockStone`, diminuent quand le chantier consomme ses ressources et disparaissent a l'achevement. Aucun placement manuel dans la map.

## FILES_OWNED

- `tools/unreal/create-site-stock.py`
- `tools/unreal/create-site-stock.ps1`
- `tools/unreal/site-stock-visual-pie.py`
- `tools/unreal/proofs.txt`
- `Content/Anastasis/SiteStock001/SM_Site_TimberBundle_01.uasset`
- `Content/Anastasis/SiteStock001/SM_Site_StoneBundle_01.uasset`
- `Source/Anastasis_UnrealV2/Village/AnastasisVillageBuilding.h`
- `Source/Anastasis_UnrealV2/Village/AnastasisVillageBuilding.cpp`
- `Source/Anastasis_UnrealV2/Village/AnastasisVillagePresentation.cpp`
- `AGENTS.md` (index des outils)
- cette fiche

Dependance lue, non possedee : `M_AnastasisArchitecture`, les archetypes et `TraceGround` de `architecture-crusade-001`, desormais integres a `main` (base `25ebf1fe`).

## COMMIT

PENDING

## MEC

- BUILD: PENDING
- TESTS: PENDING
- COMMANDS:
  - `py -3 tools/unreal/create-site-stock.py` : GEOMETRY PASS ; bois 94 x 43.52 x 33.39 cm, 204 triangles ; pierre 78 x 57 x 27 cm, 96 triangles.
  - `tools/unreal/create-site-stock.ps1` : PENDING
  - `tools/unreal/anastasis-unreal.ps1 build` : PENDING

## PROOFS

PROOFS: site-stock-visual-pie

## SCN

UNKNOWN : Alexandre a demande de laisser faire la capture. Les deux assets sont des meshes de production, sans image A/B revendiquee.

## PLY

UNKNOWN : la preuve PIE numerique est en attente du lot ; le comptage d'instances lie au stock ne prouve pas encore la lisibilite pour le joueur.

## ECARTS

AUCUN : aucun fichier `Source/AnastasisSim/` modifie ; seule la presentation Unreal lit l'etat du chantier.

## INTEGRATION_RISK

- `AGENTS.md` et `tools/unreal/proofs.txt` sont des fichiers chauds : rebaser et resoudre explicitement si `main` avance avant le lot.
- `architecture-crusade-001` remplace l'ancienne maison de 3,6 m par des archetypes d'environ 10 a 17 m et ajoute `SM_Kit_Woodpile` (pile couverte de 2 m). Les petits lots transitoires de cette mission gardent leur propre mesh ; leur position suit `EntryLocal.X` et `Footprint.Max.Y` de l'archetype applique, avec repli sur les bornes de l'ancien mesh. La recette reutilise `M_AnastasisArchitecture`.
- Les quantites visuelles lisent le stock et le devis de la simulation. Elles ne revendiquent pas la parite entre le devis architectural de chaque archetype et le cout actuel du simulateur.
- `Source/Anastasis_UnrealV2/Village/AnastasisVillageBuilding.cpp` est deja partage avec d'autres missions de presentation ; la collision de merge doit etre inspectee par l'integrateur.
- Le mesh du batiment reste aujourd'hui reduit en Z durant les travaux ; ce lot n'introduit pas encore les etapes de fondation/charpente.
- Le seuil de 1 a 3 lots est une lecture qualitative du stock present, pas une equivalence unitaire ; la preuve verifie la monotonie et la disparition.

## STOP

Ni preuve visuelle, ni amelioration revendiquee du rendu final, ni controle joueur. Aucun changement de simulation, de budget materiel ou du terrain.
