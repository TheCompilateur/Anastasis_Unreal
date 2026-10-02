# HANDOFF: help-farm-001

## MISSION

Deux pièces de la table de décision, tirées des relevés de la référence sur `endurance` :

1. **La passe collective de fin d'`adultScores`** (npc.js l. 1205-1258) : l'urgence collective, puis le
   plancher collectif après `workFactor` (un appoint si un besoin est critique, et la corvée de bois à 165),
   puis le rush famine. Elle est branchée à sa place dans `ChooseGoal` et alimentée par `FCollectiveDecision`
   (biais, planchers, urgence, corvée, rush, manque de fermiers). Le planificateur collectif qui la remplira
   n'est pas porté (`planner-module-001`, renfort) : la décision reste vide, la table garde ses bits (écart n° 27).
2. **Le but `helpFarm`, soigner une parcelle** :
   - la ligne (`helpFarmScore × wf`, puis la chaîne des biais) ;
   - la cible (la parcelle faible, `findTendFieldNear` à 8 cases) ;
   - l'acte, une session `tend` : marche, puis un coup par période ; chaque coup fait repousser le champ de
     `fieldSeasonTendAmount(2, jour)`, jusqu'à 4 coups ou la parcelle pleine.

Mesure qui motive les deux : dans la référence, le grenier n'est pas une ferme, la ligne `helpFarm` vaut 0,
et pourtant npc-0 choisit `helpFarm` au tick 196, sous le **plancher collectif** de 28. Les planchers et
biais collectifs (`gatherFood` 40 / 14, `helpFarm` 28 / 10, `build` 24 / 20, `gatherWood` 0 / 14) pèsent sur
toute la table.

## FILES_OWNED

- `Source/AnastasisSim/Public/Village/AnastasisVillage.h` : `FCollectiveDecision`, `FDecisionTrace::CollectiveDelta`, `GoalHelpFarm`, `FNpc::DeedsHelped`, `ApplyCollectivePass`, `CollectiveDecisionOf` / `CollectiveDecisionOverride`, `FindTendFieldNear` / `FindTendFieldFor`, `ProgressTendWork`, `HelpFarmRowScore`, `HelpFarmTarget`, `ClaimedFieldPostsExcept`, `EnsureCraftSession(…, CraftId)`
- `Source/AnastasisSim/Private/Village/AnastasisVillage.cpp` : ces fonctions ; `ChooseGoal` (facteur de travail pour tous, ligne `helpFarm`, passe collective) ; `Act` (chemin `tend`) ; `AssignTarget`
- `Source/AnastasisSim/Public/Work/AnastasisGather.h`, `Private/Work/AnastasisGather.cpp` : `SwingPeriodTend`, `FieldSeasonTendAmount`, constantes `Tend*`
- `Source/AnastasisSim/Private/Tests/AnastasisGatherTests.cpp`, `AnastasisGatherVectors.inl` (régénéré), `tools/migration/parity/gather.mjs` (cas `SwingTend`, `SeasonTend`)
- `Source/AnastasisSim/Private/Tests/AnastasisVillageCollectivePassTests.cpp`, `AnastasisVillageHelpFarmTests.cpp` (nouveaux)
- `Source/AnastasisSim/ECARTS.md` (n° 1, 11, 26, 27)
- `tools/migration/trace-npc.mjs` (relevé d'un habitant tick par tick)
- `docs/unreal/handoffs/help-farm-001.md`

## COMMIT

Voir `git log agent/help-farm-001`.

## MEC

- BUILD : `BUILD::PASS`.
- Vecteurs : `node tools/migration/gen-parity.mjs -ref <clone anastasis-ref-p3> gather.mjs` → 12 cas, 2 561 vecteurs.
  Deux cas nouveaux : `SwingTend` (`swingPeriodFor(npc, "tend")`, 144 vecteurs) et `SeasonTend` (`fieldSeasonTendAmount`, 22 vecteurs).
- TESTS : `report-tests.ps1 -Filter Anastasis.Sim` → PASS 123, KNOWN_EXPECTED_FAILURE 2 (`Parite.Fbm`,
  `Parite.SemantiqueJs`), FAIL 0, 125/125 annoncés.
  - `Anastasis.Sim.Village.PasseCollective` (nouveau) : sans planificateur, rien ne bouge ; un plancher
    au-dessus de la table gagne ; un plancher sous la ligne n'ajoute rien ; avec un besoin critique, le plancher
    s'ajoute (+1) ; l'urgence s'additionne (+7,5 sur `drink`) ; la corvée de bois monte `gatherWood`.
  - `Anastasis.Sim.Village.SoinsDeParcelle` (nouveau) : sous le plancher collectif injecté, le fermier choisit
    `helpFarm`, vise un poste de la parcelle faible, ouvre une session `tend` ; la parcelle passe de 10 à 16
    (2 coups de 3, `fieldSeasonTendAmount(2, 1)`), `deeds.helped` = 4.
  - `Anastasis.Sim.Parite.Recolte` : les vecteurs `SwingTend` et `SeasonTend` passent.
  - Tous les `Village.*` existants restent verts, alors que la ligne `helpFarm` vaut désormais son score et
    non plus le plancher 42.
- HARNAIS (`endurance`, 16 200 ticks) : premier tick divergent toujours **32**, avec les mêmes empreintes
  qu'avec act-gate-001. `helpFarm` ne pourra gagner dans le harnais qu'une fois le planificateur branché.

## ECARTS

- ouvert : n° 27 — planificateur collectif non porté, la passe est branchée mais vide (A_FERMER, planner-module-001 puis build-decision-001).
- modifié : n° 1 — la ligne `helpFarm` est calculée, son but porté ; sans parcelle, `farmPos` (accès au marché prévu) arrive avec build-decision-001.
- modifié : n° 11 — le soin de parcelle ne tire pas son `rollCraftMiss(tend)`, à brancher sur la fonction générique de chat-on-haul-001.
- modifié : n° 26 — `helpFarm` passe aussi par `workAtWorkplaceYard`, non porté.
- Marques posées : n° 8 (pas d'adolescent, soin de 2), n° 24 (`planBias` = 0).

## PROOFS

PROOFS: (aucune)

## SCN

`endurance`, inchangé.

## PLY

Sans objet.

## INTEGRATION_RISK

- `ChooseGoal` : le facteur de travail est calculé pour tout adulte (`Trace.WorkFactor` reste NaN hors
  fermier et chantier). La ligne `helpFarm` change pour tous : 0 + biais sans ferme, au lieu de 42.
- Conflits possibles : `chat-on-haul-001` (renfort, `Deliver` et rollCraftMiss) et `build-decision-001`
  (moi, site de marché). Régions différentes d'`AnastasisVillage.cpp`.
- Numéros d'écart : le n° 25 est pris par plusieurs branches en vol (`weather-dry-001`, `mortality-001`,
  `route-cost-001`, `anthropic-wood-001`), et le n° 26 l'est aussi hors de main (`anthropic-wood-001`,
  `geography-concordance-001`, `woodland-sequence-003`) : collisions à renuméroter au versement. Je prends le n° 27.

## STOP

- Le planificateur n'est pas porté : la passe collective est vide, et `helpFarm` ne gagne pas dans le harnais.
- Pas de rate de coup sur le soin (n° 11), pas de présence au poste (n° 26).
