"""WORLD_DRESSING_01 -- chaque lieu compose, de loin et a hauteur d'homme, lieux actives / coupes.

Les cameras sont calculees UNE fois, lieux actives, depuis le rapport de l'incarnation
(GetPlaceReport) ; l'etat "off" (anastasis.Dressing.Places 0) est capture aux memes cameras,
dans la meme session : l'A/B ne mesure que les lieux. Vue au sol : 1,7 m au-dessus du sol,
premiere direction degagee (trace de visibilite) en partant de celle du bassin habitable.

ANASTASIS_PLACES_OUT   dossier de sortie
ANASTASIS_PLACES_AB    1 = captures on + off (defaut), 0 = on seulement
ANASTASIS_PLACES_ALL   1 = aussi les lieux secondaires (bois_, affleurement_, vestiges_)
"""
import os, time, math, json, unreal

OUT = os.environ.get('ANASTASIS_PLACES_OUT', os.path.join(unreal.Paths.project_saved_dir(), 'PlacesEvidence'))
os.makedirs(OUT, exist_ok=True)
AB = os.environ.get('ANASTASIS_PLACES_AB', '1') == '1'
ALL = os.environ.get('ANASTASIS_PLACES_ALL', '0') == '1'
LEVEL = '/Game/Anastasis/Maps/Lvl_AnastasisSlice'
SEED = 12345

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
les.load_level(LEVEL)
world = ues.get_editor_world()


def cmd(c):
    unreal.SystemLibrary.execute_console_command(world, c)


for c in ('ShowFlag.Sprites 0', 'ShowFlag.Grid 0', 'viewmode lit', 'anastasis.Dressing.Places 1'):
    cmd(c)

cls = unreal.load_class(None, '/Script/Anastasis_UnrealV2.AnastasisWorldEmbodiment')
found = unreal.GameplayStatics.get_all_actors_of_class(world, cls)
actor = found[0] if len(found) > 0 else eas.spawn_actor_from_class(cls, unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0))
actor.call_method('EmbodyCanonical', args=(SEED,))
report = [r.split('|') for r in actor.call_method('GetPlaceReport')]
basin = actor.call_method('GetTerrainForgeBasin')
unreal.log('PLACES_CAPTURE_REPORT %d places' % len(report))
with open(os.path.join(OUT, 'places.json'), 'w') as f:
    json.dump(report, f, indent=1)

# Emprise rendue : la largeur du monde se lit sur le sol trace, pas sur une constante.
V = unreal.Vector


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


def visible(eye, tgt):
    t = trace(eye, tgt)
    return t is None or (t[4] - tgt).length() < 900.0


views = [('top', V(W / 2, W / 2 - 1, W * 0.78), V(W / 2, W / 2, 0)),
         ('ov_sw', V(-W * 0.16, -W * 0.16, W * 0.42), V(W * 0.55, W * 0.55, 0)),
         ('ov_ne', V(W * 1.16, W * 1.16, W * 0.42), V(W * 0.45, W * 0.45, 0))]
for pid, x, y, z, r in report:
    if not ALL and pid.split('_')[0] in ('bois', 'affleurement', 'vestiges'):
        continue
    x, y, z, r = float(x), float(y), float(z), max(float(r), 2 * T)
    dx, dy = basin.x - x, basin.y - y
    d = math.hypot(dx, dy) or 1.0
    far = r * 2.4 + 5 * T
    ex, ey = x + dx / d * far, y + dy / d * far
    views.append((pid + '_far', V(ex, ey, max(ground(ex, ey) or z, z) + far * 0.45), V(x, y, z)))
    target = V(x, y, z + 400)
    base = math.degrees(math.atan2(dy, dx))
    kept = []
    for dist in (0.8 * T + 0.2 * r, 1.4 * T + 0.35 * r, 2.4 * T + 0.6 * r):
        for k in range(24):
            a = base + ((k + 1) // 2) * 15 * (1 if k % 2 else -1)
            gx, gy = x + math.cos(math.radians(a)) * dist, y + math.sin(math.radians(a)) * dist
            gz = ground(gx, gy) if 0 < gx < W and 0 < gy < W else None
            # 300 uu : au-dessus de la nappe de mer, la camera n'est pas sous l'eau.
            if gz is None or gz < 300:
                continue
            eye = V(gx, gy, gz + 170)
            if visible(eye, target) and all(abs(a - b) > 70 for b in kept):
                kept.append(a)
                views.append((pid + ('_eye' if len(kept) == 1 else '_side'), eye, target))
            if len(kept) >= 2:
                break
        if kept:
            break

states = ['on', 'off'] if AB else ['on']
look = unreal.MathLibrary.find_look_at_rotation
queue = [(s, v) for s in states[:1] for v in views]
state_i = 0
phase, mark, shot, handle, first = 'boot', time.monotonic(), None, None, True


def finish(msg, error=False):
    (unreal.log_error if error else unreal.log)(msg)
    unreal.unregister_slate_post_tick_callback(handle)
    unreal.SystemLibrary.quit_editor()


def tick(_dt):
    global phase, mark, shot, first, state_i, queue
    el = time.monotonic() - mark
    try:
        les.editor_invalidate_viewports()
    except Exception:
        pass
    if el > 120:
        finish('PLACES_CAPTURE_TIMEOUT phase=%s' % phase, True)
        return
    if phase == 'boot':
        if el > 3:
            phase, mark = 'aim', time.monotonic()
    elif phase == 'aim':
        if not queue:
            state_i += 1
            if state_i >= len(states):
                finish('PLACES_CAPTURE_COMPLETE views=%d states=%d' % (len(views), len(states)))
                return
            cmd('anastasis.Dressing.Places %d' % (1 if states[state_i] == 'on' else 0))
            actor.call_method('EmbodyCanonical', args=(SEED,))
            queue = [(states[state_i], v) for v in views]
            first, mark = True, time.monotonic()
            return
        state, (name, eye, tgt) = queue[0]
        ues.set_level_viewport_camera_info(eye, look(eye, tgt))
        if el > (10 if first else 4):
            shot = os.path.join(OUT, '%s_%s.png' % (name, state)).replace('\\', '/')
            if os.path.exists(shot):
                os.remove(shot)
            cmd('HighResShot 1600x900 filename="%s"' % shot)
            phase, mark = 'wait', time.monotonic()
    elif phase == 'wait':
        if not os.path.isfile(shot):
            if el > 30:
                finish('PLACES_SHOT_MISSING %s' % shot, True)
            return
        if el > 1:
            unreal.log('PLACES_SHOT_OK %s' % os.path.basename(shot))
            queue.pop(0)
            first = False
            phase, mark = 'aim', time.monotonic()


handle = unreal.register_slate_post_tick_callback(tick)
