# HANDOFF: state-fields-tidy-001

## MISSION

Ranger les 33 champs d'état arrivés sur `main` après STATE_ORACLE_001, que `check-state-fields.mjs`
signalait sans les lire (WARN, « deja sur main ») :
- ils viennent de `nav-wiring-001`, `lifestyle-decision-001`, `premiere-pensee-001`, `soil-water-budget-001`… ;
- c'est un préalable à la sauvegarde de partie (chantier 4) : une sauvegarde prouvée par `StateDigest` ne
  doit pas pouvoir oublier un champ sans qu'un test le voie.

## FILES_OWNED

- `Source/AnastasisSim/Private/Village/AnastasisVillageStateDigest.cpp` : 31 champs lus, 7 hacheurs ajoutés
  (`FGoalExplainEntry`, `FGoalExplain`, `FStreetDecision`, `FNature`, `FNavJob`, `FNavCacheEntry`, `FNavService`)
- `tools/migration/state-fields.json` : les 7 structures, plus deux champs classés `cache:`
  (`FVillage::NavAgents`, `FVillage::NavSourceShared`)

## COMMIT

Commité le 2026-10-08 sur `main` = `423955c5`.

## MEC

- BUILD: PASS (`tools\unreal\anastasis-unreal.ps1 build`, worktree)
- TESTS: `report-tests.ps1 -Filter 'Anastasis.Sim.Empreinte+Anastasis.Sim.Village.Puits.MultiAgents+Anastasis.Sim.Joueur.Observateur+Anastasis.Sim.Mortality.Daily+Anastasis.Sim.Village.Repousse.Hote+Anastasis.Village.Villagers.Presentation+Anastasis.Iron'`
  → PASS 10 / KNOWN_EXPECTED_FAILURE 0 / FAIL 0, run complet.
  - `Anastasis.Sim.Empreinte.Etat.Deterministe` : deux simulations identiques ont le même état complet
    après deux jours simulés. Le service de navigation (file, cache, budget), le trafic et l'eau du sol y
    entrent maintenant : aucun non-déterminisme caché.
  - Les cinq assertions d'état complet tiennent, dont « la presentation n'ecrit pas dans l'etat complet ».
- COMMANDS:
  - `node tools/migration/check-state-fields.mjs` (strict) → `STATE_FIELDS::PASS structures=44 lacunes=2`, 0 WARN
    (33 avant)

## PROOFS

PROOFS: (aucune)

## SCN

N/A — `StateDigest` n'est appelé que par des tests.

## PLY

N/A

## ECARTS

AUCUN — lecteur pur de l'état, aucune écriture, aucun tirage. `Digest()`, la projection de parité, est inchangé.

## INTEGRATION_RISK

- Une mission en vol qui touche `AnastasisVillageStateDigest.cpp` ou `state-fields.json` peut entrer en conflit
  textuel ; c'est le cas de `opening-in-sim-001`, qui hache `OpeningSiteId` et `OpeningHome`. Les
  résolutions sont additives : garder les deux blocs.
- Le cache du service de navigation est lu dans son ordre d'insertion, qui est déterministe (ses évictions
  suivent cet ordre).

## STOP

- Les deux lacunes déclarées (`Colony`, `MarketStock` du planificateur) restent à lire avant toute sauvegarde.
- Ne prouve pas l'exhaustivité au-delà des 44 structures du registre.
