"""Preuve PIE de la metabolisation des maisons (mission iceberg-001, ICEBERG_001).

Question : si la societe meurt et que ses maisons restent, le monde visible le dit-il ?

Charge Lvl_AnastasisSlice, lance un PIE, pilote la simulation UNIQUEMENT par ses commandes
console, et lit le RENDU : le composant de lumiere reel de l'acteur maison (visibilite,
intensite), pas un champ interne. Temps accelere (anastasis.Sim.Warp), jamais attente murale.

    1. FirstHouse 4          maison possedee + 4 habitants
    2. plein jour            -> foyer eteint (un foyer ne s'allume pas a midi)
    3. la nuit, un dormeur dedans -> foyer ALLUME (intensite > 0, visible)
    4. RemoveNpc de tous     -> le batiment RESTE, le foyer S'ETEINT : la maison est noire
    5. temoin faux (Metabolism 2) -> la meme maison vide est rallumee : le mensonge est mesurable
    6. retour a Metabolism 1 -> noire de nouveau

Verdict : METABOLISM_PIE PASS / METABOLISM_PIE FAIL <raison>.
Sortie : ANASTASIS_METABOLISM_OUT (defaut Saved/MetabolismEvidence/pie), metabolism.json.

    UnrealEditor-Cmd.exe <uproject> -unattended -nosplash -NoLiveCoding -abslog=<log> ^
        -ExecCmds="py tools/unreal/metabolism-pie.py"
"""
import json
import os
import time

import unreal

OUT = os.environ.get('ANASTASIS_METABOLISM_OUT', os.path.join(unreal.Paths.project_saved_dir(), 'MetabolismEvidence', 'pie'))
os.makedirs(OUT, exist_ok=True)

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
unreal.log('METABOLISM_MAP_LOAD=' + str(les.load_level('/Game/Anastasis/Maps/Lvl_AnastasisSlice')))

DBG = unreal.AnastasisSimulationDebugLibrary
t0 = time.monotonic()
phase = 0
step = 0
mark = 0.0
frames = 0
handle = None
record = {'steps': []}


def finish(message):
    record['verdict'] = message
    with open(os.path.join(OUT, 'metabolism.json'), 'w', encoding='utf-8') as f:
        json.dump(record, f, indent=1)
    unreal.log(message)
    unreal.unregister_slate_post_tick_callback(handle)
    unreal.SystemLibrary.quit_editor()


def cmd(world, text):
    unreal.log('METABOLISM_CMD ' + text)
    unreal.SystemLibrary.execute_console_command(world, text)


def houses(world):
    """Les acteurs maison du monde de jeu avec leur lumiere de foyer."""
    found = []
    for actor in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.AnastasisVillageBuilding):
        label = actor.get_actor_label()
        if 'house' not in label:
            continue
        light = actor.get_component_by_class(unreal.PointLightComponent)
        if light:
            found.append((label, light))
    return found


def read(world, tag):
    hs = houses(world)
    row = {'tag': tag, 'sim_time': DBG.get_simulation_time(world), 'phase': DBG.get_village_phase(world),
           'houses': [{'label': l, 'visible': bool(c.is_visible()), 'intensity': float(c.get_editor_property('intensity'))}
                      for l, c in hs]}
    record['steps'].append(row)
    unreal.log('METABOLISM_READ ' + json.dumps(row))
    return hs


def lit(hs):
    return bool(hs) and all(c.is_visible() and c.get_editor_property('intensity') > 0.0 for _, c in hs)


def dark(hs):
    return bool(hs) and all((not c.is_visible()) or c.get_editor_property('intensity') <= 0.0 for _, c in hs)


def tick(dt):
    global phase, step, mark, frames
    now = time.monotonic()
    if now - t0 > 420:
        finish('METABOLISM_PIE FAIL timeout phase=%d step=%d' % (phase, step))
        return
    if phase == 0 and now - t0 > 3:
        phase = 1
        les.editor_request_begin_play()
        unreal.log('METABOLISM_PIE_REQUESTED')
        return
    if phase == 1 and les.is_in_play_in_editor():
        phase = 2
        mark = now
        return
    if phase == 3 and not les.is_in_play_in_editor():
        finish(record.get('pending') or 'METABOLISM_PIE PASS')
        return
    if phase != 2:
        return

    world = ues.get_game_world()
    if not world:
        return
    t = DBG.get_simulation_time(world)
    if t < 0:
        return
    village_phase = DBG.get_village_phase(world)
    frames += 1

    def stop(reason):
        global phase
        record['pending'] = 'METABOLISM_PIE FAIL ' + reason
        phase = 3
        les.editor_request_end_play()

    if step == 0 and now - mark > 1.5:
        cmd(world, 'anastasis.Sim.TimeScale 1')
        cmd(world, 'anastasis.Village.Metabolism 1')
        cmd(world, 'Anastasis.Village.FirstHouse 4')
        cmd(world, 'Anastasis.Sim.Advance @12')
        mark = now
        frames = 0
        step = 1
    elif step == 1 and frames > 10:
        hs = read(world, 'midi')
        if not hs:
            return stop('no house actor with a hearth light')
        if not dark(hs):
            return stop('hearth lit at noon')
        cmd(world, 'anastasis.Sim.Warp 8')
        step = 2
    elif step == 2 and village_phase == 'night' and DBG.count_inside(world, 'building-0') > 0:
        cmd(world, 'anastasis.Sim.Warp 1')
        mark = now
        frames = 0
        step = 3
    elif step == 3 and frames > 20:
        hs = read(world, 'nuit_habitee')
        if not lit(hs):
            return stop('inhabited house is not lit at night')
        for i in range(0, 12):
            cmd(world, 'Anastasis.Village.RemoveNpc npc-%d' % i)
        frames = 0
        step = 4
    elif step == 4 and frames > 20:
        hs = read(world, 'nuit_village_mort')
        if not hs:
            return stop('house actor vanished with its inhabitants (the building must stay)')
        if not dark(hs):
            return stop('house with no inhabitant still lit: the ghost village lies')
        cmd(world, 'anastasis.Village.Metabolism 2')
        frames = 0
        step = 5
    elif step == 5 and frames > 10:
        hs = read(world, 'nuit_temoin_faux')
        if not lit(hs):
            return stop('wrong-witness control did not light the empty house')
        cmd(world, 'anastasis.Village.Metabolism 1')
        frames = 0
        step = 6
    elif step == 6 and frames > 10:
        hs = read(world, 'nuit_retour_verite')
        if not dark(hs):
            return stop('truth mode did not return to dark')
        phase = 3
        les.editor_request_end_play()


handle = unreal.register_slate_post_tick_callback(tick)
