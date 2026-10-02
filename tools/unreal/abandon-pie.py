"""Preuve PIE du vieillissement des maisons vides (mission abandon-001, ABANDON_001).

Le temps est AVANCE (Anastasis.Sim.Advance), jamais attendu. On lit le RENDU : le parametre
`Neglect` du materiau dynamique reellement pose sur le mesh de la maison.

    1. FirstHouse 4, plein jour             maison habitee  -> aucun materiau d'usure
    2. RemoveNpc de tous                    maison vide, jour D0
    3. Advance 3d, 5d, 12d, 30d             Neglect = AnastasisMetabolism::NeglectForDays(jours vides)
    4. temoin faux (Metabolism 2)           meme maison vide depuis 50 j -> Neglect 0 (village propre)
    5. retour a Metabolism 1                -> de nouveau 1.0

Verdict : ABANDON_PIE PASS / ABANDON_PIE FAIL <raison>.   Sortie : ANASTASIS_ABANDON_OUT
(defaut Saved/AbandonEvidence/pie), abandon.json.

    UnrealEditor-Cmd.exe <uproject> -unattended -nosplash -NoLiveCoding -abslog=<log> ^
        -ExecCmds="py tools/unreal/abandon-pie.py"
"""
import json
import math
import os
import time

import unreal

OUT = os.environ.get('ANASTASIS_ABANDON_OUT', os.path.join(unreal.Paths.project_saved_dir(), 'AbandonEvidence', 'pie'))
os.makedirs(OUT, exist_ok=True)

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
unreal.log('ABANDON_MAP_LOAD=' + str(les.load_level('/Game/Anastasis/Maps/Lvl_AnastasisSlice')))

DBG = unreal.AnastasisSimulationDebugLibrary
DAY = 90.0
t0 = time.monotonic()
phase = 0
step = 0
frames = 0
mark = 0.0
vacated_day = None
handle = None
record = {'steps': []}

# (commande Advance, jours vides attendus apres)
PLAN = {2: ('3d', 3), 4: ('5d', 8), 6: ('12d', 20), 8: ('30d', 50)}


def expected(days):
    keys = [(0, 0.0), (6, 0.2), (18, 0.55), (45, 1.0)]
    if days >= 45:
        return 1.0
    for (d0, v0), (d1, v1) in zip(keys, keys[1:]):
        if days <= d1:
            return v0 + (v1 - v0) * (days - d0) / (d1 - d0)
    return 1.0


def finish(message):
    record['verdict'] = message
    with open(os.path.join(OUT, 'abandon.json'), 'w', encoding='utf-8') as f:
        json.dump(record, f, indent=1)
    unreal.log(message)
    unreal.unregister_slate_post_tick_callback(handle)
    unreal.SystemLibrary.quit_editor()


def cmd(world, text):
    unreal.log('ABANDON_CMD ' + text)
    unreal.SystemLibrary.execute_console_command(world, text)


def house_actor(world):
    for actor in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.AnastasisVillageBuilding):
        if 'house' in actor.get_actor_label():
            return actor
    return None


def neglect_of(actor):
    """Parametre Neglect du materiau dynamique, ou None si le materiau d'origine est encore pose."""
    body = actor.get_component_by_class(unreal.StaticMeshComponent)
    mat = body.get_material(0) if body else None
    if mat and isinstance(mat, unreal.MaterialInstanceDynamic):
        return float(mat.get_scalar_parameter_value('Neglect'))
    return None


def day_of(world):
    return 1 + int(math.floor(DBG.get_simulation_time(world) / DAY))


def tick(dt):
    global phase, step, frames, mark, vacated_day
    now = time.monotonic()
    if now - t0 > 420:
        finish('ABANDON_PIE FAIL timeout phase=%d step=%d' % (phase, step))
        return
    if phase == 0 and now - t0 > 3:
        phase = 1
        les.editor_request_begin_play()
        return
    if phase == 1 and les.is_in_play_in_editor():
        phase = 2
        mark = now
        return
    if phase == 3 and not les.is_in_play_in_editor():
        finish(record.get('pending') or 'ABANDON_PIE PASS')
        return
    if phase != 2:
        return
    world = ues.get_game_world()
    if not world or DBG.get_simulation_time(world) < 0:
        return
    frames += 1

    def stop(reason):
        global phase
        record['pending'] = 'ABANDON_PIE FAIL ' + reason
        phase = 3
        les.editor_request_end_play()

    def read(tag, days=None):
        actor = house_actor(world)
        n = neglect_of(actor) if actor else 'no-actor'
        row = {'tag': tag, 'day': day_of(world), 'vacant_days': days, 'neglect': n,
               'expected': expected(days) if days is not None else 0.0}
        record['steps'].append(row)
        unreal.log('ABANDON_READ ' + json.dumps(row))
        return actor, n

    if step == 0 and now - mark > 1.5:
        cmd(world, 'anastasis.Sim.TimeScale 1')
        cmd(world, 'anastasis.Village.Metabolism 1')
        cmd(world, 'Anastasis.Village.FirstHouse 4')
        cmd(world, 'Anastasis.Sim.Advance @12')
        frames = 0
        step = 1
    elif step == 1 and frames > 10:
        actor, n = read('habitee')
        if not actor:
            return stop('no house actor')
        if n not in (None, 0.0):
            return stop('inhabited house has a neglect material: %r' % (n,))
        for i in range(0, 12):
            cmd(world, 'Anastasis.Village.RemoveNpc npc-%d' % i)
        vacated_day = day_of(world)
        frames = 0
        step = 2
    elif step in (2, 4, 6, 8) and frames > 6:
        cmd(world, 'Anastasis.Sim.Advance ' + PLAN[step][0])
        frames = 0
        step += 1
    elif step in (3, 5, 7, 9) and frames > 10:
        days = day_of(world) - vacated_day
        actor, n = read('vide', days)
        want = expected(days)
        if n is None or n == 'no-actor' or abs(n - want) > 0.02:
            return stop('neglect %r != expected %.3f at %d vacant days' % (n, want, days))
        step += 1
        frames = 0
    elif step == 10 and frames > 3:
        cmd(world, 'anastasis.Village.Metabolism 2')
        frames = 0
        step = 11
    elif step == 11 and frames > 10:
        actor, n = read('temoin_faux', day_of(world) - vacated_day)
        if n not in (None, 0.0):
            return stop('wrong witness still ages the house: %r' % (n,))
        cmd(world, 'anastasis.Village.Metabolism 1')
        frames = 0
        step = 12
    elif step == 12 and frames > 10:
        days = day_of(world) - vacated_day
        actor, n = read('retour_verite', days)
        if n is None or n == 'no-actor' or abs(n - 1.0) > 0.02:
            return stop('truth mode did not return to the aged look: %r' % (n,))
        phase = 3
        les.editor_request_end_play()


handle = unreal.register_slate_post_tick_callback(tick)
