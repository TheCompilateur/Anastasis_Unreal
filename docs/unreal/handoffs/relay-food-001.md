# HANDOFF: relay-food-001

## MISSION

Relais d'integration (session integratrice, autorite d'Alexandre du 2026-10-07) : rejouer sur main
player-food-loop-001, dont l'agent est hors ligne, en renumerotant son ecart. La branche d'origine n'est pas
touchee ; ses commits sont cites par `cherry-pick -x` (62d823a3, b9a2348d, dc5af30d, 0389d55d).

RELAIS: player-food-loop-001

## FILES_OWNED

Ceux de player-food-loop-001, plus :
- `Source/AnastasisSim/ECARTS.md` : sa fiche, numerotee trente dans la branche d origine, devient **n° 39** (le numero trente est
  reserve a anthropic-wood-001 depuis le tri du 2026-10-07 ; 37 et 38 sont pris). Placee apres n° 38.
- marque `ecart n°39` dans `Private/Village/AnastasisVillage.cpp` ; mentions dans
  `docs/unreal/PLAYER_MINIMAL_001.md` et `docs/unreal/handoffs/player-food-loop-001.md`.

## COMMIT

PENDING

## MEC

- BUILD: voir finish
- TESTS: au lot (suite complete)

## PROOFS

PROOFS: player-food-loop-pie

## SCN

Sans objet.

## PLY

player-food-loop-pie : premiere execution au lot (jamais jouee dans la mission d'origine, EDITOR_GATE).

## ECARTS

- n° 39 OUVERT (ex-numero trente de player-food-loop-001, SUBSTITUT, A_TRANCHER) : reservation d'un repas choisi par un
  joueur sans decision Nous. Decision d'Alexandre requise, comme le demandait la fiche d'origine.
- Aucun autre ecart ouvert, modifie ou ferme.

## INTEGRATION_RISK

- La preuve player-food-loop-pie n'a jamais tourne : si elle echoue au lot, c'est elle qu'il faut regarder.
- Touches F6-F9 dans DefaultInput.ini (mission d'origine).

## STOP

- Ne tranche pas l'ecart n° 39.
