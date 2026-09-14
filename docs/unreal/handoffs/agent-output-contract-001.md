# HANDOFF: agent-output-contract-001

## MISSION

Ajouter une passation agent minimale et obligatoire pour que l'integrateur lise
vite les preuves, les fichiers possedes et les risques d'assemblage.

## FILES_OWNED

- docs/unreal/AGENT_HANDOFF_CONTRACT.md
- docs/unreal/handoffs/_TEMPLATE.md
- docs/unreal/handoffs/agent-output-contract-001.md
- docs/unreal/OPERATIONS.md
- tools/unreal/agent-worktree.ps1

## COMMIT

BRANCH_HEAD

## MEC

- SCRIPT PARSE: `tools\unreal\agent-worktree.ps1 status`
- CONTRACT GATE: `finish` refuse une mission sans `docs/unreal/handoffs/<mission>.md`
- DIFF CHECK: `git diff --check`

## SCN

NOT_ATTEMPTED

## PLY

NOT_ATTEMPTED

## INTEGRATION_RISK

- `tools/unreal/agent-worktree.ps1` est un fichier d'infrastructure central.
- Le gate est volontairement strict: une mission sans fiche de passation ne peut
  plus atteindre `HANDOFF_READY::YES`.
- Aucun build Unreal n'est revendique par cette mission documentaire/script.

## STOP

Ne revendique pas l'integration canonique, un build Unreal, une verification
Editor/PIE, ni une validation joueur.
