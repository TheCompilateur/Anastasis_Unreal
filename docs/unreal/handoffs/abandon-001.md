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
- `Content/Anastasis/VillageBuildings/M_VillageBuilding_Aged.uasset` (créé, `AGING::PASS`)
- `AGENTS.md` : deux lignes d'index
- cette fiche

Empilée sur `agent/iceberg-001` (commit `08c4648`) : **intégrer iceberg-001 d'abord**.

## COMMIT

Voir le dernier commit de `agent/abandon-001` (fiche). Code : `28339d5`, matériau : `aabd435`.

## MEC

- BUILD: PASS — `tools\unreal\anastasis-unreal.ps1 build` dans ce worktree (`BUILD::PASS`).
- TESTS: PASS 7/7, 0 échec connu, `report-tests.ps1 -Filter 'Anastasis.Sim.Vacancy+Anastasis.Village.Metabolism'` :
  `Anastasis.Sim.Vacancy.AgeParity`, `.StampParity`, `.VillageHooks` (13 cas âge/bande et 4 cas de tampon,
  valeurs tirées de l'exécution du vrai `collectivePriorities.js`) et les quatre
  `Anastasis.Village.Metabolism.*`, dont `Neglect` (ancrage 0 / 0,2 / 0,55 / 1,0 à 0 / 6 / 18 / 45 jours,
  monotone, maison habitée = 0, chantier = 0, témoin faux = 0).
- ECARTS: `node tools/migration/check-ecarts.mjs` → `ECARTS::PASS fiches=24 ouvertes=24 fail=0 warn=4`
  (avertissements antérieurs à la mission).
- COMMANDS:
  - `tools\unreal\anastasis-unreal.ps1 build`
  - `tools\unreal\create-building-aging.ps1` → `AGING::PASS`
  - `tools\unreal\report-tests.ps1 -Filter 'Anastasis.Sim.Vacancy+Anastasis.Village.Metabolism'` → `TESTS::PASS` 7/7
  - `tools\unreal\editor-batch.ps1 -Proofs abandon-pie` → `PROOF::PASS abandon-pie (56.0s)`

## PROOFS

PROOFS: abandon-pie

## SCN

PASS en PIE sur `Lvl_AnastasisSlice` (`ABANDON_PIE PASS`, `Saved/AbandonEvidence/pie/abandon.json`). Le temps
est avancé par `Anastasis.Sim.Advance`, jamais attendu. La preuve lit le paramètre `Neglect` du matériau
dynamique réellement posé sur le mesh de la maison :

| Lecture | Jours vides | `Neglect` lu | Attendu |
|---|---|---|---|
| habitée | - | matériau d'origine | - |
| vide | 3 | 0,100 | 0,100 |
| vide | 8 | 0,258 | 0,258 |
| vide | 20 | 0,583 | 0,583 |
| vide | 50 | 1,000 | 1,000 |
| témoin faux (`Metabolism 2`) | 50 | matériau d'origine (0) | 1,0 en mode vérité |
| retour au mode vérité | 50 | 1,000 | 1,000 |

Aucune capture : le rendu du matériau n'a pas été regardé.

## PLY

UNKNOWN. La mortalité n'était pas portée : `RemoveNpc` est une commande console, jamais appelée par la
partie. Elle l'est dans `mortality-001` (branche suivante, mort à santé 0 seulement). En jeu normal, une
maison n'est vide que si elle n'a pas encore reçu de propriétaire.

## ECARTS

AUCUN — `vacantSinceDay` est un port fidèle de la référence (`stampHouseVacant`, `stampHouseOccupied`,
`vacantAgeDays`, `vacantAgeBand`), prouvé par `Anastasis.Sim.Vacancy.AgeParity` et `.StampParity`
contre les valeurs du JS. Le champ est hors digest. `RestoreForHarness` ne le recharge pas (retombée sur
`createdDay`, comme la référence quand le champ est `null`).

## INTEGRATION_RISK

- Dépend de `agent/iceberg-001` (`SetHearth`, `AnastasisMetabolism`).
- `AnastasisVillage.h` / `.cpp` sont des fichiers chauds : revoir la fusion avec toute mission qui touche
  `FBuilding`, `AddBuilding`, `AssignHome` ou `RemoveNpc`. `mortality-001` s'appuie sur cette branche.
- Le matériau `M_VillageBuilding_Aged` est un asset binaire (LFS) créé par script. L'acteur ne casse rien
  s'il manque (avertissement une fois, ancien rendu).
- `Sync` gagne deux paramètres avec défauts : les appels existants gardent leur comportement.
- Une maison finie n'use pas avant son achèvement (`CompletedDay`) : choix de présentation, pas de la
  simulation. La référence date aussi les chantiers.

## STOP

Ne revendique pas : l'apparence (aucune capture, le matériau n'a pas été regardé), la partie jouée, le toit
qui cède, la reprise de la végétation autour d'une maison abandonnée, la fumée de cheminée. L'usure est un
paramètre de couleur, pas de la géométrie. Les trois états d'usure n'ont pas été comparés aux mêmes
caméras.
