# HANDOFF: hero-canopy-001

## MISSION

Quelques arbres heros a leur taille reelle (pin d'Alep, cypres, chene vert,
olivier), et une enveloppe de canopee pour la masse lointaine. La foret de
production (`SM_Tree_*`) n'est pas regeneree.

## FILES_OWNED

- `Source/Anastasis_UnrealV2/WorldView/AnastasisHeroCanopy.h`
- `Source/Anastasis_UnrealV2/WorldView/AnastasisHeroCanopy.cpp`
- `Source/Anastasis_UnrealV2/WorldView/AnastasisHeroCanopyTests.cpp`
- `Source/Anastasis_UnrealV2/WorldView/AnastasisWorldEmbodiment.cpp` (pose des heros et des enveloppes)
- `Source/Anastasis_UnrealV2/WorldView/AnastasisWorldEmbodiment.h`
- `tools/unreal/create-hero-trees.py`
- `tools/unreal/create-hero-trees.ps1`
- `Content/Anastasis/Vegetation/Hero/SM_Hero_AleppoPine.uasset`
- `Content/Anastasis/Vegetation/Hero/SM_Hero_Cypress.uasset`
- `Content/Anastasis/Vegetation/Hero/SM_Hero_HolmOak.uasset`
- `Content/Anastasis/Vegetation/Hero/SM_Hero_Olive.uasset`
- `Content/Anastasis/Vegetation/Hero/SM_CanopyShell.uasset`
- `AGENTS.md` (index)
- `docs/unreal/handoffs/hero-canopy-001.md`

## COMMIT

BRANCH_HEAD

## MEC

- BUILD: PASS (`tools\unreal\anastasis-unreal.ps1 build`)
- TESTS: PASS 1, KNOWN_EXPECTED_FAILURE 0, FAIL 0. Filtre `Anastasis.HeroCanopy`.
  - `Anastasis.HeroCanopy.Select` : au plus 8 heros, especes 1 a 4, score >= 6 m, ecart >= 22 m, un platane et un arbre court exclus, une enveloppe pour un massif de 8 troncs, plan vide et tronc non fini rejetes.
- Assets (`create-hero-trees.ps1`, `HERO_TREES::PASS`) :
  - pin d'Alep 1450 cm, rayon 616 cm, triangles 6088 / 2740 / 1240
  - cypres 1600 cm, rayon 170 cm, triangles 8596 / 3868 / 1848
  - chene vert 1100 cm, rayon 649 cm, triangles 7344 / 3305 / 1542
  - olivier 625 cm, rayon 475 cm, triangles 6520 / 2934 / 1480
  - enveloppe hauteur 611 cm, rayon 672 cm, triangles 3696 / 1664 / 1218
- CVar `anastasis.Dressing.HeroCanopy` defaut 1. Coupee pendant l'automatisation (`anastasis.HeroCanopy.InAutomation` 0).
- La suite `Anastasis` complete est celle du portail `finish`.

## SCN

UNKNOWN. Pas de capture avant/apres. Les meshes sont sauves ; la pose se fait a l'incarnation.

## PLY

UNKNOWN

## INTEGRATION_RISK

- `AnastasisWorldEmbodiment.cpp` est un fichier chaud. Le contact au sol
  (`ground-contact-001`) n'est pas dans ce commit : son rebase verra ce fichier.
- Sans les cinq meshes Hero, l'incarnation repose les heros sur les meshes de
  production et n'ajoute pas d'enveloppe (`missing` dans `ANASTASIS_HERO_CANOPY`).
- Huit heros maximum, ecartes de 22 m, seulement pin, cypres, chene vert et
  olivier d'au moins 6 m. L'enveloppe n'apparait qu'au-dela de 70 m
  (`MinDrawDistance`) et s'eteint vers 1,2 km. Un massif de moins de six troncs
  n'en a pas.
- Nanite reste off. Les `SM_Tree_*` et les materiaux de vegetation ne sont pas touches.

## STOP

- Pas de regeneration de `SM_Tree_*`, pas de Nanite sur le feuillage.
- Pas de contact au sol (worktree `ground-contact-001`, non verse).
- Pas de Lumen, pas de lieu de 30 m, pas de PNJ, pas d'eau, pas de meteo, pas de batiments.
- Pas de push, pas de prune.
