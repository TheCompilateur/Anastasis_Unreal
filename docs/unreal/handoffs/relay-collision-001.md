# HANDOFF: relay-collision-001

RELAIS: player-collision-reelle-001

## MISSION

Relais de `player-collision-reelle-001` (mandat de l'integrateur, 2026-10-11) : la branche d'origine (55a9e8ec4,
`proved`) etait en conflit avec `main` fd8563d4b sur `Source/AnastasisSim/ECARTS.md` seulement. Son commit est
rejoue par `git cherry-pick -x` sur `main`, sans rien changer a son comportement. La branche d'origine n'est pas
modifiee. Fiche de fond : `docs/unreal/handoffs/player-collision-reelle-001.md` et `docs/unreal/PLAYER_COLLISION_001.md`.

## FILES_OWNED

- ceux de `player-collision-reelle-001` (voir sa fiche), plus cette fiche

## COMMIT

voir `git log main..agent/relay-collision-001`

## MEC

- RESOLUTION : `Source/AnastasisSim/ECARTS.md` = registre de `main` fd8563d4b, intact, suivi de la fiche n°60 ENTIERE
  prise dans 55a9e8ec4. La reparation des fiches n°56 / n°59 que portait le commit d'origine est sans objet : `main`
  les a deja recomposees (`check-ecarts.mjs` sur `main` : PASS). Le n°60 est reserve a player-collision-reelle-001
  (`ANASTASIS_WORKTREES\.ecarts\reserved.txt`) et libre sur `main` : pas de renumerotation.
- `node tools/migration/check-ecarts.mjs -base main -handoff docs/unreal/handoffs/player-collision-reelle-001.md` :
  `ECARTS::PASS fiches=55 ouvertes=55 fail=0`.
- Fusion automatique relue bloc par bloc (`AnastasisVillage.{h,cpp}`, `AnastasisVillagePlayer.cpp`,
  `AnastasisSimulationPlayer.cpp`, `AnastasisSimulationSubsystem.h`, `AnastasisVillagePresentation.{h,cpp}`,
  `state-fields.json`, `proofs.txt`, `AGENTS.md`) : diff contre `main` identique au commit d'origine, declarations
  completes (dont `CVarPlayerCollision ... ECVF_Default);`), aucun marqueur de conflit, une seule ligne d'index
  `player-collision-pie.py` dans `AGENTS.md`, une seule ligne `player-collision-pie` dans `proofs.txt`.
- `SaveFormatVersion` inchange (8) : le crochet `PlayerStepResolver` est un pointeur classe dans `state-fields.json`,
  `VisitState` / `ArchiveState` ne changent pas.
- BUILD / TESTS : par `finish`.

## PROOFS

PROOFS: player-collision-pie

## SCN

Celle de l'origine (`PROOF::PASS player-collision-pie`, 36 controles, dans le worktree d'origine) ; a rejouer au lot.

## PLY

NOT_RUN (comme l'origine).

## ECARTS

OUVERT n°60 (EXTENSION, A_TRANCHER), porte tel quel depuis player-collision-reelle-001. Aucun autre numero ouvert,
modifie ou ferme.

## INTEGRATION_RISK

- `main` a recu depuis la base d'origine mains-joueur-001 / relay-mains-001 (ecart n°57) : `player-hands-pie` fait
  marcher le corps du joueur a la main (`Anastasis.Player.Move`) jusqu'au seuil du grenier. Avec ce relais, cette marche
  heurte le grenier rendu au lieu du bord de sa tuile. Non rejoue ensemble : si le lot a une place de preuve libre,
  y joindre `player-hands-pie` (et `player-pie`, deja signalee par l'origine).
- Verser `relay-collision-001`, pas `player-collision-reelle-001` (en conflit avec `main`).

## STOP

Ne revendique aucun nouveau comportement : c'est le commit d'origine, rejoue. Ne revendique pas la cohabitation avec
les mains du joueur au-dela de la compilation et de la suite.
