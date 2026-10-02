"""Ciel -- A/B de l'atmosphere sur Lvl_AnastasisSlice : realisme, horloge du ciel, meteo.

Une session, une carte, une graine, les memes cameras : seules les CVars de l'etat changent
(anastasis.Atmosphere.Realism, anastasis.Sky.Hour / Sky.Day pour epingler l'heure et le jour du
CIEL, anastasis.Sky.Weather...). Apres chaque etat l'atmosphere est reappliquee par
AAnastasisWorldAtmosphere (Apply puis ApplyMist), comme le fait le game mode en PIE ; l'editeur
n'a pas d'horloge de simulation, l'heure s'y epingle donc par CVar. Rien n'est sauve, l'acteur
est transitoire.

Vues (calculees une fois sur le sol trace, avant toute capture) :
  ov_sw           oblique haute, le relief entier et le ciel
  valley_long     1,7 m au-dessus d'un fond de vallee, dans la direction de plus longue vue
  ridge_long      1,7 m au-dessus d'une crete, dans la direction de plus longue vue
  sun_ridge       meme oeil que ridge_long, face a l'azimut du soleil (halo, diffusion avant)
"Plus longue vue" : 24 azimuts, trace a 3 degres sous l'horizontale ; une camera qui regarde
une pente a 10 m ne prouve rien sur la profondeur atmospherique.

ANASTASIS_SKY_OUT     dossier de sortie (PNG + sky.json, avec le GPU p50 de stat unit par image)
ANASTASIS_SKY_STATES  etats, separes par '|'. Un etat est "etiquette=cvar;cvar" ; un
                          nombre seul N vaut "rN=anastasis.Atmosphere.Realism N".
                          Defaut "1|0".
"""
import os, time, math, json, unreal

OUT = os.environ.get('ANASTASIS_SKY_OUT', os.path.join(unreal.Paths.project_saved_dir(), 'SkyEvidence'))
os.makedirs(OUT, exist_ok=True)
def parse_state(s):
    if '=' not in s:
        return ('r' + s, ['anastasis.Atmosphere.Realism ' + s])
    label, cvars = s.split('=', 1)
    return (label.strip(), [c.strip() for c in cvars.split(';') if c.strip()])


# ANASTASIS_SKY_VIEWS : sous-ensemble de vues (ov_sw,valley_long,ridge_long,sun_ridge), vide = toutes.
# Un balayage fin d'heures n'a pas besoin des quatre vues a chaque etat.
ONLY = [v.strip() for v in os.environ.get('ANASTASIS_SKY_VIEWS', '').split(',') if v.strip()]
STATES = [parse_state(s.strip()) for s in os.environ.get('ANASTASIS_SKY_STATES', '1|0').split('|') if s.strip()]
LEVEL = '/Game/Anastasis/Maps/Lvl_AnastasisSlice'
SEED = 12345

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
if ues.get_editor_world().get_path_name().split(".")[0] != LEVEL:
    les.load_level(LEVEL)
world = ues.get_editor_world()


def cmd(c):
    unreal.SystemLibrary.execute_console_command(world, c)
    unreal.log('SKY_CMD ' + c)


for c in ('ShowFlag.Sprites 0', 'ShowFlag.Grid 0', 'viewmode lit'):
    cmd(c)

emb_cls = unreal.load_class(None, '/Script/Anastasis_UnrealV2.AnastasisWorldEmbodiment')
atm_cls = unreal.load_class(None, '/Script/Anastasis_UnrealV2.AnastasisWorldAtmosphere')
found = unreal.GameplayStatics.get_all_actors_of_class(world, emb_cls)
emb = found[0] if len(found) > 0 else eas.spawn_actor_from_class(emb_cls, unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0))
emb.call_method('EmbodyCanonical', args=(SEED,))
atm = eas.spawn_actor_from_class(atm_cls, unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0), transient=True)

V = unreal.Vector


def trace(a, b):
    hit = unreal.SystemLibrary.line_trace_single(world, a, b, unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, True, [],
                                                 unreal.DrawDebugTrace.NONE, True)
    return hit.to_tuple() if hit else None


def ground(x, y):
    """Le SOL, pas la cime : on reprend le trace sous chaque impact qui n'est pas le terrain
    procedural (arbres et rochers sont des HISM qui bloquent aussi). Premiere capture : les
    yeux de crete etaient poses sur la canopee."""
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


# Emprise et relief, lus sur le sol trace : grille de 24 x 24.
W = 0.0
while ground(W + 1000, 1000) is not None and W < 1.0e6:
    W += 1000
samples = []
for i in range(1, 24):
    for j in range(1, 24):
        x, y = W * i / 24.0, W * j / 24.0
        z = ground(x, y)
        # 300 uu : au-dessus de la nappe de mer (meme seuil que places-capture.py).
        if z is not None and z > 300:
            samples.append((z, x, y))
samples.sort()
low, high = samples[0], samples[-1]
unreal.log('SKY_RELIEF W=%.0f samples=%d low_z=%.0f high_z=%.0f' % (W, len(samples), low[0], high[0]))


def eye(s):
    return V(s[1], s[2], s[0] + 170)


def sightline(e, az):
    """Distance libre depuis l'oeil, a 3 degres sous l'horizontale ; W si rien n'arrete le regard."""
    L = W * 0.9
    d = V(math.cos(math.radians(az)) * L, math.sin(math.radians(az)) * L, -math.tan(math.radians(3)) * L)
    t = trace(e, V(e.x + d.x, e.y + d.y, e.z + d.z))
    return ((t[4] - e).length() if t else L), V(e.x + d.x, e.y + d.y, e.z + d.z)


def open_sky(e):
    """Pas de canopee au-dessus de l'oeil : premiere capture, crete et contre-jour etaient dans le feuillage."""
    return trace(e, V(e.x, e.y, e.z + 4000)) is None


def longest_view(pool, need_sky=True):
    best = None
    for s in pool:
        e = eye(s)
        if need_sky and not open_sky(e):
            continue
        for k in range(24):
            dist, tgt = sightline(e, k * 15.0)
            if best is None or dist > best[0]:
                best = (dist, e, tgt, k * 15.0)
    return best


n = len(samples)
valley = longest_view(samples[:max(1, n // 5)]) or longest_view(samples[:max(1, n // 5)], False)
ridge = longest_view(samples[-max(1, n // 10):]) or longest_view(samples[-max(1, n // 10):], False)
unreal.log('SKY_VIEWS valley_sight=%.0f ridge_sight=%.0f' % (valley[0], ridge[0]))

views = [('ov_sw', V(-W * 0.16, -W * 0.16, W * 0.42), V(W * 0.55, W * 0.55, 0)),
         ('valley_long', valley[1], valley[2]),
         ('ridge_long', ridge[1], ridge[2])]

look = unreal.MathLibrary.find_look_at_rotation
queue, state_i, phase, mark, shot, handle, first = [], -1, 'boot', time.monotonic(), None, None, True
gpu = []  # GPU ms de stat unit (GetFrameTimingsMs), echantillonne pendant l'attente de la vue
report = {'level': LEVEL, 'seed': SEED, 'world_uu': W, 'low': low, 'high': high,
          'valley_sight_uu': valley[0], 'ridge_sight_uu': ridge[0], 'states': {}, 'gpu_ms_p50': {}}


def apply_state(state):
    label, cvars = state
    for c in cvars:
        cmd(c)
    atm.call_method('Apply')
    pockets = atm.call_method('ApplyMist')
    report['states'][label] = {'cvars': cvars, 'mist_pockets': pockets}
    unreal.log('SKY_STATE %s pockets=%s cvars=%s' % (label, pockets, ';'.join(cvars)))


def sun_view():
    # Calcule apres Apply : le soleil adopte porte alors la rotation du profil. La lumiere
    # pointe le long des rayons ; le soleil est donc dans la direction opposee.
    sun = None
    for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.DirectionalLight):
        if not a.actor_has_tag('AnastasisMoon'):
            sun = a
            break
    fwd = sun.get_actor_forward_vector() if sun else V(0.5, 0.5, -0.5)
    h = math.hypot(fwd.x, fwd.y) or 1.0
    e = ridge[1]
    # 15 degres au-dessus de l'horizon : le soleil (38 degres) reste dans le champ, en haut.
    return ('sun_ridge', e, V(e.x - fwd.x / h * 10000, e.y - fwd.y / h * 10000, e.z + math.tan(math.radians(15)) * 10000))


def finish(msg, error=False):
    (unreal.log_error if error else unreal.log)(msg)
    with open(os.path.join(OUT, 'sky.json'), 'w') as f:
        json.dump(report, f, indent=1, default=str)
    unreal.unregister_slate_post_tick_callback(handle)
    unreal.SystemLibrary.quit_editor()


def tick(_dt):
    global phase, mark, shot, first, state_i, queue, gpu
    el = time.monotonic() - mark
    try:
        les.editor_invalidate_viewports()
    except Exception:
        pass
    if el > 180:
        finish('SKY_CAPTURE_TIMEOUT phase=%s' % phase, True)
        return
    if phase == 'boot':
        if el > 3:
            phase, mark = 'aim', time.monotonic()
    elif phase == 'aim':
        if not queue:
            state_i += 1
            if state_i >= len(STATES):
                finish('SKY_CAPTURE_COMPLETE views=%d states=%d' % (len(views) + 1, len(STATES)))
                return
            apply_state(STATES[state_i])
            queue = [(STATES[state_i], v) for v in views + [sun_view()] if not ONLY or v[0] in ONLY]
            # Nuages volumetriques, brouillard volumetrique et capture temps reel du ciel ont
            # besoin de frames pour converger : la premiere vue d'un etat attend plus longtemps.
            first, mark = True, time.monotonic()
            return
        state, (name, e, tgt) = queue[0]
        ues.set_level_viewport_camera_info(e, look(e, tgt))
        # Apres 2 s : la vue est posee, l'image ne porte plus le changement d'etat ou de camera.
        if el > 2:
            gpu.append(emb.call_method('GetFrameTimingsMs').z)
        if el > (14 if first else 5):
            g = sorted(gpu)
            report['gpu_ms_p50']['%s_%s' % (name, state[0])] = round(g[len(g) // 2], 3) if g else -1
            gpu = []
            shot = os.path.join(OUT, '%s_%s.png' % (name, state[0])).replace('\\', '/')
            if os.path.exists(shot):
                os.remove(shot)
            cmd('HighResShot 1600x900 filename="%s"' % shot)
            phase, mark = 'wait', time.monotonic()
    elif phase == 'wait':
        if not os.path.isfile(shot):
            if el > 30:
                finish('SKY_SHOT_MISSING %s' % shot, True)
            return
        if el > 1:
            unreal.log('SKY_SHOT_OK %s gpu_ms_p50=%.2f' % (os.path.basename(shot),
                       report['gpu_ms_p50'].get(os.path.splitext(os.path.basename(shot))[0], -1)))
            queue.pop(0)
            first = False
            phase, mark = 'aim', time.monotonic()


handle = unreal.register_slate_post_tick_callback(tick)
