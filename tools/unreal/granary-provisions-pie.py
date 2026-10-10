"""A real farmer delivery makes provision crates appear at the real granary.

Freeze the authoritative stock and capture same-camera 0/1/0 crate-only images.
"""
import json
import os
import time
from pathlib import Path

import unreal

root = Path(unreal.Paths.project_dir())
out = Path(os.environ.get('ANASTASIS_GRANARY_PROVISIONS_OUT',
                          str(root / 'Saved' / 'GranaryProvisionsEvidence' / 'pie')))
out.mkdir(parents=True, exist_ok=True)
screens = root / 'Saved' / 'Screenshots'
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
dbg = unreal.AnastasisSimulationDebugLibrary
les.load_level('/Game/Anastasis/Maps/Lvl_AnastasisSlice')
cam = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).spawn_actor_from_class(
    unreal.CameraActor, unreal.Vector(0, 0, 1000), unreal.Rotator())
cam.set_actor_label('GranaryProvisionsProofCamera')

started = time.monotonic()
phase, phase_at = 'start', started
pending = None
handle = None
history, shots, failures = [], [], []


def log(message):
    unreal.log('GRANARY_PROVISIONS_PIE ' + message)


def advance(name):
    global phase, phase_at
    phase, phase_at = name, time.monotonic()
    log('PHASE ' + name)


def finish(ok, reason):
    global handle
    ok = ok and not failures
    report = dict(pass_=ok, reason=reason, history=history, shots=shots, failures=failures)
    report['pass'] = report.pop('pass_')
    (out / 'granary-provisions-pie.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    log(('PASS ' if ok else 'FAIL ') + reason + (' ' + json.dumps(failures) if failures else ''))
    if handle is not None:
        unreal.unregister_slate_post_tick_callback(handle)
        handle = None
    unreal.SystemLibrary.quit_editor()


def sample(world):
    row = json.loads(dbg.get_food_supply_status(world) or '{}')
    actors = [a for a in unreal.GameplayStatics.get_all_actors_of_class(
        world, unreal.AnastasisVillageBuilding)
        if a.get_actor_label().startswith('SimBuilding_granary_')]
    if len(actors) != 1:
        raise AssertionError('one granary actor expected, got %d' % len(actors))
    actor = actors[0]
    comps = actor.get_components_by_class(unreal.InstancedStaticMeshComponent)
    comp = next((c for c in comps if c.get_name().startswith('ProvisionVisual')), None)
    if comp is None:
        raise AssertionError('ProvisionVisual component missing')
    mesh = comp.get_editor_property('static_mesh')
    if not mesh or mesh.get_name() != 'SM_Granary_ProvisionCrate_01':
        raise AssertionError('provision crate static mesh missing')
    if comp.get_collision_enabled() != unreal.CollisionEnabled.NO_COLLISION:
        raise AssertionError('provision crates have collision')
    count = int(comp.get_instance_count())
    stock = int(row.get('stock', -1))
    expected = min(8, (8 * stock + 299) // 300) if stock > 0 else 0
    return dict(time=row.get('time'), stock=stock, delivered=row.get('delivered'),
                gathered=row.get('gathered'), bag=row.get('bag'), count=count,
                expected=expected), actor


def frame(world, actor):
    camera = next((c for c in unreal.GameplayStatics.get_all_actors_of_class(
        world, unreal.CameraActor)
        if c.get_actor_label() == 'GranaryProvisionsProofCamera'), None)
    if camera is None:
        raise AssertionError('PIE proof camera missing')
    transform = actor.get_actor_transform()
    position = transform.transform_location(unreal.Vector(-770, 970, 210))
    target = transform.transform_location(unreal.Vector(-760, 490, 65))
    camera.set_actor_location(position, False, True)
    camera.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(position, target), True)
    pc = unreal.GameplayStatics.get_player_controller(world, 0)
    if pc:
        pc.set_view_target_with_blend(camera, 0.0)


def files():
    return set(screens.rglob('*.png')) if screens.exists() else set()


def capture(name):
    global pending
    path = out / (name + '.png')
    if path.exists():
        path.unlink()
    pending = dict(name=name, path=path, before=files(), at=time.monotonic(), fired=False)
    log('SHOT ' + name)


def pump_capture(world):
    global pending
    now = time.monotonic()
    if not pending['fired']:
        if now - pending['at'] < 0.8:
            return False
        unreal.SystemLibrary.execute_console_command(world, 'Shot')
        pending['fired'], pending['at'] = True, now
        return False
    fresh = sorted(files() - pending['before'], key=lambda f: f.stat().st_mtime)
    if fresh and now - pending['at'] > .3:
        try:
            fresh[-1].replace(pending['path'])
        except OSError:
            return False
    if pending['path'].is_file():
        shots.append(pending['path'].name)
        pending = None
        return True
    if now - pending['at'] > 25:
        raise AssertionError('screenshot missing: ' + pending['name'])
    return False


def tick(_dt):
    try:
        now = time.monotonic()
        if now - started > 420:
            raise AssertionError('timeout phase=' + phase)
        if phase == 'start':
            if now - started > 3:
                les.editor_request_begin_play()
                advance('pie')
            return
        world = ues.get_game_world()
        if not world or not les.is_in_play_in_editor() or dbg.get_simulation_time(world) < 0:
            return
        if phase == 'pie':
            unreal.SystemLibrary.execute_console_command(world, 'anastasis.Sim.TimeScale 0')
            unreal.SystemLibrary.execute_console_command(world, 'Anastasis.Village.FirstFarmer 1')
            unreal.SystemLibrary.execute_console_command(world, 'anastasis.Village.Debug 0')
            unreal.SystemLibrary.execute_console_command(world, 'anastasis.Village.GranaryProvisions 1')
            advance('empty')
            return
        if pending is not None:
            if pump_capture(world):
                if phase == 'warmup':
                    (out / shots.pop()).unlink(missing_ok=True)
                advance({'warmup': 'off', 'off': 'on', 'on': 'control', 'control': 'done'}[phase])
            return
        if now - phase_at < .6:
            return
        rec, actor = sample(world)
        if not history or now - started - history[-1]['wall'] > 1 or phase in ('empty', 'freeze', 'done'):
            history.append(dict(rec, wall=now - started, phase=phase))
        if phase == 'empty':
            if rec['stock'] != 0 or rec['count'] != 0:
                raise AssertionError('empty granary showed crates: ' + json.dumps(rec))
            unreal.SystemLibrary.execute_console_command(world, 'anastasis.Sim.TimeScale 1')
            unreal.SystemLibrary.execute_console_command(world, 'anastasis.Sim.Speed 10')
            advance('deliver')
        elif phase == 'deliver':
            if rec['stock'] > 0 and rec['delivered'] > 0:
                if rec['count'] != rec['expected']:
                    raise AssertionError('delivery did not project to crates: ' + json.dumps(rec))
                unreal.SystemLibrary.execute_console_command(world, 'anastasis.Sim.TimeScale 0')
                frame(world, actor)
                advance('freeze')
        elif phase == 'freeze':
            if rec['count'] != rec['expected'] or rec['stock'] <= 0:
                raise AssertionError('stock changed during freeze: ' + json.dumps(rec))
            unreal.SystemLibrary.execute_console_command(world, 'anastasis.Village.GranaryProvisions 0')
            advance('warmup')
            capture('00-warmup')
        elif phase == 'off':
            if rec['count'] != 0:
                raise AssertionError('off left crates: ' + json.dumps(rec))
            capture('01-crates-off')
            advance('off')
        elif phase == 'on':
            if rec['count'] != 0:
                raise AssertionError('off snapshot changed: ' + json.dumps(rec))
            unreal.SystemLibrary.execute_console_command(world, 'anastasis.Village.GranaryProvisions 1')
            capture('02-crates-on')
            advance('on')
        elif phase == 'control':
            if rec['count'] != rec['expected']:
                raise AssertionError('on count wrong: ' + json.dumps(rec))
            unreal.SystemLibrary.execute_console_command(world, 'anastasis.Village.GranaryProvisions 0')
            capture('03-crates-off-control')
            advance('control')
        elif phase == 'done':
            if rec['count'] != 0 or len(shots) != 3:
                raise AssertionError('off control failed: ' + json.dumps(rec))
            finish(True, 'empty > farmer delivery > physical stock > crate; A/B/A captured')
    except BaseException as exc:
        failures.append(str(exc))
        finish(False, 'exception')


handle = unreal.register_slate_post_tick_callback(tick)
