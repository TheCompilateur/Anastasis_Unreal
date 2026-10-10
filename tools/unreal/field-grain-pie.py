"""Live grain-field witness: A/B/A visibility and an autonomous harvest in PIE.

The three shots use one fixed camera and a frozen simulation. Only GrainFields changes.
The later harvest must reduce the target plot's clumps and move food into a farmer's bag.
"""
import json
import os
import time
from pathlib import Path

import unreal

root = Path(unreal.Paths.project_dir())
out = Path(os.environ.get('ANASTASIS_FIELD_GRAIN_OUT', str(root / 'Saved' / 'FieldGrainEvidence' / 'pie')))
out.mkdir(parents=True, exist_ok=True)
screens = root / 'Saved' / 'Screenshots'
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
dbg = unreal.AnastasisSimulationDebugLibrary
les.load_level('/Game/Anastasis/Maps/Lvl_AnastasisSlice')
camera = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).spawn_actor_from_class(
    unreal.CameraActor, unreal.Vector(0, 0, 1000), unreal.Rotator())
camera.set_actor_label('FieldGrainProofCamera')

started = time.monotonic()
phase = 'start'
phase_at = started
pending = None
handle = None
field = None
initial = None
initial_food = None
shots = []
samples = []


def log(msg):
    unreal.log('FIELD_GRAIN_PIE ' + msg)


def advance(name):
    global phase, phase_at
    phase, phase_at = name, time.monotonic()
    log('PHASE ' + name)


def finish(ok, reason):
    global handle
    (out / 'field-grain-pie.json').write_text(json.dumps({
        'pass': ok, 'reason': reason, 'field': field, 'initial_clumps': initial,
        'initial_field_food': initial_food, 'samples': samples, 'shots': shots,
    }, indent=2), encoding='utf-8')
    log(('PASS ' if ok else 'FAIL ') + reason)
    if handle is not None:
        unreal.unregister_slate_post_tick_callback(handle)
        handle = None
    unreal.SystemLibrary.quit_editor()


def actor(world):
    found = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.AnastasisFieldGrainVisual)
    assert len(found) == 1, 'expected one grain presenter, found %d' % len(found)
    return found[0]


def shot_files():
    return set(screens.rglob('*.png')) if screens.exists() else set()


def shoot(name):
    global pending
    path = out / (name + '.png')
    path.unlink(missing_ok=True)
    pending = {'path': path, 'before': shot_files(), 'at': time.monotonic(), 'fired': False}
    log('SHOT ' + name)


def capture(world):
    global pending
    now = time.monotonic()
    if not pending['fired']:
        if now - pending['at'] < .8:
            return False
        unreal.SystemLibrary.execute_console_command(world, 'Shot')
        pending['fired'], pending['at'] = True, now
        return False
    fresh = sorted(shot_files() - pending['before'], key=lambda p: p.stat().st_mtime)
    if fresh and now - pending['at'] > .3:
        try:
            fresh[-1].replace(pending['path'])
        except OSError:
            return False
    if pending['path'].exists():
        shots.append(pending['path'].name)
        pending = None
        return True
    assert now - pending['at'] < 25, 'capture missing'
    return False


def tick(_dt):
    global field, initial, initial_food
    try:
        now = time.monotonic()
        assert now - started < 480, 'wall timeout at ' + phase
        if phase == 'start':
            if now - started > 3:
                les.editor_request_begin_play()
                advance('pie')
            return
        world = ues.get_game_world()
        if not world or not les.is_in_play_in_editor() or dbg.get_simulation_time(world) < 0:
            return
        grain = actor(world)
        if phase == 'pie':
            unreal.SystemLibrary.execute_console_command(world, 'anastasis.Sim.TimeScale 0')
            unreal.SystemLibrary.execute_console_command(world, 'anastasis.Village.Debug 0')
            unreal.SystemLibrary.execute_console_command(world, 'anastasis.Village.GrainFields.InAutomation 1')
            unreal.SystemLibrary.execute_console_command(world, 'anastasis.Village.GrainFields 1')
            advance('ready')
            return
        if phase == 'ready':
            if grain.get_total_clumps() == 0 or grain.get_pending_chunk_count() > 0:
                return
            assert grain.get_total_clumps() == grain.get_rendered_clumps(), 'HISM != live grain stock'
            best = grain.get_fullest_field()
            assert best.z >= 8, 'no visible stocked grain field'
            field = [int(best.x), int(best.y)]
            initial = grain.get_tile_clumps(*field)
            unreal.SystemLibrary.execute_console_command(world,
                'Anastasis.Village.FirstFarmer 1 %d %d' % tuple(field))
            advance('farmer')
            return
        status = json.loads(dbg.get_gather_status(world) or '{}')
        if phase == 'farmer':
            if status.get('granary', -1) < 0:
                return
            initial_food = int(status['field'])
            cams = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.CameraActor)
            cam = next((c for c in cams if c.get_actor_label() == 'FieldGrainProofCamera'), None)
            assert cam, 'PIE proof camera absent'
            target = unreal.Vector(status['fx'], status['fy'], status['fz'] + 55)
            position = target + unreal.Vector(650, 850, 180)
            cam.set_actor_location(position, False, True)
            cam.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(position, target), True)
            controller = unreal.GameplayStatics.get_player_controller(world, 0)
            assert controller, 'player controller absent'
            controller.set_view_target_with_blend(cam, 0.0)
            advance('settle')
            return
        if pending is not None:
            if capture(world):
                if phase == 'on1':
                    unreal.SystemLibrary.execute_console_command(world, 'anastasis.Village.GrainFields 0')
                    advance('off')
                    shoot('02-grain-off')
                elif phase == 'off':
                    unreal.SystemLibrary.execute_console_command(world, 'anastasis.Village.GrainFields 1')
                    advance('on2')
                    shoot('03-grain-on-control')
                else:
                    assert phase == 'on2'
                    advance('resume')
            return
        if now - phase_at < .8:
            return
        count = grain.get_tile_clumps(*field)
        total = grain.get_total_clumps()
        rendered = grain.get_rendered_clumps()
        rec = {'phase': phase, 'time': status.get('time'), 'tile_clumps': count,
               'total_clumps': total, 'rendered_clumps': rendered,
               'field_food': status.get('field'), 'bag': status.get('bag'),
               'stock': status.get('stock'), 'deliveries': status.get('deliveries')}
        if not samples or phase != samples[-1]['phase'] or now - started - samples[-1]['wall'] > 1:
            rec['wall'] = now - started
            samples.append(rec)
        if phase == 'settle':
            if grain.get_pending_chunk_count() > 0 or total != rendered:
                return
            assert count == initial, 'grain changed during setup'
            advance('on1')
            shoot('01-grain-on')
        elif phase == 'resume':
            assert len(shots) == 3 and count == initial, 'A/B/A altered crop state'
            unreal.SystemLibrary.execute_console_command(world, 'anastasis.Sim.TimeScale 1')
            unreal.SystemLibrary.execute_console_command(world, 'anastasis.Sim.Speed 10')
            advance('harvest')
        elif phase == 'harvest':
            if count < initial and int(status['field']) < initial_food and (status['bag'] > 0 or status['deliveries'] > 0):
                assert total == rendered, 'HISM failed to follow harvest'
                finish(True, 'A/B/A stable; autonomous harvest reduced live grain clumps and moved food')
    except BaseException as exc:
        finish(False, repr(exc))


handle = unreal.register_slate_post_tick_callback(tick)
