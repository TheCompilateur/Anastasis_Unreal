"""Preuve PIE des portraits d'habitants (VILLAGER_PNG_001) : la simulation existante, ses habitants
existants, et une carte portrait par habitant -- rien d'autre.

Lance par villager-pie.ps1 (editeur discret, rendu hors focus). Rien n'est sauve.
Sortie : ANASTASIS_VILLAGER_PIE_OUT (defaut Saved/VillagerEvidence/pie).

PIE sur Lvl_AnastasisSlice, puis `Anastasis.Village.FirstWell 12` : douze habitants autour du
premier puits. A chaque echantillon (GetVillagerCards) :
  - une carte par habitant simule, ni plus ni moins ;
  - portraits tous differents (12 <= 24 portraits attribuables) ;
  - adultes et aines seulement (la simulation n'a pas d'enfants) ;
  - carte cachee <=> habitant dedans.
Prises : 01-proche (un habitant, carte seule), 02-debug (sa sphere de simulation + sa carte :
memes pieds), 03-voisins (lui et son plus proche voisin) ; puis `Anastasis.Village.RemoveNpc` : la carte de l'habitant retire disparait.
Une verification qui echoue echoue ; jamais de PASS raconte.
"""
import json
import os
import time
from pathlib import Path

import unreal

ROOT = Path(unreal.Paths.project_dir())
OUT = Path(os.environ.get('ANASTASIS_VILLAGER_PIE_OUT', str(ROOT / 'Saved' / 'VillagerEvidence' / 'pie')))
OUT.mkdir(parents=True, exist_ok=True)
NPC_COUNT = int(os.environ.get('ANASTASIS_VILLAGER_PIE_NPCS', '12'))
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
DBG = unreal.AnastasisSimulationDebugLibrary
les.load_level('/Game/Anastasis/Maps/Lvl_AnastasisSlice')
proof_camera = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).spawn_actor_from_class(
    unreal.CameraActor, unreal.Vector(0, 0, 1000), unreal.Rotator())
proof_camera.set_actor_label('VillagerProofCamera')

t0 = time.monotonic()
state = {'phase': 0, 'camera': None, 'pending': None, 'shots': [], 'checks': [], 'seeded_at': None,
         'step': 0, 'removed': None, 'finished': False, 'failures': []}
handle = None
SHOTS_DIR = ROOT / 'Saved' / 'Screenshots'


def log(msg):
    unreal.log('VILLAGER_PIE ' + msg)


def finish(reason):
    if state['finished']:
        return
    state['finished'] = True
    ok = not state['failures'] and all(s['written'] for s in state['shots']) and len(state['shots']) == 3
    report = {'pass': ok, 'reason': reason, 'failures': state['failures'], 'shots': state['shots'], 'checks': state['checks']}
    (OUT / 'villager-pie.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    log(('PASS ' if ok else 'FAIL ') + reason + ('' if ok else ' ' + json.dumps(state['failures'])))
    unreal.unregister_slate_post_tick_callback(handle)
    unreal.SystemLibrary.quit_editor()


def cards(world):
    return json.loads(DBG.get_villager_cards(world) or '{}')


def check(c, label):
    """Les invariants du sens unique simulation -> cartes."""
    v = c.get('villagers', [])
    looks = [x['look'] for x in v]
    errs = []
    if c.get('cards') != c.get('npcs'):
        errs.append('cartes %s != habitants %s' % (c.get('cards'), c.get('npcs')))
    if any(not l for l in looks):
        errs.append('habitant sans carte')
    if len(set(looks)) != len(looks) and len(looks) <= 24:
        errs.append('portraits en double: %s' % sorted(looks))
    if any(('_Adult_' not in l and '_Elder_' not in l) for l in looks if l):
        errs.append('portrait hors adultes/aines: %s' % [l for l in looks if '_Child_' in l])
    if any(x['hidden'] != x['inside'] for x in v):
        errs.append('carte cachee != habitant dedans')
    state['checks'].append({'at': label, 'npcs': c.get('npcs'), 'cards': c.get('cards'), 'errors': errs})
    for e in errs:
        state['failures'].append(label + ': ' + e)
    return not errs


def aim(target, offset):
    position = target + offset
    cam = state['camera']
    cam.set_actor_location(position, False, True)
    cam.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(position, target), True)


def existing_shots():
    return set(SHOTS_DIR.rglob('*.png')) if SHOTS_DIR.exists() else set()


def shoot(world, name):
    path = OUT / (name + '.png')
    if path.exists():
        path.unlink()
    unreal.SystemLibrary.execute_console_command(world, 'anastasis.Sim.Speed 0')
    state['pending'] = {'path': path, 'at': time.monotonic(), 'fired': False, 'before': existing_shots(), 'name': name}
    log('SHOT ' + name)


def tick(dt):
    if time.monotonic() - t0 > 360:
        finish('wall timeout step=%d' % state['step'])
        return
    if state['phase'] == 0 and time.monotonic() - t0 > 3:
        state['phase'] = 1
        les.editor_request_begin_play()
        return
    world = ues.get_game_world()
    if state['phase'] == 1:
        if not les.is_in_play_in_editor() or not world or DBG.get_simulation_time(world) < 0:
            return
        for cmd in ('anastasis.Village.Debug 0', 'anastasis.Village.Portraits 1', 'showflag.Fog 0',
                    'r.MotionBlurQuality 0', 'anastasis.Sim.Speed 2', 'Anastasis.Village.FirstWell %d' % NPC_COUNT):
            unreal.SystemLibrary.execute_console_command(world, cmd)
        cams = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.CameraActor)
        state['camera'] = next((c for c in cams if c.get_actor_label() == 'VillagerProofCamera'), None)
        pc = unreal.GameplayStatics.get_player_controller(world, 0)
        if state['camera'] and pc:
            pc.set_view_target_with_blend(state['camera'], 0.0)
        state['seeded_at'] = time.monotonic()
        state['phase'] = 2
        return
    if state['phase'] != 2 or not world:
        return

    p = state['pending']
    if p is not None:
        if not p['fired']:
            if time.monotonic() - p['at'] < 0.8:
                return
            unreal.SystemLibrary.execute_console_command(world, 'Shot')
            p['fired'], p['at'] = True, time.monotonic()
            return
        fresh = sorted(existing_shots() - p['before'], key=lambda f: f.stat().st_mtime)
        if fresh and time.monotonic() - p['at'] > 1.0:
            try:
                fresh[-1].replace(p['path'])
            except OSError:
                return
        if p['path'].is_file() or time.monotonic() - p['at'] > 20:
            state['shots'].append({'file': p['path'].name, 'written': p['path'].is_file()})
            state['pending'] = None
            unreal.SystemLibrary.execute_console_command(world, 'anastasis.Sim.Speed 2')
        return

    c = cards(world)
    if not c or 'villagers' not in c:
        return
    if c['npcs'] == 0:
        if time.monotonic() - state['seeded_at'] > 20:
            finish('FirstWell n\'a pose aucun habitant')
        return
    # Laisser les habitants se disperser un peu avant la premiere prise.
    if time.monotonic() - state['seeded_at'] < 6:
        return
    step = state['step']
    v = c['villagers']
    # Cadrer UN habitant, pas le centre du groupe : FirstWell les disperse sur ~14 tuiles (280 m),
    # le barycentre tombe dans le vide et a 50 m une carte de 1,7 m fait dix pixels (premier run).
    outside = [x for x in v if not x['hidden']] or v
    near = sorted(outside, key=lambda x: (x['x'] - outside[0]['x']) ** 2 + (x['y'] - outside[0]['y']) ** 2)
    def chest(x):
        return unreal.Vector(x['x'], x['y'], x['z'] + 110)
    if step == 0:
        check(c, '01-proche')
        log('CARDS ' + json.dumps(c))
        aim(chest(near[0]), unreal.Vector(-260, -340, 40))
        log('FRAME 01-proche ' + near[0]['npc'] + ' ' + near[0]['look'])
        shoot(world, '01-proche')
    elif step == 1:
        check(c, '02-debug')
        unreal.SystemLibrary.execute_console_command(world, 'anastasis.Village.Debug 1')
        aim(chest(near[0]), unreal.Vector(-420, -540, 160))
        log('FRAME 02-debug ' + near[0]['npc'])
        shoot(world, '02-debug')
    elif step == 2:
        unreal.SystemLibrary.execute_console_command(world, 'anastasis.Village.Debug 0')
        check(c, '03-voisins')
        # Deux habitants : le premier et son plus proche voisin, cadres ensemble.
        a, b = near[0], near[1] if len(near) > 1 else near[0]
        mid = unreal.Vector((a['x'] + b['x']) / 2, (a['y'] + b['y']) / 2, (a['z'] + b['z']) / 2 + 110)
        span = max(600.0, ((a['x'] - b['x']) ** 2 + (a['y'] - b['y']) ** 2) ** 0.5)
        aim(mid, unreal.Vector(-0.6 * span, -0.8 * span, 0.25 * span))
        log('FRAME 03-voisins %s %s span=%.0f' % (a['npc'], b['npc'], span))
        shoot(world, '03-voisins')
    elif step == 3:
        victim = v[0]['npc']
        state['removed'] = {'npc': victim, 'look': v[0]['look'], 'before': c['cards']}
        unreal.SystemLibrary.execute_console_command(world, 'Anastasis.Village.RemoveNpc ' + victim)
        log('REMOVE ' + victim)
    elif step == 4:
        r = state['removed']
        gone = all(x['npc'] != r['npc'] for x in v)
        ok = gone and c['cards'] == r['before'] - 1 and check(c, '04-retrait')
        state['checks'].append({'at': '04-retrait', 'removed': r, 'cards_after': c['cards'], 'ok': ok})
        if not ok:
            state['failures'].append('04-retrait: carte de %s encore presente ou compte faux (%d -> %d)' % (r['npc'], r['before'], c['cards']))
        finish('%d habitants, %d cartes, portraits distincts, retrait suivi' % (c['npcs'] + 1, r['before']))
        return
    state['step'] += 1


def guarded(dt):
    try:
        tick(dt)
    except Exception as exc:
        import traceback
        unreal.log_error(traceback.format_exc())
        state['failures'].append(str(exc))
        finish('exception')


handle = unreal.register_slate_post_tick_callback(guarded)
