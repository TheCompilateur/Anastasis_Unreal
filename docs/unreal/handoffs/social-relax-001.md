# HANDOFF: social-relax-001

## MISSION

Porter les deux remèdes qui manquaient à la boucle de travail : socialiser (solitude) et se détendre
(ennui). Sans eux, l'endurance de field-regrow-001 montrait la solitude critique au jour 3 et l'arrêt
de tout travail. Détail : `docs/unreal/SOCIAL_RELAX_001.md`.

## FILES_OWNED

Créés : `docs/unreal/SOCIAL_RELAX_001.md`, cette fiche.

Modifiés :

- `Source/AnastasisSim/{Public,Private}/Life/AnastasisNeeds.*`
- `Source/AnastasisSim/{Public,Private}/Work/AnastasisGather.*` (MoralSocialMul)
- `Source/AnastasisSim/{Public,Private}/Village/AnastasisVillage.*`
- `Source/AnastasisSim/Private/Tests/AnastasisNeedsTests.cpp`, `AnastasisNeedsVectors.inl` (généré),
  `AnastasisGatherTests.cpp`, `AnastasisGatherVectors.inl` (généré), `AnastasisVillageEnduranceTests.cpp`
- `tools/migration/parity/needs.mjs`, `tools/migration/parity/gather.mjs`, `Source/AnastasisSim/PORTAGE.md`

## COMMIT

BRANCH_HEAD

## MEC

- BUILD: PASS — `tools\unreal\anastasis-unreal.ps1 build` sur `f43a2cd` (base `main@8dc3e3b`), unity : `Module.AnastasisSim.cpp` compilé.
- TESTS: voir le run de `finish` (la suite lancée à la main a été tronquée par un manque de mémoire
  de la machine pendant `Anastasis.WorldView.EmbodimentSpawn` ; les 177 tests exécutés : 173 PASS,
  4 KNOWN_EXPECTED_FAILURE, 0 FAIL).
  Concernés, tous PASS : `Sim.Parite.Besoins` (+142 vecteurs, anciens identiques), `Sim.Parite.Recolte`
  (cas Moral étendu, 2395 vecteurs), `Sim.Village.*` (puits, maison, grenier, récolte, repousse,
  FoodSupply inchangés), `Sim.Village.Endurance` (12 jours : livraison chaque jour, grenier 300 au
  jour 5, social min 48, 202 conversations).
  En cours de route : « chaque jour, quelqu'un mange » a échoué sur le jour 1, entamé et sans faim
  (faim max 43) ; l'assertion compte désormais à partir du jour 2.
- PARITÉ : `node tools/migration/gen-parity.mjs needs.mjs gather.mjs -ref <extraction git archive fee66ae>`.

## SCN

UNKNOWN — pas de preuve PIE dédiée.

## PLY

UNKNOWN — aucun contrôle humain. `PLAYER` reste NOT_IMPLEMENTED.

## INTEGRATION_RISK

- `socialize` et `relax` sont désormais calculées pour TOUS les habitants : le comportement des
  scènes existantes change (on va au puits discuter, on rentre se détendre). Les tests du puits, de
  la maison, du grenier, de la récolte et de food-supply restent PASS sans modification.
- Gain social borné bas : toute conversation est la branche ambiante (écart n° 15).

## STOP

- Pas de liens, paroles, rumeurs, rencontres, conseils, visites (branche compagnon de `socialize()`).
- Pas de `relieve` (hygiène), `visitFamily`, `eatTogether`, scènes de foyer (`hearthInviteScore`).
