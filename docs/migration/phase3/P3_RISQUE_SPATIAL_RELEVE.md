# Releve de spatialRiskBiasMap — endurance, 16200 ticks

Genere par `node tools/migration/trace-spatial-risk.mjs -ref <clone> -ticks 16200` (mission resource-targets-001).
29 fonctions de la reference enveloppees, 17 methodes de Simulation. Fonctions importees vues par la pile seulement : wantsColonizationClear, isUrgentColonizationClear, canSawAtMill, isLivestockDepot, extractionPostFor.

- appels de `spatialRiskBiasMap` : **381** (5 habitants, ticks 32 a 16172)
- biais non nul : **255** appels
- tirages `sim.rng` sous la preparation : **57**
- ecritures de `building.accessPoints` sous la preparation : **1**

## Cibles par but (branches atteintes)

| Appels | But :: chemin -> resultat |
|---:|---|
| 381 | `gatherWood :: gatherWoodTarget{recallOrSearch{extractionCourtTarget=null,recallResource}} -> cible` |
| 381 | `gatherStone :: recallOrSearch{extractionCourtTarget=null,recallResource} -> cible` |
| 381 | `gatherFood :: recallOrSearch{extractionCourtTarget=null,recallResource} -> cible` |
| 381 | `helpFarm :: sim.findTendFieldNear,fieldWorkTarget -> cible` |
| 274 | `build :: sim.constructionAccessPoint{sim.buildingAccessPoint{pickBuildingAccessPoint{ensureBuildingAccessPoints}}} -> cible` |
| 250 | `maintain :: sim.maintenancePos{sim.workCommutePos=null,sim.completedBuildingEntries,sim.pickDailyBuilding,sim.buildingAccessPoint{pickBuildingAccessPoint{ensureBuildingAccessPoints}}} -> cible` |
| 219 | `aidHousehold :: householdAidTarget{householdAidPlan=null,householdAidPlan=null,sim.socialPos{sim.workCommutePos=null,sim.completedBuildingEntries,sim.buildingAccessPoint{pickBuildingAccessPoint{ensureBuildingAccessPoints}}}} -> cible` |
| 216 | `explore :: intentExploreHint=null -> null` |
| 131 | `maintain :: sim.maintenancePos{sim.workCommutePos{sim.buildingAccessPoint{pickBuildingAccessPoint{ensureBuildingAccessPoints}}}} -> cible` |
| 125 | `aidHousehold :: householdAidTarget{householdAidPlan=null,householdAidPlan=null,stableBuildingAccess{sim.buildingAccessPoint{pickBuildingAccessPoint{ensureBuildingAccessPoints}}}} -> cible` |
| 108 | `explore :: intentExploreHint=null -> cible` |
| 107 | `build :: sim.constructionAccessPoint=null -> null` |
| 57 | `explore :: intentExploreHint -> cible` |
| 37 | `aidHousehold :: householdAidTarget{householdAidPlan=null,householdAidPlan=null,stableBuildingAccess} -> cible` |

## Replis surs et budgets (hors cibles)

| Appels | Chemin |
|---:|---|
| 13597 | `routeSeconds` |
| 13597 | `budgetOverrun` |
| 381 | `forecastRestTarget{bestKnownBed{seedHomeBedBelief=null,bestKnownBelief}}` |
| 381 | `forecastDrinkTarget{drinkTarget{bestKnownWater{bestKnownBelief}}}` |
| 381 | `shelterRainAccess{bestKnownBed{seedHomeBedBelief=null,bestKnownBelief}}` |
| 381 | `criticalNeedBudget` |
| 381 | `secondsUntilNight` |
| 381 | `rainReturnBudget` |
| 359 | `forecastEatTarget{sim.marketAccessPoint{sim.plannedMarketPos,sim.accessPointNear}}` |
| 22 | `forecastEatTarget{sim.buildingAccessPoint{pickBuildingAccessPoint{ensureBuildingAccessPoints}}}` |

## Biais non nuls

- tick 903, npc-4 : `{"gatherWood":-1.2637420600585547,"gatherFood":-1.6602366884085864}`
- tick 1036, npc-4 : `{"gatherWood":-22.288381237964945,"gatherStone":-7.643394747126242,"gatherFood":-21.93252814726781}`
- tick 1169, npc-4 : `{"gatherWood":-1.1746053637290697,"gatherStone":-2.0454449450489385,"gatherFood":-1.9723889426534935,"helpFarm":-0.8393448890745479,"maintain":-6.175625640898552,"aidHousehold":-9.261620709033863}`
- tick 1486, npc-4 : `{"gatherWood":-23.82888264574222,"gatherStone":-23.089462459376303,"gatherFood":-22.02731948230566,"helpFarm":-18.20850584564072,"maintain":-8.485324459840944,"aidHousehold":-10.109200477625338}`
- tick 1632, npc-4 : `{"gatherWood":-23.82888264574222,"gatherStone":-23.089462459376303,"gatherFood":-22.02731948230566,"helpFarm":-18.20850584564072,"maintain":-8.485324459840944,"aidHousehold":-10.109200477625338}`
- tick 1700, npc-4 : `{"gatherWood":-23.82888264574222,"gatherStone":-23.089462459376303,"gatherFood":-22.02731948230566,"helpFarm":-18.20850584564072,"maintain":-8.485324459840944,"aidHousehold":-10.109200477625338}`
- tick 1713, npc-2 : `{"gatherFood":-6.127381939072588}`
- tick 1836, npc-4 : `{"gatherWood":-22.780939241624544,"gatherStone":-29.404442138378165,"gatherFood":-28.10967001452088,"helpFarm":-18.20850584564072,"maintain":-8.485324459840944,"aidHousehold":-10.109200477625338}`
- tick 1896, npc-2 : `{"gatherStone":-4.6714888417982054,"gatherFood":-1.7854933298715352}`
- tick 1904, npc-4 : `{"gatherWood":-22.780939241624544,"gatherStone":-29.404442138378165,"gatherFood":-28.10967001452088,"helpFarm":-18.20850584564072,"maintain":-8.485324459840944,"aidHousehold":-10.109200477625338}`
- tick 1972, npc-4 : `{"gatherWood":-22.780939241624544,"gatherStone":-29.404442138378165,"gatherFood":-28.10967001452088,"helpFarm":-18.20850584564072,"explore":-11.51579748121271,"maintain":-8.485324459840944,"aidHousehold":-10.109200477625338}`
- tick 1983, npc-0 : `{"gatherWood":-6.869916649249532,"gatherStone":-16.559614629349085,"gatherFood":-12.969004228404911,"helpFarm":-1.582305216025616}`
- tick 2040, npc-4 : `{"gatherWood":-22.780939241624544,"gatherStone":-29.404442138378165,"gatherFood":-28.10967001452088,"helpFarm":-18.20850584564072,"maintain":-8.485324459840944,"aidHousehold":-10.109200477625338}`
- tick 2043, npc-3 : `{"gatherWood":-6.392261765246335,"gatherStone":-21.420452927063938,"gatherFood":-19.109120370775756,"helpFarm":-10.76726925303764}`
- tick 2079, npc-2 : `{"gatherWood":-15.42087810306658,"gatherStone":-33.64648884179653,"gatherFood":-30.760493329869863,"helpFarm":-19.70743051643534,"explore":-4.55122434702172,"maintain":-3.3535337293844254,"aidHousehold":-3.3535335040402594}`
- tick 2108, npc-4 : `{"gatherWood":-22.780939241624544,"gatherStone":-29.404442138378165,"gatherFood":-28.10967001452088,"helpFarm":-18.20850584564072,"maintain":-8.485324459840944,"aidHousehold":-10.109200477625338}`
- tick 2116, npc-0 : `{"gatherWood":-16.613277766178225,"gatherStone":-23.073076419577927,"gatherFood":-20.679336152281813,"helpFarm":-9.921536810692492,"maintain":-2.7254260420954015,"aidHousehold":-1.9793563823916194}`
- tick 2116, npc-1 : `{"gatherWood":-39.29888984581956,"gatherStone":-32.59379194072729,"gatherFood":-19.637404875529494,"helpFarm":-8.28482748511497,"maintain":-4.233918666902523,"aidHousehold":-2.3901612117794944}`
- tick 2176, npc-4 : `{"gatherWood":-22.780939241624544,"gatherStone":-29.404442138378165,"gatherFood":-28.10967001452088,"helpFarm":-18.20850584564072,"explore":-11.51579748121271,"maintain":-8.485324459840944,"aidHousehold":-10.109200477625338}`
- tick 2226, npc-3 : `{"gatherWood":-9.961507843509787,"gatherStone":-19.98030195138819,"gatherFood":-18.4394135805294,"helpFarm":-11.378179502034106,"maintain":-2.9443487844496365,"aidHousehold":-2.9443483640312986}`
- tick 2244, npc-4 : `{"gatherWood":-22.780939241624544,"gatherStone":-29.404442138378165,"gatherFood":-28.10967001452088,"helpFarm":-18.20850584564072,"maintain":-8.485324459840944,"aidHousehold":-10.109200477625338}`
- tick 2249, npc-0 : `{"gatherWood":-16.613277766178225,"gatherStone":-23.073076419577927,"gatherFood":-20.679336152281813,"helpFarm":-9.921536810692492,"maintain":-2.7254260420954015,"aidHousehold":-1.9793563823916194}`
- tick 2262, npc-2 : `{"gatherWood":-12.180585402056952,"gatherStone":-24.330992561210255,"gatherFood":-22.406995553259144,"helpFarm":-14.538287010966151,"explore":-4.934149564693712,"maintain":-3.635689152932209,"aidHousehold":-3.635689002702765}`
- tick 2312, npc-4 : `{"gatherWood":-22.780939241624544,"gatherStone":-29.404442138378165,"gatherFood":-28.10967001452088,"helpFarm":-18.20850584564072,"maintain":-8.485324459840944,"aidHousehold":-10.109200477625338}`
- tick 2380, npc-4 : `{"gatherWood":-22.780939241624544,"gatherStone":-29.404442138378165,"gatherFood":-28.10967001452088,"helpFarm":-18.20850584564072,"explore":-11.51579748121271,"maintain":-8.485324459840944,"aidHousehold":-10.109200477625338}`
- tick 2382, npc-0 : `{"gatherWood":-16.613277766178225,"gatherStone":-23.073076419577927,"gatherFood":-20.679336152281813,"helpFarm":-9.921536810692492,"maintain":-2.7254260420954015,"aidHousehold":-1.9793563823916194}`
- tick 2409, npc-3 : `{"gatherWood":-9.961507843509787,"gatherStone":-19.98030195138819,"gatherFood":-18.4394135805294,"helpFarm":-11.378179502034106,"maintain":-2.9443487844496365,"aidHousehold":-2.9443483640312986}`
- tick 2445, npc-2 : `{"gatherWood":-12.180585402056952,"gatherStone":-24.330992561210255,"gatherFood":-22.406995553259144,"helpFarm":-14.538287010966151,"explore":-4.934149564693712,"maintain":-3.635689152932209,"aidHousehold":-3.635689002702765}`
- tick 2448, npc-4 : `{"gatherWood":-22.780939241624544,"gatherStone":-29.404442138378165,"gatherFood":-28.10967001452088,"helpFarm":-18.20850584564072,"maintain":-8.485324459840944,"aidHousehold":-10.109200477625338}`
- tick 2456, npc-1 : `{"gatherWood":-7.7116413270163395,"gatherStone":-19.063812904686525,"gatherFood":-19.91510181761384,"helpFarm":-9.652897906542805,"maintain":-4.9895494902510515,"aidHousehold":-1.9071828130357362}`
- tick 2458, npc-0 : `{"gatherWood":-16.613277766178225,"gatherStone":-23.073076419577927,"gatherFood":-20.679336152281813,"helpFarm":-9.921536810692492,"maintain":-2.7254260420954015,"aidHousehold":-1.9793563823916194}`
- tick 2458, npc-1 : `{"gatherWood":-7.7116413270163395,"gatherStone":-19.063812904686525,"gatherFood":-19.91510181761384,"helpFarm":-9.652897906542805,"explore":-2.5883195319770707,"maintain":-4.9895494902510515,"aidHousehold":-2.1522750356291493}`
- tick 2470, npc-3 : `{"gatherWood":-9.961507843509787,"gatherStone":-19.98030195138819,"gatherFood":-18.4394135805294,"helpFarm":-11.378179502034106,"explore":-3.9959019217530782,"maintain":-2.9443487844496365,"aidHousehold":-4.241310723218487}`
- tick 2603, npc-4 : `{"gatherWood":-8.762716614423818,"gatherStone":-19.42047018756977,"gatherFood":-21.815989283670476,"helpFarm":-12.091612501758283,"maintain":-2.544433406757413,"aidHousehold":-1.9561596330373292}`
- tick 2653, npc-3 : `{"gatherWood":-34.43926497933691,"gatherStone":-43.5570874016836,"gatherFood":-36.25080939067167,"helpFarm":-24.93827910657282,"explore":-16.765641708667953,"maintain":-12.353630732702703,"aidHousehold":-15.59453455336409}`
- tick 2675, npc-2 : `{"gatherWood":-12.180585402056952,"gatherStone":-24.330992561210255,"gatherFood":-22.406995553259144,"helpFarm":-14.538287010966151,"maintain":-3.635689152932209,"aidHousehold":-3.635689002702765}`
- tick 2760, npc-4 : `{"gatherWood":-8.762716614423818,"gatherStone":-19.42047018756977,"gatherFood":-21.815989283670476,"helpFarm":-12.091612501758283,"maintain":-2.544433406757413,"aidHousehold":-1.9561596330373292}`
- tick 2836, npc-3 : `{"gatherWood":-23.653082222462825,"gatherStone":-29.73163050402729,"gatherFood":-24.860778496686,"helpFarm":-17.136572631083727,"explore":-11.870666708683524,"maintain":-8.74680704850365,"aidHousehold":-10.907409595611242}`
- tick 3066, npc-3 : `{"gatherWood":-9.961507843509787,"gatherStone":-19.98030195138819,"gatherFood":-18.4394135805294,"helpFarm":-11.378179502034106,"maintain":-2.9443487844496365,"aidHousehold":-2.9443483640312986}`
- tick 3314, npc-1 : `{"gatherWood":-8.462514723514934,"gatherStone":-19.103211674407497,"gatherFood":-20.399274159220255,"helpFarm":-10.015874717267401,"maintain":-4.512931658345143,"aidHousehold":-1.7217409491181788}`
- tick 3329, npc-0 : `{"gatherWood":-8.059774155429887,"gatherStone":-20.18384002855486,"gatherFood":-21.40827738148964,"helpFarm":-11.04120891380497,"maintain":-4.181897410303945,"aidHousehold":-1.9912421616199283}`
- tick 3396, npc-2 : `{"gatherWood":-12.180585402056952,"gatherStone":-24.330992561210255,"gatherFood":-22.406995553259144,"helpFarm":-14.538287010966151,"maintain":-3.635689152932209,"aidHousehold":-3.635689152932209}`
- tick 3607, npc-4 : `{"gatherWood":-8.762716614423818,"gatherStone":-19.42047018756977,"gatherFood":-21.815989283670476,"helpFarm":-12.091612501758283,"maintain":-2.544433406757413,"aidHousehold":-1.9561596330373292}`
- tick 3679, npc-3 : `{"gatherWood":-9.961507843509787,"gatherStone":-19.98030195138819,"gatherFood":-18.4394135805294,"helpFarm":-11.378179502034106,"maintain":-2.9443487844496365,"aidHousehold":-2.9443483640312986}`
- tick 4117, npc-2 : `{"gatherWood":-12.180585402056952,"gatherStone":-24.330992561210255,"gatherFood":-22.406995553259144,"helpFarm":-14.538287010966151,"maintain":-3.635689152932209,"aidHousehold":-3.635689152932209}`
- tick 4161, npc-1 : `{"gatherWood":-8.462514723514934,"gatherStone":-19.103211674407497,"gatherFood":-20.399274159220255,"helpFarm":-10.015874717267401,"maintain":-4.512931658345143,"aidHousehold":-1.7217409491181788}`
- tick 4176, npc-0 : `{"gatherWood":-8.059774155429887,"gatherStone":-20.18384002855486,"gatherFood":-21.40827738148964,"helpFarm":-11.04120891380497,"maintain":-4.181897410303945,"aidHousehold":-1.9912421616199283}`
- tick 5671, npc-1 : `{"build":-40.608260594864106}`
- tick 5720, npc-0 : `{"gatherWood":-12.580645439948976,"gatherStone":-11.862786406092798,"gatherFood":-6.9836948521727695,"helpFarm":-1.7403717355879231,"build":-50.4}`
- tick 5720, npc-1 : `{"gatherWood":-11.41294984464144,"build":-49.44418751240928}`
- tick 6090, npc-1 : `{"build":-6.254467496262411}`
- tick 6170, npc-0 : `{"build":-19.928402704591832}`
- tick 6170, npc-1 : `{"build":-5.766357409138483}`
- tick 6302, npc-0 : `{"gatherFood":-3.9060498826875767,"build":-39.56248709078226}`
- tick 6302, npc-1 : `{"build":-32.3959853136201}`
- tick 6434, npc-0 : `{"gatherWood":-8.817749189578782,"gatherFood":-4.048622860952255,"build":-50.4}`
- tick 6434, npc-1 : `{"gatherFood":-14.80731609824755,"build":-36.278655056992775}`
- tick 6441, npc-4 : `{"build":-7.651432871213834}`
- tick 6566, npc-1 : `{"gatherWood":-14.191658176761784,"gatherStone":-4.98100318731353,"gatherFood":-33.58932622265785,"build":-50.289960829666896}`
- tick 6575, npc-4 : `{"build":-22.156952643475975}`
- ... (195 de plus)

## Ecritures des seuils

- tick 32, npc-2, building-1 : `[{"x":49.5,"y":57.5},{"x":50.5,"y":57.5},{"x":49.5,"y":58.5},{"x":49.5,"y":56.5}]` -> `[{"x":50.5,"y":57.5},{"x":49.5,"y":58.5},{"x":49.5,"y":56.5}]`

## Tirages

- 57 x `intentExploreHint__o < intentExploreHint < spatialRiskTargetForGoal__o < spatialRiskTargetForGoal < spatialRiskBiasMap__o < spatialRiskBiasMap`
