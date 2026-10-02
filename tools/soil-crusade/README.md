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


Slope follow-up (soil-slope-002): set ANASTASIS_SOIL_PARAMETER=SlopeSurface and
ANASTASIS_SOIL_VIEWS=prairie_eye,pente_face,hors_vallee_eye,oblique, then run
capture.ps1 -Label slope-v1 -Rebuild. Same states toggle SlopeSurface0/1/0
while SoilHistory stays enabled. Missing requested views now fail explicitly.
SlopeSurface defaults to1 in the saved master; set0 for rollback. SlopeSurfaceSize
is900cm. No new textures or geometry. See soil-slope-002 handoff for bounded KEEP:
one cliff has visible middle-distance fabric; pente_face is not discriminating.


Contact pilot (soil-contact-003): ANASTASIS_SOIL_CONTACT=1 with
capture.ps1 -Label contact-v1 captures five fixed cameras (three eye positions over
6m, one lateral eye view, one elevated context). Before/after/control hide/show/hide
only SoilContact_* HISM. They do not rebuild terrain or vegetation, or change soil
materials. This is a sequence of editor viewpoints, not player movement proof.
Pilot restricted to seed12345 scale5 near the existing cliff/water junction.
Disable with anastasis.Dressing.SoilContact 0 then re-embody. No collision/navigation.
Existing RockSplit/Boulder/Low meshes, <=162 proposed instances over <=33m diameter,
rejected when unsupported or deeply submerged; small detail culls by60m, source180m.
Validation pending: neither compilation nor instance count establishes visual quality.


Prepared diagnostic, not a material change: the capture also accepts numeric
ANASTASIS_SOIL_BEFORE / ANASTASIS_SOIL_AFTER (defaults0/1, control repeats BEFORE).
To isolate the procedural normal from the photographic normal in meadow/river views:
ANASTASIS_SOIL_CONTACT=0
ANASTASIS_SOIL_PARAMETER=BumpStrength
ANASTASIS_SOIL_BEFORE=0.16
ANASTASIS_SOIL_AFTER=0
ANASTASIS_SOIL_VIEWS=prairie_eye,riviere_eye
capture.ps1 -Label ground-normal-diagnostic
No -Rebuild or -Deploy: transient MIDs only, no asset save. This diagnostic is not
executed yet and does not claim that removing bump improves the ground. Run only
in a coordinated slot; do not extend the contact slot ahead of queued agents.

Contact003 now defaults OFF until image review. The registered soil-contact-capture
job enables it temporarily, captures15images and restores its prior CVar/visibility.
Completion is technical only. The source can be queued without shipping an active
unreviewed composition. Do not enable the default based solely on capture COMPLETE.
