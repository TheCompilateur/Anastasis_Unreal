"""WORLD_THEATRE_001 -- captures des vistas canoniques, AVANT / APRES / TEMOIN, dans UN editeur, en PIE.

Le monde du jeu (incarnation de BeginPlay, village de depart), simulation gelee (anastasis.Sim.TimeScale 0),
ciel epingle (anastasis.Sky.Day / Hour / Humidity), exposition du profil d'atmosphere du jeu, aucune lumiere
ajoutee. Pour chaque vista (tools/unreal/world-theatre-vistas.json : position, cap, inclinaison, FOV), une image
par etat de la couche : `anastasis.Theatre` 0, 1, puis 0 (temoin : meme camera, meme etat que le premier ;
l'ecart off/off2 mesure le bruit de capture). Images par `Shot` (la vue affichee, TSR compris).

Sortie : ANASTASIS_THEATRE_OUT/<vista>_<etat>.png et capture.json (cameras, etats, rapport de la couche).
Variables :
  ANASTASIS_THEATRE_OUT     dossier (obligatoire)
  ANASTASIS_THEATRE_VISTAS  fichier de vistas (defaut tools/unreal/world-theatre-vistas.json)
  ANASTASIS_THEATRE_ONLY    ids de vistas separes par ',' (defaut : toutes)
  ANASTASIS_THEATRE_STATES  etats separes par ',' parmi off,on,off2 (defaut off,on,off2)
  ANASTASIS_THEATRE_HOUR    heure du ciel (defaut 11 : lumiere neutre, ni rasante ni zenithale)
Verdict : WORLD_THEATRE_CAPTURE COMPLETE (toutes les images ecrites) ou FAIL <raison>. Technique, pas artistique.
Rien n'est sauve.
"""
import json, os, pathlib, time, unreal

LEVEL = '/Game/Anastasis/Maps/Lvl_AnastasisSlice'
ROOT = pathlib.Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
OUT = pathlib.Path(os.environ.get('ANASTASIS_THEATRE_OUT', ''))
VISTAS = pathlib.Path(os.environ.get('ANASTASIS_THEATRE_VISTAS') or (ROOT / 'tools' / 'unreal' / 'world-theatre-vistas.json'))
ONLY = [v for v in os.environ.get('ANASTASIS_THEATRE_ONLY', '').split(',') if v]
STATES = [s for s in os.environ.get('ANASTASIS_THEATRE_STATES', 'off,on,off2').split(',') if s]
HOUR = os.environ.get('ANASTASIS_THEATRE_HOUR', '11')
SHOTS_DIR = ROOT / 'Saved' / 'Screenshots'
CAMERA_LABEL = 'WorldTheatreProofCamera'
SETTLE_FIRST_S = 10.0
SETTLE_S = 4.0  # couche rebatie (sondage 0,5 s x 3) + historique TSR

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
DBG = unreal.AnastasisSimulationDebugLibrary
V = unreal.Vector
t0 = time.monotonic()
st = {'phase': 'boot', 'mark': t0, 'camera': None, 'queue': [], 'finished': False, 'settled_once': False}
report = {'states': STATES, 'hour': HOUR, 'shots': {}, 'vistas': {}}
handle = None


def log(msg):
    unreal.log('WORLD_THEATRE_CAPTURE ' + msg)


def finish(ok, reason):
    if st['finished']:
        return
    st['finished'] = True
    report['complete'] = ok
    report['reason'] = reason
    try:
        (OUT / 'capture.json').write_text(json.dumps(report, indent=1), encoding='utf-8')
    except Exception:  # noqa: BLE001
        pass
    log(('COMPLETE ' if ok else 'FAIL ') + reason)
    if les.is_in_play_in_editor():
        les.editor_request_end_play()
    unreal.unregister_slate_post_tick_callback(handle)
    unreal.SystemLibrary.quit_editor()


def shots():
    return set(SHOTS_DIR.rglob('*.png')) if SHOTS_DIR.exists() else set()


def cmd(world, c):
    unreal.SystemLibrary.execute_console_command(world, c)


def state_cmds(state):
    return ['anastasis.Theatre %d' % (1 if state == 'on' else 0)]


def tick(_dt):
    now = time.monotonic()
    el = now - st['mark']
    if now - t0 > 1500:
        finish(False, 'wall timeout phase=%s' % st['phase'])
        return
    phase = st['phase']
    if phase == 'boot':
        if el > 3:
            for c in ('anastasis.Sky.Day 1', 'anastasis.Sky.Hour ' + HOUR, 'anastasis.Sky.Humidity 0', 'anastasis.Theatre 0'):
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
        if el < 12:  # incarnation, village de depart, couches de contact
            return
        for c in ('anastasis.Sim.TimeScale 0', 'r.MotionBlurQuality 0', 't.MaxFPS 30', 'anastasis.Village.Debug 0'):
            cmd(world, c)
        vistas = json.loads(VISTAS.read_text(encoding='utf-8'))['vistas']
        vistas = [v for v in vistas if not ONLY or v['id'] in ONLY]
        if not vistas:
            finish(False, 'aucune vista')
            return
        cam = next((c for c in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.CameraActor)
                    if c.get_actor_label() == CAMERA_LABEL), None)
        if cam is None:
            finish(False, 'camera de preuve absente du monde PIE')
            return
        st['camera'] = cam
        pc = unreal.GameplayStatics.get_player_controller(world, 0)
        if pc:
            pc.set_view_target_with_blend(cam, 0.0)
        # Etat d'abord, vistas ensuite : la couche ne se rebatit qu'une fois par etat.
        st['queue'] = [(s, v) for s in STATES for v in vistas]
        st['current_state'] = None
        report['vistas'] = {v['id']: v for v in vistas}
        st['phase'], st['mark'] = 'next', now
        return
    if phase == 'next':
        if not st['queue']:
            cmd(world, 'anastasis.Theatre.Status')
            finish(True, 'images=%d' % len(report['shots']))
            return
        state, v = st['queue'][0]
        settle = SETTLE_S
        if state != st['current_state']:
            for c in state_cmds(state):
                cmd(world, c)
            st['current_state'] = state
            settle = SETTLE_S + 4.0
        cam = st['camera']
        cam.set_actor_location(V(v['x'], v['y'], v['z']), False, True)
        cam.set_actor_rotation(unreal.Rotator(roll=0.0, pitch=v.get('pitch', 0.0), yaw=v['yaw']), True)
        comp = cam.get_component_by_class(unreal.CameraComponent)
        comp.set_editor_property('field_of_view', float(v.get('fov', 75.0)))
        comp.set_editor_property('constrain_aspect_ratio', False)
        st['settle'] = settle if st['settled_once'] else SETTLE_FIRST_S
        st['phase'], st['mark'] = 'settle', now
        return
    if phase == 'settle':
        if el > st['settle']:
            st['settled_once'] = True
            st['before'] = shots()
            cmd(world, 'Shot')
            st['fired_at'] = now
            st['phase'] = 'collect'
        return
    if phase == 'collect':
        fresh = sorted(shots() - st['before'], key=lambda f: f.stat().st_mtime)
        if not fresh:
            if now - st['fired_at'] > 20:
                finish(False, 'Shot sans fichier')
            return
        state, v = st['queue'][0]
        dst = OUT / ('%s_%s.png' % (v['id'], state))
        try:
            if dst.exists():
                dst.unlink()
            fresh[-1].replace(dst)
        except OSError:
            return
        report['shots']['%s_%s' % (v['id'], state)] = str(dst)
        log('SHOT %s %s' % (v['id'], state))
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
    unreal.log_error('WORLD_THEATRE_CAPTURE FAIL ANASTASIS_THEATRE_OUT manquant')
    unreal.SystemLibrary.quit_editor()
else:
    OUT.mkdir(parents=True, exist_ok=True)
    les.load_level(LEVEL)
    unreal.get_editor_subsystem(unreal.EditorActorSubsystem).spawn_actor_from_class(
        unreal.CameraActor, V(0, 0, 1000), unreal.Rotator()).set_actor_label(CAMERA_LABEL)
    handle = unreal.register_slate_post_tick_callback(guarded)
