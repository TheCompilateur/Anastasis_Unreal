# Visual pipeline audit — ANASTASIS_UNREAL_VISUAL_BUILD_001

Worktree `multi-agent-control-001`, branched from `main` at `0011190`
(`docs(slice-006): WORLD_SLICE_006::SEALED on 2c0b330`). Read-only audit of
Phase 0–2 plus the one additive change this mission makes (Phase 3). No
simulation file under `Source/AnastasisSim/` is touched.

## Governance note

`docs/unreal/WORLD_SLICE_006_SEAL.md` closes WORLD_SLICE_006 and states
`NEXT_PHASE_AUTHORIZED: Aucune`, listing forest, PCG, buildings, NPCs and
navigation as out of mandate pending explicit authorization from Alexandre.
This mission brief is treated as that authorization for the presentation layer
specifically — not for PCG, gameplay, navigation, or any `Source/AnastasisSim/`
change, all of which remain untouched here. The work below is additive only:
it does not alter `AnastasisTerrainSurface`'s sealed `TERRAIN_CONTRACT`
(vertices=1024, triangles=1922) or any `AnastasisSim` file.

## Current pipeline (Phase 1)

```
AnastasisSim::AnastasisWorld::GenerateWorld(seed, 96, 96)   -- simulation truth
        -> FTile[] (Type, Alt, Shade, Shore, Wetness, Resource, ...)
AnastasisWorldView::CaptureCanonicalWorld / CropSnapshot     -- adapter, no new data
        -> FWorldVisualSnapshot (32x32 canonical crop) -> FPlan (Locations/Types/Alts)
        |
        +-- AAnastasisWorldEmbodiment, mode DEBUG (anastasis.Terrain.Surface=0)
        |     7x UHierarchicalInstancedStaticMeshComponent, one per ETileType,
        |     Engine Cube mesh, flat AnastasisWorldDebugVisual::TerrainDebugColor tint.
        |     THIS is the "square/color placeholder" the mission's premise refers to.
        |
        +-- AnastasisTerrainSurface::Build + AAnastasisWorldEmbodiment, mode Surface (=1)
              UProceduralMeshComponent, 1024 shared vertices / 1922 triangles, per-vertex
              FLinearColor from AnastasisTerrainSurface::TileColor (type + altitude +
              shore + water-depth blend) + a flat water section at AnastasisWorld::SeaLevel.
              Sealed by WORLD_SLICE_006 (TERRAIN_CONTRACT / TERRAIN_SEMANTICS).
```

Lighting/atmosphere: no C++ owns this. `/Game/Anastasis/Maps/Lvl_AnastasisSlice`
(created and owned by `tools/unreal/observe-slice.py`, the asset's declared
source of authority) carries a DirectionalLight (75000 lux,
`atmosphere_sun_light=true`), SkyAtmosphere, SkyLight (real-time capture) and a
PostProcessVolume with fixed exposure (EV100=14) — a controlled A/B
observation rig, not the playable level. `/Game/FirstPerson/Lvl_FirstPerson`
(PIE target per `UNREAL_CANONICAL_STATE.md`) uses the template's own lighting;
`AAnastasis_UnrealV2GameMode::BeginPlay` spawns `AAnastasisWorldEmbodiment`
into whichever level is loaded when `anastasis.Visual.Mode` resolves to
`Debug` (`AnastasisVisualMode.cpp`; `Player` is `NOT_IMPLEMENTED`).

Per-subsystem classification:

| Subsystem | State | Notes |
|---|---|---|
| Terrain generation | IMPLEMENTED | `AnastasisWorld::GenerateWorld`, untouched |
| Terrain representation | IMPLEMENTED (sealed) | vertex-colored `ProceduralMeshComponent`, no texture |
| Water | IMPLEMENTED (sealed) | flat plane at `SeaLevel`, vertex-colored, no waves/refraction (documented `KNOWN_VISUAL_LIMITS`) |
| Biome color | IMPLEMENTED (sealed) | `TileColor` blend; duplicated (not shared) with the DEBUG palette by design — two independent, both-tested tables |
| Forest | **PLACEHOLDER → NOW PARTIAL** | was flat color/cube only; this mission adds discrete tree instances (see below) |
| Buildings | **PLACEHOLDER → NOW PARTIAL, narrow** | no `House`/`Market`/`Workshop` semantic exists anywhere in `AnastasisSim`; `ETileType::Ruin` is the only real structure-adjacent semantic (see REACHABILITY_GATE) |
| Roads | **MISSING** | zero source data — see REACHABILITY_GATE, not implemented |
| Lighting/atmosphere | IMPLEMENTED | scripted rig in `Lvl_AnastasisSlice`; template default in `Lvl_FirstPerson` |
| Camera | IMPLEMENTED | Editor `CameraActor` (slice rig) + template first-person pawn |
| Instancing | IMPLEMENTED | HISM already the pattern for all seven ground types; extended, not replaced |
| Collision (visual) | IMPLEMENTED | `QueryAndPhysics` / `BlockAll` on ground; navigation deliberately off |
| LOD/HLOD | NOT PRESENT | not justified at current scale (32x32 tiles = 32m x 32m) |
| Streaming / World Partition | NOT PRESENT | not justified — single small level, no case made for it (COMPLEXITY_MUST_EARN_EXISTENCE) |
| Presentation resolver (WorldSemanticState -> RenderableDefinition) | **MISSING → ADDED** | this mission's Phase 2/3 deliverable, see below |

## REACHABILITY_GATE — buildings and roads

`Source/AnastasisSim/Public/World/AnastasisWorld.h` defines exactly seven
`ETileType` values: `Grass, Water, Stone, Ruin, Forest, Scrub, Field`. There is
no settlement, building, structure-category, or road/path concept anywhere in
`AnastasisWorld::FTile`, `FWorld`, or `GenerateWorld`. Confirmed by reading the
full generation function (`AnastasisWorld.cpp`): tile type is chosen purely
from altitude vs. `SeaLevel`, a rock/stone threshold, a `Ruin` fbm threshold
(`RuinSeed=3041`, `Arch.RuinT` ~0.70–0.76), a forest/moisture threshold, and a
scrub/field split. `Ruin` is real, deterministic, land-only simulation truth
(`Resource=Stone, Amount=8`) — the closest thing to "a structure" the
simulation currently expresses. Nothing else is.

Consequence, per the mission's own protocol ("si un changement de simulation
paraît nécessaire : STOP, documenter, ne pas la modifier"):

- **Roads/paths: NOT REACHABLE.** No tile type, no path graph, no connectivity
  data exists in `AnastasisSim`. Rendering a road would mean inventing a
  network with no simulation source — refused. Documented here as a blocked
  dependency, not implemented.
- **Named building categories (House/Market/Workshop, per the mission's own
  example): NOT REACHABLE.** Same reason — no settlement/building entity
  exists yet. `ETileType::Ruin` is used instead, honestly, as the one
  structure-adjacent category simulation truth actually provides. This is
  presented as a stand-in for "the building leg of the pipeline exists and is
  driven by real semantic data," not as "houses are now rendered."

Unblocking either requires a `AnastasisSim`-side decision (a settlement/road
data model) that is explicitly out of this mission's mandate. Next-highest-leverage
step for whoever owns that mandate: see `WORLD_SLICE_006_SEAL.md`-style seal at
the end of this mission's work, section NEXT_HIGHEST_LEVERAGE_VISUAL_STEP.

## Architecture decision (Phase 2)

Boundary added, purely additive:

```
WorldSemanticState (AnastasisWorld::ETileType)
        -> AnastasisPresentationResolver::Resolve(Type)      [new]
        -> FRenderableDefinition (ArchetypeId, MeshPath, Tint, scale/jitter range)
        -> AnastasisPresentationResolver::ResolveInstanceTransform(...)  [deterministic, seed+tile keyed]
        -> AAnastasisWorldEmbodiment: one HISM per archetype, AddInstance(...)
```

- `ETileType` never names a mesh. `AnastasisPresentationResolver.h/.cpp` is the
  one seam an asset-integration pass edits (`MeshPath`/`Tint` per archetype)
  without touching worldgen, WorldView, or the deterministic placement math.
- Presence of an instance is decided **entirely** by `Resolve(Type)` — i.e. by
  simulation truth. The transform resolver only jitters position/yaw/scale
  from a hash of `(Seed, TileX, TileY)`; it never decides whether a tile gets
  an instance. This keeps the CAUSAL property in Phase 7 clean: changing a
  tile's type is the only way to change whether/what renders there.
- Reachable in this slice: `Forest -> Tree_Generic`, `Ruin -> Ruin_Generic`.
  Every other type resolves to `nullptr` (ground color only, unchanged).
- Deliberately NOT done: consolidating the two existing, independent, sealed
  ground-color tables (`AnastasisWorldDebugVisual::TerrainDebugColor`,
  `AnastasisTerrainSurface::TileColor`) into this resolver. Both are tested,
  sealed, and out of scope for a narrow additive slice; touching them risks
  the sealed `TERRAIN_CONTRACT` for a stylistic win with no functional payoff.
  Flagged as a candidate for a future, separately-mandated consolidation pass.
- Deliberately NOT a `UDataAsset`/data-driven registry yet: two archetypes,
  engine placeholder meshes only, no Content assets to swap today. The
  resolver's shape (one lookup function, one struct) is the seam a future
  `UPrimaryDataAsset` can sit behind without changing any caller — this is the
  next-highest-leverage step for the assets-integration agent.

## Implementation (Phase 3)

| File | Role |
|---|---|
| `Source/Anastasis_UnrealV2/WorldView/AnastasisPresentationResolver.h/.cpp` | New. The resolver: `Resolve(ETileType)`, `AllArchetypes()`, `ResolveInstanceTransform(...)`. Pure functions, no Actor/UObject dependency. |
| `Source/Anastasis_UnrealV2/WorldView/AnastasisPresentationResolverTests.cpp` | New. `Anastasis.Presentation.Reachability` (locks which types instance vs. ground-only) and `Anastasis.Presentation.Determinism` (same seed+tile+alt reproduces the exact transform; jitter stays bounded; base — not center — sits at Alt). |
| `Source/Anastasis_UnrealV2/WorldView/AnastasisWorldEmbodiment.h/.cpp` | Extended (not rewritten). Two new HISM components (`DressingMeshes[3]`, indexed by `EArchetype`), built alongside the existing seven ground HISMs. `EmbodyCrop` places one instance per Forest/Ruin tile after the existing ground pass, orthogonal to the DEBUG/Surface toggle. New `GetDressingInstanceCount()`. |
| `Source/Anastasis_UnrealV2/WorldView/AnastasisWorldProbeSubsystem.cpp` | One line added: `dressing_instance_count` in the existing terrain JSON block, for the same `Anastasis.World.Snapshot`/`Capture` tooling used to validate everything else in this project. |

No file under `Source/AnastasisSim/` was opened for writing. No `.uasset`/`.umap`
was created or modified — both new archetypes use `/Engine/BasicShapes/`
primitives (Cone for Tree, Cylinder for Ruin), the same fallback convention
`AAnastasisWorldEmbodiment`'s constructor already uses for its Cube/
BasicShapeMaterial.

## Performance (Phase 6)

Canonical crop is 32x32 = 1024 tiles (`TileWorldSize`=100uu ≈ 1m/tile, so a
32m x 32m patch). Reference full-world counts at seed 12345 (96x96, from
`AnastasisWorldViewTests.cpp`): `Forest=716`, `Ruin=462` out of 9216 tiles — the
32x32 crop's actual counts are logged at runtime
(`ANASTASIS_PRESENTATION dressing_instances=... tree_tiles=... ruin_tiles=...`)
and are necessarily ≤ those. Either way this is two to three orders of
magnitude below where per-tile `AActor`s would become a concern, and instancing
(`UHierarchicalInstancedStaticMeshComponent`, the pattern already established
for all seven ground types) was used from the start rather than one `AActor`
per tile — so this scales cleanly if the crop grows toward the full 96x96
world without a structural change.
