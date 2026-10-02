# Releve de la navigation — endurance, 16200 ticks

Genere par `node tools/migration/trace-nav.mjs -ref <clone> -ticks 16200` (mission nav-wiring-001).
Cache de navigation a la fin : 24 entrees ; file : 0.

| Appels | Premier tick | Fonction -> branche |
|---:|---:|---|
| 300 | 32 | `applyPathToActor -> chemin (astar)` |
| 16200 | 1 | `beginNavTick` |
| 67 | 493 | `doorQueueWaypoint -> role leader` |
| 19942 | 32 | `doorQueueWaypoint -> role solo` |
| 182 | 114 | `doorQueueWaypoint -> role waiter` |
| 434 | 32 | `lookupCachedPath -> manque` |
| 83 | 114 | `lookupCachedPath -> servi` |
| 1377 | 82 | `movementSpeedFactor -> 1` |
| 18814 | 32 | `movementSpeedFactor -> autre` |
| 32400 | 1 | `processNavQueue -> vide` |
| 217 | 32 | `requestPath -> A* immediat` |
| 83 | 114 | `requestPath -> cache` |
| 217 | 32 | `resolveNavJob` |
| 20042 | 32 | `sim.moveActor` |
| 149 | 118 | `sim.moveActor -> hesitation tiree` |
| 20191 | 32 | `sim.nextWaypoint` |
| 394 | 82 | `sim.recordPassage` |
| 217 | 32 | `storeCachedPath` |
| 60 | 33 | `sweepNavCache` |
