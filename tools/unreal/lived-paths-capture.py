"""Actual sampled traffic -> same-history off/on/off images -> no inhabitants -> recovery.
Register: lived-paths-capture. PASS is instrumental, never an artistic verdict.
No saved assets. Read screenshots and compare off/off before interpreting on/off.
"""
import json
import os
from pathlib import Path
import shutil
import time
import unreal

root = Path(unreal.Paths.project_dir())
out = Path(os.environ.get('ANASTASIS_PATHS_OUT', str(root / 'Saved/LivedPathsEvidence')))
out.mkdir(parents=True, exist_ok=True)
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
les.load_level('/Game/Anastasis/Maps/Lvl_AnastasisSlice')
camera = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).spawn_actor_from_class(
    unreal.CameraActor, unreal.Vector(0, 0, 1000), unreal.Rotator())
camera.set_actor_label('LivedPathsProofCamera')
start = time.monotonic()
state = dict(phase=0, since=start, reports=[], shots=[])
handle = None


def cmd(world, text):
    unreal.SystemLibrary.execute_console_command(world, text)


def finish(ok, reason):
    cmd(None, 'anastasis.Anthropic.Memory 0')
    cmd(None, 'anastasis.Anthropic.Draw 1')
    cmd(None, 'anastasis.Sim.TimeScale 0.0375')
    cmd(None, 'anastasis.Sky.Hour -1')
    (out / 'route.json').write_text(json.dumps(dict(ok=ok, reason=reason, state=state), indent=2), encoding='utf-8')
    unreal.log('LIVED_PATHS_CAPTURE %s %s' % ('PASS' if ok else 'FAIL', reason))
    unreal.unregister_slate_post_tick_callback(handle)
    unreal.SystemLibrary.quit_editor()


def screenshots():
    return set((root / 'Saved/Screenshots').rglob('*.png'))


def tick(_dt):
    now = time.monotonic()
    try:
        if now - start > 360:
            finish(False, 'timeout: no qualifying real route or missing screenshot')
            return
        if state['phase'] == 0 and now - start > 3:
            for text in ('anastasis.Sim.Speed 1', 'anastasis.Sim.Warp 1',
                         'anastasis.Sim.TimeScale 0.0375', 'anastasis.Anthropic.Memory 1',
                         'anastasis.Anthropic.Draw 1', 'anastasis.Sky.Hour 11'):
                cmd(None, text)
            les.editor_request_begin_play()
            state['phase'] = 1
            return
        world = ues.get_game_world()
        if not world or not les.is_in_play_in_editor():
            return
        if state['phase'] == 1:
            cmd(world, 'Anastasis.Village.FirstHouse 0')
            cmd(world, 'Anastasis.Village.FirstWell 12')
            state.update(phase=2, since=now)
            return
        if now - state.get('sample_at', 0) < .5:
            return
        state['sample_at'] = now
        report = unreal.AnastasisAnthropicDebugLibrary.get_status(world)
        data = {k: float(v) for k, v in (piece.split('=', 1) for piece in report.split())}
        if data['grass_cap'] or data['restore_errors'] or data['dropped']:
            finish(False, 'truncated route or restoration failure: ' + report)
            return
        if state['phase'] == 2 and data['grass'] > 0 and data['peak'] >= .35:
            cmd(world, 'anastasis.Sim.TimeScale 0')
            state['used'] = data
            state['reports'].append(data)
            cam = next(c for c in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.CameraActor)
                       if c.get_actor_label() == 'LivedPathsProofCamera')
            target = unreal.Vector(data['view_x'], data['view_y'], data['view_z'])
            eye = target + unreal.Vector(-300, 0, 170)
            cam.set_actor_location(eye, False, True)
            cam.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(eye, target), True)
            unreal.GameplayStatics.get_player_controller(world, 0).set_view_target_with_blend(cam, 0)
            state.update(phase=3, since=now, capture_index=0, fired=False)
            cmd(world, 'anastasis.Anthropic.Draw 0')
        elif state['phase'] == 3:
            names = ('reference', 'used', 'reference2', 'abandoned')
            if not state['fired'] and now - state['since'] > 3:
                index = state['capture_index']
                if index < 3 and abs(data['peak'] - state['used']['peak']) > 1e-5:
                    finish(False, 'A/B changed traffic history')
                    return
                if index in (0, 2) and data['grass'] != 0:
                    finish(False, 'display off failed to restore')
                    return
                if index == 1 and data['grass'] <= 0:
                    finish(False, 'display on failed to restore wear')
                    return
                if index == 3 and not (data['people'] == 0 and data['peak'] < state['used']['peak']):
                    finish(False, 'abandonment not established')
                    return
                state['reports'].append(data)
                state['before'] = [str(p) for p in screenshots()]
                cmd(world, 'Shot')
                state.update(fired=True, since=now)
            elif state['fired']:
                fresh = screenshots() - {Path(p) for p in state['before']}
                if fresh and now - state['since'] > 1:
                    name = names[state['capture_index']] + '.png'
                    shutil.copy2(max(fresh, key=lambda p: p.stat().st_mtime), out / name)
                    state['shots'].append(name)
                    state['capture_index'] += 1
                    if state['capture_index'] == 4:
                        finish(True, 'same-history A/B/A and recovery; visual quality and GPU UNKNOWN')
                        return
                    if state['capture_index'] == 3:
                        # This disposable scenario contains only our twelve inhabitants. Verify removal count below.
                        for i in range(128):
                            npc = 'npc-%d' % i
                            if unreal.AnastasisSimulationDebugLibrary.get_npc_state(world, npc):
                                cmd(world, 'Anastasis.Village.RemoveNpc ' + npc)
                        cmd(world, 'Anastasis.Sim.Advance 16d')
                    cmd(world, 'anastasis.Anthropic.Draw %d' % (state['capture_index'] % 2))
                    state.update(fired=False, since=now)
    except Exception as exc:
        finish(False, repr(exc))


handle = unreal.register_slate_post_tick_callback(tick)
