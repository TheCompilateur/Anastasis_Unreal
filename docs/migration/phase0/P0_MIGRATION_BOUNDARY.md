# P0-E Migration boundary

No implementation language/bridge chosen. No rewrite. DEC-001 in JS `docs/ai/DECISIONS.md` still **Active** ("Maintenir la pile Three.js/WebGL"). Workspace + PORTAGE.md + this Phase 0 mission declare Three.js legacy. Those claims are **not collapsed**.

```
WORKSPACE_EXCLUSION::OBSERVED   (Cursor files.exclude + rule js-sim-reference.mdc)
REPOSITORY_EXCLUSION::NOT_PERSISTED   (JS .cursorignore has no src/render3d/)
DECISIONS.md DEC-001::STILL_ACTIVE_THREEJS
CURRENT_STATE.md::STILL_DOCUMENTS_THREEJS_PILE
```

## KEEP AS AUTHORITY (simulation truth)

Behavioral source: JS `src/sim`, `src/life`, `src/ai`, `src/lang`, `src/runtime` (simClock + resilience policy, not RAF itself).

Especially:

- Determinism: `rng.js` mulberry32, seed, `_playerRng` isolation, deferred daily **job counts** not wall-ms
- World gen: `world.js` + hydrology/archetypes/crops
- `Simulation` state: actors, buildings, colony, market-as-view, time/day
- NPC decide/act: `npc.js` + `ai/*` (two brains: classic + Noûs)
- Needs, household, mortality, careers
- Stock ledger + economy prices
- Save **document** (`save.js` serialize/deserialize) — format reuse vs new Unreal save = UNKNOWN / later DEC
- Talk session + `lang/` realization (text truth)
- Invariants in `.agent/manifest.json` (needs-on-arrival, no frozen actor, sites reachable, buildings vs sites, budget throttle≠disable)
- Game rules / DEC-008–015 (foundation, player-as-habitant, orthodox V0–V3/V5, fasting unreachable)

C++ already mirroring a **subset**: `AnastasisSim` couche 0 (numeric/rng/math/clock/spatial). Authority remains JS until parity tests pass on a measured run.

## PORT / REIMPLEMENT LATER

Order in `PORTAGE.md` is a **proposal**, not a Phase 0 decision:

1. world gen  
2. nav  
3. budget + logical LOD  
4. save structures  
5. `simulation.js` tick/onNewDay (do not translate as one file)  
6. life + ai  

Also later: `src/sim/animaux`, transport, construction **mutation** path (owner of progress still UNKNOWN), social daily systems, lang/talk.

Do not start these in Phase 0.

## LEGACY

- `src/render3d/` (~137k DECLARED) — Three.js presentation. Do not port. Do not use as Unreal architecture.
- `src/render/` 2D canvas renderer imported by `main.js` — browser presentation.
- `src/ui/`, `index.html`, `styles.css` — browser shell/HUD.
- `saveStore.js` localStorage — browser persistence I/O.
- `src/runtime/gameLoop.js` `requestAnimationFrame` — browser frame pump.
- `src/runtime/faultShield.js`, `flightRecorder.js`, `watchdog.js`, `crashReport.js`, `postMortemUi.js` — PORTAGE.md: redesign on Unreal, do not translate.
- Epic template gameplay in **both** Unreal projects (Shooter/Horror/Combat/Platforming/SideScrolling/FirstPerson/ThirdPerson) — sample code, not Anastasis village. Keep until replaced; not sim authority.
- DEC-001 Three.js decision text — documentation lag vs this mission; do not silently rewrite.

Test seam: `tools/test.mjs` imports `src/render3d/animals3d.js` for body ids. Harness coupling. Not a reason to port Three.js.

## UNREAL-NATIVE RESPONSIBILITY

- Viewport, camera, PIE, packaging
- Landscape / Nanite / ISM / World Partition / lighting / atmosphere
- Skeletal/static meshes, animation, audio middleware
- UMG equivalent of HUD (new, not HTML)
- StateTree as **presentation/AI controller** only if it does not replace sim decision authority (not designed here)
- Editor MCP / Python inspect (`AnastasisInspectTools`) — agent perception of the scene
- `Anastasis_UnrealV2` module: Engine-facing; may depend on `AnastasisSim`, never reverse (`AnastasisSim.Build.cs`)

Weather **visuals** = Unreal. Weather **function** `weatherAt(seed,time)` is sim-pure and already affects movement in `simulation.js` — keep function as authority; do not let Niagara own rain gameplay.

## UNKNOWN (do not invent)

- Bridge: embed JS vs port C++ vs both. PORTAGE.md assumes C++ bit-parity; not sealed as DEC.
- Whether JS saves must replay bit-identical in Unreal (PORTAGE.md wants it; no DECISIONS.md entry).
- Canonical Unreal root (see canonical report).
- Dual NPC brains (classic vs Noûs) which survives.
- Construction progress writer.
- 5.7 `AnastasisCore` vs V2 `AnastasisSim` merge/abandon.
- Audio `src/audio/` (Cantus) — sim-adjacent vs presentation; not traced this pass.
- `src/debug/` observatory — tooling vs product.
