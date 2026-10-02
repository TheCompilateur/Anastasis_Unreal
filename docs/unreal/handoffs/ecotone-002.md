# HANDOFF: ecotone-002

## MISSION

Rendre la couche forestiere dependante des vrais arbres, plutot que des obstacles melanges. Base fe474f7c0dccb1ead8bf3be2b6d07c50a9cc9ffd. Branche agent/ecotone-002, worktree dedie, port 8801.

OBS : PlaceUnderstory ajoute buissons et rochers a Canopy avant PlaceMicroEcology. AddForest interpretait chacun comme une couronne arborée. Trunks preserve deja les seuls arbres avant cette mutation.
DEC : passer cette liste distincte a la couche forestiere ; conserver la liste complete pour les exclusions, prairies et berges. Aucun changement des assets ou des seuils de densite. CVar anastasis.Dressing.TreeCanopyEcotone 0 restitue la reference, 1 active la correction.

## FILES_OWNED

- Source/Anastasis_UnrealV2/WorldView/AnastasisMicroEcology.cpp
- Source/Anastasis_UnrealV2/WorldView/AnastasisMicroEcology.h
- Source/Anastasis_UnrealV2/WorldView/AnastasisMicroEcologyTests.cpp
- Source/Anastasis_UnrealV2/WorldView/AnastasisWorldEmbodiment.cpp
- Source/Anastasis_UnrealV2/WorldView/AnastasisWorldEmbodiment.h
- tools/unreal/ground-cover-capture.py
- tools/unreal/proofs.txt
- .claude/skills/anastasis-capture/SKILL.md
- AGENTS.md
- docs/unreal/handoffs/ecotone-002.md

## COMMIT

Voir le commit qui contient cette fiche.

## MEC

- BUILD: PASS, code corrige compile en 147.64 s (10 actions).
- TESTS: QUEUED ; nouveau test Anastasis.MicroEcology.EcotoneTreeIdentity compile, non execute.
- Python capture : AST PASS. git diff --check PASS.
- Commande : tools/unreal/anastasis-unreal.ps1 build.

## PROOFS

PROOFS: ecotone-capture

## SCN

KEEP du correctif de placement ; verdict artistique PARTIAL, pas de rupture visuelle majeure.
15 images inspectees : suppression de certaines repousses isolees, prairie et grands arbres preserves ; zone tres ouverte, pas une demonstration de transition vers une foret dense.
Preuve brute : Saved/GroundCoverEvidence/ecotone-002-v1/capture.log, habitat.json, cameras.json, ecotone-site.json et 15 PNG.
Marqueurs : ECOTONE_CAPTURE PASS sampled_ground_unchanged=1 views=5 ; GROUND_CAPTURE_COMPLETE views=5 states=3.
Terrain : 529 echantillons, SHA256 commun 235ff4c1ba4ff4c361180a332fdb35caa673d8ae173b55693ac70f019f5b139d.
Inventaire hors MicroEco (comptages et trois positions par composant) identique entre etats ; inventaire spatial reference/reference2 identique.
5489 arbres / 30604 obstacles ; herbe 1186313 instances inchangees.
MicroEco : 20000 -> 19989. Bord : buissons 4897 -> 5145, jeunes 4504 -> 4287. Sous-bois 6849 -> 5505. Berge 3439 -> 4631, effet du plafond commun, pas d'un changement de regle riveraine.
GPU p50 reference/correction/temoin en ms : prairie 15.9/15.8/16.2 ; ouvert 13.8/13.8/13.8 ; bord 14.3/14.4/14.6 ; interieur 13.6/13.9/14.0 ; oblique 12.9/13.1/13.0. Aucun gain GPU revendique, viewport 1280x720, images 1600x900.
Difference image moyenne normalisee A/B contre A/A : ouvert 0.00816/0.00680 ; bord 0.00776/0.00760 ; interieur 0.00770/0.00748 ; oblique 0.00676/0.00496 ; prairie 0.00835/0.00740. Mesure descriptive sous vent, pas verdict automatique.
Galerie hors git : C:/Users/alex_/.codex/visualizations/2026/10/02/01a0fb0b-fd55-7ed2-820a-6264c5049775/ecotone-002/index.html.
Fatal EXCEPTION_ACCESS_VIOLATION a la fermeture apres COMPLETE ; capture exploitable, fermeture saine NON prouvee. Aucun editeur restant sur ce worktree.
KEEP predefini : vegetation forestiere associee aux arbres, transition proche lisible, terrain et inventaire echantillonne hors MicroEco preserves, aucune densification massive.
REJECT : perte de couverture incoherente, chevauchement manifeste, transition moins credible. Un cadrage invalide exige de corriger l'instrument avant jugement.
Les poses a 1,7 m et la vue oblique sont choisies autour d'un arbre reel au bord d'un groupe ; comptage voisin utilise pour cadrer, pas comme mesure scientifique de communaute.

## PLY

UNKNOWN. Un transect de cameras fixes ne prouve pas une traversee jouee.

## ECARTS

AUCUN : Source/AnastasisSim non modifie ; aucune nouvelle revendication de parite.

## INTEGRATION_RISK

- Le plafond MicroEco commun reste 20000. Le changement des candidats forestiers peut changer les instances retenues des autres roles MicroEco ; les mesurer, ne pas promettre leur identite.
- Fichiers WorldEmbodiment et registre des preuves partages ; integration par l'integrateur seulement.
- Les nouveaux arbres de tree-canopy-002 devront etre observes dans le lot integre.
- Les empreintes terrain et les inventaires hors MicroEco sont echantillonnes, pas une preuve exhaustive de collision/navigation.

## STOP

Aucune reconstruction botanique historique, aucun changement de relief/hydrologie/simulation. Pas de preuve joueur ni de gain GPU revendique. Pas d'integration ou de push par cette mission.
