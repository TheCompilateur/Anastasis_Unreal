# ecarts-union-001

Mandat d'Alexandre (2026-10-09, « avec effet immédiat ») : supprimer les conflits du registre des écarts.
Le 2026-10-08/09, sept missions (memoire-decisions, opening-in-sim, valmire-grows, arrivants, voix-conseil,
player-walk, player-survie) se sont bloquées sur `Source/AnastasisSim/ECARTS.md` : chacune ajoutait sa
fiche à la fin et devinait le même « max + 1 » ; chaque collision coûtait un relais (worktree, build,
suite, renumérotation), une à deux heures.

## Ce qui change

- `.gitattributes` : `Source/AnastasisSim/ECARTS.md merge=union`. Deux missions qui ajoutent chacune leur
  fiche fusionnent sans conflit (rebase, cherry-pick du lot, simulation `merge-tree` de `status`).
  Simulation sur les branches du jour : `player-walk-001` passe de « conflit sur ECARTS.md » à « propre ».
- `agent-worktree.ps1 ecart -Mission <m>` : réserve le prochain numéro, sous un verrou de machine
  (`Global\AnastasisEcartReserve`) : plus grand numéro de `main`, de toutes les branches `agent/*` et des
  réservations (`ANASTASIS_WORKTREES\.ecarts\reserved.txt`), plus un. Sortie `ECART_RESERVE::<n>`.
- `integrate-batch` : deux fiches au même numéro dans l'arbre empilé sont refusées avant tout build
  (`FAIL: ECARTS.md du lot porte deux fiches au meme numero`), `main` intacte. `CHECKS::PASS` le mentionne.
- AGENTS.md (Portage, table du cycle de vie), `docs/migration/PROTOCOLE_ECARTS.md`, en-tête de
  `ECARTS.md`, skill `anastasis-mission` : la règle « le numéro se réserve, il ne se devine pas ».

Les missions en cours qui ont déjà un numéro deviné le gardent : s'il est en double, le lot le dit et la
plus récente réserve un numéro et renumérote. Seules les fiches restent à la fin du fichier ; une
modification d'une même ligne des deux côtés serait gardée en double par l'union (comme `proofs.txt`) :
modifier une fiche existante reste rare et se relit.

## Preuves

Banc `tools/unreal/test-agent-worktree.ps1` : nouveau cas S28c (réservation max + 1 puis le suivant ;
deux fiches ajoutées fusionnées et versées ; même numéro refusé au lot, main intacte). Résultat dans la
section ci-dessous.

Résultat : 82 PASS, 0 FAIL (S28c : 3 PASS).

PROOFS: (aucune)

## ECARTS

AUCUN — aucun comportement C++ ; seul l'en-tête de `Source/AnastasisSim/ECARTS.md` change.

## INTEGRATION_RISK

Outil d'intégration et protocole. Verser seul, nounreal. Le lot qui le suit lit déjà le nouveau
`.gitattributes` (celui de `main`).
