# gate-batch-first-001

Mandat d'Alexandre (2026-10-09) : l'intégrateur a la priorité éditeur. Les lots attendaient 3 à 8 min à la
porte mémoire derrière les éditeurs et les suites des agents.

## Ce qui change

- `tools/unreal/editor-launch.ps1` : tant que le verrou de `main` (`ANASTASIS_WORKTREES\.handoff\MAIN.lock`)
  est tenu par un processus vivant, un lancement de priorité 1 (agent) attend : `EDITOR_GATE::WAIT … lot d integration
  en cours (<titulaire>)`, même machine libre. Les lancements de priorité 0 (ceux du lot) passent. Un verrou dont
  le processus est mort est ignoré. `ANASTASIS_MAIN_LOCK_FILE` déplace le verrou lu (banc).
- `tools/unreal/agent-worktree.ps1` : `Enter-MainLock` pose `ANASTASIS_EDITOR_PRIORITY=0` pour tout ce que le
  processus titulaire lance (`integrate` comme `integrate-batch`).
- AGENTS.md, section Porte mémoire : la règle, et « un éditeur interactif ouvert se ferme dès `MAIN_LOCK::TENU` ».
- Les huit sessions Claude actives ont reçu la consigne par message le 2026-10-09.

Limite : un éditeur déjà ouvert avant le lot n'est pas fermé de force (jamais l'éditeur d'un autre).

## Preuves

Banc `tools/unreal/test-agent-worktree.ps1` : 82 PASS, 0 FAIL, dont S30b (lot en cours : agent attend ;
éditeur du lot passe ; verrou mort ignoré).

PROOFS: (aucune)

## ECARTS

AUCUN — aucun fichier de `Source/AnastasisSim/`.

## INTEGRATION_RISK

Un `finish` lancé pendant un lot attend la fin du lot (jusqu'à ~40 min) ; au-delà de 45 min : `finish -Queue`.