"""
ROCK_FORGE_001 -- Gates 1, 8 and 9. BASELINE vs CANDIDATE at four scales.

Produces eight captures in ONE editor session, so that nothing except the presence of
the rock vocabulary differs between the two halves: same level, same sun, same exposure,
same seed, same camera transforms, same embodiment call.

    BASELINE_{CLOSE,MID,GAMEPLAY,AERIAL}    Stone draws nothing (today's world)
    CANDIDATE_{CLOSE,MID,GAMEPLAY,AERIAL}   Stone draws ANASTASIS_ROCK_GRAMMAR_V1

WHY THE STONE ENTRY IS ADDED IN MEMORY AND NEVER SAVED
------------------------------------------------------
Registering Stone permanently means editing
/Game/Anastasis/Presentation/DA_AnastasisPresentation, a binary (LFS) asset that the
concurrent branch claude/anastasis-tree-visuals-136553 is ALREADY modifying for its
Forest entries. Two branches editing one .uasset is an unmergeable conflict, and the
mission's own stop condition covers exactly this case. So this script mutates the
LOADED registry object in memory and never saves it: AnastasisPresentation::GetRegistry()
hands C++ the same UObject, so the embodiment really does place rocks, the A/B is really
causal, and the file on disk is left untouched for whoever integrates last.
See docs/unreal/ROCK_FORGE_001.md -> MULTIAGENT_CONFLICTS.

Read-only with respect to every committed asset: it loads Lvl_AnastasisSlice and never
saves it, and it creates no asset of its own.

Env:
  ANASTASIS_ROCK_SHOTDIR   output directory (default Saved/RockEvidence)
  ANASTASIS_ROCK_MODE      anastasis.Terrain.Surface value, default "1" (sealed slice)

Run via tools/unreal/capture-rock-views.ps1.
"""
import json
import os
import shutil
import time

import unreal

SEED = 12345
LEVEL = '/Game/Anastasis/Maps/Lvl_AnastasisSlice'
ROCK_DIR = '/Game/Anastasis/Rock'
MODE = os.environ.get('ANASTASIS_ROCK_MODE', '1')
OUT_DIR = os.environ.get(
    'ANASTASIS_ROCK_SHOTDIR',
    os.path.join(unreal.Paths.project_saved_dir(), 'RockEvidence'))

# Tint: the same HUE family as AnastasisTerrainSurface's HighlandRock
# (0.518, 0.490, 0.463), but darker.
#
# The first pass used HighlandRock's exact value, reasoning that a rock and the highland
# it stands on should read as one material family. The captures refuted it: at 0.52
# albedo under the rig's 75000 lux / EV100 14, both the ground AND the rocks clamp to
# near-white, and the rocks stop reading as stone -- they read as snow. Matching the
# ground's albedo exactly is what destroyed the separation. Same family, two thirds the
# value, is what makes the mass legible against the ground it emerges from.
ROCK_TINT = (0.150, 0.142, 0.134)

# Scale envelope, cut ~3x after the first capture.
#
# 0.85-1.85 made rocks 3-5 tiles wide: from above the whole formation read as one white
# blob rather than as distinct mineral masses, which fails the AERIAL gate outright, and
# at MID they dwarfed the trees. A tile is 100 UU and these meshes are ~200 UU across, so
# this envelope puts a rock at roughly 0.6-1.4 tiles -- boulder scale, not hill scale.
ROCK_MIN_SCALE = 0.30
ROCK_MAX_SCALE = 0.70
ROCK_JITTER = 0.34

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)


def log(msg):
    unreal.log('ROCK_VIEW ' + str(msg))


def err(msg):
    unreal.log_error('ROCK_VIEW ' + str(msg))


# ----------------------------------------------------------------------------------
# Registry mutation (in memory only)
# ----------------------------------------------------------------------------------

REGISTRY_PATH = '/Game/Anastasis/Presentation/DA_AnastasisPresentation'


def set_prop(obj, names, value):
    """bEnabled surfaces as either `enabled` or `b_enabled` depending on the binding.

    Same defensive shape as tools/unreal/presentation-registry.py.
    """
    for name in names:
        try:
            obj.set_editor_property(name, value)
            return
        except Exception:
            continue
    raise Exception('none of these properties exist: ' + ','.join(names))


def rock_meshes():
    paths = unreal.EditorAssetLibrary.list_assets(ROCK_DIR, recursive=False)
    out = []
    for p in sorted(paths):
        asset = unreal.load_asset(p)
        if isinstance(asset, unreal.StaticMesh):
            out.append(asset)
    return out


def make_stone_entry(meshes):
    entry = unreal.AnastasisPresentationEntry()
    entry.set_editor_property('semantic_type', unreal.AnastasisSemanticType.STONE)
    entry.set_editor_property('archetype_id', 'Rock_Grammar_V1')
    set_prop(entry, ('enabled', 'b_enabled'), True)
    entry.set_editor_property(
        'tint', unreal.LinearColor(ROCK_TINT[0], ROCK_TINT[1], ROCK_TINT[2], 1.0))
    entry.set_editor_property('min_uniform_scale', ROCK_MIN_SCALE)
    entry.set_editor_property('max_uniform_scale', ROCK_MAX_SCALE)
    entry.set_editor_property('jitter_radius_fraction', ROCK_JITTER)
    set_prop(entry, ('random_yaw', 'b_random_yaw'), True)
    variants = []
    for mesh in meshes:
        v = unreal.AnastasisPresentationVariant()
        v.set_editor_property('mesh', mesh)
        variants.append(v)
    entry.set_editor_property('variants', variants)
    return entry


class StoneToggle(object):
    """Adds/removes the Stone entry on the LOADED registry. Never saves."""

    def __init__(self):
        self.registry = unreal.load_asset(REGISTRY_PATH)
        if self.registry is None:
            raise RuntimeError('registry not loadable: ' + REGISTRY_PATH)
        self.baseline = list(self.registry.get_editor_property('entries'))
        self.meshes = rock_meshes()
        log('ROCK_MESHES n=%d %s' % (
            len(self.meshes), ','.join(m.get_name() for m in self.meshes)))
        if not self.meshes:
            raise RuntimeError('no rock meshes under ' + ROCK_DIR)

    def set_enabled(self, enabled):
        entries = list(self.baseline)
        if enabled:
            entries.append(make_stone_entry(self.meshes))
        self.registry.set_editor_property('entries', entries)
        log('STONE_ENTRY enabled=%s registry_entries=%d' % (enabled, len(entries)))

    def restore(self):
        self.registry.set_editor_property('entries', self.baseline)
        log('STONE_ENTRY restored to %d entries (nothing saved)' % len(self.baseline))


# ----------------------------------------------------------------------------------
# Scene
# ----------------------------------------------------------------------------------

log('BOOT mode=%s out=%s' % (MODE, OUT_DIR))
if not os.path.isdir(OUT_DIR):
    os.makedirs(OUT_DIR)

log('MAP_LOAD=' + str(les.load_level(LEVEL)))
world = ues.get_editor_world()

for cmd in ('ShowFlag.Sprites 0', 'ShowFlag.Grid 0', 'viewmode lit',
            'anastasis.Terrain.Surface ' + MODE):
    unreal.SystemLibrary.execute_console_command(world, cmd)

emb_cls = unreal.load_class(None, '/Script/Anastasis_UnrealV2.AnastasisWorldEmbodiment')
found = unreal.GameplayStatics.get_all_actors_of_class(world, emb_cls)
if not found:
    raise RuntimeError('no AAnastasisWorldEmbodiment in ' + LEVEL)
embodiment = found[0]

toggle = StoneToggle()


def embody():
    ok = embodiment.call_method('EmbodyCanonical', args=(SEED,))
    dressing = embodiment.call_method('GetDressingInstanceCount')
    log('EMBODY ok=%s dressing_instances=%s' % (ok, dressing))
    return dressing


def rock_instance_locations():
    """Where the grammar actually landed, read back off the HISM components."""
    locs = []
    for comp in embodiment.get_components_by_class(
            unreal.HierarchicalInstancedStaticMeshComponent):
        mesh = comp.get_editor_property('static_mesh')
        if mesh is None or not mesh.get_name().startswith('SM_Rock_'):
            continue
        count = comp.get_instance_count()
        for i in range(count):
            # Returns a bare Transform in this build, not a (bool, Transform) tuple.
            xf = comp.get_instance_transform(i, world_space=True)
            if xf is None:
                continue
            loc = xf.translation
            locs.append((loc.x, loc.y, loc.z))
    return locs


def densest_cluster(locs, radius=520.0):
    """Centroid of the richest neighbourhood -- aim at real rocks, not at a guess."""
    best, best_n = None, -1
    for ax, ay, az in locs:
        near = [(x, y, z) for x, y, z in locs
                if (x - ax) ** 2 + (y - ay) ** 2 <= radius * radius]
        if len(near) > best_n:
            best_n = len(near)
            best = (sum(p[0] for p in near) / len(near),
                    sum(p[1] for p in near) / len(near),
                    max(p[2] for p in near))
    return best, best_n


# Find the aim point WITH rocks on, so both halves frame the same ground.
#
# Everything up to the tick loop runs inside this guard. An exception here used to
# leave the editor running forever with no script attached, and a stray editor blocks
# UnrealBuildTool for EVERY agent on this machine ("Unable to build while Live Coding
# is active"), not just this one. Failing loudly and quitting is the neighbourly
# behaviour.
try:
    toggle.set_enabled(True)
    embody()
    locations = rock_instance_locations()
    log('ROCK_INSTANCES n=%d' % len(locations))
    if locations:
        AIM, AIM_N = densest_cluster(locations)
    else:
        err('NO_ROCK_INSTANCES -- falling back to slice centre')
        AIM, AIM_N = (1600.0, 1600.0, 400.0), 0
except Exception as exc:  # noqa: BLE001
    import traceback
    err('SETUP_FAILED %s\n%s' % (exc, traceback.format_exc()))
    unreal.SystemLibrary.quit_editor()
    raise
log('AIM=(%.0f,%.0f,%.0f) neighbours=%d' % (AIM[0], AIM[1], AIM[2], AIM_N))


def rig(dist, height, pitch, yaw_deg=35.0):
    import math
    yaw = math.radians(yaw_deg)
    loc = unreal.Vector(AIM[0] - math.cos(yaw) * dist,
                        AIM[1] - math.sin(yaw) * dist,
                        AIM[2] + height)
    return loc, unreal.Rotator(0.0, pitch, yaw_deg)


# The four scales the mission requires. An improvement visible at only one of them
# does not count, so all four are captured for both halves.
VIEWS = [
    ('CLOSE', rig(330.0, 150.0, -14.0)),      # is it still a primitive up close?
    ('MID', rig(1000.0, 430.0, -19.0)),       # does the silhouette hold?
    ('GAMEPLAY', rig(620.0, 175.0, -8.0)),    # does it structure the ground at eye level?
    ('AERIAL', rig(2100.0, 4200.0, -62.0)),   # legible mass, or noise?
]

JOBS = []
for half, enabled in (('BASELINE', False), ('CANDIDATE', True)):
    for name, (loc, rot) in VIEWS:
        JOBS.append((half, name, enabled, loc, rot))

SHOT_DIR = os.path.join(unreal.Paths.project_saved_dir(), 'Screenshots')
RESULTS = []


def newest_png(after):
    best, best_t = None, after
    for root, _dirs, files in os.walk(SHOT_DIR):
        for f in files:
            if f.lower().endswith('.png'):
                full = os.path.join(root, f)
                t = os.path.getmtime(full)
                if t > best_t:
                    best, best_t = full, t
    return best


def aim_at(loc, rot):
    # Viewport view mode persists between editor sessions; a previous "lighting only"
    # would render everything neutral grey and destroy the comparison.
    unreal.SystemLibrary.execute_console_command(world, 'viewmode lit')
    unreal.SystemLibrary.execute_console_command(world, 'ShowFlag.Sprites 0')
    unreal.SystemLibrary.execute_console_command(world, 'ShowFlag.Grid 0')
    ues.set_level_viewport_camera_info(loc, rot)


job_index = -1
state = 'next'
mark = 0.0
t_state = time.monotonic()
current = None
handle = None
current_enabled = None


def finish(msg, error=False):
    (err if error else log)(msg)
    try:
        toggle.restore()
    except Exception as exc:  # noqa: BLE001
        err('restore failed: %s' % exc)
    with open(os.path.join(OUT_DIR, 'rock_views.json'), 'w') as fh:
        json.dump({'aim': AIM, 'results': RESULTS}, fh, indent=2)
    unreal.unregister_slate_post_tick_callback(handle)
    unreal.SystemLibrary.quit_editor()


def tick(dt):
    global job_index, state, mark, t_state, current, current_enabled
    now = time.monotonic()
    elapsed = now - t_state

    if state == 'next':
        job_index += 1
        if job_index >= len(JOBS):
            finish('COMPLETE shots=%d' % len(RESULTS))
            return
        current = JOBS[job_index]
        half, name, enabled, loc, rot = current
        if enabled != current_enabled:
            toggle.set_enabled(enabled)
            count = embody()
            log('HALF=%s dressing=%s' % (half, count))
            current_enabled = enabled
        aim_at(loc, rot)
        log('AIMING %s_%s loc=(%.0f,%.0f,%.0f) pitch=%.0f' % (
            half, name, loc.x, loc.y, loc.z, rot.pitch))
        state, t_state = 'settle', now
        return

    if state == 'settle' and elapsed > 2.5:
        half, name, enabled, loc, rot = current
        aim_at(loc, rot)
        mark = time.time()
        unreal.SystemLibrary.execute_console_command(world, 'HighResShot 1920x1080')
        log('SHOT_REQUESTED %s_%s' % (half, name))
        state, t_state = 'wait', now
        return

    if state == 'wait':
        png = newest_png(mark)
        if png:
            half, name, enabled, loc, rot = current
            dest = os.path.join(OUT_DIR, '%s_%s.png' % (half, name))
            shutil.copyfile(png, dest)
            RESULTS.append({'half': half, 'view': name,
                            'file': os.path.basename(dest),
                            'bytes': os.path.getsize(dest)})
            log('SHOT_OK %s bytes=%d' % (os.path.basename(dest), os.path.getsize(dest)))
            state, t_state = 'next', now
        elif elapsed > 14.0:
            half, name, enabled, loc, rot = current
            aim_at(loc, rot)
            mark = time.time()
            unreal.SystemLibrary.execute_console_command(world, 'HighResShot 1920x1080')
            log('SHOT_RETRY %s_%s' % (half, name))
            t_state = now
        return


handle = unreal.register_slate_post_tick_callback(tick)
