"""Preuve PIE du fermier au grenier (gather-deliver-001) : cueillir puis livrer.

Lance par gather-deliver-pie.ps1 (editeur discret, rendu hors focus). Rien n'est sauve.
Sortie : ANASTASIS_GATHER_OUT (defaut Saved/SliceEvidence/gather-deliver).

Pilote par l'etat de la simulation, jamais par l'horloge murale : PIE, puis
`Anastasis.Village.FirstFarmer`, puis une camera au-dessus du grenier et du
champ. Chaque etape observee est capturee (`Shot showui`, simulation figee le
temps de la prise, puis relancee) :

  01-depart     le fermier vient d'etre pose au seuil du grenier
  02-recolte    session de coups ouverte, sac qui se remplit
  03-retour     but `deliver`, en route vers le grenier, sac entre 1 et 10 : plein (retour force
                a > 9) ou moins, quand une reconsideration (`npc.js` l. 893, reconsider-001) fait
                gagner la ligne `deliver` — la reference livre ainsi 5 vivres dans le scenario
                endurance
  04-livre      premiere livraison faite : le sac du retour est passe au grenier, en entier
  05-large      vue large apres la deuxieme livraison

A chaque echantillon : champs + sacs + grenier + repas == total initial.
Une etape manquante echoue ; jamais de PASS raconte.
"""
import json
import os
import time
from pathlib import Path

import unreal

ROOT = Path(unreal.Paths.project_dir())
OUT = Path(os.environ.get('ANASTASIS_GATHER_OUT', str(ROOT / 'Saved' / 'SliceEvidence' / 'gather-deliver')))
OUT.mkdir(parents=True, exist_ok=True)
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
DBG = unreal.AnastasisSimulationDebugLibrary
les.load_level('/Game/Anastasis/Maps/Lvl_AnastasisSlice')
# Camera non sauvee, dupliquee avec le niveau dans le monde PIE.
proof_camera = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).spawn_actor_from_class(
    unreal.CameraActor, unreal.Vector(0, 0, 1000), unreal.Rotator())
proof_camera.set_actor_label('GatherDeliverProofCamera')

t0 = time.monotonic()
state = {'phase': 0, 'camera': None, 'initial': None, 'pending': None, 'pending_at': 0.0,
         'shots': [], 'seen': [], 'samples': [], 'last_sample': None, 'finished': False}
handle = None


def log(msg):
    unreal.log('GATHER_DELIVER ' + msg)


def finish(ok, reason):
    if state['finished']:
        return
    state['finished'] = True
    report = {'pass': ok, 'reason': reason, 'seen': state['seen'], 'shots': state['shots'], 'samples': state['samples']}
    (OUT / 'gather-deliver.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    log(('PASS ' if ok else 'FAIL ') + reason)
    unreal.unregister_slate_post_tick_callback(handle)
    unreal.SystemLibrary.quit_editor()


def aim(world, target, offset):
    position = target + offset
    camera = state['camera']
    camera.set_actor_location(position, False, True)
    camera.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(position, target), True)


SHOTS_DIR = ROOT / 'Saved' / 'Screenshots'


def existing_shots():
    return set(SHOTS_DIR.rglob('*.png')) if SHOTS_DIR.exists() else set()


def shoot(world, name):
    # `Shot` capture la vue de jeu AFFICHEE, overlay de debug et textes compris ;
    # HighResShot refait un rendu a part qui les perd. La simulation est figee, la
    # camera se pose 0,6 s (historique temporel), puis la prise ; le fichier est deplace.
    path = OUT / (name + '.png')
    if path.exists():
        path.unlink()
    unreal.SystemLibrary.execute_console_command(world, 'anastasis.Sim.Speed 0')
    state['before'] = existing_shots()
    state['pending'] = path
    state['pending_at'] = time.monotonic()
    state['fired'] = False
    log('SHOT ' + name)


def tick(dt):
    if time.monotonic() - t0 > 420:
        finish(False, 'wall timeout, missing ' + str([s for s in STAGES if s not in state['seen']]))
        return
    phase = state['phase']
    if phase == 0 and time.monotonic() - t0 > 3:
        state['phase'] = 1
        # Temps simule au rythme JS (90 s par jour) : cette preuve attend sur le temps simule ; le jeu, lui, tourne a anastasis.Sim.TimeScale 0.0375 (villager-png-001, point 4).
        unreal.SystemLibrary.execute_console_command(None, 'anastasis.Sim.TimeScale 1')
        les.editor_request_begin_play()
        return
    world = ues.get_game_world()
    if phase == 1:
        if not les.is_in_play_in_editor() or not world or DBG.get_simulation_time(world) < 0:
            return
        unreal.SystemLibrary.execute_console_command(world, 'anastasis.Village.Debug 1')
        # Preuve seulement : sans brouillard, a 20 m la tuile, le village se lit de haut.
        unreal.SystemLibrary.execute_console_command(world, 'showflag.Fog 0')
        unreal.SystemLibrary.execute_console_command(world, 'r.MotionBlurQuality 0')
        unreal.SystemLibrary.execute_console_command(world, 'anastasis.Village.RouteCost 1')
        log('ROUTE_COST 1')
        unreal.SystemLibrary.execute_console_command(world, 'Anastasis.Village.FirstFarmer 1')
        cameras = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.CameraActor)
        state['camera'] = next((c for c in cameras if c.get_actor_label() == 'GatherDeliverProofCamera'), None)
        pc = unreal.GameplayStatics.get_player_controller(world, 0)
        if state['camera'] and pc:
            pc.set_view_target_with_blend(state['camera'], 0.0)
        state['phase'] = 2
        return
    if phase != 2 or not world:
        return

    # Une prise en cours : attendre le fichier, puis relancer la simulation.
    if state['pending'] is not None:
        if not state['fired']:
            if time.monotonic() - state['pending_at'] < 0.6:
                return
            unreal.SystemLibrary.execute_console_command(world, 'Shot')
            state['fired'] = True
            state['pending_at'] = time.monotonic()
            return
        fresh = sorted(existing_shots() - state['before'], key=lambda f: f.stat().st_mtime)
        if fresh and time.monotonic() - state['pending_at'] > 1.0:
            try:
                fresh[-1].replace(state['pending'])
            except OSError:
                return
        if state['pending'].is_file() or time.monotonic() - state['pending_at'] > 20:
            state['shots'].append({'file': state['pending'].name, 'written': state['pending'].is_file()})
            state['pending'] = None
            unreal.SystemLibrary.execute_console_command(world, 'anastasis.Sim.Speed 10')
        return

    s = json.loads(DBG.get_gather_status(world))
    if not s or s.get('granary', -1) < 0:
        finish(False, 'FirstFarmer n\'a pose ni grenier ni fermier')
        return
    total = s['field'] + s['bag'] + s['stock'] + s['meals']
    if state['initial'] is None:
        state['initial'] = total
    if total != state['initial']:
        state['samples'].append(s)
        finish(False, 'conservation rompue : %d != %d' % (total, state['initial']))
        return
    if state['last_sample'] is None or s['time'] - state['last_sample'] >= 2:
        state['samples'].append(s)
        state['last_sample'] = s['time']

    granary = unreal.Vector(s['gx'], s['gy'], s['gz'])
    field = unreal.Vector(s['fx'], s['fy'], s['fz'])
    middle = (granary + field) * 0.5
    farmer = unreal.Vector(s['nx'], s['ny'], s['nz'])
    checks = {
        '01-depart': True,
        '02-recolte': s['session'] and s['bag'] > 0,
        '03-retour': s['goal'] == 'deliver' and s['bag'] > 0,
        '04-livre': s['deliveries'] >= 1,
        '05-large': s['deliveries'] >= 2,
    }
    for name in STAGES:
        if name in state['seen'] or not checks[name]:
            continue
        if name == '03-retour':
            # Au plus un sac plein : le retour est force des que le sac depasse 9.
            if s['bag'] > 10:
                finish(False, 'retour avec %d au sac : le retour force a > 9 n\'a pas joue' % s['bag'])
                return
            state['retour'] = s
        if name == '04-livre':
            # Livraison = le sac du retour, en entier (grenier loin d'etre plein). Ce qui est entre
            # au grenier depuis le retour, repas compris, est exactement ce qui a quitte le sac.
            r = state.get('retour')
            if r is None:
                finish(False, 'livraison sans retour observe')
                return
            moved = (s['stock'] + s['meals']) - (r['stock'] + r['meals'])
            if s['bag'] != 0 or moved != r['bag']:
                finish(False, 'livraison partielle : sac du retour %d, entre au grenier %d, reste au sac %d'
                       % (r['bag'], moved, s['bag']))
                return
        state['seen'].append(name)
        state['samples'].append(dict(s, stage=name))
        log('STAGE ' + name + ' ' + json.dumps(s))
        unreal.SystemLibrary.execute_console_command(world, 'Anastasis.Village.Status')
        # Une tuile = 2000 unites : cadrer en tuiles, pas en metres.
        if name == '05-large':
            aim(world, middle, unreal.Vector(-6000, -8000, 9500))
        elif name in ('02-recolte', '03-retour'):
            aim(world, farmer, unreal.Vector(-1500, -2000, 2400))
        else:
            aim(world, middle, unreal.Vector(-2800, -3700, 4400))
        shoot(world, name)
        return
    if len(state['seen']) == len(STAGES):
        finish(all(x['written'] for x in state['shots']),
               'recolte, retour, livraison et deuxieme voyage observes ; conservation tenue a chaque echantillon')
    elif s['time'] > 600:
        finish(False, 'echeance simulee, etapes manquantes ' + str([n for n in STAGES if n not in state['seen']]))


STAGES = ['01-depart', '02-recolte', '03-retour', '04-livre', '05-large']


def guarded(dt):
    try:
        tick(dt)
    except Exception as exc:
        import traceback
        unreal.log_error(traceback.format_exc())
        finish(False, str(exc))


handle = unreal.register_slate_post_tick_callback(guarded)
