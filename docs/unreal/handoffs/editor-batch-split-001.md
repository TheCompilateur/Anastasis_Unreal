# HANDOFF: editor-batch-split-001

## MISSION

Le lot 7 du 2026-10-07 (relay-lot6-001) a perdu `tree-cards-capture` : morte a son demarrage dans
python311.dll / PythonScriptPlugin juste apres `riparian-transition-capture`, dans le meme editeur, alors
que build, suite (340 PASS / 0 FAIL) et la premiere capture etaient PASS. `editor-batch.ps1` regroupe
desormais : preuves PIE + premiere capture `*-capture` dans un editeur, chaque capture de plus dans un
editeur neuf. Verdict par preuve inchange (tranche du journal, motifs du registre).

## FILES_OWNED

- `tools/unreal/editor-batch.ps1` : groupes d'editeurs, `jobs.json` / `jobs-<n>.json`, `editor-batch-<n>.log` ;
  `-DryRun` annonce `EDITOR_BATCH::EDITEURS n` quand il y en a plus d'un.
- `tools/unreal/test-agent-worktree.ps1` : S18 (deux captures -> deux editeurs, la PIE avec la premiere ;
  une capture -> un editeur).
- `AGENTS.md` (ligne editor-batch), `.claude/skills/anastasis-mission/SKILL.md` (ordre des captures ; une ligne
  de preuve s'ecrit en une fois, union de proofs.txt).

## COMMIT

PENDING

## MEC

- BUILD: sans objet
- TESTS: banc test-agent-worktree.ps1 (voir le commit)
- `-DryRun` sur wildflowers-capture, village-weather-pie, ecotone-capture, soil-matrix-normal-capture,
  sky-clock-pie : 3 editeurs (village-weather-pie+sky-clock-pie+wildflowers-capture | ecotone-capture |
  soil-matrix-normal-capture).

## PROOFS

PROOFS: (aucune)

## SCN

Sans objet.

## PLY

Sans objet.

## INTEGRATION_RISK

- A empiler EN TETE du lot qui porte plusieurs captures : `integrate-batch` appelle l'editor-batch.ps1 de
  l'arbre empile, le correctif s'applique donc au lot meme (relay-lot6-001).
- Un editeur de plus par capture supplementaire : 2 a 4 min de demarrage, porte memoire comprise.

## STOP

- Ne corrige pas le crash PythonScriptPlugin lui-meme ; il le contourne en isolant chaque capture.
