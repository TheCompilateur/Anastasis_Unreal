# HANDOFF: act-gate-001

## MISSION

Fermer les deux derniers champs qui divergeaient au tick 1 du scénario `endurance` (rapport 4) :
`placeMemory` et `workTimer`. Les deux viennent du même endroit de la référence, la porte générique d'`act`
(`npc.js` l. 3515-3540). À chaque tick, un habitant arrivé, ou sans cible, note son lieu
(`notePlaceUse`) puis avance `workTimer` ; à 1 s, il fait `perform`.

- `FVillage::NotePlaceUse`, portage de `simulation.js` `notePlaceUse` :
  - choix du bâtiment : dedans, sinon le plus proche (1,9), sinon le poste pour un geste de travail ;
  - déclin par jour, gains par sorte (travail, social, parole, boisson, foyer, crise) ;
  - `lifestyleNotePlaceUse` (module de `lifestyle-001`) ;
  - `laborToday` du poste ;
  - favori (premier à score égal, lieux vus depuis 18 jours).
- `FNpc::PlaceEntries` / `FavoriteBuildingId` (`npc.placeMemory`), `FBuilding::LaborToday` (`building.laborToday`).
- `observer` passe désormais par la porte générique : il note le lieu, attend, `perform` échoue et l'échec compte.
  La porte générique appelle aussi `redirectAfterFailure` à 3 échecs, et ne relâche la cible que si l'acte n'en a
  pas posé une autre (`if (npc.target === target)`).
- `notePlaceUse` branché partout où le C++ suit la référence :
  - attente à la porte : `waitingActivity`, `dt × 0,35`, puis le but, 1 ;
  - cueillette du fermier : `recolte`, `dt × 0,35` ;
  - chantier : `chantier`, `dt × 0,35` ;
  - intérieur : `dt × 0,5` à chaque tick, puis 1,2 après l'acte, avant la sortie ;
  - entrée dans un bâtiment : 0,8 ;
  - conversation : `dt × 0,35`.
- Lecteur du harnais : `placeMemory` et `laborToday`, lus et projetés.

## FILES_OWNED

- `Source/AnastasisSim/Public/Village/AnastasisVillage.h` : `FNpc::FPlaceEntry`, `PlaceEntries`, `FavoriteBuildingId`, `FBuilding::LaborToday`, `NotePlaceUse`, `JsWaitingActivity`, `UpdateInside(Npc, Dt)`
- `Source/AnastasisSim/Private/Village/AnastasisVillage.cpp` : `NotePlaceUse`, `JsWaitingActivity`, `Act` (porte générique), `UpdateInside`, `EnterBuilding`, `UpdateNpc` (conversation)
- `Source/AnastasisSim/Private/Village/AnastasisVillagePlayer.cpp` : appel `UpdateInside(Npc, Dt)`
- `Source/AnastasisSim/Private/Harness/AnastasisJsSave.cpp` : `placeMemory`, `laborToday`
- `Source/AnastasisSim/Private/Tests/AnastasisVillageActGateTests.cpp` (nouveau)
- `Source/AnastasisSim/ECARTS.md` : n° 8 complété, n° 26 nouveau
- `docs/unreal/handoffs/act-gate-001.md`

## COMMIT

Voir `git log agent/act-gate-001`.

## MEC

- BUILD : `anastasis-unreal.ps1 build` → `BUILD::PASS`.
- TESTS : `report-tests.ps1 -Filter Anastasis.Sim` (`ANASTASIS_HARNESS_TICKS=16200`, `ANASTASIS_HARNESS_DRILL=1`)
  → PASS 121, KNOWN_EXPECTED_FAILURE 2 (`Parite.Fbm`, `Parite.SemantiqueJs`), FAIL 0, 123/123 annoncés.
  - `Anastasis.Sim.Village.GesteEtLieux` (nouveau) : un observateur sans cible au pied d'un grenier, pensée gelée.
    - Premier tick : entrée « grenier », score et activité de 0,05 (gain minimal), jours 1, flâneur 0,05,
      favori = le grenier, `workTimer` = dt, affichage `attend`, pas de `laborToday` (« attend » n'est pas
      un travail).
    - Un habitant sans mode de vie : entrée sans mode de vie, et aucun mode de vie ne lui est donné.
    - À 1 s de geste, `perform` échoue : `FailedActions` = 1, le score égale la somme des gains, le but
      reste `observer`.
  - Tous les `Village.*` existants restent verts, `Recolte.*` et `MeteoHabitants.*` compris.
- **RAPPORT JS / Unreal** (`endurance`, 16 200 ticks, référence = clone du tag `anastasis-ref-p3`) :
  - **premier tick divergent : 32** (au rapport 4 : 1). Du tick 1 au tick 31, les dix sections sont identiques au bit près ;
  - forage du tick 1 : **0 champ différent** (15 au rapport 4, 45 au rapport 2) ;
  - au tick 32, `actors` et `buildings` divergent : c'est le point d'accès du puits retiré par la référence au
    premier appel de `spatialRiskBiasMap` (fiche n° 24, diagnostic de « Migration simulateur vers Unreal ») ;
  - `rng` au tick 125 (`maybeChatOnHaul`, `chat-on-haul-001`), `mealReservations` au tick 133, `tileDiff` au tick 257.

## ECARTS

- ouvert : n° 26 — mémoire des lieux sans présence au poste (`workPresence`, `workAtWorkplaceYard` non portés) (A_FERMER, workplace-presence-001).
- modifié : n° 8 — `lifestyleNotePlaceUse` sauté pour un habitant sans mode de vie : il en tirerait un dans le flux de secours.
- `notePlaceUse` et la porte générique d'`observer` sont fidèles. Preuves : `Anastasis.Sim.Village.GesteEtLieux`, et le harnais, identique au bit près jusqu'au tick 31.

## PROOFS

PROOFS: (aucune)

## SCN

`endurance`, inchangé.

## PLY

`UpdatePlayer` passe `Dt` à `UpdateInside` : le joueur dedans note son lieu comme un habitant.

## INTEGRATION_RISK

- `AnastasisVillage.cpp` : `Act` (porte générique), `UpdateInside`, `EnterBuilding` et le bloc de conversation
  d'`UpdateNpc`. `chat-on-haul-001` (renfort) touche `Deliver` / `Perform` : régions proches, fusion de texte possible.
- Comportement en jeu : un habitant qui `observe` échoue désormais son geste chaque seconde ; après 3 échecs,
  `redirectAfterFailure` l'envoie ailleurs (comme la référence). Avant, il attendait sans fin.
- Numéro d'écart : 25 est pris par `weather-dry-001` (en vol), d'où le n° 26.

## STOP

- Le premier tick divergent est 32, pas au-delà : le point d'accès du puits (n° 24) reste à brancher.
- `workPresence` n'est pas porté (n° 26).
- L'affichage C++ garde ses mots (« discute », « cueille », « depose ») là où la référence dit « socialise »,
  « recolte », « livre ». Le `kind` de `notePlaceUse` suit la référence (`JsWaitingActivity`), mais pas
  `npc.activity`, qui est projeté et divergera quand ces buts seront atteints.
