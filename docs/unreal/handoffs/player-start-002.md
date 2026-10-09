# HANDOFF: player-start-002

## MISSION

Mandat explicite d'Alexandre (2026-10-08, via l'integrateur) : appuyer sur Play (PIE ou jeu empaquete) = debut du
gameplay. L'habitant-joueur est incarne au village du lancement, vue premiere personne, regard vers le puits, le
village vit autour, sans aucune commande console. Leve le NOT_IMPLEMENTED du mode visuel `PLAYER` pour cette seule
chose. Doc : `docs/unreal/PLAYER_START_001.md`.

## REPRISE

Reprise de `player-start-001` (worktree dirty, jamais commite ni passe par `finish`, immobile depuis 2026-10-08 22:29, 15 commits derriere `main`). Son diff a ete rejoue sur `main` 4ff9c34a (4 hunks a la main : `get_player_status` apres player-goal-stall-001, `ResetCanonical`, `proofs.txt`, `AGENTS.md`) ; son worktree est laisse intact. Mandat d'Alexandre du 2026-10-09 (« Go autorisation de tout »). Doc : `docs/unreal/PLAYER_START_001.md`.

## FILES_OWNED

- `Source/Anastasis_UnrealV2/Sim/AnastasisSimulationPlayer.cpp` (CVar `anastasis.Player.AutoArrive`, `ArrivePlayer`, `TryAutoArrive`, `FacePawnTowardVillage`, champs de debut de partie dans `get_player_status`)
- `Source/Anastasis_UnrealV2/Sim/AnastasisSimulationSubsystem.h` / `.cpp` (appel apres le village du lancement, remise a zero)
- `Source/Anastasis_UnrealV2/Anastasis_UnrealV2GameMode.h` / `.cpp`, `WorldView/AnastasisVisualMode.h` / `.cpp` (mode `PLAYER` = meme monde que DEBUG + arrivee forcee)
- `tools/unreal/editor-launch.ps1` (`Add-AnastasisScriptedCVars` : editeur pilote par `py` -> `-dpcvars=anastasis.Player.AutoArrive=0`)
- `tools/unreal/editor-batch.py` (`anastasis.Player.AutoArrive 0` avant chaque preuve)
- `tools/unreal/player-start-pie.py`, ligne `player-start-pie` de `tools/unreal/proofs.txt`
- `AGENTS.md` (Etat, index), `docs/unreal/PLAYER_START_001.md`, `docs/unreal/PLAYER_MINIMAL_001.md` (STOP)

## COMMIT

voir `git log agent/player-start-001` (marque par `finish`)

## MEC

- BUILD: PASS (worktree, incremental, `anastasis-unreal.ps1 build`)
- TESTS: voir `finish` (HEADLESS_GATE_001)
- COMMANDS:
  - `tools\unreal\editor-batch.ps1 -Proofs player-start-pie`
  - `PROOF::PASS player-start-pie (388.0s)`, `PLAYER_START_PIE PASS checks=23 failed=0` (valeurs : `docs/unreal/PLAYER_START_001.md`, RESULTATS)

## PROOFS

Preuves PIE que le lot rejoue pour cette mission, noms de `tools/unreal/proofs.txt` (EDITOR_QUEUE_001) :

PROOFS: player-start-pie, player-pie

`player-pie` : la commande `Anastasis.Player.Arrive` passe maintenant par `ArrivePlayer` (meme appel, ligne de log
suffixee `(console)`), et la preuve doit toujours demarrer en observateur (garde editor-batch).

## SCN

Play sans commande : le village du lancement se pose, puis `ANASTASIS_PLAYER_START arrived npc-N` ; le pawn quitte
le PlayerStart, se pose sur le corps, regarde le puits. Temoin `AutoArrive 0` : personne d'incarne.

## PLY

Non joue a la main : la preuve lit l'etat (`get_player_status`), aucune image jugee.

## ECARTS

AUCUN — `Source/AnastasisSim/` n'est pas touche ; l'arrivee appelle `FVillage::ArriveAsPlayer` tel quel. Aucun champ
d'etat ajoute (`bArrivedAtStart`, `bFaceVillagePending` vivent dans l'hote Unreal, hors `VisitState`).

## INTEGRATION_RISK

- **Comportement par defaut change** : un editeur interactif et le jeu empaquete demarrent en joueur. Tout editeur
  pilote par script (`py`) demarre en observateur grace a `Start-AnastasisEditor` ; une preuve lancee par un autre
  chemin que `Start-AnastasisEditor` / `editor-batch` (aucune connue) verrait un habitant-joueur de plus.
- Une mission concurrente qui ajoute une preuve PIE au village sans passer par ces deux chemins : idem.
- `editor-launch.ps1` est partage par tous les scripts : `test-agent-worktree.ps1` a relancer apres versement si
  l'integrateur le souhaite (la fonction ajoutee ne touche pas la porte memoire).
- Le joueur arrive a `settlement + (2, 3)` tuiles (72,1 m du puits mesures en PIE le 2026-10-09), comme `Arrive` : pas sous la placette.

## CORRECTION DE player-pie

Le premier lot (2026-10-09) a joue `player-start-pie` PASS mais `player-pie` FAIL, pour deux raisons anterieures a cette mission : `build` n'est plus refuse (la simulation ouvre un chantier des le debut, opening-in-sim-001), et le joueur laisse une semaine sans rien faire meurt (mortalite). `player-pie` teste maintenant le refus avec `craft` et vit la semaine par tranches de 6 h (besoin le plus pressant). Rejoue seul : `PROOF::PASS player-pie (125.4s)`, `PLAYER_PIE PASS checks=22 failed=0` (semaine : presence 0,0304, 7,0000 jours oisifs, seenBy 0, reputation 30,1).

## STOP

- Parole dirigee, pawn et rendu propres au joueur, menu de debut : NOT_IMPLEMENTED.
- Le paquet jouable n'a pas ete reconstruit ni lance ici : il prend le meme GameMode, la meme carte et la meme CVar
  (defaut 1), sans `py` ; non verifie en execution.
- Aucun verdict visuel.
