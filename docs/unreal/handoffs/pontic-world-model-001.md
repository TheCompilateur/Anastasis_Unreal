# HANDOFF: pontic-world-model-001

## MISSION

Créer un modèle commun d'historicité pour une vallée fictive de l'arrière-pays pontique, avec eau, au début de l'après-1204. Brancher sources, inférences, scène et joueur sur les missions terrain, bâtiments et PNJ sans modifier leurs fichiers.

## FILES_OWNED

- AGENTS.md
- .claude/skills/anastasis-historicite/SKILL.md
- docs/visual/P1_6_PONTIC_BYZANTINE_ART_DIRECTION.md
- docs/historicity/MODELE.md
- docs/historicity/SOURCES.md
- docs/historicity/BRIEF.template.json
- docs/historicity/examples/exemple-eau-001.json
- tools/historicity/check-brief.py
- docs/unreal/handoffs/pontic-world-model-001.md

## COMMIT

Voir le commit portant cette fiche (`git log -1 --format=%H -- docs/unreal/handoffs/pontic-world-model-001.md`).

## MEC

- `python tools/historicity/check-brief.py docs/historicity/examples/exemple-eau-001.json` : `BRIEF::PASS traceability only`.
- `python -m py_compile tools/historicity/check-brief.py` : PASS syntaxe.
- `git diff --check` : PASS.
- BUILD: sans changement Unreal; non lancé.
- TESTS: sans changement Unreal; non lancés.

## PROOFS

PROOFS: (aucune)

## SCN

UNKNOWN. Les six captures fournies par Alexandre ont servi à motiver le cadre; aucune capture du commit ni modification de la carte.

## PLY

UNKNOWN. Aucun parcours interactif ni usage de l'eau prouvé. Le protocole de revue joueur est défini, non exécuté.

## ECARTS

AUCUN — `Source/AnastasisSim/` intact.

## INTEGRATION_RISK

- `AGENTS.md` et `docs/visual/P1_6_PONTIC_BYZANTINE_ART_DIRECTION.md` sont partagés; intégrateur : conserver les ajouts concurrents lors du lot.
- Les sources sur les milieux sont surtout modernes et servent à construire des gradients plausibles, jamais à attribuer une forêt exacte à 1204.
- La fourchette 1204-1225 est une cible de travail explicite, à soumettre au jugement d'Alexandre si le scénario réclame une date plus étroite.
- Aucun agent existant n'a été interrompu ou reconfiguré rétroactivement. Le protocole devient commun après intégration par la session désignée.

## STOP

Pas de reconstitution revendiquée, pas de correction de map, bâtiment ou PNJ, pas de validation d'historien, pas d'intégration/push. Le vérificateur contrôle la structure et les ID, pas l'exactitude d'une source ni la qualité de jeu. Passation à HANDOFF_READY seulement.
