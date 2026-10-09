# HANDOFF: player-sans-sphere-001

## MISSION

Au Play, l'habitant-joueur est incarne (player-start-001) et la camera est sur son corps. `anastasis.Village.Debug`
(1 par defaut) dessinait pourtant pour lui, comme pour tout habitant, une sphere de debug de 3,6 m de rayon centree
a 5 m de haut (son bas tombe a hauteur d'yeux), verte tant qu'il n'a pas soif, avec son etiquette : elle collait a
l'ecran. Le corps du joueur ne se dessine plus dans `FAnastasisVillagePresentation::DrawDebug` ; les autres
habitants gardent leur marqueur. Observateur (`AutoArrive 0`, pas de joueur) : rien ne change.

## FILES_OWNED

- Source/Anastasis_UnrealV2/Village/AnastasisVillagePresentation.cpp
- docs/unreal/handoffs/player-sans-sphere-001.md

## COMMIT

PENDING

## MEC

- BUILD: PASS (`tools\unreal\anastasis-unreal.ps1 build`, `BUILD::PASS`, 161 s, premier build du worktree)
- TESTS: voir `finish` (suite sans rendu)
- COMMANDS:
  - `tools\unreal\anastasis-unreal.ps1 build`
- CAUSE (lue dans `Saved/Logs/Anastasis_UnrealV2.log` du 2026-10-09 16:16 locale) : `ANASTASIS_VISUAL_MODE DEBUG`,
  `ANASTASIS_PLAYER arrive npc-14 ... (start of play)`, `ANASTASIS_PLAYER pawn BP_FirstPersonCharacter_C_0 follows npc-14`.
  Code : `DrawDebugSphere(World, Pos, Tile * 0.18, ...)` avec `Pos = corps + Tile * 0.25`, `Tile` = 2000 uu (3,6 m / 5 m).

## PROOFS

PROOFS: (aucune)

## SCN

NOT_RUN -- aucune scene rejouee ; le trace de debug n'est lu par aucune preuve (JSON) ni test. A regarder au prochain Play :
plus de sphere ni d'etiquette sur soi, celles des autres habitants restent.

## PLY

NOT_RUN -- pas de partie jouee dans ce worktree.

## ECARTS

AUCUN -- `Source/AnastasisSim/` n'est pas touche.

## INTEGRATION_RISK

- Un seul fichier, une ligne de logique dans une boucle de dessin de debug ; aucun conflit connu.

## STOP

- Ne revendique pas que le deplacement au clavier marche : le rapport d'Alexandre (« aucun mouvement ») n'est pas
  elucide. Le log montre le point de vue qui avance de ~140 m en ~1 min (2,5 m/s), sans dire par quelle main.
- Ne retire pas les autres tracés de debug du village ; `anastasis.Village.Debug 0` les coupe tous.
