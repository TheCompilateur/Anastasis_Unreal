"""Preuve PIE du tissu du village (mission village-fabric-001, VILLAGE_FABRIC_001).

Question : entre les maisons, le monde rendu porte-t-il des ruelles qui relient chaque seuil a la
placette du puits, sans traverser un batiment, et ce tissu est-il le meme a chaque fois qu'on le refait ?

Charge Lvl_AnastasisSlice, lance un PIE, pose un hameau UNIQUEMENT par les commandes console, et lit
`AnastasisVillageFabricLibrary.get_village_fabric_status` (rapport de la grammaire, acteur, defrichement).

    1. Anastasis.Village.Hamlet 6 0      (architecture-crusade-001) ; a defaut, sur la case du puits du
       lancement (wx, wy) : FirstHouse 0 wx+2 wy (remplace le village : maisons a +2 et +8) puis FirstWell 0 wx wy
    2. tissu construit : seuils tous relies, aucune chaussee sur un corps, placette, geometrie non vide
    3. FabricClear 0 -> herbe rendue (0 defrichee), meme signature ; FabricClear 1 -> defrichement refait
       (meme compte a 5 % pres : l'herbe peut etre re-instanciee entre deux passages)
    4. FabricRebuild  -> meme village, meme signature (determinisme dans le moteur)
    5. Fabric 0 -> plus d'acteur ; Fabric 1 -> meme signature

Verdict : VILLAGE_FABRIC PASS / VILLAGE_FABRIC FAIL <raison>.
Sortie : ANASTASIS_FABRIC_OUT (defaut Saved/VillageFabricEvidence/pie), fabric.json. Aucun asset sauve.

    UnrealEditor-Cmd.exe <uproject> -unattended -nosplash -NoLiveCoding -abslog=<log> ^
        -ExecCmds="py tools/unreal/village-fabric-pie.py"
"""
import json
import os
import time

import unreal

OUT = os.environ.get('ANASTASIS_FABRIC_OUT', os.path.join(unreal.Paths.project_saved_dir(), 'VillageFabricEvidence', 'pie'))
os.makedirs(OUT, exist_ok=True)

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
unreal.log('FABRIC_MAP_LOAD=' + str(les.load_level('/Game/Anastasis/Maps/Lvl_AnastasisSlice')))

LIB = unreal.AnastasisVillageFabricLibrary
t0 = time.monotonic()
phase = 0
step = 0
mark = 0.0
handle = None
record = {'steps': []}
base = {}


def finish(message):
    record['verdict'] = message
    with open(os.path.join(OUT, 'fabric.json'), 'w', encoding='utf-8') as f:
        json.dump(record, f, indent=1)
    unreal.log(message)
    unreal.unregister_slate_post_tick_callback(handle)
    unreal.SystemLibrary.quit_editor()


def cmd(world, text):
    unreal.log('FABRIC_CMD ' + text)
    unreal.SystemLibrary.execute_console_command(world, text)


def status(world, tag=None):
    raw = LIB.get_village_fabric_status(world)
    try:
        s = json.loads(raw)
    except ValueError:
        s = {}
    if tag:
        row = {'tag': tag, 'status': s}
        record['steps'].append(row)
        f = s.get('fabric', {})
        unreal.log('FABRIC_READ %s actor=%s rebuilds=%s plots=%s doors=%s connected=%s lanes=%s walls=%s cleared=%s signature=%s' % (
            tag, s.get('actor'), s.get('rebuilds'), f.get('plots'), f.get('doors'), f.get('connected'),
            f.get('lanes'), f.get('walls'), s.get('cleared'), f.get('signature')))
    return s


def tick(dt):
    global phase, step, mark
    now = time.monotonic()
    if now - t0 > 360:
        finish('VILLAGE_FABRIC FAIL timeout phase=%d step=%d' % (phase, step))
        return
    if phase == 0 and now - t0 > 3:
        phase = 1
        les.editor_request_begin_play()
        unreal.log('FABRIC_PIE_REQUESTED')
        return
    if phase == 1 and les.is_in_play_in_editor():
        phase = 2
        mark = now
        return
    if phase == 3 and not les.is_in_play_in_editor():
        finish(record.get('pending') or 'VILLAGE_FABRIC PASS')
        return
    if phase != 2:
        return

    world = ues.get_game_world()
    if not world:
        return
    waited = now - mark

    def stop(reason):
        global phase
        record['pending'] = 'VILLAGE_FABRIC FAIL ' + reason
        phase = 3
        les.editor_request_end_play()

    def advance(next_step):
        global step, mark
        step = next_step
        mark = now

    if step == 0 and waited > 2.0:
        cmd(world, 'anastasis.Village.Fabric 1')
        cmd(world, 'anastasis.Village.FabricClear 1')
        cmd(world, 'Anastasis.Village.Hamlet 6 0')
        advance(1)
    elif step == 1 and waited > 4.0:
        s = status(world)
        if s.get('fabric', {}).get('plots', 0) < 3:
            plaza = s.get('fabric', {}).get('plaza', {})
            if plaza.get('valid'):
                wx = int(plaza['centre'][0] // 2000)
                wy = int(plaza['centre'][1] // 2000)
                record['scenario'] = 'FirstHouse+FirstWell at %d,%d' % (wx, wy)
                cmd(world, 'Anastasis.Village.FirstHouse 0 %d %d' % (wx + 2, wy))
                cmd(world, 'Anastasis.Village.FirstWell 0 %d %d' % (wx, wy))
            else:
                record['scenario'] = 'FirstHouse+FirstWell at settlement'
                cmd(world, 'Anastasis.Village.FirstHouse 0')
                cmd(world, 'Anastasis.Village.FirstWell 0')
        else:
            record['scenario'] = 'Hamlet'
        advance(2)
    elif step == 2 and waited > 4.0:
        s = status(world, 'built')
        f = s.get('fabric', {})
        if not s.get('actor'):
            if waited > 40.0:
                return stop('no fabric actor (terrain never traced?)')
            return
        if f.get('doors', 0) < 2:
            return stop('fewer than two doors: %s' % f.get('doors'))
        if f.get('connected') != f.get('doors'):
            return stop('doors not all connected: %s/%s' % (f.get('connected'), f.get('doors')))
        if f.get('intrusions', 1) != 0:
            return stop('lane through a building body: %s' % f.get('intrusions'))
        if f.get('lanes', 0) < 1 or s.get('triangles', 0) <= 0:
            return stop('empty fabric geometry')
        if not f.get('plaza', {}).get('valid'):
            return stop('no well plaza')
        base.update(signature=f.get('signature'), cleared=s.get('cleared'), rebuilds=s.get('rebuilds'))
        cmd(world, 'Anastasis.Village.FabricReport')
        cmd(world, 'anastasis.Village.FabricClear 0')
        advance(3)
    elif step == 3 and waited > 2.0:
        s = status(world, 'clear_off')
        if s.get('cleared') != 0:
            return stop('FabricClear 0 kept %s instances cleared' % s.get('cleared'))
        if s.get('fabric', {}).get('signature') != base['signature']:
            return stop('FabricClear changed the geometry')
        cmd(world, 'anastasis.Village.FabricClear 1')
        advance(4)
    elif step == 4 and waited > 2.0:
        s = status(world, 'clear_on')
        if abs((s.get('cleared') or 0) - base['cleared']) > max(10, 0.05 * base['cleared']):
            return stop('cleared count not reproduced: %s vs %s' % (s.get('cleared'), base['cleared']))
        base['rebuilds'] = s.get('rebuilds')
        cmd(world, 'Anastasis.Village.FabricRebuild')
        advance(5)
    elif step == 5 and waited > 2.0:
        s = status(world, 'rebuild')
        if s.get('rebuilds', 0) <= base['rebuilds']:
            return stop('FabricRebuild did not rebuild')
        if s.get('fabric', {}).get('signature') != base['signature']:
            return stop('same village, different fabric: %s vs %s' % (s.get('fabric', {}).get('signature'), base['signature']))
        cmd(world, 'anastasis.Village.Fabric 0')
        advance(6)
    elif step == 6 and waited > 2.0:
        s = status(world, 'fabric_off')
        if s.get('actor'):
            return stop('Fabric 0 kept the actor')
        cmd(world, 'anastasis.Village.Fabric 1')
        advance(7)
    elif step == 7 and waited > 3.0:
        s = status(world, 'fabric_on_again')
        if not s.get('actor') or s.get('fabric', {}).get('signature') != base['signature']:
            return stop('Fabric 1 did not give back the same fabric')
        phase = 3
        les.editor_request_end_play()


handle = unreal.register_slate_post_tick_callback(tick)
