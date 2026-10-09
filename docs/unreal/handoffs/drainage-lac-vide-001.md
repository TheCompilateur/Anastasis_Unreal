# HANDOFF: drainage-lac-vide-001

## MISSION

Plantage au lancement trouvé par l'étude d'une année (annee-valmire-001) : pour les graines 99 et 2026, le calcul de
la géographie canonique (`AnastasisDrainage::Apply`, venu de water-network-001) lisait le milieu d'une liste vide
(`Array index out of bounds: 0 into an array of size 0`, `AnastasisDrainage.cpp:713`), et l'éditeur s'arrêtait.

Cause : l'arrondi du contour des lacs garde l'ancien masque quand le flou efface presque tout le lac, avec le test
`Cells.Num() < Lake.Cells.Num() / 4`. En entiers, `/ 4` vaut 0 pour un lac de moins de quatre cases : un lac
minuscule entièrement effacé passait le test et devenait vide, puis sa cote se calculait sur une liste vide.
Correction : un lac effacé garde son masque (`Cells.Num() == 0 || Cells.Num() * 4 < Lake.Cells.Num()`), et la cote
d'un lac de bord ne lit plus une liste vide (repli sur le niveau de la mer).

## FILES_OWNED

- `Source/Anastasis_UnrealV2/WorldView/AnastasisDrainage.cpp`
- `Source/Anastasis_UnrealV2/WorldView/AnastasisCanonicalGeographyTests.cpp` : `Anastasis.WaterNetwork.LacMinuscule`

## COMMIT

Voir `git log main..agent/drainage-lac-vide-001`.

## MEC

- BUILD: PASS (worktree)
- TESTS: `tools\unreal\report-tests.ps1 -Filter 'Anastasis.WaterNetwork'` → PASS 2 / KNOWN_EXPECTED_FAILURE 0 / FAIL 0 :
  - `Anastasis.WaterNetwork.LacMinuscule` : graines 99 (1907 tuiles d'eau, 17 rivières, 9 lacs) et 2026 (3567, 23,
    16) calculées sans plantage ;
  - `Anastasis.WaterNetwork.Canonical` : le monde de référence 12345 **inchangé au bit près** (893 tuiles d'eau,
    19 rivières, 8 lacs, comme la fiche de water-network-001).
- Suite complète : jouée par `finish`.

## PROOFS

PROOFS: (aucune) — le monde du jeu (graine de référence 12345) est identique au bit près ; seuls des mondes qui
plantaient changent.

## SCN

Dans le jeu : rien ne change sur le monde par défaut. Un monde d'une autre graine qui plantait se lance.

## PLY

NOT_JUDGED.

## ECARTS

AUCUN — le changement est dans `Source/Anastasis_UnrealV2/WorldView/` (hôte), pas dans `Source/AnastasisSim/`.

## INTEGRATION_RISK

- Aucun conflit attendu : deux lignes de `AnastasisDrainage.cpp`, un test ajouté.
- `annee-valmire-001` (en cours) en a besoin pour étudier les graines 99 et 2026.

## STOP

- Ne touche ni au réseau de drainage, ni à la simulation.
