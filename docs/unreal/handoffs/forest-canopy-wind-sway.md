# HANDOFF: forest-canopy-wind-sway

## MISSION

Faire balancer le feuillage des arbres au vent -- `M_AnastasisVegetation`
n'avait aucun noeud sur World Position Offset, la foret restait figee quels
que soient les assets deja poses (stature, espece, ombrage deux faces).

## FILES_OWNED

- tools/unreal/create_tree_asset.py (source d'autorite des assets ci-dessous,
  cf. son propre entete)
- Content/Anastasis/Materials/M_AnastasisVegetation.uasset
- Content/Anastasis/Materials/M_AnastasisBark.uasset (resauvegarde par le
  meme run, cablage inchange)
- Content/Anastasis/Vegetation/SM_Tree_*.uasset (9, resauvegardes par le meme
  run, geometrie et bounds inchangees)

## COMMIT

BRANCH_HEAD (ce4bc00 sur `forest-canopy-wind-sway`)

## MEC

- BUILD: NOT_APPLICABLE. Aucun fichier C++ touche par cette mission.
- TESTS: NOT_ATTEMPTED. Aucun test automatise ne couvre le cablage materiau
  (TREE_PIVOT / TREE_SLOTS, sur une autre lignee, verifient la geometrie et
  les slots, pas le WPO).
- PREUVE (regeneration headless) : `UnrealEditor-Cmd.exe <uproject>
  -run=pythonscript -script=tools/unreal/create_tree_asset.py` --
  `[create_tree_asset] MATERIAL wiring ... wpo=True`, `Success - 0 error(s),
  4 warning(s)` (les 4 avertissements sont des DeprecationWarning
  preexistantes, sans rapport), `RESULT::PASS meshes=9`, bounds `z=[-50.00,
  50.00]` inchangees sur les 9 meshes.
- COMMANDS:
  - `UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script=tools/unreal/create_tree_asset.py`

## SCN

NOT_ATTEMPTED. Une capture statique ne peut pas montrer un balancement pilote
par le temps ; une session editeur etait deja active au moment de cette
mission, la capture (`capture-tree-lineup.ps1`) n'a pas ete tentee pour ne
pas lui disputer le verrou d'ecriture de la DLL.

## PLY

UNKNOWN. Jamais vu depuis une camera joueur.

## INTEGRATION_RISK

- **`M_AnastasisVegetation.uasset` et les 9 `SM_Tree_*.uasset` sont la
  propriete exclusive de `create_tree_asset.py`**, qui les recree a
  l'identique a chaque run. Toute autre branche qui relance ce script sans
  ce cablage ecrasera le balancement en silence -- aucun test scelle ne le
  protege (meme classe de risque que celle documentee par la mission
  tree-visuals-136553 pour les canaux UV0/UV1).
- **Amplitude et vitesse choisies a l'estime** (`WIND_SWAY_STRENGTH=4.0`,
  `WIND_SWAY_SPEED=0.6`, unites locales du mesh), pas mesurees contre une
  planche de reference ni un retour joueur.
- **Deliberement sans `WindDirectionalSource`** : un sinus du temps, pas une
  lecture de vent de scene (cf. commentaire dans `create_tree_asset.py`). Si
  un systeme de vent/meteo est ajoute plus tard, il faudra reconcilier les
  deux au lieu de les laisser independants.
- Pas de collision de fichier connue avec les autres branches vivantes au
  moment de cette integration : aucune autre ne touche
  `create_tree_asset.py` ni les assets Vegetation/Materials concernes.

## STOP

Cette mission ne revendique PAS :

- une preuve visuelle ou joueur du balancement
- un test automatise scellant le cablage WPO
- une amplitude/vitesse calibree
- la couche au sol (fougeres, rochers, bois mort) -- levier separe, identifie
  mais non commence
