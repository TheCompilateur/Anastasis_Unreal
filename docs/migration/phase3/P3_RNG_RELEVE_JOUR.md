# Relevé des tirages `sim.rng` — scénario `endurance`, 5400 ticks

Généré par `tools/migration/trace-sim-rng.mjs` (mission sim-rng-001). Ne pas éditer à la main : relancer.

```bash
node tools/migration/trace-sim-rng.mjs -ref <clone anastasis-ref-p3> -ticks 5400
```

- Référence : `fee66ae` (tag `anastasis-ref-p3`), scénario `endurance` (graine 12345, dt 0.016666666666666666, vue (54, 57)), 24 masques, tick recomposé.
- État `sim.rng` : 2576143622 au départ, 980029788 après 5400 ticks.
- **1854 tirages**, dont 1714 dans `chooseGoal`, en **104 décisions**.
- Tirages bruts : `docs/migration/phase3/P3_RNG_RELEVE_5400.csv` (un par ligne : tick, étape, habitant, décision, état avant, site, appelant, chemin).

## Premier tirage au tick 32 ou après

- tick 32, étape `updateNpc`, habitant `npc-2`, état avant 2576143622
- site : `exploreTarget src/ai/memory.js:669`
- chemin : `updateNpc > measure > measure > (anonyme) > chooseGoal > scoreGoals > adultScores > failureTargetBiasMap > failureCauseForGoal > exploreTarget`
- 16 tirage(s) à ce tick : exploreTarget x2, goalNoise x14

## Par site (fonction et ligne de la référence qui appelle `sim.rng`)

| Tirages | Site |
| ---: | --- |
| 1456 | `goalNoise src/sim/npc.js:491` |
| 127 | `exploreTarget src/ai/memory.js:669` |
| 127 | `exploreTarget src/ai/memory.js:670` |
| 75 | `(anonyme) src/sim/npc.js:893` |
| 43 | `rollCraftMiss src/sim/craftMiss.js:79` |
| 10 | `createInformResourceSpotActs src/life/speechActs.js:62` |
| 8 | `tellPerson src/ai/socialMemory.js:449` |
| 5 | `assignDayIntent src/ai/dayIntent.js:305` |
| 2 | `maybeChatOnHaul src/sim/npc.js:5505` |
| 1 | `refreshColonyStockReport src/sim/colonyStockReport.js:149` |

## Par étape du tick

| Tirages | Étape |
| ---: | --- |
| 1854 | `updateNpc` |

## Tirages par décision (`chooseGoal`)

| Tirages | Décisions |
| ---: | ---: |
| 16 | 82 |
| 17 | 2 |
| 18 | 16 |
| 19 | 2 |
| 20 | 1 |
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
| 669 | `npc-2` | 386990324 | 16 | exploreTarget x2, goalNoise x14 |
| 719 | `npc-3` | 3858298359 | 16 | exploreTarget x2, goalNoise x14 |
| 802 | `npc-2` | 2402803428 | 16 | exploreTarget x2, goalNoise x14 |
| 903 | `npc-0` | 1011249694 | 16 | exploreTarget x2, goalNoise x14 |
| 903 | `npc-1` | 2083097443 | 16 | exploreTarget x2, goalNoise x14 |
| 903 | `npc-3` | 1323379379 | 16 | exploreTarget x2, goalNoise x14 |
| 903 | `npc-4` | 563661315 | 18 | exploreTarget x4, goalNoise x14 |
| 935 | `npc-2` | 1003673394 | 16 | exploreTarget x2, goalNoise x14 |
| 1036 | `npc-0` | 1443685473 | 16 | exploreTarget x2, goalNoise x14 |
| 1036 | `npc-1` | 2515533222 | 16 | exploreTarget x2, goalNoise x14 |
| 1036 | `npc-3` | 1755815158 | 16 | exploreTarget x2, goalNoise x14 |
| 1036 | `npc-4` | 996097094 | 16 | exploreTarget x2, goalNoise x14 |
| 1119 | `npc-2` | 3899510656 | 18 | exploreTarget x4, goalNoise x14 |
| 1169 | `npc-3` | 1244285582 | 16 | exploreTarget x2, goalNoise x14 |
| 1169 | `npc-4` | 484567518 | 16 | exploreTarget x2, goalNoise x14 |
| 1252 | `npc-2` | 1556415267 | 16 | exploreTarget x2, goalNoise x14 |
| 1302 | `npc-0` | 164861533 | 18 | exploreTarget x4, goalNoise x14 |
| 1302 | `npc-3` | 604873612 | 16 | exploreTarget x2, goalNoise x14 |
| 1385 | `npc-2` | 3508287174 | 18 | exploreTarget x4, goalNoise x14 |
| 1486 | `npc-4` | 1484897770 | 18 | exploreTarget x4, goalNoise x14 |
| 1494 | `npc-3` | 93344036 | 16 | exploreTarget x2, goalNoise x14 |
| 1568 | `npc-0` | 2996757598 | 16 | exploreTarget x2, goalNoise x14 |
| 1568 | `npc-1` | 4068605347 | 16 | exploreTarget x2, goalNoise x14 |
| 1632 | `npc-4` | 3308887283 | 16 | exploreTarget x2, goalNoise x14 |
| 1670 | `npc-0` | 1917333549 | 16 | exploreTarget x2, goalNoise x14 |
| 1670 | `npc-1` | 2989181298 | 16 | exploreTarget x2, goalNoise x14 |
| 1677 | `npc-3` | 2229463234 | 16 | exploreTarget x2, goalNoise x14 |
| 1700 | `npc-4` | 3301310983 | 16 | exploreTarget x2, goalNoise x14 |
| 1713 | `npc-2` | 78191436 | 16 | exploreTarget x2, goalNoise x14 |
| 1803 | `npc-0` | 3549499471 | 16 | exploreTarget x2, goalNoise x14 |
| 1803 | `npc-1` | 326379924 | 16 | exploreTarget x2, goalNoise x14 |
| 1836 | `npc-4` | 3861629156 | 16 | exploreTarget x2, goalNoise x14 |
| 1860 | `npc-3` | 1838239752 | 16 | exploreTarget x2, goalNoise x14 |
| 1896 | `npc-2` | 2910087501 | 16 | exploreTarget x2, goalNoise x14 |
| 1904 | `npc-4` | 2150369437 | 16 | exploreTarget x2, goalNoise x14 |
| 1972 | `npc-4` | 1958545846 | 18 | exploreTarget x4, goalNoise x14 |
| 1983 | `npc-0` | 2398557925 | 16 | exploreTarget x2, goalNoise x14 |
| 2040 | `npc-4` | 3470405674 | 16 | exploreTarget x2, goalNoise x14 |
| 2043 | `npc-3` | 2710687610 | 16 | exploreTarget x2, goalNoise x14 |
| 2079 | `npc-2` | 3782535359 | 16 | exploreTarget x2, goalNoise x14 |
| 2108 | `npc-4` | 3022817295 | 16 | exploreTarget x2, goalNoise x14 |
| 2116 | `npc-0` | 2263099231 | 16 | exploreTarget x2, goalNoise x14 |
| 2116 | `npc-1` | 1503381167 | 16 | exploreTarget x2, goalNoise x14 |
| 2176 | `npc-4` | 2575228916 | 18 | exploreTarget x4, goalNoise x14 |
| 2226 | `npc-3` | 1183675182 | 16 | exploreTarget x2, goalNoise x14 |
| 2244 | `npc-4` | 423957118 | 16 | exploreTarget x2, goalNoise x14 |
| 2249 | `npc-0` | 3959206350 | 16 | exploreTarget x2, goalNoise x14 |
| 2262 | `npc-2` | 736086803 | 16 | exploreTarget x2, goalNoise x14 |
| 2312 | `npc-4` | 4271336035 | 16 | exploreTarget x2, goalNoise x14 |
| 2380 | `npc-4` | 1048216488 | 16 | exploreTarget x2, goalNoise x14 |
| 2382 | `npc-0` | 288498424 | 16 | exploreTarget x2, goalNoise x14 |
| 2409 | `npc-3` | 3823747656 | 16 | exploreTarget x2, goalNoise x14 |
| 2445 | `npc-2` | 600628109 | 16 | exploreTarget x2, goalNoise x14 |
| 2448 | `npc-4` | 4135877341 | 16 | exploreTarget x2, goalNoise x14 |
| 2456 | `npc-1` | 3376159277 | 16 | exploreTarget x2, goalNoise x14 |
| 2458 | `npc-0` | 2616441213 | 16 | exploreTarget x2, goalNoise x14 |
| 2458 | `npc-1` | 3688288962 | 16 | exploreTarget x2, goalNoise x14 |
| 2470 | `npc-3` | 465169415 | 16 | exploreTarget x2, goalNoise x14 |
| 2603 | `npc-4` | 4000418647 | 18 | exploreTarget x4, goalNoise x14 |
| 2653 | `npc-3` | 145463430 | 16 | exploreTarget x2, goalNoise x14 |
| 2675 | `npc-2` | 3680712662 | 16 | exploreTarget x2, goalNoise x14 |
| 2760 | `npc-4` | 2920994598 | 16 | exploreTarget x2, goalNoise x14 |
| 2836 | `npc-3` | 3992842347 | 16 | exploreTarget x2, goalNoise x14 |
| 3066 | `npc-3` | 3233124283 | 16 | exploreTarget x2, goalNoise x14 |
| 3314 | `npc-1` | 2473406219 | 18 | refreshColonyStockReport, assignDayIntent, exploreTarget x2, goalNoise x14 |
| 3329 | `npc-0` | 1081852485 | 17 | assignDayIntent, exploreTarget x2, goalNoise x14 |
| 3396 | `npc-2` | 2153700234 | 19 | assignDayIntent, exploreTarget x4, goalNoise x14 |
| 3607 | `npc-4` | 2593712313 | 17 | assignDayIntent, exploreTarget x2, goalNoise x14 |
| 3679 | `npc-3` | 3665560062 | 19 | assignDayIntent, exploreTarget x4, goalNoise x14 |
| 4117 | `npc-2` | 4105572141 | 16 | exploreTarget x2, goalNoise x14 |
| 4161 | `npc-1` | 3345854077 | 18 | exploreTarget x4, goalNoise x14 |
| 4176 | `npc-0` | 1954300343 | 20 | exploreTarget x6, goalNoise x14 |
| 4292 | `npc-3` | 4225878235 | 18 | exploreTarget x4, goalNoise x14 |
| 4454 | `npc-4` | 2834324501 | 16 | exploreTarget x2, goalNoise x14 |
| 4488 | `npc-3` | 2074606437 | 16 | exploreTarget x2, goalNoise x14 |
| 4651 | `npc-4` | 1314888373 | 18 | exploreTarget x4, goalNoise x14 |
| 4659 | `npc-3` | 4218301935 | 16 | exploreTarget x2, goalNoise x14 |
| 4808 | `npc-4` | 995182388 | 16 | exploreTarget x2, goalNoise x14 |
| 4838 | `npc-2` | 235464324 | 16 | exploreTarget x2, goalNoise x14 |
| 4841 | `npc-3` | 3770713556 | 16 | exploreTarget x2, goalNoise x14 |
| 4953 | `npc-2` | 547594009 | 16 | exploreTarget x2, goalNoise x14 |
| 4974 | `npc-3` | 1619441758 | 16 | exploreTarget x2, goalNoise x14 |
| 5008 | `npc-1` | 859723694 | 16 | exploreTarget x2, goalNoise x14 |
| 5023 | `npc-0` | 100005630 | 16 | exploreTarget x2, goalNoise x14 |

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
| 636 | `updateNpc` | `npc-1` | `rollCraftMiss src/sim/craftMiss.js:79` | `updateNpc > measure > measure > (anonyme) > act > progressTendWork > rollCraftMiss` |
| 657 | `updateNpc` | `npc-1` | `rollCraftMiss src/sim/craftMiss.js:79` | `updateNpc > measure > measure > (anonyme) > act > progressTendWork > rollCraftMiss` |
| 669 | `updateNpc` | `npc-2` | `(anonyme) src/sim/npc.js:893` | `updateNpc > measure > measure > (anonyme)` |
| 672 | `updateNpc` | `npc-0` | `createInformResourceSpotActs src/life/speechActs.js:62` | `updateNpc > measure > measure > (anonyme) > act > perform > socialize > createInformResourceSpotActs` |
| 672 | `updateNpc` | `npc-0` | `createInformResourceSpotActs src/life/speechActs.js:62` | `updateNpc > measure > measure > (anonyme) > act > perform > socialize > createInformResourceSpotActs` |
| 672 | `updateNpc` | `npc-0` | `tellPerson src/ai/socialMemory.js:449` | `updateNpc > measure > measure > (anonyme) > act > perform > socialize > sim.shareRumors > spreadRumorExchange > sharePeopleBeliefs > tellPerson` |
| 672 | `updateNpc` | `npc-0` | `tellPerson src/ai/socialMemory.js:449` | `updateNpc > measure > measure > (anonyme) > act > perform > socialize > sim.shareRumors > spreadRumorExchange > sharePeopleBeliefs > tellPerson` |
| 695 | `updateNpc` | `npc-1` | `rollCraftMiss src/sim/craftMiss.js:79` | `updateNpc > measure > measure > (anonyme) > act > progressTendWork > rollCraftMiss` |
| 719 | `updateNpc` | `npc-1` | `(anonyme) src/sim/npc.js:893` | `updateNpc > measure > measure > (anonyme)` |
| 719 | `updateNpc` | `npc-3` | `(anonyme) src/sim/npc.js:893` | `updateNpc > measure > measure > (anonyme)` |
| 719 | `updateNpc` | `npc-4` | `(anonyme) src/sim/npc.js:893` | `updateNpc > measure > measure > (anonyme)` |
| 733 | `updateNpc` | `npc-1` | `rollCraftMiss src/sim/craftMiss.js:79` | `updateNpc > measure > measure > (anonyme) > act > progressTendWork > rollCraftMiss` |
| 739 | `updateNpc` | `npc-0` | `(anonyme) src/sim/npc.js:893` | `updateNpc > measure > measure > (anonyme)` |
| 770 | `updateNpc` | `npc-0` | `(anonyme) src/sim/npc.js:893` | `updateNpc > measure > measure > (anonyme)` |
| 770 | `updateNpc` | `npc-1` | `(anonyme) src/sim/npc.js:893` | `updateNpc > measure > measure > (anonyme)` |
| 770 | `updateNpc` | `npc-3` | `(anonyme) src/sim/npc.js:893` | `updateNpc > measure > measure > (anonyme)` |
| 770 | `updateNpc` | `npc-4` | `(anonyme) src/sim/npc.js:893` | `updateNpc > measure > measure > (anonyme)` |
| 771 | `updateNpc` | `npc-1` | `rollCraftMiss src/sim/craftMiss.js:79` | `updateNpc > measure > measure > (anonyme) > act > progressTendWork > rollCraftMiss` |
| 792 | `updateNpc` | `npc-1` | `rollCraftMiss src/sim/craftMiss.js:79` | `updateNpc > measure > measure > (anonyme) > act > progressTendWork > rollCraftMiss` |
| 830 | `updateNpc` | `npc-1` | `rollCraftMiss src/sim/craftMiss.js:79` | `updateNpc > measure > measure > (anonyme) > act > progressTendWork > rollCraftMiss` |
| 868 | `updateNpc` | `npc-1` | `rollCraftMiss src/sim/craftMiss.js:79` | `updateNpc > measure > measure > (anonyme) > act > progressTendWork > rollCraftMiss` |
| 903 | `updateNpc` | `npc-1` | `(anonyme) src/sim/npc.js:893` | `updateNpc > measure > measure > (anonyme)` |
| 935 | `updateNpc` | `npc-2` | `(anonyme) src/sim/npc.js:893` | `updateNpc > measure > measure > (anonyme)` |
| 986 | `updateNpc` | `npc-2` | `(anonyme) src/sim/npc.js:893` | `updateNpc > measure > measure > (anonyme)` |
| 1014 | `updateNpc` | `npc-0` | `rollCraftMiss src/sim/craftMiss.js:79` | `updateNpc > measure > measure > (anonyme) > act > progressCraftGather > rollCraftMiss` |
| 1036 | `updateNpc` | `npc-0` | `(anonyme) src/sim/npc.js:893` | `updateNpc > measure > measure > (anonyme)` |
| 1036 | `updateNpc` | `npc-1` | `(anonyme) src/sim/npc.js:893` | `updateNpc > measure > measure > (anonyme)` |
| 1055 | `updateNpc` | `npc-0` | `rollCraftMiss src/sim/craftMiss.js:79` | `updateNpc > measure > measure > (anonyme) > act > progressCraftGather > rollCraftMiss` |
| 1096 | `updateNpc` | `npc-0` | `rollCraftMiss src/sim/craftMiss.js:79` | `updateNpc > measure > measure > (anonyme) > act > progressCraftGather > rollCraftMiss` |
| 1137 | `updateNpc` | `npc-0` | `rollCraftMiss src/sim/craftMiss.js:79` | `updateNpc > measure > measure > (anonyme) > act > progressCraftGather > rollCraftMiss` |
| 1165 | `updateNpc` | `npc-0` | `rollCraftMiss src/sim/craftMiss.js:79` | `updateNpc > measure > measure > (anonyme) > act > progressCraftGather > rollCraftMiss` |
| 1169 | `updateNpc` | `npc-0` | `(anonyme) src/sim/npc.js:893` | `updateNpc > measure > measure > (anonyme)` |
| 1169 | `updateNpc` | `npc-1` | `(anonyme) src/sim/npc.js:893` | `updateNpc > measure > measure > (anonyme)` |
| 1240 | `updateNpc` | `npc-1` | `rollCraftMiss src/sim/craftMiss.js:79` | `updateNpc > measure > measure > (anonyme) > act > progressCraftGather > rollCraftMiss` |
| 1272 | `updateNpc` | `npc-1` | `rollCraftMiss src/sim/craftMiss.js:79` | `updateNpc > measure > measure > (anonyme) > act > progressCraftGather > rollCraftMiss` |
| 1302 | `updateNpc` | `npc-0` | `(anonyme) src/sim/npc.js:893` | `updateNpc > measure > measure > (anonyme)` |
| 1302 | `updateNpc` | `npc-1` | `(anonyme) src/sim/npc.js:893` | `updateNpc > measure > measure > (anonyme)` |
| 1302 | `updateNpc` | `npc-4` | `(anonyme) src/sim/npc.js:893` | `updateNpc > measure > measure > (anonyme)` |
| 1385 | `updateNpc` | `npc-2` | `(anonyme) src/sim/npc.js:893` | `updateNpc > measure > measure > (anonyme)` |
| 1435 | `updateNpc` | `npc-0` | `(anonyme) src/sim/npc.js:893` | `updateNpc > measure > measure > (anonyme)` |
| 1435 | `updateNpc` | `npc-1` | `(anonyme) src/sim/npc.js:893` | `updateNpc > measure > measure > (anonyme)` |
| 1530 | `updateNpc` | `npc-2` | `(anonyme) src/sim/npc.js:893` | `updateNpc > measure > measure > (anonyme)` |
| 1568 | `updateNpc` | `npc-0` | `(anonyme) src/sim/npc.js:893` | `updateNpc > measure > measure > (anonyme)` |
| 1568 | `updateNpc` | `npc-1` | `(anonyme) src/sim/npc.js:893` | `updateNpc > measure > measure > (anonyme)` |
| 1644 | `updateNpc` | `npc-1` | `rollCraftMiss src/sim/craftMiss.js:79` | `updateNpc > measure > measure > (anonyme) > act > progressCraftGather > rollCraftMiss` |
| 1670 | `updateNpc` | `npc-0` | `(anonyme) src/sim/npc.js:893` | `updateNpc > measure > measure > (anonyme)` |
| 1670 | `updateNpc` | `npc-1` | `(anonyme) src/sim/npc.js:893` | `updateNpc > measure > measure > (anonyme)` |
| 1688 | `updateNpc` | `npc-1` | `rollCraftMiss src/sim/craftMiss.js:79` | `updateNpc > measure > measure > (anonyme) > act > progressCraftGather > rollCraftMiss` |
| 1713 | `updateNpc` | `npc-2` | `(anonyme) src/sim/npc.js:893` | `updateNpc > measure > measure > (anonyme)` |
| 1732 | `updateNpc` | `npc-1` | `rollCraftMiss src/sim/craftMiss.js:79` | `updateNpc > measure > measure > (anonyme) > act > progressCraftGather > rollCraftMiss` |
| 1768 | `updateNpc` | `npc-4` | `(anonyme) src/sim/npc.js:893` | `updateNpc > measure > measure > (anonyme)` |
| 1776 | `updateNpc` | `npc-1` | `rollCraftMiss src/sim/craftMiss.js:79` | `updateNpc > measure > measure > (anonyme) > act > progressCraftGather > rollCraftMiss` |
| 1796 | `updateNpc` | `npc-0` | `createInformResourceSpotActs src/life/speechActs.js:62` | `updateNpc > measure > measure > (anonyme) > act > perform > socialize > createInformResourceSpotActs` |
| 1796 | `updateNpc` | `npc-0` | `createInformResourceSpotActs src/life/speechActs.js:62` | `updateNpc > measure > measure > (anonyme) > act > perform > socialize > createInformResourceSpotActs` |
| 1796 | `updateNpc` | `npc-0` | `tellPerson src/ai/socialMemory.js:449` | `updateNpc > measure > measure > (anonyme) > act > perform > socialize > sim.shareRumors > spreadRumorExchange > sharePeopleBeliefs > tellPerson` |
| 1796 | `updateNpc` | `npc-0` | `tellPerson src/ai/socialMemory.js:449` | `updateNpc > measure > measure > (anonyme) > act > perform > socialize > sim.shareRumors > spreadRumorExchange > sharePeopleBeliefs > tellPerson` |
| 1803 | `updateNpc` | `npc-1` | `(anonyme) src/sim/npc.js:893` | `updateNpc > measure > measure > (anonyme)` |
| 1856 | `updateNpc` | `npc-0` | `createInformResourceSpotActs src/life/speechActs.js:62` | `updateNpc > measure > measure > (anonyme) > act > perform > socialize > createInformResourceSpotActs` |
| 1856 | `updateNpc` | `npc-0` | `createInformResourceSpotActs src/life/speechActs.js:62` | `updateNpc > measure > measure > (anonyme) > act > perform > socialize > createInformResourceSpotActs` |
| 1856 | `updateNpc` | `npc-0` | `tellPerson src/ai/socialMemory.js:449` | `updateNpc > measure > measure > (anonyme) > act > perform > socialize > sim.shareRumors > spreadRumorExchange > sharePeopleBeliefs > tellPerson` |
| 1856 | `updateNpc` | `npc-0` | `tellPerson src/ai/socialMemory.js:449` | `updateNpc > measure > measure > (anonyme) > act > perform > socialize > sim.shareRumors > spreadRumorExchange > sharePeopleBeliefs > tellPerson` |
| 1896 | `updateNpc` | `npc-2` | `(anonyme) src/sim/npc.js:893` | `updateNpc > measure > measure > (anonyme)` |
| 1916 | `updateNpc` | `npc-0` | `createInformResourceSpotActs src/life/speechActs.js:62` | `updateNpc > measure > measure > (anonyme) > act > perform > socialize > createInformResourceSpotActs` |
| 1916 | `updateNpc` | `npc-0` | `createInformResourceSpotActs src/life/speechActs.js:62` | `updateNpc > measure > measure > (anonyme) > act > perform > socialize > createInformResourceSpotActs` |
| 1916 | `updateNpc` | `npc-0` | `tellPerson src/ai/socialMemory.js:449` | `updateNpc > measure > measure > (anonyme) > act > perform > socialize > sim.shareRumors > spreadRumorExchange > sharePeopleBeliefs > tellPerson` |
| 1916 | `updateNpc` | `npc-0` | `tellPerson src/ai/socialMemory.js:449` | `updateNpc > measure > measure > (anonyme) > act > perform > socialize > sim.shareRumors > spreadRumorExchange > sharePeopleBeliefs > tellPerson` |
| 1972 | `updateNpc` | `npc-4` | `(anonyme) src/sim/npc.js:893` | `updateNpc > measure > measure > (anonyme)` |
| 1983 | `updateNpc` | `npc-0` | `(anonyme) src/sim/npc.js:893` | `updateNpc > measure > measure > (anonyme)` |
| 1983 | `updateNpc` | `npc-1` | `(anonyme) src/sim/npc.js:893` | `updateNpc > measure > measure > (anonyme)` |
| 2079 | `updateNpc` | `npc-2` | `(anonyme) src/sim/npc.js:893` | `updateNpc > measure > measure > (anonyme)` |
| 2176 | `updateNpc` | `npc-4` | `(anonyme) src/sim/npc.js:893` | `updateNpc > measure > measure > (anonyme)` |
| 2262 | `updateNpc` | `npc-2` | `(anonyme) src/sim/npc.js:893` | `updateNpc > measure > measure > (anonyme)` |
| 2380 | `updateNpc` | `npc-4` | `(anonyme) src/sim/npc.js:893` | `updateNpc > measure > measure > (anonyme)` |
| 2445 | `updateNpc` | `npc-2` | `(anonyme) src/sim/npc.js:893` | `updateNpc > measure > measure > (anonyme)` |
| 2458 | `updateNpc` | `npc-1` | `(anonyme) src/sim/npc.js:893` | `updateNpc > measure > measure > (anonyme)` |
| 2470 | `updateNpc` | `npc-3` | `(anonyme) src/sim/npc.js:893` | `updateNpc > measure > measure > (anonyme)` |
| 2653 | `updateNpc` | `npc-3` | `(anonyme) src/sim/npc.js:893` | `updateNpc > measure > measure > (anonyme)` |
| 2836 | `updateNpc` | `npc-3` | `(anonyme) src/sim/npc.js:893` | `updateNpc > measure > measure > (anonyme)` |
| 4708 | `updateNpc` | `npc-3` | `(anonyme) src/sim/npc.js:893` | `updateNpc > measure > measure > (anonyme)` |
| 4941 | `updateNpc` | `npc-4` | `(anonyme) src/sim/npc.js:893` | `updateNpc > measure > measure > (anonyme)` |
| 4974 | `updateNpc` | `npc-3` | `(anonyme) src/sim/npc.js:893` | `updateNpc > measure > measure > (anonyme)` |
| 5043 | `updateNpc` | `npc-4` | `rollCraftMiss src/sim/craftMiss.js:79` | `updateNpc > measure > measure > (anonyme) > act > progressCraftGather > rollCraftMiss` |
| 5074 | `updateNpc` | `npc-4` | `(anonyme) src/sim/npc.js:893` | `updateNpc > measure > measure > (anonyme)` |
| 5086 | `updateNpc` | `npc-2` | `(anonyme) src/sim/npc.js:893` | `updateNpc > measure > measure > (anonyme)` |
| 5107 | `updateNpc` | `npc-3` | `(anonyme) src/sim/npc.js:893` | `updateNpc > measure > measure > (anonyme)` |
| 5112 | `updateNpc` | `npc-1` | `rollCraftMiss src/sim/craftMiss.js:79` | `updateNpc > measure > measure > (anonyme) > act > progressCraftGather > rollCraftMiss` |
| 5141 | `updateNpc` | `npc-1` | `(anonyme) src/sim/npc.js:893` | `updateNpc > measure > measure > (anonyme)` |
| 5156 | `updateNpc` | `npc-0` | `(anonyme) src/sim/npc.js:893` | `updateNpc > measure > measure > (anonyme)` |
| 5157 | `updateNpc` | `npc-0` | `rollCraftMiss src/sim/craftMiss.js:79` | `updateNpc > measure > measure > (anonyme) > act > progressCraftGather > rollCraftMiss` |
| 5179 | `updateNpc` | `npc-3` | `rollCraftMiss src/sim/craftMiss.js:79` | `updateNpc > measure > measure > (anonyme) > act > progressCraftGather > rollCraftMiss` |
| 5207 | `updateNpc` | `npc-4` | `(anonyme) src/sim/npc.js:893` | `updateNpc > measure > measure > (anonyme)` |
| 5219 | `updateNpc` | `npc-2` | `(anonyme) src/sim/npc.js:893` | `updateNpc > measure > measure > (anonyme)` |
| 5221 | `updateNpc` | `npc-3` | `rollCraftMiss src/sim/craftMiss.js:79` | `updateNpc > measure > measure > (anonyme) > act > progressCraftGather > rollCraftMiss` |
| 5240 | `updateNpc` | `npc-3` | `(anonyme) src/sim/npc.js:893` | `updateNpc > measure > measure > (anonyme)` |
| 5258 | `updateNpc` | `npc-2` | `rollCraftMiss src/sim/craftMiss.js:79` | `updateNpc > measure > measure > (anonyme) > act > progressCraftGather > rollCraftMiss` |
| 5274 | `updateNpc` | `npc-1` | `(anonyme) src/sim/npc.js:893` | `updateNpc > measure > measure > (anonyme)` |
| 5289 | `updateNpc` | `npc-0` | `(anonyme) src/sim/npc.js:893` | `updateNpc > measure > measure > (anonyme)` |
| 5302 | `updateNpc` | `npc-2` | `rollCraftMiss src/sim/craftMiss.js:79` | `updateNpc > measure > measure > (anonyme) > act > progressCraftGather > rollCraftMiss` |
| 5340 | `updateNpc` | `npc-4` | `(anonyme) src/sim/npc.js:893` | `updateNpc > measure > measure > (anonyme)` |
| 5346 | `updateNpc` | `npc-2` | `rollCraftMiss src/sim/craftMiss.js:79` | `updateNpc > measure > measure > (anonyme) > act > progressCraftGather > rollCraftMiss` |
| 5352 | `updateNpc` | `npc-2` | `(anonyme) src/sim/npc.js:893` | `updateNpc > measure > measure > (anonyme)` |
| 5373 | `updateNpc` | `npc-3` | `(anonyme) src/sim/npc.js:893` | `updateNpc > measure > measure > (anonyme)` |
| 5374 | `updateNpc` | `npc-2` | `rollCraftMiss src/sim/craftMiss.js:79` | `updateNpc > measure > measure > (anonyme) > act > progressCraftGather > rollCraftMiss` |
