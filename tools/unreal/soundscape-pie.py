"""Causal soundscape integration: delivered work, frozen silence, hard mute.
Run through editor-batch -Proofs soundscape-pie. No asset saved.
Counters prove dispatch, not audibility or artistic quality. PCM tests run in Anastasis suite.
"""
import json
import time
import unreal

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
les.load_level('/Game/Anastasis/Maps/Lvl_AnastasisSlice')
camera = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).spawn_actor_from_class(
    unreal.CameraActor, unreal.Vector(0, 0, 1000), unreal.Rotator())
camera.set_actor_label('SoundscapeProofCamera')
start = time.monotonic()
state = {'phase': 0, 'since': start}
handle = None


def command(world, text):
    unreal.SystemLibrary.execute_console_command(world, text)


def finish(ok, reason):
    world = ues.get_game_world()
    command(world, 'anastasis.Soundscape.Enabled 1')
    command(world, 'anastasis.Sim.TimeScale 1')
    unreal.log('SOUNDSCAPE_PIE %s %s' % ('PASS' if ok else 'FAIL', reason))
    unreal.unregister_slate_post_tick_callback(handle)
    unreal.SystemLibrary.quit_editor()


def tick(_dt):
    now = time.monotonic()
    if now - start > 180:
        finish(False, 'timeout: ' + str(state))
        return
    if state['phase'] == 0 and now - start > 3:
        les.editor_request_begin_play()
        state['phase'] = 1
        return
    world = ues.get_game_world()
    if not world or not les.is_in_play_in_editor():
        return
    dbg = unreal.AnastasisSimulationDebugLibrary
    if state['phase'] == 1:
        command(world, 'anastasis.Soundscape.Enabled 1')
        command(world, 'anastasis.Soundscape.Volume 0.6')
        command(world, 'anastasis.Sim.TimeScale 1')
        command(world, 'anastasis.Sim.Warp 1')
        command(world, 'Anastasis.Village.FirstSite house 2 1')
        site = json.loads(dbg.get_build_status(world))
        if not site.get('site'):
            finish(False, 'no delivered site')
            return
        cam = next(c for c in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.CameraActor)
                   if c.get_actor_label() == 'SoundscapeProofCamera')
        cam.set_actor_location(unreal.Vector(site['sx'] + 350, site['sy'], site['sz'] + 170), False, True)
        unreal.GameplayStatics.get_player_controller(world, 0).set_view_target_with_blend(cam, 0)
        state['phase'] = 2
        state['since'] = now
        return
    status = json.loads(unreal.AnastasisSoundscapeDebugLibrary.get_soundscape_status(world))
    if status.get('voices', 99) > 12 or status.get('water_bytes', 999999) > 36000:
        finish(False, 'voice/buffer budget ' + str(status))
        return
    if state['phase'] == 2 and status.get('work', 0) >= 3:
        unreal.log('SOUNDSCAPE_WORK ' + json.dumps(status))
        command(world, 'anastasis.Sim.TimeScale 0')
        state.update(phase=3, since=now)
    elif state['phase'] == 3 and now - state['since'] > 2:
        state.update(phase=4, since=now, frozen=status)
    elif state['phase'] == 4 and now - state['since'] > 3:
        if any(status[k] != state['frozen'][k] for k in ('steps', 'work')):
            finish(False, 'stationary village emitted new gestures')
            return
        command(world, 'anastasis.Soundscape.Enabled 0')
        state.update(phase=5, since=now)
    elif state['phase'] == 5 and now - state['since'] > 1:
        finish(status['voices'] == 0 and not status['water'] and status['tracks'] == 0,
               'work observed, stationary silence, mute retirement; listening UNKNOWN ' + json.dumps(status))


handle = unreal.register_slate_post_tick_callback(tick)
