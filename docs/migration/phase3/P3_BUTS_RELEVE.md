# Releve des buts — scenario `endurance`, 16200 ticks

Reference : clone du tag `anastasis-ref-p3`. Produit par `tools/migration/trace-goals.mjs`.

| But | Ticks-habitant | Choix |
|---|---|---|
| rest | 28204 | 19 |
| deliver | 22011 | 63 |
| socialize | 6551 | 12 |
| eat | 5478 | 16 |
| sell | 5325 | 20 |
| gatherStone | 4226 | 30 |
| gatherFood | 3618 | 27 |
| build | 1477 | 8 |
| drink | 1267 | 7 |
| gatherWood | 1055 | 4 |
| helpFarm | 972 | 4 |
| observer | 419 | 0 |
| relieve | 265 | 1 |
| explore | 132 | 1 |

## Changements de but

| Tick | Jour | Habitant | But | Activite | Porte | Cible |
|---|---|---|---|---|---|---|
| 32 | 1 | npc-2 | observer → **build** | chantier | f5 w0 s0 | (58.5, 55.5) |
| 63 | 1 | npc-0 | observer → **deliver** | livre | f5 w0 s0 | (49.5, 59.5) |
| 82 | 1 | npc-3 | observer → **build** | chantier | f5 w0 s0 | (59.5, 55.5) |
| 114 | 1 | npc-1 | observer → **deliver** | livre | f5 w0 s0 | (49.5, 59.5) |
| 133 | 1 | npc-4 | observer → **build** | chantier | f5 w0 s0 | (58.5, 54.5) |
| 196 | 1 | npc-0 | deliver → **helpFarm** | cherche | f0 w0 s0 | (49.3, 62.7) |
| 245 | 1 | npc-2 | build → **gatherWood** | chantier | f5 w0 s0 | (44.5, 59.5) |
| 247 | 1 | npc-3 | build → **gatherWood** | chantier | f5 w0 s0 | (44.5, 59.5) |
| 299 | 1 | npc-4 | build → **gatherWood** | chantier | f5 w0 s0 | (44.5, 59.5) |
| 321 | 1 | npc-1 | deliver → **explore** | attend | f0 w0 s0 | (76.0, 68.0) |
| 453 | 1 | npc-0 | helpFarm → **socialize** | socialise | f0 w0 s0 | (49.5, 58.5) |
| 453 | 1 | npc-1 | explore → **helpFarm** | cherche | f0 w0 s0 | (52.2, 64.5) |
| 516 | 1 | npc-3 | gatherWood → **sell** | bucher | f5 w6 s0 | (58.5, 55.5) |
| 548 | 1 | npc-4 | gatherWood → **sell** | bucher | f5 w6 s0 | (58.5, 54.5) |
| 562 | 1 | npc-2 | gatherWood → **sell** | bucher | f5 w6 s0 | (59.5, 55.5) |
| 903 | 1 | npc-0 | socialize → **gatherStone** | cherche | f0 w0 s0 | (49.5, 65.5) |
| 903 | 1 | npc-1 | helpFarm → **build** | chantier | f0 w0 s0 | (59.5, 55.5) |
| 1052 | 1 | npc-1 | build → **gatherWood** | chantier | f0 w0 s0 | (58.5, 68.5) |
| 1165 | 1 | npc-0 | gatherStone → **sell** | taille | f0 w0 s10 | (60.5, 55.5) |
| 1262 | 1 | npc-2 | sell → **deliver** | attend | f1 w6 s0 | (51.5, 57.5) |
| 1272 | 1 | npc-1 | gatherWood → **sell** | bucher | f0 w6 s0 | (60.5, 54.5) |
| 1289 | 1 | npc-4 | sell → **deliver** | attend | f1 w6 s0 | (50.5, 57.5) |
| 1319 | 1 | npc-3 | sell → **deliver** | attend | f1 w6 s0 | (51.5, 57.5) |
| 1383 | 1 | npc-0 | sell → **deliver** | attend | f0 w0 s10 | (49.5, 59.5) |
| 1506 | 1 | npc-1 | sell → **deliver** | attend | f0 w6 s0 | (48.5, 59.5) |
| 1568 | 1 | npc-0 | deliver → **gatherStone** | cherche | f0 w0 s10 | (50.5, 65.5) |
| 1568 | 1 | npc-1 | deliver → **gatherStone** | cherche | f0 w6 s0 | (61.5, 60.5) |
| 1670 | 1 | npc-0 | gatherStone → **socialize** | socialise | f0 w0 s10 | (49.5, 58.5) |
| 1776 | 1 | npc-1 | gatherStone → **deliver** | taille | f0 w6 s4 | (48.5, 59.5) |
| 2116 | 1 | npc-1 | deliver → **rest** | repose | f0 w6 s4 | (50.5, 57.5) |
| 2445 | 1 | npc-2 | deliver → **eat** | attend | f1 w6 s0 | - |
| 2448 | 1 | npc-4 | deliver → **eat** | attend | f1 w6 s0 | - |
| 2456 | 1 | npc-1 | rest → **socialize** | socialise | f0 w6 s4 | (50.5, 57.5) |
| 2458 | 1 | npc-0 | socialize → **rest** | repose | f0 w0 s10 | (50.5, 57.5) |
| 2458 | 1 | npc-1 | socialize → **rest** | repose | f0 w6 s4 | (51.5, 57.5) |
| 2675 | 1 | npc-2 | eat → **rest** | attend | f0 w6 s0 | - |
| 2760 | 1 | npc-4 | eat → **rest** | attend | f0 w6 s0 | - |
| 2836 | 1 | npc-3 | deliver → **eat** | attend | f1 w6 s0 | - |
| 3066 | 1 | npc-3 | eat → **rest** | attend | f0 w6 s0 | - |
| 4292 | 2 | npc-3 | rest → **eat** | mange | f0 w6 s0 | (49.5, 59.5) |
| 4454 | 2 | npc-4 | rest → **eat** | mange | f0 w6 s0 | (49.5, 59.5) |
| 4659 | 2 | npc-3 | eat → **drink** | boit | f0 w6 s0 | (49.5, 58.5) |
| 4808 | 2 | npc-4 | eat → **build** | chantier | f0 w6 s0 | (58.5, 55.5) |
| 4838 | 2 | npc-2 | rest → **eat** | repose | f0 w6 s0 | - |
| 4841 | 2 | npc-3 | drink → **build** | chantier | f0 w6 s0 | (59.5, 55.5) |
| 4937 | 2 | npc-4 | build → **gatherStone** | chantier | f0 w6 s0 | (61.5, 60.5) |
| 4953 | 2 | npc-2 | eat → **build** | chantier | f0 w6 s0 | (58.5, 55.5) |
| 4985 | 2 | npc-3 | build → **gatherStone** | chantier | f0 w6 s0 | (46.5, 52.5) |
| 5008 | 2 | npc-1 | rest → **gatherStone** | cherche | f0 w6 s4 | (46.5, 52.5) |
| 5023 | 2 | npc-0 | rest → **gatherStone** | cherche | f0 w0 s10 | (46.5, 52.5) |
| 5112 | 2 | npc-1 | gatherStone → **sell** | taille | f0 w6 s5 | (58.5, 54.5) |
| 5114 | 2 | npc-2 | build → **gatherStone** | chantier | f0 w6 s0 | (62.5, 60.5) |
| 5157 | 2 | npc-0 | gatherStone → **sell** | taille | f0 w0 s12 | (58.5, 55.5) |
| 5221 | 2 | npc-3 | gatherStone → **sell** | taille | f0 w6 s4 | (59.5, 55.5) |
| 5226 | 2 | npc-4 | gatherStone → **deliver** | taille | f0 w6 s5 | (51.5, 57.5) |
| 5312 | 2 | npc-1 | sell → **deliver** | attend | f0 w6 s5 | (49.5, 59.5) |
| 5360 | 2 | npc-0 | sell → **deliver** | attend | f0 w0 s12 | (48.5, 59.5) |
| 5374 | 2 | npc-2 | gatherStone → **sell** | taille | f0 w6 s4 | (60.5, 55.5) |
| 5406 | 2 | npc-3 | sell → **deliver** | attend | f0 w6 s4 | (51.5, 57.5) |
| 5500 | 2 | npc-2 | sell → **deliver** | attend | f0 w6 s4 | (50.5, 57.5) |
| 5505 | 2 | npc-3 | deliver → **build** | chantier | f0 w6 s4 | (58.5, 55.5) |
| 5671 | 2 | npc-1 | deliver → **gatherStone** | cherche | f0 w6 s5 | (50.5, 65.5) |
| 5720 | 2 | npc-0 | deliver → **eat** | mange | f0 w0 s12 | (50.5, 57.5) |
| 5720 | 2 | npc-1 | gatherStone → **eat** | mange | f0 w6 s5 | (51.5, 57.5) |
| 5855 | 2 | npc-3 | build → **socialize** | socialise | f0 w0 s0 | (49.5, 56.5) |
| 5912 | 2 | npc-0 | eat → **gatherFood** | cherche | f0 w0 s12 | (51.5, 64.5) |
| 5958 | 2 | npc-1 | eat → **gatherFood** | cherche | f0 w6 s5 | (51.5, 64.5) |
| 6029 | 2 | npc-0 | gatherFood → **deliver** | recolte | f3 w0 s12 | (48.5, 60.5) |
| 6053 | 2 | npc-1 | gatherFood → **deliver** | recolte | f2 w6 s5 | (49.5, 59.5) |
| 6170 | 2 | npc-0 | deliver → **gatherFood** | cherche | f0 w0 s12 | (51.5, 64.5) |
| 6170 | 2 | npc-1 | deliver → **gatherFood** | cherche | f0 w6 s5 | (51.5, 64.5) |
| 6170 | 2 | npc-3 | socialize → **gatherStone** | cherche | f0 w0 s0 | (46.5, 52.5) |
| 6269 | 2 | npc-1 | gatherFood → **deliver** | recolte | f2 w6 s5 | (48.5, 60.5) |
| 6401 | 2 | npc-0 | gatherFood → **deliver** | recolte | f3 w0 s12 | (49.5, 59.5) |
| 6414 | 2 | npc-3 | gatherStone → **deliver** | taille | f0 w0 s10 | (50.5, 57.5) |
| 6434 | 2 | npc-1 | deliver → **helpFarm** | cherche | f0 w6 s5 | (50.5, 62.2) |
| 6566 | 2 | npc-1 | helpFarm → **gatherFood** | cherche | f0 w6 s5 | (42.5, 67.5) |
| 6580 | 2 | npc-3 | deliver → **gatherStone** | cherche | f0 w0 s10 | (46.5, 52.5) |
| 6686 | 2 | npc-3 | gatherStone → **deliver** | taille | f0 w0 s12 | (50.5, 57.5) |
| 6694 | 2 | npc-1 | gatherFood → **deliver** | recolte | f2 w6 s5 | (48.5, 60.5) |
| 6698 | 2 | npc-0 | deliver → **gatherFood** | cherche | f0 w0 s12 | (49.5, 70.5) |
| 6830 | 2 | npc-1 | deliver → **gatherFood** | cherche | f0 w6 s5 | (42.5, 67.5) |
| 6856 | 2 | npc-3 | deliver → **gatherStone** | cherche | f0 w0 s12 | (46.5, 52.5) |
| 6874 | 2 | npc-0 | gatherFood → **deliver** | recolte | f3 w0 s12 | (48.5, 60.5) |
| 6957 | 2 | npc-1 | gatherFood → **deliver** | recolte | f2 w6 s5 | (48.5, 59.5) |
| 6958 | 2 | npc-3 | gatherStone → **deliver** | taille | f0 w0 s14 | (50.5, 57.5) |
| 7070 | 2 | npc-0 | deliver → **gatherFood** | cherche | f0 w0 s12 | (48.5, 70.5) |
| 7123 | 2 | npc-3 | deliver → **socialize** | socialise | f0 w0 s14 | (50.5, 57.5) |
| 7202 | 2 | npc-1 | deliver → **gatherFood** | cherche | f0 w6 s5 | (48.5, 71.5) |
| 7288 | 2 | npc-0 | gatherFood → **deliver** | recolte | f3 w0 s12 | (48.5, 60.5) |
| 7301 | 2 | npc-0 | deliver → **eat** | mange | f3 w0 s12 | (50.5, 57.5) |
| 7362 | 2 | npc-1 | gatherFood → **deliver** | recolte | f2 w6 s5 | (48.5, 60.5) |
| 7367 | 2 | npc-1 | deliver → **eat** | mange | f2 w6 s5 | (50.5, 57.5) |
| 7641 | 2 | npc-0 | eat → **deliver** | livre | f2 w0 s12 | (49.5, 59.5) |
| 7703 | 2 | npc-1 | eat → **deliver** | livre | f1 w6 s5 | (49.5, 59.5) |
| 7773 | 2 | npc-0 | deliver → **drink** | boit | f0 w0 s12 | (49.5, 58.5) |
| 7835 | 2 | npc-1 | deliver → **socialize** | socialise | f0 w6 s5 | (49.5, 58.5) |
| 7857 | 2 | npc-0 | drink → **rest** | repose | f0 w0 s12 | (50.5, 57.5) |
| 7857 | 2 | npc-1 | socialize → **drink** | boit | f0 w6 s5 | (49.5, 58.5) |
| 7989 | 2 | npc-1 | drink → **rest** | repose | f0 w6 s5 | (50.5, 57.5) |
| 8584 | 3 | npc-4 | deliver → **sell** | attend | f0 w6 s5 | (58.5, 55.5) |
| 8709 | 3 | npc-2 | deliver → **sell** | attend | f0 w6 s4 | (58.5, 55.5) |
| 8766 | 3 | npc-4 | sell → **deliver** | attend | f0 w6 s5 | (51.5, 57.5) |
| 8887 | 3 | npc-2 | sell → **deliver** | attend | f0 w6 s4 | (51.5, 57.5) |
| 9657 | 3 | npc-3 | socialize → **gatherFood** | cherche | f0 w0 s14 | (50.5, 63.5) |
| 9698 | 3 | npc-1 | rest → **socialize** | socialise | f0 w6 s5 | (50.5, 57.5) |
| 9758 | 3 | npc-3 | gatherFood → **sell** | recolte | f3 w0 s14 | (58.5, 55.5) |
| 9789 | 3 | npc-3 | sell → **eat** | mange | f3 w0 s14 | (49.5, 59.5) |
| 9808 | 3 | npc-2 | deliver → **eat** | attend | f0 w6 s4 | - |
| 10107 | 3 | npc-1 | socialize → **gatherStone** | cherche | f0 w6 s5 | (46.5, 52.5) |
| 10217 | 3 | npc-1 | gatherStone → **sell** | taille | f0 w6 s6 | (58.5, 55.5) |
| 10282 | 3 | npc-4 | deliver → **eat** | mange | f0 w6 s5 | (49.5, 59.5) |
| 10396 | 3 | npc-1 | sell → **deliver** | attend | f0 w6 s6 | (49.5, 59.5) |
| 10414 | 3 | npc-0 | rest → **gatherStone** | cherche | f0 w0 s12 | (46.5, 52.5) |
| 10414 | 3 | npc-3 | eat → **gatherStone** | cherche | f0 w0 s14 | (50.5, 65.5) |
| 10428 | 3 | npc-2 | eat → **drink** | boit | f0 w6 s4 | (49.5, 58.5) |
| 10512 | 3 | npc-3 | gatherStone → **sell** | taille | f0 w0 s16 | (58.5, 55.5) |
| 10542 | 3 | npc-0 | gatherStone → **sell** | taille | f0 w0 s14 | (58.5, 54.5) |
| 10626 | 3 | npc-2 | drink → **rest** | repose | f0 w6 s4 | (50.5, 57.5) |
| 10635 | 3 | npc-1 | deliver → **gatherFood** | cherche | f0 w6 s6 | (53.5, 50.5) |
| 10710 | 3 | npc-3 | sell → **deliver** | attend | f0 w0 s16 | (51.5, 57.5) |
| 10743 | 3 | npc-0 | sell → **deliver** | attend | f0 w0 s14 | (49.5, 59.5) |
| 10784 | 3 | npc-1 | gatherFood → **deliver** | recolte | f2 w6 s6 | (48.5, 59.5) |
| 10810 | 3 | npc-0 | deliver → **gatherFood** | cherche | f0 w0 s14 | (52.5, 64.5) |
| 10868 | 3 | npc-3 | deliver → **gatherStone** | cherche | f0 w0 s16 | (46.5, 52.5) |
| 10962 | 3 | npc-3 | gatherStone → **deliver** | attend | f0 w0 s16 | (50.5, 57.5) |
| 10996 | 3 | npc-2 | rest → **gatherStone** | cherche | f0 w6 s4 | (50.5, 65.5) |
| 11031 | 3 | npc-1 | deliver → **gatherFood** | cherche | f0 w6 s6 | (49.5, 64.5) |
| 11097 | 3 | npc-4 | eat → **drink** | boit | f0 w6 s5 | (49.5, 58.5) |
| 11108 | 3 | npc-0 | gatherFood → **deliver** | recolte | f3 w0 s14 | (48.5, 60.5) |
| 11116 | 3 | npc-1 | gatherFood → **deliver** | recolte | f2 w6 s6 | (48.5, 59.5) |
| 11148 | 3 | npc-2 | gatherStone → **deliver** | taille | f0 w6 s5 | (50.5, 57.5) |
| 11181 | 3 | npc-3 | deliver → **drink** | boit | f0 w0 s16 | (49.5, 56.5) |
| 11186 | 3 | npc-4 | drink → **rest** | repose | f0 w6 s5 | (51.5, 57.5) |
| 11252 | 3 | npc-0 | deliver → **gatherFood** | cherche | f0 w0 s14 | (49.5, 67.5) |
| 11252 | 3 | npc-1 | deliver → **gatherFood** | cherche | f0 w6 s6 | (48.5, 64.5) |
| 11313 | 3 | npc-3 | drink → **rest** | repose | f0 w0 s16 | (50.5, 57.5) |
| 11326 | 3 | npc-1 | gatherFood → **deliver** | recolte | f2 w6 s6 | (48.5, 60.5) |
| 11398 | 3 | npc-2 | deliver → **rest** | attend | f0 w6 s5 | - |
| 11425 | 3 | npc-0 | gatherFood → **deliver** | recolte | f3 w0 s14 | (48.5, 60.5) |
| 11516 | 3 | npc-1 | deliver → **gatherFood** | cherche | f0 w6 s6 | (42.5, 67.5) |
| 11570 | 3 | npc-0 | deliver → **gatherFood** | cherche | f0 w0 s14 | (49.5, 67.5) |
| 11628 | 3 | npc-3 | rest → **gatherStone** | cherche | f0 w0 s16 | (50.5, 65.5) |
| 11646 | 3 | npc-1 | gatherFood → **deliver** | recolte | f2 w6 s6 | (48.5, 60.5) |
| 11687 | 3 | npc-0 | gatherFood → **deliver** | recolte | f3 w0 s14 | (49.5, 59.5) |
| 11761 | 3 | npc-3 | gatherStone → **deliver** | taille | f0 w0 s18 | (50.5, 57.5) |
| 11834 | 3 | npc-0 | deliver → **gatherFood** | cherche | f0 w0 s14 | (49.5, 70.5) |
| 11834 | 3 | npc-1 | deliver → **gatherFood** | cherche | f0 w6 s6 | (42.5, 67.5) |
| 11842 | 3 | npc-4 | rest → **gatherStone** | cherche | f0 w6 s5 | (50.5, 65.5) |
| 11924 | 3 | npc-3 | deliver → **gatherStone** | cherche | f0 w0 s18 | (50.5, 65.5) |
| 11968 | 3 | npc-4 | gatherStone → **deliver** | taille | f0 w6 s6 | (50.5, 57.5) |
| 11971 | 3 | npc-1 | gatherFood → **deliver** | recolte | f2 w6 s6 | (48.5, 60.5) |
| 11974 | 3 | npc-4 | deliver → **gatherStone** | taille | f0 w6 s6 | (51.5, 65.5) |
| 12001 | 3 | npc-4 | gatherStone → **deliver** | taille | f0 w6 s8 | (50.5, 57.5) |
| 12046 | 3 | npc-3 | gatherStone → **deliver** | taille | f0 w0 s20 | (51.5, 57.5) |
| 12056 | 3 | npc-0 | gatherFood → **deliver** | recolte | f3 w0 s14 | (48.5, 59.5) |
| 12098 | 3 | npc-1 | deliver → **gatherFood** | cherche | f0 w6 s6 | (42.5, 67.5) |
| 12142 | 3 | npc-2 | rest → **gatherStone** | cherche | f0 w6 s5 | (50.5, 65.5) |
| 12163 | 3 | npc-4 | deliver → **gatherStone** | cherche | f0 w6 s8 | (50.5, 65.5) |
| 12217 | 3 | npc-3 | deliver → **gatherStone** | cherche | f0 w0 s20 | (51.5, 65.5) |
| 12221 | 3 | npc-1 | gatherFood → **deliver** | recolte | f2 w6 s6 | (48.5, 59.5) |
| 12289 | 3 | npc-2 | gatherStone → **deliver** | taille | f0 w6 s6 | (50.5, 57.5) |
| 12295 | 3 | npc-4 | gatherStone → **socialize** | socialise | f0 w6 s8 | (49.5, 58.5) |
| 12348 | 3 | npc-3 | gatherStone → **deliver** | taille | f0 w0 s22 | (51.5, 57.5) |
| 12362 | 3 | npc-0 | deliver → **gatherFood** | cherche | f0 w0 s14 | (48.5, 66.5) |
| 12470 | 3 | npc-1 | deliver → **eat** | mange | f0 w6 s6 | (50.5, 57.5) |
| 12481 | 3 | npc-0 | gatherFood → **deliver** | recolte | f3 w0 s14 | (48.5, 60.5) |
| 12524 | 3 | npc-3 | deliver → **socialize** | socialise | f0 w0 s22 | (48.5, 63.1) |
| 12569 | 3 | npc-4 | socialize → **rest** | repose | f0 w6 s8 | (50.5, 57.5) |
| 12573 | 3 | npc-2 | deliver → **gatherStone** | cherche | f0 w6 s6 | (50.5, 65.5) |
| 12602 | 3 | npc-0 | deliver → **gatherFood** | cherche | f0 w0 s14 | (48.5, 66.5) |
| 12670 | 3 | npc-1 | eat → **gatherFood** | cherche | f0 w6 s6 | (50.5, 64.5) |
| 12686 | 3 | npc-2 | gatherStone → **socialize** | socialise | f0 w6 s6 | (49.5, 58.5) |
| 12700 | 3 | npc-0 | gatherFood → **deliver** | recolte | f3 w0 s14 | (48.5, 60.5) |
| 12701 | 3 | npc-0 | deliver → **eat** | mange | f3 w0 s14 | (50.5, 57.5) |
| 12765 | 3 | npc-1 | gatherFood → **deliver** | recolte | f2 w6 s6 | (48.5, 60.5) |
| 12902 | 3 | npc-4 | rest → **relieve** | besoins | f0 w6 s8 | (48.5, 59.5) |
| 12934 | 3 | npc-1 | deliver → **gatherFood** | cherche | f0 w6 s6 | (48.5, 64.5) |
| 12978 | 3 | npc-0 | eat → **deliver** | livre | f2 w0 s14 | (48.5, 59.5) |
| 12990 | 3 | npc-1 | gatherFood → **deliver** | recolte | f2 w6 s6 | (48.5, 60.5) |
| 13066 | 3 | npc-1 | deliver → **gatherFood** | cherche | f0 w6 s6 | (48.5, 64.5) |
| 13110 | 3 | npc-0 | deliver → **socialize** | socialise | f0 w0 s14 | (49.9, 59.0) |
| 13122 | 3 | npc-1 | gatherFood → **deliver** | recolte | f2 w6 s6 | (48.5, 60.5) |
| 13167 | 3 | npc-4 | relieve → **socialize** | socialise | f0 w6 s8 | (50.5, 57.5) |
| 13199 | 3 | npc-1 | deliver → **gatherFood** | cherche | f0 w6 s6 | (48.5, 64.5) |
| 13257 | 3 | npc-0 | socialize → **rest** | repose | f0 w0 s14 | (50.5, 57.5) |
| 13257 | 3 | npc-3 | socialize → **rest** | repose | f0 w0 s22 | (51.5, 57.5) |
| 13257 | 3 | npc-4 | socialize → **rest** | socialise | f0 w6 s8 | - |
| 13336 | 3 | npc-1 | gatherFood → **deliver** | recolte | f2 w6 s6 | (48.5, 60.5) |
| 13473 | 3 | npc-2 | socialize → **rest** | socialise | f0 w6 s6 | - |
| 13523 | 3 | npc-1 | deliver → **helpFarm** | cherche | f0 w6 s6 | (49.8, 62.5) |
| 13656 | 3 | npc-1 | helpFarm → **rest** | repose | f0 w6 s6 | (50.5, 57.5) |
| 14563 | 4 | npc-1 | rest → **gatherFood** | cherche | f0 w6 s6 | (50.5, 64.5) |
| 14660 | 4 | npc-1 | gatherFood → **deliver** | recolte | f2 w6 s6 | (48.5, 60.5) |
| 14665 | 4 | npc-1 | deliver → **eat** | mange | f2 w6 s6 | (50.5, 57.5) |
| 14892 | 4 | npc-1 | eat → **deliver** | livre | f1 w6 s6 | (49.5, 59.5) |
| 15025 | 4 | npc-1 | deliver → **rest** | repose | f0 w6 s6 | (50.5, 57.5) |
| 15057 | 4 | npc-1 | rest → **drink** | boit | f0 w6 s6 | (49.5, 58.5) |
| 15507 | 4 | npc-1 | drink → **gatherStone** | cherche | f0 w6 s6 | (51.5, 65.5) |
| 15617 | 4 | npc-1 | gatherStone → **sell** | taille | f0 w6 s7 | (58.5, 55.5) |
| 15798 | 4 | npc-4 | rest → **gatherStone** | cherche | f0 w6 s8 | (51.5, 65.5) |
| 15804 | 4 | npc-1 | sell → **deliver** | attend | f0 w6 s7 | (49.5, 59.5) |
| 15826 | 4 | npc-3 | rest → **sell** | livre | f0 w0 s22 | (58.5, 55.5) |
| 15829 | 4 | npc-0 | rest → **gatherStone** | cherche | f0 w0 s14 | (51.5, 65.5) |
| 15834 | 4 | npc-2 | rest → **gatherStone** | cherche | f0 w6 s6 | (51.5, 65.5) |
| 15896 | 4 | npc-4 | gatherStone → **sell** | attend | f0 w6 s8 | (59.5, 55.5) |
| 15933 | 4 | npc-0 | gatherStone → **sell** | attend | f0 w0 s14 | (58.5, 54.5) |
| 15967 | 4 | npc-3 | sell → **deliver** | attend | f0 w0 s22 | (51.5, 57.5) |
| 15977 | 4 | npc-2 | gatherStone → **sell** | attend | f0 w6 s6 | (58.5, 55.5) |
| 16039 | 4 | npc-1 | deliver → **gatherStone** | cherche | f0 w6 s7 | (60.5, 60.5) |
| 16081 | 4 | npc-4 | sell → **deliver** | attend | f0 w6 s8 | (51.5, 57.5) |
| 16142 | 4 | npc-0 | sell → **deliver** | attend | f0 w0 s14 | (48.5, 59.5) |

## Batiments a la fin

- building-0 granary (47, 59) progress 1 stock {"food":{"physical":71,"reserved":0}}
- building-1 well (48, 57) progress 1 stock {}
- building-2 house (49, 57) progress 1 stock {}
- building-3 farm (66, 38) progress 0 stock {"wood":{"physical":6,"reserved":0},"stone":{"physical":4,"reserved":0},"planks":{"physical":0,"reserved":0},"tools":{"physical":0,"reserved":0}}
