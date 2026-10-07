# HANDOFF: soil-water-budget-001

## MISSION

Réservoir indiciel d'eau des champs, déterministe et opt-in, couplé à la pluie
du jour précédent et à la repousse. Aucun habillage visuel ni effet village
revendiqué à ce stade.

## FILES_OWNED

- Source/AnastasisSim/Public/World/AnastasisSoilWater.h
- Source/AnastasisSim/Public/Village/AnastasisVillage.h
- Source/AnastasisSim/Private/Village/AnastasisVillage.cpp
- Source/AnastasisSim/Private/Tests/AnastasisSoilWaterTests.cpp
- Source/AnastasisSim/ECARTS.md
- Source/Anastasis_UnrealV2/Sim/AnastasisSimulationSubsystem.cpp
- docs/unreal/SOIL_WATER_BUDGET_001.md
- docs/unreal/handoffs/soil-water-budget-001.md

## COMMIT

Le commit contenant cette fiche sur `agent/soil-water-budget-001`.
Base : `ba327baa961918499756a95e660cf20b9f7d1f69`.

## MEC

- BUILD: `BUILD::PASS` sur le code candidat.
- TESTS: `tools/unreal/report-tests.ps1 -Filter Anastasis.Sim.SoilWater` :
  2 PASS, 0 KNOWN_EXPECTED_FAILURE, 0 FAIL, marqueur de fin présent.
  Sur le champ témoin (jour 5) : off sec/pluie = 5/5 ; on sec/pluie = 4/5,
  réserves 0,158/0,458. Bilan fermé sur 20 pas, répétition identique.
  Une garde sur les tuiles non-champs a été ajoutée après ce run ciblé ;
  le build final couvre la garde, la suite du lot rejoue les tests.
- COMMANDS: `tools/unreal/anastasis-unreal.ps1 build` ;
  `tools/unreal/report-tests.ps1 -Filter Anastasis.Sim.SoilWater` ;
  `tools/unreal/agent-worktree.ps1 finish -Mission soil-water-budget-001`.

## PROOFS

PROOFS: (aucune)
Les deux tests C++ sont dans la suite Anastasis du lot.

## SCN

UNKNOWN — aucun changement de rendu ni capture de carte.

## PLY

UNKNOWN — aucun joueur testé.

## ECARTS

- ouvert : n° 37 — réserve d'eau expérimentale des champs (EXTENSION, A_TRANCHER).

## INTEGRATION_RISK

- Numéro 37 réservé après les numéros 30–36 présents dans d'autres branches
  non intégrées ; résoudre le registre lors du lot sans écraser leurs fiches.
- Branche indépendante de `fertility-meal-chain-001` pour ses tests locaux ;
  une future preuve village eau → repas devra être basée sur les deux.
- Le commutateur est off par défaut. L'état n'est pas sérialisé dans les
  sauvegardes JS ni relié au rendu ; ne pas l'activer par défaut à l'intégration.
- `AnastasisVillage.cpp`, `AnastasisVillage.h` et le sous-système sont des
  fichiers chauds ; vérifier les conflits avec les branches de PNJ.

## STOP

Pas de claim sur les repas, la faim, la santé, la géographie jouée ou la
fidélité physique des coefficients. Pas d'intégration ni de push par cet agent.
