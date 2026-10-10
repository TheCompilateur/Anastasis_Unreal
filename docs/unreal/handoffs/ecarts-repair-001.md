# ecarts-repair-001

Réparation du registre des écarts et retrait de la fusion par union (introduite le même jour par
ecarts-union-001).

## Ce qui s'est passé

`Source/AnastasisSim/ECARTS.md merge=union` : ma-cabane-001 (fiche n° 56) et faim-champs-001 (fiche n° 59) ont
ajouté chacune une fiche à la fin du registre. Les deux fiches partagent des lignes identiques (`- **classe** :
EXTENSION`, `- **destin** : A_TRANCHER`, `- **statut** : OUVERT`, `- **harnais** : aucune`, lignes vides) ; le diff
les a prises pour du contexte commun et l'union a entrelacé les deux fiches. Le registre de `main` (0475bf73a) était
cassé : `ECARTS::FAIL fiches=52 ouvertes=51 fail=7` (champs manquants n° 56 et n° 59). Rien ne l'a vu au lot : le
lot ne contrôlait que les numéros en double, pas le registre.

## Ce qui change

- `Source/AnastasisSim/ECARTS.md` : fiches n° 56 et n° 59 reprises **entières** de leurs branches d'origine
  (`agent/ma-cabane-001`, `agent/faim-champs-001`), dans l'ordre des numéros. `ECARTS::PASS fiches=52 ouvertes=52
  fail=0`. En-tête : la règle sans union.
- `.gitattributes` : la ligne `Source/AnastasisSim/ECARTS.md merge=union` est retirée. `proofs.txt` garde son union
  (une ligne par preuve, pas de lignes communes).
- `tools/unreal/agent-worktree.ps1` : `integrate-batch` lance `check-ecarts.mjs` sur l'arbre empilé avant tout build ;
  registre invalide → lot refusé, main intacte. La réservation des numéros (`ecart`) et le contrôle des doublons restent.
- AGENTS.md, `docs/migration/PROTOCOLE_ECARTS.md`, en-tête du registre : deux fiches ajoutées en même temps se gardent
  entières au rebase.
- Banc : les deux cas S28c d'union sont remplacés par « registre cassé dans le lot : refusé avant le build ».

PROOFS: (aucune)

## ECARTS

n° 56 et n° 59 : fiches restaurées, contenu inchangé par rapport à leurs missions.

## INTEGRATION_RISK

Les missions en cours qui ont ajouté une fiche en même temps qu'une autre retrouveront un conflit en fin de registre au
rebase : garder les deux fiches entières. relay-soif-001 (fiche n° 58) est dans ce cas et sera refaite sur ce main.
