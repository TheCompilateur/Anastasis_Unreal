"""Preuve PIE du sommeil couche (dormir-couche-001).

PIE sur Lvl_AnastasisSlice, village du lancement. Le joueur arrive, trace et leve sa cabane (`Anastasis.Player.Build`,
but `build`, sauts `Anastasis.Sim.Advance 1h`, le corps d'abord), puis la nuit (`Advance @22`) choisit `rest`. Quand il
dort, la simulation est figee (`anastasis.Sim.TimeScale 0`) et l'etat du sommeil lu (`get_sleep_status`) : qui est
dedans, a quelle activite, couche ou cache, angle du corps, exposition de chaque logis. S'il n'y a encore aucune maison
du village ou des habitants dorment, le temps reprend (`Advance 1d` puis `@22`, trois fois au plus). Prises, camera
posee dans le repere du logis, ciel epingle a 22 h 30, `Shot` :

    01-maison-nuit-avant   dans une maison ou l'on dort, `anastasis.Village.InteriorLight 0` (l'exposition du dehors)
    02-maison-nuit         meme camera, `InteriorLight 1` (l'oeil s'accoutume au foyer)
    03-cabane-nuit         dans la cabane : le joueur couche sur sa banquette
    04-maison-dehors-nuit  la meme maison, de dehors, vers la porte

Verdict SOMMEIL_PIE PASS si : le joueur dort dans sa cabane, couche (corps a moins de 20 degres de l'horizontale) ;
une maison du village a au moins un dormeur ; chaque habitant qui dort dans un logis a une place, y est couche,
visible, son corps dans le volume habite ; l'exposition d'interieur d'un logis, la nuit, est celle de la nuit
(`InteriorNightEV` a 1 EV pres) ; toutes les images ecrites. Les images ne sont pas jugees ici : Alexandre les
regarde. Sortie : Saved/SleepEvidence/pie/ (sleep.json + images). Rien n'est sauve.

Lancement : `py tools/unreal/sommeil-pie.py` dans un editeur ouvert, ou au lot (editor-batch, `sommeil-pie`).
  ANASTASIS_SLEEP_SECONDS  plafond en secondes REELLES (defaut 600)
"""
import json
import math
import os
import time
from pathlib import Path

import unreal

ROOT = Path(unreal.Paths.project_dir())
OUT = ROOT / 'Saved' / 'SleepEvidence' / 'pie'
OUT.mkdir(parents=True, exist_ok=True)
SHOTS_DIR = ROOT / 'Saved' / 'Screenshots'
SECONDS = float(os.environ.get('ANASTASIS_SLEEP_SECONDS', '600'))
# Reglage a l'oeil : des prises de plus dans la cabane a d'autres anastasis.Village.InteriorNightEV (ex. "7,8.5").
EV_SWEEP = [v for v in os.environ.get('ANASTASIS_SLEEP_EV_SWEEP', '').split(',') if v.strip()]
NIGHT_EV = 5.5

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
lib = unreal.AnastasisSimulationDebugLibrary
unreal.log('SOMMEIL_PIE_MAP_LOAD=' + str(les.load_level('/Game/Anastasis/Maps/Lvl_AnastasisSlice')))
proof_camera = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).spawn_actor_from_class(
    unreal.CameraActor, unreal.Vector(0, 0, 1000), unreal.Rotator())
proof_camera.set_actor_label('SleepProofCamera')

t0 = time.monotonic()
S = {'phase': 0, 'mark': 0.0, 'steps': 0, 'night_steps': 0, 'nights': 0, 'camera': None, 'queue': [],
     'pending': None, 'fired': False, 'before': set(), 'shots': [], 'cabin': None, 'house': None,
     'sleep': None, 'finished': False}
handle = None


def log(msg):
    unreal.log('SOMMEIL_PIE ' + msg)


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


def to_world(home, local):
    loc, yaw, pad = home['location'], math.radians(home['yaw']), home.get('pad', 0.0)
    x, y, z = local
    return unreal.Vector(loc[0] + x * math.cos(yaw) - y * math.sin(yaw),
                         loc[1] + x * math.sin(yaw) + y * math.cos(yaw), loc[2] + pad + z)


def to_local(home, world):
    loc, yaw, pad = home['location'], math.radians(home['yaw']), home.get('pad', 0.0)
    dx, dy = world[0] - loc[0], world[1] - loc[1]
    return (dx * math.cos(yaw) + dy * math.sin(yaw), -dx * math.sin(yaw) + dy * math.cos(yaw), world[2] - loc[2] - pad)


def inside_view(home):
    """Dans la piece de la banquette (une cloison peut couper le logis) : en face d'elle, a hauteur d'homme, le
    regard sur la banquette. Le long de la banquette, a son milieu ; en travers, 2,4 m devant elle au plus."""
    x0, y0, z0, x1, y1, z1 = home['interior']
    bx, by, bz = home['bench_local']
    along_x = home.get('bench_along_x', abs(by - (y0 + y1) / 2) > abs(bx - (x0 + x1) / 2))
    if along_x:
        side = 1 if (y0 + y1) / 2 > by else -1
        px, py = bx, max(y0 + 40, min(y1 - 40, by + side * 240))
    else:
        side = 1 if (x0 + x1) / 2 > bx else -1
        px, py = max(x0 + 40, min(x1 - 40, bx + side * 240)), by
    return (px, py, bz - 46 + 165), (bx, by, bz + 20)


def outside_view(home):
    x0, y0, z0, x1, y1, z1 = home['interior']
    return (0.3 * (x1 - x0), y1 + 900, 170), (0.0, y1, 120)


def act(world, player):
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


def finish(ok, reason):
    if S['finished']:
        return
    S['finished'] = True
    with open(OUT / 'sleep.json', 'w', encoding='utf-8') as f:
        json.dump({'ok': ok, 'reason': reason, 'cabin': S['cabin'], 'house': S['house'], 'sleep': S['sleep'],
                   'shots': S['shots']}, f, ensure_ascii=False, indent=1)
    unreal.log('SOMMEIL_PIE %s %s' % ('PASS' if ok else 'FAIL', reason))
    if les.is_in_play_in_editor():
        les.editor_request_end_play()
    S['phase'] = 99
    S['mark'] = time.monotonic()


def pick_house(sleep):
    """La maison du village (pas la cabane) ou dorment le plus d'habitants."""
    counts = {}
    for n in sleep.get('inside', []):
        if not n.get('player') and n.get('spot'):
            counts[n['building']] = counts.get(n['building'], 0) + 1
    homes = {h['id']: h for h in sleep.get('homes', [])}
    best = [b for b in sorted(counts, key=lambda b: -counts[b]) if b in homes and homes[b]['type'] != 'cabin']
    return homes[best[0]] if best else None


def verdict(world):
    sleep = S['sleep'] or {}
    house = S['house'] or {}
    me = next((n for n in sleep.get('inside', []) if n.get('player')), {})
    sleepers = [n for n in sleep.get('inside', []) if not n.get('player') and n.get('activity') in ('dort', 'repose')]
    homes = {h['id']: h for h in sleep.get('homes', [])}
    bad = []
    for n in sleepers:
        h = homes.get(n['building'])
        if not h:
            continue  # pas un logis (un abri sans volume habite) : hors de cette preuve
        if not n.get('spot'):
            bad.append('%s:sans-place' % n['id'])
            continue
        x0, y0, z0, x1, y1, z1 = h['interior']
        lx, ly, lz = to_local(h, n.get('body', [0, 0, -1e9]))
        if not (n.get('lying') and n.get('visible') and abs(n.get('tilt', 90)) < 20
                and x0 <= lx <= x1 and y0 <= ly <= y1 and z0 <= lz <= z1):
            bad.append('%s:lying=%s,visible=%s,tilt=%.0f,local=(%.0f,%.0f,%.0f)' % (
                n['id'], n.get('lying'), n.get('visible'), n.get('tilt', 90), lx, ly, lz))
    lit = [h for h in homes.values() if h.get('interior_light')]
    checks = {
        'player_lying': me.get('building') == (S['cabin'] or {}).get('site') and bool(me.get('lying'))
                        and abs(me.get('tilt', 90)) < 20,
        'house_sleepers': bool(house) and any(n['building'] == house.get('id') for n in sleepers),
        'all_lying': len(sleepers) > 0 and not bad,
        'interior_night_ev': bool(lit) and all(abs(h['interior_ev'] - NIGHT_EV) <= 1.0 for h in lit),
        'shots': all(any(s['file'] == n and s['written'] for s in S['shots'])
                     for n in ('01-maison-nuit-avant.png', '02-maison-nuit.png', '03-cabane-nuit.png', '04-maison-dehors-nuit.png')),
    }
    ok = all(checks.values())
    finish(ok, 'sleepers=%d house=%s bad=%s player_tilt=%s checks=%s' % (
        len(sleepers), house.get('id'), bad[:4], me.get('tilt'),
        ','.join('%s=%d' % (k, int(v)) for k, v in checks.items())))


def run_queue(world, now):
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
            homes = parse(lib.get_sleep_status(world)).get('homes', [])
            log('VIEW %s %s' % (S['pending'].name, json.dumps([(h['id'], h.get('view_inside'), h.get('interior_light'),
                                                                 h.get('interior_ev')) for h in homes])))
            S['pending'] = None
        return True
    if not S['queue']:
        return False
    name, home, view, pre = S['queue'].pop(0)
    for c in pre:
        cmd(world, c)
    pc = unreal.GameplayStatics.get_player_controller(world, 0)
    if pc and S['camera']:
        pc.set_view_target_with_blend(S['camera'], 0.0)
    pos, target = view
    p, q = to_world(home, pos), to_world(home, target)
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


NIGHT = ['anastasis.Sim.TimeScale 0', 'anastasis.Sky.Day 1', 'anastasis.Sky.Hour 22.5']
RESUME = ['anastasis.Sky.Hour -1', 'anastasis.Sky.Day -1', 'anastasis.Village.InteriorLight 1', 'anastasis.Sim.TimeScale 0.0375']


def tick(_dt):
    now = time.monotonic()
    if S['phase'] == 99:
        if not les.is_in_play_in_editor() or now - S['mark'] > 20:
            unreal.unregister_slate_post_tick_callback(handle)
            unreal.SystemLibrary.quit_editor()
        return
    if now - t0 > SECONDS:
        finish(False, 'timeout phase=%d steps=%d shots=%d' % (S['phase'], S['steps'], len(S['shots'])))
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
        S['camera'] = next((c for c in cameras if c.get_actor_label() == 'SleepProofCamera'), None)
        S['phase'] = 2
        S['mark'] = now
        return
    if S['phase'] == 2:
        if now - S['mark'] < 2.0:
            return
        cmd(world, 'Anastasis.Player.Build')
        if not parse(lib.get_cabin_status(world)).get('site'):
            if now - S['mark'] > 20:
                finish(False, 'no cabin traced')
            return
        S['phase'] = 3
        return
    if S['phase'] == 3:
        # Il leve sa cabane, une heure de jeu par image.
        cabin = parse(lib.get_cabin_status(world))
        if cabin.get('progress', 0) >= 1.0:
            S['cabin'] = cabin
            S['phase'] = 4
            return
        if S['steps'] > 4 * 24:
            finish(False, 'cabin not finished (pieces=%s)' % cabin.get('pieces'))
            return
        act(world, parse(lib.get_player_status(world)))
        cmd(world, 'Anastasis.Sim.Advance 1h')
        S['steps'] += 1
        return
    if S['phase'] == 4:
        # La nuit : il va dormir chez lui.
        cmd(world, 'Anastasis.Sim.Advance @22')
        S['night_steps'] = 0
        S['phase'] = 5
        return
    if S['phase'] == 5:
        player = parse(lib.get_player_status(world))
        cabin = parse(lib.get_cabin_status(world))
        if cabin.get('inside_building') == cabin.get('site') and cabin.get('activity') == 'dort':
            # Le ciel suit le temps qui coule : l'exposition de la nuit met quelques secondes a s'installer apres
            # les sauts. Le temps coule donc au rythme du jeu avant de figer.
            S['phase'] = 55
            S['mark'] = now
            return
        if S['night_steps'] > 8:
            finish(False, 'did not sleep in his cabin (inside=%s activity=%s)' % (cabin.get('inside_building'), cabin.get('activity')))
            return
        cmd(world, 'Anastasis.Player.Goal ' + (player.get('body') or 'rest'))
        cmd(world, 'Anastasis.Sim.Advance 1h')
        S['night_steps'] += 1
        return
    if S['phase'] == 55:
        if now - S['mark'] < 10.0:
            return
        cmd(world, 'anastasis.Sim.TimeScale 0')
        S['phase'] = 6
        S['mark'] = now
        return
    if S['phase'] == 6:
        # Fige ; les corps se couchent a la frame suivante. Une maison ou l'on dort ?
        if now - S['mark'] < 1.5:
            return
        sleep = parse(lib.get_sleep_status(world))
        house = pick_house(sleep)
        if not house and S['nights'] < 3:
            S['nights'] += 1
            log('NO_HOUSE_ASLEEP night=%d inside=%d' % (S['nights'], len(sleep.get('inside', []))))
            cmd(world, 'anastasis.Sim.TimeScale 0.0375')
            cmd(world, 'Anastasis.Sim.Advance 1d')
            S['phase'] = 4
            return
        S['sleep'] = sleep
        S['house'] = house
        log('SLEEP ' + json.dumps(sleep))
        homes = {h['id']: h for h in sleep.get('homes', [])}
        cabin_home = homes.get((S['cabin'] or {}).get('site'))
        if house:
            S['queue'].append(('01-maison-nuit-avant', house, inside_view(house), NIGHT + ['anastasis.Village.InteriorLight 0']))
            S['queue'].append(('02-maison-nuit', house, inside_view(house), ['anastasis.Village.InteriorLight 1']))
        if cabin_home:
            S['queue'].append(('03-cabane-nuit', cabin_home, inside_view(cabin_home), NIGHT))
            for ev in EV_SWEEP:
                S['queue'].append(('ev-%s-cabane' % ev.strip(), cabin_home, inside_view(cabin_home),
                                   ['anastasis.Village.InteriorNightEV ' + ev.strip()]))
                if house:
                    S['queue'].append(('ev-%s-maison' % ev.strip(), house, inside_view(house), []))
            if EV_SWEEP:
                S['queue'].append(('ev-reset', cabin_home, inside_view(cabin_home), ['anastasis.Village.InteriorNightEV 5.5']))
        if house:
            S['queue'].append(('04-maison-dehors-nuit', house, outside_view(house), []))
        S['phase'] = 7
        return
    if S['phase'] == 7:
        if run_queue(world, now):
            return
        for c in RESUME:
            cmd(world, c)
        verdict(world)


handle = unreal.register_slate_post_tick_callback(tick)
