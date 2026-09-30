"""GROUND_COVER_001 -- la strate herbacee, a hauteur d'homme et de haut, active puis coupee.

Cameras calculees UNE fois (herbe active) sur la vallee ecrite de Human_Geography_V2, en
coordonnees de tuiles de la carte ; l'etat "off" (anastasis.Dressing.GroundCover 0) est
capture aux memes cameras, dans la meme session : l'A/B ne mesure que l'herbe. Les traces
visent le sol : l'herbe n'a pas de collision, la camera ne s'y pose donc jamais.

Pour chaque vue, la duree de frame est mesuree pendant l'attente (p50 / p95) : la preuve de
cout est prise la ou l'image est prise.

ANASTASIS_GROUND_OUT   dossier de sortie (obligatoire)
"""
import os, time, math, json, unreal

OUT = os.environ.get('ANASTASIS_GROUND_OUT')
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
        raise RuntimeError('ANASTASIS_GROUND_OUT manquant')
    os.makedirs(OUT, exist_ok=True)
    les.load_level(LEVEL)
    world = ues.get_editor_world()

    def cmd(c):
        unreal.SystemLibrary.execute_console_command(world, c)

    for c in ('ShowFlag.Sprites 0', 'ShowFlag.Grid 0', 'viewmode lit', 'anastasis.Dressing.GroundCover 1'):
        cmd(c)
    cls = unreal.load_class(None, '/Script/Anastasis_UnrealV2.AnastasisWorldEmbodiment')
    found = unreal.GameplayStatics.get_all_actors_of_class(world, cls)
    actor = found[0] if len(found) > 0 else eas.spawn_actor_from_class(cls, V(0, 0, 0), unreal.Rotator(0, 0, 0))
    actor.call_method('EmbodyCanonical', args=(SEED,))
    report = [r.split('|') for r in actor.call_method('GetPlaceReport')]

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
    T = W / 96.0
    if T <= 0:
        raise RuntimeError('sol introuvable : emprise nulle')

    def at(tx, ty, lift):
        x, y = tx * T, ty * T
        z = ground(x, y)
        return None if z is None else V(x, y, z + lift)

    # (nom, oeil en tuiles, hauteur d'oeil, cible en tuiles, hauteur de cible)
    # Vallee A : ellipses (48,58) et (62,47) ; riviere principale (30,76)->(63,65) ;
    # vallee B (33,24) ; passage A-B (34,26)->(56,49). Cf. AnastasisHumanGeography.cpp.
    plan = [
        ('prairie_eye', (44.0, 52.0), 170, (55.0, 61.0), 120),
        ('prairie_low', (47.0, 54.0), 60, (52.0, 58.0), 40),
        ('riviere_eye', (49.5, 55.2), 170, (50.5, 58.4), 0),
        ('lisiere_eye', (37.0, 57.0), 170, (28.0, 58.0), 600),
        ('vallee_b_eye', (37.0, 27.0), 170, (28.0, 21.0), 300),
        ('oblique', (40.0, 44.0), 3500, (50.0, 58.0), 0),
    ]
    for pid, x, y, z, r in report:
        if pid == 'hameau':
            # Oeil DANS l'emprise pietinee, tourne vers le fond de vallee : le sol tasse au
            # premier plan, la prairie qui reprend derriere. (v2 : oeil a 1,6 rayon, hors vallee.)
            cx, cy, cz, rr = float(x), float(y), float(z), float(r)
            vx, vy = 52.0 * T - cx, 56.0 * T - cy
            vd = math.hypot(vx, vy) or 1.0
            ex, ey = cx - vx / vd * rr * 0.4, cy - vy / vd * rr * 0.4
            gz = ground(ex, ey)
            if gz is not None:
                plan.append(('hameau_eye', (ex / T, ey / T), 170, ((cx + vx / vd * rr * 1.5) / T, (cy + vy / vd * rr * 1.5) / T), 120))
            continue
    views = []
    for name, eye_t, lift, tgt_t, tlift in plan:
        eye, tgt = at(eye_t[0], eye_t[1], lift), at(tgt_t[0], tgt_t[1], tlift)
        if eye is None or tgt is None:
            unreal.log_warning('GROUND_CAPTURE_SKIP %s sol absent' % name)
            continue
        views.append((name, eye, tgt))
    with open(os.path.join(OUT, 'cameras.json'), 'w') as f:
        json.dump({'tile_uu': T, 'views': [[n, [e.x, e.y, e.z], [t.x, t.y, t.z]] for n, e, t in views]}, f, indent=1)
    unreal.log('GROUND_CAPTURE_VIEWS %d tile_uu=%.1f' % (len(views), T))
except Exception as exc:  # noqa: BLE001
    finish('GROUND_CAPTURE_FAIL %s' % exc, True)
    raise

states = ['on', 'off']
look = unreal.MathLibrary.find_look_at_rotation
queue = [(states[0], v) for v in views]
state_i = 0
phase, mark, shot, first = 'boot', time.monotonic(), None, True
frames = []
metrics = {}


def tick(dt):
    global phase, mark, shot, first, state_i, queue, frames
    try:
        el = time.monotonic() - mark
        try:
            les.editor_invalidate_viewports()
        except Exception:
            pass
        if el > 150:
            finish('GROUND_CAPTURE_TIMEOUT phase=%s' % phase, True)
            return
        if phase == 'boot':
            if el > 4:
                phase, mark = 'aim', time.monotonic()
        elif phase == 'aim':
            if not queue:
                state_i += 1
                if state_i >= len(states):
                    with open(os.path.join(OUT, 'ground-cover.json'), 'w') as f:
                        json.dump(metrics, f, indent=1)
                    finish('GROUND_CAPTURE_COMPLETE views=%d states=%d' % (len(views), len(states)))
                    return
                cmd('anastasis.Dressing.GroundCover %d' % (1 if states[state_i] == 'on' else 0))
                actor.call_method('EmbodyCanonical', args=(SEED,))
                queue = [(states[state_i], v) for v in views]
                first, mark, frames = True, time.monotonic(), []
                return
            state, (name, eye, tgt) = queue[0]
            ues.set_level_viewport_camera_info(eye, look(eye, tgt))
            if el > 2:
                frames.append(dt)
            # Premiere vue d'un etat : l'arbre asynchrone des HISM se construit encore.
            if el > (16 if first else 7):
                f = sorted(frames) or [0.0]
                metrics['%s_%s' % (name, state)] = {
                    'frame_ms_p50': 1000.0 * f[len(f) // 2], 'frame_ms_p95': 1000.0 * f[int(len(f) * 0.95) - 1 if len(f) > 1 else 0],
                    'frames': len(frames)}
                shot = os.path.join(OUT, '%s_%s.png' % (name, state)).replace('\\', '/')
                if os.path.exists(shot):
                    os.remove(shot)
                cmd('HighResShot 1600x900 filename="%s"' % shot)
                phase, mark = 'wait', time.monotonic()
        elif phase == 'wait':
            if not os.path.isfile(shot):
                if el > 45:
                    finish('GROUND_SHOT_MISSING %s' % shot, True)
                return
            if el > 1:
                m = metrics['%s_%s' % (queue[0][1][0], queue[0][0])]
                unreal.log('GROUND_SHOT_OK %s frame_ms_p50=%.1f p95=%.1f' % (os.path.basename(shot), m['frame_ms_p50'], m['frame_ms_p95']))
                queue.pop(0)
                first, frames = False, []
                phase, mark = 'aim', time.monotonic()
    except Exception as exc:  # noqa: BLE001
        finish('GROUND_CAPTURE_FAIL tick %s' % exc, True)


handle = unreal.register_slate_post_tick_callback(tick)
