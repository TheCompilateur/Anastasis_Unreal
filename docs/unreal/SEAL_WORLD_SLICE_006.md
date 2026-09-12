# Procédure de seal — WORLD_SLICE_006

État : **PRÊTE, NON EXÉCUTÉE.** Le blocage n'est pas technique : il reste du travail
concurrent non atterri dans la racine canonique.

## Préconditions

Le seal ne s'ouvre que lorsque `preflight` passe. Concrètement :

1. Tout travail concurrent est **COMMITTED** sur `main`, ou **ABANDONNÉ par son
   propriétaire**. Personne d'autre que l'intégrateur n'écrit dans la racine canonique.
2. Aucun fichier non suivi n'attend une décision à la racine du projet.
3. Aucune branche `agent/*` non intégrée ne porte du travail qui devrait être dans le seal.

```powershell
tools\unreal\agent-worktree.ps1 preflight
```

`PREFLIGHT::FAIL` liste exactement ce qui manque. Ne pas contourner : chaque ligne est
une raison pour laquelle la preuve de seal serait fausse.

## Les douze étapes

Depuis la racine canonique, dans cet ordre. `preflight` ouvre la fenêtre d'observation ;
`postflight` la ferme et prouve que rien n'a bougé pendant.

| # | Étape | Commande | Attendu |
|---|---|---|---|
| 0 | ouvrir la fenêtre | `tools\unreal\agent-worktree.ps1 preflight` | `PREFLIGHT::PASS` |
| 1 | HEAD canonique | `git rev-parse --short HEAD` | noter le hash |
| 2 | worktree intentionnel | `git status --porcelain --untracked-files=all` | vide |
| 3 | build | `tools\unreal\anastasis-unreal.ps1 build` | `BUILD::PASS` |
| 4-7 | tranche + WorldView | `tools\unreal\report-tests.ps1 -Filter Anastasis.Terrain` puis `-Filter Anastasis.WorldView` | `FAIL : 0` |
| 8 | vérité des tests | `tools\unreal\report-tests.ps1` | `PASS` / `KNOWN_EXPECTED_FAILURE` / `FAIL : 0` |
| 9-10 | éditeur + PIE | `tools\unreal\anastasis-unreal.ps1 verify` | `VERIFY::PASS` |
| 11 | intégrité visuelle | `tools\unreal\capture-slice.ps1 -Mode 0 -Out A.png` et `-Mode 1 -Out B.png` | deux PNG, comparés à `docs/visual/slice-006/` |
| 12 | fermer la fenêtre | `tools\unreal\agent-worktree.ps1 postflight` | `POSTFLIGHT::PASS` |

Aucune de ces étapes n'a le droit d'être rapportée seule. Les trois catégories de tests
restent distinctes : `KNOWN_EXPECTED_FAILURE` n'est pas `PASS`.

## Ce qui a dérivé depuis 971e2ec — à réévaluer au moment du seal

`94a3be6` (travail concurrent, déjà intégré) a modifié le comportement de la surface
scellée. Le message de `971e2ec` affirmait :

> Collision and navigation are disabled on the surface.

**Ce n'est plus vrai.** `94a3be6` active la collision sur le terrain DEBUG, cubes HISM et
surface procédurale comprises, pour qu'un pawn ne traverse plus le sol. C'est un choix
défendable, mais il contredit une revendication du commit scellé.

Au moment du seal, faire l'un des deux, jamais rien :

- constater la dérive dans l'enregistrement de clôture, ou
- corriger la revendication.

Vérifier aussi que `Anastasis.Terrain.*` couvre toujours ce que le code annonce : aucun
test n'assertait la collision, dans un sens ni dans l'autre.

## Enregistrement de clôture

Une fois les douze étapes vertes, et **seulement** alors :

```
WORLD_SLICE_006::SEALED
CANONICAL_COMMIT::<hash de l'étape 1>
VISUAL_RESULT::tranche canonique 32x32 lisible — relief, terre/eau, rive, nappe au niveau de la mer
SIMULATION_MUTATION::NONE (aucun commit de la tranche ne touche Source/AnastasisSim)
TEST_STATUS::PASS=<n> KNOWN_EXPECTED_FAILURE=4 FAIL=0
KNOWN_DEBT::<liste>
KNOWN_VISUAL_LIMITS::ciel noir dans la scène d'observation ; cuvettes d'eau intérieures sombres ; icônes d'éditeur visibles ; Build() n'accepte que 96x96 -> 32x32
WORKTREE_STATUS::intentionnel
NEXT_PHASE_AUTHORIZED::<rien, sauf mandat explicite>
```

`NEXT_PHASE_AUTHORIZED` reste vide par défaut. Un seal clôt un chantier, il n'en ouvre
aucun.
