# ATMOSPHERE_002 — mist the simulation earns

Mission worktree `atmosphere-mist-002`, branched from `main` at `e96175f`
(ATMOSPHERE_001). Mandated explicitly by Alexandre as the follow-up named at the
end of that mission: tie fog to `Wetness`.

## What the global fog could not say

ATMOSPHERE_001 gave the world height fog: uniform, everywhere, tuned once. It
says *air has depth*. It cannot say *this valley is wet and that ridge is not*,
because it has no idea where the water is. The art direction asks for "mist
pockets" and "humid depth" — that is a statement about places, and a global
scalar cannot make it.

This mission makes the fog local and makes the simulation decide where.

## What `Wetness` actually is — read, not assumed

`Source/AnastasisSim/Private/World/AnastasisWorld.cpp`:

```cpp
const uint8 WD = WaterDist[I];                                    // tiles to nearest water
const double Wetness = WD == 0 ? 1.0 : Clamp(1.0 - WD / 6.5, 0, 1);
```

| tiles from water | 0 | 1 | 2 | 3 | 4 | 5 | 6 | ≥7 |
|---|---|---|---|---|---|---|---|---|
| `Wetness` | 1.00 | 0.846 | 0.692 | 0.538 | 0.385 | 0.231 | 0.077 | **0** |

So `Wetness` is **proximity to water**, not humidity. The noise-driven `Moist`
that decides where forest grows is never written to a tile — it does not exist
outside `GenerateWorld`'s locals. Consequence, stated rather than glossed: what
this mission produces is **river and shore fog**. It cannot appear on a dry
ridge, and it would be dishonest to present it as weather, as a humidity model,
or as anything that could later drive rain.

## Pockets, not tiles

The world is cut into cells of `MistCellTiles` (8 by default) and a cell whose
**mean** wetness clears `MistWetnessThreshold` gets exactly one
`ALocalFogVolume`. Three reasons, in order of importance:

1. **Mist is an area phenomenon.** One volume per tile would be a fog decal on a
   1 m square, which is not what mist is.
2. **A puddle is not a fog bank.** The threshold applies to the cell mean, so one
   soaked tile in an otherwise dry cell raises nothing —
   `Anastasis.Mist.Threshold` locks that.
3. **The count is bounded by construction**, before any cap: (96/8)² = 144 cells
   is the ceiling at the default, whatever the seed does. `MistMaxVolumes` is a
   second, explicit ceiling that truncates **by wetness** and says so in the log;
   a silently shortened fog field would read as a data bug months later.

Each pocket sits at the **wetness-weighted centroid** of its cell, not the cell's
geometric middle: on a cell straddling a ridge and a river, the middle is the
ridge, and fog on a ridge is exactly the wrong picture.

## No randomness at all

Unlike the tree dressing — which hashes `(Seed, TileX, TileY)` for jitter — this
file contains no hash, no jitter and no noise. Every pocket is a pure function of
the tiles under it. That is what makes the causal claim real: **the only way to
move the mist is to move the water**, and `Anastasis.Mist.Causality` fails if
that ever stops being true.

## Reuse: the mist lies on the rendered ground

Pocket height comes from `AnastasisTerrainSurface::SampleHeight`, the sampler the
terrain lane landed for the tree dressing — the height of the surface actually
rendered, not the tile altitude, which is a step where the ground is a slope. Its
refusal outside the built footprint is honoured: the pocket then keeps the tile
altitude it was born with rather than being dropped, because the DEBUG slab mode
has no continuous surface at all and mist should still exist there.

## Files

| File | Role |
|---|---|
| `Source/Anastasis_UnrealV2/WorldView/AnastasisMistField.h/.cpp` | New. Pure: snapshot + params -> pockets. No UObject, no actor, no engine state. |
| `Source/Anastasis_UnrealV2/WorldView/AnastasisMistFieldTests.cpp` | New. `Causality`, `Threshold`, `DeterminismAndBounds`, `Refusals`. |
| `Source/Anastasis_UnrealV2/WorldView/AnastasisAtmosphereProfile.h` | Mist section: cell size, threshold, radius, cap, extinction, falloff, phase, albedo, ground offset. |
| `Source/Anastasis_UnrealV2/WorldView/AnastasisWorldAtmosphere.h/.cpp` | `ApplyMist()`: builds the field, places `ALocalFogVolume`s, rebuilds wholesale. New CVar `anastasis.Atmosphere.Mist`. |
| `Source/Anastasis_UnrealV2/Anastasis_UnrealV2GameMode.cpp` | Calls `ApplyMist()` after the embodiment — the ordering is written where it is visible. |
| `Source/Anastasis_UnrealV2/WorldView/AnastasisWorldProbeSubsystem.cpp` | `mist_volume_count` in the snapshot's `atmosphere` block. |
| `tools/unreal/atmosphere-profile.py` | Seeds and verifies the new mist fields. |

No file under `Source/AnastasisSim/` was opened for writing — `Wetness` is read,
never computed here. No `.umap`, no change to the WorldView adapter (`FVisualTile`
already carried `Wetness`), so nothing collides with the asset or terrain lanes.

## Two decisions that differ from the rest of the atmosphere

**Mist rebuilds; the rig adopts.** `Apply()` adopts the level's existing sun and
sky because those are singletons the level may legitimately own. `ApplyMist()`
does the opposite: it destroys its previous pockets and lays a fresh field.
Adopting here would mean a second pass stacking a new fog bank on every old one
and doubling the extinction with nothing in the world having changed.

**Mist runs after the world, not before.** Sun and sky are properties of the
level and can be set before anything exists. Mist is read from the tiles, so it
needs `AAnastasisWorldEmbodiment` to have embodied first. That ordering lives in
the game mode where it can be seen, not hidden inside a `BeginPlay`; and
`ApplyMist()` finds the embodiment itself, so an editor button or a script works
without the game mode.

## Not done, deliberately

- **Volumetric fog, light shafts through the mist.** `ALocalFogVolume` renders in
  its own cheap pass (`r.LocalFogVolume`, on by default) and does not require
  volumetric fog. Turning volumetric fog on to get shafts is a real GPU cost that
  should be argued for against a capture, not assumed.
- **Mist that thickens at dawn or with a season.** Nothing drives time of day yet
  (see ATMOSPHERE_001's derived sun, still off by default). When a clock lands,
  the density scale is one multiplication away.
- **Using `FlowAmt` / `FlowX` / `FlowZ`.** The tiles carry a flow field, which is
  the honest source for *river* fog specifically, as opposed to shore fog. It is
  a real next step, not this mission's.

## CVars

```
anastasis.Atmosphere        0 = the level's own lighting, 1 = the profile (default)
anastasis.Atmosphere.Mist   0 = no pockets, the global atmosphere untouched
                            1 = pockets from wetness (default)
```

## Evidence

Run in the `atmosphere-mist-002` worktree, 2026-09-13, against its own binaries,
**after rebasing onto `main` at `5321a97`** -- twelve commits landed from the asset
and observatory lanes while this mission was in flight, including a game-mode change
(`ShouldSpawnEmbodiment`) that conflicts directly with where the mist call sits. It
was merged so the mist runs in BOTH cases: an embodiment already placed in the level
is exactly as wet as one the game mode spawns, and `ApplyMist()` finds it either way.
The proof below was retaken on the rebased tree, including a runtime run that still
reports `pockets=69`.

```
BUILD::PASS                      tools\unreal\agent-worktree.ps1 finish
TESTS::PASS                      tools\unreal\report-tests.ps1
  PASS                   : 52    (the four below are this mission's)
  KNOWN_EXPECTED_FAILURE : 4     (the registered ones, unchanged)
  FAIL                   : 0
  Anastasis.Mist.Causality             Success
  Anastasis.Mist.Threshold             Success
  Anastasis.Mist.DeterminismAndBounds  Success
  Anastasis.Mist.Refusals              Success
```

Runtime, PIE on `/Game/FirstPerson/Lvl_FirstPerson`, canonical world 96x96 at seed 12345:

```
ANASTASIS_MIST pockets=69 cell_tiles=8 threshold=0.350 crop=96x96 ground_sampled=69 max_extinction=0.650 truncated=0
ANASTASIS_MIST pockets=0 reason=cvar_off
```

69 pockets out of the 144 cells the grid allows — so the world is wet in roughly
half its cells, the cap of 192 was never approached, and `ground_sampled=69` says
every single pocket found the rendered surface rather than falling back to tile
altitude. Controlled A/B at the `SHORE` bookmark, everything else held equal:

| | `anastasis.Atmosphere.Mist 0` | `= 1` |
|---|---|---|
| `mist_volume_count` | **0** | **69** |
| sun intensity | 75 000 | 75 000 |
| global fog density | 0.0120 | 0.0120 |
| exposure | fixed, EV100 14 | fixed, EV100 14 |
| snapshot `errors` | `[]` | `[]` |

Images in `docs/visual/atmosphere-002/`. What they show, stated as seen: in the
mist-on frame the distant band above the shoreline is veiled — trees and ground
softened — while the near ground stays crisp. That is what shore fog should look
like, and it is restrained rather than spectacular by choice.

## The capture tool is not trustworthy, and I could not fix it here

This mission hit the capture flake twice more, and a second, distinct one. Both
are in shared tooling, both predate this mission, and both matter more than they
look, because **a verdict of this project is an image**.

**Defect 1 — the shot can photograph the editor instead of the game.** Observed
once in ATMOSPHERE_001 and twice here. The JSON written beside it is correct
(`map=UEDPIE_0_Lvl_FirstPerson`, the right actor counts), so only the screenshot
lands on the wrong window.

I tried the obvious fix and **it does not work**:
`FScreenshotRequest::RequestScreenshot` takes `bInRestrictToGameViewport`, whose
own documentation describes this exact symptom ("will not include the entire
editor in PIE"). Passing `true` changed nothing, and the engine source says why —
`UGameViewportClient::ProcessScreenshotRequest` consults
`FScreenshotRequest::ShouldRestrictToGameViewport()` **only inside its
`bShowUI == true` branch** (`GameViewportClient.cpp:2397`), and this project
requests with `bShowUI = false`. On our path the flag is dead code. The change
was reverted rather than shipped: an inert fix carrying a confident comment is
worse than a known bug, because the next person stops looking.

A real fix has to target the game viewport explicitly rather than issue a global
request — that is a mission, not a side quest, and it is the highest-leverage one
left in this lane.

**Defect 2 — the shot can be taken mid camera-blend.** One capture here came back
motion-blurred, the camera still travelling toward the bookmark. The settle timer
before the request is a fixed 0.5 s (`RequestCapture`), which is a guess, not a
guarantee; and motion blur is left on during a verdict capture.

Until both are fixed: **look at every image before filing it as evidence.** Both
A/B frames filed under `docs/visual/atmosphere-002/` were checked by eye, and the
mist-on frame is a re-run after the first attempt photographed the editor.
