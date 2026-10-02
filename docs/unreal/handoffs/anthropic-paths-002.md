# HANDOFF: anthropic-paths-002

## MISSION

Make actual sampled NPC tread legible around the existing village, with human-height off/on/off evidence. User explicitly approved resuming the unfinished cultural-landscape mandate. This is its traffic slice, not the entire original mission.

## FILES_OWNED

- Source/Anastasis_UnrealV2/WorldView/AnastasisAnthropicMemory.cpp
- Source/Anastasis_UnrealV2/WorldView/AnastasisAnthropicSubsystem.cpp
- Source/Anastasis_UnrealV2/WorldView/AnastasisAnthropicSubsystem.h
- Source/Anastasis_UnrealV2/WorldView/AnastasisAnthropicTests.cpp
- tools/unreal/anthropic-paths-capture.py
- tools/unreal/proofs.txt (one registration)
- AGENTS.md (one index entry)
- docs/unreal/handoffs/anthropic-paths-002.md

## COMMIT

HEAD of agent/anthropic-paths-002; base 4fef72d526ee0be45a923c46c9e626dc3a6e7deb. Canonical .claude/settings.local.json and download.png excluded.

## CHANGE

Existing sampled-distance memory is retained. Neighbouring observed-cell kernels now blend additively with a unit cap, reducing grid-boundary gaps; no route is extrapolated across missing samples. New anastasis.Anthropic.Draw toggles only visual response, preserving the recorded history. Memory 0 still restores and clears everything. Frozen time no longer causes repeated transform restoration/reapplication once per wall second. Observation requires the embodiment seed and dimensions to match the simulation. Read-only report exposes the strongest affected grass location for evidence cameras.

No shared material, soil mesh, tree asset, simulation, navigation or NPC behaviour changes. Only existing GroundCover transforms respond. Affected grass restores through the existing transform reread check. Memory remains transient and default OFF pending visual judgement. Existing eight-simulated-day half-life is an artistic recovery envelope, not historical calibration and not saved generational history.

## MEC

BUILD: PASS, 294.72 seconds.
TESTS: four targeted tests PASS in 20261002-095802 on abf93dd; C++ unchanged since. Full suite remains queued. Coverage: observation/gaps/jumps, repetition/recovery/capacity, distance conservation and ObservedContinuity.
PYTHON_AST: PASS.

## PROOFS

PROOFS: anthropic-paths-capture, anthropic-routine-capture

## SCN

Opening observation executed; artistic activation REJECTED (results below). Routine follow-up remains queued. The registered script runs four tests, observes the ordinary twelve opening NPCs for 110 wall seconds at TimeScale0.15, requires actual affected grass and no capacity loss, then freezes simulation. Six screenshots: human-height and oblique, Draw0/1/0. Focus is selected from actual affected grass; no synthetic path or injected observations. Memory cell count must remain unchanged across visibility toggles, and original grass transforms must restore. Same camera, clock and scene; reference repeat estimates render/wind noise. Captures are functional until reviewed and measured against the repeated reference.

GPU/game/render p50 from the existing GetFrameTimingsMs API during each frozen capture; not a live simulation CPU benchmark. apply_ms separately measures the periodic grass update. No GPU claim with concurrent editors.

KEEP requires a visible local tread beyond reference noise, reversible original transforms, no invented traffic and bounded cost. A merely successful capture is not an artistic PASS. Command: tools/unreal/editor-batch.ps1 -Proofs anthropic-paths-capture.

## PLY

UNKNOWN. No player traversal or changed NPC route choice is claimed.

## ECARTS

AUCUN - Source/AnastasisSim unchanged; no new simulator mechanism or parity claim.

## INTEGRATION_RISK

Shared AGENTS.md and proofs registry must preserve other entries. Anthropic layer should be the sole writer of its owned grass transforms; terrain rebuilds may invalidate instance identities, handled conservatively by existing restore guards. Other agents own soil/material/trees; none edited here.

## STOP

No independent integration or push. No farming, grazing, logging or historical reconstruction implemented in this slice. Full cultural-landscape feedback loop remains incomplete. Do not enable the experimental layer globally on a technical test alone. Follow the single-editor queue; never close another session. Prior shutdown crash is known but not presumed fixed.

## OPENING OBSERVATION - 20261002-095802

Four targeted tests Success; geographic village and frozen-memory restoration checks PASS. Six images viewed. 1559 cells, 29 affected grass instances, maximum reported grass response 0.206, no gap/jump/drop or restore errors. Technical proof 208.8 s; subsequent shutdown crash Exit3. No global artistic activation accepted: no legible path in the opening scene.

compare.py, 1166x856: eye off/on 1.47% pixels above16, off/off2 1.28%; oblique1.34% vs reference1.19%. Marginal difference does not establish a legible path. Default remains OFF. A new external water-look NullRHI commandlet was seen at process-release check; frozen-frame GPU numbers are not accepted as a controlled performance comparison until overlap is excluded.

One bounded follow-up is prepared using EXISTING FirstFarmer1, not injected traffic: 260wall seconds at TimeScale0.3, actual delivery counter and food-conservation samples recorded. Requires at least three completed deliveries; no forced routes, resource refill or altered behaviour. Separate farmer output folder preserves opening evidence. Registered anthropic-routine-capture. Queue rank after soil-contact-003 then woodland-sequence-003; no launch until their actual release. This is a scenario proof, not normal-village traffic evidence.
