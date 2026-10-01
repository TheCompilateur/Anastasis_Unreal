"""Preuve PIE du chantier (build-001) : un batiment monte piece par piece.

Lance par build-site-pie.ps1 (editeur discret, rendu hors focus). Rien n'est sauve.
Sortie : ANASTASIS_BUILD_OUT (defaut Saved/SliceEvidence/build-site).
Type et nombre de batisseurs : ANASTASIS_BUILD_TYPE (house), ANASTASIS_BUILD_BUILDERS (2).

Pilote par l'etat de la simulation, jamais par l'horloge murale : PIE, puis
`Anastasis.Village.FirstSite`, puis une camera au-dessus du chantier. Chaque etape
observee est capturee (`Shot`, simulation figee le temps de la prise par `anastasis.Sim.TimeScale 0`, puis relancee) :

  01-ouvert      le chantier vient d'etre ouvert, devis livre, batisseurs au seuil
  02-fondations  piquets, cordeau et fondation poses (6 pieces)
  03-murs        poteaux, lisse, premieres assises (12 pieces)
  04-toit        chevrons et sous-toiture (19 pieces)
  05-acheve      22 pieces : le batiment est acheve

A chaque echantillon : pose + stock du site == devis livre, et les pieces comptees par
les habitants == les pieces du chantier. Une etape manquante echoue ; jamais de PASS raconte.
"""
import json
import os
import time
from pathlib import Path

import unreal

ROOT = Path(unreal.Paths.project_dir())
OUT = Path(os.environ.get('ANASTASIS_BUILD_OUT', str(ROOT / 'Saved' / 'SliceEvidence' / 'build-site')))
OUT.mkdir(parents=True, exist_ok=True)
TYPE = os.environ.get('ANASTASIS_BUILD_TYPE', 'house')
BUILDERS = int(os.environ.get('ANASTASIS_BUILD_BUILDERS', '2'))
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
DBG = unreal.AnastasisSimulationDebugLibrary
les.load_level('/Game/Anastasis/Maps/Lvl_AnastasisSlice')
# Camera non sauvee, dupliquee avec le niveau dans le monde PIE.
proof_camera = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).spawn_actor_from_class(
    unreal.CameraActor, unreal.Vector(0, 0, 1000), unreal.Rotator())
proof_camera.set_actor_label('BuildSiteProofCamera')

t0 = time.monotonic()
state = {'phase': 0, 'camera': None, 'pending': None, 'pending_at': 0.0,
         'shots': [], 'seen': [], 'samples': [], 'last_sample': None, 'finished': False}
handle = None
STAGES = ['01-ouvert', '02-fondations', '03-murs', '04-toit', '05-acheve']


def log(msg):
    unreal.log('BUILD_SITE ' + msg)


def finish(ok, reason):
    if state['finished']:
        return
    state['finished'] = True
    report = {'pass': ok, 'reason': reason, 'type': TYPE, 'builders': BUILDERS,
              'seen': state['seen'], 'shots': state['shots'], 'samples': state['samples']}
    (OUT / 'build-site.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    log(('PASS ' if ok else 'FAIL ') + reason)
    unreal.unregister_slate_post_tick_callback(handle)
    unreal.SystemLibrary.quit_editor()


def aim(target, offset):
    position = target + offset
    camera = state['camera']
    camera.set_actor_location(position, False, True)
    camera.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(position, target), True)


SHOTS_DIR = ROOT / 'Saved' / 'Screenshots'


def existing_shots():
    return set(SHOTS_DIR.rglob('*.png')) if SHOTS_DIR.exists() else set()


def shoot(world, name):
    # `Shot` capture la vue AFFICHEE, overlay de debug compris ; la simulation est
    # figee, la camera se pose 0,6 s, puis la prise ; le fichier est deplace.
    path = OUT / (name + '.png')
    if path.exists():
        path.unlink()
    # `Sim.Speed 0` ne gele rien (PumpFrame lit < 1 comme 1) : geler par l'echelle de temps.
    unreal.SystemLibrary.execute_console_command(world, 'anastasis.Sim.TimeScale 0')
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
        les.editor_request_begin_play()
        return
    world = ues.get_game_world()
    if phase == 1:
        if not les.is_in_play_in_editor() or not world or DBG.get_simulation_time(world) < 0:
            return
        unreal.SystemLibrary.execute_console_command(world, 'anastasis.Village.Debug 1')
        unreal.SystemLibrary.execute_console_command(world, 'showflag.Fog 0')
        unreal.SystemLibrary.execute_console_command(world, 'r.MotionBlurQuality 0')
        # Le temps simule a l'echelle 1 (le jeu tourne a 0,0375) : la preuve attend sur lui.
        unreal.SystemLibrary.execute_console_command(world, 'anastasis.Sim.Speed 1')
        unreal.SystemLibrary.execute_console_command(world, 'anastasis.Sim.TimeScale 1')
        unreal.SystemLibrary.execute_console_command(world, 'Anastasis.Village.FirstSite %s %d 1' % (TYPE, BUILDERS))
        cameras = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.CameraActor)
        state['camera'] = next((c for c in cameras if c.get_actor_label() == 'BuildSiteProofCamera'), None)
        pc = unreal.GameplayStatics.get_player_controller(world, 0)
        if state['camera'] and pc:
            pc.set_view_target_with_blend(state['camera'], 0.0)
        state['phase'] = 2
        return
    if phase != 2 or not world:
        return

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
            unreal.SystemLibrary.execute_console_command(world, 'anastasis.Sim.TimeScale 1')
        return

    s = json.loads(DBG.get_build_status(world))
    if not s.get('site'):
        finish(False, 'FirstSite n\'a pas ouvert de chantier')
        return
    if s['consumedWood'] + s['stockWood'] != s['needWood'] or s['consumedStone'] + s['stockStone'] != s['needStone']:
        state['samples'].append(s)
        finish(False, 'devis rompu : %s' % json.dumps(s))
        return
    if s['byNpcs'] != s['pieces']:
        state['samples'].append(s)
        finish(False, 'pieces comptees %d != pieces posees %d' % (s['byNpcs'], s['pieces']))
        return
    if state['last_sample'] is None or s['time'] - state['last_sample'] >= 1:
        state['samples'].append(s)
        state['last_sample'] = s['time']

    site = unreal.Vector(s['sx'], s['sy'], s['sz'])
    checks = {
        '01-ouvert': True,
        '02-fondations': s['pieces'] >= 6,
        '03-murs': s['pieces'] >= 12,
        '04-toit': s['pieces'] >= 19,
        '05-acheve': s['completed'],
    }
    for name in STAGES:
        if name in state['seen'] or not checks[name]:
            continue
        state['seen'].append(name)
        state['samples'].append(dict(s, stage=name))
        log('STAGE ' + name + ' ' + json.dumps(s))
        unreal.SystemLibrary.execute_console_command(world, 'Anastasis.Village.Status')
        # Une tuile = 2000 unites : cadrer en tuiles.
        aim(site, unreal.Vector(-1000, -1300, 900))
        shoot(world, name)
        return
    if len(state['seen']) == len(STAGES):
        finish(all(x['written'] for x in state['shots']),
               'ouverture, fondations, murs, toit et achevement observes ; devis et pieces tenus a chaque echantillon')
    elif s['time'] > 600:
        finish(False, 'echeance simulee, etapes manquantes ' + str([n for n in STAGES if n not in state['seen']]))


def guarded(dt):
    try:
        tick(dt)
    except Exception as exc:
        import traceback
        unreal.log_error(traceback.format_exc())
        finish(False, str(exc))


handle = unreal.register_slate_post_tick_callback(guarded)
