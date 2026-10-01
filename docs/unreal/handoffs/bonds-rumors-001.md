# HANDOFF: bonds-rumors-001

## MISSION

Porter la branche « avec compagnon » de `socialize()` : relations, conversations, fiches de
personnes, rumeurs de gisements, oubli quotidien. Détail : `docs/unreal/BONDS_RUMORS_001.md`.

## FILES_OWNED

Créés :

- `Source/AnastasisSim/{Public,Private}/Life/AnastasisBonds.*`
- `Source/AnastasisSim/Private/Tests/AnastasisBondsTests.cpp`, `AnastasisBondsVectors.inl` (généré),
  `AnastasisVillageBondsTests.cpp`
- `tools/migration/parity/bonds.mjs`
- `docs/unreal/BONDS_RUMORS_001.md`, cette fiche

Modifiés :

- `Source/AnastasisSim/{Public,Private}/Village/AnastasisVillage.*`
- `Source/AnastasisSim/{Public,Private}/Sim/AnastasisSimulation.*`
- `Source/AnastasisSim/Private/Tests/AnastasisSimulationTests.cpp`, `AnastasisVillageEnduranceTests.cpp`
- `Source/AnastasisSim/PORTAGE.md`

## COMMIT

BRANCH_HEAD

## MEC

- BUILD: PASS — `tools\unreal\anastasis-unreal.ps1 build` (base `main@a15f0b9`).
- TESTS: `tools\unreal\report-tests.ps1`, suite complète : 184 PASS, 4 KNOWN_EXPECTED_FAILURE
  (registre), 0 FAIL, 188 annoncés.
  Nouveaux, tous PASS : `Sim.Parite.Liens` (2 834 vecteurs), `Sim.Village.Liens.Conversation`,
  `.Amitie`, `.Rumeur`, `.Memoire`, `.Oubli`.
  Modifiés : `Sim.Tick.DayAdvance` (15 travaux restent après minuit, file vide en 9 ticks),
  `Sim.Village.Endurance` (la file est entamée, plus vidée, au tick de minuit ; 149 conversations
  avec compagnon, 466 gisements appris par on-dit, santé min 95).
  En cours de route : l'endurance a échoué (santé 0 au jour 11) — l'ancre du regard posée par la
  session devenait la destination durable d'un habitant en `observer`. Corrigé et déclaré (écart n° 16).
  `Village.Liens.Conversation` a d'abord échoué sur une attente fausse : la porte de parole filtre
  les premiers échanges (fidèle) ; le test compte désormais les N échanges.
- PARITÉ : `node tools/migration/gen-parity.mjs tools/migration/parity/bonds.mjs -ref <extraction git archive fee66ae>`.

## SCN

UNKNOWN — pas de preuve PIE dédiée.

## PLY

UNKNOWN — aucun contrôle humain. `PLAYER` reste NOT_IMPLEMENTED.

## INTEGRATION_RISK

- Les habitants qui se croisent se FIGENT 0,45 à 8,8 s : les scènes et preuves PIE qui suivent un
  habitant au puits peuvent le voir s'arrêter à côté d'un autre.
- La file de minuit compte 17 travaux : `GetDeferredRemaining()` vaut 15 après le tick de minuit
  (et non plus 0) ; `landRegen` reste au tick de minuit.
- `Reset` pose la graine du flux de tirages du village (rumeurs) sur celle du monde.

## STOP

- Pas de texte des répliques, pas de `shareRumors` hors gisements, pas de visites, conseils,
  rencontres quotidiennes ni frictions (écart n° 16).
- Pas de `relieve` (hygiène), `visitFamily`, `eatTogether`.
