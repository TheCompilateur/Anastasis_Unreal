# HANDOFF: integration-dir-001

## MISSION

Rendre le dossier du worktree d'integration deplacable (`ANASTASIS_INTEGRATION_DIR`, defaut `_integration`).
Le 2026-10-02 vers 13:20, la machine a refuse d'ecrire `ANASTASIS_WORKTREES\_integration\tools\unreal\editor-window-guard.ps1`,
meme dans un dossier neuf et meme a travers une jonction, alors que le meme contenu s'ecrivait ailleurs et
qu'un autre nom passait au meme endroit. Aucun processus (Restart Manager), aucune regle Claude Code, aucune
detection Defender visible. Tous les lots etaient bloques (`FAIL: remise a zero du worktree d integration`).

## FILES_OWNED

- `tools/unreal/agent-worktree.ps1` (`Integration-Path`, deux appels)
- `AGENTS.md` (ligne `integrate-batch`)

## COMMIT

Voir `git log` de la branche.

## MEC

- BUILD: sans objet (aucun fichier Unreal).
- TESTS: `tools\unreal\test-agent-worktree.ps1` : PASS=43 FAIL=0.

## PROOFS

PROOFS: (aucune)

## SCN

Sans objet.

## PLY

Sans objet.

## ECARTS

Sans objet.

## INTEGRATION_RISK

- Sans la variable, comportement inchange. L'ancien worktree est garde de cote : `_integration-bloque-20261002`.

## STOP

Ne trouve pas la cause du refus ; ne nettoie pas l'ancien worktree.
