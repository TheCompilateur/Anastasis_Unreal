# HANDOFF: retest-rule-001

## MISSION

Règle de retest adoptée par Alexandre le 2026-10-01 (demande de la session intégratrice) : une preuve
se refait quand ce qu'elle juge a changé, pas quand `main` a bougé. Si les arbres Unreal (`Source/`,
`Config/`, `Content/`, `Plugins/`, `.uproject`) du commit marqué par `finish` sont identiques à ceux de
l'arbre rebasé ou empilé, alors ni build ni suite : `RETEST::SKIP (arbres Unreal identiques a <sha>)`.
Sinon, portail complet.

Branche posée sur `agent/editor-queue-001` (`44aebe6`) : elle en dépend (marqueur `<sha> <mode>`,
verrou de `main`, `integrate-batch` avec preuves PIE). **À verser après editor-queue-001, ou dans le même lot.**

## FILES_OWNED

- `tools/unreal/agent-worktree.ps1`
- `tools/unreal/test-agent-worktree.ps1`
- `AGENTS.md` (ligne `integrate`, point 6 de « Une file, un éditeur », ligne d'index du banc)
- `docs/unreal/handoffs/retest-rule-001.md`

## COMMIT

Voir `git log agent/retest-rule-001`.

## MEC

- BUILD: non concerné (aucun fichier Unreal) ; `finish` → `BUILD::SKIP TESTS::SKIP`, mode `nounreal`
- TESTS: `tools\unreal\test-agent-worktree.ps1` → **42 PASS, 1 FAIL**
  - les 7 nouveaux contrôles S20 et S21 passent ;
  - FAIL `S18 registre lisible, scripts presents` : préexistant, hors de cette mission. Le registre
    `proofs.txt` d'editor-queue-001 déclare `player-pie`, dont le script `tools/unreal/player-pie.py`
    arrive avec `player-minimal-001`, absent de cette base. Ce contrôle passera quand les deux seront dans `main`.
- Ce qui change :
  - `Test-SameUnrealTrees` compare les arbres Unreal par `git diff --no-renames --name-only <prouvé>..<candidat>`,
    filtré par `Test-UnrealPath` : même définition que le portail. Faux si le commit prouvé n'existe plus.
  - `Unreal-Changes` passe en `--no-renames`. Avant, un `Source/x` renommé en `docs/x` n'affichait que `docs/x`
    et passait pour « pas de changement Unreal ».
  - `finish` : marqueur antérieur `proved` (ou `queued` sans `-Prove`) et arbres identiques à HEAD → `RETEST::SKIP`,
    marqueur reporté sur HEAD **avec le même mode**. Un `queued` reste `queued`. `-Full` force le portail.
  - `integrate` : quand l'avance rapide n'est plus possible, et seulement si un marqueur existe sur le commit
    actuel de la branche, la branche est rejouée sur `main` dans `_integration` (sous le verrou). Ensuite :
    pas de changement Unreal → skip ordinaire ; marqueur `proved` et arbres identiques → `RETEST::SKIP` et
    versement ; sinon `RETEST::REQUIS`, et `main` reste intacte. Sans marqueur : l'ancien refus.
  - `integrate-batch` : si une mission `proved` du lot a des arbres Unreal identiques au sommet → `RETEST::SKIP`.
    Sinon, portail complet comme avant.
  - `integrate-batch` (correctif voisin) : si des preuves PIE sont déclarées et que le lot n'a pas compilé
    (`RETEST::SKIP`, ou lot sans changement Unreal), `BUILD::RUN` incrémental avant `editor-batch`. Avant,
    les preuves pouvaient tourner sur les binaires de `_integration` d'un lot précédent.
  - `Reset-IntegrationTree` : remise à zéro de `_integration`, partagée par `integrate` et `integrate-batch`.
- COMMANDS:
  - `tools\unreal\test-agent-worktree.ps1`
  - `tools\unreal\agent-worktree.ps1 finish -Mission retest-rule-001`

## PROOFS

PROOFS: (aucune)

## SCN

Non concerné.

## PLY

Non concerné.

## INTEGRATION_RISK

- Dépend d'editor-queue-001 (même fichier, commits parents). À verser avec elle ou après elle.
- `integrate` écrit maintenant dans `_integration`, mais sous le verrou de `main`, donc jamais en même temps qu'un lot.
- `tools/` n'entre pas dans la comparaison. Un changement de `report-tests.ps1` ou du registre des
  KNOWN_EXPECTED_FAILURE arrivé par `main` ne relance pas la suite d'une mission reprise : c'est voulu
  (ils classent la suite, ils ne la changent pas), mais à savoir.

## STOP

- Pas vérifié en conditions réelles (lot avec un vrai build) : seulement sur le banc jetable, où le build échoue toujours.
- Un `queued` repris garde sa suite en attente du lot. La règle ne transforme jamais un `queued` en `proved`.
