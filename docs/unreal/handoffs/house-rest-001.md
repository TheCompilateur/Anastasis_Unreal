# HANDOFF: house-rest-001

## MISSION

Brancher la maison au simulateur : un habitant rentre chez lui la nuit, entre (`npc.inside`),
dort, et son énergie remonte selon la qualité de son lit (foyer 1,12, abri 0,78, ailleurs 0,42),
exactement au bit près du JS sur le tick de sortie. Détail : `docs/unreal/HOUSE_REST_001.md`.

## FILES_OWNED

Créés :

- `Source/AnastasisSim/Public/Life/AnastasisVillageRhythm.h`, `Private/Life/AnastasisVillageRhythm.cpp`
- `Source/AnastasisSim/Private/Tests/AnastasisRhythmTests.cpp`, `AnastasisRhythmVectors.inl`, `AnastasisDomesticVectors.inl` (générés), `AnastasisVillageHouseTests.cpp`
- `tools/migration/parity/village-rhythm.mjs`, `tools/migration/parity/domestic.mjs`, `tools/unreal/house-rest-pie.py`
- `docs/unreal/HOUSE_REST_001.md`, cette fiche

Modifiés :

- `Source/AnastasisSim/{Public,Private}/Life/AnastasisNeeds.*` — branche intérieure rest, satisfyRest, sleepQuality
- `Source/AnastasisSim/{Public,Private}/Village/AnastasisVillage.*` — maison, foyer, intérieur, plancher rythmé
- `Source/AnastasisSim/Private/Sim/AnastasisSimulation.cpp` — `assignSheltersDaily` à minuit
- `Source/AnastasisSim/Private/Tests/AnastasisNeedsTests.cpp`, `AnastasisNeedsVectors.inl`, `AnastasisVillageSimTests.cpp`
- `Source/Anastasis_UnrealV2/Sim/AnastasisSimulationSubsystem.*` — `FirstHouse`, `UAnastasisSimulationDebugLibrary`
- `Source/Anastasis_UnrealV2/Village/AnastasisVillagePresentation.cpp`, `AnastasisFirstBuildingTests.cpp`
- `tools/migration/parity/needs.mjs`, `Source/AnastasisSim/PORTAGE.md`, `docs/unreal/FIRST_BUILDING_001.md`, `AGENTS.md`

## COMMIT

BRANCH_HEAD

## MEC

- BUILD: PASS — `tools\unreal\anastasis-unreal.ps1 build`, `Module.Anastasis_UnrealV2.cpp`
  compilé en unity avec les fichiers de la mission (base de preuve : voir TESTS).
- TESTS: PASS — `tools\unreal\report-tests.ps1 -Filter Anastasis`, run complet (120 annoncés, 120 exécutés),
  sur `8049685` (base `main@a699adb`) puis refait par `finish` après chaque rebase :
  ```
  PASS                  : 116
  KNOWN_EXPECTED_FAILURE: 4   (le registre : Sim.Parite.Fbm, Sim.Parite.SemantiqueJs, 2 x AI.Toolsets.AnastasisInspect)
  FAIL                  : 0
  ```
  Nouveaux, tous PASS : `Sim.Parite.Rythme` ; `Sim.Village.Maison.Enregistrement`, `.Selection`,
  `.Sommeil` (entrée, un tick dedans et le tick de sortie égaux au bit près à
  `tickNeedsRest` + `satisfyRest` ; énergie 40 -> 100 en une nuit de 11,5 s ; il se recouche),
  `.MultiAgents` (3 abrités + 1 sans abri : 4 dedans à la fois, qualités 0,78 / 0,42, un dormeur
  retiré), `.Destruction` (maison occupée démolie : dormeur sorti, foyer et abri effacés),
  `.Inaccessible` (foyer de l'autre côté du fleuve : repos dehors, jamais d'entrée),
  `.Hote` (minuit : `assignSheltersDaily` abrite le sans-toit) ;
  `Village.FirstBuilding.MaisonPresentation`. Les 7 tests du puits restent PASS, rendus conscients de la phase.
  En cours de route : 2 FAIL dans les tests du puits, dus à des assertions devenues fausses
  avec le rythme (seuil fixe, +6 exact) ; tests corrigés, pas le code.
- PARITÉ (exécutée contre la référence `fee66ae`, `node tools/migration/gen-parity.mjs needs.mjs domestic.mjs village-rhythm.mjs`) :
  `Anastasis.Sim.Parite.Besoins` 495 vecteurs, 0 écart ; `Anastasis.Sim.Parite.Rythme` 649 vecteurs, 0 écart.
- COMMANDS:
  - `tools\unreal\anastasis-unreal.ps1 build`
  - `tools\unreal\report-tests.ps1 -Filter Anastasis`
  - `UnrealEditor-Cmd ... -ExecCmds="py tools/unreal/house-rest-pie.py"`

## SCN

PASS (log runtime, pas de capture) — `tools/unreal/house-rest-pie.py` en PIE sur
`Lvl_AnastasisSlice`, `8049685` (base `main@a699adb`), exit 0, `HOUSE_REST_COMPLETE`, piloté par
l'état de la simulation. Non refait après le rebase sur `64fea21` (souris/entrée, aucun fichier commun) :

- matin : `FirstHouse 4` -> `building-0` (possédée par npc-0) et `building-1` (libre, 3/3 abrités),
  acteurs à la nouvelle échelle spatiale de `main` (X=95000 = 47,5 tuiles).
- nuit, t=85,1 : npc-0 DANS sa maison (qualité 1,12), énergie 47,99 -> 86,99 six secondes plus tard ;
  les abrités dorment dans `building-1` (0,78).
- `RemoveNpc npc-2` : `actors=3`, le reste continue.
- `RemoveBuilding building-0` (occupée, `inside=1`) : « actor destroyed », npc-0 dehors, sans foyer,
  `navVersion` 2 -> 3.
- aube, t=108,8 : npc-0 a redécidé et dort dans `building-1` (0,42, pas son abri).
- Seules erreurs du log : 20 x `Condition failed` (bruit de démarrage connu).

## PLY

UNKNOWN — aucun contrôle humain ; les habitants sont des tracés de debug. `PLAYER` reste NOT_IMPLEMENTED.

## INTEGRATION_RISK

- **Le seuil de soif du puits change** : il suit désormais la phase (40 la nuit, à l'aube et à
  midi ; environ 64 le matin). C'est voulu (le rythme JS s'applique à toutes les lignes), mais
  toute démo ou attente écrite sur « boit à 40 » est périmée.
- `AnastasisVillagePresentation.cpp` : rebasé sur la projection de `afaf2b6` (échelle spatiale,
  raycast sur la surface rendue). Le seul conflit a été résolu en gardant les deux apports.
- `UAnastasisSimulationDebugLibrary` : nouvelle `UCLASS` (bibliothèque Blueprint), en lecture seule.
- Aucun `.uasset` ni `.umap` touché ; le PIE n'a pas de maison tant que personne ne tape
  `Anastasis.Village.FirstHouse`.

## STOP

- Pas de parité de trajectoire (table réduite à rest + drink + plancher rythmé).
- Pas de famille, pas d'achat de maison, pas d'agrandissement, pas de dortoir, pas d'hospitalité.
- Pas de contrôle de capacité à l'entrée — c'est la référence, pas un oubli.
- Pas de sauvegarde (voir FIRST_BUILDING_001 §6) ; `Digest()` projette désormais foyer, abri et intérieur.
