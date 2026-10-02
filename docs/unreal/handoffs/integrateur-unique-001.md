# HANDOFF: integrateur-unique-001

## MISSION

Ecrire dans `AGENTS.md` (et le skill `anastasis-mission`) la regle d'Alexandre du 2026-10-01 : **un seul
integrateur**. Elle n'existait que dans la memoire des sessions Claude et dans leurs messages ; Codex, qui ne lit
que `AGENTS.md`, a donc verse lui-meme ses lots dans la nuit du 2026-10-02 (03:15, 08:43, 08:53) pendant que la
session integratrice versait les siens.

## FILES_OWNED

- `AGENTS.md` (« Une file, un editeur », point 4 ; renumerotation du doublon 6 → 7)
- `.claude/skills/anastasis-mission/SKILL.md` (section 3 point 4, titre de la section 4)

## COMMIT

Voir `git log` de la branche.

## MEC

- BUILD: sans objet (aucun fichier Unreal).
- TESTS: sans objet.
- COMMANDS:
  - `tools\unreal\agent-worktree.ps1 finish -Mission integrateur-unique-001`

## PROOFS

PROOFS: (aucune)

## SCN

Sans objet.

## PLY

Sans objet.

## ECARTS

Sans objet : ne touche pas `Source/AnastasisSim`.

## INTEGRATION_RISK

- Texte seulement ; `agent-worktree.ps1` n'est pas modifie : la regle n'est pas imposee par l'outil (aucun
  controle de « qui » lance `integrate`).

## STOP

Ne verrouille pas `integrate` par l'outil ; ne designe pas l'integrateur par un fichier. A faire si la regle
ecrite ne suffit pas.
