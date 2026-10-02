"""EYE_PLANE_001 -- un seul plan net a 1,7 m.

Meme carte, meme graine, meme cadrage. La seule variable est anastasis.Depth.EyePlane.
L'oeil est a 170 cm. On cherche une vue ou quelque chose coupe le cadre a moins de 3 m
(tronc, roseau, rocher) et ou le regard au centre porte a plus de 40 m, legerement sous
l'horizon, pour que le milieu soit le sol ou l'eau et le fond le relief.

ANASTASIS_EYE_OUT     dossier de sortie
ANASTASIS_EYE_STATES  etats "etiquette=cvar;cvar" separes par '|'.
                      Defaut : off (plan coupe) | on (plan net).
"""
import os, time, math, json, unreal

OUT = os.environ.get('ANASTASIS_EYE_OUT', os.path.join(unreal.Paths.project_saved_dir(), 'EyePlaneEvidence'))
os.makedirs(OUT, exist_ok=True)

def parse_state(s):
    label, cvars = s.split('=', 1)
    return (label.strip(), [c.strip() for c in cvars.split(';') if c.strip()])

STATES = [parse_state(s.strip()) for s in os.environ.get(
    'ANASTASIS_EYE_STATES',
    'off=anastasis.Atmosphere.Realism 1;anastasis.Sky.Day 1;anastasis.Sky.Hour 11;anastasis.Sky.Weather 0;anastasis.Depth.EyePlane 0'
    '|on=anastasis.Atmosphere.Realism 1;anastasis.Sky.Day 1;anastasis.Sky.Hour 11;anastasis.Sky.Weather 0;anastasis.Depth.EyePlane 1'
).split('|') if s.strip()]
LEVEL = '/Game/Anastasis/Maps/Lvl_AnastasisSlice'
SEED = 12345
EYE = 170.0

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
les.load_level(LEVEL)
world = ues.get_editor_world()
V = unreal.Vector


def cmd(c):
    unreal.SystemLibrary.execute_console_command(world, c)
    unreal.log('EYE_CMD ' + c)


for c in ('ShowFlag.Sprites 0', 'ShowFlag.Grid 0', 'viewmode lit'):
    cmd(c)

emb_cls = unreal.load_class(None, '/Script/Anastasis_UnrealV2.AnastasisWorldEmbodiment')
atm_cls = unreal.load_class(None, '/Script/Anastasis_UnrealV2.AnastasisWorldAtmosphere')
found = unreal.GameplayStatics.get_all_actors_of_class(world, emb_cls)
emb = found[0] if len(found) > 0 else eas.spawn_actor_from_class(emb_cls, V(0, 0, 0), unreal.Rotator(0, 0, 0))
emb.call_method('EmbodyCanonical', args=(SEED,))
atm = eas.spawn_actor_from_class(atm_cls, V(0, 0, 0), unreal.Rotator(0, 0, 0), transient=True)


def trace(a, b):
    hit = unreal.SystemLibrary.line_trace_single(
        world, a, b, unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, True, [], unreal.DrawDebugTrace.NONE, True)
    return hit.to_tuple() if hit else None


def ground(x, y):
    top = 200000.0
    for _ in range(12):
        t = trace(V(x, y, top), V(x, y, -200000))
        if not t:
            return None
        comp = t[10]
        if comp and 'Procedural' in comp.get_class().get_name():
            return t[4].z
        top = t[4].z - 5.0
    return None


W = 0.0
while ground(W + 1000, 1000) is not None and W < 1.0e6:
    W += 1000
samples = []
for i in range(1, 24):
    for j in range(1, 24):
        x, y = W * i / 24.0, W * j / 24.0
        z = ground(x, y)
        if z is not None and z > 300:
            samples.append((z, x, y))
samples.sort()
unreal.log('EYE_RELIEF W=%.0f samples=%d' % (W, len(samples)))


def sight(e, az):
    L = W * 0.9
    d = V(math.cos(math.radians(az)) * L, math.sin(math.radians(az)) * L, -math.tan(math.radians(3)) * L)
    end = V(e.x + d.x, e.y + d.y, e.z + d.z)
    t = trace(e, end)
    dist = (t[4] - e).length() if t else L
    return dist, end


def near_cut(e, az):
    """Un obstacle a 0,8-2,6 m, 14 degres hors de l'axe : il coupe le cadre, il n'est pas le sujet."""
    best = None
    for side in (-14.0, 14.0):
        a = math.radians(az + side)
        end = V(e.x + math.cos(a) * 280.0, e.y + math.sin(a) * 280.0, e.z - 40.0)
        t = trace(e, end)
        if not t:
            continue
        d = (t[4] - e).length()
        if 80.0 < d < 260.0 and (best is None or d < best):
            best = d
    return best


pool = samples[:max(1, len(samples) // 4)]
best = None
for s in pool:
    e = V(s[1], s[2], s[0] + EYE)
    for k in range(24):
        az = k * 15.0
        dist, end = sight(e, az)
        if dist < 4000.0:
            continue
        cut = near_cut(e, az)
        rank = (1 if cut else 0, dist)
        if best is None or rank > best[0]:
            best = (rank, e, end, az, dist, cut)
if best is None and samples:
    s = samples[0]
    e = V(s[1], s[2], s[0] + EYE)
    dist, end = sight(e, 0.0)
    best = ((0, dist), e, end, 0.0, dist, None)

eye, target, az, dist, cut = best[1], best[2], best[3], best[4], best[5]
unreal.log('EYE_VIEW az=%.0f sight_cm=%.0f cut_cm=%s eye_z=%.0f' % (
    az, dist, ('%.0f' % cut) if cut else 'none', eye.z))
report = {'level': LEVEL, 'seed': SEED, 'eye_uu': EYE, 'azimuth': az, 'sight_cm': dist,
          'cut_cm': cut, 'states': {}}
look = unreal.MathLibrary.find_look_at_rotation
queue, state_i, phase, mark, shot, handle, first = [], -1, 'boot', time.monotonic(), None, None, True


def apply_state(state):
    label, cvars = state
    for c in cvars:
        cmd(c)
    applied = atm.call_method('Apply')
    pockets = atm.call_method('ApplyMist')
    report['states'][label] = {'cvars': cvars, 'applied': applied, 'mist_pockets': pockets}
    unreal.log('EYE_STATE %s applied=%s pockets=%s' % (label, applied, pockets))


def finish(msg, error=False):
    (unreal.log_error if error else unreal.log)(msg)
    with open(os.path.join(OUT, 'eye-plane.json'), 'w') as f:
        json.dump(report, f, indent=1, default=str)
    unreal.unregister_slate_post_tick_callback(handle)
    unreal.SystemLibrary.quit_editor()


def tick(_dt):
    global phase, mark, shot, first, state_i, queue
    el = time.monotonic() - mark
    try:
        les.editor_invalidate_viewports()
    except Exception:
        pass
    if el > 180:
        finish('EYE_CAPTURE_TIMEOUT phase=%s' % phase, True)
        return
    if phase == 'boot':
        if el > 3:
            phase, mark = 'aim', time.monotonic()
    elif phase == 'aim':
        if not queue:
            state_i += 1
            if state_i >= len(STATES):
                finish('EYE_CAPTURE_COMPLETE states=%d' % len(STATES))
                return
            apply_state(STATES[state_i])
            queue = [STATES[state_i]]
            first, mark = True, time.monotonic()
            return
        state = queue[0]
        ues.set_level_viewport_camera_info(eye, look(eye, target))
        if el > (14 if first else 6):
            shot = os.path.join(OUT, 'eye_%s.png' % state[0]).replace('\\', '/')
            if os.path.exists(shot):
                os.remove(shot)
            cmd('HighResShot 1600x900 filename="%s"' % shot)
            phase, mark = 'wait', time.monotonic()
    elif phase == 'wait':
        if not os.path.isfile(shot):
            if el > 40:
                finish('EYE_SHOT_MISSING %s' % shot, True)
            return
        if el > 1.5:
            unreal.log('EYE_SHOT_OK %s bytes=%d' % (os.path.basename(shot), os.path.getsize(shot)))
            queue.pop(0)
            first = False
            phase, mark = 'aim', time.monotonic()


handle = unreal.register_slate_post_tick_callback(tick)
