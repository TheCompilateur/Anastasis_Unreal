# P1.6 — Visual architecture

Status: LOT 0 audit / decision gate. No AAA renderer has been implemented.
Project: `Anastasis_UnrealV2`, UE 5.8.2.

## Verdict

The current `WorldView` is a valid metrology adapter, not a player renderer. It
must remain available as `VISUAL_MODE::DEBUG`. A second consumer must be added
under `WorldVisual`, reading a read-only semantic snapshot derived from
`AnastasisSim::FWorld`.

The first recommended target is a **hybrid terrain architecture**: a
deterministic runtime-generated procedural mesh for the bounded visual slice,
with a later migration path to Landscape only if editor-authored terrain and
World Partition become requirements. This is a recommendation for the next
proof, not an irreversible implementation decision.

## Current ownership map

| Component | Current evidence | Classification |
|---|---|---|
| `Source/AnastasisSim` | canonical `GenerateWorld`, hydrology, parity tests | KEEP / SEALED |
| `WorldView/AnastasisWorldView.*` | coordinate adapter and debug plan | KEEP / REFACTOR boundary comments only |
| `WorldView/AnastasisWorldEmbodiment.*` | 7 HISM cube diagnostic view | DEBUG_ONLY |
| `Anastasis_UnrealV2GameMode` | spawns the debug embodiment unconditionally | REFACTOR later to visual-mode bootstrap |
| `Variant_Horror`, `Variant_Shooter` | template gameplay variants | KEEP outside visual mission; no visual canon |
| `Content/LevelPrototyping` | cubes, grid, flat prototype materials | DEBUG_ONLY / DEFER migration |
| `Content/FirstPerson`, `Characters`, `Weapons` | template/player dependencies | KEEP for reachability; not world canon |
| `Content/Variant_*` | template maps and UI | KEEP outside P1.6 scope |
| `Config/DefaultEngine.ini` | DX12/SM6, Lumen-class GI, VSM, ray tracing, Substrate | KEEP baseline; verify at runtime |
| `docs/migration/phase1_5` | sealed machine evidence | KEEP as historical evidence |
| `Content/__ExternalActors__`, `__ExternalObjects__` | level partitioned/template assets | DO NOT MOVE blindly |

## Target source architecture

```text
Source/AnastasisSim/                         canonical simulation
Source/Anastasis_UnrealV2/WorldView/         debug/metrology consumer
Source/Anastasis_UnrealV2/WorldVisual/
  Core/                                      semantic snapshot + mode contract
  Terrain/                                   continuous surface and chunking
  Biomes/                                    derived ecological fields
  Materials/                                 layer weights, no sim writes
  Vegetation/                                deterministic visual ecology
  Water/                                     shoreline/bank/water presentation
  Atmosphere/                                sky/fog/light orchestration
  Settlements/                               future ownership boundary only
  Diagnostics/                               visual metrics and capture hooks
Source/Anastasis_UnrealV2/Gameplay/          existing legitimate gameplay only
```

Do not create empty classes for every folder. The first real foundation should
be `FVisualWorldData` plus a pure translator/derivation pass. It owns no
simulation state and cannot write to `FWorld`.

## Simulation-to-visual contract

```text
FWorld / FTile (read-only)
  -> FVisualWorldData (copy + explicit provenance)
  -> derived fields: height, slope, curvature, moisture, shore distance,
     wetness, rock exposure, soil family, vegetation density, farm influence
  -> DEBUG consumer OR PLAYER consumer
```

`Alt` remains simulation truth. Continuous height, slope, curvature and
microrelief are visual derivations. A visual actor must never call a mutating
simulation API. The translator must retain seed, dimensions, source index and
derivation version for reproducibility.

## Terrain options

### A — Unreal Landscape

Strengths: mature sculpting, landscape material tooling, foliage/PCG ecosystem,
navigation/editor workflows, streaming integration.

Costs: runtime creation and deterministic regeneration are awkward; converting
discrete elevations to a useful heightmap introduces resolution and seam
decisions; Landscape remains a poor first proof for a generated 16x16/32x32
slice; Nanite is not the reason to choose it.

Risk: the team starts authoring terrain outside simulation truth.

### B — runtime procedural mesh/chunks

Strengths: direct deterministic translation, continuous interpolation, explicit
chunk ownership, easy fixed-seed tests, no visible grid, natural fit for a
bounded slice.

Costs: custom collision, nav integration, editor tooling, material streaming
and large-world scaling must be designed; Nanite/runtime mesh support must be
verified on the chosen UE 5.8 path.

### C — hybrid

Use a runtime procedural base surface for semantic truth and visual proof;
permit later Landscape or authored proxy chunks only as a derived presentation
layer. Keep the source data and derivation stable so either backend can consume
the same `FVisualWorldData`.

## Recommendation and gate

Recommend **C, with B as the first implementation backend**. Prove a 16x16 or
32x32 canonical slice before any 96x96 conversion. The owner must still rule on
whether future production requires Landscape authoring/World Partition. No
Landscape asset, World Partition map, or runtime mesh class is created in LOT 0.

## Other architectural choices

- Materials: layered master material with semantic weights; no one-flat-color
  terrain type mapping.
- Vegetation: deterministic placement data first; PCG/Foliage/Nanite backend
  remains a proof choice. Avoid HISM as a semantic substitute for ecology.
- Water: custom derived shoreline/bank proof first; evaluate Water plugin only
  after runtime-generation constraints are measured.
- Lighting: Lumen/VSM/SkyAtmosphere baseline is plausible in config but has no
  player-frame proof; add a visual slice before tuning.
- Nanite: use only for measured static/high-density geometry. No Nanite policy
  is accepted from the current config alone.

## Gate evidence required before LOT 2

1. fixed seed `12345`, canonical `96x96`, bounded 16x16 or 32x32 extraction;
2. same semantic sample visible in DEBUG and PLAYER modes;
3. no mutation of `AnastasisSim` or parity vectors;
4. runtime capture, not `-nullrhi` only;
5. seam, collision, generation-time and memory measurements;
6. owner ruling on Landscape/World Partition and PCG/Water plugin scope.

