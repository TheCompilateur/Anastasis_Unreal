# HANDOFF: forest-structure-001

## MISSION

Structurer la forêt macro déjà placée : âges, groupes, clairières irrégulières,
crêtes et couloirs, sans ajouter d'arbres et sans toucher aux fichiers ouverts
par les agents forêt, sous-bois, écotone, eau, ciel ou village.

## FILES_OWNED

- `Source/Anastasis_UnrealV2/WorldView/AnastasisForestStructure.h`
- `Source/Anastasis_UnrealV2/WorldView/AnastasisForestStructure.cpp`
- `Source/Anastasis_UnrealV2/WorldView/AnastasisForestStructureTests.cpp`
- `Source/Anastasis_UnrealV2/WorldView/AnastasisWorldEmbodiment.cpp` (appel de `Shape` après `Build`)
- `docs/unreal/FOREST_STRUCTURE_001.md`
- `docs/unreal/handoffs/forest-structure-001.md`

## COMMIT

BRANCH_HEAD

## MEC

- BUILD: PASS (`tools\unreal\anastasis-unreal.ps1 build`)
- TESTS: PASS 2, KNOWN_EXPECTED_FAILURE 0, FAIL 0. Filtre `Anastasis.Ecology.ForestStructure`.
  - `Anastasis.Ecology.ForestStructure` : plan plat 2663 → 1573, jeunes 251, anciens 548, clairières 346, une grande, voisins serrés 341 cm contre 386 cm dans les vides.
  - `Anastasis.Ecology.ForestStructureRelief` : relief canonique graine 12345, 6502 → 5151, crêtes 255, jeunes 1689, anciens 2030, clairières 39.
- Ouverture éditeur du worktree, même graine : `ANASTASIS_FOREST_STRUCTURE before=6550 after=5255 young=1691 mature=1207 old=2118 disturbed=202 clearing=37 large=1 moved=1435`.
- La suite `Anastasis` complète est celle du portail `finish`.

## SCN

OBSERVED partiel. L'ouverture de l'éditeur a posé la forêt structurée (6550 → 5255, une grande clairière). Pas de capture avant/après.

## PLY

UNKNOWN

## INTEGRATION_RISK

- Rebasé sur `main` qui contient déjà P1–P4. `Shape` passe après `Build` :
  il ne retire pas la masse une seconde fois, il pose les âges (`Maturity`),
  les clairières dessinées, les crêtes et les couloirs. `bLone` n'est pas touché.
- `AnastasisWorldEmbodiment.cpp` est aussi modifié, non commité, dans
  d'autres worktrees (`macro-forest-001`, `understory-001`). Leur rebase
  verra ce conflit. Cette intégration n'écrit que dans `main`.
- Un second appel à `Shape` rééclaircirait la forêt. Un seul appel par `Build`.
- `Maturity` reste dans [0,20 ; 1]. Les hauteurs d'essence P1 restent sous le
  plafond de 30 m du test `HumanGeography.CollisionAndDressing`.

## STOP

- Pas de PCG, pas de retouche du `.umap`, pas de régénération des `SM_Tree_*`.
- Pas d'arbres morts : les meshes de chablis sont dans `ecotone-forge-001`, non intégrés.
- Pas de changement de collision, de navigation, de météo, d'eau, de sol, de bâtiments, de PNJ.
- La scène change : `Shape` est appelé après `Build`. Les arbres isolés P2 restent. Pas de second appel.
