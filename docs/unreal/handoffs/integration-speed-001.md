# HANDOFF: integration-speed-001

## MISSION

Accelerer l'integration sans editeur : `agent-worktree.ps1 status` simule le lot avant qu'on le lance
(levier n°1 de l'analyse du 2026-10-07, validee par Alexandre). Le 2026-10-07, quatre missions annoncees
pretes ont ete ecartees pour conflit apres 25 min de portail, et architecture-crusade-001 une fois de plus.

## FILES_OWNED

- `tools/unreal/agent-worktree.ps1` : `Get-StackPreview` (empilement par `git merge-tree` + `commit-tree`
  sur commits flottants, commits apportes par contenu comme `integrate-batch`) ; `status` classe les
  missions pretes en `PRETES_POUR_LE_LOT::` (propres, dans la commande), `LOT_SUIVANT::`, `A_REBASER::`
  (fichiers en conflit nommes), `DEJA_DANS_MAIN::` ; `status` sort en code 0.
- `tools/unreal/test-agent-worktree.ps1` : cas S25 (quatre classes, main et copie de travail intactes), S26 (union de proofs.txt, doublon refuse), S27 (mise a jour du canonique) ;
  le banc copie aussi `tools/soil-crusade` (S18 echouait sur main : le registre cite `tools/soil-crusade/capture.py`).
- `AGENTS.md` : ligne `status` de la table du cycle de vie ; regle absolue CANONICAL_FRESH_001 sous
  « Racine canonique — regle n°1 ».
- `.gitattributes` : `tools/unreal/proofs.txt merge=union` (cause n°1 des conflits du 2026-10-07 : chaque
  mission ajoute sa ligne en fin de registre). `Get-DuplicateProofs` : `finish` et `integrate-batch`
  refusent un nom de preuve inscrit deux fois (ligne modifiee des deux cotes, gardee en double par l'union).
- CANONICAL_FRESH_001 (regle d'Alexandre, 2026-10-07) : `Test-CanonicalBinaries` (DLL de jeu plus ancienne
  que le dernier commit de main sur Source/, Plugins/, .uproject = perime) ; `Update-CanonicalBuild` apres
  chaque `integrate` / `integrate-batch` : `CANONICAL_BUILD::PASS|A_JOUR|DIFFERE|FAIL|SKIP` ; jamais sous un
  editeur ouvert sur le canonique ; `status` affiche `CANONICAL_BINAIRES::`. Constat d'origine : DLL du
  2026-10-02 alors que main etait du 2026-10-07.

## COMMIT

PENDING

## MEC

- BUILD: sans objet (aucun fichier Unreal)
- TESTS: `tools\unreal\test-agent-worktree.ps1` -- voir COMMIT pour le verdict du dernier run
- COMMANDS:
  - `tools\unreal\agent-worktree.ps1 status` sur l'etat reel du 2026-10-07 15:00 (lecture seule), 107 s :
    7 pretes et propres, 1 au lot suivant (village-fabric-001, en conflit avec architecture-crusade-001),
    15 a rebaser avec fichiers, 11 deja dans main. Le lot 3 (architecture-crusade-001 seule) a ensuite ete
    ecarte pour conflit avec village-fabric-001 versee : la simulation l'avait annonce.

## PROOFS

PROOFS: (aucune)

## SCN

Sans objet.

## PLY

Sans objet.

## ECARTS

Sans objet (Source/AnastasisSim non touche).

## INTEGRATION_RISK

- A empiler EN TETE du lot : son `.gitattributes` s'applique alors aux missions empilees derriere
  (architecture-crusade-001 et geopolitical-world-001 ne heurtent main que sur proofs.txt).
- Le lot qui la verse recompile ensuite le canonique (quelques minutes, CPU) : normal, c'est la regle.

- `status` dure ~1 a 2 min avec ~35 missions pretes (merge-tree ~1 s par commit sur ce depot). Pas de cache.
- `commit-tree` cree des objets flottants dans le depot ; `git gc` les ramasse.
- Une mission versee par un lot dont l'empilement a modifie le diff n'est plus reconnue par `git cherry` :
  elle est classee `DEJA_DANS_MAIN` si son rejeu ne change pas l'arbre (cas night-soundscape-001).

## STOP

- Ne fusionne pas la suite et les preuves PIE dans un seul editeur (levier n°3) : mission separee.
- `integrate-batch` n'est pas modifie : il ne lit pas la simulation, il continue d'ecarter au cherry-pick.
- Ne resout aucun conflit de registre (AGENTS.md / proofs.txt) a la place des agents.
