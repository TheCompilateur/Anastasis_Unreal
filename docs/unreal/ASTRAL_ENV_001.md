# ASTRAL ENV-001 — discovery / mission log
GATE 0 completed before feature edits, 2026-09-13 (Toronto).
Base: e96175f95afb753fa746ecce05f3d927baf7895a main.
Canonical dirty: Config/DefaultEditor.ini; untracked .claude/. Preserved.
Owned branch/worktree: agent/astral-env-001; C:/dev/ANASTASIS_WORKTREES/astral-env-001.

Pipeline: AnastasisSim GenerateWorld(seed,96,96) -> WorldView snapshot/crop ->
WorldEmbodiment -> TerrainSurface ProceduralMesh (ground+sea plane) +
PresentationRegistry/Resolver -> HISM per archetype/variant.
No PCG, Landscape, Foliage or material parameter collection consumer found.
World signals: Type, Alt, Shore, Wetness, Flow, Fertility, Resource/Amount.
ForestMargin is invalidated by ApplyForest cap; do not consume it.
Slope can be derived from the existing triangle surface. No human influence data.

Asset map (canonical paths, no external copies inspected):
- Tree: Content/Anastasis/Vegetation/SM_Tree_Generic_01.uasset; live load passed,
  centered bounds z +/-50, x +/-34, one LOD; primitive trunk+cone silhouette;
  HISM-capable, usable for three size strata, not botanical diversity.
- Forest/Ruin look: Content/Anastasis/Presentation/DA_AnastasisPresentation.uasset;
  live load passed; references remain owned by the registry.
- Ground/bank/water: Content/Anastasis/Materials/M_AnastasisSlice.uasset;
  used by terrain; vertex color, shore transition, flat water.
- Ruin: registry + engine cylinder stand-in; unchanged.
- Shrubs, ferns, grasses, deadwood, wetland meshes, ecological rocks: none found.
- LevelPrototyping basic meshes exist; unsuitable ecological art, not repurposed.
- docs/visual/reference: reference images, not meshes or runtime proof.

Concurrent worktrees observed:
asset-agent-002: edits registry and ruin variant script, separate ruin mesh commit.
atmosphere-mist-002: atmosphere/GameMode/probe files and new MistField files.
atmosphere-light-001: clean when observed.
multi-agent-control-001: older fork modifies WorldEmbodiment and TerrainSurface;
textual and semantic intersection. No automatic integration permitted.
One active Unreal commandlet was identified as atmosphere-mist-002; preserved.

Architecture: pure deterministic forest plan on FULL canonical snapshot, emitted
only inside actually rendered surface. Three size layers reuse the Forest registry.
Local forest support + coherent presentation cluster field -> density/age gradient.
Actual wetness, triangle slope, sea clearance and habitat exclusions condition it.
Settings grouped on WorldEmbodiment, CVar A/B switch, HISM reused. No simulation edit.
No new mesh/material/Blueprint/PCG. Missing botanical layers stay explicitly absent.

KEEP only after build/tests and neutral fixed-camera A/B inspection. Aesthetic
acceptance remains PARTIAL if geometry or ground coloring still exposes tile classes.

Canonical HEAD changed during build to 7ddae7b00d7e26f7999e37a5d5ffbe9378f6b4a4.
Mutation paused; inspected e96175f..7ddae7b: ruin mesh + six asset-related paths only,
no intersection with ASTRAL source or consumed world data. Resumed on pinned e96175f,
without merge/rebase/cherry-pick. Canonical DefaultEditor.ini edit had disappeared
when rechecked; ASTRAL did not edit or restore it.

Second canonical movement: 7ddae7b -> 453137321b57380486d660abc1140705839fc86c.
Inspected before resuming: data-asset ruin binding and editor construction path
(default terrain mode 2, transient HISM, OnConstruction, GameMode/config/tests).
WorldEmbodiment.h/.cpp overlap textually; runtime reconstruction and component
lifetime overlap semantically. ASTRAL stays on e96175f; no integration performed.
Any integration must preserve new transient component handling, constructor-owned
terrain component and OnConstruction, then rerun reconstruction, grounding and A/B.

Initial compile exposed C4458 (local Layers shadowed AActor::Layers), fixed to
ForestLayerCounts. A subsequent compile succeeded, but the operator correctly
rejected its source fingerprint because the legacy grounding test had been scoped
during the build-lock wait. That run is NOT BUILD::PASS; stable rerun requested.

Pinned-base evidence (before updating from canonical):
BUILD::PASS on frozen source; 49 PASS, 4 KNOWN_EXPECTED_FAILURE, 0 FAIL / 53.
Ecology: seed12345 438 trees (180 young/129 secondary/129 canopy).
Rejected: 28 water/footprint, 87 slope, 194 spacing.
Synthetic edge: fringe40 vs equal-width interior181; dry2242 vs wet1289;
submerged0, steep0. Three Ecology tests passed.
Neutral editor A/B: 1162 -> 886 dressing instances, 2 HISM, 6 scene actors.
Repeated numerical transform hash identical; PIE launched but 2 embodiments
(the older GameMode duplicated the placed actor). Do not call this SCN PASS.
Visual: more openings and graded sizes; still cone silhouettes, hard terrain colors.
Live registry audit confirmed the pinned base still bound Cone/Cylinder; the
dedicated SM_Tree_Generic_01 was loadable but not selected by that data asset.

Canonical now at 2cf1328c5de4b5b0fdd880b3bf26f279753582a4, including both actual
asset bindings and the single-embodiment fix. Decision: commit ASTRAL, then update
ONLY this worktree from that exact canonical commit after collision inspection.
Textual overlap: WorldEmbodiment header and constructor/placement neighborhood.
Semantic requirements: retain canonical OnConstruction, transient components,
constructor-owned terrain, ShouldSpawnEmbodiment, registry bindings; retain
ASTRAL full-source plan and one-time-per-component material setup. No source
simulation changes in either side. New evidence required after combination.
