"""PIE proof of map crown -> village rain coverage, with a 1/0/1 host switch.

Run through editor-batch.ps1 -Proofs canopy-rain-pie. This observes a rendered tree
sample and an open position; it does not claim a player's weather experience.
"""
import time

import unreal


level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
debug = unreal.AnastasisSimulationDebugLibrary
unreal.log('CANOPY_RAIN_MAP_LOAD=' + str(level.load_level('/Game/Anastasis/Maps/Lvl_AnastasisSlice')))
start = time.monotonic()
phase = 0
pie_started = 0.0
pie_seen = None
pie_frames = 0
handle = None


def finish(ok, detail):
    unreal.log(('CANOPY_RAIN_PIE PASS ' if ok else 'CANOPY_RAIN_PIE FAIL ') + detail)
    unreal.SystemLibrary.execute_console_command(None, 'anastasis.Village.CanopyRain 1')
    unreal.unregister_slate_post_tick_callback(handle)
    unreal.SystemLibrary.quit_editor()


def tick(_dt):
    global phase, pie_started, pie_seen, pie_frames
    now = time.monotonic()
    if now - start > 120:
        finish(False, 'timeout')
        return
    if phase == 0 and now - start > 2:
        phase = 1
        pie_started = now
        level.editor_request_begin_play()
        return
    if phase != 1 or not level.is_in_play_in_editor():
        return
    # The host binds the crowns on its own Tick (BindRainCanopy). PIE startup on this map now
    # blocks ~40 s inside the request frame (StaticDuplicateObject), so a wall clock started at
    # the request had already expired at the first PIE frame, before any host tick: the read
    # then saw an unbound village (0 everywhere). Count real PIE frames instead.
    if pie_seen is None:
        pie_seen = now
    pie_frames += 1
    if pie_frames < 30 or now - pie_seen < 3:
        return
    world = editor.get_game_world()
    if not world or debug.get_simulation_time(world) < 0:
        return
    actors = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.AnastasisWorldEmbodiment)
    if len(actors) != 1:
        finish(False, 'embodiment_count=%d' % len(actors))
        return
    point = actors[0].get_rain_canopy_sample_tiles()
    if point.z <= 0:
        finish(False, 'no_embodied_tree_sample')
        return
    x, y = float(point.x), float(point.y)
    open_cover = None
    for gy in range(5, 95, 5):
        for gx in range(5, 95, 5):
            candidate = debug.get_rain_canopy_cover(world, gx + 0.5, gy + 0.5)
            if candidate < 0.001:
                open_cover = candidate
                break
        if open_cover is not None:
            break
    if open_cover is None:
        finish(False, 'no_open_ground_sample')
        return
    unreal.SystemLibrary.execute_console_command(world, 'anastasis.Village.CanopyRain 1')
    on = debug.get_rain_canopy_cover(world, x, y)
    unreal.SystemLibrary.execute_console_command(world, 'anastasis.Village.CanopyRain 0')
    off = debug.get_rain_canopy_cover(world, x, y)
    unreal.SystemLibrary.execute_console_command(world, 'anastasis.Village.CanopyRain 1')
    on_again = debug.get_rain_canopy_cover(world, x, y)
    okay = on > 0.5 and off == 0.0 and on_again == on and open_cover == 0.0
    finish(okay, 'tree=(%.3f,%.3f) radius_tiles=%.3f on=%.3f off=%.3f on_again=%.3f open=%.3f' %
           (x, y, point.z, on, off, on_again, open_cover))


handle = unreal.register_slate_post_tick_callback(tick)
