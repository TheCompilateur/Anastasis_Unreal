# ATMOSPHERE_001 — the world's air

Mission worktree `atmosphere-light-001`, branched from `main` at `30120fc`
(`feat(worldview): data-driven presentation — Forest/Ruin look comes from an asset`).

## Why this mission, and why it does not collide with the other three

Four visual lanes were live when this started, and their branches say exactly
what they own:

| Lane | Branch | Owns |
|---|---|---|
| Assets | `agent/asset-agent-001`, `-002` | real StaticMeshes (`SM_Tree_Generic_01`), Geometry Script authoring |
| Terrain | `agent/terrain-surface-world`, `agent/dressing-on-surface` | `AnastasisTerrainSurface`, surface over the whole crop, dressing placed on it |
| Presentation | `agent/visual-build-002` (in `main`) | `UAnastasisPresentationRegistry`: which mesh a semantic type draws |
| **Atmosphere** | **`agent/atmosphere-light-001`** | **sun, sky, fog, exposure** |

`git grep -i atmosphere -- Source/` across every branch returned nothing before
this mission: no C++ file anywhere in the project mentioned light, sky, fog or
exposure. The only lighting that existed was hand-spawned by
`tools/unreal/observe-slice.py` into one editor-built level,
`/Game/Anastasis/Maps/Lvl_AnastasisSlice`:

```python
sun = spawn('/Script/Engine.DirectionalLight', Vector(0,0,4000), Rotator(0.0, -38.0, -55.0))
sun_comp.set_intensity(SUN_LUX)            # 75000
sun_comp.set_editor_property('atmosphere_sun_light', True)
spawn('/Script/Engine.SkyAtmosphere', ...)
sky.get_component_by_class(SkyLightComponent).set_editor_property('real_time_capture', True)
ppv.settings.auto_exposure_min/max_brightness = EV100   # 14
```

Consequences this mission removes:

1. **The playable level was never lit by ANASTASIS.** `Lvl_FirstPerson` — the PIE
   target named in `UNREAL_CANONICAL_STATE.md` — ran on the FirstPerson
   template's lighting. Two levels, two unrelated looks, neither authored.
2. **Light was not data.** Changing the sun meant editing a Python script that
   only runs when the observation level does not yet exist.
3. **No one owned the fog.** The observation rig has none at all. The playable
   level carries the FirstPerson template's `ExponentialHeightFog` — measured,
   not assumed: `density=0.0436, start_distance=0, max_opacity=1.0, z=-6850`
   (`atmosphere-off` snapshot below). Nobody chose those numbers for ANASTASIS,
   nothing reproduces them, and `max_opacity=1` with `start_distance=0` is the
   opposite of what the art direction asks for — haze that starts at the lens
   and can fully paint over the horizon, rather than a clear near field and
   depth in the distance.
4. **The probe could not see light.** `anastasis.world_snapshot.v1` reported
   actors, terrain, water, camera and navigation — nothing about whether the
   world was lit. A capture could not distinguish a lit world from a black one.

## What was built

```
UAnastasisAtmosphereProfile            data: sun / sky / fog / exposure
        (/Game/Anastasis/Presentation/DA_AnastasisAtmosphere, seeded by
         tools/unreal/atmosphere-profile.py, editable in-editor)
        |
        v
AnastasisAtmosphere::GetProfile()      asset, else code defaults (fail-closed, logged)
AnastasisAtmosphere::ResolveSunRotation explicit angles, or solar geometry from time of day
        |
        v
AAnastasisWorldAtmosphere::Apply()     adopt existing actors, spawn only what is missing
        |
        +-- DirectionalLight     rotation, lux, colour, shadows, atmosphere-sun flag
        +-- SkyAtmosphere
        +-- SkyLight             real-time capture, intensity
        +-- ExponentialHeightFog density, falloff, start distance, max opacity, inscattering
        +-- PostProcessVolume    unbound, auto-exposure min = max = EV100
        |
        v
AAnastasis_UnrealV2GameMode::BeginPlay spawns it before the embodiment (anastasis.Atmosphere=1)
UAnastasisWorldProbeSubsystem          new "atmosphere" block in the snapshot JSON
```

| File | Role |
|---|---|
| `Source/Anastasis_UnrealV2/WorldView/AnastasisAtmosphereProfile.h/.cpp` | New. The `UDataAsset`: every value an art pass edits. `CreateCodeDefaults()` is the fallback. |
| `Source/Anastasis_UnrealV2/WorldView/AnastasisAtmosphereResolver.h/.cpp` | New. Profile load + fail-closed fallback + `SunRotationForTimeOfDay`. Pure, no actor state. |
| `Source/Anastasis_UnrealV2/WorldView/AnastasisWorldAtmosphere.h/.cpp` | New. The actor: adopt-or-spawn, apply, log, `DestroySpawnedActors()`. |
| `Source/Anastasis_UnrealV2/WorldView/AnastasisAtmosphereTests.cpp` | New. `RigParity`, `SunFromTime`, `ProfileFallback`, `Idempotence`. |
| `Source/Anastasis_UnrealV2/Anastasis_UnrealV2GameMode.cpp` | Spawns the atmosphere before the embodiment, behind `anastasis.Atmosphere`. |
| `Source/Anastasis_UnrealV2/WorldView/AnastasisWorldProbeSubsystem.cpp` | New `atmosphere` block in the snapshot; `ATMOSPHERE_NO_SUN` error when a world has no sun. |
| `tools/unreal/atmosphere-profile.py` | Source of authority for `DA_AnastasisAtmosphere`, same convention as `presentation-registry.py`. |

No file under `Source/AnastasisSim/` was opened for writing. No `.umap` was
modified. The sealed `TERRAIN_CONTRACT` is untouched: this mission adds no
vertex, no triangle and no tile.

## Three decisions worth arguing with

**1. The code defaults ARE the observation rig, value for value.**
Sun pitch −38, yaw −55, 75 000 lux, atmosphere-sun on, real-time-capture sky
light, exposure pinned at EV100 14. That rig is the only lighting ANASTASIS has
ever taken comparable captures under, so the new owner starts at the known-good
image instead of at somebody's taste. `Anastasis.Atmosphere.RigParity` fails if
a default drifts, which is what makes it safe for code to take the light over
from the Python script. The one value the rig never had is fog.

**2. Adopt before spawn.**
`Apply()` looks for an existing `DirectionalLight` / `SkyAtmosphere` / `SkyLight`
/ `ExponentialHeightFog` / `PostProcessVolume` and configures *that*, spawning
only what is missing, and records which actors it created. So:
- the observation rig keeps working — its actors are adopted and rewritten with
  identical values, so existing evidence stays comparable;
- `Lvl_FirstPerson` gains an authored sun, authored fog values and a fixed
  exposure instead of a second sun stacked on the template's — measured:
  `adopted=4 spawned=1`, the four being sun / sky atmosphere / sky light / fog,
  the one spawned being the unbound `PostProcessVolume` the level never had;
- applying twice is idempotent, proven by `Anastasis.Atmosphere.Idempotence`. A
  world that gained a sun per `Apply()` would double its exposure and silently
  invalidate every capture taken afterwards.

Spawned actors are `RF_Transient`: the atmosphere can never be saved into a
level asset by accident.

**3. The derived sun exists, tested, and is OFF by default.**
`SunRotationForTimeOfDay(hours, latitude, declination)` is real solar geometry
(hour angle, declination, UE axes X=north/Y=east, light pointing along the
rays), locked by `Anastasis.Atmosphere.SunFromTime`: noon elevation is exactly
90 − latitude, morning light comes from the east, 9 h and 15 h mirror about
noon, summer noon is higher than equinox noon. The profile still uses its
explicit angles because **nothing in ANASTASIS drives a time of day yet** —
`AnastasisSimClock` exists but no world clock reaches presentation. Turning the
derivation on by default would move the sun away from the angle every existing
capture was taken at, for a day/night cycle nothing can currently request. The
seam is the deliverable; flipping `bDeriveSunFromTimeOfDay` is a one-line change
for whoever lands the clock.

## Not done, deliberately

- **Volumetric fog, light shafts, mist pockets, cloud layers.** The art
  direction warns against "LUT/bloom used to counterfeit material quality" and
  "universal god rays". Height fog is the cheapest honest depth cue; anything
  more expensive should be argued for against a capture, not added on faith.
- **Day/night cycle.** No simulation clock reaches presentation (above).
- **Per-biome or weather-driven atmosphere.** `AnastasisWorld::FTile` carries
  `Wetness`, which is a real hook for local mist — but tying atmosphere to tile
  data is a design decision with simulation-side implications, out of this
  mission's mandate. Flagged, not taken.
- **Touching `Lvl_AnastasisSlice` or `Lvl_FirstPerson`.** The atmosphere is
  applied at runtime from code; no `.umap` is edited, so no binary asset
  conflicts with the other three lanes.

## CVar

```
anastasis.Atmosphere   0 = leave the level's own lighting alone (pre-ATMOSPHERE_001 image)
                       1 = apply the profile (default)
```

Read once, when the game mode spawns the world. The escape hatch for anyone who
needs to A/B against the old image without editing data.

## Evidence

Run in the `atmosphere-light-001` worktree, 2026-09-13, against its own binaries,
**after rebasing onto `main` at `f16cf10`** — `main` moved 15 commits
(asset-agent-001, world-observatory, unreal-guardian and dressing-on-surface all
integrated, plus two ops fixes) while this mission was in flight, so the first run's proof no longer
described what is on disk and was retaken rather than reported. The only conflict
was one `#include` line in `AnastasisWorldProbeSubsystem.cpp`; both sides kept.

```
BUILD::PASS                      tools\unreal\agent-worktree.ps1 finish
TESTS::PASS                      tools\unreal\report-tests.ps1
  PASS                   : 46    (the four below are this mission's)
  KNOWN_EXPECTED_FAILURE : 4     (the registered ones, unchanged)
  FAIL                   : 0
  Anastasis.Atmosphere.RigParity        Success
  Anastasis.Atmosphere.SunFromTime      Success
  Anastasis.Atmosphere.ProfileFallback  Success
  Anastasis.Atmosphere.Idempotence      Success
```

Data asset, created headlessly by `tools/unreal/atmosphere-profile.py`:

```
ATMOSPHERE_PROFILE CREATE /Game/Anastasis/Presentation/DA_AnastasisAtmosphere
ATMOSPHERE_PROFILE VERIFY enabled=True sun_pitch=-38.000 sun_yaw=-55.000 lux=75000.0 atmo_sun=True
ATMOSPHERE_PROFILE VERIFY derive_sun=False time=12.00 lat=41.00 decl=0.00
ATMOSPHERE_PROFILE VERIFY fog=True density=0.0120 falloff=0.200 start=1500.0 max_opacity=0.85
ATMOSPHERE_PROFILE VERIFY fixed_exposure=True ev100=14.00 sky_rtc=True
```

Idempotence, from the automation run — same instance, two passes:

```
ANASTASIS_ATMOSPHERE applied=1 profile=asset ... adopted=4 spawned=1
ANASTASIS_ATMOSPHERE applied=1 profile=asset ... adopted=5 spawned=0
```

Runtime A/B in PIE on `/Game/FirstPerson/Lvl_FirstPerson`
(`tools\unreal\probe-demo.ps1 -Mission <m> -Bookmark <b> [-PreCmds 'anastasis.Atmosphere 0']`),
read back from the new `atmosphere` block of `anastasis.world_snapshot.v1`:

| | `anastasis.Atmosphere 0` (before) | `= 1` (after) |
|---|---|---|
| sun intensity | **6** | **75 000** lux |
| sun rotation (P,Y,R) | −49.52, −10.31, **112.36** | −38.00, −55.00, **0** |
| exposure | **auto** (not pinned) | fixed, EV100 14, unbound volume |
| fog density | 0.0436 | 0.0120 |
| fog start distance | 0 | 1500 uu |
| fog max opacity | 1.00 | 0.85 |
| fog height Z | −6850 | 0 |
| profile source | asset | asset |
| snapshot `errors` | `[]` | `[]` |

Captures, retaken on the rebased tree
(`Saved/Anastasis/Captures/<mission>/<bookmark>/`, versioned copies in
`docs/visual/atmosphere-001/`):

```
r2-overview-off/OVERVIEW/20260913-205324.png   r2-overview-on/OVERVIEW/20260913-205403.png
r2-ground-off/GROUND/20260913-205442.png       r2-ground-on/GROUND/20260913-205521.png
```

What the images actually show, stated as seen and not as hoped:

- **Directional light and cast shadows appear.** At 6 lux with auto-exposure the
  template sun modelled nothing; the OVERVIEW pair is the clearest — trees gain
  long cast shadows, ground relief gains a lit and a shaded face.
- **The near field clears and the distance recedes.** At GROUND, `start_distance`
  1500 uu leaves the foreground crisp while the far ridge hazes — the template's
  `start_distance=0` fogged everything equally, including the lens.
- **Exposure is now reproducible.** Two captures of the same world are
  comparable for the first time outside the observation rig.
- **The fog is not visible from OVERVIEW** (camera at Z≈10047, fog anchored at
  Z=0 with falloff 0.2). It is a ground-level cue. Claiming an atmospheric gain
  in the overview pair would be false; the gain there is the sun.

One flake worth recording, seen once during the first round: a `GROUND` run
captured the editor viewport instead of the PIE viewport. Its JSON was correct
(`map=UEDPIE_0_Lvl_FirstPerson`, `sun_intensity=75000`), so only the screenshot
landed on the wrong window; the re-run captured correctly, and all four images
of the final round are PIE. This is pre-existing capture tooling behaviour, not
an atmosphere defect, and it is a candidate for a separate fix — a capture that
can silently photograph the wrong window is a weak link in a project whose
verdicts are images.

## Tooling repaired along the way

`tools/unreal/probe-demo.ps1` hardcoded `$Root = 'C:\dev\ANASTASIS_UNREAL'`,
so an agent proving work in a worktree was in fact launching the canonical
root's binaries — i.e. measuring somebody else's code. It now derives its root
from `$PSScriptRoot` like `capture-slice.ps1`, and gained `-PreCmds` (CVars
before launch, which is what makes an A/B possible), `-Bookmark` and
`-TimeoutSec`. `tools/unreal/probe-demo.py` reads the bookmark from
`ANASTASIS_PROBE_BOOKMARK`, defaulting to `OVERVIEW` as before.
