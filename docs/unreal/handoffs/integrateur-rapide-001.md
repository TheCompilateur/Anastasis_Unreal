# HANDOFF: integrateur-rapide-001

## MISSION

Mandat d'Alexandre (2026-10-09) : l'integration est trop lente, reduire les preuves PIE. Plafond d'abord
demande a 3, **fixe a 4** par Alexandre le meme jour (relaye par l'integrateur). Cette premiere passe pose le
plafond de preuves PIE (PROOFS_CAP_001) ; le diagnostic complet est dans la memoire de projet
`integration-lot-goulots-2026-10-09`.

Mesure qui motive le plafond (lot `relay-valmire-001`, 2026-10-09, une mission) : build 301 s, **9 preuves PIE
dans un editeur de 12:55 a 13:18 (23 min)**, puis build du canonique : le verrou de `main` a ete tenu plus de
40 min. Les preuves se repetent d'une fiche a l'autre (`chronicle-pie`, `villager-pie`, `memory-pie`,
`npc-life-pie`, `material-courier-pie`) : `opening-in-sim-001` en declarait 10, `relay-valmire-001` 9,
`water-network-001` 8.

Ce qui change :

- `finish` refuse une fiche qui declare plus de 4 preuves (`PROOFS_CAP::n preuves PIE declarees, maximum 4`).
- `integrate-batch` ecarte une fiche de plus de 4, et renvoie au lot suivant la mission qui ferait depasser 4
  preuves distinctes pour le lot (`BATCH_REJECTED::<m> : le lot depasserait 4 preuves PIE (...) -- lot suivant`).
  Aucune preuve n'est retiree en silence : la mission attend, elle n'est pas affaiblie.
- `status` annonce `PREUVES_DU_LOT::n/4 : ...`, `LOT_SUIVANT_PREUVES::` et `PREUVES_A_REDUIRE::` ; la commande
  `integrate-batch` qu'il donne respecte le plafond.
- Relais (RELAY_ADMISSION_001) : un relais ne peut plus reprendre toutes les preuves des missions qu'il porte.
  Tant qu'il lui reste de la place sous le plafond il doit la leur donner (sinon `relais incomplet`, comme avant) ;
  plafond atteint, il est admis et le lot dit ce qui n'est pas rejoue (`RELAY_PREUVES_NON_REJOUEES::`).
- `ANASTASIS_PROOFS_MAX` change le plafond, sur demande d'Alexandre seulement.

## FILES_OWNED

- tools/unreal/agent-worktree.ps1
- tools/unreal/test-agent-worktree.ps1
- AGENTS.md (section « Une file, un editeur », point 8)
- .claude/skills/anastasis-mission/SKILL.md
- docs/unreal/handoffs/_TEMPLATE.md
- docs/unreal/handoffs/integrateur-rapide-001.md

## COMMIT

PENDING

## MEC

- BUILD: SKIP (aucun fichier Source/, Config/, Content/, Plugins/ ni .uproject)
- TESTS: banc d'essai `tools/unreal/test-agent-worktree.ps1`, rejoue apres rebase sur main `16623526e`
  (avec `ecarts-union-001` et `gate-batch-first-001`) : **94 PASS, 0 FAIL** en 1037 s. Neuf cas S32 :
  - cinq preuves : `finish` refuse (`PROOFS_CAP::5 ... maximum 4`), pas de marqueur ; quatre preuves : `finish` passe ;
  - `ANASTASIS_PROOFS_MAX=5` : la meme fiche de cinq passe ;
  - `status` : `PREUVES_DU_LOT::[0-4]/4`, surplus en `LOT_SUIVANT_PREUVES::`, fiche de 5 en `PREUVES_A_REDUIRE::`
    et absente de la commande ;
  - lot `cap5,capx,cap4` : `cap5` ecartee (5 > 4), `capx` empilee, `cap4` renvoyee au lot suivant, `main` intacte ;
  - relais au plafond : admis, `RELAY_PREUVES_NON_REJOUEES::rc-a : metabolism-pie` ; relais avec place libre :
    refuse (`preuves non reprises : player-pie, house-rest-pie, sky-clock-pie, metabolism-pie`).
  Les cas anterieurs (S1 a S31, dont S28c et S30b des deux missions d'outils) passent inchanges.
- COMMANDS:
  - `tools\unreal\test-agent-worktree.ps1`

## PROOFS

PROOFS: (aucune)

## SCN

UNKNOWN -- outil d'integration, pas de scene.

## PLY

UNKNOWN -- outil d'integration, pas de jeu.

## ECARTS

AUCUN — aucun comportement C++ ; seuls des scripts d'integration et de la documentation changent.

## INTEGRATION_RISK

- **Rebase sur `ecarts-union-001` et `gate-batch-first-001`** (versees juste avant, d'apres l'integrateur) : meme
  `agent-worktree.ps1`, `test-agent-worktree.ps1`, `AGENTS.md` et `SKILL.md`. Zones distinctes (eux : commande
  `ecart`, controle des numeros d'ecarts du lot, porte memoire ; moi : le plafond autour de `Get-DeclaredProofs`,
  de l'admission du lot et de `status`). `AGENTS.md` ne se fusionne jamais par union, a la main.
- **Effet immediat sur les missions non versees** : `finish` refusera une fiche de plus de 4 preuves. Concernees
  aujourd'hui : `ma-cabane-001` (10), `voix-conseil-001` (6), `valmire-grows-001` (6), `site-from-sim-001` (6) ;
  `relay-valmire-001` (9) est deja verse. Les deux « valmire » sont en fait relayees par `relay-valmire-001` : a
  verifier par `status` / `prune` avant de leur demander de reduire.
- Un marqueur HANDOFF_READY deja pose reste valable pour `finish`, mais `integrate-batch` ecarte desormais la
  mission si sa fiche declare plus de 4 preuves : reduire `PROOFS:` dans la fiche, commiter, relancer `finish`
  (si les arbres Unreal sont identiques, `RETEST::SKIP`, quelques secondes).
- Verser seul, nounreal : le lot qui le suit lit deja le nouveau script de `main`.

## STOP

- Ne dit pas quelles preuves garder : c'est le choix de chaque mission (celles qui jugent ce qu'elle change).
- Ne remplace aucune preuve par une autre : une preuve de moins est une preuve de moins, la suite sans rendu de
  `finish` reste le filet du reste.
- Ne touche ni a la duree d'une preuve, ni au verrou de `main` pendant le build du canonique, ni a la duree de
  `status` (~104 s mesures le 2026-10-09) : leviers suivants, non faits.
