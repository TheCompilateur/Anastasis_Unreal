# HANDOFF: reader-rng-001

## MISSION

Raccorder le harnais au flux partagé `sim.rng` (`sim-rng-001`) et à la mémoire des régions
(`perception-explore-001`) :
- `save.rng` repris par `FVillage::SetSimRngState` après la remise à zéro de l'hôte ;
- la section `rng` projetée depuis l'état VIVANT du flux, et non plus depuis l'état lu, figé ;
- `npc.mind.cells` et `mind.cellCount` lus dans `FNpc::KnownCells` / `CellCount`, puis projetés.

## FILES_OWNED

- `Source/AnastasisSim/Private/Harness/AnastasisHarnessTrace.cpp` : `Restore` (`SetSimRngState`), `Snapshot` (`RngState` vivant)
- `Source/AnastasisSim/Private/Harness/AnastasisJsSave.cpp` : lecture et projection de `mind.cells` / `mind.cellCount`
- `Source/AnastasisSim/Private/Tests/AnastasisJsSaveTests.cpp` : deux contrôles de projection (régions, état du flux)
- `Source/AnastasisSim/ECARTS.md` (fiche n° 16)
- `docs/unreal/handoffs/reader-rng-001.md`

## COMMIT

Voir `git log agent/reader-rng-001`.

## MEC

- BUILD : `anastasis-unreal.ps1 build` → `BUILD::PASS`.
- TESTS : `report-tests.ps1 -Filter Anastasis.Sim` → PASS 118, KNOWN_EXPECTED_FAILURE 2 (`Parite.Fbm`,
  `Parite.SemantiqueJs`), FAIL 0, 120/120 annoncés.
  - `Harnais.Lecture` : `mind.cells` lue (région 104, `cellCount` 1) ; une région de plus change `actors` ; un état de flux différent change `rng`.
  - `Harnais.Trace` : au tick 0, les 10 sections ont toujours l'empreinte de la référence.
- RAPPORT JS / Unreal (`endurance`, 16 200 ticks, référence = clone du tag `anastasis-ref-p3`) :
  - **`rng` : première divergence au tick 125**, au lieu du tick 32. Au tick 125, la référence tire dans
    `maybeChatOnHaul` (`npc.js:5505`, depuis `deliver`, npc-0, état 3832238662) ; le C++ livre sans tirer.
  - Premier tick divergent : 1 (`actors`, 15 champs : `placeMemory`, `workTimer`) ; `buildings` au tick 32,
    `mealReservations` au 133, `tileDiff` au 257 (inchangés).
- COMMANDS : celles de `needs-wiring-001` ; relevé : `docs/migration/phase3/P3_RNG_RELEVE_600.csv`.

## ECARTS

- modifié : n° 16 — `VillageRng` est `sim.rng`, repris de `save.rng` et projeté vivant ; `rng` identique jusqu'au tick 125, où la référence tire dans `maybeChatOnHaul`, non porté.
- aucun ouvert, aucun fermé : la reprise et les projections sont fidèles à `deserialize` (`sim.rng.setState(data.rng)` réancré en fin de chargement, `save.js` l. 485). Preuves : `Anastasis.Sim.Harnais.Lecture` et `.Trace` (tick 0 identique).

## PROOFS

PROOFS: (aucune)

## SCN

`endurance`, inchangé.

## PLY

Sans objet.

## INTEGRATION_RISK

- `reconsider-001` (renfort) ajoutera `FNpc::PhaseChangedAt` ; la lecture de `phaseChangedAt` viendra après son versement, dans une mission du lecteur.
- `mind.lastScan`, `scanX` et `scanY` (porte de `perceive`) ne sont ni lus ni projetés : ils restent ceux du départ.

## STOP

- La divergence `rng` n'est pas levée : `maybeChatOnHaul` au tick 125, puis la ligne 893 (`reconsider-001`).
- `actors` diverge toujours au tick 1 (`placeMemory`, `workTimer`).
