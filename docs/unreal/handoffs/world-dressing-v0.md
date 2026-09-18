# HANDOFF: world-dressing-v0

## MISSION

Create an isolated, deterministic, editor-only World Dressing V0 using existing meshes and transient HISM components. Default preview, no bake, no mutation of map generation, NPCs, navigation or buildings.

## FILES_OWNED

- Source/Anastasis_UnrealV2/WorldDressing/ (all files, entirely additive)
- docs/unreal/handoffs/world-dressing-v0.md

## COMMIT

The commit containing this handoff on agent/world-dressing-v0; base main 7047156.
No merge into the canonical checkout is part of this mission.

## MEC

- BUILD: PASS, UE 5.8.2 / CL 56702186, final Editor Development build.
- Targeted tests: 3 PASS, 0 KNOWN_EXPECTED_FAILURE, 0 FAIL.
- Anastasis.WorldDressing.DeterminismAndBounds
- Anastasis.WorldDressing.FiltersAndInvalidInput
- Anastasis.WorldDressing.RenderedSlopeAndAlignment
- Commands: tools/unreal/anastasis-unreal.ps1 build; tools/unreal/report-tests.ps1 -Filter Anastasis.WorldDressing.
- Live editor proof: 504 instances, 3 HISM, 0 spawned prop actors, 0 invalid roots in water/out of bounds; repeated C++ hash and independent HISM transform hash match. Clear removes all generated components/instances, existing source HISM inventory is unchanged.
- Counts: BroadleafUnderstory=351, ConiferCanopy=106, RuinDetail=47.
- Seed=12345; placement SHA1=FD164D6FE61A44CB4177E77E0A25EA96FF0BC32C.
- Independent HISM SHA256=30281581d1a42d87ab58c67384eaffe429819c65f3e2505febc30e22a393598f.

## SCN

PASS for visible additive before/after impact in a dedicated editor at fixed cameras; not final art acceptance.
Evidence: C:/Users/alex_/.codex/visualizations/2026/09/18/01a0b630-23b2-7983-9b5a-b90996d58ce8/world-dressing-v0/
Final-binary replay via delivered launcher: sibling world-dressing-final/.
Files: before_detail.png, after_detail.png, before_overview.png, after_overview.png, observation.json, capture.log.
No scene/profile/asset was saved. Existing tree and ruin meshes only. No generated artifact is committed.

## PLY

UNKNOWN / out of scope. Manager is editor-only and preview components have no collision/navigation effects.
Performance at maximum density is UNKNOWN. HISM grouping is verified, not a GPU performance verdict.

## INTEGRATION_RISK

- No shared implementation file or module build file changed. Folder compiles inside existing Anastasis_UnrealV2 module.
- Consumer reads existing GetSnapshot and ExperimentalTerrain section 0. If that component/section contract changes, update the isolated adapter (fail explicitly, never invent fallback heights).
- Snapshot has no building mask. Known VillageBuilding actors, tagged actors and explicit exclusion arrays cover represented buildings/roads; other renderers require supplied exclusion actors.
- Source must have visible surface geometry and translation-only actor transform.
- Validation covers roots and eight support-ring samples, not every mesh vertex or complete canopy spacing.
- Hash comparisons require the same profile/order, map snapshot, rendered mesh, mesh bounds, exclusions and source transform. Rebuild explicitly after edits.
- Canonical checkout was already dirty on terrain-forge-chunk-seam-halo, HEAD d598c55. Its unrelated files were not modified, staged or merged by this mission.

## STOP

No terrain/ecology rewrite, NPC, pathfinding, building generation, final asset creation, bake, automatic merge, PLAYER work or artistic release claim.
See Source/Anastasis_UnrealV2/WorldDressing/README.md for designer workflow and evidence reproduction.
