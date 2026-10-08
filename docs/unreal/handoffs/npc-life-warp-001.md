# HANDOFF: npc-life-warp-001

## MISSION

`npc-life-pie` attendait 180 s **réelles** à `anastasis.Sim.Speed 5`. Elle enfreignait la règle TIME_WARP_001
(« une preuve n'attend pas le temps simulé, elle l'avance »). Son débit simulé dépendait de la fréquence
d'images, parce que `PumpFrame` plafonne son rattrapage par image.

Elle avance maintenant le temps par `anastasis.Sim.Warp 10` et borne son attente en temps **simulé** :
1 800 s, soit vingt jours, réglable par `ANASTASIS_NPC_LIFE_SIM_SECONDS`. Un plafond réel de sécurité de
200 s (`ANASTASIS_NPC_LIFE_WALL_SECONDS`) reste sous le délai du registre ; il rend `wall_timeout`,
distinct de `sim_timeout`. La preuve remet `Warp 1` en partant. **Les critères de réussite sont inchangés.**

## FILES_OWNED

- `tools/unreal/npc-life-pie.py`

## COMMIT

Commité le 2026-10-08 sur `main` = `423955c5`.

## MEC

- BUILD: N/A (pas de changement Unreal)
- TESTS: N/A
- COMMANDS: `tools\unreal\editor-batch.ps1 -Proofs npc-life-pie`, trois runs. Le bridage `t.MaxFPS=15` est
  posé le temps du run par `[SystemSettings]` dans la config du worktree, puis retiré.

| Run | Script | Images | Verdict | Temps simulé | Temps réel |
|---|---|---|---|---|---|
| A | nouveau | libre | PASS | 151 s | 64 s |
| B | nouveau | 15 images/s | PASS | 151 s | 91 s |
| C (témoin) | ancien | 15 images/s | PASS | — | 117 s (plafond 180 s) |

**Ce qui n'est pas démontré** : l'hypothèse « la preuve échoue faute de temps réel sur une machine
chargée ». Le témoin C passe même bridé à 15 images/s. L'échec vu par `opening-in-sim-001` sur
`70237070` ressemble à un blocage, pas à de la lenteur : porteur en `gatherStone`, 0 pierre, aucune pièce.
Il est instruit à part (`site-from-sim-001`, ci-dessous).

## PROOFS

PROOFS: npc-life-pie

## SCN

N/A

## PLY

N/A

## ECARTS

Non concerné (outil de preuve seulement).

## INTEGRATION_RISK

- Toute mission qui déclare `npc-life-pie` est jugée par le nouveau script. Il est plus rapide en temps
  réel (64 s au lieu d'environ 80 s) et ne dépend plus du débit d'images.
- Un `wall_timeout` dit que la machine est trop lente, pas que le village a régressé.

## STOP

Ne corrige aucun comportement du village.
