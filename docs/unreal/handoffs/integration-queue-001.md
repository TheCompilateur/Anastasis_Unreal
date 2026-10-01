# HANDOFF: integration-queue-001

## MISSION

Débloquer l'intégration multi-agent (2026-10-01 : des agents bloqués plus de 30 min à intégrer). Décision
d'Alexandre : (1) une file d'intégration groupée, (2) pas de build ni de suite Unreal quand une branche ne
touche rien d'Unreal.

## FILES_OWNED

- `tools/unreal/agent-worktree.ps1` : `finish` (portail Unreal conditionnel, marqueur du commit prouvé),
  `integrate-batch` (nouveau), `integrate` (déplacement de `main` factorisé dans `Move-Main`, comportement inchangé),
  `prune` (versement reconnu par contenu)
- `tools/unreal/test-agent-worktree.ps1` : 10 contrôles ajoutés (23 au total)
- `AGENTS.md` (cycle de vie, index), `.claude/skills/anastasis-mission/SKILL.md` (sections 3 et 4)
- `docs/unreal/handoffs/integration-queue-001.md`

## COMMIT

BRANCH_HEAD

## MEC

- `tools\unreal\test-agent-worktree.ps1` (dépôt jetable, hook `reference-transaction` réel) → **23 PASS / 0 FAIL** :
  - les 13 contrôles d'avant (hook, S1 à S7) : inchangés après la factorisation de `integrate` ;
  - S8 `finish` docs seulement : `TESTS::SKIP`, `HANDOFF_READY::YES`, marqueur = commit de la branche ;
  - S9 `finish` avec `Source/x.cpp` : `UNREAL_CHANGE::OUI`, le build est tenté (et échoue sur ce dépôt sans moteur), pas de marqueur ;
  - S10 `integrate-batch -Missions b1,b2,b3,b4` : `main` avance d'un coup avec b1 et b2 (deux commits dépendants,
    dans l'ordre) ; b3 écartée (pas de `finish`), b4 écartée (conflit), un seul portail ;
  - S11 `prune` reconnaît une mission versée par lot (copie de ses commits) ;
  - S12 un commit ajouté après `finish` : la mission est refusée par le lot.
- `powershell [Parser]::ParseFile` sur `agent-worktree.ps1` → aucune erreur.
- FINISH : par le script de cette branche (le canonique a encore l'ancien) — voir le compte rendu.

## SCN

Sans objet.

## PLY

Sans objet.

## INTEGRATION_RISK

- `agent-worktree.ps1` est l'outil de tous les agents. Il est lu depuis la racine canonique : les nouvelles règles
  valent dès que `main` est extraite dans `C:\dev\ANASTASIS_UNREAL`.
- Le saut du portail Unreal est un CHANGEMENT DE RÈGLE : une branche docs/outils n'est plus jugée par la suite
  d'automation. Les scripts `tools/unreal/*.py` exécutés dans l'éditeur ne relèvent pas de la suite de toute façon ;
  `Content/Python` (toolset d'inspection) déclenche le portail.
- `ANASTASIS_WORKTREES\_integration` (branche `integration/batch`) est créé au premier lot et réutilisé : ne pas y
  développer. `ANASTASIS_WORKTREES\.handoff\` tient les marqueurs.
- Le lot avance `main` en avance rapide sur des COPIES des commits des missions : `status` (qui compare par identité)
  peut lister une mission versée par lot comme non intégrée jusqu'à son `prune`.

## STOP

- Pas de reprise automatique quand `main` bouge pendant le lot : refus, relancer.
- Pas de file persistante ni de démon : l'intégrateur lance `integrate-batch` à la main.
- Ne règle pas la porte mémoire elle-même : deux éditeurs interactifs ouverts sur la racine canonique (8 Go chacun,
  lanceurs disparus) la tenaient le 2026-10-01 ; signalé à Alexandre, non fermés.
