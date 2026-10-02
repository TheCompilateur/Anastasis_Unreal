# HANDOFF: reconsider-001

> **Pour Alexandre : le fermier peut maintenant rentrer livrer avant d'avoir le sac plein (il reconsidère son but comme dans la référence), et sous l'orage il passe par la porte d'abri puis reprend sa cueillette.**

## MISSION

Commandée par « Simulateur IV Kingdoms migration phase 3 », découpage complet approuvé le 2026-10-02
(« le PhaseBias personnel compris ») :
- porter le tirage de reconsidération de `updateNpc` (npc.js l. 893) :
  `if (!npc.target || sim.rng() < chance) chooseGoal(sim, npc)`, avec
  chance = `committedReconsiderChance(phaseReconsiderChance(needsReconsiderChance(npc, thinkDt)))` ;
- la phase PERSONNELLE (`personalFrac` / `villagePhaseFor`) dans `syncVillagePhase`, `chooseGoal`
  (`phaseBias`, `phaseWorkFactor`) et la reconsidération ; `npc.phaseChangedAt` ;
- le quart de travail (`workShift.js`, `opensExtractionShift`) : ouverture au commit, arrivée, bouclier,
  verrou de quart dans `commitGoalChoice`.

Les trois précisions demandées sont tenues : sans mode de vie, les bits de la phase du village (prouvé par
les vecteurs `ReconsiderPersonalPhase` sans mode de vie) ; l'état initial du quart est documenté ci-dessous ;
la valeur « absent » de `phaseChangedAt` est donnée ci-dessous.

## FILES_OWNED

- `Source/AnastasisSim/Public/Life/AnastasisReconsider.h`, `Private/Life/AnastasisReconsider.cpp` (nouveaux)
- `Source/AnastasisSim/Public/Life/AnastasisWorkShift.h`, `Private/Life/AnastasisWorkShift.cpp` (nouveaux)
- `Source/AnastasisSim/Public/Village/AnastasisVillage.h` : `FNpc::PhaseChangedAt`, `FNpc::WorkShift`,
  `FDecisionTrace::bShiftLock`, `FVillage::ReconsiderChanceNow` / `ReconsiderChanceAt` / `PersonalPhaseOf` ;
  note n° 2 de l'en-tête
- `Source/AnastasisSim/Private/Village/AnastasisVillage.cpp` : `UpdateNpc` (synchronisation de phase
  personnelle, tirage l. 893), `ChooseGoal` (phase personnelle, verrou de quart), `CommitGoal`
  (`noteShiftGoalCommit` après la cible), `EnsureCraftSession` / `EnsureBuildSession` (`noteShiftArrival`),
  `RelaxTarget` (phase personnelle)
- `Source/AnastasisSim/Private/Tests/AnastasisReconsiderTests.cpp`, `AnastasisReconsiderPureVectors.inl`,
  `AnastasisReconsiderVectors.inl` (nouveaux)
- `tools/migration/parity/reconsider.mjs`, `tools/migration/trace-reconsider.mjs` (nouveaux) ;
  `tools/migration/rng-trace-lib.mjs` (option `photographier`, sans effet sur les relevés existants)
- `docs/migration/phase3/P3_RECONSIDERATION.md` (nouveau, généré)
- `Source/AnastasisSim/ECARTS.md` (n° 2), `PORTAGE.md`, `tools/migration/ported-functions.mjs`,
  `docs/migration/phase2/P2_INVENTAIRE_JS.md` (régénéré)

Non touchés : `AnastasisVillagePlayer.cpp` (`UpdatePlayer` lit encore la phase du VILLAGE : l'habitant
incarné reste à player-goals), `Harness/*`, `AnastasisJsSave`, les scénarios, `masks.mjs`,
`known-expected-failures.txt`, les `.inl` existants.

## COMMIT

Le commit qui porte cette fiche sur `agent/reconsider-001`.

## POUR LE LECTEUR — `phaseChangedAt` et `workShift`

- `npc.phaseChangedAt` : JS `number` ou absent (`undefined`, jamais écrit avant la première bascule).
  C++ : **`FNpc::PhaseChangedAt`** (`TOptional<double>`). **Absent = `TOptional` vide** : le lecteur ne le
  pose que si la sauvegarde a le champ. `committedReconsiderChance` lit `Number.isFinite(phaseChangedAt)` :
  vide se comporte comme absent.
- `npc.workShift` : **non sérialisé par la référence**. Après `deserialize`, `npc.workShift` est `undefined`
  côté JS ; côté C++ `FNpc::WorkShift.State == EState::None`. Les deux côtés du harnais partent donc du
  même état : aucun quart. Le lecteur n'a rien à lire.
- `npc.villagePhase` devient la phase PERSONNELLE (décalée par le mode de vie) ; déjà lue par le lecteur,
  sémantique inchangée côté JS.

## MEC

- BUILD : `tools\unreal\anastasis-unreal.ps1 build` → `BUILD::PASS`.
- Vecteurs purs : `node tools/migration/gen-parity.mjs -ref <clone anastasis-ref-p3> reconsider.mjs` →
  4 cas, 492 vecteurs (128 `needsReconsiderChance`, 104 phases personnelles, 255 commits de quart, 5 arrivées).
- Tirages mesurés : `node tools/migration/trace-reconsider.mjs -ref <clone>` (scénario `endurance`, un jour) :
  75 tirages l. 893, 38 reconsidérations, **75/75 expliqués** par la chance reconstruite (engagement 31,
  bascule récente 26, quart 7, base 7, critique 4). La branche 0,92 de `phaseReconsiderChance` n'est jamais
  prise : `updateNpc` synchronise la phase en tête de tick, avant la pensée.
- Collant de but (`goalStickinessBonus`, `criticalReliefGoals`) porté dans la mission, avec l'accord de migration
  phase 3 : sans lui, une reconsidération envoyait le fermier livrer à sac 4. 1 944 vecteurs `ReconsiderStickiness`.
- TESTS (`report-tests.ps1 -Filter Anastasis.Sim`) : **PASS 120, KNOWN_EXPECTED_FAILURE 2** (`Parite.Fbm`,
  `Parite.SemantiqueJs`), **FAIL 0**, 122/122. `Village.Reconsideration` : 75 tirages rejoués, 38 reconsidérations, 0 faux.
  `Village.TiragesDecision` : 104 décisions, 0 fausse. `Recolte.Cueillette` : retour à sac 4 en 1,15 s.
- Six tests réécrits en invariants (Recolte.Cueillette / Livraison / Epuisement / Destruction / Plein,
  MeteoHabitants.Orage) : livraison = sac au moment du retour, sac = cueilli, sac + grenier conservés ; orage selon
  l'ordre de `commitGoalChoice` (npc.js l. 1855 `applyGoalStickiness`, porte d'orage l. 2055-2071). Référence :
  dans `endurance`, les deux livraisons du jour font 5 vivres (npc-0 t=125, npc-1 t=141), pas 10.
- MUTATIONS : faites dans reconsider-mutations-001 (une à la fois, suite `Anastasis.Sim` entière) :

| Test réécrit | Ancienne assertion | Nouvel invariant | Mutation | Détectée par (message) |
|---|---|---|---|---|
| `Recolte.Cueillette` | `5 coups : sac 10` | sac = cueilli, 2 ≤ sac ≤ 10 ; **renforcé** : sans pensée (aucune reconsidération possible), retour forcé à sac 10 | M1 — retour forcé à sac > 9 supprimé | `Recolte.Cueillette` : `sans pensee : retour force a sac 10 (5 coups)` to be 10, but it was 44 ; aussi `Village.Endurance` (solitude critique) |
| `Recolte.Livraison` | `grenier 0 -> 10`, `livre` 10, marché 10, `grenier 20` | grenier = livré = sac du retour ; marché = sac ; grenier = tout ce qui a été livré, deuxième voyage > sac | M2 — livrer sac − 1 | `Recolte.Livraison` : `grenier 0 -> le sac` to be 4, but it was 3 ; `sac vide` 0 / 1 ; `livre` 4 / 3 |
| `Recolte.Epuisement` | `sac 5 : sous le seuil`, `grenier 5` | 5 cueillis ; sac + grenier = 5 ; le grenier finit à 5 | M2 — livrer sac − 1 | `Recolte.Epuisement` : `champ vide` to be 0, but it was 1 |
| `Recolte.Plein` | une livraison, `5 livres`, `5 au sac` | grenier plein à 300 ; 5 livrés ; reste au sac = cueilli − 5 | M2 — livrer sac − 1 | `Recolte.Plein` : `plein a 300` 300 / 299 ; `5 livres` 5 / 4 ; `le reste au sac` 5 / 1 |
| `Recolte.Destruction` | `le sac reste plein` = 10 | le sac reste celui d'avant la démolition | M7 — sac vidé à la démolition | `Recolte.Destruction` : `le sac reste plein` to be 4, but it was 0 |
| `MeteoHabitants.Orage` | `shelterRain` gagne la table, pas de porte, pas de but repris ; reprise : observer | deux chemins selon la table (ligne ou porte + `shelterResumeGoal` = gatherFood) ; **renforcé** : la PREMIÈRE décision sous l'orage l'envoie à l'abri ; reprise = but retenu par la porte | M3 — porte d'orage ignorée | `Orage` : `storm: his first decision under the storm drops the harvest for shelter` (goal=gatherFood) |
| `MeteoHabitants.Orage` | (idem) | (idem) | M4 — `shelterResumeGoal` oublié | `Orage` : `and keeps the harvest to resume` to be "gatherFood", but it was "" ; aussi `TempsSec` (`every storm-gate decision keeps an exposed goal to resume`) |
| — (reconsidération) | — | — | M5 — échelle 0,42 du tirage retirée (`CommitReconsiderScale` = 1) | `Village.Reconsideration` : `tick 165 npc-2 (build) : chance 0.396000 / 0.166320` (puis 74 autres) |
| — (collant) | — | — | M6 — collant de but retiré de la table (`Row.Value += Bonus` supprimé) | `Orage` : première décision sous l'orage = `deliver` (goal=deliver), que la porte d'orage exclut ; avec le collant, la cueillette garde la table et la porte l'envoie à l'abri |

Sans mutation (tests renforcés) : `Anastasis.Sim` PASS 120, KNOWN_EXPECTED_FAILURE 2, FAIL 0. Chaque mutation est
posée seule, build, suite `Anastasis.Sim` entière, puis retirée. Deux invariants ne détectaient rien et ont été
renforcés, pas les mutations : `Recolte.Cueillette` (M1 passait : le fermier rentre à sac 4 sur reconsidération
avant que le retour forcé ne joue) et `MeteoHabitants.Orage` (M3 passait : sans porte, il finissait à l'abri plus
tard par la ligne de la table).

## ECARTS

- modifié : n° 2 — le tirage l. 893 est porté (phase personnelle, quart, `committedReconsiderChance`,
  prouvé par `Village.Reconsideration`) ; restent le collant de but (`goalStickinessBonus`) et deux fins
  d'action qui relâchent la cible ; fermeture : suite de goal-noise-001.

Le n° 8 est inchangé : un habitant sans mode de vie lit la phase du village, comme la référence
(`personalFrac` sans `lifestyle.id`).

## PROOFS

PROOFS: village-weather-pie

Preuve par tests d'automation (`Parite.Reconsideration`, `Village.Reconsideration`), rejoués par la suite du lot.

`gather-deliver-pie` exigeait `bag > 9` à l'étape `03-retour` : corrigée et inscrite au registre dans reconsider-mutations-001.

## SCN

Aucune scène, aucun asset.

## PLY

Sans objet. `UpdatePlayer` n'est pas modifié.

## INTEGRATION_RISK

- Le comportement du village change : chaque pensée avec cible tire maintenant dans `sim.rng` et peut
  redécider. Le flux partagé avance de 75 tirages de plus par jour (scénario `endurance`), donc les tirages
  qui suivent (bruits, rumeurs) changent de valeur.
- `AnastasisVillage.h/.cpp` : `FNpc` (deux champs après `GoalSince`), `UpdateNpc`, `ChooseGoal`,
  `CommitGoal`. Fusion probable avec toute mission qui touche la décision.

## STOP

- Ne revendique pas la parité de trajectoire du harnais : le collant de but (n° 2), le rate de coup
  (n° 11) et l'intention du jour (n° 24) manquent encore. Les 75 tirages sont prouvés chacun sur la photo
  mesurée, pas en enchaînement.
- Le verrou de quart et `noteShiftArrival` sont branchés mais ne sont prouvés que par les vecteurs purs et
  par les quarts photographiés aux 75 tirages, pas par une trajectoire de harnais.
