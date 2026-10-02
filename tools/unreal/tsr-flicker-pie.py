"""TSR_FLICKER_001 -- scintillement de l'herbe et des branches, images successives en PIE.

Une capture fixe ne montre pas un scintillement : il est TEMPOREL. Ce script pose une camera
fixe en PIE (vrai rendu du jeu, TSR et son historique), laisse le vent animer l'herbe et les
arbres, et prend N images successives par `Shot` -- la vue AFFICHEE, apres anticrenelage
temporel. HighResShot refait un rendu a part : il ne verrait pas l'historique du TSR.

Etats (une seule variable : r.TSR.ThinGeometryDetection), dans cet ordre a chaque vue :
  d0   detection de la geometrie fine coupee (defaut du moteur 5.8.2)
  d1   detection active
  d0b  temoin : d0 refait apres d1 ; l'ecart d0 / d0b est le bruit de la mesure (vent,
       Lumen, machine), l'effet ne se revendique qu'au-dela.
Ou `ANASTASIS_TSR_STATES` : "etiquette=cvar;cvar|..." pour un autre A/B.

Vues : prairie a 1,7 m, prairie au ras du sol, lisiere de foret (memes tuiles que
ground-cover-capture.py, Human_Geography_V2). Ciel epingle a midi (Sky.Day 1, Sky.Hour 12,
Sky.Humidity 0), simulation gelee (anastasis.Sim.TimeScale 0) : seul le vent bouge, il suit
le temps du rendu, pas celui de la simulation. Flou de mouvement coupe ; t.MaxFPS 30 pour un
pas d'image regulier.

Mesure hors editeur : python tools/unreal/tsr-flicker-metrics.py <dossier>.

ANASTASIS_TSR_OUT     dossier de sortie (obligatoire) : <vue>_<etat>/f00.png ... + tsr-flicker.json
ANASTASIS_TSR_FRAMES  images par sequence (defaut 24)
ANASTASIS_TSR_VIEWS   sous-ensemble de vues (prairie_eye,prairie_low,lisiere_eye), vide = toutes
"""
import json
import math
import os
import time
from pathlib import Path

import unreal

ROOT = Path(unreal.Paths.project_dir())
OUT = Path(os.environ.get('ANASTASIS_TSR_OUT', ''))
FRAMES = int(os.environ.get('ANASTASIS_TSR_FRAMES', '24'))
ONLY = [v.strip() for v in os.environ.get('ANASTASIS_TSR_VIEWS', '').split(',') if v.strip()]
LEVEL = '/Game/Anastasis/Maps/Lvl_AnastasisSlice'
SHOTS_DIR = ROOT / 'Saved' / 'Screenshots'
SETTLE_S = 3.0  # historique TSR + compilation eventuelle du shader de detection
CAMERA_LABEL = 'TsrFlickerProofCamera'


def parse_states(raw):
    out = []
    for s in raw.split('|'):
        if '=' in s:
            label, cvars = s.split('=', 1)
            out.append((label.strip(), [c.strip() for c in cvars.split(';') if c.strip()]))
    return out


STATES = parse_states(os.environ.get('ANASTASIS_TSR_STATES', '')) or [
    ('d0', ['r.TSR.ThinGeometryDetection 0']),
    ('d1', ['r.TSR.ThinGeometryDetection 1']),
    ('d0b', ['r.TSR.ThinGeometryDetection 0']),
]
# (nom, oeil en tuiles, hauteur d'oeil cm, cible en tuiles, hauteur de cible cm) : ground-cover-capture.py
PLAN = [
    ('prairie_eye', (44.0, 52.0), 170, (55.0, 61.0), 120),
    ('prairie_low', (47.0, 54.0), 60, (52.0, 58.0), 40),
    ('lisiere_eye', (37.0, 57.0), 170, (28.0, 58.0), 600),
]

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
DBG = unreal.AnastasisSimulationDebugLibrary
V = unreal.Vector

t0 = time.monotonic()
st = {'phase': 'boot', 'mark': t0, 'camera': None, 'queue': [], 'seq': None, 'frame': 0,
      'before': None, 'fired_at': 0.0, 'times': [], 'finished': False}
report = {'level': LEVEL, 'frames': FRAMES, 'states': [[l, c] for l, c in STATES], 'views': {}, 'sequences': {}}
handle = None


def log(msg):
    unreal.log('TSR_FLICKER ' + msg)


def finish(ok, reason):
    if st['finished']:
        return
    st['finished'] = True
    report['pass'] = ok
    report['reason'] = reason
    if OUT.name:
        (OUT / 'tsr-flicker.json').write_text(json.dumps(report, indent=1), encoding='utf-8')
    (unreal.log if ok else unreal.log_error)('TSR_FLICKER ' + ('COMPLETE ' if ok else 'FAIL ') + reason)
    unreal.unregister_slate_post_tick_callback(handle)
    unreal.SystemLibrary.quit_editor()


def shots():
    return set(SHOTS_DIR.rglob('*.png')) if SHOTS_DIR.exists() else set()


def cmd(world, c):
    unreal.SystemLibrary.execute_console_command(world, c)


def views_in(world):
    def ground(x, y):
        hit = unreal.SystemLibrary.line_trace_single(world, V(x, y, 200000), V(x, y, -200000),
                                                     unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, True, [],
                                                     unreal.DrawDebugTrace.NONE, True)
        return hit.to_tuple()[4].z if hit else None

    w = 0.0
    while ground(w + 1000, 1000) is not None and w < 1.0e6:
        w += 1000
    tile = w / 96.0
    if tile <= 0:
        raise RuntimeError('sol introuvable en PIE : emprise nulle')
    out = []
    for name, eye_t, lift, tgt_t, tlift in PLAN:
        if ONLY and name not in ONLY:
            continue
        ez, tz = ground(eye_t[0] * tile, eye_t[1] * tile), ground(tgt_t[0] * tile, tgt_t[1] * tile)
        if ez is None or tz is None:
            log('SKIP %s sol absent' % name)
            continue
        out.append((name, V(eye_t[0] * tile, eye_t[1] * tile, ez + lift), V(tgt_t[0] * tile, tgt_t[1] * tile, tz + tlift)))
    report['tile_uu'] = tile
    return out


def tick(_dt):
    now = time.monotonic()
    el = now - st['mark']
    if now - t0 > 1500:
        finish(False, 'wall timeout phase=%s' % st['phase'])
        return
    phase = st['phase']
    if phase == 'boot':
        if el > 3:
            for c in ('anastasis.Sky.Day 1', 'anastasis.Sky.Hour 12', 'anastasis.Sky.Humidity 0'):
                cmd(None, c)
            les.editor_request_begin_play()
            st['phase'], st['mark'] = 'pie', now
        return
    world = ues.get_game_world()
    if phase == 'pie':
        if not les.is_in_play_in_editor() or not world or DBG.get_simulation_time(world) < 0:
            if el > 300:
                finish(False, 'PIE sans simulation apres 300 s')
            return
        if el < 4:  # incarnation du monde (terrain, vegetation) avant les traces
            return
        for c in ('anastasis.Sim.TimeScale 0', 'r.MotionBlurQuality 0', 't.MaxFPS 30'):
            cmd(world, c)
        views = views_in(world)
        if not views:
            finish(False, 'aucune vue')
            return
        report['views'] = {n: [[e.x, e.y, e.z], [t.x, t.y, t.z]] for n, e, t in views}
        report['anti_aliasing_method'] = unreal.SystemLibrary.get_console_variable_int_value('r.AntiAliasingMethod')
        log('VIEWS %d tile_uu=%.1f aa_method=%d' % (len(views), report['tile_uu'], report['anti_aliasing_method']))
        cameras = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.CameraActor)
        cam = next((c for c in cameras if c.get_actor_label() == CAMERA_LABEL), None)
        if cam is None:
            finish(False, 'camera de preuve absente du monde PIE')
            return
        st['camera'] = cam
        pc = unreal.GameplayStatics.get_player_controller(world, 0)
        if pc:
            pc.set_view_target_with_blend(cam, 0.0)
        st['queue'] = [(v, s) for v in views for s in STATES]
        st['phase'], st['mark'] = 'next', now
        return
    if phase == 'next':
        if not st['queue']:
            finish(True, 'sequences=%d frames=%d' % (len(report['sequences']), FRAMES))
            return
        (name, eye, tgt), (label, cvars) = st['queue'][0]
        for c in cvars:
            cmd(world, c)
        st['camera'].set_actor_location(eye, False, True)
        st['camera'].set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(eye, tgt), True)
        seq = '%s_%s' % (name, label)
        (OUT / seq).mkdir(parents=True, exist_ok=True)
        for old in (OUT / seq).glob('*.png'):
            old.unlink()
        st['seq'], st['frame'], st['times'] = seq, 0, []
        report['sequences'][seq] = {'view': name, 'state': label, 'cvars': cvars,
                                    'readback': {c.split()[0]: unreal.SystemLibrary.get_console_variable_int_value(c.split()[0])
                                                 for c in cvars if c.startswith('r.')}}
        st['phase'], st['mark'] = 'settle', now
        return
    if phase == 'settle':
        if el > SETTLE_S:
            st['phase'] = 'fire'
        return
    if phase == 'fire':
        st['before'] = shots()
        cmd(world, 'Shot')
        st['fired_at'] = now
        st['phase'] = 'collect'
        return
    if phase == 'collect':
        fresh = sorted(shots() - st['before'], key=lambda f: f.stat().st_mtime)
        if not fresh:
            if now - st['fired_at'] > 20:
                finish(False, 'Shot sans fichier : %s f%02d' % (st['seq'], st['frame']))
            return
        dst = OUT / st['seq'] / ('f%02d.png' % st['frame'])
        try:
            fresh[-1].replace(dst)
        except OSError:
            return  # fichier encore ouvert par l'ecriture : prochain tick
        st['times'].append(st['fired_at'])
        st['frame'] += 1
        if st['frame'] < FRAMES:
            st['phase'] = 'fire'
            return
        t = st['times']
        gaps = [b - a for a, b in zip(t, t[1:])]
        report['sequences'][st['seq']]['gap_s'] = {'mean': sum(gaps) / len(gaps) if gaps else 0,
                                                   'max': max(gaps) if gaps else 0}
        log('SEQ %s frames=%d gap_mean=%.3fs' % (st['seq'], st['frame'], report['sequences'][st['seq']]['gap_s']['mean']))
        st['queue'].pop(0)
        st['phase'], st['mark'] = 'next', now


def guarded(dt):
    try:
        tick(dt)
    except Exception:  # noqa: BLE001
        import traceback
        unreal.log_error(traceback.format_exc())
        finish(False, 'exception, voir le log')


if not OUT.name:
    unreal.log_error('TSR_FLICKER FAIL ANASTASIS_TSR_OUT manquant')
    unreal.SystemLibrary.quit_editor()
else:
    OUT.mkdir(parents=True, exist_ok=True)
    les.load_level(LEVEL)
    # Camera non sauvee, dupliquee avec le niveau dans le monde PIE (cf. gather-deliver-pie.py).
    unreal.get_editor_subsystem(unreal.EditorActorSubsystem).spawn_actor_from_class(
        unreal.CameraActor, V(0, 0, 1000), unreal.Rotator()).set_actor_label(CAMERA_LABEL)
    handle = unreal.register_slate_post_tick_callback(guarded)
