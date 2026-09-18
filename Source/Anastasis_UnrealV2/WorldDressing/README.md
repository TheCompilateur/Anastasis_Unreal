# World Dressing V0

Independent, opt-in editor preview; all implementation and proof tooling live in this folder.
No changes to WorldView, terrain generation, simulation, navigation, buildings or existing dressing.
The folder belongs to the existing gameplay module: no module/uproject/Build.cs changes required.

## Designer workflow

1. Open the built **world-dressing-v0 worktree** project (the canonical project does not contain this candidate until integrated).
2. Open an existing map with `AnastasisWorldEmbodiment` and its visible terrain surface.
3. Content Browser > Miscellaneous > Data Asset > `AnastasisWorldDressingProfile`.
4. Add Rules, each with a unique AssetId and a StaticMesh reference. Unassigned meshes are reported and skipped.
5. Place `AnastasisWorldDressingManager` from the Place Actors class list or C++ Classes.
6. Assign Profile and the existing WorldSource actor. Choose Seed.
7. Press **Generate Preview** in Details. **Clear Preview** returns to the baseline. **Rebuild From Seed** repeats generation; **Print Placement Report** writes counts/hash/validation to Output Log.

The manager does not generate automatically. Editing/moving the manager clears its preview.
Editing the source world or the profile does not automatically update instances: explicitly rebuild.
The manager transform does not offset the map: instance transforms are anchored to WorldSource.
Preview components and reports are transient and excluded from serialization/duplication; the manager is editor-only.
Saving the manager configuration/profile is a designer action; generated props do not bake into the map.
There is no Bake feature in V0. No collision, overlap events, ticking, navigation contribution or prop actors.

## Rule units and boundaries

- Terrain families mirror semantic Grass, Water, Stone, Ruin, Forest, Scrub, Field, Road. Empty = all land.
- Altitudes and distances are Unreal centimeters in source space; slope uses geometric surface normals in degrees.
- Density = expected candidates per 100x100cm semantic tile BEFORE filters, limited to 0..16.
- ClusterRadius 0 gives uniform scatter. Positive values scatter candidates in a disk around a seeded point per tile; the destination tile is revalidated.
- Scale is uniform and positive. Mesh lowest local Z is placed at the surface along its rotated up axis.
- RandomYaw rotates around aligned up; AlignToSurfaceNormal is optional.
- AvoidWater adds 25cm shoreline clearance. V0 always forbids submerged/semantic-water roots, including when this margin is disabled.
- DistanceToWater uses XY distance to semantic water tile rectangles, not geodesic distance or exact forged shoreline. Maximum -1 = unlimited. A finite maximum rejects a map with no water.
- ClearanceRadius checks the root and an eight-point support ring. This is not full mesh/canopy collision checking. Large crowns can overlap each other; V0 has no inter-prop spacing solver.
- Road exclusions read Road tiles plus actors tagged `WorldDressingRoad` or assigned in RoadExclusions.
- Building exclusions read `AAnastasisVillageBuilding`, actors tagged `WorldDressingBuilding`, and BuildingExclusions. XY bounds are expanded by ExclusionPadding (default 100cm, also covers logical buildings without mesh bounds).
- The snapshot contains no building mask; unsupported building/road representations need explicit exclusion actors. Absence of these inputs is not proof of complete village clearance.
- Ground sampling reads visible `ExperimentalTerrain` section 0, including forged heights. Water section 1 is never sampled as ground. Missing/hidden surface or invalid snapshot fails explicitly; no synthetic fallback terrain.
- Source translation is supported. Rotated/scaled sources are rejected in V0, avoiding incorrect slope/water metrics.
- Limits: 128 rules and 100,000 instances maximum (default 20,000). Reaching the instance cap is reported.

## Determinism and reporting

Fixed row-major traversal and per-AssetId FRandomStream; no global random source or unordered map iteration.
Same seed + profile/order + snapshot + rendered geometry + source transform + exclusions + mesh bounds on the same engine produce identical placements.
The placement hash is versioned SHA-1 over AssetId, asset path, and final world transforms. Positions/scales quantized to 1e-4; quaternion to 1e-7.
It is a placement comparison checksum, not a content fingerprint: changing a mesh's materials alone does not change it, and two empty previews have the same hash.
Report: counts by AssetId, HISM count, rejected candidates, unassigned assets, cap, root validation and hash.
The report describes the last generation, not later source edits. Zero water/out-of-bounds applies to roots/support samples, not every vertex of a tilted mesh.

## Build and tests

From the worktree root:

```powershell
tools/unreal/anastasis-unreal.ps1 build
tools/unreal/report-tests.ps1 -Filter Anastasis.WorldDressing
```

Tests cover exact repeat hash, seed sensitivity, bounds, geometry height, clumping, count cap, terrain/altitude/slope/water-distance filters, road/building toggles, unassigned assets, malformed input, and mandatory water rejection.

## Real before/after proof

`capture_preview.py` runs inside a dedicated editor, using existing tree and ruin meshes with a transient profile.
It keeps the existing map/dressing, captures before/after at two fixed cameras, independently hashes HISM transforms, verifies Rebuild/Clear and actor counts, and saves **no map or asset**.
Output: four PNGs, `observation.json`, and the launcher log. Use `launch_preview.ps1` below. `-KeepOpen` leaves the preview selected in the dedicated editor for inspection.
Only trees and existing ruin pieces are demonstrated; bushes/reeds/deadwood/rocks can be assigned when suitable assets are available. No substitute final assets are fabricated.
