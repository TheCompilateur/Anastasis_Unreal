# inherit-same-tip-001

Garde des missions heritees de `integrate-batch` : une branche `agent/*` ouverte sur le sommet meme
de la mission versee, sans commit propre, etait comptee comme un ancetre non verse. Le 2026-10-09,
`ma-cabane-001` (creee sur `relay-valmire-001`) a fait refuser `relay-valmire-001` :
`BATCH_REJECTED::relay-valmire-001 : commits de mission(s) heritee(s) non versees avant elle : ma-cabane-001`.

Correction : dans `tools/unreal/agent-worktree.ps1`, une branche dont le sommet est celui de la mission
n'est plus un ancetre (c'est une descendante qui n'a pas commence). La garde reste entiere pour toute
branche qui porte au moins un commit propre (S22, S23, S28 inchanges).

Banc : `tools/unreal/test-agent-worktree.ps1` 79 PASS, 0 FAIL, dont le nouveau cas S28b.

PROOFS: (aucune)

## ECARTS

AUCUN — aucun fichier de `Source/AnastasisSim/` touche.

## INTEGRATION_RISK

Aucun : outil d'integration seulement.
