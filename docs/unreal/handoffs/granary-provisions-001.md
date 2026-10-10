# HANDOFF: granary-provisions-001

## MISSION

Modeliser une caisse de provisions fermee et projeter le stock physique du grenier en 0 a 8 caisses 3D pres de son entree. Le stock reste exclusivement dans `AnastasisVillage::FBuilding::FoodPhysical` ; les caisses ne produisent aucune nourriture et ne bloquent ni collision ni navigation.

## FILES_OWNED

- `Content/Anastasis/GranaryProvisions/SM_Granary_ProvisionCrate_01.uasset`
- `Source/Anastasis_UnrealV2/Village/AnastasisVillageBuilding.{h,cpp}`
- `Source/Anastasis_UnrealV2/Village/AnastasisVillagePresentation.cpp`
- `tools/unreal/create-granary-provisions.{py,ps1}`
- `tools/unreal/granary-provisions-pie.py`
- `tools/unreal/proofs.txt`
- `AGENTS.md`
- `docs/unreal/handoffs/granary-provisions-001.md`

## COMMIT

Le commit de passation est le HEAD de `agent/granary-provisions-001` marque par `agent-worktree.ps1 finish` ; consulter `.handoff/granary-provisions-001.txt` pour son empreinte et son verdict.

## MEC

- BUILD: `BUILD::PASS` apres stabilisation des sources ; premier build avait compile mais refusait son empreinte, car les edits etaient en cours.
- Geometrie pure: 888 vertices, 660 triangles, 129.53 x 82.5 x 69.1 cm. Le SHA256 brut differe entre Python systeme et Python Unreal (calculs trigonométriques flottants) ; le format, les dimensions et le nombre de triangles sont egaux.
- Asset Unreal: `GRANARY_PROVISIONS::PASS`, mesh 129.53 x 82.5 x 69.1 cm, 660 triangles, empreinte Unreal `cafc32da060a9d6d6023cad12113f807ec5c3abe747a102d4b6b826c70373340`, collision desactivee.
- PIE local: `PROOF::PASS granary-provisions-pie` (128.5 s, run final `Saved/EditorBatch/20261010-112138/`). Grenier 0 stock / 0 caisse, puis `gathered=8`, `delivered=5`, `FoodPhysical=5`, `count=1` ; temps simule fige a 52.9667 pendant 0/1/0 caisse ; stock et livraison constants.
- TESTS finish: verdict de `finish` sur le commit marque ; le journal de passation fait foi.
- COMMANDS: `py -3 tools/unreal/create-granary-provisions.py`; `tools/unreal/anastasis-unreal.ps1 build`; `tools/unreal/create-granary-provisions.ps1`; `tools/unreal/editor-batch.ps1 -Proofs granary-provisions-pie`.

## PROOFS

PROOFS: granary-provisions-pie

## SCN

Images `Saved/GranaryProvisionsEvidence/pie/{01-crates-off,02-crates-on,03-crates-off-control}.png` lues. Meme camera, meme stock et temps simule figes. `compare.py` : OFF/ON 2.16 % des pixels different de plus de 16/255 ; OFF/OFF 0.50 %. La heatmap `diff-on.png` concentre le changement sur la caisse et son ombre. La caisse est entiere, posee sur la terrasse et lisible pres de l'entree. Le premier essai local etait presque enterre ; correction du niveau de terrasse puis nouveau A/B/A, aucun resultat du premier essai revendique.

## PLY

UNKNOWN : camera de preuve proche a hauteur d'homme, mais pas de marche controlee avec le joueur, ni d'evaluation de la lisibilite depuis les trajets habituels.

## ECARTS

AUCUN : `Source/AnastasisSim/` non modifie.

## INTEGRATION_RISK

- `AnastasisVillageBuilding.cpp`, `AnastasisVillagePresentation.cpp` et `tools/unreal/proofs.txt` sont des fichiers partages avec d'autres missions ; resoudre les conflits au lot.
- Le mesh architectural du storehouse contient deja des jarres et des sacs fixes. Ils ne suivent pas `FoodPhysical` : ne pas interpreter leur presence comme preuve de stock. La caisse dynamique situee dehors est la seule projection ajoutee ici.
- Mission independante de `field-grain-001` : la preuve seme son propre fermier et son grenier depuis `main`.

## STOP

La caisse n'est pas un conteneur interactif et ne distingue pas les aliments. Son nombre est un indice visuel grossier (8 paliers pour 300 portions), pas une correspondance caisse/portion. Le verdict local ne vaut pas integration dans `main`.
