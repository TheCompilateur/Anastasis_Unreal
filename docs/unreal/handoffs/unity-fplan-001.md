# HANDOFF: unity-fplan-001

## MISSION

Le lot 15 ne compilait plus : en unity, `AnastasisMicroEcologyTests.cpp` partageait un fichier fusionne avec un
`.cpp` qui fait `using namespace AnastasisWorldView;` au niveau du fichier (quatre dans main :
`AnastasisDrainageTests.cpp`, `AnastasisEcologicalDressing.cpp`, `AnastasisForestStructure.cpp`,
`AnastasisHumanGeographyTests.cpp`). Les 13 `.cpp` ajoutes par le lot ont redistribue les fichiers fusionnes et
rendu `FPlan` ambigu (`AnastasisWorldView::FPlan` / `AnastasisMicroEcology::FPlan`, C2872). Le fichier dit deja
`using AnastasisMicroEcology::FPlan;` partout sauf dans deux blocs qui importent tout l'espace : ajoute la.

## FILES_OWNED

- `Source/Anastasis_UnrealV2/WorldView/AnastasisMicroEcologyTests.cpp` (deux lignes)

## COMMIT

Voir `git log` de la branche.

## MEC

- BUILD: sur l'arbre du lot 15 empile (`_integration-2`) avec le correctif : `Build.bat ... -DisableAdaptiveUnity`,
  `Module.Anastasis_UnrealV2.3.cpp` recompile, `Result: Succeeded` (sans le correctif : C2872 / C2664 / C2039).
- TESTS: suite au lot.

## PROOFS

PROOFS: (aucune)

## SCN

Sans objet.

## PLY

Sans objet.

## ECARTS

Sans objet : ne touche pas `Source/AnastasisSim`.

## INTEGRATION_RISK

- A verser en tete du lot 15. Les quatre `using namespace AnastasisWorldView;` de main restent une mine pour le
  prochain changement de composition unity.

## STOP

Ne retire pas les `using namespace` de fichier ; n'est que le correctif minimal du lot.
