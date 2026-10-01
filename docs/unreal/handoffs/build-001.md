# HANDOFF: build-001

## MISSION

Porter le chantier du simulateur JS : un bâtiment ouvert (par l'hôte) monte en 22 pièces sous les coups
des bâtisseurs, chaque pièce consomme sa part du devis au stock du site, le bâtiment achevé sert.
Ouverture par les habitants, livraisons, bois et pierre : hors mission (build-002). Détail :
`docs/unreal/BUILD_001.md`.

## FILES_OWNED

Créés :

- `Source/AnastasisSim/{Public,Private}/Work/AnastasisBuild.*`
- `Source/AnastasisSim/Private/Tests/AnastasisBuildTests.cpp`, `AnastasisBuildVectors.inl` (généré),
  `AnastasisVillageBuildTests.cpp`
- `tools/migration/parity/build.mjs`
- `tools/unreal/build-site-pie.ps1`, `tools/unreal/build-site-pie.py` (indexés dans `AGENTS.md`)
- `docs/unreal/BUILD_001.md`, cette fiche

Modifiés :

- `Source/AnastasisSim/{Public,Private}/Village/AnastasisVillage.*` (écart n° 18)
- `Source/AnastasisSim/{Public,Private}/Work/AnastasisGather.*` (priorités du métier builder)
- `Source/Anastasis_UnrealV2/Sim/AnastasisSimulationSubsystem.*` (`FirstSite`, `DeliverSite`, `GetBuildStatus`)
- `Source/Anastasis_UnrealV2/Village/AnastasisVillageBuilding.*`, `AnastasisVillagePresentation.cpp`
  (le corps monte avec les pièces, étiquette du chantier, `Status` du chantier)
- `Source/AnastasisSim/PORTAGE.md`, `AGENTS.md` (index)

## COMMIT

BRANCH_HEAD

## MEC

- BUILD: PASS — `tools\unreal\anastasis-unreal.ps1 build`.
- TESTS: `report-tests -Filter Anastasis.Sim` : 85 PASS, 2 KNOWN_EXPECTED_FAILURE (Parite.Fbm,
  Parite.SemantiqueJs), 0 FAIL. Suite complète : voir le run de `finish`.
  Nouveaux, tous PASS : `Sim.Parite.Chantier` (432 vecteurs, 0 écart), `Sim.Village.Chantier.Ouverture`,
  `.Batisseur` (seul : 22 pièces, 13,9 s de chantier, craft 1,0000 → 1,1042), `.ASec` (3 pièces puis
  arrêt ; livré, achevé), `.Plusieurs` (grenier, 4 bras 7/5/5/5), `.Score` (ligne build
  145,75 × wf 0,9240 + rythme 62 = 196,6730). `Sim.Village.Endurance` inchangée au chiffre près.
- PARITÉ : `node tools/migration/gen-parity.mjs tools/migration/parity/build.mjs -ref <extraction git archive fee66ae>`.

## SCN

PASS — `tools\unreal\build-site-pie.ps1` (maison, 2 bâtisseurs) : ouverture (devis 24/8 livré), fondations
(6 pièces), murs (12), toit (19), achevée (22) en 7,3 s simulées ; devis et pièces contrôlés à chaque
échantillon. Images : `Saved/SliceEvidence/build-site/01..05-*.png`.

## PLY

UNKNOWN — aucun contrôle humain. `PLAYER` reste NOT_IMPLEMENTED.

## INTEGRATION_RISK

- Dès qu'un chantier est ouvert, la ligne `build` est calculée pour TOUS les adultes (comme le JS) :
  sans-métier et fermiers bâtissent aussi. Sans chantier, rien ne change (endurance identique).
- Un travail de chantier fini relâche la cible (adaptation à l'écart n° 2).
- `FDecisionTrace` gagne `BuildRowScore` ; `FBuilding`, `FWorkSession`, `FNpc` gagnent des champs ;
  l'empreinte du village inclut le chantier.
- `villager-png-001` (non intégrée) ajoute `anastasis.Sim.TimeScale` : le banc PIE la pose à 1 et gèle
  les prises par elle ; sur `main` sans elle, la commande est sans effet.

## STOP

- Pas d'ouverture de chantier par les habitants, pas de livraisons (porteurs), pas de bois ni de pierre
  récoltés, pas de camp.
- Pas de rendu pièce par pièce : le corps monte par une échelle verticale.
