# HANDOFF: tree-canopy-002

## MISSION
Tree silhouette experiment, second visual campaign. REJECT: smaller leaf sprays visibly depleted medium-distance crowns. No runtime change retained. This handoff also requests the existing villager PIE proof for closure of the assembled visual batch.

## FILES_OWNED
Only docs/unreal/handoffs/tree-canopy-002.md is delivered. The experimental generator and all 29 changed tree meshes were restored to this branch HEAD fe474f7. Canonical tree assets were never written by this experiment.

## MEC
UE5.8.2 CL56702186 Development Editor build passed (132.53s). Second run generated 29 trees in Entry before callbacks. Inventory checked 30 meshes: equal LOD0 triangles and collisions. CANOPY_ABB_COMPLETE views=4 stages=3. All 12 captures inspected. Process exited 3 after completion, not a clean shutdown. First attempt re-entered a Slate callback during mesh building and failed before candidate images; it provides no artistic evidence.

## PROOFS
PROOFS: villager-pie

## SCN
REJECT. At 1600x900, fixed prairie/river/oblique/forest cameras, same daytime lighting and materials, smaller leaf sprays reproducibly made the medium-distance forest look too bare. LOD decimation of disconnected leaves is a hypothesis, not a confirmed diagnosis. Existing geometry retained. No further iteration in this campaign.
Pixel differences >16/255, A/B versus B/B control (%): prairie 2.01/1.55; river 4.38/4.85; oblique 2.16/0.35; forest 18.92/3.97. These measure change, not quality.
GPU p50 before/candidate/control (ms): prairie 16.823/16.425/16.382; river 17.750/17.417/17.344; oblique 14.670/14.771/14.462; forest 20.687/21.096/20.537. Editor measurements only; no packaged performance claim.
Evidence: C:/dev/ANASTASIS_WORKTREES/tree-canopy-002/Saved/TreeCanopyEvidence/v2/{before,candidate,control}/. Candidate is the repeated control, not a restored baseline.
Experiment source archive: C:/Users/alex_/.codex/visualizations/2026/10/02/01a0fb0a-e751-7c03-8347-f506f516434a/tree-canopy-002-rejected/. Archived scripts depend on their original worktree location and patch; do not execute blindly.

## PLY
No simulation, topology, navigation, density, lighting, material or character change retained. Fresh villager-pie is queued for the combined soil/ecotone integration, not already passed by this experiment.

## ECARTS
AUCUN: Source/AnastasisSim unchanged.

## INTEGRATION
Only this rejection record. Accept soil-slope-002 as bounded rock material improvement; accept ecotone-002 as placement correction with shared-cap redistribution also affecting banks. Valley-air-001 is diagnostic REJECT only. Their individual handoffs define ownership and proof boundaries.

## ROLLBACK
No tree runtime rollback needed: experimental meshes and source already restored exactly from this branch HEAD. Revert only this documentation commit if needed. Never restore the whole repository or erase other agents' work.

## STOP
No new tree realism claim, no AAA claim. No clean shutdown or overall stability claim. Remaining tree silhouettes need a better geometry/LOD solution or a genuinely accessible realistic library.
