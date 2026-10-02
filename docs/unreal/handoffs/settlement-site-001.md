# HANDOFF: settlement-site-001

## MISSION

Replace the opening village's centre-first free-tile search with reproducible geographic selection. Direct user mandate: fix placement and launch the next step. Separate from the visual crusade assembly.

## FILES_OWNED

- Source/Anastasis_UnrealV2/WorldView/AnastasisSettlementSite.h
- Source/Anastasis_UnrealV2/WorldView/AnastasisSettlementSite.cpp
- Source/Anastasis_UnrealV2/WorldView/AnastasisSettlementSiteTests.cpp
- Source/Anastasis_UnrealV2/WorldView/AnastasisSettlementSurvey.h
- Source/Anastasis_UnrealV2/WorldView/AnastasisSettlementSurvey.cpp
- Source/Anastasis_UnrealV2/Sim/AnastasisSimulationSubsystem.h
- Source/Anastasis_UnrealV2/Sim/AnastasisSimulationSubsystem.cpp
- tools/unreal/settlement-site-pie.py
- tools/unreal/proofs.txt (settlement-site-pie entry)
- AGENTS.md (one index entry)
- docs/unreal/handoffs/settlement-site-001.md

## COMMIT

HEAD of agent/settlement-site-001. Base: 506f6db49aea640d4f0a8f96373b2e84d8853590. Functional commits: 93d2bfc and c790e8a. Tested code: c790e8a34d46e0cd6a1b5880cfa1155df2ca27b3. Later commits only update this handoff.

Do NOT include implicitly in the visual assembly: changed village placement invalidates its comparison conditions.

## MEC

- BUILD: PASS (182.76 s initial, 45.17 s proof API, 175.06 s placement-margin correction).
- TARGETED TESTS: PASS. GeographicChoice, BarriersAndMissingEvidence, ResourcesDriveChoice each have a current-run Success record.
- Full suite: QUEUED for integration; not claimed executed here.
- Command: tools/unreal/editor-batch.ps1 -Proofs settlement-site-pie
- Local run: Saved/EditorBatch/20261002-022202/editor-batch.log
- PYTHON_AST: PASS.
- finish on 201b083 returned BUILD::PASS::CACHED and HANDOFF_READY::YES (queued); rerun after this documentation repair.

## PROOFS

PROOFS: settlement-site-pie

## CHANGE

The first ready tick surveys the actual ExperimentalTerrain triangles in its own world: section 0 ground, 1 lake, 2 river ribbons. It verifies seed, dimensions and identity transform; no global Forge cache. Nine samples per tile estimate conservative slope and rendered-water freeboard. Existing IsFootBlocked, resource types and quantities remain authoritative.

Candidate policy: grass/scrub, wetness below 0.6, slope <=8 degrees, rendered-water freeboard >=1 m, two-cell border margin matching SeedFirstWell, at least three cardinal exits, at least nine connected expansion cells <=12 degrees. Reachable semantic AND rendered water <=300 m, productive field <=600 m, existing wood <=600 m. Graph edges require dry walkable ground and <=18 degrees; distances are cardinal graph lengths, NOT exact NPC A* costs.

Score weights: expansion area 40, water 25, food 20, wood 15. Ties use tile index; no new RNG. These thresholds are explicit startup policy, not historical or agronomic facts. Freeboard is not a flood forecast.

The host uses existing SetSettlement and SeedFirstWell. Existing twelve-NPC ring and needs are preserved; NPC spawning/navigation are not extended to use rendered slope. Existing populated villages are never moved; explicit scenarios cancel pending automatic placement. Source/AnastasisSim is unchanged.

anastasis.Village.SiteSelection defaults to 1; 0 retains legacy opening for a NEW startup, without teleporting an existing village. No eligible candidate produces an unavailable report and no silent centre fallback. Missing terrain gets a bounded ten-second wait.

## SCN

OBSERVED on c790e8a. SETTLEMENT_SITE_PIE PASS in 52.3 s: actual well at selected coordinates, twelve NPCs, time progression and at least one NPC movement after ten simulated seconds, three screenshots obtained and viewed.

Survey: 69.428 ms, 8836 surveyed cells, 59 eligible sites. Selected (74,36): slope 6.077 degrees, connected area 9600 m2, water access 60 m, field 120 m, wood access adjacent (0 m to the harvesting-access cell). Legacy (47,47): ineligible, water graph distance 660 m. Legacy area zero means its base eligibility failed before the area computation; it does NOT mean measured absence of available land.

Saved/SettlementSiteEvidence/comparison.json contains the report. Images: selected_eye.png, selected_oblique.png, legacy_terrain_oblique.png. Viewed result: well grounded in an opening at a forest edge; nearby trees and grass, visible terrain at human height. NPC debug markers remain visible. Functional evidence, not final artistic validation. Legacy image shows the old TERRAIN without a fabricated old village; nearby visible river does not contradict graph distance to water matching both semantic and rendered layers.

SHUTDOWN FAILURE: proof completed at 06:23:40; crash during shutdown at 06:23:47, RequestExitWithStatus 3. The launcher reports EDITOR_BATCH::PASS based on job markers; this does NOT prove a clean process exit. PID 4892 was absent afterwards, shared editor slot released. General stability: UNKNOWN. Evidence files are local generated outputs, not committed.

## PLY

UNKNOWN. Initial site selection does not close the general modified-terrain -> NPC/player navigation loop. Presence/movement/images do not prove all NPC journeys or player traversability.

## ECARTS

AUCUN - Source/AnastasisSim unchanged. Startup selection belongs to the Unreal host using existing APIs. No executed parity claim.

## INTEGRATION_RISK

Shared Sim/AnastasisSimulationSubsystem.cpp overlaps anthropic-landscape-001 reset hook: preserve its ResetPresentation call on integration. Main advanced after worktree creation. Merge registry/index entries selectively. No binary assets modified.

Strict policy can refuse maps without all required resources. Use the real report to diagnose; do not weaken tests merely for PASS. Source/terrain changes after integration require the declared proof again.

## STOP

No independent integration or push. Keep this mission separate from visual comparisons. No historical precision invented. Targeted runtime proof and captures obtained; combined integration tests remain queued. Shutdown crash is declared without an unrelated speculative patch.
