# HANDOFF: player-collision-reelle-001

## MISSION

Alexandre (2026-10-09) : en jouant, le corps du joueur est bloque par un mur invisible au puits, aux arbres et aux
batiments. Cause : `FVillage::DrivePlayer` suit la regle de la reference (`IsFootBlocked`) -- toute tuile de 20 m qui
porte un batiment, une foret pleine ou de l'eau bloque. Reponse d'Alexandre a la question posee : collision reelle
d'Unreal. Un crochet hote (`SetPlayerStepResolver`, ecart n°60) remplace, pour la seule marche a la main, les blocs de
tuile des batiments et des forets par un balayage de la capsule du pawn contre la geometrie rendue ; l'eau et le bord
du monde bloquent toujours. `anastasis.Player.Collision 0` rend la regle de la reference. Voir
`docs/unreal/PLAYER_COLLISION_001.md`.

## FILES_OWNED

- Source/AnastasisSim/Public/Village/AnastasisVillage.h
- Source/AnastasisSim/Private/Village/AnastasisVillage.cpp
- Source/AnastasisSim/Private/Village/AnastasisVillagePlayer.cpp
- Source/AnastasisSim/Private/Tests/AnastasisPlayerTests.cpp
- Source/AnastasisSim/ECARTS.md (fiche n°60)
- Source/Anastasis_UnrealV2/Sim/AnastasisSimulationPlayer.cpp
- Source/Anastasis_UnrealV2/Sim/AnastasisSimulationSubsystem.h
- Source/Anastasis_UnrealV2/Village/AnastasisVillagePresentation.{h,cpp} (`SimCellSize`)
- tools/migration/state-fields.json (`PlayerStepResolver`)
- tools/unreal/player-collision-pie.py, tools/unreal/proofs.txt, AGENTS.md (index)
- docs/unreal/PLAYER_COLLISION_001.md, docs/unreal/handoffs/player-collision-reelle-001.md

## COMMIT

PENDING

## MEC

- BUILD: PASS (`tools\unreal\anastasis-unreal.ps1 build`, `BUILD::PASS`, 244 s, premier build du worktree)
- TESTS: `report-tests.ps1 -Filter Anastasis.Sim.Joueur` sans rendu : PASS 8, KNOWN_EXPECTED_FAILURE 0, FAIL 0, TOTAL 8
  (59 s). Dont `Anastasis.Sim.Joueur.CollisionHote` (nouveau), `.Marche`, `.Choix.Tient`, `.Choix.Refus`, `.Incarnation`,
  `.Observateur`, `.Presence`, `.Reputation`. La suite complete tourne dans `finish`.
- ECARTS: `check-ecarts.mjs -base main -handoff` : `ECARTS::PASS fiches=52 fail=0` ; `check-state-fields.mjs` :
  `STATE_FIELDS::PASS structures=46`.
- COMMANDS:
  - `tools\unreal\anastasis-unreal.ps1 build`
  - `tools\unreal\report-tests.ps1 -Filter Anastasis.Sim.Joueur`
  - `tools\unreal\editor-batch.ps1 -Proofs player-collision-pie`
  - `node tools/migration/check-ecarts.mjs -base main -handoff docs/unreal/handoffs/player-collision-reelle-001.md`
  - `node tools/migration/check-state-fields.mjs`

## PROOFS

PROOFS: player-collision-pie

## SCN

`editor-batch.ps1 -Proofs player-collision-pie` dans ce worktree, un editeur : `PROOF::PASS` (checks=36 failed=0), memes
valeurs a chaque essai. PASS instrumental seulement. Depuis quatre cotes a 60 m du puits, la marche a la main s'arrete a
**10,67 m** du centre sur les quatre cotes avec `anastasis.Player.Collision 0` (le mur de la tuile de 20 m, comme la
reference) et a **1,41 / 1,41 / 2,00 / 4,53 m** avec `1` (nord, sud, est, ouest) ; le bloqueur nomme est
`AnastasisVillageBuilding_0` / `Body` / `SM_Arch_Well_01`. Le pawn reste sur le corps simule (ecart <= 0,05 uu) et le
corps n'est pas dans la geometrie rendue (puits).

MESURE SANS VERDICT (lignes `MEASURE`, marche droite vers le pied de quatre troncs rendus, 4,8 m/s) :

- oliviers `SM_Tree_Olive_02` / `SM_Tree_Olive_01` (composants `Dressing_Tree_Generic_v18` / `_v17`) : arret a **2,73 m** et
  **3,17 m** du pied, bloque par la collision simple de l'arbre entier ;
- platanes `SM_Tree_PlaneTree_01` / `_03` : arret a **0,41 m** et **0,17 m** du pied, la capsule recouvrant une instance
  de platane (`initialOverlap`) ; cause non elucidee.

## PLY

NOT_RUN -- personne n'a marche dans le jeu avec ce changement. A regarder en jouant : le puits (on s'en approche a
1,4-2 m au lieu de 10,67 m), le long d'une maison, entre des arbres. Attendu pour les arbres : plus de mur de 20 m, mais un
mur de la taille de l'arbre (~3 m pour un olivier) et des platanes etranges (voir STOP).

## ECARTS

OUVERT n°60 (EXTENSION, A_TRANCHER) : la marche a la main du joueur heurte le decor rendu, pas la tuile entiere.
Sans crochet (harnais, tests de parite, observateur, `anastasis.Player.Collision 0`) la simulation est celle de la
reference, au bit pres.

REPARE, contenu inchange : n°56 (ma-cabane-001) et n°59 (faim-champs-001), restent OUVERT. Sur `main` depuis 0475bf73a le
registre est malforme : la fusion par union a melange les deux fiches (leurs lignes `classe`, `destin`, `statut` communes
dedoublonnees, les deux titres l'un sur l'autre), d'ou `ECARTS::FAIL champ manquant` pour toute mission rebasee sur main.
Les deux fiches sont recomposees a partir de leur commit d'origine (`ef4c30088` et `agent/faim-champs-001`) ; chaque
ligne d'origine est presente dans le texte de main, aucune n'est ajoutee ni retiree.

## INTEGRATION_RISK

- Touche `DrivePlayer` (ligne chaude, aussi dans l'ecart n°42). Le chemin sans crochet est le code d'avant, intact.
- `player-pie` (`Anastasis.Player.Move 1 0` pendant 2 s, `dx > 1 tuile`) marche maintenant avec la collision reelle : un
  tronc ou un mur rendu juste devant l'arrivee l'arreterait. Rejouer `player-pie` avec ce lot si elle est declaree.
- Verifie en PIE : les HISM d'arbres bloquent le canal Pawn (olivier a ~3 m) ; le puits arrete le pawn par son `Body`.
  Non verifie : maisons et autres decors (seul le puits est mesure) ; l'assise (`Footing`, `BlockAll`) n'a pas bloque.
- Un seul `proofs.txt` : une ligne ajoutee (`player-collision-pie`), a ne pas modifier en second commit.

## STOP

- Ne revendique PAS une collision de tronc. Les arbres arretent le joueur par un volume de la taille de l'arbre
  (`NDOP10_X` de `create_tree_asset.py`) : ~3 m pour un olivier ; deux platanes recouvrent la capsule (cause non
  elucidee). Suite : une capsule de tronc comme collision simple (`create_tree_asset.py`) ou un test de cercles de tronc
  cote hote, puis rendre la mesure `MEASURE` de `player-collision-pie` verdict. Voir `docs/unreal/PLAYER_COLLISION_001.md`.
- `bStartPenetrating` rend le pas libre : sortir d'un obstacle est possible, mais ce pas ne voit pas les autres obstacles.
- Ne change pas les trajets de buts du joueur (boire, manger...) ni les autres habitants : tuiles et points d'acces.
- Ne revendique ni le plaisir de marcher, ni l'absence d'autres murs invisibles (relief, eau).
