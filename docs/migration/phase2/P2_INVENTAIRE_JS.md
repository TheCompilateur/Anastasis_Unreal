# Inventaire du simulateur JS — porter / generer / jeter

**Genere. Ne pas editer a la main.**

```bash
# -ref : checkout propre du tag de reference, voir docs/migration/phase3/REFERENCE_JS.md
node tools/migration/inventory-js-sim.mjs -ref <depot JS> -out docs/migration/phase2/P2_INVENTAIRE_JS.md
```

Reference : tag `anastasis-ref-p3` — commit `fee66ae` (2026-08-30)  
Perimetre : `src/sim/`, `src/life/`, `src/ai/`, `src/lang/`, `src/runtime/` — le rendu, l'UI, l'audio et le debug sont hors sujet par decision (AGENTS.md).

Le portage ne se mesure pas en lignes de JS. Une table de contenu devient une table de
donnees, pas du C++ ecrit a la main ; un filet de securite navigateur se re-decide dans
Unreal, il ne se traduit pas. Ce document dit, fichier par fichier, dans quel seau il tombe.

## Ce qu'il y a devant

| | fichiers | lignes | dont code | dont code a porter |
| --- | ---: | ---: | ---: | ---: |
| **A porter** | 160 | 50855 | 39819 | 39819 |
| **Partiellement porte** | 34 | 28454 | 23539 | 17904 |
| **A generer** (donnees) | 4 | 2101 | 1278 | 0 |
| **A jeter** | 23 | 4968 | 3847 | 0 |
| Deja porte | 12 | 2687 | 1939 | 0 |
| **Total** | 233 | 89065 | 70422 | 57723 |

Sur les 50855 lignes des modules a porter, 5559 sont du commentaire et
3755 des lignes vides : **39819 lignes de code**. Les 34 modules
partiellement portes ajoutent **17904 lignes de code** qui restent (sur 23539 ;
5635 portees, 0 hors perimetre). Total a porter : **57723 lignes de code**.
12 modules a porter melangent logique et table de contenu : la table s'extrait, le selecteur se porte.

## Reste a porter, par vague

L'ordre est celui de `Source/AnastasisSim/PORTAGE.md` — il suit les dependances reelles,
pas l'interet du gameplay. Un module partiellement porte compte pour ce qui lui reste.

| Vague | fichiers | dont partiels | lignes de code a porter |
| --- | ---: | ---: | ---: |
| 0 — socle deterministe | 1 | 1 | 24 |
| 1 — generation du monde | 2 | 2 | 663 |
| 2 — navigation | 8 | 2 | 639 |
| 3 — budget et LOD logique | 1 | 0 | 228 |
| 4 — etat du monde et sauvegarde | 3 | 2 | 701 |
| 5 — boucle de simulation | 79 | 9 | 33194 |
| 6 — vie, IA, langue | 100 | 18 | 22274 |

Les vagues 5 et 6 ne sont pas des vagues, ce sont des marecages : 179 modules a
elles deux. Elles se decoupent en chantiers, et c'est a ce grain qu'un module se confie.

| Vague | Chantier | fichiers | lignes de code a porter | plus gros reste |
| --- | --- | ---: | ---: | --- |
| 0 | socle | 1 | 24 | `sim/spatialGrid.js` (24, partiel) |
| 1 | generation du monde | 2 | 663 | `sim/worldArchetypes.js` (595, partiel) |
| 2 | navigation | 8 | 639 | `sim/crowdNav.js` (228) |
| 3 | budget et LOD | 1 | 228 | `sim/logicalLod.js` (228) |
| 4 | etat et sauvegarde | 3 | 701 | `sim/save.js` (609, partiel) |
| 5 | noyau de boucle | 8 | 12414 | `sim/simulation.js` (6647, partiel) |
| 5 | societe et institutions | 8 | 6629 | `sim/collectivePriorities.js` (3035) |
| 5 | urbanisme | 14 | 3954 | `sim/urban/intent.js` (489) |
| 5 | economie et travail | 18 | 3373 | `sim/craftWork.js` (854, partiel) |
| 5 | transport et logistique | 13 | 3310 | `sim/transport/delivery.js` (770) |
| 5 | chronique et memoire collective | 11 | 2483 | `sim/villageChronicle.js` (414) |
| 5 | regne animal | 7 | 1031 | `sim/animaux/updateAnimals.js` (465) |
| 6 | personne et famille | 32 | 6286 | `life/lifeScenes.js` (818) |
| 6 | cognition | 30 | 5549 | `ai/memory.js` (717, partiel) |
| 6 | parole et narration | 15 | 4074 | `life/talk.js` (1464, partiel) |
| 6 | rites et culture | 14 | 4072 | `life/kosmos1204UneBouchePlus.js` (640) |
| 6 | langue | 9 | 2293 | `lang/lexicon.js` (646) |

## Partiellement porte

Modules dont PORTAGE.md declare une partie portee (`tools/migration/ported-functions.mjs`).
`code` = lignes de code du module ; `porte` = lignes des fonctions portees ; `hors` = fonctions
ecartees du portage par PORTAGE.md (observation, three.js) ; `reste` = fonctions non portees et fonctions
**reduites** (une branche portee sur plusieurs). Le code hors fonction (imports, constantes, tables) est
reparti entre `porte` et `reste` au prorata du code de fonctions.

| Module | C++ | fonctions portees | reduites | code | porte | hors | reste | source PORTAGE.md |
| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| `sim/simulation.js` | Village/AnastasisVillage, Sim/AnastasisSimulation, Work/AnastasisFields, Work/AnastasisBuild, Life/AnastasisWeatherBehavior | 24 / 388 | 6 | 7196 | 549 | 0 | **6647** | couche 2 ; puits ; maison ; gather-deliver ; field-regrow ; social-relax ; bonds-rumors ; village-weather ; build-001 |
| `sim/npc.js` | Village/AnastasisVillage, Work/AnastasisGather, Life/AnastasisWeatherBehavior | 21 / 181 | 10 | 5169 | 662 | 0 | **4507** | puits ; maison ; grenier ; gather-deliver ; social-relax ; bonds-rumors ; village-weather ; build-001 |
| `life/talk.js` | Life/AnastasisBonds, Village/AnastasisVillage | 15 / 92 | 0 | 1887 | 423 | 0 | **1464** | bonds-rumors (hash, portes, durees, tours, refus) |
| `sim/craftWork.js` | Work/AnastasisGather, Work/AnastasisBuild | 4 / 51 | 0 | 964 | 110 | 0 | **854** | gather-deliver ; build-001 (profil build) |
| `ai/memory.js` | Village/AnastasisVillage, Work/AnastasisGather | 8 / 53 | 0 | 873 | 156 | 0 | **717** | grenier ; gather-deliver ; bonds-rumors (n° 14) |
| `sim/save.js` | Harness/AnastasisJsSave | 1 / 11 | 4 | 656 | 47 | 0 | **609** | sim-state-reader-001 (lecteur du harnais) |
| `sim/worldArchetypes.js` | World/AnastasisWorldArchetype | 2 / 4 | 1 | 752 | 157 | 0 | **595** | couche 1 (knobs sim seuls) |
| `life/speechActs.js` | Village/AnastasisVillage | 3 / 33 | 0 | 553 | 50 | 0 | **503** | bonds-rumors |
| `sim/transport/stockLedger.js` | Village/AnastasisVillage | 3 / 30 | 0 | 395 | 40 | 0 | **355** | grenier (reserver / rendre / prelever) |
| `life/domestic.js` | Life/AnastasisNeeds, Village/AnastasisVillage | 9 / 31 | 0 | 361 | 102 | 0 | **259** | maison ; social-relax |
| `ai/socialMemory.js` | Life/AnastasisBonds | 13 / 23 | 0 | 407 | 230 | 0 | **177** | bonds-rumors |
| `life/needs.js` | Life/AnastasisNeeds (+ Work/AnastasisGather) | 19 / 29 | 0 | 466 | 314 | 0 | **152** | puits ; maison ; grenier ; gather-deliver ; social-relax ; needs-factors-001 |
| `life/villageRhythm.js` | Life/AnastasisVillageRhythm, Village/AnastasisVillage | 8 / 20 | 0 | 343 | 230 | 0 | **113** | maison ; puits ; grenier ; social-relax |
| `life/bonds.js` | Life/AnastasisBonds | 10 / 22 | 0 | 328 | 231 | 0 | **97** | bonds-rumors |
| `life/moodlets.js` | Life/AnastasisBonds | 4 / 11 | 0 | 220 | 123 | 0 | **97** | bonds-rumors (newFriend) |
| `sim/weather.js` | World/AnastasisWeather | 11 / 16 | 0 | 268 | 172 | 0 | **96** | env-realism-001 |
| `ai/algorithmic/mealReservation.js` | Village/AnastasisVillage | 10 / 17 | 0 | 377 | 297 | 0 | **80** | grenier (lignes 1-377) |
| `life/skills.js` | Work/AnastasisGather | 3 / 8 | 0 | 122 | 44 | 0 | **78** | gather-deliver |
| `sim/navGrid.js` | World/AnastasisNavGrid, World/AnastasisNavService | 13 / 19 | 2 | 364 | 289 | 0 | **75** | couche 2 (couche terrain) ; puits (seuils) ; nav-service-001 (metriques, anneau de trace, cle de cible) |
| `sim/fieldCrops.js` | World/AnastasisWorld, Work/AnastasisFields, Work/AnastasisGather | 8 / 14 | 0 | 120 | 52 | 0 | **68** | couche 1 ; gather-deliver-001 ; field-regrow-001 ; village-weather-001 |
| `sim/craftToolSwitch.js` | Work/AnastasisBuild, Village/AnastasisVillage | 1 / 6 | 0 | 65 | 10 | 0 | **55** | build-001 |
| `ai/moralPressure.js` | Work/AnastasisGather | 1 / 4 | 0 | 116 | 70 | 0 | **46** | gather-deliver ; social-relax |
| `ai/algorithmic/runtime.js` | Village/AnastasisVillage | 4 / 9 | 0 | 224 | 183 | 0 | **41** | grenier |
| `sim/constructionPieces.js` | Work/AnastasisBuild | 1 / 5 | 0 | 62 | 23 | 0 | **39** | build-001 |
| `ai/algorithmic/scheduler.js` | Ai/AnastasisNous | 1 / 5 | 0 | 51 | 15 | 0 | **36** | grenier |
| `life/genome.js` | Life/AnastasisGenome | 15 / 18 | 0 | 173 | 142 | 0 | **31** | needs-factors-001 |
| `ai/weatherGoalBias.js` | Life/AnastasisWeatherBehavior | 6 / 8 | 0 | 166 | 137 | 0 | **29** | village-weather-001 |
| `ai/algorithmic/hungerAction.js` | Village/AnastasisVillage | 3 / 5 | 0 | 205 | 180 | 0 | **25** | grenier |
| `sim/spatialGrid.js` | Core/AnastasisSpatialGrid | 6 / 12 | 0 | 100 | 76 | 0 | **24** | couche 0 |
| `ai/algorithmic/bridge.js` | Village/AnastasisVillage | 3 / 5 | 0 | 191 | 179 | 0 | **12** | grenier |
| `sim/pristineWorld.js` | Harness/AnastasisJsSave (GenerateWorld) | 2 / 4 | 0 | 39 | 30 | 0 | **9** | sim-state-reader-001 |
| `sim/pathfinding.js` | World/AnastasisPathfinding | 13 / 15 | 0 | 179 | 172 | 0 | **7** | couche 2 |
| `sim/craftFatigue.js` | Work/AnastasisGather | 2 / 3 | 0 | 43 | 39 | 0 | **4** | gather-deliver |
| `sim/fieldWorkPosts.js` | Work/AnastasisGather, Village/AnastasisVillage | 6 / 7 | 0 | 104 | 101 | 0 | **3** | gather-deliver |

### Detail par module

**`ai/algorithmic/bridge.js`** — Village/AnastasisVillage. Reste 12 lignes de code sur 191 (dont 1 des 20 lignes hors fonction, au prorata).

- portees (3) : `bump`, `applyAlgorithmicScoreBias`, `applyAlgorithmicCommitGate`
- restent (2 fonctions) : `bridgeInertiaCheck` (8), `previousIsWork` (3)

**`ai/algorithmic/hungerAction.js`** — Village/AnastasisVillage. Reste 25 lignes de code sur 205 (dont 3 des 24 lignes hors fonction, au prorata).

- portees (3) : `cancelHungerAction`, `failHungerAction`, `runHungerActionStep`
- restent (2 fonctions) : `ensureHungerAction` (17), `stampHungerAction` (5)

**`ai/algorithmic/mealReservation.js`** — Village/AnastasisVillage. Reste 80 lignes de code sur 377 (dont 4 des 20 lignes hors fonction, au prorata).

- portees (10) : `computeMealTtlSeconds`, `reserveMeal`, `getMealReservation`, `releaseMeal`, `confirmMeal`, `maybeRenewMealReservation`, `expireMealReservations`, `mealSourceAccessPoint`, `isNpcAtMealSource`, `serializeMealLedger`
- restent (7 fonctions) : `restoreMealLedger` (32), `ensureMealLedger` (11), `resolveBuilding` (10), `availableMealUnits` (8), `nextReservationId` (5), `findMealSourceBuilding` (5), `activeMealReservationCount` (5)

**`ai/algorithmic/runtime.js`** — Village/AnastasisVillage. Reste 41 lignes de code sur 224 (dont 6 des 34 lignes hors fonction, au prorata).

- portees (4) : `onAlgorithmicGoalCommitted`, `tryAlgorithmicEat`, `computeAlgorithmicDecision`, `tickAlgorithmicNpc`
- restent (5 fonctions) : `algorithmicShouldThink` (10), `cooldownExcludeTypes` (9), `stampDebug` (8), `algorithmicThinkInterval` (4), `algorithmicMealNavigationTarget` (4)

**`ai/algorithmic/scheduler.js`** — Ai/AnastasisNous. Reste 36 lignes de code sur 51 (dont 6 des 9 lignes hors fonction, au prorata).

- portees (1) : `decisionIntervalSeconds`
- restent (4 fonctions) : `deterministicStagger` (9), `nextDecisionAt` (9), `shouldRunDecision` (6), `scheduleNextDecision` (6)

**`ai/memory.js`** — Village/AnastasisVillage, Work/AnastasisGather. Reste 717 lignes de code sur 873 (dont 45 des 55 lignes hors fonction, au prorata).

- portees (8) : `perceive`, `forgetEmptied`, `rememberSpot`, `trimMemory`, `forgetStale`, `recallResource`, `presumedNoise`, `believedStock`
- restent (45 fonctions) : `repairMindReferences` (88), `stampMarketBelief` (52), `perceiveBeliefs` (51), `seedFoundingCampStockBeliefs` (39), `shareRumors` (34), `createMind` (27), `scrubBeliefMap` (27), `rememberBeliefEntry` (24), … et 37 autres

**`ai/moralPressure.js`** — Work/AnastasisGather. Reste 46 lignes de code sur 116 (dont 11 des 27 lignes hors fonction, au prorata).

- portees (1) : `moralPressure`
- restent (3 fonctions) : `snapshotMoralPressure` (20), `mourningRoleOf` (8), `colonySeasonOf` (7)

**`ai/socialMemory.js`** — Life/AnastasisBonds. Reste 177 lignes de code sur 407 (dont 21 des 49 lignes hors fonction, au prorata).

- portees (13) : `trustOf`, `personTag`, `notePerson`, `promoteTag`, `trimPeople`, `forgetStalePeople`, `noteMeeting`, `socialMemoryBias`, `socialCompanionBonus`, `isWorkingGoal`, `pickRememberedSeek`, `rememberedSocialTarget`, `socialSeekBias`
- restent (10 fonctions) : `noteSocialFromEpisode` (33), `tellPerson` (29), `socialMemoryLabel` (25), `pickRememberedRival` (22), `noteConfrontation` (12), `noteFoodShare` (11), `socialSeekLabel` (10), `ensurePeopleMind` (5), … et 2 autres

**`ai/weatherGoalBias.js`** — Life/AnastasisWeatherBehavior. Reste 29 lignes de code sur 166 (dont 7 des 42 lignes hors fonction, au prorata).

- portees (6) : `readSimWeather`, `shouldSeekRainShelter`, `shelterRainScore`, `weatherGoalBias`, `weatherGoalBiasFromState`, `isOutdoorWorker`
- restent (2 fonctions) : `weatherGoalLabel` (19), `isHeavyRain` (3)

**`life/bonds.js`** — Life/AnastasisBonds. Reste 97 lignes de code sur 328 (dont 14 des 48 lignes hors fonction, au prorata).

- portees (10) : `relationOf`, `isSociallyAvailable`, `companionAffinity`, `pickSocialCompanion`, `rendezvousPoint`, `bondSocialTarget`, `bondTalkGain`, `bondStageFromRelation`, `noteBondStageCross`, `bumpRelation`
- restent (12 fonctions) : `bondPlayTarget` (36), `isFamilyWith` (8), `isBondedWith` (6), `bondStageOf` (6), `bondStageWithRelation` (6), `isFriendWith` (3), `isYoungNpc` (3), `isPlaying` (3), … et 4 autres

**`life/domestic.js`** — Life/AnastasisNeeds, Village/AnastasisVillage. Reste 259 lignes de code sur 361 (dont 20 des 28 lignes hors fonction, au prorata).

- portees (9) : `distToBuilding`, `nearLivingHome`, `isEnterableHousing`, `domesticTarget`, `findOpenShelter`, `shelterCapacity`, `countShelterOccupants`, `assignSheltersDaily`, `sleepQuality`
- restent (22 fonctions) : `tryHostGuest` (77), `householdMembersUnderRoof` (33), `reconcileHospitality` (17), `hospitalityState` (14), `ensureHouseTableSlot` (13), `closeHospitalityRelation` (12), `buildingIdHash` (9), `endHospitality` (9), … et 14 autres

**`life/genome.js`** — Life/AnastasisGenome. Reste 31 lignes de code sur 173 (dont 4 des 21 lignes hors fonction, au prorata).

- portees (15) : `hashString`, `deriveGenomeSeed`, `createGenome`, `mutateAllele`, `recombineGenome`, `derivePhenotype`, `clamp01`, `hydrationLossMultiplierFromRetention`, `heatDissipationEfficiencyFromRetention`, `metabolicDemandMultiplierFromEfficiency`, `metabolicPeakRecoveryMultiplierFromEfficiency`, `fatigueRecoveryMultiplierFromRecovery`, `fatigueRecoveryStrainCostFromRecovery`, `ensureGenome`, `genomeFingerprint`
- restent (3 fonctions) : `serializeGenome` (16), `phenotypeSummary` (7), `deserializeGenome` (4)

**`life/moodlets.js`** — Life/AnastasisBonds. Reste 97 lignes de code sur 220 (dont 44 des 100 lignes hors fonction, au prorata).

- portees (4) : `pruneMoodlets`, `stampMoodlet`, `moodletGoalBias`, `tickMoodlets`
- restent (7 fonctions) : `primaryMoodlet` (14), `stampMoodletOnActors` (13), `moodletVignetteLine` (8), `ensureMoodlets` (5), `hasMoodlet` (5), `moodletProfile` (4), `moodletLabel` (4)

**`life/needs.js`** — Life/AnastasisNeeds (+ Work/AnastasisGather). Reste 152 lignes de code sur 466 (dont 29 des 89 lignes hors fonction, au prorata).

- portees (19) : `atDrinkSpot`, `hydrationLossFactor`, `metabolicDemandFactor`, `fatigueRecoveryFactor`, `fatigueAdaptationFactor`, `recoveryConditioningFactor`, `tickNeeds`, `tickVitality`, `urgeScore`, `needGoalScores`, `risingWillTier`, `workWillFactor`, `needsCritical`, `restActivity`, `satisfyDrink`, `satisfyEat`, `satisfyRest`, `satisfySocial`, `satisfyRelax`
- restent (10 fonctions) : `workNeedFactor` (23), `ensureNeeds` (19), `streetUrgency` (19), `dominantNeedLabel` (15), `satisfyRelieve` (14), `worstWillTier` (13), `indoorNeedDuration` (9), `needsReconsiderChance` (5), … et 2 autres

**`life/skills.js`** — Work/AnastasisGather. Reste 78 lignes de code sur 122 (dont 22 des 34 lignes hors fonction, au prorata).

- portees (3) : `createSkills`, `gainDomainSkill`, `skillGoalBias`
- restent (5 fonctions) : `ensureSkills` (24), `skillsLabel` (15), `domainForGoal` (6), `skillTierWord` (6), `skillOf` (5)

**`life/speechActs.js`** — Village/AnastasisVillage. Reste 503 lignes de code sur 553 (dont 11 des 12 lignes hors fonction, au prorata).

- portees (3) : `createInformResourceSpotActs`, `commitHearsayResourceSpot`, `clamp01`
- restent (30 fonctions) : `createGossipEpisode` (51), `commitSpeechEffects` (44), `interpretSpeechAct` (43), `createResourceClaimGossipEpisode` (39), `createInformResourceSpot` (31), `createInformBlockedTarget` (29), `createInformHouseholdPractice` (28), `noteReceivedEpisodeSignal` (26), … et 22 autres

**`life/talk.js`** — Life/AnastasisBonds, Village/AnastasisVillage. Reste 1464 lignes de code sur 1887 (dont 110 des 142 lignes hors fonction, au prorata).

- portees (15) : `isTalkUrgent`, `isTalkWorkBusy`, `isTalking`, `clearTalkSession`, `talkFatigueLevel`, `canStartTalk`, `shouldSpeakNow`, `beginTalkSession`, `advanceTalkTurn`, `bondKindBetween`, `pick`, `villageEmitCount`, `chance`, `hashTalk`, `recordTalk`
- restent (77 fonctions) : `pickTalkTopic` (222), `replyUtteranceFor` (114), `chainedUtteranceFor` (78), `talkUtteranceFor` (57), `speakWorthFor` (46), `rememberTalkHistory` (43), `talkHistoryLines` (29), `holdTalkSession` (28), … et 69 autres

**`life/villageRhythm.js`** — Life/AnastasisVillageRhythm, Village/AnastasisVillage. Reste 113 lignes de code sur 343 (dont 18 des 55 lignes hors fonction, au prorata).

- portees (8) : `dayFracOf`, `villagePhase`, `isNightPhase`, `phaseBias`, `rhythmTarget`, `nearestWell`, `nearestHousing`, `mealPlace`
- restent (12 fonctions) : `villageRhythmStats` (24), `isNocturnalWanderer` (13), `phaseWorkFactor` (11), `syncVillagePhase` (10), `phaseReconsiderChance` (8), `personalFrac` (7), `civilHourOf` (4), `isWorkPhase` (4), … et 4 autres

**`sim/constructionPieces.js`** — Work/AnastasisBuild. Reste 39 lignes de code sur 62 (dont 16 des 25 lignes hors fonction, au prorata).

- portees (1) : `placeConstructionPieces`
- restent (4 fonctions) : `constructionPiecesPlaced` (9), `constructionPieceSettling` (5), `nextConstructionPiece` (5), `placedConstructionPieces` (4)

**`sim/craftFatigue.js`** — Work/AnastasisGather. Reste 4 lignes de code sur 43 (dont 1 des 11 lignes hors fonction, au prorata).

- portees (2) : `craftFatigueOf`, `craftFatiguePeriodMul`
- restent (1 fonctions) : `craftFatigueMissMul` (3)

**`sim/craftToolSwitch.js`** — Work/AnastasisBuild, Village/AnastasisVillage. Reste 55 lignes de code sur 65 (dont 17 des 20 lignes hors fonction, au prorata).

- portees (1) : `craftToolSwitchSeconds`
- restent (5 fonctions) : `craftToolSwitchProgress` (24), `craftArriveStartAt` (5), `pairKey` (3), `craftToolSecondaryFocus` (3), `isCraftToolSwitching` (3)

**`sim/craftWork.js`** — Work/AnastasisGather, Work/AnastasisBuild. Reste 854 lignes de code sur 964 (dont 359 des 405 lignes hors fonction, au prorata).

- portees (4) : `clearWorkSession`, `swingPeriodFor`, `yieldPerSwing`, `ensureCraftSession`
- restent (47 fonctions) : `applyWorkshopBatch` (78), `craftSwingPhase` (38), `workshopCanRun` (33), `creditDepotYard` (28), `applySawBatch` (24), `craftSwingProgress` (20), `craftIdForNpc` (19), `packWorkSession` (15), … et 39 autres

**`sim/fieldCrops.js`** — World/AnastasisWorld, Work/AnastasisFields, Work/AnastasisGather. Reste 68 lignes de code sur 120 (dont 10 des 17 lignes hors fonction, au prorata).

- portees (8) : `hash2d`, `fieldSeasonFromDay`, `fieldSeasonRegenAmount`, `fieldSeasonGatherAmount`, `pickFieldCropId`, `rotateFieldCropId`, `ensureFieldFallow`, `ensureFieldCropReady`
- restent (6 fonctions) : `majorityFieldCropId` (22), `fieldSeasonReadOf` (20), `fieldSeasonYieldOf` (6), `fieldSeasonTendAmount` (4), `isFieldCropId` (3), `isFieldFoodCropId` (3)

**`sim/fieldWorkPosts.js`** — Work/AnastasisGather, Village/AnastasisVillage. Reste 3 lignes de code sur 104.

- portees (6) : `hashText`, `preferredFieldPostIndex`, `fieldPostWorld`, `nearestFieldPostIndex`, `claimedFieldPosts`, `fieldWorkTarget`
- restent (1 fonctions) : `countFieldWorkers` (3)

**`sim/navGrid.js`** — World/AnastasisNavGrid, World/AnastasisNavService. Reste 75 lignes de code sur 364 (dont 7 des 34 lignes hors fonction, au prorata).

- portees (13) : `isStandingTreeTile`, `footBlockedAt`, `createNavMetrics`, `recordNavTransition`, `navTraceSnapshot`, `terrainMoveCostOf`, `rebuildMoveCosts`, `moveCostAt`, `navigationTargetKey`, `isFreeCell`, `computeBuildingAccessPoints`, `ensureBuildingAccessPoints`, `pickBuildingAccessPoint`
- reduites (2) : `ensureNavigation`, `syncNavigationFromActor`
- restent (4 fonctions) : `buildingForAccessTarget` (17), `bumpNavVersion` (11), `towardSettlementDir` (10), `syncActorFromNavigation` (4)

**`sim/npc.js`** — Village/AnastasisVillage, Work/AnastasisGather, Life/AnastasisWeatherBehavior. Reste 4507 lignes de code sur 5169 (dont 385 des 442 lignes hors fonction, au prorata).

- portees (21) : `traitGoalBias`, `applyGoalEligibility`, `holdTalkAct`, `mealPathBlocked`, `survivalWorkFactor`, `applyRainExposure`, `shelterRainDuration`, `shelterRainAccess`, `performShelterRain`, `completionBias`, `jobPriority`, `reachedMoveTarget`, `updateInside`, `tryEnterIndoorAction`, `redirectDomesticDoorFailure`, `socialize`, `shouldHaulGatherLoad`, `progressCraftGather`, `beginHaulToDepot`, `progressBuildWork`, `pickBuildSite`
- reduites (10) : `updateNpc`, `adultScores`, `commitGoalChoice`, `resourceScore`, `deliveryScore`, `workplaceGoalBias`, `assignTarget`, `act`, `perform`, `deliver`
- restent (150 fonctions) : `createNpc` (178), `tryOpenNewConstruction` (111), `begForFood` (73), `fetchInput` (73), `buy` (66), `progressWorkshopCraft` (65), `progressTendWork` (55), `collectiveUrgencyBiasMap` (52), … et 142 autres

**`sim/pathfinding.js`** — World/AnastasisPathfinding. Reste 7 lignes de code sur 179 (dont 1 des 18 lignes hors fonction, au prorata).

- portees (13) : `findPath`, `reconstructPath`, `heuristic`, `isBlocked`, `traversalCost`, `inBounds`, `compareNodes`, `constructor`, `size`, `push`, `pop`, `bubbleUp`, `sinkDown`
- restent (2 fonctions) : `cutsBlockedCorner` (3), `cellKey` (3)

**`sim/pristineWorld.js`** — Harness/AnastasisJsSave (GenerateWorld). Reste 9 lignes de code sur 39.

- portees (2) : `storePristineRef`, `pristineReference`
- restent (2 fonctions) : `warmPristineFromSim` (6), `clearPristineWorldCache` (3)

**`sim/save.js`** — Harness/AnastasisJsSave. Reste 609 lignes de code sur 656 (dont 35 des 38 lignes hors fonction, au prorata).

- portees (1) : `tileDiff`
- reduites (4) : `serialize`, `deserialize`, `applyTileDiff`, `unpackActor`
- restent (6 fonctions) : `repairReferences` (68), `packActor` (12), `unpackTraffic` (8), `packTraffic` (7), `normalizeMarketStock` (5), `rehydrateJob` (5)

**`sim/simulation.js`** — Village/AnastasisVillage, Sim/AnastasisSimulation, Work/AnastasisFields, Work/AnastasisBuild, Life/AnastasisWeatherBehavior. Reste 6647 lignes de code sur 7196 (dont 591 des 640 lignes hors fonction, au prorata).

- portees (24) : `socialPos`, `buildingNearActor`, `buildingForIndoorAction`, `workplaceAcceptsIndoorGoal`, `enterBuilding`, `exitBuilding`, `costMultiplier`, `buildCost`, `blockedAt`, `footBlockedAt`, `tileTraversalCost`, `depleteTile`, `regrowFieldTile`, `regrowFieldsDaily`, `countBuildings`, `siteCanPlacePiece`, `consumeSiteMaterials`, `constructionAccessPoint`, `workConstruction`, `addBuilding`, `drinkAccessPoint`, `accessPointNear`, `localOccupancy`, `resourceTileNear`
- reduites (6) : `movementSpeedFactor`, `tick`, `enqueueDayDeferred`, `assignHomeToHousehold`, `moveActor`, `nextWaypoint`
- restent (358 fonctions) : `resolveDailyBuildingProduction` (133), `urbanSpotScore` (115), `resetWorldBase` (97), `onNewDay` (91), `populateFoundingLife` (78), `ensureFarmFieldParcel` (78), `seedShoreTerminus` (78), `stampClearingFieldClusters` (75), … et 350 autres

**`sim/spatialGrid.js`** — Core/AnastasisSpatialGrid. Reste 24 lignes de code sur 100 (dont 1 des 4 lignes hors fonction, au prorata).

- portees (6) : `packSpatialCellKey`, `recycleGridCells`, `acquireBucket`, `buildActorGrid`, `forEachNear`, `rebuildActorSpatialIndex`
- restent (6 fonctions) : `rebuildAnimalSpatialIndex` (12), `actorSpatialIndex` (4), `animalSpatialIndex` (4), `_NO_SKIP` (1), `_SELF_POINT` (1), `_SKIP_DEAD_ANIMAL` (1)

**`sim/transport/stockLedger.js`** — Village/AnastasisVillage. Reste 355 lignes de code sur 395 (dont 61 des 68 lignes hors fonction, au prorata).

- portees (3) : `reserveStock`, `releaseStock`, `takeReserved`
- restent (27 fonctions) : `migrateYardsToStock` (28), `ensureBuildingStock` (21), `creditColonyStock` (19), `rebuildMarketAggregate` (17), `salvageCompletedSiteStock` (16), `findDepotsForResource` (15), `distributeMarketToDepots` (15), `syncYardFromStock` (14), … et 19 autres

**`sim/weather.js`** — World/AnastasisWeather. Reste 96 lignes de code sur 268 (dont 7 des 20 lignes hors fonction, au prorata).

- portees (11) : `seasonClimateBias`, `winterSnowAt`, `winterFrostAt`, `hash`, `clamp01`, `smoothstep`, `coverLobeAt`, `sampleCoverFront`, `weatherAt`, `weatherWetnessAt`, `weatherHumidityAt`
- restent (5 fonctions) : `sampleWindField` (24), `readWindFromView` (23), `calmWindField` (20), `packWindView` (13), `windGustAt` (9)

**`sim/worldArchetypes.js`** — World/AnastasisWorldArchetype. Reste 595 lignes de code sur 752 (dont 557 des 704 lignes hors fonction, au prorata).

- portees (2) : `pickWorldArchetypeId`, `archetypeJitter`
- reduites (1) : `resolveWorldArchetype`
- restent (1 fonctions) : `settlementDisplayName` (4)

### Deja porte, controle

Classes *porte* parce que chaque fonction y est portee — nommee par PORTAGE.md ou retrouvee dans le
C++ — ou ecartee par PORTAGE.md :

| Module | C++ | fonctions | hors perimetre | source |
| --- | --- | ---: | --- | --- |
| `ai/algorithmic/decision.js` | Ai/AnastasisNous | 3 | — | grenier |
| `ai/algorithmic/hungerUtility.js` | Ai/AnastasisNous | 6 | — | grenier |
| `ai/algorithmic/inertia.js` | Ai/AnastasisNous | 2 | — | grenier |
| `life/conditioning.js` | Life/AnastasisConditioning | 4 | — | needs-factors-001 |
| `runtime/simClock.js` | Core/AnastasisSimClock | 4 | `simSpeedRenderGate3d`, `applySimSpeedRenderGate3d` | couche 0 |
| `sim/hydrology.js` | World/AnastasisHydrology | 13 | — | couche 1 |
| `sim/lifestyle.js` | Life/AnastasisLifestyle | 10 | `lifestyleLabel`, `lifestyleColor` | lifestyle-001 (module seul) |
| `sim/navService.js` | World/AnastasisNavService | 22 | — | couche 2 (nav-service-001, module seul) |
| `sim/rng.js` | Core/AnastasisRng | 4 | — | couche 0 |
| `sim/simulationBudget.js` | Core/AnastasisSimBudget | 9 | `ema`, `resetSimulationBudgetStats`, `noteSimulationBudgetFrame`, `formatSimulationBudgetHud` | couche 3 (noyau causal) |
| `sim/util.js` | Core/AnastasisSimMath | 9 | — | couche 0 |
| `sim/world.js` | World/AnastasisWorld (+ WorldNoise) | 15 | — | couche 1 |

Cites par PORTAGE.md, non comptes en fonctions :

- `sim/content.js` — table TRAITS (donnee) — gather-deliver
- `sim/metiers/catalog.js` — metiers farmer / settler (donnee, module a generer) — gather-deliver ; tables metiers — build-001
- `runtime/gameLoop.js` — frameDeltaSeconds — couche 0 ; le module est jete (requestAnimationFrame)

## Les regles

Elles s'appliquent dans cet ordre, la premiere qui match gagne.

1. **Porte / partiellement porte** — declare dans `ported-functions.mjs`, recopie des tableaux de
   `PORTAGE.md`. *Porte* seulement si **chaque** fonction du module est portee (ou ecartee par
   PORTAGE.md) et qu'aucune n'y est reduite. Sinon *partiel*, et ce qui reste compte dans les lignes
   a porter.
2. **Jeter / navigateur** — filets de securite et amarres DOM. `PORTAGE.md` pose deja la regle :
   Unreal a ses propres equivalents, *le choix est a refaire, pas a traduire*.
3. **Jeter / presentation** — lit la simulation pour la raconter a une UI qui n'existera pas
   sous cette forme. Refait contre le HUD Unreal.
4. **Jeter / baril** — `index.js` de re-export : sans objet en C++.
5. **Jeter / non atteint** — aucun chemin depuis `src/main.js`. A brancher ou a enterrer,
   decision de conception, pas de portage.
6. **Generer** — table de contenu. Ajouter un batiment, une espece ou une replique ne doit
   jamais demander une recompilation.
7. **Porter** — le reste. Marque *scinder* quand une table de contenu y est melee.

Une fonction est **portee** si PORTAGE.md la nomme. Sinon, le C++ de `Source/AnastasisSim` (hors
tests) est interroge :

- module que PORTAGE.md cite **en entier** ou sans liste : il suffit que le C++ nomme la fonction (nom JS,
  PascalCase ou alias declare) sur une ligne qui ne dit pas qu'elle n'est *pas* portee ;
- module dont PORTAGE.md **liste** les fonctions : il faut une fonction C++ de ce nom (PascalCase suivi
  d'une parenthese). Un commentaire qui cite la reference ne suffit pas — il dit souvent ce que le C++
  ne fait pas (`exploreTarget` tire `sim.rng` : non porte).
- `simulation.js` et `npc.js` ne sont pas interroges : seule la liste de PORTAGE.md compte.

## Ce que cet inventaire ne sait pas

- Les verdicts venus d'une liste explicite sont des **decisions**, pas des mesures. Elles
  sont dans `tools/migration/inventory-js-sim.mjs` et `ported-functions.mjs`, chacune avec sa raison,
  et se discutent.
- **Portee ne veut pas dire prouvee bit a bit.** La colonne dit ce que PORTAGE.md declare porte ; la
  preuve est dans les tests `Anastasis.Sim.Parite.*` cites par PORTAGE.md.
- Pour un module cite en entier, la recherche de noms dans le C++ est **indulgente** : un nom generique
  (`push`, `pop`, `bump`) y est trouve sans que la fonction JS soit forcement celle-la. Pour un module a
  liste, elle est **stricte** : une fonction aidante absorbee sans nom par une fonction portee compte dans
  le reste. Le reste d'un module partiel est donc un ordre de grandeur, pas un decompte au mot pres.
- Les extents de fonctions viennent d'un scanner (`js-functions.mjs`), pas d'un parseur JS : une
  expression reguliere precedee de `)` serait mal lue. Controle a chaque generation : sur les
  233 modules du perimetre, 0 definition(s) en colonne 0 avalee(s) par une autre (attendu : 0).
- `code` compte les lignes de noms d'un baril de re-export : le total de la colonne **A jeter**
  est surevalue d'environ 800 lignes pour cette raison. Sans consequence, on les jette.
- La part de litteraux rate les gabarits multi-lignes. Un module peut porter plus de contenu
  que la colonne `txt` ne le dit — le seuil *scinder* est un plancher, pas un plafond.
- **Non atteint depuis `main.js`** ne veut pas dire mort. `sim/villageSpectrum.js` (partition
  spectrale du village par vecteur de Fiedler) est ecrit, documente, et branche nulle part :
  c'est une decision de conception en attente, pas un dechet.
- `world.js` garde un vecteur `Fbm` divergent d'environ 2,5 ulp. Voir `PORTAGE.md`.

## Le detail

`code` exclut commentaires, lignes vides et litteraux de texte. `reste` = lignes de code a porter.
`txt` est le nombre de lignes qui ne sont qu'un litteral. `imp` = nombre de modules du noyau qui
importent celui-ci.

| Verdict | Module | lignes | code | reste | txt | imp | Vague | Chantier | Note |
| --- | --- | ---: | ---: | ---: | ---: | ---: | --- | --- | --- |
| porter | `sim/crowdNav.js` | 280 | 228 | 228 | 3 | 1 | 2 | navigation |  |
| porter | `sim/percolation.js` | 205 | 127 | 127 | 0 | 1 | 2 | navigation |  |
| porter | `sim/trafficDecay.js` | 106 | 73 | 73 | 0 | 1 | 2 | navigation |  |
| porter | `sim/destination.js` | 85 | 50 | 50 | 0 | 1 | 2 | navigation |  |
| porter | `sim/landRoads.js` | 80 | 44 | 44 | 0 | 2 | 2 | navigation |  |
| porter | `sim/landExtent.js` | 58 | 35 | 35 | 0 | 2 | 2 | navigation |  |
| porter | `sim/logicalLod.js` | 253 | 228 | 228 | 0 | 1 | 3 | budget et LOD |  |
| porter | `sim/worldChange.js` | 107 | 83 | 83 | 0 | 0 | 4 | etat et sauvegarde |  |
| porter | `sim/collectivePriorities.js` | 3847 | 3035 | 3035 | 24 | 18 | 5 | societe et institutions |  |
| porter | `sim/colonizationDoctrine.js` | 1018 | 799 | 799 | 4 | 8 | 5 | societe et institutions |  |
| porter | `sim/transport/delivery.js` | 875 | 770 | 770 | 2 | 12 | 5 | transport et logistique |  |
| porter | `sim/socialOrders.js` | 912 | 749 | 749 | 38 | 11 | 5 | societe et institutions |  |
| porter | `sim/jobMarket.js` | 828 | 680 | 680 | 2 | 3 | 5 | economie et travail |  |
| porter | `sim/socialAbduction.js` | 649 | 543 | 543 | 38 | 4 | 5 | societe et institutions |  |
| porter | `sim/life.js` | 656 | 540 | 540 | 18 | 2 | 5 | noyau de boucle |  |
| porter | `sim/oecumene.js` | 624 | 509 | 509 | 37 | 8 | 5 | societe et institutions |  |
| porter | `sim/urban/intent.js` | 554 | 489 | 489 | 0 | 3 | 5 | urbanisme |  |
| porter | `sim/animaux/updateAnimals.js` | 594 | 465 | 465 | 0 | 4 | 5 | regne animal |  |
| porter | `sim/transport/carts.js` | 520 | 449 | 449 | 1 | 3 | 5 | transport et logistique |  |
| porter | `sim/transportProjects.js` | 535 | 437 | 437 | 6 | 2 | 5 | urbanisme |  |
| porter | `sim/urban/taxonomy.js` | 502 | 421 | 421 | 17 | 8 | 5 | urbanisme |  |
| porter | `sim/cohesionClimate.js` | 516 | 414 | 414 | 29 | 5 | 5 | societe et institutions |  |
| porter | `sim/villageChronicle.js` | 516 | 414 | 414 | 21 | 4 | 5 | chronique et memoire collective |  |
| porter | `sim/clioscopeSensitivity.js` | 466 | 389 | 389 | 21 | 1 | 5 | chronique et memoire collective |  |
| porter | `sim/founderCharter.js` | 517 | 381 | 381 | 5 | 6 | 5 | urbanisme |  |
| porter | `sim/colonySite.js` | 451 | 363 | 363 | 10 | 5 | 5 | urbanisme |  |
| porter | `sim/socialCycles.js` | 447 | 361 | 361 | 17 | 7 | 5 | societe et institutions |  |
| porter | `sim/transport/congestion.js` | 394 | 327 | 327 | 7 | 3 | 5 | transport et logistique |  |
| porter + scinder | `sim/romanChronicle.js` | 456 | 322 | 322 | 99 | 7 | 5 | chronique et memoire collective | formules de chronique + declenchement |
| porter | `sim/sagas.js` | 429 | 312 | 312 | 2 | 3 | 5 | chronique et memoire collective |  |
| porter | `sim/urban/terrainMorphology.js` | 346 | 310 | 310 | 0 | 3 | 5 | urbanisme |  |
| porter | `sim/urban/potentialField.js` | 346 | 301 | 301 | 0 | 2 | 5 | urbanisme |  |
| porter | `sim/transport/diagnostics.js` | 318 | 284 | 284 | 0 | 1 | 5 | transport et logistique |  |
| porter | `sim/economyLoopMetrics.js` | 342 | 279 | 279 | 5 | 1 | 5 | economie et travail |  |
| porter | `sim/transport/index.js` | 302 | 267 | 267 | 0 | 7 | 5 | transport et logistique |  |
| porter | `sim/colonyStockReport.js` | 305 | 254 | 254 | 0 | 5 | 5 | economie et travail |  |
| porter | `sim/urban/districtBehavior.js` | 294 | 249 | 249 | 2 | 3 | 5 | urbanisme |  |
| porter | `sim/settlementSite.js` | 296 | 247 | 247 | 0 | 2 | 5 | urbanisme |  |
| porter | `sim/urban/cadastre.js` | 313 | 244 | 244 | 1 | 4 | 5 | urbanisme |  |
| porter | `sim/clioscope.js` | 279 | 234 | 234 | 2 | 4 | 5 | chronique et memoire collective |  |
| porter | `sim/transport/porters.js` | 322 | 234 | 234 | 0 | 1 | 5 | transport et logistique |  |
| porter | `sim/constructionPipeline.js` | 316 | 232 | 232 | 1 | 2 | 5 | economie et travail |  |
| porter | `sim/clioscopeScenarios.js` | 288 | 231 | 231 | 25 | 3 | 5 | chronique et memoire collective |  |
| porter | `sim/eraClimateBridge.js` | 287 | 226 | 226 | 9 | 4 | 5 | chronique et memoire collective |  |
| porter | `sim/collectivePulse.js` | 293 | 219 | 219 | 26 | 1 | 5 | societe et institutions |  |
| porter | `sim/playerGenesis.js` | 254 | 218 | 218 | 11 | 0 | 5 | noyau de boucle |  |
| porter | `sim/transport/haulThroughput.js` | 266 | 202 | 202 | 0 | 4 | 5 | transport et logistique |  |
| porter | `sim/forestSustain.js` | 257 | 182 | 182 | 0 | 5 | 5 | economie et travail |  |
| porter | `sim/animaux/livestockEconomy.js` | 213 | 177 | 177 | 0 | 1 | 5 | regne animal |  |
| porter | `sim/watchPosts.js` | 208 | 174 | 174 | 0 | 2 | 5 | urbanisme |  |
| porter | `sim/content.js` | 261 | 171 | 171 | 0 | 24 | 5 | noyau de boucle |  |
| porter | `sim/resourceRelay.js` | 203 | 171 | 171 | 2 | 4 | 5 | economie et travail |  |
| porter | `sim/craftHandoff.js` | 215 | 168 | 168 | 1 | 1 | 5 | economie et travail |  |
| porter | `sim/clioscopeBatch.js` | 195 | 158 | 158 | 7 | 1 | 5 | chronique et memoire collective |  |
| porter | `sim/transport/orphanReserve.js` | 227 | 158 | 158 | 2 | 3 | 5 | transport et logistique |  |
| porter | `sim/animaux/hunting.js` | 213 | 155 | 155 | 0 | 3 | 5 | regne animal |  |
| porter | `sim/eventPrimitives.js` | 165 | 146 | 146 | 0 | 1 | 5 | noyau de boucle |  |
| porter | `sim/craftPairHelp.js` | 174 | 137 | 137 | 0 | 1 | 5 | economie et travail |  |
| porter | `sim/growthChapter.js` | 184 | 134 | 134 | 9 | 4 | 5 | chronique et memoire collective |  |
| porter | `sim/villageCrown.js` | 163 | 114 | 114 | 0 | 2 | 5 | urbanisme |  |
| porter | `sim/animaux/seedAnimals.js` | 139 | 113 | 113 | 0 | 1 | 5 | regne animal |  |
| porter | `sim/economy.js` | 145 | 112 | 112 | 0 | 3 | 5 | economie et travail |  |
| porter | `sim/landWaterPresets.js` | 151 | 112 | 112 | 1 | 1 | 5 | urbanisme |  |
| porter | `sim/urban/districtLoyalty.js` | 134 | 112 | 112 | 0 | 3 | 5 | urbanisme |  |
| porter | `sim/transport/spoilage.js` | 131 | 101 | 101 | 0 | 2 | 5 | transport et logistique |  |
| porter | `sim/transport/haulWatchdog.js` | 123 | 91 | 91 | 3 | 1 | 5 | transport et logistique |  |
| porter | `sim/decisionProvider.js` | 183 | 89 | 89 | 1 | 1 | 5 | noyau de boucle |  |
| porter | `sim/craftMiss.js` | 115 | 78 | 78 | 0 | 1 | 5 | economie et travail |  |
| porter | `sim/chronicleKindBias.js` | 77 | 57 | 57 | 0 | 2 | 5 | chronique et memoire collective |  |
| porter | `sim/animaux/createAnimal.js` | 87 | 55 | 55 | 0 | 3 | 5 | regne animal |  |
| porter | `sim/transport/reserveReconcile.js` | 85 | 51 | 51 | 0 | 1 | 5 | transport et logistique |  |
| porter | `sim/metiers/extractionPost.js` | 115 | 44 | 44 | 0 | 2 | 5 | economie et travail |  |
| porter | `sim/metiers/porterJob.js` | 119 | 43 | 43 | 0 | 3 | 5 | economie et travail |  |
| porter | `sim/animaux/landFauna.js` | 77 | 42 | 42 | 0 | 3 | 5 | regne animal |  |
| porter | `sim/haulLoad.js` | 65 | 38 | 38 | 0 | 2 | 5 | economie et travail |  |
| porter | `sim/animaux/animalDeath.js` | 42 | 24 | 24 | 0 | 4 | 5 | regne animal |  |
| porter | `sim/transport/roadTraffic.js` | 25 | 21 | 21 | 0 | 2 | 5 | transport et logistique |  |
| porter | `sim/valmireSignatures.js` | 17 | 6 | 6 | 0 | 2 | 5 | chronique et memoire collective |  |
| porter | `life/lifeScenes.js` | 949 | 818 | 818 | 2 | 6 | 6 | personne et famille |  |
| porter | `ai/ambitions.js` | 835 | 685 | 685 | 53 | 5 | 6 | cognition |  |
| porter | `lang/lexicon.js` | 817 | 646 | 646 | 0 | 2 | 6 | langue |  |
| porter | `life/kosmos1204UneBouchePlus.js` | 736 | 640 | 640 | 39 | 1 | 6 | rites et culture |  |
| porter + scinder | `ai/episodes.js` | 963 | 636 | 636 | 146 | 17 | 6 | cognition | gabarits d'episodes + machine a etats |
| porter | `life/techniques.js` | 684 | 554 | 554 | 15 | 4 | 6 | personne et famille |  |
| porter + scinder | `life/followVignette.js` | 740 | 480 | 480 | 139 | 2 | 6 | parole et narration | vignettes + composition |
| porter | `ai/householdPlan.js` | 529 | 469 | 469 | 11 | 6 | 6 | cognition |  |
| porter | `life/nature.js` | 526 | 464 | 464 | 1 | 8 | 6 | personne et famille |  |
| porter | `life/orthodoxBaptism1204.js` | 490 | 441 | 441 | 11 | 3 | 6 | rites et culture |  |
| porter | `life/orthodoxMarriage1204.js` | 423 | 378 | 378 | 7 | 3 | 6 | rites et culture |  |
| porter | `ai/socialCognition.js` | 550 | 366 | 366 | 6 | 3 | 6 | cognition |  |
| porter | `life/orthodoxFunerary1204.js` | 410 | 366 | 366 | 5 | 2 | 6 | rites et culture |  |
| porter + scinder | `life/colonySeals.js` | 528 | 362 | 362 | 123 | 7 | 6 | rites et culture | sceaux + conditions |
| porter | `life/witnessMemory.js` | 473 | 356 | 356 | 49 | 2 | 6 | parole et narration |  |
| porter | `life/careers.js` | 421 | 351 | 351 | 6 | 3 | 6 | personne et famille |  |
| porter | `lang/phonology.js` | 472 | 350 | 350 | 0 | 7 | 6 | langue |  |
| porter | `life/culture.js` | 449 | 350 | 350 | 16 | 15 | 6 | rites et culture |  |
| porter | `lang/decode.js` | 445 | 340 | 340 | 0 | 1 | 6 | langue |  |
| porter + scinder | `life/names.js` | 441 | 340 | 340 | 10 | 5 | 6 | personne et famille | pools onomastiques + selection |
| porter | `lang/generate.js` | 422 | 335 | 335 | 1 | 3 | 6 | langue |  |
| porter | `life/romanCouncils.js` | 372 | 330 | 330 | 5 | 1 | 6 | rites et culture |  |
| porter | `ai/dayIntent.js` | 378 | 319 | 319 | 13 | 5 | 6 | cognition |  |
| porter | `life/orthodoxParish1204.js` | 368 | 314 | 314 | 10 | 4 | 6 | rites et culture |  |
| porter | `life/momentTalk.js` | 412 | 313 | 313 | 50 | 5 | 6 | parole et narration |  |
| porter | `life/lineage.js` | 355 | 302 | 302 | 4 | 4 | 6 | personne et famille |  |
| porter | `life/socialGesture.js` | 351 | 302 | 302 | 0 | 7 | 6 | personne et famille |  |
| porter | `life/culturalMemory.js` | 351 | 298 | 298 | 24 | 3 | 6 | rites et culture |  |
| porter | `life/standing.js` | 324 | 266 | 266 | 1 | 3 | 6 | personne et famille |  |
| porter | `life/identityFactions.js` | 290 | 256 | 256 | 8 | 5 | 6 | personne et famille |  |
| porter | `life/nameForge.js` | 322 | 251 | 251 | 0 | 3 | 6 | personne et famille |  |
| porter + scinder | `life/collectiveTalk.js` | 356 | 206 | 206 | 99 | 1 | 6 | parole et narration | formules + agregation |
| porter + scinder | `life/foundingCeremony.js` | 307 | 201 | 201 | 49 | 2 | 6 | rites et culture | ceremonie + deroule |
| porter | `life/chronicleEvents.js` | 240 | 195 | 195 | 23 | 2 | 6 | personne et famille |  |
| porter | `ai/haulStreetSignal.js` | 222 | 194 | 194 | 2 | 1 | 6 | cognition |  |
| porter | `lang/fromSpeechAct.js` | 229 | 187 | 187 | 0 | 1 | 6 | langue |  |
| porter | `ai/accessMemory.js` | 209 | 186 | 186 | 0 | 2 | 6 | cognition |  |
| porter | `life/pneuma/causalSignals.js` | 215 | 186 | 186 | 6 | 4 | 6 | personne et famille |  |
| porter | `ai/ageSignature.js` | 218 | 184 | 184 | 0 | 5 | 6 | cognition |  |
| porter | `life/mortality.js` | 262 | 184 | 184 | 3 | 2 | 6 | personne et famille |  |
| porter | `lang/fromNpc.js` | 250 | 183 | 183 | 1 | 2 | 6 | langue |  |
| porter | `life/socialMemoryIntegrity.js` | 189 | 176 | 176 | 0 | 2 | 6 | personne et famille |  |
| porter | `ai/failureMemory.js` | 202 | 168 | 168 | 3 | 4 | 6 | cognition |  |
| porter | `ai/negativeKnowledge.js` | 195 | 164 | 164 | 1 | 3 | 6 | cognition |  |
| porter | `ai/socialStanding.js` | 206 | 161 | 161 | 0 | 1 | 6 | cognition |  |
| porter | `life/cultureDaily.js` | 194 | 157 | 157 | 1 | 2 | 6 | rites et culture |  |
| porter | `ai/rumorSpread.js` | 186 | 151 | 151 | 2 | 2 | 6 | cognition |  |
| porter | `life/nameLedger.js` | 198 | 146 | 146 | 0 | 2 | 6 | personne et famille |  |
| porter | `lang/ir.js` | 167 | 128 | 128 | 0 | 4 | 6 | langue |  |
| porter + scinder | `life/sealTalk.js` | 255 | 128 | 128 | 92 | 5 | 6 | parole et narration | formules + declenchement |
| porter | `ai/siteDesirability.js` | 162 | 120 | 120 | 0 | 1 | 6 | cognition |  |
| porter + scinder | `life/historicalRumors.js` | 176 | 120 | 120 | 41 | 1 | 6 | parole et narration | corpus de rumeurs + propagation |
| porter | `ai/dailyRoutine.js` | 171 | 117 | 117 | 0 | 1 | 6 | cognition |  |
| porter | `life/household.js` | 148 | 116 | 116 | 0 | 4 | 6 | personne et famille |  |
| porter + scinder | `life/landTalk.js` | 227 | 112 | 112 | 73 | 2 | 6 | parole et narration | formules + declenchement |
| porter | `ai/algorithmic/foodPerception.js` | 136 | 108 | 108 | 0 | 3 | 6 | cognition |  |
| porter | `ai/streetSignal.js` | 148 | 108 | 108 | 0 | 2 | 6 | cognition |  |
| porter | `life/orthodoxPriestArrival1204.js` | 119 | 107 | 107 | 1 | 1 | 6 | rites et culture |  |
| porter | `life/liturgicalCalendar.js` | 143 | 101 | 101 | 0 | 4 | 6 | rites et culture |  |
| porter | `ai/householdRoles.js` | 130 | 96 | 96 | 0 | 1 | 6 | cognition |  |
| porter | `life/liturgicalNarrative.js` | 118 | 96 | 96 | 0 | 1 | 6 | parole et narration |  |
| porter | `life/elders.js` | 120 | 95 | 95 | 0 | 3 | 6 | personne et famille |  |
| porter | `life/sagaFamine.js` | 158 | 84 | 84 | 5 | 1 | 6 | personne et famille |  |
| porter | `life/seasonNarrative.js` | 112 | 84 | 84 | 3 | 2 | 6 | parole et narration |  |
| porter + scinder | `life/talkCanon1204.js` | 124 | 75 | 75 | 39 | 1 | 6 | parole et narration | canon 1204 + selection |
| porter | `lang/channel.js` | 112 | 73 | 73 | 0 | 1 | 6 | langue |  |
| porter | `life/restTraces.js` | 101 | 72 | 72 | 0 | 2 | 6 | personne et famille |  |
| porter | `life/workShift.js` | 205 | 69 | 69 | 0 | 5 | 6 | personne et famille |  |
| porter | `ai/algorithmic/memoryEvent.js` | 80 | 66 | 66 | 0 | 3 | 6 | cognition |  |
| porter | `life/jobTransition.js` | 92 | 61 | 61 | 0 | 3 | 6 | personne et famille |  |
| porter | `life/needActNarrative.js` | 100 | 61 | 61 | 0 | 3 | 6 | parole et narration |  |
| porter | `life/landAmbition.js` | 101 | 60 | 60 | 0 | 3 | 6 | personne et famille |  |
| porter + scinder | `life/arrivalContext.js` | 77 | 53 | 53 | 13 | 2 | 6 | personne et famille | logique posee sur une table (20% de litteraux) |
| porter | `lang/idiolect.js` | 102 | 51 | 51 | 0 | 2 | 6 | langue |  |
| porter | `life/followMotif.js` | 85 | 43 | 43 | 0 | 4 | 6 | parole et narration |  |
| porter | `ai/algorithmic/metrics.js` | 48 | 41 | 41 | 0 | 4 | 6 | cognition |  |
| porter | `life/familyVisual.js` | 52 | 38 | 38 | 0 | 1 | 6 | personne et famille |  |
| porter | `ai/goalUtility.js` | 55 | 37 | 37 | 0 | 1 | 6 | cognition |  |
| porter | `life/generationNarrative.js` | 46 | 33 | 33 | 1 | 2 | 6 | parole et narration |  |
| porter | `life/orthodoxAssembly1204.js` | 35 | 27 | 27 | 0 | 5 | 6 | rites et culture |  |
| porter | `life/goalTransition.js` | 72 | 20 | 20 | 0 | 2 | 6 | personne et famille |  |
| porter | `ai/algorithmic/flags.js` | 19 | 10 | 10 | 0 | 3 | 6 | cognition |  |
| partiel | `sim/spatialGrid.js` | 142 | 100 | 24 | 0 | 8 | 0 | socle | Core/AnastasisSpatialGrid — 6/12 fonctions portees |
| partiel | `sim/worldArchetypes.js` | 866 | 752 | 595 | 7 | 4 | 1 | generation du monde | World/AnastasisWorldArchetype — 2/4 fonctions portees |
| partiel | `sim/fieldCrops.js` | 155 | 120 | 68 | 0 | 11 | 1 | generation du monde | World/AnastasisWorld, Work/AnastasisFields, Work/AnastasisGather — 8/14 fonctions portees |
| partiel | `sim/navGrid.js` | 435 | 364 | 75 | 2 | 6 | 2 | navigation | World/AnastasisNavGrid, World/AnastasisNavService — 13/19 fonctions portees |
| partiel | `sim/pathfinding.js` | 212 | 179 | 7 | 0 | 3 | 2 | navigation | World/AnastasisPathfinding — 13/15 fonctions portees |
| partiel | `sim/save.js` | 829 | 656 | 609 | 2 | 0 | 4 | etat et sauvegarde | Harness/AnastasisJsSave — 1/11 fonctions portees |
| partiel | `sim/pristineWorld.js` | 56 | 39 | 9 | 0 | 2 | 4 | etat et sauvegarde | Harness/AnastasisJsSave (GenerateWorld) — 2/4 fonctions portees |
| partiel | `sim/simulation.js` | 8664 | 7196 | 6647 | 16 | 1 | 5 | noyau de boucle | Village/AnastasisVillage, Sim/AnastasisSimulation, Work/AnastasisFields, Work/AnastasisBuild, Life/AnastasisWeatherBehavior — 24/388 fonctions portees |
| partiel | `sim/npc.js` | 6060 | 5169 | 4507 | 41 | 3 | 5 | noyau de boucle | Village/AnastasisVillage, Work/AnastasisGather, Life/AnastasisWeatherBehavior — 21/181 fonctions portees |
| partiel | `sim/craftWork.js` | 1133 | 964 | 854 | 0 | 4 | 5 | economie et travail | Work/AnastasisGather, Work/AnastasisBuild — 4/51 fonctions portees |
| partiel | `sim/transport/stockLedger.js` | 498 | 395 | 355 | 7 | 18 | 5 | transport et logistique | Village/AnastasisVillage — 3/30 fonctions portees |
| partiel | `sim/weather.js` | 357 | 268 | 96 | 0 | 2 | 5 | noyau de boucle | World/AnastasisWeather — 11/16 fonctions portees |
| partiel | `sim/craftToolSwitch.js` | 96 | 65 | 55 | 0 | 1 | 5 | economie et travail | Work/AnastasisBuild, Village/AnastasisVillage — 1/6 fonctions portees |
| partiel | `sim/constructionPieces.js` | 77 | 62 | 39 | 0 | 1 | 5 | economie et travail | Work/AnastasisBuild — 1/5 fonctions portees |
| partiel | `sim/craftFatigue.js` | 71 | 43 | 4 | 0 | 2 | 5 | economie et travail | Work/AnastasisGather — 2/3 fonctions portees |
| partiel | `sim/fieldWorkPosts.js` | 127 | 104 | 3 | 0 | 2 | 5 | economie et travail | Work/AnastasisGather, Village/AnastasisVillage — 6/7 fonctions portees |
| partiel | `life/talk.js` | 2220 | 1887 | 1464 | 0 | 4 | 6 | parole et narration | Life/AnastasisBonds, Village/AnastasisVillage — 15/92 fonctions portees |
| partiel | `ai/memory.js` | 1050 | 873 | 717 | 14 | 6 | 6 | cognition | Village/AnastasisVillage, Work/AnastasisGather — 8/53 fonctions portees |
| partiel | `life/speechActs.js` | 613 | 553 | 503 | 21 | 5 | 6 | parole et narration | Village/AnastasisVillage — 3/33 fonctions portees |
| partiel | `life/domestic.js` | 434 | 361 | 259 | 1 | 8 | 6 | personne et famille | Life/AnastasisNeeds, Village/AnastasisVillage — 9/31 fonctions portees |
| partiel | `ai/socialMemory.js` | 515 | 407 | 177 | 1 | 10 | 6 | cognition | Life/AnastasisBonds — 13/23 fonctions portees |
| partiel | `life/needs.js` | 702 | 466 | 152 | 0 | 21 | 6 | personne et famille | Life/AnastasisNeeds (+ Work/AnastasisGather) — 19/29 fonctions portees |
| partiel | `life/villageRhythm.js` | 412 | 343 | 113 | 5 | 10 | 6 | personne et famille | Life/AnastasisVillageRhythm, Village/AnastasisVillage — 8/20 fonctions portees |
| partiel | `life/bonds.js` | 422 | 328 | 97 | 0 | 6 | 6 | personne et famille | Life/AnastasisBonds — 10/22 fonctions portees |
| partiel | `life/moodlets.js` | 291 | 220 | 97 | 32 | 6 | 6 | personne et famille | Life/AnastasisBonds — 4/11 fonctions portees |
| partiel | `ai/algorithmic/mealReservation.js` | 439 | 377 | 80 | 0 | 7 | 6 | cognition | Village/AnastasisVillage — 10/17 fonctions portees |
| partiel | `life/skills.js` | 150 | 122 | 78 | 0 | 7 | 6 | personne et famille | Work/AnastasisGather — 3/8 fonctions portees |
| partiel | `ai/moralPressure.js` | 165 | 116 | 46 | 0 | 2 | 6 | cognition | Work/AnastasisGather — 1/4 fonctions portees |
| partiel | `ai/algorithmic/runtime.js` | 247 | 224 | 41 | 0 | 2 | 6 | cognition | Village/AnastasisVillage — 4/9 fonctions portees |
| partiel | `ai/algorithmic/scheduler.js` | 66 | 51 | 36 | 0 | 2 | 6 | cognition | Ai/AnastasisNous — 1/5 fonctions portees |
| partiel | `life/genome.js` | 293 | 173 | 31 | 0 | 2 | 6 | personne et famille | Life/AnastasisGenome — 15/18 fonctions portees |
| partiel | `ai/weatherGoalBias.js` | 212 | 166 | 29 | 0 | 4 | 6 | cognition | Life/AnastasisWeatherBehavior — 6/8 fonctions portees |
| partiel | `ai/algorithmic/hungerAction.js` | 230 | 205 | 25 | 0 | 4 | 6 | cognition | Village/AnastasisVillage — 3/5 fonctions portees |
| partiel | `ai/algorithmic/bridge.js` | 215 | 191 | 12 | 2 | 1 | 6 | cognition | Village/AnastasisVillage — 3/5 fonctions portees |
| generer | `sim/batiments/catalog.js` | 551 | 528 | 0 | 15 | 4 | — | — | catalogue des batiments |
| generer | `sim/animaux/catalog.js` | 349 | 285 | 0 | 0 | 10 | — | — | catalogue des especes |
| generer | `sim/metiers/catalog.js` | 129 | 116 | 0 | 1 | 2 | — | — | catalogue des metiers |
| generer | `life/talkCatalog.js` | 1072 | 349 | 0 | 652 | 1 | — | — | catalogue de repliques |
| jeter | `runtime/sessionLaunchProfile.js` | 466 | 362 | 0 | 5 | 1 | — | — | profil de lancement navigateur |
| jeter | `runtime/index.js` | 426 | 303 | 0 | 6 | 0 | — | — | amorcage du runtime navigateur (cable les filets ci-dessus) |
| jeter | `runtime/faultShield.js` | 317 | 234 | 0 | 0 | 1 | — | — | filet de securite navigateur |
| jeter | `runtime/crashReport.js` | 268 | 229 | 0 | 0 | 1 | — | — | filet de securite navigateur |
| jeter | `runtime/flightRecorder.js` | 226 | 169 | 0 | 0 | 1 | — | — | filet de securite navigateur |
| jeter | `runtime/postMortemUi.js` | 180 | 136 | 0 | 0 | 1 | — | — | filet de securite navigateur |
| jeter | `sim/villageSpectrum.js` | 199 | 132 | 0 | 0 | 0 | — | — | non atteint depuis src/main.js — a brancher ou a enterrer, pas a porter |
| jeter | `sim/saveStore.js` | 177 | 127 | 0 | 0 | 0 | — | — | localStorage -> SaveGame d'Unreal |
| jeter | `runtime/bootTelemetry.js` | 177 | 126 | 0 | 0 | 0 | — | — | telemetrie de demarrage navigateur |
| jeter | `runtime/stateRewind.js` | 160 | 100 | 0 | 0 | 1 | — | — | rembobinage outil de dev, a refaire sur le save Unreal |
| jeter | `runtime/watchdog.js` | 142 | 95 | 0 | 0 | 1 | — | — | filet de securite navigateur |
| jeter | `sim/collectivePrioritiesPanel.js` | 99 | 77 | 0 | 0 | 0 | — | — | panneau UI |
| jeter | `sim/animaux/index.js` | 62 | 47 | 0 | 0 | 2 | — | — | baril de re-export, sans equivalent C++ |
| jeter | `runtime/gameLoop.js` | 45 | 31 | 0 | 0 | 0 | — | — | requestAnimationFrame -> Tick d'Unreal |
| jeter | `life/index.js` | 531 | 494 | 0 | 0 | 2 | — | — | baril de re-export, sans equivalent C++ |
| jeter | `life/pneuma/PneumaBubbleDirector.js` | 388 | 305 | 0 | 1 | 0 | — | — | politique d'attention des bulles (rendu three.js) |
| jeter | `ai/index.js` | 277 | 241 | 0 | 0 | 2 | — | — | baril de re-export, sans equivalent C++ |
| jeter | `ai/explainGoal.js` | 280 | 228 | 0 | 0 | 3 | — | — | texte d'explication pour l'UI de debug |
| jeter | `ai/npcInspector.js` | 229 | 163 | 0 | 27 | 1 | — | — | inspecteur Observatoire F3 (Unreal: AnastasisInspectTools) |
| jeter | `ai/algorithmic/index.js` | 111 | 96 | 0 | 0 | 2 | — | — | baril de re-export, sans equivalent C++ |
| jeter | `life/engineBridge.js` | 90 | 81 | 0 | 0 | 0 | — | — | facade de lecture pour main.js et l'UI |
| jeter | `ai/algorithmic/debug.js` | 79 | 60 | 0 | 10 | 1 | — | — | sondes de debug |
| jeter | `ai/goalLabels.js` | 39 | 11 | 0 | 24 | 5 | — | — | libelles d'objectifs pour l'UI |
| porte | `runtime/simClock.js` | 217 | 128 | 0 | 0 | 1 | 0 | — | Core/AnastasisSimClock |
| porte | `sim/util.js` | 55 | 27 | 0 | 0 | 69 | 0 | — | Core/AnastasisSimMath |
| porte | `sim/rng.js` | 40 | 25 | 0 | 0 | 14 | 0 | — | Core/AnastasisRng |
| porte | `sim/hydrology.js` | 445 | 333 | 0 | 0 | 1 | 1 | — | World/AnastasisHydrology |
| porte | `sim/world.js` | 427 | 320 | 0 | 0 | 3 | 1 | — | World/AnastasisWorld (+ WorldNoise) |
| porte | `sim/navService.js` | 511 | 391 | 0 | 3 | 2 | 2 | — | World/AnastasisNavService |
| porte | `sim/simulationBudget.js` | 275 | 166 | 0 | 0 | 2 | 3 | — | Core/AnastasisSimBudget |
| porte | `sim/lifestyle.js` | 249 | 216 | 0 | 9 | 4 | 5 | — | Life/AnastasisLifestyle |
| porte | `ai/algorithmic/hungerUtility.js` | 245 | 204 | 0 | 2 | 4 | 6 | — | Ai/AnastasisNous |
| porte | `life/conditioning.js` | 109 | 48 | 0 | 0 | 3 | 6 | — | Life/AnastasisConditioning |
| porte | `ai/algorithmic/inertia.js` | 55 | 41 | 0 | 0 | 4 | 6 | — | Ai/AnastasisNous |
| porte | `ai/algorithmic/decision.js` | 59 | 40 | 0 | 0 | 4 | 6 | — | Ai/AnastasisNous |
