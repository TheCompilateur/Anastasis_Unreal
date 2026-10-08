"""Preuve PIE des corps 3D des habitants (VILLAGER_BODY_3D_001).

Lance par villager-body-pie.ps1 (editeur discret, rendu hors focus). Rien n'est sauve.
Sortie : ANASTASIS_VILLAGER_BODY_OUT (defaut Saved/VillagerEvidence/body).

PIE sur Lvl_AnastasisSlice SANS COMMANDE de scenario : le village du lancement, au rythme du jeu
(anastasis.Sim.TimeScale) -- la simulation n'est jamais gelee, l'animation doit jouer.

Mesures (pas des impressions) :
  - chaque habitant a un corps monte (has_body) ;
  - corps 3D montre a toute distance, aucune carte PNG en jeu ;
  - un habitant QUI MARCHE : sur ~3 s, ses deux pieds (os foot_l / foot_r, relatifs aux pieds de
    l'acteur, projetes sur son cap) balaient chacun plus de 20 cm -- les jambes marchent ;
    son cap suit sa direction de deplacement (ecart median < 30 degres) ;
  - un habitant A L'ARRET : sa main et sa tete bougent encore (respiration de l'idle), > 0,2 cm.
Prises : 01-marche-a..d (le marcheur suivi de profil, ~0,4 s d'ecart), 02-arret (face, 2,5 m),
03-groupe (vue de 15 m), 04-loin (120 m : toujours des corps 3D).
Une verification qui echoue echoue ; jamais de PASS raconte.
"""
import json
import math
import os
import time
from pathlib import Path

import unreal

ROOT = Path(unreal.Paths.project_dir())
OUT = Path(os.environ.get('ANASTASIS_VILLAGER_BODY_OUT', str(ROOT / 'Saved' / 'VillagerEvidence' / 'body')))
OUT.mkdir(parents=True, exist_ok=True)
SHOTS_DIR = ROOT / 'Saved' / 'Screenshots'
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
DBG = unreal.AnastasisSimulationDebugLibrary
les.load_level('/Game/Anastasis/Maps/Lvl_AnastasisSlice')
proof_camera = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).spawn_actor_from_class(
    unreal.CameraActor, unreal.Vector(0, 0, 1000), unreal.Rotator())
proof_camera.set_actor_label('VillagerBodyProofCamera')

t0 = time.monotonic()
state = {'phase': 0, 'camera': None, 'step': 0, 'pending': None, 'shots': [], 'checks': [],
         'failures': [], 'finished': False, 'walker': None, 'stander': None, 'track': None, 'begin': None}
handle = None


def log(msg):
    unreal.log('VILLAGER_BODY_PIE ' + msg)


def fail(msg):
    state['failures'].append(msg)
    unreal.log_warning('VILLAGER_BODY_PIE FAIL ' + msg)


def finish(reason):
    if state['finished']:
        return
    state['finished'] = True
    ok = not state['failures'] and len(state['shots']) == 7 and all(s['written'] for s in state['shots'])
    report = {'pass': ok, 'reason': reason, 'failures': state['failures'], 'shots': state['shots'], 'checks': state['checks']}
    (OUT / 'villager-body-pie.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    log(('PASS ' if ok else 'FAIL ') + reason + ('' if ok else ' ' + json.dumps(state['failures'])))
    unreal.unregister_slate_post_tick_callback(handle)
    unreal.SystemLibrary.quit_editor()


def cards(world):
    return json.loads(DBG.get_villager_cards(world) or '{}')


def actor_of(world, npc):
    for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.AnastasisVillagerVisual):
        if a.get_actor_label().startswith('Villager_%s_' % npc):
            return a
    return None


def bone(actor, name):
    comp = actor.get_component_by_class(unreal.SkeletalMeshComponent)
    p = comp.get_socket_location(name)
    f = actor.get_actor_location()
    return (p.x - f.x, p.y - f.y, p.z - f.z)


def look_at(position, target):
    cam = state['camera']
    cam.set_actor_location(position, False, True)
    cam.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(position, target), True)
    world = ues.get_game_world()
    pc = unreal.GameplayStatics.get_player_controller(world, 0) if world else None
    if pc and pc.get_view_target() != cam:
        pc.set_view_target_with_blend(cam, 0.0)


def frame(world, spec):
    """Cadre l'habitant de la prise a chaque tick : il marche, la camera le suit."""
    c = cards(world)
    x = next((v for v in c.get('villagers', []) if v['npc'] == spec['npc']), None)
    if not x:
        return None
    feet = unreal.Vector(x['x'], x['y'], x['z'])
    h = math.radians(x['heading'])
    fwd = (math.cos(h), math.sin(h))
    side = (-fwd[1], fwd[0])
    d, up, aim_z = spec['dist'], spec['up'], spec['aim_z']
    if spec['view'] == 'side':
        pos = unreal.Vector(feet.x + side[0] * d, feet.y + side[1] * d, feet.z + up)
    elif spec['view'] == 'front':
        pos = unreal.Vector(feet.x + fwd[0] * d, feet.y + fwd[1] * d, feet.z + up)
    else:  # 'oblique'
        pos = unreal.Vector(feet.x + (fwd[0] * 0.5 + side[0]) * d, feet.y + (fwd[1] * 0.5 + side[1]) * d, feet.z + up)
    look_at(pos, unreal.Vector(feet.x, feet.y, feet.z + aim_z))
    return x


def existing_shots():
    return set(SHOTS_DIR.rglob('*.png')) if SHOTS_DIR.exists() else set()


def shoot(name, spec):
    path = OUT / (name + '.png')
    if path.exists():
        path.unlink()
    state['pending'] = {'path': path, 'name': name, 'spec': spec, 'at': time.monotonic(), 'fired': False,
                        'before': existing_shots()}
    log('SHOT ' + name + ' ' + json.dumps(spec))


def pump_shot(world):
    """Rend True quand la prise en cours est ecrite (ou abandonnee)."""
    p = state['pending']
    frame(world, p['spec'])
    if not p['fired']:
        if time.monotonic() - p['at'] < p['spec'].get('settle', 0.6):
            return False
        cam = state['camera'].get_actor_location()
        pc = unreal.GameplayStatics.get_player_controller(world, 0)
        target = pc.get_view_target() if pc else None
        log('FIRE %s camera=(%.0f,%.0f,%.0f) view_target=%s' % (p['name'], cam.x, cam.y, cam.z,
            target.get_actor_label() if target else None))
        unreal.SystemLibrary.execute_console_command(world, 'Shot')
        p['fired'], p['at'] = True, time.monotonic()
        return False
    fresh = sorted(existing_shots() - p['before'], key=lambda f: f.stat().st_mtime)
    if fresh and time.monotonic() - p['at'] > 0.3:
        try:
            fresh[-1].replace(p['path'])
        except OSError:
            return False
    if p['path'].is_file() or time.monotonic() - p['at'] > 20:
        state['shots'].append({'file': p['path'].name, 'written': p['path'].is_file()})
        state['pending'] = None
        return True
    return False


def check_bodies(world, c, label):
    """Tous les habitants visibles gardent un corps 3D, meme au loin."""
    limit = 80.0 * 100.0  # ancien seuil, seulement pour separer les prises proches et lointaines
    cam = state['camera'].get_actor_location()
    wrong, near, far = [], 0, 0
    for x in c.get('villagers', []):
        if x['hidden']:
            continue
        if not x['has_body']:
            wrong.append('%s sans corps' % x['npc'])
            continue
        d = math.sqrt((x['x'] - cam.x) ** 2 + (x['y'] - cam.y) ** 2 + (x['z'] - cam.z) ** 2)
        near += d < limit
        far += d >= limit
        if not x['body']:
            wrong.append('%s a %.0f m : body=%s' % (x['npc'], d / 100.0, x['body']))
    rec = {'at': label, 'reference_distance_m': limit / 100.0, 'near_bodies': near, 'far_bodies': far, 'errors': wrong}
    state['checks'].append(rec)
    log('BODIES ' + json.dumps(rec))
    for e in wrong:
        fail(label + ': ' + e)
    return rec


def tick(dt):
    if time.monotonic() - t0 > 600:
        fail('wall timeout step=%d' % state['step'])
        finish('wall timeout')
        return
    if state['phase'] == 0 and time.monotonic() - t0 > 3:
        state['phase'] = 1
        les.editor_request_begin_play()
        return
    world = ues.get_game_world()
    if state['phase'] == 1:
        if not les.is_in_play_in_editor() or not world or DBG.get_simulation_time(world) < 0:
            return
        for cmd in ('anastasis.Village.Debug 0', 'anastasis.Village.Portraits 1', 'anastasis.Village.Bodies 2',
                    'showflag.Fog 0', 'r.MotionBlurQuality 0', 'anastasis.Sim.Speed 1'):
            unreal.SystemLibrary.execute_console_command(world, cmd)
        cams = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.CameraActor)
        state['camera'] = next((c for c in cams if c.get_actor_label() == 'VillagerBodyProofCamera'), None)
        if not state['camera']:
            fail('camera de preuve absente du monde PIE')
            finish('camera')
            return
        state['begin'] = time.monotonic()
        state['phase'] = 2
        return
    if state['phase'] != 2 or not world:
        return
    if state['pending'] is not None:
        if pump_shot(world):
            state['step'] += 1
        return

    c = cards(world)
    v = [x for x in c.get('villagers', []) if not x['hidden']]
    step = state['step']
    if step == 0:
        # Attendre des corps et un marcheur : un habitant ne decide que toutes les ~59 s reelles.
        if c.get('npcs', 0) == 0 or c.get('cards', 0) < c.get('npcs', 0):
            if time.monotonic() - state['begin'] > 120:
                fail('cartes %s / habitants %s apres 120 s' % (c.get('cards'), c.get('npcs')))
                finish('pas de village')
            return
        missing = [x['npc'] for x in c['villagers'] if not x['has_body']]
        if missing:
            fail('habitants sans corps 3D : %s' % missing)
            finish('corps absents')
            return
        walkers = [x for x in v if x['speed'] > 120.0]
        if not walkers:
            if time.monotonic() - state['begin'] > 240:
                fail('personne ne marche apres 240 s')
                finish('aucun marcheur')
            return
        state['walker'] = max(walkers, key=lambda x: x['speed'])['npc']
        log('WALKER %s %s' % (state['walker'], json.dumps(c)))
        state['track'] = {'t0': time.monotonic(), 'feet': {'foot_l': [], 'foot_r': []}, 'head_err': [], 'speeds': []}
        state['step'] = 1
    elif step == 1:
        # ~3 s de marche, camera de profil : les pieds de l'os, projetes sur le cap du corps.
        x = frame(world, {'npc': state['walker'], 'view': 'side', 'dist': 450, 'up': 140, 'aim_z': 95})
        a = actor_of(world, state['walker'])
        tr = state['track']
        if x and a and x['body']:
            h = math.radians(x['heading'])
            for name in ('foot_l', 'foot_r'):
                b = bone(a, name)
                tr['feet'][name].append(b[0] * math.cos(h) + b[1] * math.sin(h))
            tr['speeds'].append(x['speed'])
            last = tr.get('last')
            if last and x['speed'] > 120.0:
                dx, dy = x['x'] - last[0], x['y'] - last[1]
                if dx * dx + dy * dy > 1.0:
                    move = math.degrees(math.atan2(dy, dx))
                    tr['head_err'].append(abs((x['heading'] - move + 180.0) % 360.0 - 180.0))
            tr['last'] = (x['x'], x['y'])
        # Au moins 3 s ET 10 echantillons : sur une machine saturee (2 images/s, deuxieme run) 3 s n'en
        # donnent que 6. Plafond 30 s.
        if (time.monotonic() - tr['t0'] < 3.0 or len(tr['speeds']) < 10) and time.monotonic() - tr['t0'] < 30.0:
            return
        swing = {k: (max(s) - min(s)) if s else 0.0 for k, s in tr['feet'].items()}
        errs = sorted(tr['head_err'])
        median_err = errs[len(errs) // 2] if errs else None
        rec = {'at': 'marche', 'npc': state['walker'], 'samples': len(tr['speeds']),
               'foot_swing_cm': {k: round(s, 1) for k, s in swing.items()},
               'speed_cm_s': [round(min(tr['speeds']), 1), round(max(tr['speeds']), 1)] if tr['speeds'] else None,
               'heading_error_median_deg': round(median_err, 1) if median_err is not None else None}
        state['checks'].append(rec)
        log('MARCHE ' + json.dumps(rec))
        if len(tr['speeds']) < 10:
            fail('marche : %d echantillons seulement (corps cache ou habitant entre ?)' % len(tr['speeds']))
        for k, s in swing.items():
            if s < 20.0:
                fail('marche : %s balaie %.1f cm (< 20) -- les jambes ne marchent pas' % (k, s))
        if median_err is None or median_err > 30.0:
            fail('marche : cap a %s degres de la direction de marche' % median_err)
        state['step'] = 2
    elif 2 <= step <= 5:
        shoot('01-marche-' + 'abcd'[step - 2], {'npc': state['walker'], 'view': 'side', 'dist': 450, 'up': 140,
                                                'aim_z': 95, 'settle': 0.4})
    elif step == 6:
        # Un habitant VRAIMENT a l'arret (premier run : celui choisi s'etait remis en marche, la main
        # « bougeait » de 43 cm -- c'etait la marche, pas la respiration).
        tried = state.setdefault('tried', [])
        standers = [x for x in v if x['speed'] < 5.0 and x['npc'] not in tried]
        if not standers:
            state.setdefault('stand_wait', time.monotonic())
            if time.monotonic() - state['stand_wait'] > 150:
                fail('arret : aucun habitant immobile en 150 s')
                state['step'] = 8
            return
        s = standers[0]
        tried.append(s['npc'])
        state['stander'] = s['npc']
        a = actor_of(world, s['npc'])
        state['track'] = {'t0': time.monotonic(), 'hand': [], 'head': [], 'speed': [], 'actor': a}
        state['step'] = 7
    elif step == 7:
        tr = state['track']
        x = frame(world, {'npc': state['stander'], 'view': 'front', 'dist': 250, 'up': 160, 'aim_z': 150})
        if tr['actor'] and x and x['body']:
            tr['hand'].append(bone(tr['actor'], 'hand_r'))
            tr['head'].append(bone(tr['actor'], 'head'))
            tr['speed'].append(x['speed'])
        if time.monotonic() - tr['t0'] < 2.5:
            return
        if not tr['speed'] or max(tr['speed']) > 20.0:
            # Il s'est remis en marche pendant la mesure : un autre, au plus trois essais.
            log('ARRET %s s est remis en marche (%.0f cm/s), autre essai' % (state['stander'], max(tr['speed'] or [0])))
            if len(state['tried']) >= 3:
                fail('arret : trois habitants se sont remis en marche pendant la mesure')
                state['step'] = 8
            else:
                state['step'] = 6
            return
        def spread(pts):
            if not pts:
                return 0.0
            return max(max(p[i] for p in pts) - min(p[i] for p in pts) for i in range(3))
        rec = {'at': 'arret', 'npc': state['stander'], 'max_speed_cm_s': round(max(tr['speed']), 1),
               'hand_motion_cm': round(spread(tr['hand']), 2), 'head_motion_cm': round(spread(tr['head']), 2),
               'samples': len(tr['hand'])}
        state['checks'].append(rec)
        log('ARRET ' + json.dumps(rec))
        if rec['hand_motion_cm'] < 0.2 or rec['head_motion_cm'] < 0.2:
            fail('arret : main %.2f cm, tete %.2f cm -- le corps est fige' % (rec['hand_motion_cm'], rec['head_motion_cm']))
        shoot('02-arret', {'npc': state['stander'], 'view': 'front', 'dist': 250, 'up': 160, 'aim_z': 150, 'settle': 0.8})
    elif step == 8:
        shoot('03-groupe', {'npc': state['walker'], 'view': 'oblique', 'dist': 1300, 'up': 500, 'aim_z': 100, 'settle': 1.0})
    elif step == 9:
        check_bodies(world, c, '03-groupe')
        shoot('04-loin', {'npc': state['walker'], 'view': 'oblique', 'dist': 11000, 'up': 2500, 'aim_z': 100, 'settle': 1.5})
    elif step == 10:
        rec = check_bodies(world, c, '04-loin')
        if rec['far_bodies'] == 0:
            fail('04-loin : aucun habitant au-dela de la distance de reference')
        finish('corps montes a toute distance, jambes qui marchent, idle vivant')
        return


def guarded(dt):
    try:
        tick(dt)
    except Exception as exc:
        import traceback
        unreal.log_error(traceback.format_exc())
        fail(str(exc))
        finish('exception')


handle = unreal.register_slate_post_tick_callback(guarded)
