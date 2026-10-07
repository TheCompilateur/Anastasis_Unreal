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

Dependance lue, non possedee : `M_AnastasisArchitecture`, les archetypes et `TraceGround` de `architecture-crusade-001`, deja integres a `main` (base de validation `f64aeac8`).

## COMMIT

Voir `git log` de la branche `agent/site-stock-visual-001` ; `finish` marque le commit final.

## MEC

- BUILD: PASS sur `main` `f64aeac8` + branche `agent/site-stock-visual-001` : 18 actions, `Result: Succeeded`, `BUILD::PASS` (2026-10-07).
- TESTS: preuve PIE locale `PROOF::PASS site-stock-visual-pie` (81,0 s) ; suite `Anastasis` complete reservee au lot d'integration.
- COMMANDS:
  - `py -3 tools/unreal/create-site-stock.py` : GEOMETRY PASS ; bois 94 x 43.52 x 33.39 cm, 204 triangles ; pierre 78 x 57 x 27 cm, 96 triangles.
  - `tools/unreal/create-site-stock.ps1` : `SITE_STOCK::PASS`, deux assets reels, tailles et nombres de triangles relus en editeur ; log local `Saved/SiteStockEvidence/create-site-stock.log`.
  - `tools/unreal/anastasis-unreal.ps1 build` : `BUILD::PASS` ; compilation des deux modules et lien Editor.
  - `tools/unreal/editor-batch.ps1 -Proofs site-stock-visual-pie` : `EDITOR_BATCH::PASS 1/1` ; log local `Saved/EditorBatch/20261007-180246/editor-batch.log`.

## PROOFS

PROOFS: site-stock-visual-pie

## SCN

PIE numerique : le chantier actuel sur `Lvl_AnastasisSlice` charge les deux meshes, sans collision, avec 0/0 instance a sec, 1/1 apres une livraison de chaque materiau, 3/3 au stock plein, puis une diminution observee (bois 18, pierre 4 -> instances 3, 2) et 0/0 a l'achevement. `SITE_STOCK_VISUAL_PIE PASS` est une preuve de projection de la simulation, pas un verdict de qualite visuelle. Alexandre a demande de laisser faire la capture : aucune image A/B revendiquee.

## PLY

UNKNOWN : la preuve PIE numerique ne mesure ni lisibilite a hauteur humaine, ni comprehension du stock par le joueur. Elle doit etre rejouee au lot sur l'arbre integre.

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
