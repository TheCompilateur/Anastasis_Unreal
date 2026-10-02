# HANDOFF: abandon-001

## MISSION

ABANDON_001. Une maison que personne n'habite vieillit à l'écran avec les jours où elle est vide, selon
l'horloge de vacance que la référence JS tient déjà (`vacantSinceDay`). Suite d'`iceberg-001` (la nuit,
une maison vide reste noire) : ici, le temps long.

Port fidèle de `stampHouseVacant`, `stampHouseOccupied`, `vacantAgeDays`, `vacantAgeBand`
(`src/sim/collectivePriorities.js`), puis projection : usure continue 0..1 ancrée sur les bandes de la
référence (6 / 18 / 45 jours), matériau d'usure, application par l'acteur maison.

## FILES_OWNED

- `Source/AnastasisSim/Public/Village/AnastasisVillage.h` (champ `FBuilding::VacantSinceDay`, quatre fonctions)
- `Source/AnastasisSim/Private/Village/AnastasisVillage.cpp` (tampons dans `AddBuilding`, `AssignHome`, `RemoveNpc`)
- `Source/AnastasisSim/Private/Tests/AnastasisVacancyTests.cpp`
- `Source/Anastasis_UnrealV2/Village/AnastasisBuildingMetabolism.{h,cpp}` (usure, `NeglectForDays`)
- `Source/Anastasis_UnrealV2/Village/AnastasisBuildingMetabolismTests.cpp` (test `Neglect`)
- `Source/Anastasis_UnrealV2/Village/AnastasisVillageBuilding.{h,cpp}` (`SetNeglect`, matériau dynamique)
- `Source/Anastasis_UnrealV2/Village/AnastasisVillagePresentation.{h,cpp}` (paramètre `Day`, jours vides)
- `Source/Anastasis_UnrealV2/Sim/AnastasisSimulationSubsystem.cpp` (jour de simulation passé à `Sync`)
- `tools/unreal/create-building-aging.{ps1,py}`, `tools/unreal/abandon-pie.py`, `tools/unreal/proofs.txt`
- `Content/Anastasis/VillageBuildings/M_VillageBuilding_Aged.uasset` (**à créer** : voir PLY / STOP)
- `AGENTS.md` : deux lignes d'index
- cette fiche

Empilée sur `agent/iceberg-001` (commit `50a173d`) : **intégrer iceberg-001 d'abord**.

## COMMIT

PENDING

## MEC

- BUILD: PASS — `tools\unreal\anastasis-unreal.ps1 build` dans ce worktree (`BUILD::PASS`).
- TESTS: PAS ENCORE JOUÉS. Quatre tests nouveaux : `Anastasis.Sim.Vacancy.AgeParity`, `.StampParity`,
  `.VillageHooks` et `Anastasis.Village.Metabolism.Neglect`. Ils s'ajoutent à ceux d'iceberg-001, qui eux
  sont passés (`TESTS::PASS` 3/3, dans le worktree iceberg-001, sans les tests d'usure).
- ECARTS: `node tools/migration/check-ecarts.mjs` → `ECARTS::PASS fiches=24 ouvertes=24 fail=0 warn=4`
  (avertissements antérieurs à la mission).
- COMMANDS:
  - `tools\unreal\anastasis-unreal.ps1 build`
  - `tools\unreal\create-building-aging.ps1` (crée le matériau, éditeur dédié)
  - `tools\unreal\report-tests.ps1 -Filter Anastasis.Sim.Vacancy` et `-Filter Anastasis.Village.Metabolism`
  - `tools\unreal\editor-batch.ps1 -Proofs abandon-pie`

Valeurs de référence des tests de parité : sorties de l'exécution du vrai `collectivePriorities.js`
(13 cas âge/bande, 4 cas de tampon), reprises ligne à ligne.

## PROOFS

PROOFS: abandon-pie

## SCN

UNKNOWN. `abandon-pie` n'a pas été jouée : elle exige `M_VillageBuilding_Aged`, que seul
`create-building-aging.ps1` crée, dans un éditeur. Aucune capture du rendu.

## PLY

UNKNOWN. La mortalité n'est pas portée : `RemoveNpc` est une commande console, jamais appelée par la
partie. En jeu normal, une maison n'est vide que si elle n'a pas encore reçu de propriétaire.

## ECARTS

AUCUN — `vacantSinceDay` est un port fidèle de la référence (`stampHouseVacant`, `stampHouseOccupied`,
`vacantAgeDays`, `vacantAgeBand`), prouvé par `Anastasis.Sim.Vacancy.AgeParity` et `.StampParity`
contre les valeurs du JS. Le champ est hors digest. `RestoreForHarness` ne le recharge pas (retombée sur
`createdDay`, comme la référence quand le champ est `null`).

## INTEGRATION_RISK

- Dépend du commit `50a173d` d'iceberg-001 (`SetHearth`, `AnastasisMetabolism`).
- `AnastasisVillage.h` / `.cpp` sont des fichiers chauds : revoir la fusion avec toute mission qui touche
  `FBuilding`, `AddBuilding`, `AssignHome` ou `RemoveNpc`.
- Le matériau `M_VillageBuilding_Aged` est un asset binaire (LFS) créé par script. L'acteur ne casse rien
  s'il manque (avertissement une fois, ancien rendu).
- `Sync` gagne deux paramètres avec défauts : les appels existants gardent leur comportement.
- Une maison finie n'use pas avant son achèvement (`CompletedDay`) : choix de présentation, pas de la
  simulation.

## STOP

Ne revendique pas : le rendu (aucune capture), la partie jouée (mortalité non portée), le toit qui cède,
la reprise de la végétation autour d'une maison abandonnée, la fumée de cheminée. L'usure est un
paramètre de couleur, pas de la géométrie. Les trois états d'usure n'ont pas été comparés aux mêmes
caméras ; le matériau n'a pas été regardé.
