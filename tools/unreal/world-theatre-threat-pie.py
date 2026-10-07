"""WORLD_THEATRE v2.2 -- preuve PIE de la menace lue dans la simulation.

Le monde du jeu (incarnation, village de depart), le monde exterieur simule charge (`Anastasis.Geo.Load`,
geo-pontos-1204.json), simulation gelee entre deux sauts (`anastasis.Sim.TimeScale 0`, puis `Anastasis.Sim.Advance` :
une preuve n'attend pas le temps simule, elle l'avance). Le ciel suit l'horloge de la simulation : 22 h est la nuit.

Chronologie :
  jour 1, 10 h  calme : scenario charge, aucun choc ajoute -> aucun signe.
  injection     un raid venu de Paipert, au-dela de Parcharia, choc de developpeur sans acteur attribue :
                `Anastasis.Geo.Inject paipert Insecurity=0.9 Military=0.5 info=1 duration=4 ...`
  points        alternance 22 h / 11 h sur cinq jours ; a chaque point : etat du theatre (JSON du sous-systeme) et du
                monde exterieur (GetGeoStatus), puis deux images aux memes camera et instant, menace 1 puis 0.

Verdict instrumental (pas artistique) : WORLD_THREAT_PIE PASS si
  (1) au calme, rien n'est allume ;
  (2) un point montre des feux de signaux allumes alors qu'aucune fumee ne l'est (la nouvelle precede le raid) ;
  (3) un point ulterieur montre de la fumee du cote de Parcharia ;
  (4) un point montre de l'effroi (> 0) quand le village est expose ;
  (5) menace a 0, aucun signe n'est visible.
Sortie : ANASTASIS_THREAT_OUT/<point>_<on|off>.png et threat.json. Rien n'est sauve.
"""
import json, os, pathlib, time, unreal

LEVEL = '/Game/Anastasis/Maps/Lvl_AnastasisSlice'
ROOT = pathlib.Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
OUT = pathlib.Path(os.environ.get('ANASTASIS_THREAT_OUT') or (ROOT / 'Saved' / 'WorldTheatreEvidence' / 'threat'))
SHOTS_DIR = ROOT / 'Saved' / 'Screenshots'
CAMERA_LABEL = 'WorldThreatProofCamera'
# Choc de DEVELOPPEUR (provenance dev-intervention, ABSTRACTION) : un raid venu de Paipert, sans acteur attribue. La pression
# turkmene systematique n'est pas attestee pour 1204-1225 (HIS-04, surtout apres 1277) : la preuve ne la revendique pas.
INJECT = ('Anastasis.Geo.Inject paipert Insecurity=0.9 Military=0.5 info=1 duration=4 '
          'id=raid-proof label=raid_venu_de_paipert tags=raid')
POINTS = ['@22', '@11', '@22', '@11', '@22', '@11', '@22', '@11', '@22', '@11']
# Vue du village vers Parcharia (cap 108, la seconde chaine) : feux a 8,5-13 km, fumees a 2,75-10 km.
# L'oeil est celui de la vista V9 (meme point du village, z resolu sur le sol rendu par world-theatre-analyze.py).
_V9 = next(v for v in json.loads((ROOT / 'docs' / 'unreal' / 'world-theatre-001' / 'vistas.json').read_text(encoding='utf-8'))['vistas']
           if v['id'].startswith('V9'))
CAM = {'x': _V9['x'], 'y': _V9['y'], 'z': _V9['z'], 'yaw': 108.0, 'pitch': 2.0, 'fov': 62.0}

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
DBG = unreal.AnastasisSimulationDebugLibrary
V = unreal.Vector
t0 = time.monotonic()
st = {'phase': 'boot', 'mark': t0, 'queue': [], 'finished': False, 'cam': None, 'step': None, 'ab': None}
report = {'points': [], 'inject': INJECT}
handle = None


def log(msg):
    unreal.log('WORLD_THREAT_PIE ' + msg)


def cmd(world, c):
    unreal.SystemLibrary.execute_console_command(world, c)


def theatre(world):
    raw = unreal.AnastasisWorldTheatreLibrary.get_threat_status(world)
    return json.loads(raw) if raw else None


def geo(world):
    try:
        return json.loads(DBG.get_geo_status(world))
    except Exception:  # noqa: BLE001
        return None


def shots():
    return set(SHOTS_DIR.rglob('*.png')) if SHOTS_DIR.exists() else set()


def verdict():
    pts = report['points']
    lit = lambda p, sign: sum(1 for s in p['theatre']['sites'] if s['sign'] == sign and s['intensity'] > 0.01)
    calm = pts and pts[0]['label'] == 'calme' and lit(pts[0], 'beacon') + lit(pts[0], 'smoke') == 0
    news_first = next((i for i, p in enumerate(pts) if lit(p, 'beacon') > 0 and lit(p, 'smoke') == 0), None)
    smoke = next((i for i, p in enumerate(pts) if any(s['sign'] == 'smoke' and s['node'] == 'parcharia' and s['intensity'] > 0.01
                                                      for s in p['theatre']['sites'])), None)
    dread = any(p['theatre']['dread'] > 0.0 for p in pts)
    off_dark = all(p.get('visible_when_off', 0) == 0 for p in pts)
    checks = {'calme_sans_signe': bool(calm), 'feux_avant_fumee': news_first is not None and (smoke is None or news_first < smoke),
              'fumee_parcharia': smoke is not None, 'effroi': dread, 'menace_0_rien_visible': off_dark}
    report['checks'] = checks
    report['first_news_point'] = news_first
    report['first_smoke_point'] = smoke
    return all(checks.values())


def finish(ok, reason):
    if st['finished']:
        return
    st['finished'] = True
    report['reason'] = reason
    try:
        (OUT / 'threat.json').write_text(json.dumps(report, indent=1), encoding='utf-8')
    except Exception:  # noqa: BLE001
        pass
    unreal.SystemLibrary.execute_console_command(None, 'anastasis.Sim.TimeScale 0.0375')
    log(('PASS ' if ok else 'FAIL ') + reason)
    if les.is_in_play_in_editor():
        les.editor_request_end_play()
    unreal.unregister_slate_post_tick_callback(handle)
    unreal.SystemLibrary.quit_editor()


def tick(_dt):
    now = time.monotonic()
    el = now - st['mark']
    if now - t0 > 1500:
        finish(False, 'wall timeout phase=%s' % st['phase'])
        return
    phase = st['phase']
    if phase == 'boot':
        if el > 3:
            les.editor_request_begin_play()
            st['phase'], st['mark'] = 'pie', now
        return
    world = ues.get_game_world()
    if phase == 'pie':
        if not les.is_in_play_in_editor() or not world or DBG.get_simulation_time(world) < 0:
            if el > 300:
                finish(False, 'PIE sans simulation apres 300 s')
            return
        if el < 14:
            return
        for c in ('anastasis.Sim.TimeScale 0', 'r.MotionBlurQuality 0', 't.MaxFPS 30', 'anastasis.Village.Debug 0',
                  'anastasis.Theatre 1', 'anastasis.Theatre.Light 1', 'anastasis.Theatre.Threat 1', 'Anastasis.Geo.Load'):
            cmd(world, c)
        cam = next((c for c in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.CameraActor)
                    if c.get_actor_label() == CAMERA_LABEL), None)
        if cam is None:
            finish(False, 'camera de preuve absente du monde PIE')
            return
        st['cam'] = cam
        st['queue'] = [('calme', '@10', False)] + [('inject', None, True)] + \
                      [('p%02d' % k, adv, False) for k, adv in enumerate(POINTS)]
        st['phase'], st['mark'] = 'next', now
        return
    if phase == 'next':
        if not st['queue']:
            ok = verdict()
            finish(ok, 'points=%d checks=%s' % (len(report['points']), report.get('checks')))
            return
        label, adv, is_inject = st['queue'].pop(0)
        if is_inject:
            cmd(world, INJECT)
            log('INJECT ' + INJECT)
            st['mark'] = now
            return
        cmd(world, 'Anastasis.Sim.Advance ' + adv)
        st['step'] = {'label': label, 'advance': adv}
        st['phase'], st['mark'] = 'settle', now
        return
    if phase == 'settle':
        if el < 5.0:
            return
        cam = st['cam']
        pc = unreal.GameplayStatics.get_player_controller(world, 0)
        if pc:
            pc.set_view_target_with_blend(cam, 0.0)
        cam.set_actor_location(V(CAM['x'], CAM['y'], CAM['z']), False, True)
        cam.set_actor_rotation(unreal.Rotator(roll=0.0, pitch=CAM['pitch'], yaw=CAM['yaw']), True)
        comp = cam.get_component_by_class(unreal.CameraComponent)
        comp.set_editor_property('field_of_view', CAM['fov'])
        comp.set_editor_property('constrain_aspect_ratio', False)
        th = theatre(world)
        if th is None:
            finish(False, 'sous-systeme du theatre introuvable')
            return
        st['step'].update({'theatre': th, 'geo_exposure': (geo(world) or {}).get('exposure'),
                           'sim_time': DBG.get_simulation_time(world)})
        log('POINT %s day=%.3f lit_beacons=%d lit_smoke=%d dread=%.3f sun=%.1f' % (
            st['step']['label'], th['day'], sum(1 for s in th['sites'] if s['sign'] == 'beacon' and s['intensity'] > 0.01),
            sum(1 for s in th['sites'] if s['sign'] == 'smoke' and s['intensity'] > 0.01), th['dread'], th['sun']))
        st['ab'] = 'on'
        st['phase'], st['mark'] = 'fire', now
        return
    if phase == 'fire':
        if el < 2.0:
            return
        st['before'] = shots()
        cmd(world, 'Shot')
        st['fired'] = now
        st['phase'] = 'collect'
        return
    if phase == 'collect':
        fresh = sorted(shots() - st['before'], key=lambda f: f.stat().st_mtime)
        if not fresh:
            if now - st['fired'] > 20:
                finish(False, 'Shot sans fichier')
            return
        dst = OUT / ('%s_%s.png' % (st['step']['label'], st['ab']))
        try:
            if dst.exists():
                dst.unlink()
            fresh[-1].replace(dst)
        except OSError:
            return
        if st['ab'] == 'on':
            cmd(world, 'anastasis.Theatre.Threat 0')
            st['ab'] = 'off'
            st['phase'], st['mark'] = 'fire', now
            return
        th_off = theatre(world)
        st['step']['visible_when_off'] = th_off['visible'] if th_off else -1
        cmd(world, 'anastasis.Theatre.Threat 1')
        report['points'].append(st['step'])
        st['phase'], st['mark'] = 'next', now


def guarded(dt):
    try:
        tick(dt)
    except Exception:  # noqa: BLE001
        import traceback
        unreal.log_error(traceback.format_exc())
        finish(False, 'exception, voir le log')


OUT.mkdir(parents=True, exist_ok=True)
les.load_level(LEVEL)
unreal.get_editor_subsystem(unreal.EditorActorSubsystem).spawn_actor_from_class(
    unreal.CameraActor, V(0, 0, 1000), unreal.Rotator()).set_actor_label(CAMERA_LABEL)
handle = unreal.register_slate_post_tick_callback(guarded)
