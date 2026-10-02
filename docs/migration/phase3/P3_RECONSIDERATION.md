# Relevé de la reconsidération (`npc.js` l. 893) — scénario `endurance`, 5400 ticks

Généré par `tools/migration/trace-reconsider.mjs` (mission reconsider-001). Ne pas éditer à la main : relancer.

- Référence : `fee66ae`, scénario `endurance`, tick recomposé (rng-trace-lib.mjs).
- **75 tirages** l. 893 ; **38 suivis d'un `chooseGoal`**.
- Chance reconstruite avec les fonctions de la référence : **75 / 75 résultats expliqués** (tirage < chance ⇔ `chooseGoal` suit).

## Branches de `committedReconsiderChance`

| Branche | Tirages |
| --- | ---: |
| `engagement` | 31 |
| `bascule-recente` | 26 |
| `quart` | 7 |
| `base` | 7 |
| `critique` | 4 |

## Ce que la chance a lu

- bascule de phase au tick du tirage : 15
- phase personnelle ≠ phase du village (mode de vie) : 17 tirages d'habitants leve-tot / noctambule
- bouclier de quart actif : 35 ; états de quart vus : COMMUTING, , ON_SHIFT, OFF_DUTY
- besoins critiques : 4

## Les tirages, un par ligne

| Tick | Habitant | But | Mode de vie | Phase | Bascule | thinkDt | Base | Branche | Chance | Tirage | Choisit | Expliqué |
| ---: | --- | --- | --- | --- | --- | ---: | ---: | --- | ---: | ---: | --- | --- |
| 165 | `npc-2` | build | nightOwl | morning |  | 2.2000 | 0.3960 | engagement | 0.1663 | 0.5073 |  | oui |
| 215 | `npc-3` | build | tavernRegular | morning |  | 2.2000 | 0.3960 | engagement | 0.1663 | 0.9442 |  | oui |
| 266 | `npc-4` | build | familyFirst | morning |  | 2.2000 | 0.3960 | engagement | 0.1663 | 0.1050 | oui | oui |
| 298 | `npc-2` | gatherWood | nightOwl | morning |  | 2.2000 | 0.3960 | engagement | 0.1663 | 0.0635 | oui | oui |
| 320 | `npc-0` | helpFarm | wanderer | midday | oui | 2.2000 | 0.3960 | bascule-recente | 1.2100 | 0.1362 | oui | oui |
| 320 | `npc-3` | gatherWood | tavernRegular | midday | oui | 2.2000 | 0.3960 | bascule-recente | 1.2100 | 0.6888 | oui | oui |
| 320 | `npc-4` | gatherWood | familyFirst | midday | oui | 2.2000 | 0.3960 | bascule-recente | 1.2100 | 0.3026 | oui | oui |
| 431 | `npc-2` | gatherWood | nightOwl | morning |  | 2.2000 | 0.3960 | engagement | 0.1663 | 0.3111 |  | oui |
| 453 | `npc-0` | helpFarm | wanderer | midday |  | 2.2000 | 0.3960 | bascule-recente | 1.2100 | 0.0936 | oui | oui |
| 453 | `npc-1` | explore | workhorse | midday |  | 2.2000 | 0.3960 | bascule-recente | 1.2100 | 0.2646 | oui | oui |
| 453 | `npc-3` | gatherWood | tavernRegular | midday |  | 2.2000 | 0.3960 | bascule-recente | 1.2100 | 0.7491 | oui | oui |
| 453 | `npc-4` | gatherWood | familyFirst | midday |  | 2.2000 | 0.3960 | bascule-recente | 1.2100 | 0.6443 | oui | oui |
| 536 | `npc-2` | gatherWood | nightOwl | midday | oui | 2.2000 | 0.3960 | bascule-recente | 1.2100 | 0.2466 | oui | oui |
| 586 | `npc-1` | helpFarm | workhorse | midday |  | 2.2000 | 0.3960 | engagement | 0.5082 | 0.4192 | oui | oui |
| 586 | `npc-3` | sell | tavernRegular | midday |  | 2.2000 | 0.3960 | quart | 0.5082 | 0.6171 |  | oui |
| 586 | `npc-4` | sell | familyFirst | midday |  | 2.2000 | 0.3960 | quart | 0.5082 | 0.5446 |  | oui |
| 669 | `npc-2` | sell | nightOwl | midday |  | 2.2000 | 0.3960 | bascule-recente | 1.2100 | 0.8442 | oui | oui |
| 719 | `npc-1` | helpFarm | workhorse | midday |  | 2.2000 | 0.3960 | engagement | 0.5082 | 0.9298 |  | oui |
| 719 | `npc-3` | sell | tavernRegular | midday |  | 2.2000 | 0.3960 | quart | 0.5082 | 0.0942 | oui | oui |
| 719 | `npc-4` | sell | familyFirst | midday |  | 2.2000 | 0.3960 | quart | 0.5082 | 0.8999 |  | oui |
| 739 | `npc-0` | socialize | wanderer | midday |  | 2.2000 | 0.3960 | engagement | 0.5082 | 0.7057 |  | oui |
| 770 | `npc-0` | socialize | wanderer | afternoon | oui | 2.2000 | 0.3960 | bascule-recente | 0.3960 | 0.7154 |  | oui |
| 770 | `npc-1` | helpFarm | workhorse | afternoon | oui | 2.2000 | 0.3960 | bascule-recente | 0.3960 | 0.6727 |  | oui |
| 770 | `npc-3` | sell | tavernRegular | afternoon | oui | 2.2000 | 0.3960 | bascule-recente | 0.3960 | 0.4086 |  | oui |
| 770 | `npc-4` | sell | familyFirst | afternoon | oui | 2.2000 | 0.3960 | bascule-recente | 0.3960 | 0.5341 |  | oui |
| 903 | `npc-1` | helpFarm | workhorse | afternoon |  | 2.2000 | 0.3960 | bascule-recente | 0.3960 | 0.2681 | oui | oui |
| 935 | `npc-2` | sell | nightOwl | midday |  | 2.2000 | 0.3960 | base | 1.2100 | 0.9910 | oui | oui |
| 986 | `npc-2` | sell | nightOwl | afternoon | oui | 2.2000 | 0.3960 | bascule-recente | 0.3960 | 0.9134 |  | oui |
| 1036 | `npc-0` | gatherStone | wanderer | afternoon |  | 2.2000 | 0.3960 | engagement | 0.1663 | 0.1545 | oui | oui |
| 1036 | `npc-1` | build | workhorse | afternoon |  | 2.2000 | 0.3960 | engagement | 0.1663 | 0.1456 | oui | oui |
| 1169 | `npc-0` | sell | wanderer | afternoon |  | 2.2000 | 0.3960 | engagement | 0.1663 | 0.9872 |  | oui |
| 1169 | `npc-1` | gatherWood | workhorse | afternoon |  | 2.2000 | 0.3960 | engagement | 0.1663 | 0.3673 |  | oui |
| 1302 | `npc-0` | sell | wanderer | afternoon |  | 2.2000 | 0.3960 | engagement | 0.1663 | 0.1359 | oui | oui |
| 1302 | `npc-1` | sell | workhorse | afternoon |  | 2.2000 | 0.3960 | engagement | 0.1663 | 0.1845 |  | oui |
| 1302 | `npc-4` | deliver | familyFirst | afternoon |  | 2.2000 | 0.3960 | engagement | 0.1663 | 0.5288 |  | oui |
| 1385 | `npc-2` | deliver | nightOwl | afternoon |  | 2.2000 | 0.3960 | engagement | 0.1663 | 0.1175 | oui | oui |
| 1435 | `npc-0` | deliver | wanderer | afternoon |  | 2.2000 | 0.3960 | engagement | 0.1663 | 0.3160 |  | oui |
| 1435 | `npc-1` | sell | workhorse | afternoon |  | 2.2000 | 0.3960 | quart | 0.1663 | 0.2914 |  | oui |
| 1530 | `npc-2` | deliver | nightOwl | afternoon |  | 2.2000 | 0.3960 | engagement | 0.1663 | 0.5371 |  | oui |
| 1568 | `npc-0` | deliver | wanderer | afternoon |  | 2.2000 | 0.3960 | engagement | 0.1663 | 0.0930 | oui | oui |
| 1568 | `npc-1` | deliver | workhorse | afternoon |  | 2.2000 | 0.3960 | engagement | 0.1663 | 0.0630 | oui | oui |
| 1670 | `npc-0` | gatherStone | wanderer | evening | oui | 2.2000 | 0.3960 | bascule-recente | 1.2100 | 0.7330 | oui | oui |
| 1670 | `npc-1` | gatherStone | workhorse | evening | oui | 2.2000 | 0.3960 | bascule-recente | 1.2100 | 0.7861 | oui | oui |
| 1713 | `npc-2` | deliver | nightOwl | afternoon |  | 2.2000 | 0.3960 | base | 0.3960 | 0.3348 | oui | oui |
| 1768 | `npc-4` | deliver | familyFirst | evening |  | 0.5500 | 0.7425 | critique | 0.7425 | 0.9523 |  | oui |
| 1803 | `npc-1` | deliver | workhorse | evening |  | 2.2000 | 0.3960 | bascule-recente | 1.2100 | 0.2550 | oui | oui |
| 1896 | `npc-2` | deliver | nightOwl | evening | oui | 2.2000 | 0.3960 | bascule-recente | 1.2100 | 0.7421 | oui | oui |
| 1972 | `npc-4` | deliver | familyFirst | evening |  | 0.5500 | 0.7425 | critique | 0.7425 | 0.0179 | oui | oui |
| 1983 | `npc-0` | socialize | wanderer | evening |  | 2.2000 | 0.3960 | engagement | 0.5082 | 0.0008 | oui | oui |
| 1983 | `npc-1` | deliver | workhorse | evening |  | 2.2000 | 0.3960 | engagement | 0.5082 | 0.8891 |  | oui |
| 2079 | `npc-2` | deliver | nightOwl | evening |  | 2.2000 | 0.3960 | base | 1.2100 | 0.8943 | oui | oui |
| 2176 | `npc-4` | deliver | familyFirst | evening |  | 0.5500 | 0.7425 | critique | 0.7425 | 0.2702 | oui | oui |
| 2262 | `npc-2` | deliver | nightOwl | evening |  | 2.2000 | 0.3960 | base | 1.2100 | 0.9511 | oui | oui |
| 2380 | `npc-4` | deliver | familyFirst | evening |  | 0.5500 | 0.7425 | critique | 0.7425 | 0.3455 | oui | oui |
| 2445 | `npc-2` | deliver | nightOwl | evening |  | 2.2000 | 0.3960 | base | 1.2100 | 0.4828 | oui | oui |
| 2458 | `npc-1` | socialize | workhorse | night | oui | 2.2000 | 0.3960 | bascule-recente | 1.2100 | 0.7739 | oui | oui |
| 2470 | `npc-3` | deliver | tavernRegular | night | oui | 2.2000 | 0.3960 | bascule-recente | 1.2100 | 0.9167 | oui | oui |
| 2653 | `npc-3` | deliver | tavernRegular | night |  | 2.2000 | 0.3960 | base | 1.2100 | 0.7326 | oui | oui |
| 2836 | `npc-3` | deliver | tavernRegular | night |  | 2.2000 | 0.3960 | base | 1.2100 | 0.9010 | oui | oui |
| 4708 | `npc-3` | drink | tavernRegular | morning | oui | 2.2000 | 0.3960 | bascule-recente | 0.3960 | 0.5456 |  | oui |
| 4941 | `npc-4` | gatherStone | familyFirst | morning |  | 2.2000 | 0.3960 | bascule-recente | 0.3960 | 0.8852 |  | oui |
| 4974 | `npc-3` | build | tavernRegular | morning |  | 2.2000 | 0.3960 | engagement | 0.1663 | 0.1081 | oui | oui |
| 5074 | `npc-4` | gatherStone | familyFirst | morning |  | 2.2000 | 0.3960 | engagement | 0.1663 | 0.1848 |  | oui |
| 5086 | `npc-2` | build | nightOwl | morning |  | 2.2000 | 0.3960 | bascule-recente | 0.3960 | 0.6855 |  | oui |
| 5107 | `npc-3` | gatherStone | tavernRegular | morning |  | 2.2000 | 0.3960 | engagement | 0.1663 | 0.2789 |  | oui |
| 5141 | `npc-1` | sell | workhorse | morning |  | 2.2000 | 0.3960 | bascule-recente | 0.3960 | 0.7429 |  | oui |
| 5156 | `npc-0` | gatherStone | wanderer | morning |  | 2.2000 | 0.3960 | bascule-recente | 0.3960 | 0.4980 |  | oui |
| 5207 | `npc-4` | gatherStone | familyFirst | morning |  | 2.2000 | 0.3960 | engagement | 0.1663 | 0.1667 |  | oui |
| 5219 | `npc-2` | gatherStone | nightOwl | morning |  | 2.2000 | 0.3960 | engagement | 0.1663 | 0.5335 |  | oui |
| 5240 | `npc-3` | sell | tavernRegular | morning |  | 2.2000 | 0.3960 | engagement | 0.1663 | 0.1856 |  | oui |
| 5274 | `npc-1` | sell | workhorse | morning |  | 2.2000 | 0.3960 | engagement | 0.1663 | 0.9566 |  | oui |
| 5289 | `npc-0` | sell | wanderer | morning |  | 2.2000 | 0.3960 | engagement | 0.1663 | 0.8175 |  | oui |
| 5340 | `npc-4` | deliver | familyFirst | morning |  | 2.2000 | 0.3960 | quart | 0.1663 | 0.3447 |  | oui |
| 5352 | `npc-2` | gatherStone | nightOwl | morning |  | 2.2000 | 0.3960 | engagement | 0.1663 | 0.5129 |  | oui |
| 5373 | `npc-3` | sell | tavernRegular | morning |  | 2.2000 | 0.3960 | quart | 0.1663 | 0.5981 |  | oui |
