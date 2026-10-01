# HANDOFF: sleep-urgency-001

## MISSION

Test whether retained sleep urgency causes repeated rest after recovery in the
four-person village observed by map-intelligence-001. This is a one-scalar A/B,
not a new production policy. Base 9175a37. Reused managed worktree:
C:/Users/alex_/.codex/worktrees/map-intelligence-001/ANASTASIS_UNREAL,
branch agent/map-intelligence-001. No edits to canonical source or the JS reference.

## FILES_OWNED

- Source/AnastasisSim/Public/Village/AnastasisVillage.h
- Source/AnastasisSim/Private/Village/AnastasisVillage.cpp
- Source/AnastasisSim/Private/Tests/AnastasisSleepUrgencyTests.cpp
- Source/Anastasis_UnrealV2/Sim/AnastasisSimulationSubsystem.cpp
- docs/unreal/handoffs/sleep-urgency-001.md

## COMMIT

Pending experiment.

## PROTOCOL

Reference src/ai/algorithmic/runtime.js:185 reuses the entire previous decision
when inertia says keep. The C++ port does the same. This is inherited reference
behavior, not evidence of a C++ parity regression.

A = existing behavior. B = after the unchanged inertia vote, refresh only the
retained sleep decision's urgency from the newly scored sleep candidate. Score,
raw score, target, reason, creation time, duration and switching thresholds stay
unchanged. A reason such as fatigue_high can therefore remain historical in B;
this is deliberate to isolate the causal variable, not a production debug format.

CVar anastasis.Village.RefreshRetainedSleepUrgency defaults to 0. Set it to 1
BEFORE PIE/reset for B. FVillage resets the option to false on Bind; the Unreal
host applies the CVar after Reset. No editor tick polling or config persistence.

KEEP criterion, declared before measurements: B removes stale urgent sleep at
reconsideration, reduces npc-3's repeated high-energy rests, and conserves food.
REJECT if that intervention does not discriminate the observed behavior or breaks
food accounting. KEEP means retain this causal lead, not enable it in production.

Kernel fixture: generated seed 12345, 96x96, initial time 37.8, dt 1/60,
10800 ticks (180 seconds = two 90-second days). Two houses, one well, one granary,
96 initial portions, four inhabitants with energy 70/50/30/10. Placement and needs
match the prior PIE fixture. No active food sources, as in the original observation;
no claim about the separate new harvesting circuit. Check food conservation and
blocked-cell occupancy each tick. Third replica compares default vs explicit A
digests each tick. This is a fixed-tick mechanism test, not rendered/player proof.

Tests: Anastasis.Sim.Village.SleepUrgency.RetainedScalar and
Anastasis.Sim.Village.SleepUrgency.FourPeopleTwoDaysAB. Existing Village tests and
Parite.Nous are the regression scope. KEF are classified separately by the existing
registry and automation-log.ps1 reader. No registry change.

## MEC

Pending build and targeted automation. Evidence directory:
C:/Users/alex_/.codex/visualizations/2026/09/29/01a0eec3-f3bb-7ea3-b905-b86ada136008/
Source/config fingerprint: sleep-urgency-preflight.json.

## SCN

Pending. Prepared paired PIE probe sleep_urgency_pie.py uses two fresh PIE worlds,
the original three commands, speed 1, and 180 seconds per mode. Only the CVar differs.
No captures or asset saves; runtime counters cannot prove visual legibility.

## PLY

UNKNOWN. PLAYER remains NOT_IMPLEMENTED. No human playtest.

## INTEGRATION_RISK

Experimental default-off extension. Do not advertise JS parity for mode B.
The stale sleep score is intentionally untouched; no claim that all inertia problems
are solved. Existing home-first hunger relief without confirmed physical meals also
remains outside scope. Shared village/subsystem files are concurrent hotspots;
review exact patches against main before integration. Managed worktree operator
path guard requires direct engine Build.bat invocation; no HANDOFF_READY claim.
