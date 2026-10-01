"""HYDRA_FORGE_001 -- captures before/after des trois archetypes.

HighResShot is served by the next viewport redraw. A static scene never
redraws, so we invalidate viewports. The filename= argument is unreliable on
this editor; we pick up the newest PNG from Saved/Screenshots like
capture-tree-lineup.py.
"""
import os, shutil, time, unreal

OUT_DIR = os.environ.get('ANASTASIS_HYDRA_OUT', os.path.join(unreal.Paths.project_saved_dir(), 'HydraEvidence'))
os.makedirs(OUT_DIR, exist_ok=True)
SHOT_DIR = os.path.abspath(os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), 'Saved', 'Screenshots'))
SEED = 12345
LEVEL = '/Game/Anastasis/Maps/Lvl_AnastasisSlice'

OVERVIEW_LOC = unreal.Vector(-5400.0, -5400.0, 10500.0)
OVERVIEW_ROT = unreal.Rotator(0.0, -32.8, 45.0)

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

unreal.log('HYDRA_CAPTURE_BOOT out=%s' % OUT_DIR)
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


def embody(hydro):
    cmd('anastasis.Dressing.Hydrology %d' % hydro)
    ok = actor.call_method('EmbodyCanonical', args=(SEED,))
    redraw()
    unreal.log('HYDRA_EMBODY hydro=%d ok=%s' % (hydro, ok))
    return ok


def aim(loc, rot):
    cmd('viewmode lit')
    cmd('ShowFlag.Sprites 0')
    cmd('ShowFlag.Grid 0')
    ues.set_level_viewport_camera_info(loc, rot)
    redraw()
    got_loc, got_rot = ues.get_level_viewport_camera_info()
    unreal.log('HYDRA_CAMERA loc=(%.0f,%.0f,%.0f) pitch=%.1f yaw=%.1f'
               % (got_loc.x, got_loc.y, got_loc.z, got_rot.pitch, got_rot.yaw))


def look_at(from_loc, to_loc):
    return unreal.MathLibrary.find_look_at_rotation(from_loc, to_loc)


def oblique_of(site):
    loc = unreal.Vector(site.x - 720.0, site.y - 860.0, site.z + 380.0)
    rot = look_at(loc, unreal.Vector(site.x, site.y, site.z + 20.0))
    return loc, rot


def human_of(site, dx=-40.0, dy=-90.0):
    loc = unreal.Vector(site.x + dx, site.y + dy, max(site.z, 275.0) + 120.0)
    rot = look_at(loc, unreal.Vector(site.x + 70.0, site.y + 40.0, site.z + 25.0))
    return loc, rot


SHOTS = [
    'torrent_before.png',
    'valley_before.png',
    'inflow_before.png',
    'torrent_after.png',
    'valley_after.png',
    'inflow_after.png',
    'aerial_after.png',
    'oblique_after.png',
    'human_torrent.png',
    'human_inflow.png',
]
paths = [os.path.join(OUT_DIR, name).replace('\\', '/') for name in SHOTS]
for path in paths:
    if os.path.exists(path):
        os.remove(path)

sites = {
    'torrent': unreal.Vector(2400, 2400, 280),
    'valley': unreal.Vector(3600, 3600, 280),
    'inflow': unreal.Vector(4800, 4800, 280),
}

phase = 0
mark = time.monotonic()
requested = False
handle = None
shot_stamp = time.time()
retries = 0


def newest_png(after):
    best, best_t = None, after
    if not os.path.isdir(SHOT_DIR):
        return None
    for root, _dirs, files in os.walk(SHOT_DIR):
        for name in files:
            if not name.lower().endswith('.png'):
                continue
            full = os.path.join(root, name)
            t = os.path.getmtime(full)
            if t > best_t:
                best, best_t = full, t
    return best


def finish(msg, error=False):
    (unreal.log_error if error else unreal.log)(msg)
    unreal.unregister_slate_post_tick_callback(handle)
    unreal.SystemLibrary.quit_editor()


def tick(_dt):
    global phase, mark, requested, sites, shot_stamp, retries
    elapsed = time.monotonic() - mark
    redraw()
    if elapsed > 120:
        finish('HYDRA_TIMEOUT phase=%d requested=%s' % (phase, requested), True)
        return

    def request_shot(index):
        global requested, mark, shot_stamp
        cmd('HighResShot 1920x1080 filename="%s"' % paths[index])
        requested = True
        mark = time.monotonic()
        shot_stamp = time.time()
        unreal.log('HYDRA_SHOT_REQUESTED %s' % SHOTS[index])

    def wait_shot(index, next_phase, next_setup):
        global phase, mark, requested
        if not requested and elapsed > 8.0:
            request_shot(index)
            return
        if requested and elapsed > 14.0:
            dest = paths[index]
            found = dest if (os.path.isfile(dest) and os.path.getsize(dest) > 1000) else newest_png(shot_stamp - 1.0)
            if found:
                if found != dest:
                    shutil.copyfile(found, dest)
                unreal.log('HYDRA_SHOT_OK %s bytes=%d' % (SHOTS[index], os.path.getsize(dest)))
                next_setup()
                phase = next_phase
                mark = time.monotonic()
                requested = False
                return
            finish('HYDRA_SHOT_MISSING %s dir=%s' % (SHOTS[index], SHOT_DIR), True)

    if phase == 0:
        if elapsed > 3.0:
            embody(0)
            embody(1)
            t = actor.call_method('GetHydrologyTorrentSite')
            v = actor.call_method('GetHydrologyValleySite')
            i = actor.call_method('GetHydrologyInflowSite')
            unreal.log('HYDRA_SITES torrent=(%.0f,%.0f,%.0f) valley=(%.0f,%.0f,%.0f) inflow=(%.0f,%.0f,%.0f)'
                       % (t.x, t.y, t.z, v.x, v.y, v.z, i.x, i.y, i.z))
            if t.x > 1.0:
                sites['torrent'] = t
            if v.x > 1.0:
                sites['valley'] = v
            if i.x > 1.0:
                sites['inflow'] = i
            embody(0)
            aim(*oblique_of(sites['torrent']))
            phase = 1
            mark = time.monotonic()
            requested = False
    elif phase == 1:
        if not requested:
            aim(*oblique_of(sites['torrent']))
        wait_shot(0, 2, lambda: aim(*oblique_of(sites['valley'])))
    elif phase == 2:
        wait_shot(1, 3, lambda: aim(*oblique_of(sites['inflow'])))
    elif phase == 3:
        wait_shot(2, 4, lambda: (embody(1), aim(*oblique_of(sites['torrent']))))
    elif phase == 4:
        wait_shot(3, 5, lambda: aim(*oblique_of(sites['valley'])))
    elif phase == 5:
        wait_shot(4, 6, lambda: aim(*oblique_of(sites['inflow'])))
    elif phase == 6:
        wait_shot(5, 7, lambda: aim(OVERVIEW_LOC, OVERVIEW_ROT))
    elif phase == 7:
        wait_shot(6, 8, lambda: aim(*oblique_of(sites['valley'])))
    elif phase == 8:
        wait_shot(7, 9, lambda: aim(*human_of(sites['torrent'])))
    elif phase == 9:
        wait_shot(8, 10, lambda: aim(*human_of(sites['inflow'], 140.0, -180.0)))
    elif phase == 10:
        wait_shot(9, 11, lambda: None)
    elif phase == 11:
        finish('HYDRA_CAPTURE_COMPLETE')


handle = unreal.register_slate_post_tick_callback(tick)
