"""A farmer's real food inventory drives a 3D hand basket in PIE.

One farmer gathers and delivers through the canonical simulation. At positive bag stock,
freeze simulation and capture a same-camera 0/1/0 basket-only A/B/A. Instrumental PASS
does not judge the image; inspect and measure the three PNGs separately.
"""
import json
import math
import os
import time
from pathlib import Path

import unreal

ROOT = Path(unreal.Paths.project_dir())
OUT = Path(os.environ.get('ANASTASIS_FOOD_BASKET_OUT', str(ROOT / 'Saved' / 'FoodBasketEvidence' / 'pie')))
OUT.mkdir(parents=True, exist_ok=True)
SHOTS = ROOT / 'Saved' / 'Screenshots'
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
dbg = unreal.AnastasisSimulationDebugLibrary
les.load_level('/Game/Anastasis/Maps/Lvl_AnastasisSlice')
camera = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).spawn_actor_from_class(
    unreal.CameraActor, unreal.Vector(0, 0, 1000), unreal.Rotator())
camera.set_actor_label('FoodBasketProofCamera')

started = time.monotonic()
phase = 'start'
phase_at = started
handle = None
pending = None
shots = []
history = []
failures = []
farmer_id = None


def log(message):
    unreal.log('FOOD_BASKET_PIE ' + message)


def advance(name):
    global phase, phase_at
    phase, phase_at = name, time.monotonic()
    log('PHASE ' + name)


def finish(ok, reason):
    global handle
    ok = ok and not failures
    report = {'pass': ok, 'reason': reason, 'history': history, 'shots': shots, 'failures': failures}
    (OUT / 'food-basket-pie.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    log(('PASS ' if ok else 'FAIL ') + reason + (' ' + json.dumps(failures) if failures else ''))
    if handle is not None:
        unreal.unregister_slate_post_tick_callback(handle)
        handle = None
    unreal.SystemLibrary.quit_editor()


def sample(world):
    row = json.loads(dbg.get_food_supply_status(world) or '{}')
    cards = json.loads(dbg.get_villager_cards(world) or '{}')
    farmers = [c for c in cards.get('villagers', []) if c.get('job') == 'farmer']
    if len(farmers) != 1:
        raise AssertionError('one farmer expected, got %s' % farmers)
    npc = farmers[0]
    actor = next((a for a in unreal.GameplayStatics.get_all_actors_of_class(
        world, unreal.AnastasisVillagerVisual)
        if a.get_actor_label().startswith('Villager_' + npc['npc'] + '_')), None)
    if actor is None:
        raise AssertionError('farmer visual missing')
    comps = actor.get_components_by_class(unreal.StaticMeshComponent)
    basket = next((c for c in comps if c.get_name().startswith('FoodBasket')), None)
    if basket is None:
        raise AssertionError('FoodBasket component missing')
    mesh = basket.get_editor_property('static_mesh')
    if not mesh or mesh.get_name() != 'SM_Food_Basket_01':
        raise AssertionError('real basket mesh missing')
    if basket.get_collision_enabled() != unreal.CollisionEnabled.NO_COLLISION:
        raise AssertionError('basket affects collision')
    bag = int(row.get('bag', -1))
    carried = int(actor.get_carried_food())
    visible = bool(actor.is_food_basket_visible())
    if bag != carried:
        raise AssertionError('basket owner does not mirror InventoryFood: %d != %d' % (bag, carried))
    rec = {'time': row.get('time'), 'bag': bag, 'carried': carried,
           'basket': visible, 'stock': row.get('stock'), 'gathered': row.get('gathered'),
           'delivered': row.get('delivered'), 'npc': npc['npc']}
    if visible:
        body = actor.get_component_by_class(unreal.SkeletalMeshComponent)
        hand = body.get_socket_location('hand_l')
        base = basket.get_world_location()
        dxy = math.hypot(hand.x - base.x, hand.y - base.y)
        dz = hand.z - base.z
        rec.update(hand_xy_cm=round(dxy, 1), hand_above_base_cm=round(dz, 1))
        if dxy > 30 or not 20 < dz < 70:
            raise AssertionError('basket detached from hand: ' + json.dumps(rec))
    return rec, actor


def files():
    return set(SHOTS.rglob('*.png')) if SHOTS.exists() else set()


def frame(world, actor):
    cam = next((c for c in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.CameraActor)
                if c.get_actor_label() == 'FoodBasketProofCamera'), None)
    if not cam:
        raise AssertionError('proof camera missing in PIE')
    p = actor.get_actor_location()
    position = unreal.Vector(p.x + 155, p.y + 120, p.z + 140)
    target = unreal.Vector(p.x, p.y, p.z + 95)
    cam.set_actor_location(position, False, True)
    cam.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(position, target), True)
    pc = unreal.GameplayStatics.get_player_controller(world, 0)
    if pc and pc.get_view_target() != cam:
        pc.set_view_target_with_blend(cam, 0.0)


def capture(world, name):
    global pending
    path = OUT / (name + '.png')
    if path.exists():
        path.unlink()
    pending = {'name': name, 'path': path, 'before': files(), 'at': time.monotonic(), 'fired': False}
    log('SHOT ' + name)


def pump_capture(world):
    global pending
    now = time.monotonic()
    if not pending['fired']:
        if now - pending['at'] < 0.7:
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
    if now - pending['at'] > 20:
        raise AssertionError('screenshot missing: ' + pending['name'])
    return False


def tick(_dt):
    global farmer_id
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
            unreal.SystemLibrary.execute_console_command(world, 'anastasis.Village.Bodies 2')
            unreal.SystemLibrary.execute_console_command(world, 'anastasis.Village.Debug 0')
            unreal.SystemLibrary.execute_console_command(world, 'anastasis.Village.FoodBasket 1')
            advance('empty')
            return
        if pending is not None:
            if pump_capture(world):
                if phase == 'warmup':
                    (OUT / shots.pop()).unlink(missing_ok=True)
                    advance('off1')
                    capture(world, '01-basket-off')
                else:
                    advance({'off1': 'on', 'on': 'off2', 'off2': 'resume'}[phase])
            return
        if now - phase_at < .6:
            return
        rec, actor = sample(world)
        if not history or now - started - history[-1]['wall'] > .5 or phase in ('empty', 'freeze', 'resume'):
            history.append(dict(rec, wall=now - started, phase=phase))
        if phase == 'empty':
            if rec['bag'] != 0 or rec['basket']:
                raise AssertionError('empty farmer has basket: ' + json.dumps(rec))
            farmer_id = rec['npc']
            unreal.SystemLibrary.execute_console_command(world, 'anastasis.Sim.TimeScale 1')
            unreal.SystemLibrary.execute_console_command(world, 'anastasis.Sim.Speed 10')
            advance('gather')
        elif phase == 'gather':
            if rec['npc'] != farmer_id:
                raise AssertionError('farmer identity changed')
            if rec['bag'] > 0:
                if not rec['basket'] or rec['gathered'] < rec['bag']:
                    raise AssertionError('gathered food without visible basket: ' + json.dumps(rec))
                unreal.SystemLibrary.execute_console_command(world, 'anastasis.Sim.TimeScale 0')
                frame(world, actor)
                advance('freeze')
        elif phase == 'freeze':
            if rec['bag'] <= 0 or not rec['basket']:
                raise AssertionError('bag changed during freeze: ' + json.dumps(rec))
            unreal.SystemLibrary.execute_console_command(world, 'anastasis.Village.FoodBasket 0')
            advance('warmup')
            capture(world, '00-viewport-warmup')
        elif phase == 'on':
            if rec['bag'] <= 0:
                raise AssertionError('bag lost during A/B')
            if rec['basket']:
                raise AssertionError('FoodBasket 0 did not hide the basket')
            unreal.SystemLibrary.execute_console_command(world, 'anastasis.Village.FoodBasket 1')
            advance('on')
            capture(world, '02-basket-on')
        elif phase == 'off2':
            if not rec['basket']:
                raise AssertionError('FoodBasket 1 did not show the basket')
            unreal.SystemLibrary.execute_console_command(world, 'anastasis.Village.FoodBasket 0')
            advance('off2')
            capture(world, '03-basket-off-control')
        elif phase == 'resume':
            if rec['bag'] <= 0:
                raise AssertionError('bag lost during A/B')
            if rec['basket']:
                raise AssertionError('FoodBasket 0 control did not hide the basket')
            unreal.SystemLibrary.execute_console_command(world, 'anastasis.Village.FoodBasket 1')
            unreal.SystemLibrary.execute_console_command(world, 'anastasis.Sim.TimeScale 1')
            advance('deliver')
        elif phase == 'deliver':
            if rec['bag'] == 0 and rec['stock'] > 0 and rec['delivered'] > 0:
                if rec['basket']:
                    raise AssertionError('empty basket still visible after delivery')
                if len(shots) != 3:
                    raise AssertionError('A/B/A captures incomplete')
                finish(True, 'real gather > basket visible > real delivery > basket hidden; A/B/A captured')
    except BaseException as exc:
        failures.append(str(exc))
        finish(False, 'exception')


handle = unreal.register_slate_post_tick_callback(tick)
