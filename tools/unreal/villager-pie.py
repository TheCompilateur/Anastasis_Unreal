"""Preuve PIE des portraits d'habitants (VILLAGER_PNG_001) : la simulation existante, ses habitants
existants, et une carte portrait par habitant -- rien d'autre.

Lance par villager-pie.ps1 (editeur discret, rendu hors focus). Rien n'est sauve.
Sortie : ANASTASIS_VILLAGER_PIE_OUT (defaut Saved/VillagerEvidence/pie).

PIE sur Lvl_AnastasisSlice SANS AUCUNE COMMANDE : le village du lancement
(`anastasis.Village.StartVillagers`, 12) doit deja montrer ses habitants, une carte chacun (00-demarrage).
Puis `Anastasis.Village.FirstWell 12` : le scenario REMPLACE ce village (12 habitants, pas 24).
A chaque echantillon (GetVillagerCards) :
  - une carte par habitant simule, ni plus ni moins ;
  - portraits tous differents (12 <= 24 portraits attribuables) ;
  - adultes et aines debout seulement, et du METIER simule de l'habitant (l'objet peint est celui
    du metier : fourche ou panier pour un fermier, mains vides sans metier) ;
  - carte cachee <=> habitant dedans.
Prises : 00-demarrage (le village du lancement), 01-proche (un habitant, carte seule), 02-debug (sa sphere de simulation + sa carte :
memes pieds), 03-voisins (lui et son plus proche voisin), 05-fermier (`FirstFarmer 1`) ; puis `Anastasis.Village.RemoveNpc` : la carte de l'habitant retire disparait.
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
START_COUNT = 12  # anastasis.Village.StartVillagers par defaut
# Les portraits attribuables : debout, adultes et aines (ecrit par villager-png.py sheets).
EXTRACT = ROOT / 'SourceArt' / 'Characters' / 'villager-extract.json'
# Portrait -> metiers simules dont il porte l'objet (debout, adultes et aines).
JOBS = {p['id']: set(p.get('jobs', [])) for p in json.loads(EXTRACT.read_text(encoding='utf-8'))['people']
        if p.get('in_game', True) and not p['category'].startswith('Child')}
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
    ok = not state['failures'] and all(s['written'] for s in state['shots']) and len(state['shots']) == 5
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
    wrong = ['%s(%s)=%s' % (x['npc'], x['job'], x['look']) for x in v if x['look'] and x['job'] not in JOBS.get(x['look'], set())]
    if wrong:
        errs.append('portrait d un autre metier, assis ou enfant: %s' % wrong)
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
    # Le controleur reprend la vue de son pion quand celui-ci reapparait (02-debug filmait la vue du
    # joueur, identique d'un run a l'autre) : on reimpose la camera de preuve a chaque cadrage.
    world = ues.get_game_world()
    pc = unreal.GameplayStatics.get_player_controller(world, 0) if world else None
    if pc and pc.get_view_target() != cam:
        log('VIEW_TARGET repris par le controleur, camera de preuve reimposee')
        pc.set_view_target_with_blend(cam, 0.0)


def existing_shots():
    return set(SHOTS_DIR.rglob('*.png')) if SHOTS_DIR.exists() else set()


def shoot(world, name, npc=None, offset=None):
    """Fige la simulation, PUIS cadre : a la vitesse de la simulation (4 tuiles/s, 20 m la tuile), un
    habitant qui marche quitte le cadre entre la lecture de sa position et le gel (00-demarrage, premier
    run : image vide). La position est relue au tick suivant le gel."""
    path = OUT / (name + '.png')
    if path.exists():
        path.unlink()
    unreal.SystemLibrary.execute_console_command(world, 'anastasis.Sim.Speed 0')
    state['pending'] = {'path': path, 'at': time.monotonic(), 'fired': False, 'before': existing_shots(), 'name': name,
                        'npc': npc, 'offset': offset, 'aimed': npc is None}
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
        # Aucune commande de scenario ici : le village doit deja etre la.
        for cmd in ('anastasis.Village.Debug 0', 'anastasis.Village.Portraits 1', 'showflag.Fog 0',
                    'r.MotionBlurQuality 0', 'anastasis.Sim.Speed 2'):
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
        if not p['aimed']:
            frozen = cards(world)
            x = next((v for v in frozen.get('villagers', []) if v['npc'] == p['npc']), None)
            if x:
                aim(unreal.Vector(x['x'], x['y'], x['z'] + 110), p['offset'])
            cam = state['camera'].get_actor_location() if state['camera'] else None
            log('AIM %s npc=%s pos=%s hidden=%s camera=%s' % (p['name'], p['npc'],
                (round(x['x']), round(x['y']), round(x['z'])) if x else None, x['hidden'] if x else None,
                (round(cam.x), round(cam.y), round(cam.z)) if cam else None))
            p['aimed'], p['at'] = True, time.monotonic()
            return
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
            state['failures'].append('aucun habitant (etape %d)' % state['step'])
            finish('village vide')
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
        check(c, '00-demarrage')
        if c['npcs'] != START_COUNT:
            state['failures'].append('00-demarrage: %d habitants au lancement, %d attendus' % (c['npcs'], START_COUNT))
        log('START ' + json.dumps(c))
        log('FRAME 00-demarrage ' + near[0]['npc'] + ' ' + near[0]['look'])
        shoot(world, '00-demarrage', near[0]['npc'], unreal.Vector(-300, -400, 60))
    elif step == 1:
        unreal.SystemLibrary.execute_console_command(world, 'Anastasis.Village.FirstWell %d' % NPC_COUNT)
        log('SCENARIO FirstWell %d' % NPC_COUNT)
        state['seeded_at'] = time.monotonic()
    elif step == 2:
        if c['npcs'] != NPC_COUNT:
            state['failures'].append('01-scenario: %d habitants apres FirstWell %d (le village du lancement aurait du etre remplace)' % (c['npcs'], NPC_COUNT))
        check(c, '01-proche')
        log('CARDS ' + json.dumps(c))
        log('FRAME 01-proche ' + near[0]['npc'] + ' ' + near[0]['look'])
        shoot(world, '01-proche', near[0]['npc'], unreal.Vector(-260, -340, 40))
    elif step == 3:
        check(c, '02-debug')
        unreal.SystemLibrary.execute_console_command(world, 'anastasis.Village.Debug 1')
        log('FRAME 02-debug ' + near[0]['npc'])
        shoot(world, '02-debug', near[0]['npc'], unreal.Vector(-420, -540, 160))
    elif step == 4:
        unreal.SystemLibrary.execute_console_command(world, 'anastasis.Village.Debug 0')
        check(c, '03-voisins')
        # Deux habitants : le premier et son plus proche voisin, cadres ensemble.
        a, b = near[0], near[1] if len(near) > 1 else near[0]
        mid = unreal.Vector((a['x'] + b['x']) / 2, (a['y'] + b['y']) / 2, (a['z'] + b['z']) / 2 + 110)
        span = max(600.0, ((a['x'] - b['x']) ** 2 + (a['y'] - b['y']) ** 2) ** 0.5)
        aim(mid, unreal.Vector(-0.6 * span, -0.8 * span, 0.25 * span))
        log('FRAME 03-voisins %s %s span=%.0f' % (a['npc'], b['npc'], span))
        shoot(world, '03-voisins')
    elif step == 5:
        victim = v[0]['npc']
        state['removed'] = {'npc': victim, 'look': v[0]['look'], 'before': c['cards']}
        unreal.SystemLibrary.execute_console_command(world, 'Anastasis.Village.RemoveNpc ' + victim)
        log('REMOVE ' + victim)
    elif step == 6:
        r = state['removed']
        gone = all(x['npc'] != r['npc'] for x in v)
        ok = gone and c['cards'] == r['before'] - 1 and check(c, '04-retrait')
        state['checks'].append({'at': '04-retrait', 'removed': r, 'cards_after': c['cards'], 'ok': ok})
        if not ok:
            state['failures'].append('04-retrait: carte de %s encore presente ou compte faux (%d -> %d)' % (r['npc'], r['before'], c['cards']))
        # Un fermier embauche : son portrait doit porter un outil de fermier.
        unreal.SystemLibrary.execute_console_command(world, 'Anastasis.Village.FirstFarmer 1')
        log('SCENARIO FirstFarmer 1')
        state['seeded_at'] = time.monotonic()
    elif step == 7:
        farmers = [x for x in v if x['job'] == 'farmer']
        if not farmers:
            if time.monotonic() - state['seeded_at'] < 15:
                return
            state['failures'].append('05-fermier: aucun habitant de metier farmer apres FirstFarmer')
            finish('pas de fermier')
            return
        check(c, '05-fermier')
        x = farmers[0]
        log('FRAME 05-fermier %s %s' % (x['npc'], x['look']))
        shoot(world, '05-fermier', x['npc'], unreal.Vector(-260, -340, 40))
    elif step == 8:
        farmers = [x for x in v if x['job'] == 'farmer']
        finish('%d habitants, portraits distincts et du bon metier, retrait suivi, fermier %s en %s' % (
            state['removed']['before'], farmers[0]['npc'] if farmers else '-', farmers[0]['look'] if farmers else '-'))
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
