"""Preuve PIE de la morphogenese du peuplement (SETTLEMENT_MORPHOGENESIS_001).

Lance par settlement-morphogenesis-pie.ps1 (editeur discret) ou par editor-batch (registre :
settlement-morphogenesis-pie). Rien n'est sauve. Sortie : ANASTASIS_MORPH_OUT (defaut
Saved/SettlementEvidence/pie). ANASTASIS_MORPH_KEEP=1 laisse l'editeur ouvert apres les prises.

Le village d'ouverture tel quel (aucun scenario) : le site choisi par la geographie, le puits, le foyer
du cultivateur et son grenier, le chantier des batisseurs. Le ciel est epingle, la pluie coupee.

  1. Jour 0 : etat (JSON) et prise du village.
  2. Le temps avance (`Anastasis.Sim.Advance`, ANASTASIS_MORPH_DAYS jours, defaut 36) : les habitants vivent,
     marchent, fondent ; la simulation compte le passage et fixe les sentiers (ecart n°42).
  3. Etat (JSON) et prises aux memes cameras, puis au sentier, aux portes, aux maisons fondees.

Mesures (pas des impressions) :
  - des passages ont ete comptes et au moins un sentier est ne ; chaque sentier est ne d'un passage >= 14 ;
  - la bande rendue existe (segments > 0) ;
  - au moins une maison a une forme FIXEE par son fondateur, et chaque corps affiche est le programme
    de sa biographie (la projection dit la verite de l'histoire) ;
  - la patine d'un batiment acheve depuis des jours depasse celle du premier jour.
Une verification qui echoue echoue ; jamais de PASS raconte.
"""
import json
import math
import os
import time
from pathlib import Path

import unreal

ROOT = Path(unreal.Paths.project_dir())
OUT = Path(os.environ.get('ANASTASIS_MORPH_OUT', str(ROOT / 'Saved' / 'SettlementEvidence' / 'pie')))
OUT.mkdir(parents=True, exist_ok=True)
DAYS = int(os.environ.get('ANASTASIS_MORPH_DAYS', '36'))
KEEP = os.environ.get('ANASTASIS_MORPH_KEEP', '0') == '1'
HOUR = os.environ.get('ANASTASIS_MORPH_HOUR', '9.5')
SHOTS_DIR = ROOT / 'Saved' / 'Screenshots'
KIT = json.loads((ROOT / 'docs' / 'unreal' / 'architecture' / 'architecture-kit-001.json').read_text(encoding='utf-8'))
ARCH = {b['name']: b for b in KIT['buildings']}
ARCH_BY_ID = {'house_poor': 'SM_Arch_House_Poor_01', 'house_medium': 'SM_Arch_House_Medium_01', 'house_farm': 'SM_Arch_House_Farm_01',
              'storehouse': 'SM_Arch_Storehouse_01', 'well': 'SM_Arch_Well_01'}
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
DBG = unreal.AnastasisSimulationDebugLibrary
les.load_level('/Game/Anastasis/Maps/Lvl_AnastasisSlice')
cam0 = eas.spawn_actor_from_class(unreal.CameraActor, unreal.Vector(0, 0, 1000), unreal.Rotator())
cam0.set_actor_label('MorphProofCamera')
cam0.camera_component.set_editor_property('field_of_view', 70.0)
cam0.camera_component.set_editor_property('constrain_aspect_ratio', False)
MANNY = unreal.load_asset('/Game/Characters/Mannequins/Meshes/SKM_Manny')
men_labels = []
for k in range(2):
    m = eas.spawn_actor_from_class(unreal.SkeletalMeshActor, unreal.Vector(0, 0, -100000 - k * 500), unreal.Rotator())
    m.set_actor_label('MorphMannequin_%d' % k)
    if MANNY:
        m.skeletal_mesh_component.set_skeletal_mesh_asset(MANNY)
    m.set_actor_scale3d(unreal.Vector(.93, .93, .93))
    men_labels.append(m.get_actor_label())

t0 = time.monotonic()
state = {'phase': 0, 'pending': None, 'shots': [], 'failures': [], 'checks': [], 'finished': False, 'camera': None,
         'men': [], 'queue': [], 'wait': 0.0, 'days_done': 0, 'before': None, 'after': None, 'center': None}
handle = None


def log(msg):
    unreal.log('MORPH_PIE ' + msg)


def fail(msg):
    state['failures'].append(msg)
    unreal.log_warning('MORPH_PIE FAIL ' + msg)


def cmd(world, c):
    unreal.SystemLibrary.execute_console_command(world, c)


def status(world):
    return json.loads(DBG.get_settlement_status(world) or '{}')


def finish(reason):
    if state['finished']:
        return
    state['finished'] = True
    ok = not state['failures'] and state['shots'] and all(s['written'] for s in state['shots'])
    report = {'pass': bool(ok), 'reason': reason, 'failures': state['failures'], 'shots': state['shots'],
              'checks': state['checks'], 'before': state['before'], 'after': state['after']}
    (OUT / 'settlement-morphogenesis-pie.json').write_text(json.dumps(report, indent=1), encoding='utf-8')
    log(('PASS ' if ok else 'FAIL ') + reason + ('' if ok else ' ' + json.dumps(state['failures'])))
    unreal.unregister_slate_post_tick_callback(handle)
    world = ues.get_game_world()
    if KEEP:
        log('KEEP editeur garde ouvert (ANASTASIS_MORPH_KEEP=1) : PIE et camera restent en place')
        return
    if world:
        cmd(world, 'anastasis.Sim.Warp 1')
    unreal.SystemLibrary.quit_editor()


def look(pos, target):
    cam = state['camera']
    cam.set_actor_location(pos, False, True)
    cam.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(pos, target), True)
    world = ues.get_game_world()
    pc = unreal.GameplayStatics.get_player_controller(world, 0) if world else None
    if pc and pc.get_view_target() != cam:
        pc.set_view_target_with_blend(cam, 0.0)


def place_man(i, pos, yaw):
    if i < len(state['men']) and state['men'][i]:
        state['men'][i].set_actor_location(pos, False, True)
        state['men'][i].set_actor_rotation(unreal.Rotator(0, 0, yaw - 90), True)


def park_men():
    for k, m in enumerate(state['men']):
        if m:
            m.set_actor_location(unreal.Vector(0, 0, -100000 - k * 500), False, True)


def existing_shots():
    return set(SHOTS_DIR.rglob('*.png')) if SHOTS_DIR.exists() else set()


def shoot(name, setup):
    path = OUT / (name + '.png')
    if path.exists():
        path.unlink()
    state['pending'] = {'path': path, 'name': name, 'at': time.monotonic(), 'fired': False, 'setup': setup,
                        'before': existing_shots()}
    setup()


def pump_shot(world):
    p = state['pending']
    if not p['fired']:
        if time.monotonic() - p['at'] < 1.5:
            return False
        cmd(world, 'Shot')
        p['fired'], p['at'] = True, time.monotonic()
        return False
    fresh = sorted(existing_shots() - p['before'], key=lambda f: f.stat().st_mtime)
    if fresh and time.monotonic() - p['at'] > 0.4:
        try:
            fresh[-1].replace(p['path'])
        except OSError:
            return False
    if not p['path'].is_file() and time.monotonic() - p['at'] > 25 and not p.get('retried'):
        p['retried'], p['fired'], p['at'], p['before'] = True, False, time.monotonic(), existing_shots()
        p['setup']()
        return False
    if p['path'].is_file() or time.monotonic() - p['at'] > 25:
        state['shots'].append({'file': p['path'].name, 'written': p['path'].is_file()})
        log('SHOT %s written=%s' % (p['name'], p['path'].is_file()))
        state['pending'] = None
        return True
    return False


def actor_of(world, bid):
    for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.AnastasisVillageBuilding):
        if a.get_actor_label().endswith('_' + bid):
            return a
    return None


def local_to_world(world, b, local):
    a = actor_of(world, b['id'])
    if not a:
        return unreal.Vector(b['x'] + local[0], b['y'] + local[1], b['z'] + local[2])
    p = a.get_actor_transform().transform_location(unreal.Vector(local[0], local[1], local[2]))
    return unreal.Vector(p.x, p.y, p.z + b['pad'])


def overview_cameras(s):
    xs = [b['x'] for b in s['buildings'] if b['x'] or b['y']]
    ys = [b['y'] for b in s['buildings'] if b['x'] or b['y']]
    zs = [b['z'] + b['pad'] for b in s['buildings'] if b['x'] or b['y']]
    for r in s['roads']:
        xs.append(r['wx']); ys.append(r['wy']); zs.append(r['wz'])
    c = unreal.Vector(sum(xs) / len(xs), sum(ys) / len(ys), sum(zs) / len(zs))
    span = max(4000.0, max(xs) - min(xs), max(ys) - min(ys))
    return c, span


def plan_after(world, s):
    plan = []
    c, span = state['center']
    plan.append(('01-village-apres', lambda: look(unreal.Vector(c.x + span * .55, c.y + span * .45, c.z + span * .32), c)))
    plan.append(('02-village-haut', lambda: look(unreal.Vector(c.x - span * .15, c.y - span * .5, c.z + span * .9), c)))

    def debug_view():
        cmd(world, 'anastasis.Village.Debug 1')
        look(unreal.Vector(c.x - span * .15, c.y - span * .5, c.z + span * .9), c)
    plan.append(('03-passage-debug', debug_view))
    roads = s['roads']
    houses = [b for b in s['buildings'] if b['type'] == 'house' and b['variant'] in ARCH_BY_ID]
    if roads:
        # Au milieu du sentier le plus ancien, a hauteur d'homme, vers la maison la plus proche.
        r = sorted(roads, key=lambda r: r['day'])[0]
        p = unreal.Vector(r['wx'], r['wy'], r['wz'])
        tgt = min(houses, key=lambda b: (b['x'] - p.x) ** 2 + (b['y'] - p.y) ** 2) if houses else None
        aim = unreal.Vector(tgt['x'], tgt['y'], tgt['z'] + tgt['pad'] + 250) if tgt else unreal.Vector(c.x, c.y, c.z + 200)
        d = unreal.Vector(aim.x - p.x, aim.y - p.y, 0)
        L = max(1.0, math.hypot(d.x, d.y))
        eye = unreal.Vector(p.x - d.x / L * 900, p.y - d.y / L * 900, p.z + 170)

        def sentier():
            cmd(world, 'anastasis.Village.Debug 0')
            park_men()
            place_man(0, unreal.Vector(p.x, p.y, p.z), math.degrees(math.atan2(d.y, d.x)) + 90)
            look(eye, aim)
        plan.append(('04-sentier-170cm', sentier))
    for k, b in enumerate(sorted(houses, key=lambda b: (not b['fixed'], b['id']))[:3]):
        spec = ARCH[ARCH_BY_ID[b['variant']]]
        lo, hi = spec['bounds']
        sp = max(hi[0] - lo[0], hi[1] - lo[1])
        e = spec['entry']

        def maison(b=b, lo=lo, hi=hi, sp=sp, e=e):
            cmd(world, 'anastasis.Village.Debug 0')
            park_men()
            place_man(0, local_to_world(world, b, (e[0], e[1], 0)), b['yaw'] + 270)
            look(local_to_world(world, b, (sp * .45, hi[1] + sp * .75, 170)), local_to_world(world, b, ((lo[0] + hi[0]) / 2, (lo[1] + hi[1]) / 2, hi[2] * .35)))
        plan.append(('%02d-maison-%s-%s' % (5 + k, b['id'], b['variant']), maison))
    return plan


def tick(dt):
    if time.monotonic() - t0 > 1800:
        fail('wall timeout phase=%d' % state['phase'])
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
        for c in ('anastasis.Village.Debug 0', 'r.MotionBlurQuality 0', 'anastasis.Sky.Clock 0', 'anastasis.Sky.Hour ' + HOUR,
                  'anastasis.Weather.Rain 0', 'anastasis.Sky.Weather 0'):
            cmd(world, c)
        cams = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.CameraActor)
        state['camera'] = next((x for x in cams if x.get_actor_label() == 'MorphProofCamera'), None)
        sk = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.SkeletalMeshActor)
        state['men'] = [next((m for m in sk if m.get_actor_label() == lab), None) for lab in men_labels]
        if not state['camera']:
            fail('camera de preuve absente')
            finish('camera')
            return
        state['wait'] = time.monotonic()
        state['phase'] = 2
        return
    if not world:
        return
    if state['pending'] is not None:
        pump_shot(world)
        return
    if state['phase'] == 2:
        # Le village d'ouverture se pose quand le terrain est pret (site choisi par la geographie).
        s = status(world)
        if len(s.get('buildings', [])) < 2:
            if time.monotonic() - state['wait'] > 180:
                fail('village d ouverture absent apres 180 s')
                finish('pas de village')
            return
        if time.monotonic() - state['wait'] < 6:
            return
        state['before'] = s
        state['center'] = overview_cameras(s)
        (OUT / 'status-j0.json').write_text(json.dumps(s, indent=1), encoding='utf-8')
        log('AVANT ' + json.dumps({'day': s['day'], 'buildings': [(b['id'], b['type'], b['program'], b['owner']) for b in s['buildings']],
                                  'passages': s['passages'], 'roads': len(s['roads'])}))
        c, span = state['center']
        shoot('00-village-j%d' % s['day'], lambda: look(unreal.Vector(c.x + span * .55, c.y + span * .45, c.z + span * .32), c))
        state['phase'] = 3
        return
    if state['phase'] == 3:
        # Le temps passe par sauts de 6 jours : la vie et le passage se jouent dans la simulation.
        if state['days_done'] >= DAYS:
            state['phase'] = 4
            state['wait'] = time.monotonic()
            return
        step = min(6, DAYS - state['days_done'])
        cmd(world, 'Anastasis.Sim.Advance %dd' % step)
        state['days_done'] += step
        s = status(world)
        log('AVANCE +%dd -> jour %s passages=%s roads=%d hot=%s efforts=%s' % (step, s.get('day'), s.get('passages'), len(s.get('roads', [])), s.get('hot_cells'), s.get('efforts')))
        return
    if state['phase'] == 4:
        if time.monotonic() - state['wait'] < 5:   # la presentation se met a jour (sentiers, formes, patine)
            return
        cmd(world, 'Anastasis.Village.SettlementReport')
        s = status(world)
        state['after'] = s
        (OUT / 'status-apres.json').write_text(json.dumps(s, indent=1), encoding='utf-8')
        roads = s.get('roads', [])
        houses = [b for b in s['buildings'] if b['type'] == 'house']
        fixed = [b for b in houses if b['fixed']]
        rec = {'day': s['day'], 'passages': s['passages'], 'roads': len(roads), 'segments': s['segments'], 'grass_hidden': s['grass_hidden'],
               'hot_cells': s['hot_cells'], 'efforts': s['efforts'], 'houses': len(houses), 'founded': len(fixed),
               'programs': sorted({b['program'] for b in houses}),
               'buildings': [{k: b[k] for k in ('id', 'type', 'program', 'variant', 'fixed', 'founder', 'job', 'household', 'owner', 'occupants', 'age', 'weathering', 'crowded_days', 'cause')} for b in s['buildings']]}
        state['checks'].append(rec)
        log('APRES ' + json.dumps(rec))
        if s['passages'] <= 0:
            fail('aucun passage compte')
        if not roads:
            fail('aucun sentier ne de %d jours de vie' % DAYS)
        for r in roads:
            if r['traffic_at_birth'] < 14.0:
                fail('sentier (%d,%d) ne d un passage %.1f < 14' % (r['x'], r['y'], r['traffic_at_birth']))
            # roadClassForTraffic(min(traffic, 46)) : sentier (path, 0,86) ou ruelle (lane, 0,74) selon le passage a la naissance.
            expected = 0.74 if r['traffic_at_birth'] >= 46.0 else 0.86
            if abs(r['cost'] - expected) > 1e-3:
                fail('sentier (%d,%d) ne a %.1f : cout %.4f != %.2f' % (r['x'], r['y'], r['traffic_at_birth'], r['cost'], expected))
        if roads and s['segments'] <= 0:
            fail('sentiers simules mais aucune bande rendue')
        if not fixed:
            fail('aucune maison fondee par un foyer')
        for b in s['buildings']:
            if b['variant'] and b['program'] and b['variant'] != b['program']:
                fail('%s affiche %s mais son histoire dit %s' % (b['id'], b['variant'], b['program']))
            if b['variant'] and b['age'] >= 10 and b['weathering'] <= 0.2:
                fail('%s acheve depuis %d jours sans patine (%.2f)' % (b['id'], b['age'], b['weathering']))
        state['queue'] = plan_after(world, s)
        state['phase'] = 5
        return
    if state['phase'] == 5:
        if not state['queue']:
            finish('%d jours de vie : %d passages, %d sentiers, %d maisons fondees' % (
                DAYS, state['after']['passages'], len(state['after']['roads']), len([b for b in state['after']['buildings'] if b['fixed'] and b['type'] == 'house'])))
            return
        name, setup = state['queue'].pop(0)
        shoot(name, setup)


def guarded(dt):
    try:
        tick(dt)
    except Exception as exc:
        import traceback
        unreal.log_error(traceback.format_exc())
        fail(str(exc))
        finish('exception')


handle = unreal.register_slate_post_tick_callback(guarded)
