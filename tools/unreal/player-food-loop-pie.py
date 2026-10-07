"""PIE proof: a human intention moves finite food from field to depot to meal.

Uses the same console commands bound to F6-F9 in PIE. This proves the causal
runtime path; a real keyboard and visual play session remains a separate check.
No asset is saved. ANASTASIS_PLAYER_FOOD_OUT selects the evidence directory.
"""

import json
import os
import time
from pathlib import Path

import unreal


OUT = Path(os.environ.get('ANASTASIS_PLAYER_FOOD_OUT', 'Saved/PlayerFoodEvidence/pie'))
OUT.mkdir(parents=True, exist_ok=True)
LES = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
UES = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
DBG = unreal.AnastasisSimulationDebugLibrary
LES.load_level('/Game/Anastasis/Maps/Lvl_AnastasisSlice')

phase = 'open'
started_wall = time.monotonic()
phase_time = 0.0
initial = None
samples = []
checks = []
pickup_position = None
deposit_stock = None
meal_hunger_before = None
handle = None
finished = False


def command(world, value):
    unreal.SystemLibrary.execute_console_command(world, value)


def check(name, ok, detail=''):
    checks.append([name, bool(ok), detail])
    unreal.log('PLAYER_FOOD_LOOP CHECK %s %s %s' % ('OK' if ok else 'FAIL', name, detail))
    return bool(ok)


def finish(reason):
    global finished
    if finished:
        return
    finished = True
    passed = bool(checks) and all(item[1] for item in checks) and reason == 'complete'
    (OUT / 'player-food-loop.json').write_text(json.dumps({
        'pass': passed, 'reason': reason, 'checks': checks, 'samples': samples,
    }, indent=2), encoding='utf-8')
    unreal.log('PLAYER_FOOD_LOOP %s %s checks=%d failed=%d' % (
        'PASS' if passed else 'FAIL', reason, len(checks), sum(not item[1] for item in checks)))
    unreal.unregister_slate_post_tick_callback(handle)
    unreal.SystemLibrary.quit_editor()


def tick(_dt):
    global phase, phase_time, initial, pickup_position, deposit_stock, meal_hunger_before
    if time.monotonic() - started_wall > 300:
        finish('wall timeout in ' + phase)
        return
    if phase == 'open':
        if time.monotonic() - started_wall < 3:
            return
        LES.editor_request_begin_play()
        phase = 'begin'
        return
    if not LES.is_in_play_in_editor():
        return
    world = UES.get_game_world()
    if not world:
        return
    t = DBG.get_simulation_time(world)
    if t < 0:
        return
    if phase == 'begin':
        command(world, 'anastasis.Sim.TimeScale 1')
        command(world, 'Anastasis.Player.FoodLoop')
        phase, phase_time = 'setup', t
        return

    player = json.loads(DBG.get_player_status(world) or '{}')
    food = json.loads(DBG.get_food_supply_status(world) or '{}')
    if phase == 'setup':
        if t - phase_time < 1:
            return
        initial = food.get('initial', 0)
        check('incarnated inhabitant', bool(player.get('player')), str(player.get('player')))
        check('finite source', initial > 0, str(initial))
        check('empty granary', food.get('stock') == 0, str(food.get('stock')))
        check('one starting inhabitant', player.get('npcs') == 1, str(player.get('npcs')))
        if not all(item[1] for item in checks):
            finish('scenario setup')
            return
        command(world, 'Anastasis.Player.Goal gatherFood')
        phase, phase_time = 'gather', t

    if initial is not None:
        total = food.get('remaining', 0) + food.get('bag', 0) + food.get('stock', 0) + food.get('meals', 0)
        if total != initial:
            check('food conservation', False, '%d != %d at t=%.2f' % (total, initial, t))
            finish('conservation')
            return
    if not samples or t - samples[-1]['time'] >= 2 or phase != samples[-1]['phase']:
        samples.append({'phase': phase, 'time': t, 'player': player, 'food': food})

    if phase == 'gather':
        if player.get('bag', 0) > 0:
            pickup_position = (player['x'], player['y'])
            check('player gathered generated food', player.get('gathered', 0) > 0 and food['remaining'] < initial,
                  'bag=%s remaining=%s' % (player.get('bag'), food['remaining']))
            phase, phase_time = 'hold_bag', t
        elif t - phase_time > 90:
            check('player gathered generated food', False, str(player))
            finish('gather timeout')
    elif phase == 'hold_bag' and t - phase_time > 2:
        check('delivery waits for player choice', player.get('delivered', 0) == 0 and player.get('bag', 0) > 0,
              'bag=%s delivered=%s' % (player.get('bag'), player.get('delivered')))
        command(world, 'Anastasis.Player.Goal deliver')
        phase, phase_time = 'deliver', t
    elif phase == 'deliver':
        if player.get('delivered', 0) > 0:
            distance = ((player['x'] - pickup_position[0]) ** 2 + (player['y'] - pickup_position[1]) ** 2) ** 0.5
            check('carried food to depot', distance > 0.75 and player.get('bag') == 0 and food.get('stock', 0) > 0,
                  'distance=%.2f stock=%s' % (distance, food.get('stock')))
            deposit_stock = food['stock']
            phase, phase_time = 'hold_stock', t
        elif t - phase_time > 90:
            check('carried food to depot', False, str(player))
            finish('deliver timeout')
    elif phase == 'hold_stock' and t - phase_time > 2:
        check('meal waits for player choice', player.get('meals', 0) == 0 and food.get('stock') == deposit_stock,
              'meals=%s stock=%s' % (player.get('meals'), food.get('stock')))
        meal_hunger_before = player.get('hunger', 0)
        command(world, 'Anastasis.Player.Goal eat')
        phase, phase_time = 'eat', t
    elif phase == 'eat':
        if player.get('meals', 0) > 0:
            check('player ate depot portion', food.get('stock', 0) < deposit_stock,
                  'stock %s -> %s' % (deposit_stock, food.get('stock')))
            check('meal reduced hunger', player.get('hunger', 100) < meal_hunger_before,
                  'hunger %.2f -> %.2f' % (meal_hunger_before, player.get('hunger', -1)))
            finish('complete')
        elif t - phase_time > 90:
            check('player ate depot portion', False, str(player))
            finish('meal timeout')


def guarded_tick(dt):
    try:
        tick(dt)
    except Exception as exc:
        import traceback
        unreal.log_error(traceback.format_exc())
        check('no python exception', False, str(exc))
        finish('python exception')


handle = unreal.register_slate_post_tick_callback(guarded_tick)
