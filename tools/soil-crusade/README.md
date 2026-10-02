# Soil crusade

Owner: agent/soil-crusade-001. Base: 506f6db49aea640d4f0a8f96373b2e84d8853590.

Authority: tools/unreal/ground-material.py. Existing procedural TerrainSurface, not Landscape.
SoilHistory=0 restores the previous behavior; 1 enables the soil layer.
The master retains diagnostic SoilPilotCenter=(96000,110000,0) cm and radius=38000 cm
(outer 20% feather). The shipped instance overrides radius to 10000000 cm, covering
this world. Set its radius back to 38000 for the original pilot.

Rendered slope, water-proximity wetness (UV1.y), litter (UV0.y), rock and the existing
meso field govern exposed rock, dry thin earth and plausible moist fines. Near detail
fades from 2.5 to 16 m: a mineral/organic matrix replaces the flat green surface under
the separate grass geometry. Litter follows Forest simulation tiles, not actual crowns.
Wetness is not flood history; no claim to reconstruct sediment transport or old rivers.
No new geometry, collision, tick, texture, vegetation asset or simulator changes.

Commands (one editor, MAIN.lock respected, ANASTASIS_EDITOR_MAX=1):
- tools/soil-crusade/capture.ps1 -Label pilot-v2 -Rebuild: regenerate master and instance,
  then capture prairie, riverbank, forest edge, forest floor, slope and oblique view.
- tools/soil-crusade/capture.ps1 -Label deployed -Deploy: save the instance coverage
  and compare two views outside the pilot (vallee_b_eye, hors_vallee_eye).
- tools/soil-crusade/capture.ps1 -Label check: compare the current assets.

The isolated capture derives from ground-cover-capture.py. SoilHistory alone changes
0/1/0; cameras, geometry and dynamic instances stay fixed. Screenshots are 1600x900;
GPU stat-unit sampling precedes HighResShot, so this is not a packaged benchmark.
KEEP requires eye-height improvement beyond before/control drift and acceptable GPU
cost. Compilation alone is not KEEP. The launcher rejects a crash even after COMPLETE.

Pilot V1: artistic REJECT, weak effect and exit3 after complete images.
Pilot V2: local KEEP / artistic PARTIAL, 18 images inspected, clean exit0.
GPU after 11.379..15.973 ms; +0.102..0.368 ms against before, within control drift
(up to +0.482 ms). Shader: 198 expressions, 1066 pixel instructions, 10 samplers.
Limits: close Worked grain can look too regular/tilled; no physical microrelief added;
alluvial history and combined Naturalist/tree/weather result are not demonstrated.
See docs/unreal/handoffs/soil-crusade-001.md for deployment evidence and handoff.

Deployment: six images inspected in Saved/SoilEvidence/deployed-v3. Reloaded master
parameters verified; global radius read back and instance saved. GPU after12.968/14.197ms.
Complete images followed by exit3 shutdown crash: EDITOR_EXIT::FAIL, not a clean PASS.
Large steep faces remain smooth. Integration must inspect the combined result.
