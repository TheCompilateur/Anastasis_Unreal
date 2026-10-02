# HANDOFF: premiere-pensee-001

## MISSION

Porter ce que la première pensée d'un habitant écrit dans la référence. Au tick 32 du harnais `endurance`,
npc-2 décide pour la première fois du scénario : la référence crée alors une série de clés que le C++
n'écrivait pas. Relevé par `tools/migration/trace-first-writes.mjs` (première écriture de chaque clé, tick et
pile) :

| Clé | Écrite par (référence) | C++ |
|---|---|---|
| `goalExplain` | `captureGoalExplain` au commit | `CaptureGoalExplain` (trois premières lignes, cause dominante, phrase) |
| `streetDecision` | `stampStreetDecision` au commit | `StampStreetDecision` (marge, fenêtre 3 / 3,2 / 4,5 / 6,5 s, cause) |
| `buildBinding` | commit (`null` hors `build`), `bindBuildSite` | posé au commit, vidé hors `build` |
| `socialSeekId` | `resolveNpcDestination` à chaque cible | posé en tête d'`AssignTarget`, vidé hors `socialize` / `visitFamily` / `play` |
| `workShift` | `noteShiftGoalCommit` | déjà porté : lu et projeté |
| `hungerAction` | `ensureHungerAction` (première décision Noûs) | marqué à la première décision Noûs : lu et projeté |
| `mind.failures` | `ensureFailures` / `failureStore` (table) | `{ goals, causes, negative }` vides, posés par la table |
| `nocturnalIntent` | `phaseBias` à chaque ligne | posé après la table (nuit hors garde : `isNocturnalWanderer` ; jour : faux) |
| `activitySince` | `setActivity` | toutes les affectations d'activité passent par `SetActivity` |
| `skills.care` | (sauvegarde) | lu : `skillGoalBias` des buts sociaux |

## FILES_OWNED

- `Source/AnastasisSim/Public/Village/AnastasisVillage.h` : `FGoalExplain`, `FGoalExplainEntry`, `FStreetDecision`, champs de `FNpc` (`ActivitySince`, `GoalExplain`, `StreetDecision`, `bHasHungerAction`, `NocturnalIntent`, `bHasBuildBinding`, `bHasSocialSeekId`, `bHasFailureStore`, `SkillCare`), `SetActivity`, `CaptureGoalExplain`, `StampStreetDecision`, `CommitGoal(…, Scores)`
- `Source/AnastasisSim/Private/Village/AnastasisVillage.cpp`, `AnastasisFoodSupply.cpp`, `AnastasisVillagePlayer.cpp` : `SetActivity` partout, le commit, `AssignTarget`, la fin de table, Noûs
- `Source/AnastasisSim/Private/Harness/AnastasisJsSave.cpp` : `ReadFirstThought`, `ProjectFirstThought`, `activitySince`, `skills.care`
- `Source/AnastasisSim/Private/Tests/AnastasisVillageFirstThoughtTests.cpp` (nouveau) : `Village.PremierePensee`
- `Source/AnastasisSim/ECARTS.md` : n° 1 (complété), n° 34, n° 35, n° 36 (nouveaux)
- `tools/migration/trace-first-writes.mjs` (nouveau)
- `docs/unreal/handoffs/premiere-pensee-001.md`

## COMMIT

Voir `git log agent/premiere-pensee-001`. Posée sur `agent/resource-targets-001` (a6b827c), elle-même posée sur planner-wiring-001.

## MEC

- BUILD : `BUILD::PASS`.
- TESTS (`finish -Prove`) : PASS 281, KNOWN_EXPECTED_FAILURE 4, FAIL 0, 285/285 annoncés.
  - `Anastasis.Sim.Village.PremierePensee` (nouveau) : avant toute pensée, aucune clé ; après une décision,
    `goalExplain` (heure, gagnant de la table, trois lignes triées, scores au dixième, une cause chacune, phrase
    `parce que …` ou trois buts courts), `streetDecision` (gagnant, depuis `observer`, bascule, cause de
    l'explication, fenêtre 3 / 4,5 / 6,5 s, marge ≥ 0), `buildBinding` nul hors `build`, `socialSeekId`,
    `mind.failures`, `nocturnalIntent` posés.
  - `Harnais.Lecture` : la sauvegarde porte `mind.failures = {}` (forme ancienne) ; elle n'est pas marquée à la
    lecture (la référence la convertit à la première pensée), le tick 0 se relit au bit près.
- HARNAIS (`endurance`, 16 200 ticks, forage au tick 32 ; base resource-targets-001) :
  - premier tick divergent toujours **32**, désormais `actors` seul (`buildings` au tick 125, comme `rng`) ;
  - au tick 32, npc-2 : **33 → 29 champs**. Concordent désormais : `activitySince`, `workShift`, `hungerAction`,
    `nocturnalIntent`, `socialSeekId`, `buildBinding`, `mind.failures`, la forme de `goalExplain` et de
    `streetDecision` (`at`, `goal`, `top[0].goal`, `cause` « habitude », `line`, `until`, `from`, `changed`, `tight`) ;
  - restent : les **valeurs** de `goalExplain` / `streetDecision` (la ligne `build` vaut 288,7 en C++ contre 223,8 :
    npc-2 est noctambule et le C++ ignore le mode de vie dans la décision, n° 8 ; deuxième et troisième lignes
    `deliver` 164,1 (micro-plan) et `gatherStone` 155,9 (ambition), non portées, n° 1 / 24 ; collant 18 contre 19,4) ;
    `_algoDebug` (n° 35) ; `workTimer` 0 contre 0,5167 (n° 36) ; navigation et pas (n° 4, nav-service-001).
- Relevé : `node tools/migration/trace-first-writes.mjs -ref <clone> -scenario tools/migration/scenarios/endurance.json -ticks 32 -npc npc-2`.

## PROOFS

PROOFS: (aucune)

## SCN

`endurance`, inchangé.

## PLY

Sans objet.

## ECARTS

- ouvert : n° 34 — traces de repos (`maybeStampRestTrace`) non posées par `setActivity` (harnais buildings).
- ouvert : n° 35 — le paquet de débogage Noûs `_algoDebug` n'est pas tenu (harnais actors).
- ouvert : n° 36 — `workTimer` remis à zéro au changement de but ; la référence ne le fait pas (fermeture : worktimer-001, qui refait trois tests de scénario).
- hérité : n° 32 (planner-module-001) et n° 33 (resource-targets-001), ouverts par les missions dont cette branche part, non encore versées ; inchangés ici.
- modifié : n° 1 — `goalExplain` et `streetDecision` sont écrits sur la table du C++ : tant qu'une ligne reste au plancher, son rang, son score et sa cause diffèrent.

## INTEGRATION_RISK

- Dépend de resource-targets-001 et de planner-wiring-001 (non versées) : verser après elles, dans le même lot.
- nav-service-001 (renfort) touche `NextWaypoint`, `MoveActor`, la boucle des habitants et le bloc `actors` du
  lecteur. Ici : `SetActivity` partout (32 affectations converties), `CommitGoal` (table en paramètre),
  `AssignTarget` (en tête), lecteur et projection des habitants (fonctions séparées `ReadFirstThought` /
  `ProjectFirstThought`). Conflits textuels possibles dans le lecteur ; à rebaser dans l'ordre du lot.
- Comportement : aucun but, aucune cible ne change (les clés ajoutées ne sont lues que par la projection),
  sauf `socialSeekId`, désormais vidé à chaque cible hors socialisation, comme la référence.

## STOP

- Ne revendique pas la concordance de `goalExplain` / `streetDecision` avec la référence : elle attend la table
  complète (micro-plan, ambition, intention du jour : n° 24 ; mode de vie dans la décision : n° 8).
- Ne retire pas la remise à zéro de `workTimer` (n° 36).
