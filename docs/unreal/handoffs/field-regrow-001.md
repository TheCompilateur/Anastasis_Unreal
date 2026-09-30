# HANDOFF: field-regrow-001

## MISSION

La repousse des champs (`regrowFieldsDaily`), fidèle à la référence `fee66ae`, puis un test
d'endurance qui fait vivre puits, maison, grenier et fermiers plusieurs jours sur le monde canonique.
Détail : `docs/unreal/FIELD_REGROW_001.md`.

## FILES_OWNED

Créés :

- `Source/AnastasisSim/Public/Work/AnastasisFields.h`, `Private/Work/AnastasisFields.cpp`
- `Source/AnastasisSim/Private/Tests/AnastasisRegrowTests.cpp`, `AnastasisRegrowVectors.inl` (généré), `AnastasisVillageEnduranceTests.cpp`
- `tools/migration/parity/regrow.mjs`
- `docs/unreal/FIELD_REGROW_001.md`, cette fiche

Modifiés :

- `Source/AnastasisSim/{Public,Private}/Village/AnastasisVillage.*` (RegrowFieldsDaily, GetRegrownFood)
- `Source/AnastasisSim/{Public,Private}/Sim/AnastasisSimulation.*` (`landRegen` dans la file de minuit)
- `tools/migration/parity-kit.mjs` (provenance `.tete` d'une extraction sans git), `Source/AnastasisSim/PORTAGE.md`

## COMMIT

BRANCH_HEAD

## MEC

- BUILD: PASS — `tools\unreal\anastasis-unreal.ps1 build` sur `dfe2d5e` (base `main@e05875c`), unity : `Module.AnastasisSim.cpp` compilé.
- TESTS: PASS — `tools\unreal\report-tests.ps1 -Filter Anastasis`, run complet (167 annoncés, 167 exécutés) :
  ```
  PASS                  : 163
  KNOWN_EXPECTED_FAILURE: 4   (le registre : Sim.Parite.Fbm, Sim.Parite.SemantiqueJs, 2 x AI.Toolsets.AnastasisInspect)
  FAIL                  : 0
  ```
  Nouveaux, tous PASS : `Sim.Parite.Repousse` (686 vecteurs, 0 écart, 210 tuiles repoussées dans les
  vecteurs) ; `Sim.Village.Repousse.Jachere` (regarnie au jour 2 : 5 portions), `.Plafond` (200 jours,
  jamais > 37), `.Hote` (trois minuits, file vidée au tick même, déterministe) ; `Sim.Village.Endurance`
  (8 jours, conservation à chaque tick, grenier 40 / 142 / 212 puis 204 -> 175, personne n'a faim,
  5 jours sans livraison tous expliqués par la solitude critique des fermiers).
  En cours de route : l'assertion « chaque jour, le grenier reçoit » a échoué (jours 4 à 8) ;
  instrumentée, la cause est la solitude critique (`socialize` non porté) -> `workWillFactor` = 0.
  L'assertion est devenue « aucun jour sans livraison n'est inexpliqué ».
- PARITÉ : `node tools/migration/gen-parity.mjs regrow.mjs -ref <extraction git archive fee66ae>`.
- COMMANDS:
  - `tools\unreal\anastasis-unreal.ps1 build`
  - `tools\unreal\report-tests.ps1 -Filter Anastasis`

## SCN

UNKNOWN — pas de preuve PIE dédiée : la repousse n'a pas de rendu (le tracé de debug du fermier
montre les champs vivants, `tools/unreal/gather-deliver-pie.ps1`, mais la PIE ne dure pas une nuit).

## PLY

UNKNOWN — aucun contrôle humain. `PLAYER` reste NOT_IMPLEMENTED.

## INTEGRATION_RISK

- L'hôte regarnit désormais les champs de TOUT le monde canonique chaque nuit (des milliers de
  portions en quelques jours) : l'état vivant des tuiles grossit (une entrée par champ touché).
- `GetDeferredRemaining` peut valoir 1 entre le changement de jour et le traitement de la file (même tick en pratique).
- Écart latent signalé, non corrigé : `PickFieldCropId` (génération du monde) utilise `Imul` là où la
  référence multiplie en double.

## STOP

- Pas de soins de parcelle (`helpFarm`, `tendNearbyField`), pas de repousse du bois (sans effet dans la référence).
- Pas de `socialize` / `relax` : c'est la limite que l'endurance a mise en évidence, et la tranche suivante.
