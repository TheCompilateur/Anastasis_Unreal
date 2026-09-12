# P1.6 — Project restructure report

## P1_6_STATUS::

`LOT_0_AUDIT_COMPLETE / SAFE_DOCUMENTATION_ONLY / ARCHITECTURE_GATE_OPEN`

## CURRENT_VISUAL_ARCHITECTURE::

`AnastasisSim::GenerateWorld` is consumed by `WorldView::BuildPlan`; `AAnastasisWorldEmbodiment`
spawns seven HISM components using the Engine cube, one component per tile type.
`AAnastasis_UnrealV2GameMode::BeginPlay` spawns this actor unconditionally. The
current runtime is therefore a debug/metrology embodiment, not a player visual
world. P1.5 machine evidence confirms seed 12345, 96x96, 9216 tiles and 9216
instances; viewport pixels remain unproven because that evidence used `-nullrhi`.

## DEBUG_VIEW_OWNER::

`Source/Anastasis_UnrealV2/WorldView/AnastasisWorldView.*` plus
`AAnastasisWorldEmbodiment`. Keep as the diagnostic owner. Do not polish it into
the AAA renderer.

## SIMULATION_TO_VISUAL_BOUNDARY::

Current boundary is `FWorld -> FPlan -> HISM transforms`; it preserves the
simulation adapter direction but carries too little semantic structure for
player rendering. Target boundary is `FWorld/FTile -> FVisualWorldData ->
derived fields -> DEBUG or PLAYER consumer`, read-only and versioned.

## CURRENT_COUPLING_PROBLEMS::

- GameMode owns debug spawning instead of selecting a visual mode.
- `WorldView` is both the only visible world and the only adapter.
- Cube geometry, per-type debug colors and tile-centered coordinates define the
  visible surface.
- No player visual module, visual slice map, terrain backend or material system
  exists.
- Config contains high-end renderer toggles but no runtime/player proof.
- Project content is predominantly FirstPerson/Horror/Shooter template content.
- No Landscape, PCG, Water, Foliage, World Partition or HLOD evidence exists.

## FILES/FOLDERS_TO_KEEP::

`Source/AnastasisSim/**`; `WorldView/**`; parity tests and P0/P1/P1.5 reports;
existing gameplay modules; template assets needed for reachability. Keep
`__ExternalActors__` and `__ExternalObjects__` in place until Unreal reference
inspection is available.

## FILES/FOLDERS_TO_REFACTOR::

`Anastasis_UnrealV2GameMode.*` for a future visual-mode bootstrap;
`WorldView` comments/contracts to explicitly mark DEBUG; module build rules when
the first `WorldVisual` code exists. No refactor was applied in LOT 0.

## FILES/FOLDERS_TO_MOVE::

None. Binary assets were not moved. A future Content reorganization must use
Unreal-aware asset operations after reference checks.

## FILES/FOLDERS_TO_DEPRECATE::

None deleted. `Content/LevelPrototyping` and the template variants are
classified as non-canon/debug or out-of-scope, not removed.

## TARGET_SOURCE_ARCHITECTURE::

`AnastasisSim` remains canonical. Add `WorldVisual/Core`, `Terrain`, `Biomes`,
`Materials`, `Vegetation`, `Water`, `Atmosphere`, `Settlements` and
`Diagnostics` only when an owned implementation earns them. `WorldView` remains
the separate debug consumer.

## TARGET_CONTENT_ARCHITECTURE::

Prepare, but do not mass-move: `/Game/Anastasis/Core`, `/World/Terrain`,
`Materials`, `Vegetation`, `Water`, `Atmosphere`, `/Dev/Debug`,
`/Dev/VisualSlices`, `/Tests`, and future `/Architecture/Pontic` and
`/Architecture/Byzantine`.

## TERRAIN_OPTIONS::

`A::Landscape` — strongest editor tooling; weakest first fit for deterministic
runtime slice generation and a risk of authoring outside sim truth.

`B::ProceduralMesh` — strongest direct deterministic translation; custom
collision, streaming and tooling cost.

`C::Hybrid` — preserve a backend-neutral semantic layer; first proof uses B,
future production may add Landscape-derived presentation.

## RECOMMENDED_TERRAIN_ARCHITECTURE::

`C / B-first bounded slice`. Owner ruling remains required before implementation.

## MATERIAL_ARCHITECTURE::

Semantic-weighted layered master material: soil, grass, wet grass, mud, rock,
moss, litter, gravel, shore and cultivated earth. Macro/micro variation and
roughness/normal variation are required; runtime virtual texture and parallax
are deferred until measured.

## VEGETATION_ARCHITECTURE::

Deterministic visual ecology derived from semantic fields. PCG/Foliage/HISM/
Nanite are backend candidates, not yet selected. Prove clusters, edges and
clearings before density scaling.

## WATER_ARCHITECTURE::

Start with a custom derived shoreline/bank/wetness proof. Evaluate Unreal Water
only after runtime-generation, collision and streaming constraints are measured.

## LIGHTING_ARCHITECTURE::

Retain the current DX12/SM6, Lumen-class GI, VSM and Substrate intent as a
baseline. Add SkyAtmosphere, directional light, skylight, fog and post-process
only in the visual slice; current config is not proof.

## NANITE_POLICY::

Opt-in by measured asset class. No Nanite-everything rule; profile triangles,
draws, material cost, shadow cost and memory.

## PCG_POLICY::

PCG may generate presentation-only ecology from deterministic semantic inputs.
It must not become simulation authority or hide non-deterministic placement.

## WORLD_PARTITION_POLICY::

Deferred. Do not convert the 96x96 world or create a World Partition map until
slice scale, streaming boundaries and runtime generation are proven.

## PONTIC_VISUAL_PILLARS::

Humid valleys; forested ridges; mist and atmospheric depth; rocky exposures;
stream/shore transitions; wet soils; meadow clearings; worked terraces; clustered
temperate vegetation.

## BYZANTINE_VISUAL_PILLARS::

Post-1204 Rhomaioi material culture; timber/stone/lime/plaster/earth; regional
roof families; social repair and defensive pragmatism; no fantasy Byzantium.

## VISUAL_SLICE_REGION::

Canonical seed `12345`; derive a bounded 16x16 or 32x32 region from the existing
96x96 generated world. Exact coordinates should be selected after a read-only
semantic scan for a water edge, forest, clearing, slope and field.

## VISUAL_SLICE_CONTENT::

Continuous terrain; at least three material transitions; one water/shore edge if
available; clustered forest edge; clearing; atmospheric baseline; debug/player
side-by-side evidence. No buildings, NPCs, navigation or full-world conversion.

## PERFORMANCE_RISKS::

Runtime mesh generation and collision; over-dense vegetation; shader/material
permutation growth; shadow cost; Lumen/VSM cost; streaming and memory; PCG CPU
spikes; hidden duplicate debug/player worlds.

## ASSET_DEPENDENCIES::

No production art dependency is currently present. Future work needs sourced
Pontic vegetation, terrain scans/materials, water/shore assets and culturally
bounded architecture; provenance and licensing must precede import.

## PLUGIN_DEPENDENCIES::

Current `.uproject` enables ModelingToolsEditorMode, StateTree, GameplayStateTree,
PythonScriptPlugin, EditorScriptingUtilities, ModelContextProtocol, EditorToolset,
StateTreeToolset and AutomationTestToolset. PCG, Water, Foliage and World
Partition are not evidenced as enabled project plugins.

## PROJECT_SETTINGS_CHANGES_PROPOSED::

Later: explicit visual mode selection, visual slice map, runtime capture profile,
and verified Lumen/VSM/SkyAtmosphere settings. No settings were changed in LOT 0.

## SAFE_CHANGES_EXECUTED::

Created the four P1.6 operational documents under `docs/visual/`. No C++,
simulation, config, map or binary asset was modified.

## IRREVERSIBLE_CHANGES_DEFERRED::

Landscape/procedural/hybrid backend implementation; World Partition; PCG vs
Foliage/Nanite selection; Water plugin/custom water; asset moves/imports;
GameMode visual-mode bootstrap; material and lighting assets.

## BLOCKERS::

Owner ruling is required on the terrain backend and future streaming/editor
expectation. Runtime plugin availability and a player-frame capture are not yet
verified.

## UNKNOWN::

Actual UE Editor plugin availability, runtime GPU feature support, map references,
asset referencers, collision/navigation expectations, visual slice coordinates,
and measured generation/GPU/memory budgets.

## NEXT_LOT::

LOT 1 should implement only the semantic visual snapshot and a read-only scan
tool/test, after owner ruling. It should not create the final terrain backend.

## OWNER_RULING_REQUIRED::

1. Accept `C / B-first` as the terrain direction, or choose A/B explicitly.
2. Decide whether World Partition is a near-term requirement.
3. Decide whether PCG and Water plugins may be enabled for experiments.
4. Approve a fixed visual-slice coordinate selection after semantic scan.

