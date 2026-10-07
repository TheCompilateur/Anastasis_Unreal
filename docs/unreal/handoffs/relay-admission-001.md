# HANDOFF: relay-admission-001

## MISSION

Deux corrections de `tools/unreal/agent-worktree.ps1` constatees au versement du 2026-10-07 (session
integratrice, autorite d'Alexandre) :

1. **Relais declare (RELAY_ADMISSION_001).** La garde d'integration-proof-closure-001 refuse a juste titre une
   mission qui verse les commits d'une autre sans ses preuves. Mais elle refusait aussi relay-lot6-001, qui rejoue
   proprement quatre missions dont l'agent est hors ligne et reprend leurs preuves. Une fiche peut desormais
   ecrire `RELAIS: a, b` : chaque mission heritee qu'elle nomme est admise par elle (`RELAY_ADMITTED::`) si toutes
   les preuves declarees par la fiche portee figurent dans le `PROOFS:` du relais ; sinon
   `BATCH_REJECTED::<m> : relais incomplet`. Une heritee non nommee reste refusee comme avant.
2. **`prune` par contenu.** L'union de `tools/unreal/proofs.txt` (integration-speed-001) retouche le contexte des
   copies versees ; `git cherry` ne les reconnait plus et `prune` refusait (world-theatre-001 : 2 commits).
   Si rejouer la branche sur main ne change rien (`Get-StackPreview`, deja utilise par `status`), `prune` passe :
   `PRUNE::PAR_CONTENU`. Une branche dont le rejeu change l'arbre reste refusee.

## FILES_OWNED

- `tools/unreal/agent-worktree.ps1` : `Get-DeclaredRelay`, admission des heritees nommees dans la garde
  d'`integrate-batch`, repli par contenu dans `prune`.
- `tools/unreal/test-agent-worktree.ps1` : S28 (relais incomplet refuse ; sans RELAIS: refus inchange ; relais
  complet admis et verse), S29 (prune par contenu ; branche non versee toujours refusee).
- `AGENTS.md` : lignes `integrate-batch` et `prune` de la table du cycle de vie.

## COMMIT

PENDING

## MEC

- BUILD: sans objet (aucun fichier Unreal)
- TESTS: `tools\unreal\test-agent-worktree.ps1` (voir le dernier run cite au commit)

## PROOFS

PROOFS: (aucune)

## SCN

Sans objet.

## PLY

Sans objet.

## INTEGRATION_RISK

- Touche la garde ecrite par Codex (integration-proof-closure-001) sans en retirer aucun cas : ses S22/S23
  restent verts ; seule une heritee nommee dans RELAIS: ET couverte par PROOFS: passe.
- Premier client : relay-lot6-001 (weather-wind-003, tree-leafcards-001, riparian-transition-004,
  soil-water-budget-001), a verser dans le lot qui suit celui-ci.

## STOP

- Ne juge pas la qualite d'un relais (resolutions de conflit) : c'est la suite et les preuves du lot qui le font.
- `status` peut encore classer A_REBASER une mission deja versee sous une autre forme dont le rejeu conflicte
  (abandon-001, iceberg-001, fog-fsss-001...) : non traite ici.
