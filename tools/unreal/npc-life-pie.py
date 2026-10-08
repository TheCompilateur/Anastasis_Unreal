"""PIE du village initial : un habitant a foyer, travail et une vie autonome.

Lance par editor-batch.ps1 -Proofs npc-life-pie. Aucune commande de scenario.

Temps accelere (TIME_WARP_001, npc-life-warp-001) : la preuve avance le temps simule par
anastasis.Sim.Warp et borne son attente en temps SIMULE. Avant, elle attendait 180 s reelles a
Speed 5 : PumpFrame plafonne son rattrapage par image, donc une machine chargee (plusieurs
editeurs) simulait moins de temps dans les memes 180 s et la preuve tombait en timeout sans que le
village ait change. Les criteres de reussite sont inchanges.
  ANASTASIS_NPC_LIFE_SIM_SECONDS  temps simule maximal (defaut 1800 : vingt jours)
  ANASTASIS_NPC_LIFE_WARP         acceleration (defaut 10)
  ANASTASIS_NPC_LIFE_WALL_SECONDS plafond reel de securite (defaut 200, sous le delai du registre)
"""
import json
import os
import time

import unreal

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
dbg = unreal.AnastasisSimulationDebugLibrary
les.load_level('/Game/Anastasis/Maps/Lvl_AnastasisSlice')

SIM_SECONDS = float(os.environ.get('ANASTASIS_NPC_LIFE_SIM_SECONDS', '1800'))
WARP = float(os.environ.get('ANASTASIS_NPC_LIFE_WARP', '10'))
WALL_SECONDS = float(os.environ.get('ANASTASIS_NPC_LIFE_WALL_SECONDS', '200'))

started = time.monotonic()
sim_started = None
sim_elapsed = 0.0
last_sample = 0.0
phase = 0
rows = []
build_rows = []
builder_positions = []
saw_material_carry = False
initial_site_dry = False
handle = None


def finish(ok, reason):
    # Une preuve qui accelere remet le rythme en partant (TIME_WARP_001).
    unreal.SystemLibrary.execute_console_command(None, 'anastasis.Sim.Warp 1')
    unreal.log('NPC_LIFE_PIE %s %s sim_s=%.0f wall_s=%.0f' % ('PASS' if ok else 'FAIL', reason, sim_elapsed, time.monotonic() - started))
    unreal.unregister_slate_post_tick_callback(handle)
    unreal.SystemLibrary.quit_editor()


def tick(_dt):
    global phase, last_sample, saw_material_carry, initial_site_dry, sim_started, sim_elapsed
    now = time.monotonic()
    state = lambda: (json.dumps(rows[-1]) if rows else 'none', json.dumps(build_rows[-1]) if build_rows else 'none')
    if sim_elapsed > SIM_SECONDS:
        finish(False, 'sim_timeout life=%s build=%s' % state())
        return
    if now - started > WALL_SECONDS:
        # Machine trop lente pour parcourir le temps simule demande : ce n'est pas un verdict sur le village.
        finish(False, 'wall_timeout life=%s build=%s' % state())
        return
    if phase == 0 and now - started > 3:
        phase = 1
        les.editor_request_begin_play()
        return
    if phase == 1 and les.is_in_play_in_editor():
        phase = 2
        world = ues.get_game_world()
        unreal.SystemLibrary.execute_console_command(world, 'anastasis.Sim.TimeScale 1')
        unreal.SystemLibrary.execute_console_command(world, 'anastasis.Sim.Warp %g' % WARP)
        return
    if phase != 2 or now - last_sample < 0.5:
        return
    last_sample = now
    world = ues.get_game_world()
    if not world:
        return
    sim_now = dbg.get_simulation_time(world)
    if sim_started is None:
        sim_started = sim_now
    sim_elapsed = sim_now - sim_started
    row = json.loads(dbg.get_opening_life_status(world) or '{}')
    build = json.loads(dbg.get_build_status(world) or '{}')
    cards = json.loads(dbg.get_villager_cards(world) or '{}')
    if not row or not row.get('home') or not row.get('work'):
        if now - started > 25:
            finish(False, 'no_home_or_work ' + json.dumps(row))
        return
    rows.append(row)
    build_rows.append(build)
    if len(rows) == 1:
        initial_site_dry = build.get('site') and build.get('stockWood') == 0 and build.get('stockStone') == 0
    saw_material_carry |= build.get('carry', 0) > 0
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
    real_materials = saw_material_carry and build.get('materialsDelivered', 0) >= 32
    if moved and drank and worked and slept and claimed and len(goals) >= 3 and built and builder_moved and owned_home and shared_work and real_materials:
        finish(True, 'id=%s home=%s work=%s drinks=%d deliveries=%d rests=%d site_pieces=%d builders=%d owner=%s owner_rests=%d farmers=%d other_deliveries=%d materials_delivered=%d' %
               (row['id'], row['home'], row['work'], row['drinks'], row['deliveries'], row['rests'],
                build['pieces'], len(builders), build['owner'], build['ownerRests'], row['farmers'], row['otherFarmerDeliveries'], build['materialsDelivered']))


handle = unreal.register_slate_post_tick_callback(tick)
