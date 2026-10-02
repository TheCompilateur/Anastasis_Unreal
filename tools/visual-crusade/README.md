# Tree silhouette pass

`render.ps1` regenerates the 29 owned tree meshes in the existing library then takes five views of the existing map. It needs `Saved/VisualCrusade/before/ground/cameras.json` (the exact baseline poses); no map is saved. The final files are already generated; do not regenerate merely to view them.

`generate.py` preserves existing asset identities, collision settings and material slots. It deliberately does not run material creation. `generate-capture.py` runs the direct capture without the editor-batch proxy.

`run.ps1` and `village.py` preserve the original before/v1 attempt. That batch failed at reload/PIE. They are historical reproduction helpers, not a verified production or PIE gate. `water-candidate.patch` was prepared then abandoned when ownership went to atmosphere-crusade-001; it is not applied.

All evidence and limits: `docs/unreal/handoffs/visual-crusade-001.md`. V1 rejected, v2 partial improvement. This folder does not confer a new simulator or asset framework.