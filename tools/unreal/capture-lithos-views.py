"""Captures LITHOS_FORGE before/after + human-height views.

anastasis.Dressing.Lithos 0 = relief seul (baseline).
anastasis.Dressing.Lithos 1 = formations geologiques.

The level is never saved.
"""
import os, time, unreal

OUT_DIR = os.environ.get('ANASTASIS_LITHOS_OUT', os.path.join(unreal.Paths.project_saved_dir(), 'LithosEvidence'))
os.makedirs(OUT_DIR, exist_ok=True)
SEED = 12345
LEVEL = '/Game/Anastasis/Maps/Lvl_AnastasisSlice'

OVERVIEW_LOC = unreal.Vector(-5400.0, -5400.0, 10500.0)
OVERVIEW_ROT = unreal.Rotator(0.0, -32.8, 45.0)

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

unreal.log('LITHOS_CAPTURE_BOOT out=%s' % OUT_DIR)
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
cmd('anastasis.Terrain.Forge 1')

cls = unreal.load_class(None, '/Script/Anastasis_UnrealV2.AnastasisWorldEmbodiment')
found = unreal.GameplayStatics.get_all_actors_of_class(world, cls)
actor = found[0] if len(found) > 0 else eas.spawn_actor_from_class(cls, unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0))


def embody(lithos):
    cmd('anastasis.Dressing.Lithos %d' % lithos)
    ok = actor.call_method('EmbodyCanonical', args=(SEED,))
    redraw()
    unreal.log('LITHOS_EMBODY lithos=%d ok=%s' % (lithos, ok))
    return ok


def aim(loc, rot):
    cmd('viewmode lit')
    cmd('ShowFlag.Sprites 0')
    cmd('ShowFlag.Grid 0')
    ues.set_level_viewport_camera_info(loc, rot)
    redraw()
    got_loc, got_rot = ues.get_level_viewport_camera_info()
    unreal.log('LITHOS_CAMERA loc=(%.0f,%.0f,%.0f) pitch=%.1f yaw=%.1f'
               % (got_loc.x, got_loc.y, got_loc.z, got_rot.pitch, got_rot.yaw))


def look_at(from_loc, to_loc):
    return unreal.MathLibrary.find_look_at_rotation(from_loc, to_loc)


shots = [
    os.path.join(OUT_DIR, 'A_aerial_before.png').replace('\\', '/'),
    os.path.join(OUT_DIR, 'B_aerial_after.png').replace('\\', '/'),
    os.path.join(OUT_DIR, 'C_oblique_after.png').replace('\\', '/'),
    os.path.join(OUT_DIR, 'D_human_cliff.png').replace('\\', '/'),
    os.path.join(OUT_DIR, 'E_human_talus.png').replace('\\', '/'),
]
for path in shots:
    if os.path.exists(path):
        os.remove(path)

human_cliff = unreal.Vector(3600, 4800, 520)
human_cliff_rot = unreal.Rotator(0.0, -8.0, 40.0)
human_talus = unreal.Vector(4200, 5100, 420)
human_talus_rot = unreal.Rotator(0.0, -6.0, 55.0)
oblique_loc = unreal.Vector(-1800.0, -2400.0, 4200.0)
oblique_rot = unreal.Rotator(0.0, -28.0, 38.0)

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
    global phase, mark, requested, active, human_cliff, human_cliff_rot, human_talus, human_talus_rot, oblique_loc, oblique_rot
    elapsed = time.monotonic() - mark
    redraw()
    if elapsed > 90:
        finish('LITHOS_TIMEOUT phase=%d requested=%s' % (phase, requested), True)
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
            unreal.log('LITHOS_SHOT_REQUESTED A')
        elif requested and elapsed > 12.0:
            if not os.path.isfile(shots[0]):
                finish('LITHOS_SHOT_MISSING A', True)
                return
            unreal.log('LITHOS_SHOT_OK A bytes=%d' % os.path.getsize(shots[0]))
            embody(1)
            basin = actor.call_method('GetTerrainForgeBasin')
            landmark = actor.call_method('GetTerrainForgeLandmark')
            unreal.log('LITHOS_SITES basin=(%.0f,%.0f,%.0f) landmark=(%.0f,%.0f,%.0f)'
                       % (basin.x, basin.y, basin.z, landmark.x, landmark.y, landmark.z))
            if landmark.x > 1.0:
                human_cliff = unreal.Vector(landmark.x - 620.0, landmark.y - 480.0, landmark.z * 0.35 + 220.0)
                human_cliff_rot = look_at(human_cliff, unreal.Vector(landmark.x, landmark.y, landmark.z * 0.72))
                human_talus = unreal.Vector(landmark.x - 180.0, landmark.y + 520.0, landmark.z * 0.18 + 140.0)
                human_talus_rot = look_at(human_talus, unreal.Vector(landmark.x, landmark.y, landmark.z * 0.40))
                oblique_loc = unreal.Vector(landmark.x - 2200.0, landmark.y - 1800.0, landmark.z + 1600.0)
                oblique_rot = look_at(oblique_loc, unreal.Vector(landmark.x, landmark.y, landmark.z * 0.55))
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
            unreal.log('LITHOS_SHOT_REQUESTED B')
        elif requested and elapsed > 12.0:
            if not os.path.isfile(shots[1]):
                finish('LITHOS_SHOT_MISSING B', True)
                return
            unreal.log('LITHOS_SHOT_OK B bytes=%d' % os.path.getsize(shots[1]))
            aim(oblique_loc, oblique_rot)
            phase = 3
            mark = time.monotonic()
            requested = False
    elif phase == 3:
        if not requested and elapsed > 4.0:
            aim(oblique_loc, oblique_rot)
            active = shots[2]
            cmd('HighResShot 1920x1080 filename="%s"' % active)
            requested = True
            mark = time.monotonic()
            unreal.log('LITHOS_SHOT_REQUESTED C')
        elif requested and elapsed > 12.0:
            if not os.path.isfile(shots[2]):
                finish('LITHOS_SHOT_MISSING C', True)
                return
            unreal.log('LITHOS_SHOT_OK C bytes=%d' % os.path.getsize(shots[2]))
            aim(human_cliff, human_cliff_rot)
            phase = 4
            mark = time.monotonic()
            requested = False
    elif phase == 4:
        if not requested and elapsed > 4.0:
            aim(human_cliff, human_cliff_rot)
            active = shots[3]
            cmd('HighResShot 1920x1080 filename="%s"' % active)
            requested = True
            mark = time.monotonic()
            unreal.log('LITHOS_SHOT_REQUESTED D')
        elif requested and elapsed > 12.0:
            if not os.path.isfile(shots[3]):
                finish('LITHOS_SHOT_MISSING D', True)
                return
            unreal.log('LITHOS_SHOT_OK D bytes=%d' % os.path.getsize(shots[3]))
            aim(human_talus, human_talus_rot)
            phase = 5
            mark = time.monotonic()
            requested = False
    elif phase == 5:
        if not requested and elapsed > 4.0:
            aim(human_talus, human_talus_rot)
            active = shots[4]
            cmd('HighResShot 1920x1080 filename="%s"' % active)
            requested = True
            mark = time.monotonic()
            unreal.log('LITHOS_SHOT_REQUESTED E')
        elif requested and elapsed > 12.0:
            if not os.path.isfile(shots[4]):
                finish('LITHOS_SHOT_MISSING E', True)
                return
            unreal.log('LITHOS_SHOT_OK E bytes=%d' % os.path.getsize(shots[4]))
            finish('LITHOS_CAPTURE_COMPLETE')


handle = unreal.register_slate_post_tick_callback(tick)
