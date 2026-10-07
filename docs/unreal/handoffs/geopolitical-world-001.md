# HANDOFF: geopolitical-world-001

## MISSION

ANASTASIS_GEOPOLITICAL_WORLD_V1 (Alexandre, 2026-10-07) : le monde exterieur comme champ de pressions
qui se propagent jusqu'au village, avec une information separee de la verite, une trace causale, une
persistance et UN branchement local reel (groupe de migrants -> habitants). Doc : `docs/unreal/GEOPOLITICAL_WORLD_001.md`.

## FILES_OWNED

- Source/AnastasisSim/Public/Geo/AnastasisGeo.h
- Source/AnastasisSim/Private/Geo/AnastasisGeo.cpp
- Source/AnastasisSim/Private/Village/AnastasisVillageGeo.cpp
- Source/AnastasisSim/Private/Tests/AnastasisGeoTests.cpp
- Source/AnastasisSim/Public/Sim/AnastasisSimulation.h (membre `Geo`, `AdmitGeoMigration`)
- Source/AnastasisSim/Private/Sim/AnastasisSimulation.cpp (`OnNewDay`, `Reset`, `ResetFromWorld`, `AdmitGeoMigration`)
- Source/AnastasisSim/Public/Village/AnastasisVillage.h (`AdmitExternalArrivals`, une declaration)
- Source/AnastasisSim/ECARTS.md (n° 38)
- Source/Anastasis_UnrealV2/Sim/AnastasisSimulationGeo.cpp
- Source/Anastasis_UnrealV2/Sim/AnastasisSimulationSubsystem.h (`LogGeoEvents`, `GetGeoStatus`, `GetGeoTrace`)
- Source/Anastasis_UnrealV2/Sim/AnastasisSimulationSubsystem.cpp (`LogDayIfChanged` appelle `LogGeoEvents`)
- Content/Anastasis/Scenario/geo-pontos-1204.json
- tools/unreal/geo-remote-crisis-pie.py, tools/unreal/proofs.txt (une ligne), AGENTS.md (une ligne d'index)
- docs/unreal/GEOPOLITICAL_WORLD_001.md, docs/unreal/handoffs/geopolitical-world-001.md
- docs/historicity/briefs/geopolitical-world-001.json, docs/historicity/SOURCES.md (HIS-04 a HIS-06)

## COMMIT

Voir `git log agent/geopolitical-world-001`.

## MEC

- BUILD: PASS (adaptatif puis unity apres commit, sur main 5c756b4c rebase)
- TESTS: `report-tests.ps1 -Filter Anastasis.Sim.Geo` sur la premiere version : PASS 13, KNOWN_EXPECTED_FAILURE 0, FAIL 0. La suite complete (version finale) : voir le resultat de `finish -Prove` ci-dessous.
- PREUVE PIE : `editor-batch.ps1 -Proofs geo-remote-crisis-pie` -> `PROOF::PASS geo-remote-crisis-pie (243.1s)`, `EDITOR_BATCH::PASS 1/1` ; sortie `Saved/GeoEvidence/pie/geo-remote-crisis.json`.
- Chronologie observee en PIE (Lvl_AnastasisSlice, crise injectee a Konya au jour 1, `Anastasis.Sim.Advance 1d`) :
  - jour 1 : rien au village (ni arrivee ni savoir de cette cause) ;
  - jour 8 : la nouvelle de la chute de Constantinople (choc historique du scenario) arrive, fiabilite 0,614, 3 relais ;
  - jour 11 : la nouvelle de la crise de Konya arrive, fiabilite 0,470, 4 relais ; corroboree au jour 12 par une autre route ;
  - jour 11 : `Anastasis.Geo.Save` puis `Restore` avec 7 paquets + 5 nouvelles en route : etat identique ;
  - jour 13 : penurie issue de 1204 au village, 0,256 ;
  - jour 19 : penurie de Konya au village, m = 0,369 par konya->sinope->trebizond->matzouka->village ; exposition 0,169 -> 0,467 ;
  - jour 25 : un arrivant de 1204 admis (npc-12) ;
  - jour 37 : un arrivant de Konya admis (npc-13), habitants 11 -> 12 ;
  - trace : `village TradeDisruption` remonte pk-23 <- pk-21 <- pk-15 <- pk-8 <- pk-5 -> `remote-crisis-proof`.
- COMMANDS:
  - `tools\unreal\anastasis-unreal.ps1 build`
  - `tools\unreal\report-tests.ps1 -Filter Anastasis.Sim.Geo`
  - `tools\unreal\editor-batch.ps1 -Proofs geo-remote-crisis-pie`
  - `python tools/historicity/check-brief.py docs/historicity/briefs/geopolitical-world-001.json` -> `BRIEF::PASS traceability only`

## PROOFS

PROOFS: geo-remote-crisis-pie

## SCN

UNKNOWN : aucune scene visuelle revendiquee. Les arrivants sont des habitants ordinaires (carte ou corps
du village), poses a 7 cases du camp ; aucune mise en scene d'arrivee.

## PLY

UNKNOWN : le joueur ne choisit rien face au monde exterieur en V1.

## ECARTS

- n° 38 OUVERT (EXTENSION, A_TRANCHER) : monde exterieur, hote seulement. Decharge, `OnNewDay` est inchange au bit pres (`Anastasis.Sim.Geo.CalmeSansEffet` : empreinte du village et etat du flux `sim.rng` identiques avec un monde exterieur charge mais calme). Aucun tirage aleatoire nouveau. n° 34 a 37 sont pris par d'autres branches (premiere-pensee-001, soil water budget).

## INTEGRATION_RISK

- Verser `pontos-histoire-001` avant ou avec cette mission : le scenario et `SOURCES.md` (HIS-04 a HIS-06) citent `docs/recherche/histoire-pontique/`.
- `AnastasisSimulation.cpp` (`OnNewDay`) et `AnastasisSimulationSubsystem.{h,cpp}` sont des fichiers chauds : ajouts localises.
- Numero d'ecart 38 : verifier au versement qu'aucune autre branche ne l'a pris entre-temps.
- Fermeture de l'editeur apres `GEO_PIE_COMPLETE` : crash `python311.dll` / `PythonScriptPlugin` (EXCEPTION_ACCESS_VIOLATION, aucune frame Anastasis), connu du projet (crash Python de fermeture) ; la preuve a rendu son verdict avant.
- Le scenario n'est pas charge par defaut : aucune autre preuve PIE ne voit le monde exterieur.

## STOP

- Ne revendique pas de realisme historique des durees, intensites ni de la fuite de 1204 jusqu'au Pont : `ABSTRACTION` / `PLAUSIBLE` dans le scenario.
- `TradeDisruption`, `Insecurity`, `Military`, `Extraction` sont exposes mais n'ont pas de consommateur local : `BLOCKED_BY_MISSING_LOCAL_OWNER` (pas d'echange porte en C++).
- Pas de sauvegarde de jeu canonique : la persistance passe par `Anastasis.Geo.Save` / `Restore`.
- Pas de memoire des relations village-acteurs, pas de diplomatie, pas d'IA d'acteur.
