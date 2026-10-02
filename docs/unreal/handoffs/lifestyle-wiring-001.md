# HANDOFF: lifestyle-wiring-001

## MISSION

Brancher le mode de vie (`lifestyle-001`, module seul) dans le village et le lecteur du harnais :
`FNpc::Lifestyle`, `lifestyleDailyUpdate` après la cadence, lecture et projection de `npc.lifestyle`.
C'est la partie `lifestyle.lastNotedDay` de la divergence du tick 1 (rapport 2).

**Branche posée sur `agent/needs-wiring-001` (74b8e27), dont elle dépend. À verser après elle, ou dans
le même lot.**

## FILES_OWNED

- `Source/AnastasisSim/Public/Village/AnastasisVillage.h` : `FNpc::Lifestyle`, include du module
- `Source/AnastasisSim/Private/Village/AnastasisVillage.cpp` : `LifestyleDailyUpdate` en tête d'`UpdateNpc`, après la cadence
- `Source/AnastasisSim/Private/Harness/AnastasisJsSave.cpp` : `ReadLifestyle`, projection de `lifestyle`
- `Source/AnastasisSim/Private/Tests/AnastasisVillageLifestyleTests.cpp` (nouveau)
- `Source/AnastasisSim/Private/Tests/AnastasisJsSaveTests.cpp` : refus d'un habitant sans mode de vie
- `Source/AnastasisSim/ECARTS.md` (fiche n° 8), `Source/AnastasisSim/PORTAGE.md`
- `docs/unreal/handoffs/lifestyle-wiring-001.md`

## COMMIT

Voir `git log agent/lifestyle-wiring-001`.

## MEC

- BUILD : `anastasis-unreal.ps1 build` → `BUILD::PASS`.
- TESTS : `report-tests.ps1 -Filter Anastasis.Sim` → PASS 112, KNOWN_EXPECTED_FAILURE 2 (`Parite.Fbm`,
  `Parite.SemantiqueJs`), FAIL 0, 114/114 annoncés.
  - `Anastasis.Sim.Village.ModeDeVie` (nouveau) :
    - travailleur acharné au travail : score de 3 à 4, jour 1 noté ;
    - lève-tôt qui regarde : score de 0,1 à 0 (max(0, −0,2)), jour noté ;
    - le même jour, rien ne bouge ;
    - un habitant sans mode de vie n'en reçoit pas.
  - `Anastasis.Sim.Harnais.Lecture` : un habitant sans mode de vie est refusé.
- RAPPORT JS / Unreal (`endurance`, 16 200 ticks) : premier tick divergent toujours 1 (`actors`,
  B=`143261ff92332a01`). Le reste ne change pas : `buildings` et `rng` au tick 32, `mealReservations` au 133,
  `tileDiff` au 257.
  - Forage du tick 1 : **15 champs** au lieu de 20 (45 au rapport 2). `lifestyle` est égal à la référence pour les cinq habitants.
  - Il reste, par habitant :
    - `placeMemory.favoriteBuildingId` et `placeMemory.buildings.building-0` : `notePlaceUse` de `simulation.js`, non porté ;
    - `workTimer` : chemin d'acte du but `observer`, non porté.
- COMMANDS : celles de `needs-wiring-001`.

## ECARTS

- modifié : n° 8 — l'habitant créé par le C++ n'a pas de mode de vie, et l'`ensureLifestyle` de tête, qui le tirerait dans `sim.rng`, n'est pas branché. Les autres lectures du mode de vie (biais de décision, marche, intérieur, destination, `placeMemory`) attendent la décision et `placeMemory`.
- aucun ouvert, aucun fermé : `lifestyleDailyUpdate`, la lecture et la projection sont fidèles. Preuves : `Anastasis.Sim.Village.ModeDeVie`, `Anastasis.Sim.Parite.ModeDeVie` et le forage du tick 1.

## PROOFS

PROOFS: (aucune)

## SCN

`endurance`, inchangé.

## PLY

Sans objet.

## INTEGRATION_RISK

- Dépend de `needs-wiring-001`.
- `perception-explore-001` (renfort) touche la décision et `Perceive` dans `AnastasisVillage.cpp`, et ajoute
  `FNpc::KnownCells`. Elle lit `FNpc::Lifestyle` sans le modifier. Les régions de code sont distinctes ;
  fusion de texte possible dans `FNpc`.
- Le lecteur refuse désormais un habitant sans `lifestyle`. Les scénarios existants en ont un ; un scénario
  plus ancien serait refusé, avec un message clair.

## STOP

- La divergence au tick 1 n'est pas levée : il reste `placeMemory` (`notePlaceUse`) et `workTimer`.
- `lifestyleBias` n'entre pas dans la table de décision. Ce branchement revient à la mission de la décision.
