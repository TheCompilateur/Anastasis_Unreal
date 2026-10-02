# HANDOFF: resource-targets-001

## MISSION

Commandée par « Simulateur IV Kingdoms migration phase 3 » (2026-10-02), d'après mon diagnostic du tick 32 :
au tick 32 d'endurance, la référence retire du puits `building-1` son seuil (49.5, 57.5), bloqué dès le tick 0
par la maison `building-2`. Elle le fait de façon paresseuse, au premier `ensureBuildingAccessPoints`, qui
vient de la préparation de la décision : `adultScores` > `spatialRiskBiasMap`. Mission : porter
`spatialRiskBiasMap`, `spatialRiskTargetForGoal`, `recallOrSearch` et leurs cibles, et lever la divergence.
Élargie par le coordinateur à `survivalForecastBias`, calculée juste avant sur les mêmes replis et avec leurs
écritures (la couper en deux aurait cassé l'ordre des écritures entre deux versements).

Relevé d'abord (`tools/migration/trace-spatial-risk.mjs`, référence instrumentée, endurance, 16 200 ticks →
`docs/migration/phase3/P3_RISQUE_SPATIAL_RELEVE.md`) :
- 381 décisions, biais non nul dans 255 (dès le tick 903) ; la prévision de survie, non nulle dans 352 dès le tick 82 ;
- une seule écriture de seuil, le puits au tick 32 ;
- 57 tirages, tous dans `intentExploreHint` (intention du jour, écart n° 24, hors mission).

Porté :
- `Ai/AnastasisSpatialRisk` (pur) : budgets (faim, soif, fatigue, nuit, averse), trajet aller-retour,
  dépassement, carte du risque une fois replis et cibles connus ; prévision de survie complète ;
- `Village/AnastasisVillageSpatialRisk.cpp` : replis et cibles, dans l'ordre d'appel de la référence ;
- branchement dans `ChooseGoal` : préparation après la lecture du planificateur (`CollectiveDecisionOf`) et
  avant `failureTargetBiasMap` ; les deux cartes s'ajoutent à chaque ligne après la météo, avant la passe
  collective, comme dans la chaîne `addScore` ;
- lecteur : `mind.spots` lus (ordre des clés) et reprojetés. Sans eux, `recallOrSearch` ne connaissait aucun
  gisement et tirait `exploreTarget` (zone du lecteur confiée par le coordinateur).

## FILES_OWNED

- `Source/AnastasisSim/Public/Ai/AnastasisSpatialRisk.h`, `Private/Ai/AnastasisSpatialRisk.cpp` (nouveaux)
- `Source/AnastasisSim/Private/Village/AnastasisVillageSpatialRisk.cpp` (nouveau)
- `Source/AnastasisSim/Private/Tests/AnastasisSpatialRiskTests.cpp`, `AnastasisSpatialRiskVectors.inl` (nouveaux, le
  `.inl` généré par `node tools/migration/gen-parity.mjs -ref <clone> spatial-risk.mjs`)
- `tools/migration/parity/spatial-risk.mjs`, `tools/migration/trace-spatial-risk.mjs` (nouveaux)
- `Source/AnastasisSim/Public/Village/AnastasisVillage.h` (déclarations, champs de trace `ForecastBias` / `SpatialRiskBias`)
- `Source/AnastasisSim/Private/Village/AnastasisVillage.cpp` : `ChooseGoal`, deux blocs balisés `--- resource-targets-001`
- `Source/AnastasisSim/Private/Harness/AnastasisJsSave.cpp` : bloc `mind` des acteurs (lecture et projection de `spots`)
- Tests modifiés : `AnastasisVillageSimTests.cpp` (`Puits.Selection`), `AnastasisVillageWeatherTests.cpp` (`Orage`),
  `AnastasisVillageGatherTests.cpp` (`Recolte.MultiAgents`)
- `Source/AnastasisSim/ECARTS.md`, `PORTAGE.md`, `tools/migration/ported-functions.mjs`,
  `docs/migration/phase2/P2_INVENTAIRE_JS.md` (régénéré), `docs/migration/phase3/P3_RISQUE_SPATIAL_RELEVE.md`

## COMMIT

Les commits de `agent/resource-targets-001`, posés sur `agent/planner-wiring-001` (a78518c), qui contient
planner-module-001.

## MEC

- BUILD : `tools\unreal\anastasis-unreal.ps1 build` → `BUILD::PASS`.
- `Anastasis.Sim.Parite.RisqueSpatial` : 9 cas, 772 vecteurs de la référence exécutée, 5 780 valeurs, 0 écart
  (384 cartes de risque dont 375 non vides, 96 prévisions dont 84 non vides).
- Suite `Anastasis.Sim`, sur planner-wiring-001 : 126 PASS, 2 KNOWN_EXPECTED_FAILURE, 0 FAIL.
- Harnais (endurance, 600 ticks, `compare-digests.mjs` contre la référence) : `buildings` ne diverge plus au
  tick 32 mais au tick 125 (avec `rng`) ; `tileDiff` 257, `mealReservations` 320. Forage du tick 32
  (`diff-states.mjs`) : aucun champ de `buildings`. npc-2 choisit `build` comme la référence. Dans `actors`
  restent ce qu'écrit la première pensée (`goalExplain`, `workShift`, `streetDecision`, `hungerAction`,
  `hesitation*`, `mind.failures`, `activitySince`) et la navigation (n° 4).
- Tests de scénario ajustés, avec leur raison :
  - `Puits.Selection` : la ligne `drink` exacte inclut la prévision (`score.survival_forecast`) ;
  - `Orage` : le fermier part frais (énergie 100). Parti de 70, il est à 45,7 à sa première décision sous
    l'orage : fatigue critique, la porte d'orage ne joue plus, et la prévision fait gagner `rest`, comme dans
    la référence. Le test prouve le chemin de l'abri ;
  - `Recolte.MultiAgents` : deux fermiers au moins ensemble sur la tuile, jamais au même poste, chacun a cueilli.
    Les trois à la fois dépendaient des horaires de livraison, que les nouveaux tirages décalent.

## PROOFS

PROOFS: (aucune)

Décision seule, sans scène : preuve par `Parite.RisqueSpatial`, la suite et le harnais.

## SCN

Aucune scène, aucun asset.

## PLY

Sans objet.

## ECARTS

- ouvert : n° 33 — Prévision de survie et risque spatial : les croyances des lieux sûrs (`bestKnownWater`,
  `bestKnownBed`, l'écriture de `seedHomeBedBelief`) et la doctrine de lisière (`wantsColonizationClear`) ne sont
  pas portées. Fermeture : beliefs-001 ; doctrine à attribuer.
- modifiés :
  - n° 24 : la partie `spatialRiskBiasMap` / `recallOrSearch` est portée ; reste l'intention du jour
    (`intentExploreHint`, ses 57 tirages) ; `buildings` ajouté au champ harnais ;
  - n° 1 : la prévision de survie et le risque spatial sont portés et s'ajoutent à chaque ligne ;
  - n° 17 : le biais de la prévision de survie est porté ; `bestKnownBed` renvoie au n° 33 ;
  - n° 3 : `pickDailyBuilding` sans quartiers prend le repli par hachage ;
  - n° 7 : sans plan d'aide du foyer, la cible `aidHousehold` est le foyer ou `socialPos`.
- hérités de la base (`agent/planner-wiring-001`, qui contient planner-module-001), non modifiés ici : n° 32
  (départage ordinal de la corvée de bois), et n° 27 tel que planner-wiring-001 l'a réécrit.

## INTEGRATION_RISK

- Dépend de `agent/planner-wiring-001` (qui contient planner-module-001) : à verser APRÈS elle.
- `ChooseGoal` : mes deux blocs sont balisés ; l'ordre `Work` → `CollectiveDecisionOf` → mon bloc est convenu
  avec le coordinateur.
- Le lecteur change : `mind.spots` est relu et reprojeté. La projection d'un gisement d'on-dit créé par le C++
  n'a pas `speechActId`, `confidence`, `viaPlayerId` (port des rumeurs, n° 14/16) : divergence possible sur
  `actors` le jour où une rumeur de gisement passe.
- Les décisions changent partout où un biais n'est pas nul : tout test de scénario qui supposait l'ancienne
  table peut bouger (trois l'ont fait, ajustés ci-dessus).

## STOP

- Pas les croyances (`mind.beliefs`) : écart n° 33, mission beliefs-001.
- Pas l'intention du jour : `intentExploreHint` reste au n° 24 ; la cible `explore` est `npc.target`.
- Pas les postes d'extraction, scierie, élevage, taverne, gardes : absents du catalogue porté (puits, maison,
  grenier), leurs branches ne peuvent pas s'y présenter.
