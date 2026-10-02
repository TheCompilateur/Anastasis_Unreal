"""FOREST_COST_001 -- ce que coute la vegetation, strate par strate, en ms GPU.

Aucune CVar ne retire les arbres (anastasis.Dressing.Ecology / MacroForest 0 rendent d'autres
forets, pas une carte sans foret). Ce script incarne le monde UNE fois, puis MASQUE a
l'execution les composants instancies d'une strate (set_visibility : ni dessin, ni ombre), aux
memes cameras, dans la meme session. Cout d'une strate = GPU(all) - GPU(strate masquee).
Rien n'est sauve ; les composants sont transitoires.

Strates (classees par le mesh dessine et le nom du composant, inventaire dans le JSON) :
  trees      /Vegetation/SM_Tree*, /Vegetation/Hero/* (arbres heros, coquilles de couronne)
  under      Understory_*, SM_Shrub_*
  grass      GroundCover_*, /GroundCover/*
  bank       Riverbank_*
  micro      MicroEco_*
  rock       /Rock/* (jamais masque : ce n'est pas de la vegetation)
Etats, dans cet ordre : all, notrees, nounder, nograss, bare (toute la vegetation masquee),
all2 (temoin : all refait a la fin ; l'ecart all / all2 est la derive de la machine).

Vues : interieur de foret a 1,7 m (pres d'un tronc de la HISM d'arbres la plus peuplee,
regard vers un voisin), lisiere, prairie, vallee B, oblique haute, aerien (tuiles de
ground-cover-capture.py). Ciel epingle a midi, sec. GPU de stat unit (GetFrameTimingsMs)
echantillonne apres stabilisation (ombres virtuelles recalculees apres chaque masquage).

ANASTASIS_VEGCOST_OUT     dossier de sortie (obligatoire) : vegetation-cost.json + <vue>_<etat>.png
ANASTASIS_VEGCOST_STATES  sous-ensemble ordonne des etats (defaut : tous)
"""
import json
import math
import os
import time

import unreal

OUT = os.environ.get('ANASTASIS_VEGCOST_OUT')
LEVEL = '/Game/Anastasis/Maps/Lvl_AnastasisSlice'
SEED = 12345
VEG = ('trees', 'under', 'grass', 'bank', 'micro')
HIDE = {
    'all': (), 'notrees': ('trees',), 'nounder': ('under',), 'nograss': ('grass',),
    'bare': VEG, 'all2': (),
}
STATES = [s.strip() for s in os.environ.get('ANASTASIS_VEGCOST_STATES', ','.join(HIDE)).split(',') if s.strip()]
SETTLE_FIRST_S, SETTLE_S, SAMPLE_S = 8.0, 4.0, 5.0

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
V = unreal.Vector
handle = None
report = {'level': LEVEL, 'seed': SEED, 'states': STATES, 'inventory': {}, 'views': {}, 'gpu_ms': {}, 'frame_ms': {}}


def finish(msg, error=False):
    (unreal.log_error if error else unreal.log)(msg)
    if OUT:
        with open(os.path.join(OUT, 'vegetation-cost.json'), 'w') as f:
            json.dump(report, f, indent=1)
    if handle is not None:
        unreal.unregister_slate_post_tick_callback(handle)
    unreal.SystemLibrary.quit_editor()


def classify(comp):
    name = comp.get_name()
    mesh = comp.get_editor_property('static_mesh')
    path = mesh.get_path_name() if mesh else ''
    if name.startswith('GroundCover_') or '/GroundCover/' in path:
        return 'grass'
    if name.startswith('Understory_') or '/SM_Shrub' in path:
        return 'under'
    if name.startswith('Riverbank_'):
        return 'bank'
    if name.startswith('MicroEco_'):
        return 'micro'
    if '/Vegetation/SM_Tree' in path or '/Vegetation/Hero/' in path:
        return 'trees'
    if '/Rock/' in path:
        return 'rock'
    return 'other'


try:
    if not OUT:
        raise RuntimeError('ANASTASIS_VEGCOST_OUT manquant')
    if not STATES or any(s not in HIDE for s in STATES):
        raise RuntimeError('ANASTASIS_VEGCOST_STATES invalide : %r' % STATES)
    os.makedirs(OUT, exist_ok=True)
    les.load_level(LEVEL)
    world = ues.get_editor_world()

    def cmd(c):
        unreal.SystemLibrary.execute_console_command(world, c)

    for c in ('ShowFlag.Sprites 0', 'ShowFlag.Grid 0', 'viewmode lit', 'r.MotionBlurQuality 0',
              'anastasis.Sky.Day 1', 'anastasis.Sky.Hour 12', 'anastasis.Sky.Humidity 0'):
        cmd(c)
    emb_cls = unreal.load_class(None, '/Script/Anastasis_UnrealV2.AnastasisWorldEmbodiment')
    found = unreal.GameplayStatics.get_all_actors_of_class(world, emb_cls)
    emb = found[0] if len(found) > 0 else eas.spawn_actor_from_class(emb_cls, V(0, 0, 0), unreal.Rotator(0, 0, 0))
    emb.call_method('EmbodyCanonical', args=(SEED,))
    atm_cls = unreal.load_class(None, '/Script/Anastasis_UnrealV2.AnastasisWorldAtmosphere')
    for a in unreal.GameplayStatics.get_all_actors_of_class(world, atm_cls):
        a.call_method('Apply')

    # Inventaire : toutes les instances de la carte, pas seulement celles de l'incarnation.
    layers = {}
    for actor in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor):
        for comp in actor.get_components_by_class(unreal.InstancedStaticMeshComponent):
            n = comp.get_instance_count()
            if n <= 0 or not comp.is_visible():
                continue
            layer = classify(comp)
            layers.setdefault(layer, []).append(comp)
            inv = report['inventory'].setdefault(layer, {'components': 0, 'instances': 0, 'meshes': {}})
            inv['components'] += 1
            inv['instances'] += n
            mesh = comp.get_editor_property('static_mesh')
            key = mesh.get_name() if mesh else '?'
            inv['meshes'][key] = inv['meshes'].get(key, 0) + n
    for layer, inv in sorted(report['inventory'].items()):
        unreal.log('VEGCOST_INVENTORY %s components=%d instances=%d' % (layer, inv['components'], inv['instances']))
    if not layers.get('trees'):
        raise RuntimeError('aucune instance d arbre trouvee : classement a revoir')

    def trace(a, b):
        hit = unreal.SystemLibrary.line_trace_single(world, a, b, unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, True, [],
                                                     unreal.DrawDebugTrace.NONE, True)
        return hit.to_tuple() if hit else None

    def ground(x, y):
        # Les arbres ont une collision : viser le sol en ignorant la vegetation, par la
        # hauteur du maillage de terrain (premier impact sous un tronc = la couronne).
        t = trace(V(x, y, 200000), V(x, y, -200000))
        z = t[4].z if t else None
        for _ in range(6):
            if t is None or classify_hit(t) not in VEG:
                break
            t = trace(V(x, y, z - 5), V(x, y, -200000))
            z = t[4].z if t else z
        return z

    def classify_hit(t):
        comp = t[10] if len(t) > 10 else None  # (…, hit_actor 9, hit_component 10, …)
        try:
            return classify(comp) if comp and isinstance(comp, unreal.InstancedStaticMeshComponent) else 'ground'
        except Exception:  # noqa: BLE001
            return 'ground'

    W = 0.0
    while ground(W + 1000, 1000) is not None and W < 1.0e6:
        W += 1000
    T = W / 96.0
    if T <= 0:
        raise RuntimeError('sol introuvable : emprise nulle')

    def at(tx, ty, lift):
        z = ground(tx * T, ty * T)
        return None if z is None else V(tx * T, ty * T, z + lift)

    views = []
    # Interieur de foret : la HISM d'arbres la plus peuplee, un tronc pres du milieu de sa liste,
    # l'oeil a 2,5 m du tronc, regard vers le voisin le plus proche entre 15 et 80 m.
    trees = max(layers['trees'], key=lambda c: c.get_instance_count())
    n = trees.get_instance_count()
    pts = []
    for i in range(0, n, max(1, n // 300)):
        got = trees.get_instance_transform(i, True)
        xf = got[1] if isinstance(got, tuple) else got
        pts.append(xf.translation)
    c0 = pts[len(pts) // 2]
    near = [p for p in pts if 1500 < math.hypot(p.x - c0.x, p.y - c0.y) < 8000]
    if near:
        tgt = min(near, key=lambda p: math.hypot(p.x - c0.x, p.y - c0.y))
        dx, dy = tgt.x - c0.x, tgt.y - c0.y
        d = math.hypot(dx, dy) or 1.0
        ez, tz = ground(c0.x - dx / d * 250, c0.y - dy / d * 250), ground(tgt.x, tgt.y)
        if ez is not None and tz is not None:
            views.append(('foret_eye', V(c0.x - dx / d * 250, c0.y - dy / d * 250, ez + 170), V(tgt.x, tgt.y, tz + 300)))
    # (nom, oeil en tuiles, hauteur d'oeil, cible en tuiles, hauteur de cible) : ground-cover-capture.py
    for name, eye_t, lift, tgt_t, tlift in [
        ('lisiere_eye', (37.0, 57.0), 170, (28.0, 58.0), 600),
        ('prairie_eye', (44.0, 52.0), 170, (55.0, 61.0), 120),
        ('vallee_b_eye', (37.0, 27.0), 170, (28.0, 21.0), 300),
        ('oblique', (40.0, 44.0), 3500, (50.0, 58.0), 0),
        ('aerien', (30.0, 34.0), 30000, (52.0, 58.0), 0),
    ]:
        eye, tgt = at(eye_t[0], eye_t[1], lift), at(tgt_t[0], tgt_t[1], tlift)
        if eye is None or tgt is None:
            unreal.log_warning('VEGCOST_SKIP %s sol absent' % name)
            continue
        views.append((name, eye, tgt))
    report['views'] = {n_: [[e.x, e.y, e.z], [t.x, t.y, t.z]] for n_, e, t in views}
    report['tile_uu'] = T
    unreal.log('VEGCOST_VIEWS %d tile_uu=%.1f trees_hism=%s' % (len(views), T, trees.get_name()))
except Exception as exc:  # noqa: BLE001
    finish('VEGCOST_CAPTURE_FAIL %s' % exc, True)
    raise


def apply_state(state):
    hidden = HIDE[state]
    for layer, comps in layers.items():
        if layer not in VEG:
            continue
        for comp in comps:
            comp.set_visibility(layer not in hidden, False)
    unreal.log('VEGCOST_STATE %s hidden=%s' % (state, ','.join(hidden) or '-'))


look = unreal.MathLibrary.find_look_at_rotation
st = {'i': -1, 'queue': [], 'phase': 'boot', 'mark': time.monotonic(), 'first': True, 'gpu': [], 'frames': [], 'shot': None}


def tick(dt):
    now = time.monotonic()
    el = now - st['mark']
    try:
        les.editor_invalidate_viewports()
    except Exception:  # noqa: BLE001
        pass
    if el > 240:
        finish('VEGCOST_CAPTURE_TIMEOUT phase=%s' % st['phase'], True)
        return
    if st['phase'] == 'boot':
        if el > 4:
            st['phase'], st['mark'] = 'aim', now
        return
    if st['phase'] == 'aim':
        if not st['queue']:
            st['i'] += 1
            if st['i'] >= len(STATES):
                finish('VEGCOST_CAPTURE_COMPLETE views=%d states=%d' % (len(views), len(STATES)))
                return
            apply_state(STATES[st['i']])
            st['queue'] = [(STATES[st['i']], v) for v in views]
            st['first'], st['mark'] = True, now
            return
        state, (name, eye, tgt) = st['queue'][0]
        ues.set_level_viewport_camera_info(eye, look(eye, tgt))
        settle = SETTLE_FIRST_S if st['first'] else SETTLE_S
        if el > settle:
            st['frames'].append(dt)
            st['gpu'].append(emb.call_method('GetFrameTimingsMs').z)
        if el > settle + SAMPLE_S:
            key = '%s_%s' % (name, state)
            g, f = sorted(st['gpu']), sorted(st['frames'])
            report['gpu_ms'][key] = round(g[len(g) // 2], 3) if g else -1
            report['frame_ms'][key] = round(1000.0 * f[len(f) // 2], 3) if f else -1
            unreal.log('VEGCOST_VIEW %s gpu_ms_p50=%.2f frame_ms_p50=%.2f samples=%d'
                       % (key, report['gpu_ms'][key], report['frame_ms'][key], len(g)))
            st['gpu'], st['frames'] = [], []
            shot = os.path.join(OUT, key + '.png').replace('\\', '/')
            if os.path.exists(shot):
                os.remove(shot)
            unreal.SystemLibrary.execute_console_command(world, 'HighResShot 1280x720 filename="%s"' % shot)
            st['shot'], st['phase'], st['mark'] = shot, 'wait', now
        return
    if st['phase'] == 'wait':
        if not os.path.isfile(st['shot']):
            if el <= 30:
                return
            # L'image ne sert qu'a verifier le masquage ; la mesure GPU de la vue est deja prise.
            unreal.log_warning('VEGCOST_SHOT_MISSING %s (mesure gardee)' % st['shot'])
            report.setdefault('missing_shots', []).append(os.path.basename(st['shot']))
            st['queue'].pop(0)
            st['first'] = False
            st['phase'], st['mark'] = 'aim', now
            return
        if el > 1:
            st['queue'].pop(0)
            st['first'] = False
            st['phase'], st['mark'] = 'aim', now


def guarded(dt):
    try:
        tick(dt)
    except Exception as exc:  # noqa: BLE001
        import traceback
        unreal.log_error(traceback.format_exc())
        finish('VEGCOST_CAPTURE_FAIL tick %s' % exc, True)


handle = unreal.register_slate_post_tick_callback(guarded)
