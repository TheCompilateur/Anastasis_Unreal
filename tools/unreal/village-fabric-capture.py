"""Captures du tissu du village (mission village-fabric-001, VILLAGE_FABRIC_001).

PIE sur Lvl_AnastasisSlice, hameau pose par commandes (comme village-fabric-pie.py), ciel epingle
(anastasis.Sky.Hour 16.5), overlay de debug coupe, simulation figee. Cameras calees sur le tissu lu
par AnastasisVillageFabricLibrary (placette, ruelle la plus longue), sol lu sous la camera. Prises
(`Shot`, vue affichee) :

    01-oblique-on       vue plongeante sur la placette et les ruelles
    02-ruelle-1m70-on   a hauteur d'homme sur la ruelle la plus longue, vers la placette
    03-placette-1m70-on a hauteur d'homme au bord de la placette, vers le puits
    04-oblique-off      meme camera que 01, anastasis.Village.Fabric 0 (temoin A/B)
    05-ruelle-1m70-off  meme camera que 02, Fabric 0

Verdict : VILLAGE_FABRIC_CAPTURE COMPLETE (toutes les images ecrites) / VILLAGE_FABRIC_CAPTURE FAIL.
COMPLETE ne juge pas l'image : il faut la regarder. Sortie : ANASTASIS_FABRIC_CAPTURE_OUT
(defaut Saved/VillageFabricEvidence/capture). Aucun asset sauve.
"""
import json
import math
import os
import time
from pathlib import Path

import unreal

ROOT = Path(unreal.Paths.project_dir())
OUT = Path(os.environ.get('ANASTASIS_FABRIC_CAPTURE_OUT', str(ROOT / 'Saved' / 'VillageFabricEvidence' / 'capture')))
OUT.mkdir(parents=True, exist_ok=True)
SHOTS_DIR = ROOT / 'Saved' / 'Screenshots'

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
DBG = unreal.AnastasisSimulationDebugLibrary
LIB = unreal.AnastasisVillageFabricLibrary
les.load_level('/Game/Anastasis/Maps/Lvl_AnastasisSlice')
proof_camera = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).spawn_actor_from_class(
    unreal.CameraActor, unreal.Vector(0, 0, 1000), unreal.Rotator())
proof_camera.set_actor_label('FabricProofCamera')

t0 = time.monotonic()
S = {'phase': 0, 'step': 0, 'mark': 0.0, 'camera': None, 'pending': None, 'fired': False, 'before': set(),
     'queue': [], 'shots': [], 'views': {}, 'finished': False}
handle = None


def log(msg):
    unreal.log('FABRIC_CAPTURE ' + msg)


def finish(ok, reason):
    if S['finished']:
        return
    S['finished'] = True
    (OUT / 'capture.json').write_text(json.dumps({'ok': ok, 'reason': reason, 'shots': S['shots'], 'views': S['views'],
                                                  'scenario': S.get('scenario')}, indent=1), encoding='utf-8')
    unreal.log('VILLAGE_FABRIC_CAPTURE ' + ('COMPLETE ' if ok else 'FAIL ') + reason)
    unreal.unregister_slate_post_tick_callback(handle)
    unreal.SystemLibrary.quit_editor()


def cmd(world, text):
    log('CMD ' + text)
    unreal.SystemLibrary.execute_console_command(world, text)


def status(world):
    try:
        return json.loads(LIB.get_village_fabric_status(world))
    except ValueError:
        return {}


def ground(world, x, y):
    p = DBG.get_settlement_ground_point(world, x / 2000.0, y / 2000.0)
    return p.z


def vec(a):
    return unreal.Vector(a[0], a[1], a[2])


def place(view):
    camera = S['camera']
    pos, target = vec(view['pos']), vec(view['target'])
    camera.set_actor_location(pos, False, True)
    camera.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(pos, target), True)


def existing_shots():
    return set(SHOTS_DIR.rglob('*.png')) if SHOTS_DIR.exists() else set()


def plan_views(world, s):
    f = s['fabric']
    c = f['plaza']['centre']
    views = {'oblique': {'pos': [c[0] - 3200, c[1] - 3600, c[2] + 2600], 'target': [c[0] + 400, c[1] + 400, c[2]]}}
    lanes = sorted(f.get('laneList', []), key=lambda l: -l['length'])
    if lanes:
        a, b = lanes[0]['a'], lanes[0]['b']
        # Aux deux tiers de la corde cote seuil, le regard vers la placette.
        px, py = b[0] + (a[0] - b[0]) * 0.65, b[1] + (a[1] - b[1]) * 0.65
        views['ruelle'] = {'pos': [px, py, ground(world, px, py) + 170], 'target': [b[0], b[1], b[2] + 120]}
    ang = math.radians(-35)
    px, py = c[0] + 1100 * math.cos(ang), c[1] + 1100 * math.sin(ang)
    views['placette'] = {'pos': [px, py, ground(world, px, py) + 170], 'target': [c[0], c[1], c[2] + 110]}
    S['views'] = views
    q = [('01-oblique-on', 'oblique', None)]
    if 'ruelle' in views:
        q.append(('02-ruelle-1m70-on', 'ruelle', None))
    q.append(('03-placette-1m70-on', 'placette', None))
    q.append(('04-oblique-off', 'oblique', 'anastasis.Village.Fabric 0'))
    if 'ruelle' in views:
        q.append(('05-ruelle-1m70-off', 'ruelle', None))
    S['queue'] = q


def tick(dt):
    now = time.monotonic()
    if now - t0 > 480:
        finish(False, 'timeout step=%d shots=%d' % (S['step'], len(S['shots'])))
        return
    if S['phase'] == 0 and now - t0 > 3:
        S['phase'] = 1
        les.editor_request_begin_play()
        return
    world = ues.get_game_world()
    if S['phase'] == 1:
        if not les.is_in_play_in_editor() or not world or DBG.get_simulation_time(world) < 0:
            return
        for c in ('anastasis.Village.Debug 0', 'r.MotionBlurQuality 0', 'anastasis.Sky.Hour 16.5', 'anastasis.Sky.Day 1',
                  'anastasis.Village.Fabric 1', 'anastasis.Village.FabricClear 1', 'Anastasis.Village.Hamlet 6 0'):
            cmd(world, c)
        S['phase'] = 2
        S['mark'] = now
        return
    if S['phase'] != 2 or not world:
        return
    waited = now - S['mark']

    if S['step'] == 0 and waited > 4:
        s = status(world)
        f = s.get('fabric', {})
        if f.get('plots', 0) < 3:
            plaza = f.get('plaza', {})
            if plaza.get('valid'):
                wx, wy = int(plaza['centre'][0] // 2000), int(plaza['centre'][1] // 2000)
                S['scenario'] = 'FirstHouse+FirstWell at %d,%d' % (wx, wy)
                cmd(world, 'Anastasis.Village.FirstHouse 0 %d %d' % (wx + 2, wy))
                cmd(world, 'Anastasis.Village.FirstWell 0 %d %d' % (wx, wy))
            else:
                S['scenario'] = 'FirstHouse+FirstWell at settlement'
                cmd(world, 'Anastasis.Village.FirstHouse 0')
                cmd(world, 'Anastasis.Village.FirstWell 0')
        else:
            S['scenario'] = 'Hamlet'
        S['step'] = 1
        S['mark'] = now
        return
    if S['step'] == 1:
        if waited < 5:
            return
        s = status(world)
        f = s.get('fabric', {})
        if not s.get('actor') or not f.get('plaza', {}).get('valid'):
            if waited > 45:
                finish(False, 'no fabric with a plaza')
            return
        log('FABRIC ' + json.dumps({k: f.get(k) for k in ('plots', 'doors', 'connected', 'lanes', 'laneLength', 'walls', 'signature')}))
        cmd(world, 'anastasis.Sim.TimeScale 0')
        cameras = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.CameraActor)
        S['camera'] = next((c for c in cameras if c.get_actor_label() == 'FabricProofCamera'), None)
        pc = unreal.GameplayStatics.get_player_controller(world, 0)
        if not S['camera'] or not pc:
            finish(False, 'no camera or player controller')
            return
        pc.set_view_target_with_blend(S['camera'], 0.0)
        plan_views(world, s)
        S['step'] = 2
        S['mark'] = now
        return

    # step 2 : file des prises
    if S['pending'] is not None:
        if not S['fired']:
            if now - S['mark'] < 2.5:
                return
            cmd(world, 'Shot')
            S['fired'] = True
            S['mark'] = now
            return
        fresh = sorted(existing_shots() - S['before'], key=lambda p: p.stat().st_mtime)
        if fresh and now - S['mark'] > 1.0:
            try:
                fresh[-1].replace(S['pending'])
            except OSError:
                return
        if S['pending'].is_file() or now - S['mark'] > 20:
            S['shots'].append({'file': S['pending'].name, 'written': S['pending'].is_file()})
            log('SHOT %s written=%s' % (S['pending'].name, S['pending'].is_file()))
            S['pending'] = None
        return
    if not S['queue']:
        cmd(world, 'anastasis.Village.Fabric 1')
        cmd(world, 'anastasis.Sky.Hour -1')
        cmd(world, 'anastasis.Sky.Day -1')
        missing = [s['file'] for s in S['shots'] if not s['written']]
        les.editor_request_end_play()
        finish(not missing, 'shots=%d missing=%s' % (len(S['shots']), missing))
        return
    name, view, pre = S['queue'].pop(0)
    if pre:
        cmd(world, pre)
    place(S['views'][view])
    path = OUT / (name + '.png')
    if path.exists():
        path.unlink()
    S['before'] = existing_shots()
    S['pending'] = path
    S['fired'] = False
    S['mark'] = now


handle = unreal.register_slate_post_tick_callback(tick)
