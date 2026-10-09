# PLAYER_START_001 — appuyer sur Play, c'est commencer à jouer

Mandat d'Alexandre (2026-10-08, transmis par l'intégrateur) : lever le NOT_IMPLEMENTED du mode visuel
`PLAYER` pour une chose précise — **appuyer sur Play (PIE, ou lancer le jeu empaqueté) = début du
gameplay** : le joueur est l'habitant-joueur, incarné au village d'ouverture, vue première personne, le
village vit autour, sans aucune commande console.

## Ce qui se passe au Play

1. Le GameMode du projet (`AAnastasis_UnrealV2GameMode`, parent C++ de `BP_FirstPersonGameMode`, qui reste le
   `GlobalDefaultGameMode` de `DefaultEngine.ini`) incarne le monde comme avant. Le pawn naît au
   `PlayerStart` de `Lvl_AnastasisSlice`.
2. `UAnastasisSimulationSubsystem::TryStartVillage` choisit le site sur le relief rendu et pose le village du
   lancement (puits, fondateurs, foyer, chantier, travail).
3. **Juste après**, `TryAutoArrive` : si `anastasis.Player.AutoArrive` vaut 1 (défaut) ou si
   `anastasis.Visual.Mode` vaut 2 (`PLAYER`), l'habitant-joueur arrive par `ArrivePlayer`, le **même** chemin
   que la commande `Anastasis.Player.Arrive` (qui l'appelle désormais) : `FVillage::ArriveAsPlayer`, à
   `settlement + (2, 3)` tuiles, premier sol libre, incarné. Personne n'est créé deux fois : un village qui a
   déjà un joueur (sauvegarde relue, commande) le garde.
4. À la frame même, `PlacePlayerPawn` lie le pawn au corps (marche Unreal coupée, carte portrait cachée,
   comme pour `Arrive`) et le pose dessus : le pawn quitte le `PlayerStart` pour le village. À cette première
   pose, `FacePawnTowardVillage` tourne la caméra vers le puits (lacet vers le puits, 6° plongés).
5. Il attend (`idle`) : la main du joueur reste celle de player-goals-001 (ZQSD/WASD, touches 1 à 5).

Pas de mécanisme Unreal de plus : ni nouveau GameMode, ni réglage de World Settings, ni `.umap` modifiée. Le
GameMode du projet était déjà en place ; le jeu empaqueté (`package-playable.ps1`) prend le même GameMode,
la même carte et la même CVar, donc démarre lui aussi en joueur au village.

## Les réglages

| CVar | Défaut | Effet |
|---|---|---|
| `anastasis.Player.AutoArrive` | 1 | 1 = le joueur arrive au début de la partie ; 0 = début en observateur, comme avant. Lue quand le village du lancement se pose. |
| `anastasis.Visual.Mode` | 1 | 2 (`PLAYER`) incarne le même monde que 1 (`DEBUG`) et **force** l'arrivée, même avec `AutoArrive 0`. Plus d'avertissement « unimplemented ». |

La CVar porte un nom distinct de la commande `Anastasis.Player.Arrive` (le ConsoleManager ignore la casse :
même nom = Fatal au chargement de la DLL).

Si aucun village du lancement ne se pose (`StartVillagers 0`, site inéligible), personne n'arrive : il n'y a
pas de village où arriver. Un scénario explicite (`Anastasis.Village.First*`, `FoodSupply`,
`Anastasis.Player.FoodLoop`) remplace le village du lancement, joueur compris (`ResetCanonical` vide
`PlayerPersonId`) : le pawn est délié et remarche seul, le scénario se joue comme avant.

## Les preuves ne changent pas

Un joueur incarné est une personne de plus (elle boit, mange, se compte), et le temps accéléré avec un
joueur incarné change sa réputation et ce que les habitants voient (TIME_WARP_001). Les preuves PIE
existantes ont été écrites pour un début en observateur. Donc :

- `Start-AnastasisEditor` (`editor-launch.ps1`) ajoute `-dpcvars=anastasis.Player.AutoArrive=0` à tout éditeur
  piloté par script (`py` dans `-ExecCmds`, `-ExecutePythonScript`), sauf si la ligne de commande nomme déjà la
  CVar. Couvre les preuves lancées seules par leur `.ps1`, `verify` (smoke-pie), les captures, les générateurs.
- `editor-batch.py` repose `anastasis.Player.AutoArrive 0` avant chaque preuve du lot, comme les CVars de
  rythme : une preuve qui l'a mise à 1 ne l'impose pas à la suivante.
- L'éditeur interactif (`anastasis-unreal.ps1 editor`, l'éditeur d'Alexandre) et le jeu empaqueté n'ont pas de
  `py` : ils démarrent en joueur.

Preuves qui auraient changé sans cela (début sur le village du lancement, sans scénario qui le remplace) :
`player-pie` (un second habitant-joueur), `chronicle-pie`, `save-load-pie`, `settlement-morphogenesis-pie`,
`npc-life-pie`, `sky-clock-pie`, `geo-remote-crisis-pie`, `villager-pie`, `villager-body-pie`, et toute capture
PIE au village. Avec les deux gardes, elles démarrent exactement comme avant.

## Preuve : `player-start-pie`

`tools/unreal/player-start-pie.py`, au registre. Trois PIE dans un éditeur, sans aucune commande `Anastasis.*`
pendant le jeu (seule la CVar est posée avant Play) :

| Run | Avant Play | Attendu |
|---|---|---|
| `auto` | `AutoArrive 1` | joueur incarné dès le village posé, `arrivedAtStart`, pawn lié et posé sur le corps, à moins de 100 m du puits, regard vers le puits (< 20°), but `idle`, sa carte cachée, d'autres habitants qui agissent (bougent, marchent, ou au moins trois avec un but autre qu'attendre), temps simulé qui avance (observés après 1 s simulée au rythme du jeu, sans accélérer, au plus 240 s réelles) |
| `witness_off` | `AutoArrive 0` | village posé, personne d'incarné, pawn libre |
| `mode_player` | `AutoArrive 0`, `Visual.Mode 2` | le mode `PLAYER` force l'arrivée : mêmes contrôles qu'`auto` |

`get_player_status` porte désormais, avec ou sans joueur : `autoArrive`, `visualMode`, `arrivedAtStart`,
`well`, `wx/wy/wz` (puits, Unreal), `pawnPresent`, `px/py/pz` (pawn), `pawnToWellM`, `facingErrDeg`.

RESULTATS (2026-10-09, `editor-batch.ps1 -Proofs player-start-pie`, worktree player-start-002 sur main 4ff9c34a, 388 s) :
`PLAYER_START_PIE PASS checks=23 failed=0`. Run `auto` : `npc-14` incarne sans aucune commande (`arrivedAtStart` vrai), pawn lie a
(77000, 47000), a **72,1 m** du puits, regard vers le puits (`facingErrDeg` 0,0), but `idle` / `attend`, 15 habitants dont 7 qui marchent,
temps simule 37,80 -> 38,82 s. Run `witness_off` : 14 habitants, personne d'incarne, pawn libre. Run `mode_player` : `Visual.Mode 2`
force l'arrivee (`AutoArrive 0`), memes mesures qu'`auto`. L'ancienne mesure du doc (44,7 m) date d'avant la reprise sur main 4ff9c34a ; la cause de l'ecart n'est pas cherchee ici.

## STOP — ce qui n'est pas fait

- La parole dirigée (`T`) : non portée (écart n°20).
- Un pawn et un rendu propres au joueur : le pawn reste celui du template First Person ; le mode `PLAYER`
  incarne le même monde que `DEBUG`.
- Le joueur arrive à `settlement + (2, 3)` tuiles du puits (72,1 m mesurés en PIE le 2026-10-09), là où la référence le fait arriver : la
  placette n'est pas sous ses pieds, elle est devant lui. Le rapprocher serait un écart à la référence.
- Les touches de jeu restent les `DebugExecBindings` PIE et le template : aucune interface de début de partie
  (menu, choix du personnage).
- Aucun verdict visuel : la preuve dit où est le pawn et vers où il regarde, pas que l'image est belle.
