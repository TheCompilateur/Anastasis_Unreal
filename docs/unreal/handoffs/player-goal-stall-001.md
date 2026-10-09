# HANDOFF: player-goal-stall-001

## MISSION

Le joueur incarné mourait de faim vers le jour 8 dans `memory-pie`, même quand on lui posait « boire » ou « manger ».
Cause trouvée : **ce n'était pas un défaut du joueur**. Épuisé (énergie ≤ 12), son corps passe devant
(`FVillage::BodyOverrides`, écart n°21 ASSUMÉ par Alexandre) : seul le remède, dormir, est accepté ; c'est au joueur
de le choisir. Le pilote de la preuve ne choisissait jamais `rest` : le joueur refusait tout (`le-corps-parle`),
attendait, ne dormait jamais, et mourait. Correction : le pilote choisit le remède ; l'écran et l'état du joueur
disent ce que le corps réclame.

RELAIS: (aucun)

## FILES_OWNED

- `Source/Anastasis_UnrealV2/Sim/AnastasisSimulationPlayer.cpp` : `BodyAsks` (le remède réclamé, mêmes seuils que `BodyOverrides`) ; l'écran dit « refuse : le corps passe devant : il faut dormir » ; `get_player_status` gagne `energy` et `body`
- `Source/Anastasis_UnrealV2/Sim/AnastasisPlayerStallTests.cpp` (nouveau) : `Anastasis.Joueur.TrenteJours`
- `tools/unreal/memory-pie.py` : le pilote dort quand le corps le demande ; `player_alive` redevient un critère
- cette fiche

## COMMIT

Le dernier commit de la branche `agent/player-goal-stall-001` (marqué par `finish`).

## MEC

- Reproduction (avant) : `Anastasis.Joueur.TrenteJours` avec l'ancien pilote -> FAIL ; `refus=drink/le-corps-parle table=rest` dès le jour 3, mort au jour 7.
- Après : `report-tests.ps1 -Filter "Anastasis.Joueur.TrenteJours"` -> PASS 1, FAIL 0 ; `JOUEUR_30J vivant=1`, énergie 31,9 au jour 30.
- PROOF : `editor-batch.ps1 -Proofs memory-pie` -> `PROOF::PASS memory-pie` ; `MEMORY_PIE_PLAYER alive=True drinks=286 meals=29` ; `MEMORY_PIE PASS rumors=60 houses=4 help=6/8 notes=50 checks=...,player_alive=1,...`.
- `player-pie` échoue (`walks east when driven dx=0.467 tiles`) **aussi sur main sans ce changement** (vérifié le 2026-10-09, modifications mises de côté) : défaut préexistant, tâche séparée proposée ; ce changement ne touche pas la marche.

## PROOFS

PROOFS: memory-pie

## SCN

NOT_CLAIMED — aucun changement visuel ; seule la ligne d'état du joueur à l'écran dit le remède réclamé.

## PLY

NOT_JUDGED — le joueur humain voit désormais pourquoi son intention cède, et quoi choisir.

## ECARTS

AUCUN — aucun fichier de `Source/AnastasisSim/` n'est touché ; la règle du corps (écart n°21) est inchangée.

## INTEGRATION_RISK

- `memory-pie` exige désormais un joueur vivant au bout des trente jours (`player_alive`).
- `get_player_status` ajoute deux champs JSON (`energy`, `body`) ; les lecteurs existants les ignorent.

## STOP

- La marche du joueur (`player-pie`) n'est pas corrigée ici.
- Le joueur ne choisit toujours rien seul : c'est voulu (écart n°21). Une aide automatique au joueur n'est pas faite.
