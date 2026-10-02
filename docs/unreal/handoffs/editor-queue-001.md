# HANDOFF: editor-queue-001

## MISSION

« Une file, un éditeur » (validé par Alexandre le 2026-10-01) : les agents, Cursor comme Claude, ne
démarrent plus d'éditeur pour se prouver ; l'intégrateur verse par lots, rejoue la suite une fois et
TOUTES les preuves PIE du lot dans un seul éditeur, sous un verrou de `main`. Règles : `AGENTS.md`,
section « Une file, un éditeur ». Procédure : skill `anastasis-mission`.

Constat qui l'a motivée (2026-10-01, cette session) : quatre portails d'une seule mission ont attendu la
porte mémoire 8 à 18 min chacun, puis ~12 min de suite ; deux fois la preuve a été perdue parce qu'un
autre agent avait avancé `main` pendant ces 25 min. Deux places d'éditeur pour toute la machine, prises
par des agents qui se prouvaient chacun de leur côté.

## FILES_OWNED

- `tools/unreal/editor-batch.ps1`, `tools/unreal/editor-batch.py`, `tools/unreal/proofs.txt` (nouveaux)
- `tools/unreal/agent-worktree.ps1` (verrou de `main`, marqueur `<sha> proved|queued|nounreal`, `finish`
  sans éditeur par défaut + `-Prove`, preuves déclarées `PROOFS:`, `integrate` renvoie `queued` au lot,
  `integrate-batch` rejoue les preuves en un éditeur, `status` liste les missions prêtes)
- `tools/unreal/test-agent-worktree.ps1` (S13 à S18)
- `AGENTS.md`, `.claude/skills/anastasis-mission/SKILL.md`, `docs/unreal/handoffs/_TEMPLATE.md`

## COMMIT

PENDING

## PROOFS

PROOFS: (aucune)

## MEC

- BUILD: sans objet (aucun fichier Unreal).
- TESTS: `tools\unreal\test-agent-worktree.ps1` — PENDING
- COMMANDS:
  - `tools\unreal\test-agent-worktree.ps1`
  - `tools\unreal\editor-batch.ps1 -Proofs player-pie,village-weather-pie` (validation réelle, un éditeur)

## SCN

Sans objet.

## PLY

Sans objet.

## INTEGRATION_RISK

- **À verser APRÈS `player-minimal-001`** : le registre cite `tools/unreal/player-pie.py`, qui arrive avec
  elle (S18 le refuse sinon : `script=ABSENT`).
- `finish` change de comportement par défaut pour tous les agents : build seul, `HANDOFF_READY::YES (queued)`.
  Un agent qui attend encore la suite de `finish` la trouvera au lot, ou avec `-Prove`.
- `integrate` refuse désormais une mission `queued` et refuse pendant un lot (`MAIN_LOCK::TENU`).
- Un ancien marqueur (sha seul) vaut `proved` : les missions finies avant ce changement passent comme avant.

## STOP

- Pas d'intégrateur automatique (tâche planifiée) : l'outil est prêt, le rôle reste tenu par une session.
- Pas d'attribution automatique d'un test de suite en échec à sa mission : seules les preuves PIE
  désignent leur mission. Une suite rouge arrête le lot, l'intégrateur relance sans la suspecte.
- La porte mémoire compte un éditeur interactif inactif comme une place entière : non changé.
- Les scripts PIE qui ne loguent qu'un `*_COMPLETE` ne sont pas au registre (sauf `village-weather-pie`,
  avec motif d'échec) : à y inscrire par leurs missions, avec un verdict.
