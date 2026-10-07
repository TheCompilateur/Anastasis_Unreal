"""PIE du chantier sec : le porteur extrait et livre du bois/pierre du monde.

Lance par editor-batch.ps1 -Proofs material-courier-pie. Aucun asset sauve.
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
handle = None
samples = 0
saw_carry = False
saw_source = False
initially_dry = False
last = {}


def finish(ok, reason):
    unreal.log('MATERIAL_COURIER_PIE %s %s' % ('PASS' if ok else 'FAIL', reason))
    unreal.unregister_slate_post_tick_callback(handle)
    unreal.SystemLibrary.quit_editor()


def tick(_dt):
    global phase, last_sample, samples, saw_carry, saw_source, initially_dry, last
    now = time.monotonic()
    if now - started > 180:
        finish(False, 'timeout ' + json.dumps(last))
        return
    if phase == 0 and now - started > 3:
        phase = 1
        les.editor_request_begin_play()
        return
    if phase == 1 and les.is_in_play_in_editor():
        phase = 2
        world = ues.get_game_world()
        unreal.SystemLibrary.execute_console_command(world, 'Anastasis.Village.FirstSite house 2 0')
        unreal.SystemLibrary.execute_console_command(world, 'anastasis.Sim.TimeScale 1')
        unreal.SystemLibrary.execute_console_command(world, 'anastasis.Sim.Speed 5')
        return
    if phase != 2 or now - last_sample < 0.5:
        return
    last_sample = now
    world = ues.get_game_world()
    if not world:
        return
    row = json.loads(dbg.get_build_status(world) or '{}')
    if not row.get('site'):
        if now - started > 25:
            finish(False, 'no_site')
        return
    last = row
    samples += 1
    if samples == 1:
        initially_dry = row['stockWood'] == 0 and row['stockStone'] == 0
    saw_carry |= row.get('carry', 0) > 0
    saw_source |= row.get('sourceIndex', -1) >= 0
    if samples % 20 == 0:
        unreal.log('MATERIAL_COURIER_SAMPLE ' + json.dumps(row))
    if row['stockWood'] + row['consumedWood'] + row['stockStone'] + row['consumedStone'] != row['materialsDelivered']:
        finish(False, 'site_ledger_mismatch ' + json.dumps(row))
        return
    if row['completed']:
        ok = (initially_dry and saw_carry and saw_source and row['materialsDelivered'] >= 32
              and row['consumedWood'] == row['needWood'] and row['consumedStone'] == row['needStone'])
        finish(ok, 'site=%s courier=%s delivered=%d wood=%d stone=%d carry_seen=%s source_seen=%s' %
               (row['id'], row['courier'], row['materialsDelivered'], row['consumedWood'],
                row['consumedStone'], saw_carry, saw_source))


handle = unreal.register_slate_post_tick_callback(tick)
