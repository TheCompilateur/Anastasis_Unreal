"""Captures TERRAIN_FORGE before/after + human + basin views.

HighResShot is served by the next viewport redraw. A static scene never
redraws, so we invalidate viewports (see capture-tree-lineup.py).
"""
import os, math, time, unreal

OUT_DIR = os.environ.get('ANASTASIS_FORGE_OUT', os.path.join(unreal.Paths.project_saved_dir(), 'TerrainForgeEvidence'))
os.makedirs(OUT_DIR, exist_ok=True)
SEED = 12345
LEVEL = '/Game/Anastasis/Maps/Lvl_AnastasisSlice'

OVERVIEW_LOC = unreal.Vector(-5400.0, -5400.0, 10500.0)
OVERVIEW_ROT = unreal.Rotator(0.0, -32.8, 45.0)

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

unreal.log('FORGE_CAPTURE_BOOT out=%s' % OUT_DIR)
les.load_level(LEVEL)
world = ues.get_editor_world()


def cmd(c):
    unreal.SystemLibrary.execute_console_command(world, c)


def redraw():
    try:
        les.editor_invalidate_viewports()
    except Exception:
        pass


cmd('ShowFlag.Sprites 0')
cmd('ShowFlag.Grid 0')
cmd('viewmode lit')
cmd('anastasis.Terrain.Surface 2')

cls = unreal.load_class(None, '/Script/Anastasis_UnrealV2.AnastasisWorldEmbodiment')
found = unreal.GameplayStatics.get_all_actors_of_class(world, cls)
actor = found[0] if len(found) > 0 else eas.spawn_actor_from_class(cls, unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0))


def hide_dressing():
    for comp in actor.get_components_by_class(unreal.HierarchicalInstancedStaticMeshComponent):
        comp.set_visibility(False)


def embody(forge):
    cmd('anastasis.Terrain.Forge %d' % forge)
    ok = actor.call_method('EmbodyCanonical', args=(SEED,))
    hide_dressing()
    redraw()
    unreal.log('FORGE_EMBODY forge=%d ok=%s' % (forge, ok))
    return ok


def aim(loc, rot):
    cmd('viewmode lit')
    cmd('ShowFlag.Sprites 0')
    cmd('ShowFlag.Grid 0')
    ues.set_level_viewport_camera_info(loc, rot)
    redraw()
    got_loc, got_rot = ues.get_level_viewport_camera_info()
    unreal.log('FORGE_CAMERA loc=(%.0f,%.0f,%.0f) pitch=%.1f yaw=%.1f'
               % (got_loc.x, got_loc.y, got_loc.z, got_rot.pitch, got_rot.yaw))


def look_at(from_loc, to_loc):
    return unreal.MathLibrary.find_look_at_rotation(from_loc, to_loc)


shots = [
    os.path.join(OUT_DIR, 'A_overview_before.png').replace('\\', '/'),
    os.path.join(OUT_DIR, 'B_overview_after.png').replace('\\', '/'),
    os.path.join(OUT_DIR, 'C_human.png').replace('\\', '/'),
    os.path.join(OUT_DIR, 'D_from_basin.png').replace('\\', '/'),
]
for path in shots:
    if os.path.exists(path):
        os.remove(path)

human_loc = unreal.Vector(3600, 4800, 520)
human_rot = unreal.Rotator(0.0, -8.0, 40.0)
village_loc = unreal.Vector(4200, 5100, 620)
village_rot = unreal.Rotator(0.0, -12.0, 55.0)

# job: (name, setup_fn) then camera applied separately
phase = 0
mark = time.monotonic()
requested = False
handle = None
active = None


def finish(msg, error=False):
    (unreal.log_error if error else unreal.log)(msg)
    unreal.unregister_slate_post_tick_callback(handle)
    unreal.SystemLibrary.quit_editor()


def tick(_dt):
    global phase, mark, requested, active, human_loc, human_rot, village_loc, village_rot
    elapsed = time.monotonic() - mark
    redraw()
    if elapsed > 90:
        finish('FORGE_TIMEOUT phase=%d requested=%s' % (phase, requested), True)
        return

    if phase == 0:
        if elapsed > 3.0:
            embody(0)
            aim(OVERVIEW_LOC, OVERVIEW_ROT)
            phase = 1
            mark = time.monotonic()
            requested = False
    elif phase == 1:
        if not requested and elapsed > 8.0:
            aim(OVERVIEW_LOC, OVERVIEW_ROT)
            active = shots[0]
            cmd('HighResShot 1920x1080 filename="%s"' % active)
            requested = True
            mark = time.monotonic()
            unreal.log('FORGE_SHOT_REQUESTED A')
        elif requested and elapsed > 12.0:
            if not os.path.isfile(shots[0]):
                finish('FORGE_SHOT_MISSING A', True)
                return
            unreal.log('FORGE_SHOT_OK A bytes=%d' % os.path.getsize(shots[0]))
            embody(1)
            basin = actor.call_method('GetTerrainForgeBasin')
            landmark = actor.call_method('GetTerrainForgeLandmark')
            unreal.log('FORGE_SITES basin=(%.0f,%.0f,%.0f) landmark=(%.0f,%.0f,%.0f)'
                       % (basin.x, basin.y, basin.z, landmark.x, landmark.y, landmark.z))
            if basin.x > 1.0 and landmark.x > 1.0:
                human_loc = unreal.Vector(basin.x - 420.0, basin.y - 520.0, basin.z + 180.0)
                human_rot = look_at(human_loc, unreal.Vector(landmark.x, landmark.y, landmark.z * 0.55 + basin.z * 0.45))
                village_loc = unreal.Vector(basin.x + 80.0, basin.y - 60.0, basin.z + 110.0)
                village_rot = look_at(village_loc, unreal.Vector(landmark.x, landmark.y, landmark.z + 40.0))
            aim(OVERVIEW_LOC, OVERVIEW_ROT)
            phase = 2
            mark = time.monotonic()
            requested = False
    elif phase == 2:
        if not requested and elapsed > 8.0:
            aim(OVERVIEW_LOC, OVERVIEW_ROT)
            active = shots[1]
            cmd('HighResShot 1920x1080 filename="%s"' % active)
            requested = True
            mark = time.monotonic()
            unreal.log('FORGE_SHOT_REQUESTED B')
        elif requested and elapsed > 12.0:
            if not os.path.isfile(shots[1]):
                finish('FORGE_SHOT_MISSING B', True)
                return
            unreal.log('FORGE_SHOT_OK B bytes=%d' % os.path.getsize(shots[1]))
            aim(human_loc, human_rot)
            phase = 3
            mark = time.monotonic()
            requested = False
    elif phase == 3:
        if not requested and elapsed > 4.0:
            aim(human_loc, human_rot)
            active = shots[2]
            cmd('HighResShot 1920x1080 filename="%s"' % active)
            requested = True
            mark = time.monotonic()
            unreal.log('FORGE_SHOT_REQUESTED C')
        elif requested and elapsed > 12.0:
            if not os.path.isfile(shots[2]):
                finish('FORGE_SHOT_MISSING C', True)
                return
            unreal.log('FORGE_SHOT_OK C bytes=%d' % os.path.getsize(shots[2]))
            aim(village_loc, village_rot)
            phase = 4
            mark = time.monotonic()
            requested = False
    elif phase == 4:
        if not requested and elapsed > 4.0:
            aim(village_loc, village_rot)
            active = shots[3]
            cmd('HighResShot 1920x1080 filename="%s"' % active)
            requested = True
            mark = time.monotonic()
            unreal.log('FORGE_SHOT_REQUESTED D')
        elif requested and elapsed > 12.0:
            if not os.path.isfile(shots[3]):
                finish('FORGE_SHOT_MISSING D', True)
                return
            unreal.log('FORGE_SHOT_OK D bytes=%d' % os.path.getsize(shots[3]))
            finish('FORGE_CAPTURE_COMPLETE')


handle = unreal.register_slate_post_tick_callback(tick)
