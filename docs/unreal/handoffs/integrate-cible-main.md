# HANDOFF: integrate-cible-main

## MISSION

`agent-worktree.ps1 integrate` deplace `main`, et seulement `main`, sans jamais toucher la copie de
travail d'un canonique extrait ailleurs ; et il rejoue sur l'arbre verse les controles de `finish`.

Constat : `integrate` faisait `git merge --ff-only` dans le canonique, donc sur la branche EXTRAITE. Le
2026-09-29 le canonique etait sur `atmosphere-cached-lighting-preexposure` avec du C++ non commite d'un autre
agent : integrate aurait avance cette branche et reecrit sa copie de travail. Il ne posait pas non plus
`ANASTASIS_INTEGRATION=1`, que le hook `reference-transaction` exige pour deplacer `main`. Le 2026-09-30,
`main` a apporte deux lanceurs Unreal directs entre la preuve de `finish` et le versement.

## FILES_OWNED

- `tools/unreal/agent-worktree.ps1`
  - `integrate` : refuse d'abord s'il n'y a rien a verser (`NOTHING_TO_INTEGRATE`, code 0), puis tout
    non-fast-forward ; extrait l'arbre verse (`git archive` -> fichier -> `tar`) et y rejoue
    `Test-AnastasisToolsIndex` et `Find-RawEditorLaunch` ; pose `ANASTASIS_INTEGRATION=1` ; canonique sur
    `main` -> `merge --ff-only` (refus precis si recouvrement avec des fichiers modifies, et detection d'une
    copie de travail salie par un refus du hook) ; canonique ailleurs -> `fetch . <branche>:main`.
    Affiche `CANONICAL_BRANCH`, `MAIN_BEFORE`, `MAIN_AFTER`.
  - `prune` (nouveau) : refuse si un commit manque a `main` ou si le worktree n'est pas propre ; retire
    l'enregistrement MCP local, `git worktree remove`, `git branch -D`.
  - `Invoke-Git` : git sans que PS 5.1 ne transforme son stderr en erreur fatale.
- `tools/unreal/test-agent-worktree.ps1` (nouveau) -- banc d'essai sur depot jetable sous %TEMP%.
- `AGENTS.md` -- tables `integrate` / `prune`, index.
- `docs/unreal/OPERATIONS.md` -- etapes 4 et 6 de la passe d'integration.

## COMMIT

Voir `git log agent/integrate-cible-main`.

## MEC

- BUILD: N/A -- aucun C++ ; `finish` le rejoue de toute facon.
- TESTS: `tools\unreal\test-agent-worktree.ps1` : 13/13 PASS.
  - hook actif : un `merge --ff-only` direct de main est refuse ;
  - S1 canonique sur main : main avance, copie de travail a jour et propre ;
  - S2 canonique sur une autre branche avec AGENTS.md modifie : main avance, branche extraite et copie de
    travail inchangees, message explicite ;
  - S3 non-fast-forward : refus, main inchange ;
  - S4 lanceur `Start-Process $Editor` dans l'arbre verse : refus ; S5 script non indexe : refus ;
  - S6 branche deja versee puis main avance : `NOTHING_TO_INTEGRATE` (defaut trouve par le banc : l'ordre
    des controles donnait « pas d'avance rapide ») ;
  - S7 `prune` refuse une branche hors main ; supprime worktree et branche apres versement.
- Suite Unreal : voir `finish` (report-tests).

## SCN

N/A

## PLY

N/A

## INTEGRATION_RISK

- Non teste par le banc : le chemin « refus du hook en phase prepared qui salit la copie de travail » (le
  script le detecte et liste les entrees, sans restaurer : restaurer est une decision humaine).
- Ce versement-ci passe par l'ANCIEN `integrate` du canonique (le nouveau n'y est pas encore) : a faire
  a la main avec `ANASTASIS_INTEGRATION=1`, comme les precedents.

## STOP

Ne revendique pas : l'integration de plusieurs branches (etapes 1-3 de la passe, toujours manuelles).
