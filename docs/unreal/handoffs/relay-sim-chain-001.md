# HANDOFF: relay-sim-chain-001

## MISSION

Relais d'integration (session integratrice, autorite d'Alexandre du 2026-10-07) : rejouer sur main la chaine
de simulation dont les agents sont hors ligne et dont les marqueurs `finish` etaient perimes :
premiere-pensee-001 -> lifestyle-decision-001 -> spatial-risk-test-001 -> nav-wiring-001 (chaque branche
contient la precedente). Branches d'origine intactes ; 14 commits cites par `cherry-pick -x`.

RELAIS: premiere-pensee-001, lifestyle-decision-001, spatial-risk-test-001, nav-wiring-001

## FILES_OWNED

Ceux des quatre missions. Seule resolution manuelle : `Source/AnastasisSim/ECARTS.md`, fiches n° 34, 35, 36
(premiere-pensee-001) placees entre n° 33 et n° 37 (soil-water-budget-001), avant n° 38 (geopolitical-world-001).
Les commits C++ (AnastasisVillage.cpp/.h, AnastasisJsSave.cpp, tests) se sont fusionnes sans conflit.

## COMMIT

PENDING

## MEC

- BUILD: voir finish
- TESTS: au lot (suite complete : tests Village.RisqueSpatial, premiere pensee, file de porte compris)
- Inventaire JS (`tools/migration/inventory-js-sim.mjs`) NON regenere : le depot de reference
  C:/dev/Jeux IV Kingdoms porte 36 modifications non commitees sous src/ et l'outil refuse (a juste titre).
  `docs/migration/phase2/P2_INVENTAIRE_JS.md` s'est fusionne sans conflit.

## PROOFS

PROOFS: (aucune)

## SCN

Sans objet.

## PLY

Sans objet.

## ECARTS

- n° 34 OUVERT (premiere-pensee-001, A_TRANCHER), inchange.
- n° 35 OUVERT (premiere-pensee-001, A_TRANCHER), inchange.
- n° 36 OUVERT (premiere-pensee-001, A_TRANCHER), inchange. Les trois places en ordre numerique.
- n° 29 MODIFIE (route-cost-001, extension mode jeu) : le seuil de blocage de la marche portee par nav-wiring-001
  (0,05 case par pas, celui de la reference) est divise par le multiplicateur de terrain du dernier segment
  quand il depasse 1 (`AnastasisVillageNav.cpp`, marques `ecart n°29`). Sans cela, le lot 9 du 2026-10-07 echouait :
  `Anastasis.Sim.Village.Recolte.RouteCostDelivery`, herbe detrempee `wet-grass=nan` (le fermier, ralenti, etait
  pris pour coince et recalculait son chemin sans fin). Apres : road=7.150 grass=8.083 wet-grass=11.767
  reference-grass=8.050 ; `report-tests -Filter Anastasis.Sim` dans ce worktree : 158 PASS / 2 KEF / 0 FAIL.
  Mode reference (parite JS) inchange : LastTravelCost reste 1.
- Aucun autre ecart ouvert, modifie ou ferme.

## INTEGRATION_RISK

- Fusion automatique de C++ de simulation sur un main qui a recu geopolitical-world-001 (OnNewDay,
  AdmitExternalArrivals) et soil-water-budget-001 (RegrowFieldsDaily) : textuellement propre, semantiquement
  jugee par la suite complete du lot. Un echec de test Anastasis.Sim.* ou Village.* designe ce relais.

## STOP

- Ne tranche aucun ecart. Ne regenere pas l'inventaire JS.
