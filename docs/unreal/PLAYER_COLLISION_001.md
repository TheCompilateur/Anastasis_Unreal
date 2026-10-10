# PLAYER_COLLISION_001 — la marche à la main heurte le décor rendu

Mission `player-collision-reelle-001`, mandat d'Alexandre du 2026-10-09 (« en PNJ je bloque physiquement à des lieux
comme le puits, je peux pas y aller, c'est comme s'il y avait un mur, pareil quand il y a des arbres ou des
bâtiments » ; réponse à la question : « collision réelle d'Unreal »). Écart n°60.

## La cause

Le corps du joueur est un habitant de la simulation (`player-minimal-001`) : le pawn n'a plus de marche propre, il est
posé sur ce corps à chaque image. Les pas du corps à la main (`FVillage::DrivePlayer`) obéissaient à la règle de la
référence JS (`drivePlayerActor` = les collisions de `moveActor`) : `IsFootBlocked`, soit

- toute tuile qui porte un bâtiment (`Nav.Blocked`, posé par `AddBuilding`) — le puits compris,
- toute tuile de forêt qui a encore du bois (`IsStandingTreeTile`),
- l'eau.

Une tuile fait 20 m. Un puits, un tronc ou une maison n'en occupent qu'une fraction : le joueur butait contre le bord
d'une tuile de 20 m, parfois à 10 m du puits visible. Les autres habitants contournent le défaut en visant des points
d'accès, pas la tuile.

## Ce que fait la mission

- `FVillage::SetPlayerStepResolver` : crochet hôte, absent par défaut (le harnais, les tests, l'observateur : la
  référence, au bit près). Posé, `DrivePlayer` ne bloque plus que l'eau et le bord du monde, puis demande à l'hôte le
  point réellement atteint.
- `UAnastasisSimulationSubsystem::ResolvePlayerStep` (`Sim/AnastasisSimulationPlayer.cpp`) : balaie la capsule du pawn
  lié contre la géométrie rendue de `From` à `To` (canal `ECC_Pawn`), glisse le long de l'obstacle (une seconde passe),
  laisse 5 mm de jeu. La capsule est levée d'un pas de marche (45 cm) : un caillou ou un seuil ne l'arrête pas, un mur,
  un tronc ou une pente raide, si. Un villageois rendu (`AAnastasisVillagerVisual`) ne bloque pas. Un joueur posé
  dans un obstacle (`bStartPenetrating`) n'est pas retenu : il peut en sortir.
- `anastasis.Player.Collision` (1 par défaut, relu à chaque image) : 0 rend la règle de la référence.
- Le crochet est posé quand un pawn est lié (`PlacePlayerPawn`) et retiré quand il est libéré (`UnbindPawn`) ; il n'est
  jamais sauvegardé. La position du joueur reste celle de la simulation, donc hachée et sauvée comme avant.

## Ce qu'elle ne fait pas

- Les trajets de buts (boire, manger…) restent menés par la simulation, par tuiles et points d'accès : un tronc rendu
  sur le chemin d'un but n'arrête pas le joueur. Les autres habitants n'ont aucune collision rendue.
- Elle ne donne pas aux arbres une collision de tronc. Mesuré en PIE (2026-10-09, `player-collision-pie`, marche droite vers le
  pied de quatre troncs rendus, 4,8 m/s) : les arbres bloquent bien le pawn (canal `Pawn`), mais leur collision simple
  (`add_simple_collisions NDOP10_X` de `create_tree_asset.py`) est un volume de la taille de l'arbre entier, pas un tronc.
  Les oliviers (`SM_Tree_Olive_01/02`) l'arrêtent à 2,7 et 3,2 m du pied ; deux platanes (`SM_Tree_PlaneTree_01/03`) le
  laissent à 0,41 et 0,17 m, la capsule recouvrant une instance de platane (cause non élucidée). C'est mieux qu'une tuile
  de 20 m, mais c'est encore un mur plus gros que le tronc. Le correctif propre est dans l'asset (une capsule de tronc comme
  collision simple, `create_tree_asset.py`) ou dans l'hôte (ignorer ces HISM et tester des cercles de tronc), pas dans la
  simulation. À faire : mission suivante. Le crochet ne change pas.
- Un joueur dont la capsule recouvre déjà un obstacle (`bStartPenetrating`) n'est pas retenu pour ce pas : il peut en sortir,
  mais ce pas-là ne voit pas non plus les autres obstacles. À durcir avec la collision de tronc (dépénétration par la normale).
- Elle ne dit rien du plaisir de marcher : le verdict est au jeu.

## Preuves

- Tests (`Anastasis.Sim.Joueur.CollisionHote`) : sans crochet la tuile du puits est un mur ; crochet transparent, elle se
  traverse et l'eau bloque toujours ; un mur de l'hôte raccourcit puis fait glisser le pas ; un point d'hôte dans l'eau
  annule le pas ; depuis la tuile du puits, le joueur atteint son but `drink`.
- PIE `player-collision-pie` : la marche vers le puits depuis quatre côtés, `anastasis.Player.Collision 0` puis `1`.
