"""RIVERBANK_LIFE_001 -- l'eau et ses rives a hauteur d'homme, par etats, aux memes cameras.

Jusqu'ici l'eau n'a ete jugee que de 100 m et plus (HYDRO_NETWORK_001, WATER_LOOK_001). Ce
script la regarde a 1,7 m, la ou un joueur la verrait : ligne d'eau contre la berge, fond
vu par transparence, bord du ruban, et la rive elle-meme.

Les cameras sont calculees UNE fois, a partir du reseau REELLEMENT rendu : le premier etat
incarne le monde avec anastasis.Drainage.Dump (network_<etat>.json, un par etat), et les sites sont tires du JSON du reseau
(points [x, y, z, largeur, profondeur, vitesse, talus, pente, ordre]) :
  calme_plaine  riviere d'ordre max, vitesse faible, loin des bords
  eau_vive      le point le plus rapide hors des bords
  confluence    l'embouchure d'un affluent dans sa riviere
  ruisseau      une riviere d'ordre 1, a mi-cours
  rive_lac      le plus grand lac interieur, depuis sa rive
  mare          la premiere zone humide (mares de plaine d'inondation)
  berge_haute   la confluence vue de 40 m : les bandes de rive en plan
Oeil a 1,7 m au-dessus du sol trace, sur la berge (demi-largeur + 8 m), regard en biais vers
l'aval et vers l'eau. Les etats suivants sont captures aux MEMES cameras.

Pour chaque vue : duree de frame p50 / p95 et GPU p50 (stat unit via GetFrameTimingsMs). La
duree de frame de l'editeur mesure surtout la charge des AUTRES processus ; le GPU mesure la
scene. L'ecart water / flat a la meme camera est le cout de Single Layer Water.

ANASTASIS_RIVERBANK_PROFILE "1" : un ProfileGPU par vue (cout par passe, lu dans le log)
ANASTASIS_RIVERBANK_OUT     dossier de sortie (obligatoire)
ANASTASIS_RIVERBANK_STATES  etats, dans l'ordre (defaut "water,flat") :
    water    eau WATER_LOOK_001 (anastasis.Terrain.WaterLook 1), rives actives
    flat     eau d'avant (WaterLook 0), rives actives
    banks    WaterLook 1, rives vivantes actives (anastasis.Dressing.Riverbank 1)
    nobanks  WaterLook 1, rives vivantes coupees (anastasis.Dressing.Riverbank 0)
    water2   repetition de "water" : l'ecart water / water2 mesure la derive de la machine
"""
import os, time, math, json, unreal

OUT = os.environ.get('ANASTASIS_RIVERBANK_OUT')
STATE_CMDS = {
    'water': ('anastasis.Terrain.WaterLook 1', 'anastasis.Dressing.Riverbank 1'),
    'flat': ('anastasis.Terrain.WaterLook 0', 'anastasis.Dressing.Riverbank 1'),
    'banks': ('anastasis.Terrain.WaterLook 1', 'anastasis.Dressing.Riverbank 1'),
    'nobanks': ('anastasis.Terrain.WaterLook 1', 'anastasis.Dressing.Riverbank 0'),
    'water2': ('anastasis.Terrain.WaterLook 1', 'anastasis.Dressing.Riverbank 1'),
}
COMMON = ('ShowFlag.Sprites 0', 'ShowFlag.Grid 0', 'viewmode lit', 'ShowFlag.Fog 1', 'ShowFlag.VolumetricFog 1')
states = [x.strip() for x in os.environ.get('ANASTASIS_RIVERBANK_STATES', 'water,flat').split(',') if x.strip()]
PROFILE = os.environ.get('ANASTASIS_RIVERBANK_PROFILE', '0') == '1'
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
        raise RuntimeError('ANASTASIS_RIVERBANK_OUT manquant')
    if not states or any(x not in STATE_CMDS for x in states):
        raise RuntimeError('ANASTASIS_RIVERBANK_STATES invalide : %r' % states)
    os.makedirs(OUT, exist_ok=True)
    if ues.get_editor_world().get_path_name().split('.')[0] != LEVEL:
        les.load_level(LEVEL)
    world = ues.get_editor_world()

    def cmd(c):
        unreal.SystemLibrary.execute_console_command(world, c)

    def net_file(state):
        return os.path.join(OUT, 'network_%s.json' % state).replace('\\', '/')

    net_path = net_file(states[0])
    if os.path.exists(net_path):
        os.remove(net_path)
    for c in COMMON + STATE_CMDS[states[0]] + ('anastasis.Drainage.Dump %s' % net_path,):
        cmd(c)
    cls = unreal.load_class(None, '/Script/Anastasis_UnrealV2.AnastasisWorldEmbodiment')
    found = unreal.GameplayStatics.get_all_actors_of_class(world, cls)
    actor = found[0] if len(found) > 0 else eas.spawn_actor_from_class(cls, V(0, 0, 0), unreal.Rotator(0, 0, 0))
    actor.call_method('EmbodyCanonical', args=(SEED,))
    if not os.path.isfile(net_path):
        raise RuntimeError('reseau non exporte : %s' % net_path)
    with open(net_path, encoding='utf-8') as f:
        net = json.load(f)

    def trace(a, b):
        hit = unreal.SystemLibrary.line_trace_single(world, a, b, unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, True, [],
                                                     unreal.DrawDebugTrace.NONE, True)
        return hit.to_tuple() if hit else None

    def ground(x, y):
        t = trace(V(x, y, 200000), V(x, y, -200000))
        return t[4].z if t else None

    W = 0.0
    while ground(W + 1000, 1000) is not None and W < 1.0e6:
        W += 1000
    if W <= 0:
        raise RuntimeError('sol introuvable : emprise nulle')
    margin = 0.12 * W

    def inside(x, y):
        return margin < x < W - margin and margin < y < W - margin

    rivers = net.get('rivers', [])

    def frame(points, k):
        # Tangente et normale horizontales au point k d'une polyligne.
        a, b = points[max(k - 1, 0)], points[min(k + 1, len(points) - 1)]
        tx, ty = b[0] - a[0], b[1] - a[1]
        d = math.hypot(tx, ty) or 1.0
        return tx / d, ty / d, -ty / d, tx / d

    def bank_view(name, points, k, side=1.0, back_m=8.0, lift=170.0, ahead_m=28.0):
        p = points[k]
        tx, ty, nx, ny = frame(points, k)
        off = 0.5 * p[3] + back_m * 100.0
        ex, ey = p[0] + nx * off * side, p[1] + ny * off * side
        gz = ground(ex, ey)
        if gz is None:
            return None
        # Regard vers l'aval et vers l'eau : la ligne d'eau proche, la berge d'en face, le courant.
        cx, cy = p[0] + tx * ahead_m * 100.0 - nx * side * 0.15 * p[3], p[1] + ty * ahead_m * 100.0 - ny * side * 0.15 * p[3]
        return (name, V(ex, ey, gz + lift), V(cx, cy, p[2]))

    picks = []
    # calme_plaine : ordre maximal, vitesse la plus faible, au milieu de la carte.
    best = None
    for r in rivers:
        pts = r['points']
        for k in range(2, len(pts) - 2):
            p = pts[k]
            if not inside(p[0], p[1]):
                continue
            key = (p[8], -p[5])
            if best is None or key > best[0]:
                best = (key, pts, k)
    if best:
        picks.append(bank_view('calme_plaine', best[1], best[2]))
    # eau_vive : le point le plus rapide.
    best = None
    for r in rivers:
        pts = r['points']
        for k in range(2, len(pts) - 2):
            p = pts[k]
            if inside(p[0], p[1]) and (best is None or p[5] > best[0]):
                best = (p[5], pts, k)
    if best:
        picks.append(bank_view('eau_vive', best[1], best[2], back_m=6.0))
    # confluence : l'embouchure d'un affluent (parent >= 0), vue depuis la berge de l'affluent.
    conf = None
    for r in sorted(rivers, key=lambda r: -r['length_m']):
        pts = r['points']
        if r['parent'] >= 0 and len(pts) > 6 and inside(pts[-1][0], pts[-1][1]):
            conf = (pts, len(pts) - 4)
            break
    if conf:
        picks.append(bank_view('confluence', conf[0], conf[1], ahead_m=35.0))
        v = bank_view('berge_haute', conf[0], conf[1], back_m=25.0, lift=4000.0, ahead_m=45.0)
        if v:
            picks.append(v)
    # ruisseau : ordre 1, a mi-cours.
    for r in sorted(rivers, key=lambda r: -r['length_m']):
        pts = r['points']
        k = len(pts) // 2
        if r['order'] == 1 and len(pts) > 6 and inside(pts[k][0], pts[k][1]):
            picks.append(bank_view('ruisseau', pts, k, back_m=5.0, ahead_m=18.0))
            break
    # rive_lac : le plus grand lac interieur ; on marche du centre vers l'exterieur jusqu'a la rive.
    lakes = [l for l in net.get('lakes', []) if not l.get('border') and inside(l['at'][0], l['at'][1])]
    if lakes:
        lake = max(lakes, key=lambda l: l['cells'])
        cx, cy, lz = lake['at'][0], lake['at'][1], lake['z']
        for ang in range(0, 360, 30):
            dx, dy = math.cos(math.radians(ang)), math.sin(math.radians(ang))
            shore = None
            for s in range(10, 600, 5):
                gz = ground(cx + dx * s * 100.0, cy + dy * s * 100.0)
                if gz is not None and gz > lz + 30.0:
                    shore = s
                    break
            if shore is None:
                continue
            ex, ey = cx + dx * (shore + 8) * 100.0, cy + dy * (shore + 8) * 100.0
            gz = ground(ex, ey)
            if gz is None or gz - lz > 600.0:
                continue
            picks.append(('rive_lac', V(ex, ey, gz + 170.0), V(cx + dx * (shore - 40) * 100.0, cy + dy * (shore - 40) * 100.0, lz)))
            break
    # mare : premiere zone humide, vue depuis 25 m.
    for wl in net.get('wetlands', []):
        x, y, z = wl['at']
        if not inside(x, y):
            continue
        for ang in range(0, 360, 45):
            ex, ey = x + math.cos(math.radians(ang)) * 2500.0, y + math.sin(math.radians(ang)) * 2500.0
            gz = ground(ex, ey)
            if gz is not None:
                picks.append(('mare', V(ex, ey, gz + 170.0), V(x, y, z)))
                break
        break

    views = [p for p in picks if p]
    with open(os.path.join(OUT, 'cameras.json'), 'w') as f:
        json.dump({'extent_uu': W, 'views': [[n, [e.x, e.y, e.z], [t.x, t.y, t.z]] for n, e, t in views]}, f, indent=1)
    unreal.log('RIVERBANK_CAPTURE_VIEWS %d %s' % (len(views), ','.join(v[0] for v in views)))
    if not views:
        raise RuntimeError('aucune camera')
except Exception as exc:  # noqa: BLE001
    finish('RIVERBANK_CAPTURE_FAIL %s' % exc, True)
    raise

look = unreal.MathLibrary.find_look_at_rotation
selected_views = os.environ.get('ANASTASIS_CAPTURE_VIEWS', '').split(',')
if selected_views != ['']:
    views = [v for v in views if v[0] in selected_views]
queue = [(states[0], v) for v in views]
state_i = 0
phase, mark, shot, first = 'boot', time.monotonic(), None, True
frames, timings, metrics = [], [], {}


def tick(dt):
    global phase, mark, shot, first, state_i, queue, frames, timings
    try:
        el = time.monotonic() - mark
        try:
            les.editor_invalidate_viewports()
        except Exception:
            pass
        if el > 150:
            finish('RIVERBANK_CAPTURE_TIMEOUT phase=%s' % phase, True)
            return
        if phase == 'boot':
            if el > 4:
                phase, mark = 'aim', time.monotonic()
        elif phase == 'aim':
            if not queue:
                state_i += 1
                if state_i >= len(states):
                    with open(os.path.join(OUT, 'metrics.json'), 'w') as f:
                        json.dump(metrics, f, indent=1)
                    finish('RIVERBANK_CAPTURE_COMPLETE views=%d states=%d' % (len(views), len(states)))
                    return
                for c in STATE_CMDS[states[state_i]] + ('anastasis.Drainage.Dump %s' % net_file(states[state_i]),):
                    cmd(c)
                actor.call_method('EmbodyCanonical', args=(SEED,))
                queue = [(states[state_i], v) for v in views]
                first, mark, frames, timings = True, time.monotonic(), [], []
                return
            state, (name, eye, tgt) = queue[0]
            ues.set_level_viewport_camera_info(eye, look(eye, tgt))
            if el > 2:
                frames.append(dt)
                t = actor.call_method('GetFrameTimingsMs')
                timings.append((t.x, t.y, t.z))
            # Premiere vue d'un etat : l'arbre asynchrone des HISM se construit encore.
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
                    # Duree GPU PAR PASSE dans une seule frame : independante de la charge des
                    # autres processus, contrairement a la frame. Lue apres coup dans le log
                    # entre les deux marqueurs (riverbank-capture.ps1 -Profile).
                    unreal.log('RIVERBANK_PROFILE_BEGIN %s_%s' % (name, state))
                    cmd('ProfileGPU')
                    unreal.log('RIVERBANK_PROFILE_END %s_%s' % (name, state))
                shot = os.path.join(OUT, '%s_%s.png' % (name, state)).replace('\\', '/')
                if os.path.exists(shot):
                    os.remove(shot)
                cmd('HighResShot 1600x900 filename="%s"' % shot)
                phase, mark = 'wait', time.monotonic()
        elif phase == 'wait':
            if not os.path.isfile(shot):
                if el > 45:
                    finish('RIVERBANK_SHOT_MISSING %s' % shot, True)
                return
            if el > 1:
                m = metrics['%s_%s' % (queue[0][1][0], queue[0][0])]
                unreal.log('RIVERBANK_SHOT_OK %s frame_ms_p50=%.1f p95=%.1f gpu=%.1f' % (
                    os.path.basename(shot), m['frame_ms_p50'], m['frame_ms_p95'], m.get('gpu_ms_p50', -1)))
                queue.pop(0)
                first, frames, timings = False, [], []
                phase, mark = 'aim', time.monotonic()
    except Exception as exc:  # noqa: BLE001
        finish('RIVERBANK_CAPTURE_FAIL tick %s' % exc, True)


handle = unreal.register_slate_post_tick_callback(tick)
