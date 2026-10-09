"""PIE: a woodcutter autonomously supplies a dry well site, then another NPC uses it.

The console commands establish initial conditions only. No NPC goal, path, harvest, or
construction progress is forced after setup. Runs through editor-batch.ps1.
"""
import json
import os
import re
import time
from pathlib import Path

import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
OUT = ROOT / 'Saved' / 'LaborSocialPieEvidence'
OUT.mkdir(parents=True, exist_ok=True)
LOG = Path(os.environ['ANASTASIS_EDITOR_BATCH_JOBS']).parent / 'editor-batch.log'
LEVEL = '/Game/Anastasis/Maps/Lvl_AnastasisSlice'
LES = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
UES = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
DBG = unreal.AnastasisSimulationDebugLibrary
if UES.get_editor_world().get_path_name().split('.')[0] != LEVEL:
    LES.load_level(LEVEL)

START = time.monotonic()
WALL_LIMIT = 220.0
SIM_LIMIT = 1800.0
state = {
    'phase': 'start', 'finished': False, 'cutter': '', 'observer': '',
    'samples': [], 'checks': {}, 'saw_gather_goal': False,
    'saw_delivery_goal': False, 'saw_harvest': False, 'saw_delivery': False,
    'saw_delivery_at_site': False, 'saw_motion': False,
    'saw_other_well_use': False, 'wood_actor': None, 'wood_total': None,
    'log_offset': LOG.stat().st_size if LOG.exists() else 0,
    'pending': None, 'last_sample_at': 0.0,
}
handle = None


def cmd(text):
    unreal.SystemLibrary.execute_console_command(UES.get_game_world(), text)


def check(name, value):
    state['checks'][name] = bool(value)
    unreal.log('LABOR_SOCIAL_CHECK %s %s' % ('PASS' if value else 'FAIL', name))


def finish(reason):
    if state['finished']:
        return
    state['finished'] = True
    required = ('initial_site_dry', 'site_at_requested_tile', 'stone_only', 'vital_supply', 'no_courier', 'woodcutter_job',
                'mass_conserved', 'stone_conserved', 'harvest', 'movement', 'delivery_goal',
                'physical_delivery', 'well_completed', 'other_well_use')
    ok = all(state['checks'].get(key, False) for key in required)
    data = {key: value for key, value in state.items()
            if key not in ('pending', 'wood_actor', 'wood_total', 'log_offset')}
    data.update({'pass': ok, 'reason': reason, 'project': str(ROOT),
                 'wall_seconds': round(time.monotonic() - START, 2)})
    (OUT / 'labor-social-pie.json').write_text(json.dumps(data, indent=2), encoding='utf-8')
    unreal.log('LABOR_SOCIAL_PIE %s %s' % ('PASS' if ok else 'FAIL', reason))
    unreal.SystemLibrary.execute_console_command(None, 'anastasis.Sim.Warp 1')
    unreal.unregister_slate_post_tick_callback(handle)
    if LES.is_in_play_in_editor():
        LES.editor_request_end_play()
    unreal.SystemLibrary.quit_editor()


def poll_wood_log():
    if not LOG.exists():
        return
    with LOG.open('rb') as source:
        source.seek(state['log_offset'])
        tail = source.read().decode('utf-8', errors='replace')
        state['log_offset'] = source.tell()
    for line in tail.splitlines():
        if 'ANASTASIS_WOOD npc=' in line:
            match = re.search(r'ANASTASIS_WOOD npc=(\S+) goal=(\S*) carried=(\d+) gathered=(\d+) x=([\d.-]+) y=([\d.-]+)', line)
            if match and match.group(1) == state['cutter']:
                state['wood_actor'] = {'goal': match.group(2), 'carried': int(match.group(3)),
                                       'gathered': int(match.group(4)),
                                       'x': float(match.group(5)), 'y': float(match.group(6))}
        if 'ANASTASIS_WOOD remaining=' in line:
            match = re.search(r'ANASTASIS_WOOD remaining=(\d+) carried=(\d+) gathered=(\d+) total=(\d+)', line)
            if match:
                state['wood_total'] = {'remaining': int(match.group(1)),
                                       'carried': int(match.group(2)),
                                       'gathered': int(match.group(3)), 'total': int(match.group(4))}


def setup(world):
    cmd('Anastasis.Village.FirstSite well 4 0 46 47')
    site = json.loads(DBG.get_build_status(world) or '{}')
    cards = json.loads(DBG.get_villager_cards(world) or '{}').get('villagers', [])
    if not site.get('site') or site.get('type') != 'well' or len(cards) != 4:
        finish('setup_failed site=%s cards=%d' % (site, len(cards)))
        return
    courier = site.get('courier', '')
    remaining = [card['npc'] for card in cards if card['npc'] != courier]
    if not courier or len(remaining) != 3:
        finish('no_distinct_courier site=%s' % site)
        return
    state['cutter'] = remaining[-1]
    state['observer'] = remaining[0]
    state['site_id'] = site['id']
    cmd('Anastasis.Village.RemoveNpc ' + courier)
    cmd('Anastasis.Village.Woodcutter ' + state['cutter'])
    cmd('Anastasis.Village.DeliverSite %s 0 %d' % (site['id'], site['needStone']))
    cmd('Anastasis.Village.FirstWell 0 35 47')
    cmd('Anastasis.Village.FirstGranary 0 100 40 47')
    site = json.loads(DBG.get_build_status(world) or '{}')
    cards = json.loads(DBG.get_villager_cards(world) or '{}').get('villagers', [])
    cutter = next((card for card in cards if card['npc'] == state['cutter']), None)
    life = json.loads(DBG.get_opening_life_status(world) or '{}')
    settlement = json.loads(DBG.get_settlement_status(world) or '{}')
    buildings = settlement.get('buildings', [])
    existing_wells = [b for b in buildings if b.get('type') == 'well' and b.get('progress', 0) >= 1]
    granaries = [b for b in buildings if b.get('type') == 'granary']
    check('initial_site_dry', site.get('needWood', 0) > 0 and site.get('stockWood') == 0
          and site.get('consumedWood') == 0)
    expected = DBG.get_settlement_ground_point(world, 46.5, 47.5)
    check('site_at_requested_tile', abs(site['sx'] - expected.x) < 1
          and abs(site['sy'] - expected.y) < 1)
    check('stone_only', site.get('needStone', 0) > 0
          and site.get('stockStone') == site.get('needStone')
          and site.get('consumedStone') == 0)
    check('vital_supply', bool(existing_wells) and any(
        DBG.get_food_stock(world, b['id']) >= 100 for b in granaries))
    check('no_courier', site.get('courier') == '' and site.get('carry') == 0)
    check('woodcutter_job', cutter is not None and cutter.get('job') == 'woodcutter'
          and life.get('id') == state['observer'] and life.get('id') != state['cutter'])
    if not all(state['checks'].values()):
        finish('invalid_initial_conditions')
        return
    state['phase'] = 'observe'
    cmd('anastasis.Sim.TimeScale 1')
    cmd('anastasis.Sim.Warp 10')
    unreal.log('LABOR_SOCIAL_SETUP site=%s cutter=%s observer=%s courier_removed=%s stone=%d' %
               (site['id'], state['cutter'], state['observer'], courier, site['stockStone']))


def process_sample(sample, actor, wood):
    site, life, goal = sample['site'], sample['life'], sample['goal']
    if not actor or not wood or life.get('id') != state['observer']:
        finish('missing_runtime_observer')
        return
    mass = wood['total'] + site['stockWood'] + site['consumedWood']
    if 'initial_mass' not in state:
        state['initial_mass'] = mass
        state['initial_stone_mass'] = site['stockStone'] + site['consumedStone']
        state['initial_remaining'] = wood['remaining']
        state['initial_position'] = [actor['x'], actor['y']]
        state['sim_start'] = site['time']
        state['observer_drinks_at_completion'] = None
        state['observer_last_drinks'] = life['drinks']
        state['previous_site_wood'] = site['stockWood'] + site['consumedWood']
        state['previous_site_completed'] = site['completed']
        check('mass_conserved', True)
        check('stone_conserved', state['initial_stone_mass'] == site['needStone'])
    if site['stockStone'] + site['consumedStone'] != state['initial_stone_mass']:
        check('stone_conserved', False)
        finish('stone_mass_changed')
        return
    if mass != state['initial_mass']:
        check('mass_conserved', False)
        finish('wood_mass_changed initial=%d actual=%d' % (state['initial_mass'], mass))
        return
    sim_elapsed = site['time'] - state['sim_start']
    site_wood = site['stockWood'] + site['consumedWood']
    cutter_dist = ((actor['x'] - 46.5) ** 2 + (actor['y'] - 47.5) ** 2) ** 0.5
    observer_dist = ((life['x'] - 46.5) ** 2 + (life['y'] - 47.5) ** 2) ** 0.5
    near_site = cutter_dist < 3.0
    observer_near_site = observer_dist < 2.5
    state['saw_gather_goal'] |= goal == 'gatherWood'
    state['saw_delivery_goal'] |= goal == 'deliver'
    state['saw_harvest'] |= actor['gathered'] > 0 and wood['remaining'] < state['initial_remaining']
    state['saw_motion'] |= abs(actor['x'] - state['initial_position'][0]) + abs(actor['y'] - state['initial_position'][1]) > 1.0
    state['saw_delivery'] |= site_wood > 0 and actor['gathered'] >= site_wood
    state['saw_delivery_at_site'] |= site_wood > state['previous_site_wood'] and near_site
    state['previous_site_wood'] = site_wood
    if site['completed'] and state['observer_drinks_at_completion'] is None:
        state['observer_drinks_at_completion'] = life['drinks']
    if (state['previous_site_completed'] and state['observer_drinks_at_completion'] is not None
            and life['drinks'] > state['observer_last_drinks']
            and life['id'] != state['cutter'] and observer_near_site):
        state['saw_other_well_use'] = True
        state['well_use_event'] = {'sim_s': round(sim_elapsed, 2),
                                   'drinks_before': state['observer_last_drinks'],
                                   'drinks_after': life['drinks'],
                                   'goal': life['goal'], 'distance_tiles': round(observer_dist, 3)}
    state['observer_last_drinks'] = life['drinks']
    state['previous_site_completed'] = site['completed']
    row = {'sim_s': round(sim_elapsed, 2), 'remaining': wood['remaining'],
           'carried': actor['carried'], 'gathered': actor['gathered'],
           'goal': goal, 'site_wood': site_wood, 'stock_wood': site['stockWood'],
           'consumed_wood': site['consumedWood'], 'stock_stone': site['stockStone'],
           'consumed_stone': site['consumedStone'], 'completed': site['completed'],
           'near_site': near_site, 'observer_near_site': observer_near_site,
           'observer_drinks': life['drinks'], 'observer_goal': life['goal'],
           'observer_xy': [life['x'], life['y']], 'observer_site_distance': round(observer_dist, 3),
           'cutter_site_distance': round(cutter_dist, 3), 'mass': mass}
    state['samples'].append(row)
    if len(state['samples']) % 20 == 0:
        unreal.log('LABOR_SOCIAL_SAMPLE ' + json.dumps(row))
    if sim_elapsed > SIM_LIMIT:
        finish('sim_timeout last=' + json.dumps(row))
        return
    if (site['completed'] and state['saw_other_well_use']):
        check('harvest', state['saw_harvest'] and state['saw_gather_goal'])
        check('movement', state['saw_motion'])
        check('delivery_goal', state['saw_delivery_goal'])
        check('physical_delivery', state['saw_delivery'] and state['saw_delivery_at_site'])
        check('well_completed', site['consumedWood'] == site['needWood']
              and site['consumedStone'] == site['needStone'])
        check('other_well_use', state['saw_other_well_use'])
        finish('site=%s gathered=%d delivered_wood=%d consumed_wood=%d observer_drinks=%d mass=%d' %
               (site['id'], actor['gathered'], site_wood, site['consumedWood'],
                life['drinks'], mass))


def tick(_dt):
    try:
        now = time.monotonic()
        if now - START > WALL_LIMIT:
            finish('wall_timeout phase=%s last=%s' %
                   (state['phase'], state['samples'][-1] if state['samples'] else 'none'))
            return
        if state['phase'] == 'start' and now - START > 3:
            state['phase'] = 'await_pie'
            LES.editor_request_begin_play()
            return
        if state['phase'] == 'await_pie' and LES.is_in_play_in_editor():
            setup(UES.get_game_world())
            return
        if state['phase'] != 'observe':
            return
        if state['pending'] is not None:
            poll_wood_log()
            if state['wood_actor'] is not None and state['wood_total'] is not None:
                process_sample(state['pending'], state['wood_actor'], state['wood_total'])
                state['pending'] = None
            elif now - state['pending']['at'] > 2:
                finish('wood_status_log_missing')
            return
        if now - state['last_sample_at'] < 0.4:
            return
        state['last_sample_at'] = now
        world = UES.get_game_world()
        site = json.loads(DBG.get_build_status(world) or '{}')
        life = json.loads(DBG.get_opening_life_status(world) or '{}')
        goal = DBG.get_npc_state(world, state['cutter']).split('|')[0]
        if not site.get('site') or not life or not goal:
            finish('site_or_npc_disappeared')
            return
        state['pending'] = {'site': site, 'life': life, 'goal': goal, 'at': now}
        state['wood_actor'] = None
        state['wood_total'] = None
        cmd('Anastasis.Village.WoodStatus')
    except Exception as exc:
        unreal.log_error('LABOR_SOCIAL_EXCEPTION ' + repr(exc))
        finish('exception=' + repr(exc))


handle = unreal.register_slate_post_tick_callback(tick)
