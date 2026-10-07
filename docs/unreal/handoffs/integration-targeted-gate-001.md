# HANDOFF: integration-targeted-gate-001

## MISSION

Permettre a l'integrateur de borner un lot a 1..30 cas d'automation Unreal, sur instruction d'Alexandre du 2026-10-07, sans masquer la portee reduite du verdict.

## FILES_OWNED

- `tools/unreal/agent-worktree.ps1`
- `AGENTS.md`
- `.claude/skills/anastasis-mission/SKILL.md`
- cette fiche

## COMMIT

Commit contenant cette fiche sur `agent/integration-targeted-gate-001`.

## MEC

- Analyse syntaxique PowerShell de `agent-worktree.ps1` : 0 erreur.
- `git diff --check` sur les fichiers modifies : PASS.
- Deux controles de validation des arguments : filtre interdit hors `integrate-batch`, caracteres non admis refuses avant verrou.
- Le premier lot cible en production verifiera la voie d'execution du filtre et le plafond reel.

## PROOFS

PROOFS: (aucune)

## SCN

UNKNOWN : aucun editeur ou asset modifie par cette mission.

## PLY

UNKNOWN : aucun joueur teste par cette mission.

## INTEGRATION_RISK

- `-TestFilter` est optionnel, seulement pour `integrate-batch`. Sans lui, la suite complete garde son comportement.
- Un resultat cible entre 1 et 30 cas porte `TESTS::TARGETED_PASS`, jamais un claim de suite complete.
- Le filtre est valide avant le verrou, puis le nombre de cas apres execution. L'integrateur choisit un filtre etroit a partir des tests connus pour eviter de lancer trop de cas.
- Build et preuves PIE du lot restent obligatoires ; une preuve PIE n'est pas un cas de la suite d'automation.

## STOP

Pas de modification Unreal, de gameplay, de preuve joueur ni de qualification de la suite complete sur les futurs lots cibles.
