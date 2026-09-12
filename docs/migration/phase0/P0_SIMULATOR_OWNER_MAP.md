# P0-C Simulator owner map

Source tree: `C:\dev\Jeux IV Kingdoms` checkout `codex/p0-temporal-hud` @ `fee66ae8`.  
`src/render3d/` not read. Line counts (~87k sim / ~137k render) DECLARED by mission + `.agent/manifest.json` largest-files; not re-counted this session.

Method: entrypoints + imports + targeted reads. Not a full decompile.

## Highest-level flow (supported)

```
BOOT (src/main.js)
  → Simulation constructor | initBootDeferredShell | reset → resetWorldBase
WORLD (src/sim/world.js generateWorld; hydrology, archetypes, fieldCrops)
  → CLOCK (src/runtime/simClock.js plan; Simulation.tick: time += dt; day = 1+floor(time/DAY_LENGTH))
  → ACTORS (sim.actors; spawnNpc; updateNpc each tick)
  → DECISIONS (perceive + chooseGoal) OR (Noûs: computeAlgorithmicDecision) — flag ALGORITHMIC_NPC_V1 default true
  → ACTIONS (act / updateInside / drivePlayerActor; nav moveActor)
  → WORLD MUTATION (transport tick, animals, onNewDay economy + deferred life/ai jobs)
  → OBSERVABILITY (src/sim/observability.js no-op core; tools/test.mjs headless)
```

Browser RAF is `src/runtime/gameLoop.js:startGameLoop`. Tick injection: `main.js` `sim.tick(plan.stepDt)` inside `resilience.guard("sim", ...)`. Wall-clock budget can skip steps — JS runtime, not Unreal Tick.

## Domain cards

### World state

- OWNER: `Simulation` (`src/sim/simulation.js`)
- ENTRYPOINT: `resetWorldBase` → `generateWorld(seed,w,h)`
- STATE_READ/WRITE: `tiles`, `w/h`, `archetype`, `settlement`, `cadastre`, `buildings`, `blocked`, resource grids
- UPDATE_CADENCE: boot; tile diffs thereafter; daily regen via deferred `landRegen`
- DEPENDENCIES: `world.js`, `worldArchetypes.js`, `hydrology.js`, `fieldCrops.js`, `landExtent.js`, `rng.js`
- PUBLIC_INTERFACE: `generateWorld`; `Simulation.reset` / `resetWorldBase`
- SIDE_EFFECTS: `warmPristineFromSim` (save cache)
- TEST_SURFACE: `tools/test.mjs` (imports `Simulation`); `tools/unreal/gen-parity-vectors.mjs` does **not** yet vector `world.js`
- UNKNOWN: exact `valueNoise`/`Math.sin` portability (PORTAGE.md warning; not measured here)

### Time

- OWNER: `Simulation.time` / `Simulation.day`; planner `src/runtime/simClock.js`
- ENTRYPOINT: `Simulation.tick(dt)` ; `onNewDay`
- STATE_READ/WRITE: `time`, `day`, `_dayDeferred`
- UPDATE_CADENCE: every sim tick; day boundary when `floor(time/DAY_LENGTH)` changes. `DAY_LENGTH = 90` sim seconds. Lived year: `src/sim/life.js` `MONTHS_PER_DAY = 1` (12 sim days / year)
- DEPENDENCIES: callers must pass dt from `simStepPlan` (fat steps at >1x)
- PUBLIC_INTERFACE: `tick`, `onNewDay`, `processDayDeferred`
- SIDE_EFFECTS: enqueues deferred daily jobs (count-budgeted, not wall-ms — determinism invariant)
- TEST_SURFACE: C++ `Anastasis.Sim.Parite.Horloge` (clock helpers only)
- UNKNOWN: whether Unreal Tick will use fat-step or 60 Hz always

### Actor lifecycle

- OWNER: `Simulation.spawnNpc` / `src/life/mortality.js` `removeActor`
- ENTRYPOINT: founding `populateFoundingLife`; immigration `maybeImmigrate`; player `arriveAsPlayer` → spawn + `incarnate`
- STATE_READ/WRITE: `actors[]`, `_nextId`, `playerPersonId`
- UPDATE_CADENCE: spawn events; death daily; tick `updateNpc`
- DEPENDENCIES: `content.js` jobs; `life/names.js` identity; `_playerRng` isolated from world rng
- PUBLIC_INTERFACE: `spawnNpc`, `incarnate`, `release`, `arriveAsPlayer`
- SIDE_EFFECTS: index invalidation; save fields
- TEST_SURFACE: player lineage verifies (ACTIVE_MISSION history); not re-run here
- UNKNOWN: full spawn field list (simulation.js is 8k+ lines; not fully read)

### NPC decision / AI

- OWNER (classic): `src/sim/npc.js` `chooseGoal` ← `scoreGoals`; memory `src/ai/memory.js` `perceive`
- OWNER (Noûs, default on): `src/ai/algorithmic/runtime.js` `computeAlgorithmicDecision`; flag `src/ai/algorithmic/flags.js` `ALGORITHMIC_NPC_V1 = true`
- ENTRYPOINT: `updateNpc` (cadenced think; move/act every cadence dt)
- STATE_READ: npc.mind, needs, job, beliefs — **not** omniscient world (file header)
- STATE_WRITE: `npc.goal`, `npc.target`, activity
- UPDATE_CADENCE: `NPC_AI.thinkEvery` / critical; logical LOD can replace village tick (`logicalLod.js`)
- DEPENDENCIES: needs, lifestyle, culture, nature, skills, orthodox V0–V3 sync hooks
- PUBLIC_INTERFACE: `updateNpc`, `chooseGoal`; algorithmic exports
- SIDE_EFFECTS: nav requests; talk hold short-circuits
- TEST_SURFACE: `npm run verify:algorithmic-npc` (CURRENT_STATE); not run here
- UNKNOWN: which path is canonical for migration if both remain — two living brains

### Needs

- OWNER: `src/life/needs.js`
- ENTRYPOINT: `tickNeeds` from `updateNpc`; scores via `needGoalScores`
- STATE_READ/WRITE: npc need fields (hunger/fatigue/social/…)
- UPDATE_CADENCE: per NPC tick (not only daily)
- DEPENDENCIES: `sim/life.js` LIFE constants; inventory/food paths
- PUBLIC_INTERFACE: `ensureNeeds`, `tickNeeds`, `needGoalScores`, `satisfy*`
- SIDE_EFFECTS: none beyond npc
- TEST_SURFACE: tools/test.mjs health invariants; causal-needs benches in manifest
- UNKNOWN: full need schema

### Work / craft

- OWNER: `src/sim/craftWork.js` + `npc.js` `act` for CRAFT_GOALS
- ENTRYPOINT: `act` when goal is craft; yards sync `syncAllYardsFromStock`
- UPDATE_CADENCE: tick (sessions) + daily production in `onNewDay`
- DEPENDENCIES: `content.js`, `transport/stockLedger.js`
- PUBLIC_INTERFACE: craft profiles, depot predicates, `applySawBatch`, …
- SIDE_EFFECTS: stock/yards
- TEST_SURFACE: UNKNOWN specific verify name this session
- UNKNOWN: complete craftId list

### Economy / resources

- OWNER: physical stock `src/sim/transport/stockLedger.js`; prices `src/sim/economy.js`; daily resolve `Simulation.onNewDay`
- ENTRYPOINT: `tickTransport`; `buyFromMarket`/`sellToMarket`; `rebuildMarketAggregate`
- STATE_READ/WRITE: building stocks, `market.stock` (derived view), `colony.treasury`
- UPDATE_CADENCE: transport tick (budgeted); economy daily at midnight
- DEPENDENCIES: `content.js` MARKET; spoil/export tables
- PUBLIC_INTERFACE: stockLedger exports; economy.js trade fns
- SIDE_EFFECTS: morale, logs
- TEST_SURFACE: save roundtrip comments in `save.js` (wood 19 vs 37 bug)
- UNKNOWN: full resource ontology

### Construction

- OWNER (mutation): `Simulation` buildings / progress (not fully read); haul via transport
- OWNER (read/diagnostics): `src/sim/constructionPipeline.js` (snapshot/verdict, not the stepper)
- UPDATE_CADENCE: UNKNOWN exact progress writer
- TEST_SURFACE: UNKNOWN
- UNKNOWN: which function increments `building.progress` (claim_scope stop)

### Household / demography

- OWNER: `src/sim/life.js` `updateLifeDaily` → `runLifeHouseholdPhase` (`formCouples`, `createBirths`); `src/life/household.js`, `lineage.js`, `mortality.js`
- ENTRYPOINT: deferred job `lifeDaily` after midnight
- STATE_READ/WRITE: `sim.life.families`, npc partner/parent/child ids
- UPDATE_CADENCE: daily (guard `lastSocialDay`)
- DEPENDENCIES: `life/index.js` re-exports
- PUBLIC_INTERFACE: `updateLifeDaily`, `createLifeState`, `seedStartingFamilies`
- SIDE_EFFECTS: scenes, narrative hooks, orthodox observe-only V2/V3
- TEST_SURFACE: religion verifies V0–V3 (CURRENT_STATE)
- UNKNOWN: couple formation algorithm details (not required this pass)

### Social systems

- OWNER: daily `updateSocialOrdersDaily`, `oecumene`, `socialCycles`, `cohesionClimate`, `eraClimateBridge`, `socialAbduction`, `clioscope`, `collectivePriorities` (from `onNewDay` deferred)
- Tick-level: `src/ai/socialMemory.js`, `episodes.js`, `src/life/talk.js`
- UPDATE_CADENCE: mixed daily + tick (talk)
- TEST_SURFACE: UNKNOWN per-system this session
- UNKNOWN: which of these are load-bearing vs observatory

### Language / communication

- OWNER (session): `src/life/talk.js` (`beginTalkSession`, `holdTalkAct` from npc)
- OWNER (realization): `src/lang/fromNpc.js` `romaikaSpeechForNpc`; `fromSpeechAct.js` `uttFromSpeechAct`
- ENTRYPOINT: talk.js imports `romaikaSpeechForNpc`
- UPDATE_CADENCE: tick while talking
- DEPENDENCIES: phonology.js; speech acts
- PUBLIC_INTERFACE: talk.js + lang exports
- SIDE_EFFECTS: memory/history on npc
- TEST_SURFACE: PNEUMA verifies (CURRENT_STATE) — director vs bubble; bubble is render (excluded)
- UNKNOWN: PneumaBubbleDirector vs talk.js ownership split beyond CURRENT_STATE

### Memory

- OWNER: `src/ai/memory.js` (world knowledge), `socialMemory.js`, `failureMemory.js`, `episodes.js`
- ENTRYPOINT: `perceive` in `updateNpc`; daily `forgetStale` / `fadeEpisodes` deferred job `memory`
- UPDATE_CADENCE: think cadence + daily fade
- PUBLIC_INTERFACE: `createMind`, `perceive`, `believedStock`, …
- TEST_SURFACE: UNKNOWN
- UNKNOWN: mind schema completeness

### Persistence

- OWNER (pure): `src/sim/save.js` `serialize`/`deserialize` — world regenerated from seed + `tileDiff`
- OWNER (browser store): `src/sim/saveStore.js` localStorage keys `anastasis-save-v1`
- ENTRYPOINT: main.js import serialize/deserialize
- STATE_READ/WRITE: JSON document SAVE_VERSION 1 + GAME_VERSION `0.3.1`
- UPDATE_CADENCE: player save/load, not tick
- SIDE_EFFECTS: localStorage (browser-only)
- TEST_SURFACE: save/load player lineage (ACTIVE_MISSION history)
- UNKNOWN: Unreal save format decision (PORTAGE.md item 4; not decided here)

### Observability / testing

- OWNER (sim core): `src/sim/observability.js` — no-op unless `installObservability`
- OWNER (suite): `tools/test.mjs` (`npm test` / `test:quick`); `package.json` `"sim": "node tools/dev/headless.mjs"`
- SEAM: `tools/test.mjs` imports `src/render3d/animals3d.js` for `ANIMAL_BODY` ids — test harness, not sim tick. Not used as architecture.
- C++: `Anastasis.Sim.Parite.{Rng,RngHelpers,Hash,Math,SemantiqueJs,Horloge,GrilleSpatiale}` — not executed this session
- UNKNOWN: last green `test:quick` on this branch

### Player

- OWNER: `Simulation.playerPersonId` (null = observer). DEC-013.
- ENTRYPOINT: `incarnate` / `release` / `arriveAsPlayer`; movement `drivePlayerActor` when drive non-null
- UPDATE_CADENCE: tick
- SIDE_EFFECTS: disables Noûs decision for that body while driven
- TEST_SURFACE: PLAYER_DIRECT_CONTROL_001 (ACTIVE_MISSION complete on another base)

### Animals

- OWNER: `src/sim/animaux/updateAnimals.js` via `animaux/index.js`
- ENTRYPOINT: `Simulation.tick` → `updateAnimalsTick`; daily deferred `animalsDaily`
- UPDATE_CADENCE: tick with `animalTickMul` budget
- TEST_SURFACE: test.mjs animal catalogue vs WORLD_TILE_TYPES
- UNKNOWN: full species list

### Navigation

- OWNER: `src/sim/navGrid.js`, `navService.js`, `pathfinding.js`; tick `beginNavTick` / `processNavQueue`
- UPDATE_CADENCE: every hydrated tick; A* count-budgeted
- TEST_SURFACE: UNKNOWN
- UNKNOWN: grid resolution vs world tiles

## Runtime clock vs sim clock

`startGameLoop` is RAF (browser). `simClock.js` is the sim-step policy. Unreal replacement of RAF is presentation/engine, not a sim-owner change — see boundary doc.

## Dual-brain note

`ALGORITHMIC_NPC_V1` defaults true. Classic `chooseGoal` still exists and is used for player-controlled think and as fallback. Migration must not pick one silently. UNKNOWN which is the long-term authority.
