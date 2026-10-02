# HANDOFF: visual-crusade-001

## MISSION
Improve the existing tree silhouettes in Lvl_AnastasisSlice through its existing render assets. Base 506f6db49aea640d4f0a8f96373b2e84d8853590. Keep topology, simulation, transforms, materials and collision configuration.

## FILES_OWNED
- tools/unreal/create_tree_asset.py: geometry, in-place mesh update, import guard only.
- Content/Anastasis/Vegetation/SM_Tree_*.uasset: 29 existing meshes (8 family and 21 species assets); SM_Tree_Generic excluded.
- tools/unreal/ground-cover-capture.py: optional fixed cameras and view selection.
- tools/unreal/riverbank-capture.py: avoid same-map reload, optional view selection.
- tools/visual-crusade/: generation and capture entrypoints.
- docs/unreal/handoffs/visual-crusade-001.md.

## COMMIT
Use the commit containing this handoff; exact owned paths only. No push requested.

## MEC
- BUILD: PASS before geometry production, Development Editor UE5.8.2 CL56702186. No C++ changed.
- AST: PASS all six Python files; git diff --check PASS.
- Generation v2: 29 CRUSADE_TREE_SAVED records, CRUSADE_GENERATION_COMPLETE at 2026-10-02 05:55:45 UTC.
- Collision checks before/after: all 29 retain 0 simple shapes and CTF_USE_DEFAULT. BodySetup is preserved by updating mesh LODs in place. Two material slots retained. Normalized vertical extent remains [-50,50].
- Three LODs retained; Nanite not enabled. No per-instance Tick, new collision, placement/density or simulator change.
- TESTS: queued for the combined integration; no general stability claim.

## PROOFS
PROOFS: (aucune)

## SCN
PARTIAL, OBSERVED IN UNREAL, saved in the mission worktree; not yet integrated in main.
- Before: Saved/VisualCrusade/before/ground/{prairie_eye,lisiere_eye,sousbois_eye,oblique}_on.png.
- After v2: Saved/VisualCrusade/after-v2/ground/{prairie_eye,riviere_eye,lisiere_eye,sousbois_eye,oblique}_on.png. Five images completed at 05:57:03 UTC, then Python shutdown crash at 05:57:25. Images complete is not a clean-exit PASS.
- 1600x900 HighResShot, baseline camera coordinates reloaded exactly; no lighting/exposure/material setting changed in this lot. Requested editor window1280x720; actual viewport resolution not independently measured.
- Human-height forest: opaque internal crown spheres removed; finer pointed leaves and connected branchlets, roots less star-shaped. V1 was rejected for defoliated crowns; v2 restores authored outer leaf coverage.
- Prairie, edge and aerial gain modest. Distant crowns still opaque stylized proxies; bark remains dark and striped; some crowns still sparse. Not AAA, not a realistic-library replacement.
- compare.py pixels above16/255: prairie7.22%, edge4.26%, forest24.53%, aerial3.94%. Differences are not quality scores; moving foliage contributes variance, no same-run control for this geometry substitution.
- Shared river-before: natural-history-001/Saved/NaturalHistoryEvidence-v1-rejected/riviere_eye_reference.png; both camera JSONs checked identical coordinates. Keep attribution; do not compare unlike weather states.
- Performance: v2 overlapped an external integration editor and is NOT valid for attributed CPU/GPU comparison. Raw metrics remain in ground-cover.json. No performance success claim.
- No loaded realistic replacement library found locally. No purchase/download performed. Existing recipe-generated assets improved instead.

## PLY
UNKNOWN. Earlier batch crashed at PIE duplication before PNJ activity was proved. Combined integration still requires a fresh targeted PIE observation with PNJs, simulation and camera movement. Static editor images are not player proof.

## ECARTS
AUCUN: Source/AnastasisSim untouched. No simulation behavior introduced.

## INTEGRATION_RISK
- atmosphere-crusade-001 also edits create_tree_asset.py material functions and identical import guard. Preserve both disjoint regions; never regenerate its materials from our old worktree.
- natural-history-001 also edits ground-cover-capture.py state/view controls. Preserve fixed-camera support alongside those changes.
- Bounds change may influence derived ecology/camera selection. Fixed before cameras prevent misleading recadrage; runtime placement/collision still needs combined check.
- Canonical .claude/settings.local.json and download.png are unrelated and excluded. External Claude integration anthropic/reconsider/rain holds MAIN.lock; do not overwrite or bypass.

## ROLLBACK
On a dedicated rollback worktree based on the final integrated tree, revert only the integration commit for visual-crusade-001, preserving any later material edits in create_tree_asset.py and the other capture states. The parent commit contains all 29 original LFS meshes. Finish then use the normal integration gate. Never reset main or restore an entire shared source file from our base.

## STOP
No new assets acquired, no landscape or water migration, no character edits, no navigation changes, no claim that a screenshot proves simulation or general stability. Final assembled visual judgement and PIE remain required.