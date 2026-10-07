"""Numeric PIE proof: site stock meshes follow delivered and consumed materials.

Run with editor-batch.ps1 -Proofs site-stock-visual-pie. No screenshots or assets saved.
"""
import json
import time

import unreal

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
dbg = unreal.AnastasisSimulationDebugLibrary
les.load_level('/Game/Anastasis/Maps/Lvl_AnastasisSlice')
started = time.monotonic()
phase = 'start'
phase_at = started
handle = None
site_id = None
full_stock = None
saw_consumed_stock = False
saw_smaller_visual = False
reported_shrink = False
last_sample = 0.0
last = {}


def finish(ok, reason):
    unreal.log('SITE_STOCK_VISUAL_PIE %s %s' % ('PASS' if ok else 'FAIL', reason))
    unreal.unregister_slate_post_tick_callback(handle)
    unreal.SystemLibrary.quit_editor()


def advance(name):
    global phase, phase_at
    phase, phase_at = name, time.monotonic()


def visual_counts(world, row):
    actors = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.AnastasisVillageBuilding)
    actor = next((a for a in actors if a.get_actor_label().endswith('_' + row['id'])), None)
    if not actor:
        raise AssertionError('site actor missing: ' + row['id'])
    comps = list(actor.get_components_by_class(unreal.InstancedStaticMeshComponent))
    wood = next(c for c in comps if c.get_name().startswith('WoodStockVisual'))
    stone = next(c for c in comps if c.get_name().startswith('StoneStockVisual'))
    for component, name in ((wood, 'SM_Site_TimberBundle_01'), (stone, 'SM_Site_StoneBundle_01')):
        mesh = component.get_editor_property('static_mesh')
        if not mesh or mesh.get_name() != name:
            raise AssertionError('missing site-stock asset ' + name)
        if component.get_collision_enabled() != unreal.CollisionEnabled.NO_COLLISION:
            raise AssertionError('visual stock blocks movement')
    return wood.get_instance_count(), stone.get_instance_count()


def expected(stock, need, active=True):
    return min(3, max(1, (3 * stock + need - 1) // need)) if active and stock > 0 and need > 0 else 0


def tick(_dt):
    global site_id, full_stock, saw_consumed_stock, saw_smaller_visual, reported_shrink, last_sample, last
    now = time.monotonic()
    if now - started > 210:
        finish(False, 'timeout phase=%s row=%s' % (phase, json.dumps(last)))
        return
    if phase == 'start':
        if now - started > 3:
            les.editor_request_begin_play()
            advance('pie')
        return
    world = ues.get_game_world()
    if not world or not les.is_in_play_in_editor():
        return
    if phase == 'pie':
        unreal.SystemLibrary.execute_console_command(world, 'anastasis.Sim.TimeScale 0')
        unreal.SystemLibrary.execute_console_command(world, 'Anastasis.Village.FirstSite house 2 0')
        advance('dry')
        return
    if now - phase_at < .8:
        return
    if now - last_sample < .2:
        return
    last_sample = now
    row = json.loads(dbg.get_build_status(world) or '{}')
    if not row.get('site'):
        if now - phase_at > 12:
            finish(False, 'no site')
        return
    last = row
    try:
        counts = visual_counts(world, row)
        active = not row['completed']
        want = (expected(row['stockWood'], row['needWood'], active),
                expected(row['stockStone'], row['needStone'], active))
        if phase in ('dry', 'one', 'full') and counts != want:
            raise AssertionError('phase=%s count=%s expected=%s ledger=%s' % (phase, counts, want, json.dumps(row)))
        if phase == 'dry':
            if row['stockWood'] or row['stockStone'] or counts != (0, 0):
                raise AssertionError('site was not dry')
            if row['needWood'] <= 1 or row['needStone'] <= 1:
                raise AssertionError('house site has no useful two-material budget')
            site_id = row['id']
            unreal.SystemLibrary.execute_console_command(world, 'Anastasis.Village.DeliverSite %s 1 1' % site_id)
            advance('one')
        elif phase == 'one':
            if (row['stockWood'], row['stockStone']) != (1, 1) or counts != (1, 1):
                raise AssertionError('single deliveries did not appear')
            unreal.SystemLibrary.execute_console_command(world, 'Anastasis.Village.DeliverSite %s %d %d' %
                                                          (site_id, row['needWood'] - 1, row['needStone'] - 1))
            advance('full')
        elif phase == 'full':
            full_stock = (row['stockWood'], row['stockStone'])
            if full_stock != (row['needWood'], row['needStone']) or counts != (3, 3):
                raise AssertionError('full deliveries did not show three bundles')
            unreal.SystemLibrary.execute_console_command(world, 'anastasis.Sim.TimeScale 1')
            unreal.SystemLibrary.execute_console_command(world, 'anastasis.Sim.Speed 5')
            advance('consume')
        elif phase == 'consume':
            if row['stockWood'] < full_stock[0] or row['stockStone'] < full_stock[1]:
                saw_consumed_stock = True
            if not row['completed'] and counts == want and (counts[0] < 3 or counts[1] < 3):
                saw_smaller_visual = True
            if row['completed']:
                advance('complete')
            elif counts == want and saw_smaller_visual and not reported_shrink:
                reported_shrink = True
                unreal.log('SITE_STOCK_VISUAL_PIE CONSUMED ledger=%d,%d visual=%d,%d' %
                           (row['stockWood'], row['stockStone'], counts[0], counts[1]))
        elif phase == 'complete':
            if counts != (0, 0) or not saw_consumed_stock or not saw_smaller_visual:
                raise AssertionError('completed site retained stock or consumption unseen: ' + str(counts))
            finish(True, 'dry=0,0 one=1,1 full=3,3 consumed=yes complete=0,0 site=' + site_id)
    except Exception as exc:
        finish(False, str(exc))


def guarded_tick(dt):
    try:
        tick(dt)
    except Exception as exc:
        import traceback
        unreal.log_error(traceback.format_exc())
        finish(False, str(exc))


handle = unreal.register_slate_post_tick_callback(guarded_tick)
