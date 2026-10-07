"""PIE du village initial : un habitant a foyer, travail et une vie autonome.

Lance par editor-batch.ps1 -Proofs npc-life-pie. Aucune commande de scenario.
"""
import json
import time

import unreal

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
dbg = unreal.AnastasisSimulationDebugLibrary
les.load_level('/Game/Anastasis/Maps/Lvl_AnastasisSlice')

started = time.monotonic()
last_sample = 0.0
phase = 0
rows = []
build_rows = []
builder_positions = []
handle = None


def finish(ok, reason):
    unreal.log('NPC_LIFE_PIE %s %s' % ('PASS' if ok else 'FAIL', reason))
    unreal.unregister_slate_post_tick_callback(handle)
    unreal.SystemLibrary.quit_editor()


def tick(_dt):
    global phase, last_sample
    now = time.monotonic()
    if now - started > 180:
        finish(False, 'timeout life=%s build=%s' %
               (json.dumps(rows[-1]) if rows else 'none', json.dumps(build_rows[-1]) if build_rows else 'none'))
        return
    if phase == 0 and now - started > 3:
        phase = 1
        les.editor_request_begin_play()
        return
    if phase == 1 and les.is_in_play_in_editor():
        phase = 2
        world = ues.get_game_world()
        unreal.SystemLibrary.execute_console_command(world, 'anastasis.Sim.TimeScale 1')
        unreal.SystemLibrary.execute_console_command(world, 'anastasis.Sim.Speed 5')
        return
    if phase != 2 or now - last_sample < 0.5:
        return
    last_sample = now
    world = ues.get_game_world()
    if not world:
        return
    row = json.loads(dbg.get_opening_life_status(world) or '{}')
    build = json.loads(dbg.get_build_status(world) or '{}')
    cards = json.loads(dbg.get_villager_cards(world) or '{}')
    if not row or not row.get('home') or not row.get('work'):
        if now - started > 25:
            finish(False, 'no_home_or_work ' + json.dumps(row))
        return
    rows.append(row)
    build_rows.append(build)
    builders = [c for c in cards.get('villagers', []) if c.get('job') == 'builder']
    if builders:
        builder_positions.append({c['npc']: (c['x'], c['y']) for c in builders})
    if len(rows) % 20 == 0:
        unreal.log('NPC_LIFE_SAMPLE ' + json.dumps({'life': row, 'build': build, 'builders': builders}))
    goals = {r['goal'] for r in rows}
    moved = any(abs(r['x'] - rows[0]['x']) + abs(r['y'] - rows[0]['y']) > 1.0 for r in rows)
    drank = any(r['drinks'] > rows[0]['drinks'] for r in rows)
    worked = any(r['deliveries'] > rows[0]['deliveries'] for r in rows)
    slept = any(r['rests'] > rows[0]['rests'] for r in rows)
    claimed = any(r['claimed'] for r in rows)
    built = any(b.get('pieces', 0) > 0 and b.get('workers', 0) > 0 for b in build_rows)
    builder_moved = (len(builder_positions) > 1 and
                     any(abs(pos[0] - builder_positions[0][id][0]) + abs(pos[1] - builder_positions[0][id][1]) > 100
                         for sample in builder_positions[1:] for id, pos in sample.items()
                         if id in builder_positions[0]))
    owned_home = build.get('completed') and build.get('owner') and build.get('ownerHome') == build.get('id')
    shared_work = row.get('farmers', 0) >= 2 and row.get('otherFarmerDeliveries', 0) > 0
    if moved and drank and worked and slept and claimed and len(goals) >= 3 and built and builder_moved and owned_home and shared_work:
        finish(True, 'id=%s home=%s work=%s drinks=%d deliveries=%d rests=%d site_pieces=%d builders=%d owner=%s owner_rests=%d farmers=%d other_deliveries=%d' %
               (row['id'], row['home'], row['work'], row['drinks'], row['deliveries'], row['rests'],
                build['pieces'], len(builders), build['owner'], build['ownerRests'], row['farmers'], row['otherFarmerDeliveries']))


handle = unreal.register_slate_post_tick_callback(tick)
