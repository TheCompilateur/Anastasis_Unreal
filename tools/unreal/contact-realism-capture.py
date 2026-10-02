"""AAA_CONTACT_REALISM_001 -- la peau de contact, avant / apres, aux memes cameras.

Meme monde (graine 12345), meme ciel epingle (jour 1, 16 h 30), memes cameras : seule variable,
anastasis.Contact.Realism (1 = decalques DBuffer et cailloux de contact, 0 = le monde d'avant).
Le premier etat incarne le monde, rebatit la couche, ecrit son plan (plan.json) et ELIT LA ZONE
D'EPREUVE : la case de 40 m qui porte a la fois de l'eau (bandes de rive), des arbres (raccord de
pied) et, si possible, un rocher et des roseaux. Les cameras en sortent, une fois, et tous les
etats sont pris aux memes :
  z<N>_arbre     1,7 m, a ~7 m d'un arbre de la zone, regard vers son pied et l'eau
  rive_proche    ~1,1 m, a ~3 m de la ligne d'eau, regard rasant vers l'eau
  rive_pierre    idem sur une rive de pierre, si la zone en a une
  rocher         ~1,1 m, a ~3 m d'un rocher qui a sa collerette
  humain_20m     1,7 m, a ~20 m de la zone, regard vers elle
  paysage_80m    35 m de haut, a ~80 m, regard vers la zone

Etats (ANASTASIS_ACR_STATES, defaut "contact,base,contact2") :
    contact    anastasis.Contact.Realism 1
    base       anastasis.Contact.Realism 0
    contact2   repetition de "contact" : l'ecart contact / contact2 mesure la derive de capture

Pour chaque vue : duree de frame p50 / p95 et GPU p50 (stat unit). L'ecart contact / base a la
meme camera est le cout GPU de la couche.

ANASTASIS_ACR_OUT     dossier de sortie (obligatoire)
ANASTASIS_ACR_STATES  etats, dans l'ordre
ANASTASIS_ACR_VIEWS   noms de vues (virgules) ; vide = toutes
ANASTASIS_ACR_PROFILE "1" : un ProfileGPU par vue
"""
import os, time, math, json, unreal

OUT = os.environ.get('ANASTASIS_ACR_OUT')
STATE_CMDS = {
    'contact': ('anastasis.Contact.Realism 1',),
    'base': ('anastasis.Contact.Realism 0',),
    'contact2': ('anastasis.Contact.Realism 1',),
}
COMMON = ('ShowFlag.Sprites 0', 'ShowFlag.Grid 0', 'viewmode lit', 'ShowFlag.Fog 1', 'ShowFlag.VolumetricFog 1',
          'anastasis.Sky.Day 1', 'anastasis.Sky.Hour 16.5')
states = [x.strip() for x in os.environ.get('ANASTASIS_ACR_STATES', 'contact,base,contact2').split(',') if x.strip()]
PROFILE = os.environ.get('ANASTASIS_ACR_PROFILE', '0') == '1'
LEVEL = '/Game/Anastasis/Maps/Lvl_AnastasisSlice'
SEED = 12345
V = unreal.Vector

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
handle = None


def finish(msg, error=False):
    (unreal.log_error if error else unreal.log)(msg)
    if handle is not None:
        unreal.unregister_slate_post_tick_callback(handle)
    unreal.SystemLibrary.quit_editor()


try:
    if not OUT:
        raise RuntimeError('ANASTASIS_ACR_OUT manquant')
    if not states or any(x not in STATE_CMDS for x in states):
        raise RuntimeError('ANASTASIS_ACR_STATES invalide : %r' % states)
    os.makedirs(OUT, exist_ok=True)
    if ues.get_editor_world().get_path_name().split('.')[0] != LEVEL:
        les.load_level(LEVEL)
    world = ues.get_editor_world()

    def cmd(c):
        unreal.SystemLibrary.execute_console_command(world, c)

    plan_path = os.path.join(OUT, 'plan.json').replace('\\', '/')
    if os.path.exists(plan_path):
        os.remove(plan_path)
    for c in COMMON + STATE_CMDS[states[0]]:
        cmd(c)
    cls = unreal.load_class(None, '/Script/Anastasis_UnrealV2.AnastasisWorldEmbodiment')
    found = unreal.GameplayStatics.get_all_actors_of_class(world, cls)
    actor = found[0] if len(found) > 0 else eas.spawn_actor_from_class(cls, V(0, 0, 0), unreal.Rotator(0, 0, 0))
    actor.call_method('EmbodyCanonical', args=(SEED,))
    cmd('anastasis.Contact.Rebuild')
    cmd('anastasis.Contact.Dump %s' % plan_path)
    if not os.path.isfile(plan_path):
        raise RuntimeError('plan de contact non ecrit : %s' % plan_path)
    with open(plan_path, encoding='utf-8') as f:
        plan = json.load(f)
    decals = plan['decals']
    unreal.log('ACR_CAPTURE_PLAN decals=%d pebbles=%d' % (len(decals), len(plan['pebbles'])))
    if not decals:
        raise RuntimeError('plan vide : la couche n\'a rien pose (materiaux ? incarnation ?)')

    def trace(a, b):
        hit = unreal.SystemLibrary.line_trace_single(world, a, b, unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, True, [],
                                                     unreal.DrawDebugTrace.NONE, True)
        return hit.to_tuple() if hit else None

    def ground(x, y):
        t = trace(V(x, y, 200000), V(x, y, -200000))
        return t[4].z if t else None

    SHORE_KINDS = ('WetBand', 'Mud', 'StoneWet', 'ReedBed')
    TREE_KINDS = ('ContactDark', 'Litter')
    CELL = 6000.0
    cells = {}
    for d in decals:
        key = (int(d[2] // CELL), int(d[3] // CELL))
        c = cells.setdefault(key, {'shore': [], 'tree': [], 'rock': [], 'reed': [], 'stone': []})
        if d[0] in SHORE_KINDS and d[1] != 'SHORE_NONE':
            c['shore'].append(d)
            if d[0] == 'StoneWet':
                c['stone'].append(d)
            if d[0] == 'ReedBed':
                c['reed'].append(d)
        elif d[0] in TREE_KINDS:
            c['tree'].append(d)
        elif d[0] == 'RockDirt':
            c['rock'].append(d)

    def score(c):
        if not c['shore'] or not c['tree']:
            return -1.0
        return len(c['shore']) * math.sqrt(len(c['tree'])) + 3.0 * (len(c['rock']) > 0) + 2.0 * (len(c['reed']) > 0) + 1.0 * (len(c['stone']) > 0)

    ranked = [kv for kv in sorted(cells.items(), key=lambda kv: -score(kv[1])) if score(kv[1]) >= 0]
    if not ranked:
        raise RuntimeError('aucune case n\'a a la fois de l\'eau et des arbres')

    def nearest(rows, x, y, maxd=1.0e9):
        best = None
        for d in rows:
            dist = math.hypot(d[2] - x, d[3] - y)
            if dist < maxd and (best is None or dist < best[0]):
                best = (dist, d)
        return best[1] if best else None

    def away(d):
        return (math.cos(math.radians(d[7])), math.sin(math.radians(d[7]))) if d[8] else (1.0, 0.0)

    def cam(name, x, y, lift, tx, ty, tlift=0.0, known_z=None):
        # known_z : altitude du sol connue (decalque de pied d'arbre). Un arbre a une collision : le trace
        # de sol touchait son feuillage et la camera regardait la couronne (capture fourth).
        gz, tz = (known_z, known_z) if known_z is not None else (ground(x, y), ground(tx, ty))
        if gz is None or tz is None:
            return None
        return (name, V(x, y, gz + lift), V(tx, ty, tz + tlift))

    shore_rows = [d for d in decals if d[0] in SHORE_KINDS and d[8]]
    reed_rows = [d for d in decals if d[0] == 'ReedBed']
    # Un arbre (ou une souche) porte une zone sombre ET une nappe de litiere ; un buisson ou une bille
    # seulement la zone sombre : on elit l'arbre (Source 0) par sa litiere voisine.
    litters = [d for d in decals if d[0] == 'Litter']
    all_trees = [d for d in decals if d[0] == 'ContactDark' and len(d) > 9 and d[9] == 0 and nearest(litters, d[2], d[3], 900.0)]

    # Zones d'epreuve : les N meilleures cases, a plus de 120 m les unes des autres (ANASTASIS_ACR_ZONES).
    n_zones = int(os.environ.get('ANASTASIS_ACR_ZONES', '2'))
    zones = []
    for key, zc in ranked:
        cx, cy = (key[0] + 0.5) * CELL, (key[1] + 0.5) * CELL
        if all(math.hypot(cx - z[0], cy - z[1]) > 12000.0 for z in zones):
            zones.append((cx, cy, key, zc))
        if len(zones) >= n_zones:
            break
    views = []
    for zi, (cx, cy, key, zc) in enumerate(zones, 1):
        pool = [d for k in ((key[0] + dx, key[1] + dy) for dx in (-1, 0, 1) for dy in (-1, 0, 1)) if k in cells
                for d in cells[k]['shore']]
        zx = sum(d[2] for d in pool) / len(pool)
        zy = sum(d[3] for d in pool) / len(pool)
        unreal.log('ACR_CAPTURE_ZONE z%d cell=%s shore=%d tree=%d rock=%d reed=%d stone=%d centre=(%.0f,%.0f)' % (
            zi, key, len(zc['shore']), len(zc['tree']), len(zc['rock']), len(zc['reed']), len(zc['stone']), zx, zy))
        P = 'z%d_' % zi
        # Le plus gros arbre (la plus grande zone sombre) a moins de 60 m du centre : celui dont le pied se voit.
        near_zone = [d for d in all_trees if math.hypot(d[2] - zx, d[3] - zy) < 6000.0 and nearest(shore_rows, d[2], d[3], 4500.0)]
        tree = max(near_zone, key=lambda d: d[5]) if near_zone else None
        if tree:
            w = nearest(shore_rows, tree[2], tree[3], 4500.0)
            ax, ay = away(w) if w else (1.0, 0.0)
            # Camera du cote de l'eau : l'arbre est entre l'oeil et la terre, le sol visible entre les deux.
            views.append(cam(P + 'arbre', tree[2] - ax * 480.0, tree[3] - ay * 480.0, 150.0, tree[2], tree[3], 40.0, known_z=tree[4]))
        # Une rive HORS roseaux (a plus de 9 m d'un lit de roseaux) : la camera ne se retrouve pas dans les tiges.
        clear = [d for d in pool if d[8] and d[0] in ('Mud', 'WetBand', 'StoneWet') and not nearest(reed_rows, d[2], d[3], 900.0)]
        near_shore = nearest(clear or [d for d in pool if d[8]], zx, zy)
        if near_shore:
            ax, ay = away(near_shore)
            views.append(cam(P + 'rive', near_shore[2] + ax * 320.0, near_shore[3] + ay * 320.0, 110.0,
                             near_shore[2] - ax * 150.0, near_shore[3] - ay * 150.0, 0.0))
            views.append(cam(P + 'humain_20m', near_shore[2] + ax * 2000.0, near_shore[3] + ay * 2000.0, 170.0,
                             near_shore[2], near_shore[3], 20.0))
            views.append(cam(P + 'paysage_80m', near_shore[2] + ax * 8000.0, near_shore[3] + ay * 8000.0, 3500.0,
                             near_shore[2], near_shore[3], 0.0))
        stone = nearest([d for d in pool if d[0] == 'StoneWet' and d[8]], zx, zy)
        if stone:
            ax, ay = away(stone)
            views.append(cam(P + 'pierre', stone[2] + ax * 300.0, stone[3] + ay * 300.0, 110.0,
                             stone[2] - ax * 130.0, stone[3] - ay * 130.0, 0.0))
        rock = nearest([d for d in decals if d[0] == 'RockDirt' and math.hypot(d[2] - zx, d[3] - zy) < 9000.0], zx, zy)
        if rock:
            views.append(cam(P + 'rocher', rock[2] + 280.0, rock[3] - 120.0, 110.0, rock[2], rock[3], 30.0))
    views = [v for v in views if v]
    with open(os.path.join(OUT, 'cameras.json'), 'w') as f:
        json.dump({'zones': [[z[2][0], z[2][1], z[0], z[1]] for z in zones],
                   'views': [[n, [e.x, e.y, e.z], [t.x, t.y, t.z]] for n, e, t in views]}, f, indent=1)
    unreal.log('ACR_CAPTURE_VIEWS %d %s' % (len(views), ','.join(v[0] for v in views)))
    if not views:
        raise RuntimeError('aucune camera')
except Exception as exc:  # noqa: BLE001
    finish('ACR_CAPTURE_FAIL %s' % exc, True)
    raise

look = unreal.MathLibrary.find_look_at_rotation
selected_views = os.environ.get('ANASTASIS_ACR_VIEWS', '').split(',')
if selected_views != ['']:
    views = [v for v in views if v[0] in selected_views]
queue = [(states[0], v) for v in views]
state_i = 0
phase, mark, shot, first = 'boot', time.monotonic(), None, True
focused_for = None
frames, timings, metrics = [], [], {}


def tick(dt):
    global phase, mark, shot, first, state_i, queue, frames, timings, focused_for
    try:
        el = time.monotonic() - mark
        try:
            les.editor_invalidate_viewports()
        except Exception:
            pass
        if el > 150:
            finish('ACR_CAPTURE_TIMEOUT phase=%s' % phase, True)
            return
        if phase == 'boot':
            if el > 6:
                phase, mark = 'aim', time.monotonic()
        elif phase == 'aim':
            if not queue:
                state_i += 1
                if state_i >= len(states):
                    with open(os.path.join(OUT, 'metrics.json'), 'w') as f:
                        json.dump(metrics, f, indent=1)
                    finish('ACR_CAPTURE_COMPLETE views=%d states=%d' % (len(views), len(states)))
                    return
                for c in STATE_CMDS[states[state_i]]:
                    cmd(c)
                cmd('anastasis.Contact.Rebuild')
                queue = [(states[state_i], v) for v in views]
                first, mark, frames, timings = True, time.monotonic(), [], []
                return
            state, (name, eye, tgt) = queue[0]
            ues.set_level_viewport_camera_info(eye, look(eye, tgt))
            if focused_for != (state, name):
                # Les decalques n'existent qu'autour du point de vue : on le pose sur la camera, puis on attend.
                cmd('anastasis.Contact.Focus %.0f %.0f' % (eye.x, eye.y))
                focused_for = (state, name)
                mark = time.monotonic()
                return
            if el > 2:
                frames.append(dt)
                t = actor.call_method('GetFrameTimingsMs')
                timings.append((t.x, t.y, t.z))
            # Premiere vue d'un etat : decalques et HISM se construisent encore.
            if el > (16 if first else 7):
                f = sorted(frames) or [0.0]
                m = {'frame_ms_p50': 1000.0 * f[len(f) // 2],
                     'frame_ms_p95': 1000.0 * f[int(len(f) * 0.95) - 1 if len(f) > 1 else 0], 'frames': len(frames)}
                if timings:
                    for axis, key in ((0, 'game_ms_p50'), (1, 'render_ms_p50'), (2, 'gpu_ms_p50')):
                        v = sorted(x[axis] for x in timings)
                        m[key] = v[len(v) // 2]
                metrics['%s_%s' % (name, state)] = m
                if PROFILE:
                    unreal.log('ACR_PROFILE_BEGIN %s_%s' % (name, state))
                    cmd('ProfileGPU')
                    unreal.log('ACR_PROFILE_END %s_%s' % (name, state))
                shot = os.path.join(OUT, '%s_%s.png' % (name, state)).replace('\\', '/')
                if os.path.exists(shot):
                    os.remove(shot)
                cmd('HighResShot 1600x900 filename="%s"' % shot)
                phase, mark = 'wait', time.monotonic()
        elif phase == 'wait':
            if not os.path.isfile(shot):
                if el > 45:
                    finish('ACR_SHOT_MISSING %s' % shot, True)
                return
            if el > 1:
                m = metrics['%s_%s' % (queue[0][1][0], queue[0][0])]
                unreal.log('ACR_SHOT_OK %s frame_ms_p50=%.1f p95=%.1f gpu=%.1f' % (
                    os.path.basename(shot), m['frame_ms_p50'], m['frame_ms_p95'], m.get('gpu_ms_p50', -1)))
                queue.pop(0)
                first, frames, timings = False, [], []
                phase, mark = 'aim', time.monotonic()
    except Exception as exc:  # noqa: BLE001
        finish('ACR_CAPTURE_FAIL tick %s' % exc, True)


handle = unreal.register_slate_post_tick_callback(tick)
