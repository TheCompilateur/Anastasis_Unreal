"""Preuve PIE de la cabane du joueur (ma-cabane-001, ecart n°56).

PIE sur Lvl_AnastasisSlice, village du lancement. Le joueur arrive (`Anastasis.Player.Arrive`), decide de batir
(`Anastasis.Player.Build` : sa cabane se trace pres de lui), puis choisit `build` jusqu'a ce qu'elle soit debout, en
servant d'abord son corps (boire, manger, dormir), le temps avance par sauts d'une heure (`Anastasis.Sim.Advance 1h`).
La nuit venue (`Advance @22`), il choisit `rest` : il passe la porte et dort dedans. Simulation figee
(`anastasis.Sim.TimeScale 0`) pour chaque prise, ciel epingle, camera posee dans le repere de la cabane, `Shot` :

    01-chantier        a mi-chantier, de dehors, a hauteur d'homme
    02-cabane          achevee, de jour, de dehors
    03-porte           sur le seuil, a 1,7 m, le regard dans la piece
    04-dedans-nuit     dans la piece, la nuit : le joueur endormi pres du banc, le foyer
    05-dehors-nuit     de dehors, la nuit, vers la porte

Verdict CABANE_PIE PASS si : une cabane (type `cabin`) tracee pres du joueur et a lui des le trace ; debout, posee par
lui seul ; en moins de trois jours ; elle est son foyer ; la nuit il est dedans et dort, son corps pose dans l'emprise de
la cabane ; personne d'autre n'y loge ni n'y entre ; toutes les images ecrites. Les images ne sont pas jugees ici :
Alexandre les regarde. Sortie : Saved/CabinEvidence/pie/ (cabin.json + images). Rien n'est sauve.

Lancement : `py tools/unreal/cabane-pie.py` dans un editeur ouvert, ou au lot (editor-batch, `cabane-pie`).
  ANASTASIS_CABIN_SECONDS  plafond en secondes REELLES (defaut 540)
"""
import json
import math
import os
import time
from pathlib import Path

import unreal

ROOT = Path(unreal.Paths.project_dir())
OUT = ROOT / 'Saved' / 'CabinEvidence' / 'pie'
OUT.mkdir(parents=True, exist_ok=True)
SHOTS_DIR = ROOT / 'Saved' / 'Screenshots'
SECONDS = float(os.environ.get('ANASTASIS_CABIN_SECONDS', '540'))

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
lib = unreal.AnastasisSimulationDebugLibrary
unreal.log('CABANE_PIE_MAP_LOAD=' + str(les.load_level('/Game/Anastasis/Maps/Lvl_AnastasisSlice')))
proof_camera = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).spawn_actor_from_class(
    unreal.CameraActor, unreal.Vector(0, 0, 1000), unreal.Rotator())
proof_camera.set_actor_label('CabinProofCamera')

t0 = time.monotonic()
S = {'phase': 0, 'mark': 0.0, 'steps': 0, 'camera': None, 'queue': [], 'pending': None, 'fired': False,
     'before': set(), 'shots': [], 'samples': [], 'traced': None, 'mid_shot': False, 'night_steps': 0,
     'asleep': None, 'finished': False, 'after_shots': None}
handle = None


def log(msg):
    unreal.log('CABANE_PIE ' + msg)


def parse(raw):
    try:
        return json.loads(raw) if raw else {}
    except ValueError:
        return {}


def cmd(world, text):
    log('CMD ' + text)
    unreal.SystemLibrary.execute_console_command(world, text)


def existing_shots():
    return set(SHOTS_DIR.rglob('*.png')) if SHOTS_DIR.exists() else set()


def world_of(cabin, local):
    """Point du repere de la cabane (cm, z au-dessus de la cour) vers le monde."""
    loc, yaw, pad = cabin['location'], math.radians(cabin['yaw']), cabin.get('pad', 0.0)
    x, y, z = local
    return unreal.Vector(loc[0] + x * math.cos(yaw) - y * math.sin(yaw),
                         loc[1] + x * math.sin(yaw) + y * math.cos(yaw),
                         loc[2] + pad + z)


# Cameras dans le repere de la cabane : porte sur +Y, entree a (0, 360), piece x [-235, 235], y [-275, 175], sol a 18 cm.
VIEWS = {
    'dehors': ((520, 1150, 170), (0, 0, 150)),
    'porte': ((0, 430, 183), (-70, -160, 70)),
    'dedans': ((190, 140, 190), (-120, -70, 60)),
    'dehors-nuit': ((260, 950, 170), (0, 225, 120)),
}


def enqueue(name, view, pre):
    S['queue'].append((name, view, pre))


def act(world, player):
    """Le corps d'abord (c'est au joueur de choisir le remede, ecart n°21), sinon il batit."""
    if player.get('body'):
        goal = player['body']
    elif player.get('energy', 100) <= 25:
        goal = 'rest'
    elif player.get('thirst', 0) >= 45:
        goal = 'drink'
    elif player.get('hunger', 0) >= 45:
        goal = 'eat'
    else:
        goal = 'build'
    cmd(world, 'Anastasis.Player.Goal ' + goal)


def finish(ok, reason, cabin=None):
    if S['finished']:
        return
    S['finished'] = True
    with open(OUT / 'cabin.json', 'w', encoding='utf-8') as f:
        json.dump({'ok': ok, 'reason': reason, 'cabin': cabin, 'traced': S['traced'], 'asleep': S['asleep'],
                   'samples': S['samples'], 'shots': S['shots']}, f, ensure_ascii=False, indent=1)
    unreal.log('CABANE_PIE %s %s' % ('PASS' if ok else 'FAIL', reason))
    if les.is_in_play_in_editor():
        les.editor_request_end_play()
    S['phase'] = 99
    S['mark'] = time.monotonic()


def verdict(world):
    c = parse(lib.get_cabin_status(world))
    t = S['traced'] or {}
    a = S['asleep'] or {}
    fp = a.get('footprint') or [0, 0, 0, 0]
    pl = a.get('pawn_local') or [1e9, 1e9, 0]
    workers = c.get('workers', [])
    built_days = (c.get('completed_day', -1) - t.get('day', 0)) if c.get('completed_day', -1) >= 0 else 99
    near = t.get('site') and math.hypot(t.get('tile_x', 0) + 0.5 - t.get('x', 0), t.get('tile_y', 0) + 0.5 - t.get('y', 0)) <= 7.5
    checks = {
        'traced_cabin': t.get('type') == 'cabin' and t.get('owner') == t.get('player') and bool(near),
        'standing': c.get('progress', 0) >= 1.0 and c.get('architecture') is True and c.get('archetype') == 'cabin',
        'built_alone': len(workers) == 1 and workers[0].split(':')[0] == c.get('player'),
        'few_days': built_days <= 3,
        'his_home': c.get('home') == c.get('site') and c.get('owner') == c.get('player'),
        'sleeps_inside': a.get('inside_building') == c.get('site') and a.get('activity') == 'dort',
        'body_inside': bool(a.get('pawn_indoors')) and fp[0] <= pl[0] <= fp[2] and fp[1] <= pl[1] <= fp[3],
        'his_alone': not c.get('others_inside') and not c.get('others_lodged') and not a.get('others_inside'),
        'shots': len(S['shots']) == 5 and all(s['written'] for s in S['shots']),
    }
    ok = all(checks.values())
    finish(ok, 'built_days=%d pieces=%s/%s workers=%s checks=%s' % (
        built_days, c.get('pieces'), c.get('piece_total'), workers,
        ','.join('%s=%d' % (k, int(v)) for k, v in checks.items())), c)


def run_queue(world, now):
    """Une prise a la fois : pose, 2,5 s pour que l'image se stabilise, `Shot`, puis on range le fichier."""
    if S['pending'] is not None:
        if not S['fired']:
            if now - S['mark'] < 2.5:
                return True
            cmd(world, 'Shot')
            S['fired'] = True
            S['mark'] = now
            return True
        fresh = sorted(existing_shots() - S['before'], key=lambda p: p.stat().st_mtime)
        if fresh and now - S['mark'] > 1.0:
            try:
                fresh[-1].replace(S['pending'])
            except OSError:
                return True
        if S['pending'].is_file() or now - S['mark'] > 20:
            S['shots'].append({'file': S['pending'].name, 'written': S['pending'].is_file()})
            log('SHOT %s written=%s' % (S['pending'].name, S['pending'].is_file()))
            S['pending'] = None
        return True
    if not S['queue']:
        return False
    name, view, pre = S['queue'].pop(0)
    cabin = parse(lib.get_cabin_status(world))
    if not cabin.get('location'):
        S['shots'].append({'file': name + '.png', 'written': False})
        return True
    for c in pre:
        cmd(world, c)
    pc = unreal.GameplayStatics.get_player_controller(world, 0)
    if pc and S['camera']:
        pc.set_view_target_with_blend(S['camera'], 0.0)
    pos, target = VIEWS[view]
    p, q = world_of(cabin, pos), world_of(cabin, target)
    S['camera'].set_actor_location(p, False, True)
    S['camera'].set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(p, q), True)
    path = OUT / (name + '.png')
    if path.exists():
        path.unlink()
    S['before'] = existing_shots()
    S['pending'] = path
    S['fired'] = False
    S['mark'] = now
    return True


DAY = ['anastasis.Sim.TimeScale 0', 'anastasis.Sky.Day 1', 'anastasis.Sky.Hour 16.5']
NIGHT = ['anastasis.Sim.TimeScale 0', 'anastasis.Sky.Day 1', 'anastasis.Sky.Hour 22.5']
RESUME = ['anastasis.Sky.Hour -1', 'anastasis.Sky.Day -1', 'anastasis.Sim.TimeScale 0.0375']


def tick(_dt):
    now = time.monotonic()
    if S['phase'] == 99:
        if not les.is_in_play_in_editor() or now - S['mark'] > 20:
            unreal.unregister_slate_post_tick_callback(handle)
            unreal.SystemLibrary.quit_editor()
        return
    if now - t0 > SECONDS:
        finish(False, 'timeout phase=%d steps=%d shots=%d' % (S['phase'], S['steps'], len(S['shots'])),
               parse(lib.get_cabin_status(ues.get_game_world())) if ues.get_game_world() else None)
        return
    if S['phase'] == 0:
        if now - t0 > 3.0:
            S['phase'] = 1
            les.editor_request_begin_play()
        return
    world = ues.get_game_world()
    if not world:
        return
    if S['phase'] == 1:
        if not les.is_in_play_in_editor() or not parse(lib.get_chronicle_status(world)).get('started'):
            return
        for c in ('anastasis.Village.Debug 0', 'r.MotionBlurQuality 0', 'Anastasis.Player.Arrive'):
            cmd(world, c)
        cameras = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.CameraActor)
        S['camera'] = next((c for c in cameras if c.get_actor_label() == 'CabinProofCamera'), None)
        S['phase'] = 2
        S['mark'] = now
        return
    if S['phase'] == 2:
        # Il decide de batir, la ou il se tient.
        if now - S['mark'] < 2.0:
            return
        cmd(world, 'Anastasis.Player.Build')
        traced = parse(lib.get_cabin_status(world))
        if not traced.get('site'):
            if now - S['mark'] > 20:
                finish(False, 'no cabin traced', traced)
            return
        S['traced'] = traced
        log('TRACED ' + json.dumps(traced))
        S['phase'] = 3
        return
    if S['phase'] == 3:
        # Il batit, une heure de jeu par image.
        if run_queue(world, now):
            return
        cabin = parse(lib.get_cabin_status(world))
        if S['mid_shot'] == 'queued':
            for c in RESUME:
                cmd(world, c)
            S['mid_shot'] = True
        if cabin.get('progress', 0) >= 1.0:
            log('STANDING ' + json.dumps(cabin))
            S['samples'].append({'step': S['steps'], 'day': cabin.get('day'), 'pieces': cabin.get('pieces'), 'progress': 1.0})
            enqueue('02-cabane', 'dehors', DAY)
            enqueue('03-porte', 'porte', [])
            S['phase'] = 4
            return
        if not S['mid_shot'] and cabin.get('progress', 0) >= 0.4 and cabin.get('actor'):
            enqueue('01-chantier', 'dehors', DAY)
            S['mid_shot'] = 'queued'
            return
        if S['steps'] > 4 * 24:
            finish(False, 'cabin not finished after 4 days (pieces=%s)' % cabin.get('pieces'), cabin)
            return
        act(world, parse(lib.get_player_status(world)))
        cmd(world, 'Anastasis.Sim.Advance 1h')
        S['steps'] += 1
        if S['steps'] % 2 == 0:
            S['samples'].append({'step': S['steps'], 'day': cabin.get('day'), 'pieces': cabin.get('pieces'),
                                 'goal': cabin.get('goal'), 'progress': cabin.get('progress')})
        return
    if S['phase'] == 4:
        # Les deux prises de jour, puis la nuit : il va dormir chez lui.
        if run_queue(world, now):
            return
        for c in RESUME:
            cmd(world, c)
        cmd(world, 'Anastasis.Sim.Advance @22')
        S['phase'] = 5
        return
    if S['phase'] == 5:
        player = parse(lib.get_player_status(world))
        cabin = parse(lib.get_cabin_status(world))
        if cabin.get('inside') and cabin.get('inside_building') == cabin.get('site') and cabin.get('activity') == 'dort':
            # Fige, puis laisse le corps se poser avant de lire ou il est.
            cmd(world, 'anastasis.Sim.TimeScale 0')
            S['phase'] = 6
            S['mark'] = now
            return
        if S['night_steps'] > 8:
            finish(False, 'did not sleep in his cabin (inside=%s activity=%s)' % (cabin.get('inside_building'), cabin.get('activity')), cabin)
            return
        cmd(world, 'Anastasis.Player.Goal ' + (player.get('body') or 'rest'))
        cmd(world, 'Anastasis.Sim.Advance 1h')
        S['night_steps'] += 1
        return
    if S['phase'] == 6:
        if now - S['mark'] < 1.5:
            return
        S['asleep'] = parse(lib.get_cabin_status(world))
        log('ASLEEP ' + json.dumps(S['asleep']))
        enqueue('04-dedans-nuit', 'dedans', NIGHT)
        enqueue('05-dehors-nuit', 'dehors-nuit', [])
        S['phase'] = 7
        return
    if S['phase'] == 7:
        if run_queue(world, now):
            return
        for c in RESUME:
            cmd(world, c)
        verdict(world)


handle = unreal.register_slate_post_tick_callback(tick)
