# HANDOFF: ecarts-protocole-001

## MISSION

Protocole des écarts de portage : tout comportement C++ de `Source/AnastasisSim/` qui n'est pas la
copie fidèle de la référence JS se déclare dans le commit qui l'introduit, pour que la stabilisation
sache ranger chaque divergence du harnais (bug, écart provisoire, évolution voulue).

## FILES_OWNED

- `Source/AnastasisSim/ECARTS.md` (nouveau : registre, 19 fiches)
- `tools/migration/check-ecarts.mjs` (nouveau : contrôleur)
- `docs/migration/PROTOCOLE_ECARTS.md` (nouveau : le protocole)
- `tools/unreal/agent-worktree.ps1` (`finish` : un appel au contrôleur, avant le build)
- `docs/unreal/handoffs/_TEMPLATE.md`, `docs/unreal/AGENT_HANDOFF_CONTRACT.md` (section `ECARTS`)
- `.claude/skills/anastasis-mission/SKILL.md`, `AGENTS.md` (la règle, une section chacun)
- `docs/migration/phase3/P3_PLAN.md` §7, `Source/AnastasisSim/PORTAGE.md` (une règle, un renvoi)
- `Source/AnastasisSim/Public/Village/AnastasisVillage.h` (deux lignes de commentaire : renvoi au registre)

## COMMIT

BRANCH_HEAD sur `agent/ecarts-protocole-001`.

## MEC

- BUILD: NOT_ATTEMPTED. Session cloud Linux, sans Unreal. Le seul changement C++ est un commentaire
  de deux lignes dans `AnastasisVillage.h`.
- TESTS: NOT_ATTEMPTED (suite `Anastasis`). Aucun test C++ ne lit les fichiers touchés.
- Contrôleur, sur cette branche (node 22) :
  - `node tools/migration/check-ecarts.mjs` → `ECARTS::PASS fiches=19 ouvertes=19 fail=0 warn=3` ;
    les avertissements : fermetures « à attribuer » (n° 9, 17), écarts ouverts sans marque dans le
    code (n° 5, 6, 7, 14, 15, 19).
  - `-bilan` → `A_FERMER=15 A_TRANCHER=4 ASSUME=0` ; `REDUIT 11, SUBSTITUT 4, EXTENSION 3, REFERENCE 1`.
  - `-section actors` → 15 écarts ouverts, dont le n° 5 (cadence), cause du premier rapport.
  - Portail `-base main -handoff`, joué sur des missions simulées dans un clone jetable : voir
    `### Banc du portail` ci-dessous.
- `agent-worktree.ps1` : analysé sans erreur par le parseur de PowerShell 7.4 (Linux). `finish` n'a
  pas été joué (chemins Windows). `test-agent-worktree.ps1` ne couvre que `integrate` et `prune`.

### Banc du portail

Rempli après le banc, voir la fin de la fiche.

## SCN

NOT_APPLICABLE — aucun changement de scène ni de simulation.

## PLY

NOT_APPLICABLE.

## ECARTS

- ouvert : n° 19 — circuit vivrier food-supply, extension opt-in (A_TRANCHER). Il existait, sans
  numéro, dans le bloc FOOD SUPPLY de `AnastasisVillage.h`.
- classés, sans changement de comportement : n° 1 à 18 reçoivent une fiche (classe, destin,
  fermeture tirée de `P3_PLAN.md`, sections du harnais, masques). Aucun code de simulation modifié.

## INTEGRATION_RISK

- **Missions en vol qui touchent `Source/AnastasisSim/`** (nav-service-001, goal-noise-001…) :
  après rebase, leur `finish` exigera la section `## ECARTS`. C'est voulu.
- **Numérotation** : le n° 19 est pris (food-supply). Une mission en vol qui aurait écrit un
  `ecart n°19` pour autre chose **ne sera pas arrêtée** : le contrôleur vérifie que le numéro a une
  fiche, pas que le sens correspond. L'intégrateur regarde, au rebase, si un `ecart n°19` arrive.
- `AnastasisVillage.h` est un fichier chaud : seulement deux lignes ajoutées en tête du bloc
  ECARTS DECLARES. Conflit improbable.
- `finish` exige désormais `node` dans le PATH du poste (déjà utilisé par `tools/migration/`).
- Les destins et fermetures des n° 1 à 18 sont **mes** lectures de `P3_PLAN.md` et du bloc
  d'en-tête, pas des décisions : à relire, surtout les quatre `A_TRANCHER` (n° 6, 13, 14, 19).

## STOP

- Je ne revendique pas que le registre soit complet : il reprend ce qui était déjà déclaré, plus
  food-supply. Un écart jamais écrit nulle part reste invisible ; le contrôleur ne repère que les
  aveux (« non porté »), les marques et les tirages.
- Le n° 11 de l'en-tête dit « pas de repousse » : c'est périmé (field-regrow-001). La fiche le dit,
  l'en-tête n'est pas corrigé.
- Les six marques manquantes dans le code (n° 5, 6, 7, 14, 15, 19) ne sont pas posées : ce serait
  toucher `AnastasisVillage.cpp`, là où travaille nav-service-001.
- `integrate` ne rejoue pas ce contrôle sur l'arbre versé ; seul `finish` le fait.
- Aucun `ASSUME` posé : c'est à Alexandre.
