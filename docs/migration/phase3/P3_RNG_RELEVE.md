# Relevé des tirages `sim.rng` — scénario `endurance`, 600 ticks

Généré par `tools/migration/trace-sim-rng.mjs` (mission sim-rng-001). Ne pas éditer à la main : relancer.

```bash
node tools/migration/trace-sim-rng.mjs -ref <clone anastasis-ref-p3> -ticks 600
```

- Référence : `fee66ae` (tag `anastasis-ref-p3`), scénario `endurance` (graine 12345, dt 0.016666666666666666, vue (54, 57)), 24 masques, tick recomposé.
- État `sim.rng` : 2576143622 au départ, 3482227477 après 600 ticks.
- **371 tirages**, dont 334 dans `chooseGoal`, en **20 décisions**.
- Tirages bruts : `docs/migration/phase3/P3_RNG_RELEVE_600.csv` (un par ligne : tick, étape, habitant, décision, état avant, site, appelant, chemin).

## Premier tirage au tick 32 ou après

- tick 32, étape `updateNpc`, habitant `npc-2`, état avant 2576143622
- site : `exploreTarget src/ai/memory.js:669`
- chemin : `updateNpc > measure > measure > (anonyme) > chooseGoal > scoreGoals > adultScores > failureTargetBiasMap > failureCauseForGoal > exploreTarget`
- 16 tirage(s) à ce tick : exploreTarget x2, goalNoise x14

## Par site (fonction et ligne de la référence qui appelle `sim.rng`)

| Tirages | Site |
| ---: | --- |
| 280 | `goalNoise src/sim/npc.js:491` |
| 28 | `exploreTarget src/ai/memory.js:669` |
| 28 | `exploreTarget src/ai/memory.js:670` |
| 16 | `(anonyme) src/sim/npc.js:893` |
| 15 | `rollCraftMiss src/sim/craftMiss.js:79` |
| 2 | `maybeChatOnHaul src/sim/npc.js:5505` |
| 2 | `createInformResourceSpotActs src/life/speechActs.js:62` |

## Par étape du tick

| Tirages | Étape |
| ---: | --- |
| 371 | `updateNpc` |

## Tirages par décision (`chooseGoal`)

| Tirages | Décisions |
| ---: | ---: |
| 16 | 15 |
| 18 | 4 |
| 22 | 1 |

## Les décisions, une par ligne

Sites dans l'ordre des tirages ; `x6` = six tirages consécutifs au même site.

| Tick | Habitant | État avant | Tirages | Sites |
| ---: | --- | ---: | ---: | --- |
| 32 | `npc-2` | 2576143622 | 16 | exploreTarget x2, goalNoise x14 |
| 63 | `npc-0` | 1816425558 | 16 | exploreTarget x2, goalNoise x14 |
| 82 | `npc-3` | 1056707494 | 16 | exploreTarget x2, goalNoise x14 |
| 114 | `npc-1` | 296989430 | 16 | exploreTarget x2, goalNoise x14 |
| 133 | `npc-4` | 1368837179 | 16 | exploreTarget x2, goalNoise x14 |
| 196 | `npc-0` | 4272250741 | 16 | exploreTarget x2, goalNoise x14 |
| 247 | `npc-1` | 1049131194 | 16 | exploreTarget x2, goalNoise x14 |
| 266 | `npc-4` | 3952544756 | 16 | exploreTarget x2, goalNoise x14 |
| 298 | `npc-2` | 2560991022 | 16 | exploreTarget x2, goalNoise x14 |
| 320 | `npc-0` | 3632838771 | 18 | exploreTarget x4, goalNoise x14 |
| 320 | `npc-1` | 2241285037 | 16 | exploreTarget x2, goalNoise x14 |
| 320 | `npc-3` | 3313132786 | 18 | exploreTarget x4, goalNoise x14 |
| 320 | `npc-4` | 3753144865 | 16 | exploreTarget x2, goalNoise x14 |
| 453 | `npc-0` | 466084121 | 18 | exploreTarget x4, goalNoise x14 |
| 453 | `npc-1` | 906096200 | 16 | exploreTarget x2, goalNoise x14 |
| 453 | `npc-3` | 1977943949 | 22 | exploreTarget x8, goalNoise x14 |
| 453 | `npc-4` | 1154284688 | 16 | exploreTarget x2, goalNoise x14 |
| 536 | `npc-2` | 2794026910 | 16 | exploreTarget x2, goalNoise x14 |
| 586 | `npc-0` | 2602203319 | 16 | exploreTarget x2, goalNoise x14 |
| 586 | `npc-1` | 3674051068 | 18 | exploreTarget x4, goalNoise x14 |

## Tirages hors décision

| Tick | Étape | Habitant | Site | Chemin |
| ---: | --- | --- | --- | --- |
| 125 | `updateNpc` | `npc-0` | `maybeChatOnHaul src/sim/npc.js:5505` | `updateNpc > measure > measure > (anonyme) > act > perform > deliver > maybeChatOnHaul` |
| 141 | `updateNpc` | `npc-1` | `maybeChatOnHaul src/sim/npc.js:5505` | `updateNpc > measure > measure > (anonyme) > act > perform > deliver > maybeChatOnHaul` |
| 165 | `updateNpc` | `npc-2` | `(anonyme) src/sim/npc.js:893` | `updateNpc > measure > measure > (anonyme)` |
| 215 | `updateNpc` | `npc-3` | `(anonyme) src/sim/npc.js:893` | `updateNpc > measure > measure > (anonyme)` |
| 257 | `updateNpc` | `npc-0` | `rollCraftMiss src/sim/craftMiss.js:79` | `updateNpc > measure > measure > (anonyme) > act > progressTendWork > rollCraftMiss` |
| 266 | `updateNpc` | `npc-4` | `(anonyme) src/sim/npc.js:893` | `updateNpc > measure > measure > (anonyme)` |
| 293 | `updateNpc` | `npc-0` | `rollCraftMiss src/sim/craftMiss.js:79` | `updateNpc > measure > measure > (anonyme) > act > progressTendWork > rollCraftMiss` |
| 298 | `updateNpc` | `npc-2` | `(anonyme) src/sim/npc.js:893` | `updateNpc > measure > measure > (anonyme)` |
| 320 | `updateNpc` | `npc-0` | `(anonyme) src/sim/npc.js:893` | `updateNpc > measure > measure > (anonyme)` |
| 320 | `updateNpc` | `npc-3` | `(anonyme) src/sim/npc.js:893` | `updateNpc > measure > measure > (anonyme)` |
| 320 | `updateNpc` | `npc-4` | `(anonyme) src/sim/npc.js:893` | `updateNpc > measure > measure > (anonyme)` |
| 321 | `updateNpc` | `npc-1` | `exploreTarget src/ai/memory.js:669` | `updateNpc > measure > measure > (anonyme) > act > redirectAfterFailure > exploreTarget` |
| 321 | `updateNpc` | `npc-1` | `exploreTarget src/ai/memory.js:670` | `updateNpc > measure > measure > (anonyme) > act > redirectAfterFailure > exploreTarget` |
| 329 | `updateNpc` | `npc-0` | `rollCraftMiss src/sim/craftMiss.js:79` | `updateNpc > measure > measure > (anonyme) > act > progressTendWork > rollCraftMiss` |
| 365 | `updateNpc` | `npc-0` | `rollCraftMiss src/sim/craftMiss.js:79` | `updateNpc > measure > measure > (anonyme) > act > progressTendWork > rollCraftMiss` |
| 386 | `updateNpc` | `npc-0` | `rollCraftMiss src/sim/craftMiss.js:79` | `updateNpc > measure > measure > (anonyme) > act > progressTendWork > rollCraftMiss` |
| 422 | `updateNpc` | `npc-0` | `rollCraftMiss src/sim/craftMiss.js:79` | `updateNpc > measure > measure > (anonyme) > act > progressTendWork > rollCraftMiss` |
| 431 | `updateNpc` | `npc-2` | `(anonyme) src/sim/npc.js:893` | `updateNpc > measure > measure > (anonyme)` |
| 453 | `updateNpc` | `npc-0` | `(anonyme) src/sim/npc.js:893` | `updateNpc > measure > measure > (anonyme)` |
| 453 | `updateNpc` | `npc-1` | `(anonyme) src/sim/npc.js:893` | `updateNpc > measure > measure > (anonyme)` |
| 453 | `updateNpc` | `npc-3` | `(anonyme) src/sim/npc.js:893` | `updateNpc > measure > measure > (anonyme)` |
| 453 | `updateNpc` | `npc-4` | `(anonyme) src/sim/npc.js:893` | `updateNpc > measure > measure > (anonyme)` |
| 487 | `updateNpc` | `npc-3` | `rollCraftMiss src/sim/craftMiss.js:79` | `updateNpc > measure > measure > (anonyme) > act > progressCraftGather > rollCraftMiss` |
| 511 | `updateNpc` | `npc-1` | `rollCraftMiss src/sim/craftMiss.js:79` | `updateNpc > measure > measure > (anonyme) > act > progressTendWork > rollCraftMiss` |
| 516 | `updateNpc` | `npc-3` | `rollCraftMiss src/sim/craftMiss.js:79` | `updateNpc > measure > measure > (anonyme) > act > progressCraftGather > rollCraftMiss` |
| 519 | `updateNpc` | `npc-4` | `rollCraftMiss src/sim/craftMiss.js:79` | `updateNpc > measure > measure > (anonyme) > act > progressCraftGather > rollCraftMiss` |
| 531 | `updateNpc` | `npc-2` | `rollCraftMiss src/sim/craftMiss.js:79` | `updateNpc > measure > measure > (anonyme) > act > progressCraftGather > rollCraftMiss` |
| 536 | `updateNpc` | `npc-2` | `(anonyme) src/sim/npc.js:893` | `updateNpc > measure > measure > (anonyme)` |
| 548 | `updateNpc` | `npc-4` | `rollCraftMiss src/sim/craftMiss.js:79` | `updateNpc > measure > measure > (anonyme) > act > progressCraftGather > rollCraftMiss` |
| 549 | `updateNpc` | `npc-1` | `rollCraftMiss src/sim/craftMiss.js:79` | `updateNpc > measure > measure > (anonyme) > act > progressTendWork > rollCraftMiss` |
| 552 | `updateNpc` | `npc-0` | `createInformResourceSpotActs src/life/speechActs.js:62` | `updateNpc > measure > measure > (anonyme) > act > perform > socialize > createInformResourceSpotActs` |
| 552 | `updateNpc` | `npc-0` | `createInformResourceSpotActs src/life/speechActs.js:62` | `updateNpc > measure > measure > (anonyme) > act > perform > socialize > createInformResourceSpotActs` |
| 562 | `updateNpc` | `npc-2` | `rollCraftMiss src/sim/craftMiss.js:79` | `updateNpc > measure > measure > (anonyme) > act > progressCraftGather > rollCraftMiss` |
| 586 | `updateNpc` | `npc-1` | `(anonyme) src/sim/npc.js:893` | `updateNpc > measure > measure > (anonyme)` |
| 586 | `updateNpc` | `npc-3` | `(anonyme) src/sim/npc.js:893` | `updateNpc > measure > measure > (anonyme)` |
| 586 | `updateNpc` | `npc-4` | `(anonyme) src/sim/npc.js:893` | `updateNpc > measure > measure > (anonyme)` |
| 598 | `updateNpc` | `npc-1` | `rollCraftMiss src/sim/craftMiss.js:79` | `updateNpc > measure > measure > (anonyme) > act > progressTendWork > rollCraftMiss` |
