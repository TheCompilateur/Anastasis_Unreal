"""GROUND_COVER_001 -- la strate herbacee, a hauteur d'homme et de haut, active puis coupee.

Cameras calculees UNE fois (herbe active) sur la vallee ecrite de Human_Geography_V2, en
coordonnees de tuiles de la carte ; l'etat "off" (anastasis.Dressing.GroundCover 0) est
capture aux memes cameras, dans la meme session : l'A/B ne mesure que l'herbe. Les traces
visent le sol : l'herbe n'a pas de collision, la camera ne s'y pose donc jamais.

Pour chaque vue, la duree de frame est mesuree pendant l'attente (p50 / p95), avec les temps
game thread / render thread / GPU de stat unit (GetFrameTimingsMs) : la duree de frame de
l'editeur mesure surtout la charge des AUTRES processus de la machine (v4 : la meme vue
sans ombres sortait plus lente qu'avec) ; le GPU mesure la scene.

La vue hors_vallee est choisie parmi les touffes REELLEMENT posees (HISM GroundCover_*) loin
des ellipses de la vallee ecrite : elle prouve que la carte entiere est couverte.

ANASTASIS_GROUND_OUT     dossier de sortie (obligatoire)
ANASTASIS_GROUND_STATES  etats captures, dans l'ordre, le premier doit poser l'herbe
                         (defaut "on,off") : on | off | noshadow | notint | on2
                         | on_notex | bare | bare_notex   (GROUND_TEXTURE_001)
                         | eco | noeco   (MICRO_ECOLOGY_001 : herbe laissee, seule la micro-ecologie change)

Etats *_notex : le sol est rendu par une instance DYNAMIQUE de MI_AnastasisGround dont le
fondu des textures photo est ferme (TexFadeStart 0, TexFadeEnd 1). Le materiau rend alors
exactement l'ancien sol, aux memes cameras. Rien n'est ecrit dans l'asset : une MID est
transitoire, et l'editeur n'a aucun paquet sale a proposer de sauver en quittant.
"""
import os, time, math, json, unreal

OUT = os.environ.get('ANASTASIS_GROUND_OUT')
STATE_CMDS = {
    'on': ('anastasis.Dressing.GroundCover 1', 'anastasis.GroundCover.Shadows 1', 'anastasis.GroundCover.SoilTint 1'),
    'off': ('anastasis.Dressing.GroundCover 0', 'anastasis.GroundCover.Shadows 1', 'anastasis.GroundCover.SoilTint 1'),
    'noshadow': ('anastasis.Dressing.GroundCover 1', 'anastasis.GroundCover.Shadows 0', 'anastasis.GroundCover.SoilTint 1'),
    # Memes touffes, sol non teinte : l'A/B du sol sous l'herbe.
    'notint': ('anastasis.Dressing.GroundCover 1', 'anastasis.GroundCover.Shadows 1', 'anastasis.GroundCover.SoilTint 0'),
    # Repetition de "on" en fin de serie : l'ecart on / on2 mesure la derive de la machine.
    'on2': ('anastasis.Dressing.GroundCover 1', 'anastasis.GroundCover.Shadows 1', 'anastasis.GroundCover.SoilTint 1'),
    'on_notex': ('anastasis.Dressing.GroundCover 1', 'anastasis.GroundCover.Shadows 1', 'anastasis.GroundCover.SoilTint 1'),
    'bare': ('anastasis.Dressing.GroundCover 0', 'anastasis.GroundCover.Shadows 1', 'anastasis.GroundCover.SoilTint 1'),
    'bare_notex': ('anastasis.Dressing.GroundCover 0', 'anastasis.GroundCover.Shadows 1', 'anastasis.GroundCover.SoilTint 1'),
    # MICRO_ECOLOGY_001. L'herbe, sa teinte et les rives vivantes restent. Seule cette couche bouge.
    'eco': ('anastasis.Dressing.GroundCover 1', 'anastasis.GroundCover.Shadows 1', 'anastasis.GroundCover.SoilTint 1',
            'anastasis.Dressing.MicroEcology 1', 'anastasis.MicroEcology.Soil 1'),
    'noeco': ('anastasis.Dressing.GroundCover 1', 'anastasis.GroundCover.Shadows 1', 'anastasis.GroundCover.SoilTint 1',
              'anastasis.Dressing.MicroEcology 0', 'anastasis.MicroEcology.Soil 0'),
}
for _state in ('soil_before', 'soil_after', 'soil_control'):
    STATE_CMDS[_state] = STATE_CMDS['on']
NO_TEXTURE = ('on_notex', 'bare_notex')
GROUND_MI = '/Game/Anastasis/Materials/MI_AnastasisGround'
CONTACT = os.environ.get('ANASTASIS_SOIL_CONTACT') == '1'
SOIL_PARAMETER = os.environ.get('ANASTASIS_SOIL_PARAMETER', 'SoilHistory')
SOIL_BEFORE = float(os.environ.get('ANASTASIS_SOIL_BEFORE', '0'))
SOIL_AFTER = float(os.environ.get('ANASTASIS_SOIL_AFTER', '1'))
states = [x.strip() for x in os.environ.get('ANASTASIS_GROUND_STATES', 'on,off').split(',') if x.strip()]
LEVEL = '/Game/Anastasis/Maps/Lvl_AnastasisSlice'
SEED = 12345
V = unreal.Vector

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
handle = None
contact_cvar_before = None


def finish(msg, error=False):
    (unreal.log_error if error else unreal.log)(msg)
    if handle is not None:
        unreal.unregister_slate_post_tick_callback(handle)
    if contact_cvar_before is not None:
        unreal.SystemLibrary.execute_console_command(None, 'anastasis.Dressing.SoilContact %g' % contact_cvar_before)
        for comp in globals().get('contact_components', []):
            comp.set_visibility(bool(contact_cvar_before), True)
            comp.set_hidden_in_game(not bool(contact_cvar_before), True)
    if CONTACT and not error and msg.startswith('GROUND_CAPTURE_COMPLETE'):
        unreal.log('SOIL_CONTACT_CAPTURE COMPLETE')
    unreal.SystemLibrary.quit_editor()


try:
    if not OUT:
        raise RuntimeError('ANASTASIS_GROUND_OUT manquant')
    if not states or any(x not in STATE_CMDS for x in states) or states[0] == 'off':
        raise RuntimeError('ANASTASIS_GROUND_STATES invalide : %r' % states)
    os.makedirs(OUT, exist_ok=True)
    # Reuse startup map: avoid a redundant reload of live Python/editor objects.
    world = ues.get_editor_world()
    if world is None or world.get_name() != 'Lvl_AnastasisSlice':
        les.load_level(LEVEL)
        world = ues.get_editor_world()

    def cmd(c):
        unreal.SystemLibrary.execute_console_command(world, c)

    for c in ('ShowFlag.Sprites 0', 'ShowFlag.Grid 0', 'viewmode lit') + STATE_CMDS[states[0]]:
        cmd(c)
    cls = unreal.load_class(None, '/Script/Anastasis_UnrealV2.AnastasisWorldEmbodiment')
    found = unreal.GameplayStatics.get_all_actors_of_class(world, cls)
    actor = found[0] if len(found) > 0 else eas.spawn_actor_from_class(cls, V(0, 0, 0), unreal.Rotator(0, 0, 0))
    if CONTACT:
        contact_cvar_before = unreal.SystemLibrary.get_console_variable_float_value('anastasis.Dressing.SoilContact')
        cmd('anastasis.Dressing.SoilContact 1')
    actor.call_method('EmbodyCanonical', args=(SEED,))

    contact_components = [c for c in actor.get_components_by_class(unreal.HierarchicalInstancedStaticMeshComponent)
                          if c.get_name().startswith('SoilContact_') and c.get_instance_count()]
    if CONTACT and not contact_components:
        raise RuntimeError('SoilContact pilot missing')
    soil_mids = []
    def texture_state(state):
        if CONTACT:
            visible = state == 'soil_after'
            for comp in contact_components:
                comp.set_visibility(visible, True)
                comp.set_hidden_in_game(not visible, True)
            unreal.log('SOIL_CONTACT_STATE %s visible=%s instances=%d' %
                       (state, visible, sum(c.get_instance_count() for c in contact_components)))
            return
        # Material-only A/B: retain the same geometry and MIDs across all soil states.
        if state.startswith('soil_') and soil_mids:
            value = SOIL_AFTER if state == 'soil_after' else SOIL_BEFORE
            for mid in soil_mids:
                mid.set_scalar_parameter_value(SOIL_PARAMETER, value)
                get = getattr(mid, 'k2_get_scalar_parameter_value', None) or mid.get_scalar_parameter_value
                if abs(get(SOIL_PARAMETER) - value) > 1e-4:
                    raise RuntimeError(SOIL_PARAMETER + ' readback failed')
            unreal.log('SOIL_STATE state=%s sections=%d geometry_unchanged=1' % (state, len(soil_mids)))
            return
        # Apres CHAQUE incarnation : EmbodyCanonical repose le materiau de l'asset.
        if state not in NO_TEXTURE and not state.startswith('soil_'):
            return
        # Une MID par section, creee par le composant lui-meme (UFUNCTION exposee a Python).
        n = 0
        for comp in actor.get_components_by_class(unreal.ProceduralMeshComponent):
            for i in range(comp.get_num_materials()):
                m = comp.get_material(i)
                if m is None or not m.get_path_name().startswith(GROUND_MI):
                    continue
                mid = comp.create_dynamic_material_instance(i, m)
                if state.startswith('soil_'):
                    value = SOIL_AFTER if state == 'soil_after' else SOIL_BEFORE
                    mid.set_scalar_parameter_value(SOIL_PARAMETER, value)
                    soil_mids.append(mid)
                    get = getattr(mid, 'k2_get_scalar_parameter_value', None) or mid.get_scalar_parameter_value
                    if abs(get(SOIL_PARAMETER) - value) > 1e-4:
                        raise RuntimeError(SOIL_PARAMETER + ' parameter readback failed')
                    n += 1
                    continue
                mid.set_scalar_parameter_value('TexFadeStart', 0.0)
                mid.set_scalar_parameter_value('TexFadeEnd', 1.0)
                # Un materiau sans le parametre l'ignore en silence et rendrait un faux A/B.
                get = getattr(mid, 'k2_get_scalar_parameter_value', None) or mid.get_scalar_parameter_value
                if abs(get('TexFadeEnd') - 1.0) > 1e-4:
                    raise RuntimeError('MI_AnastasisGround sans TexFadeEnd : rien a couper')
                n += 1
        if n == 0:
            raise RuntimeError('aucune section ne porte MI_AnastasisGround')
        unreal.log('GROUND_CAPTURE_NOTEX state=%s sections=%d' % (state, n))

    unreal.log('SOIL_PARAMETER ' + SOIL_PARAMETER)
    texture_state(states[0])
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
        # Vue d'ensemble a ~300 m : l'herbe est coupee a 108 m, ce qu'on voit d'ici c'est le
        # SOL -- la mosaique prairie / laiches / lande de la vue aerienne d'EZ5.
        ('aerien', (30.0, 34.0), 30000, (52.0, 58.0), 0),
    ]
    plan.extend([
        ('prairie_ground', (44.0,52.0),170,(44.45,52.35),0),
        ('rive_ground', (49.5,55.2),170,(49.75,55.6),0),
    ])
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
    # Hors vallee : une touffe lointaine posee loin des ellipses ecrites, oeil a 1,7 m au-dessus,
    # regard vers le centre de la carte. Rien trouve = le masque ne couvre toujours que la vallee.
    def valley_distance(tx, ty):
        return min(((tx - 48) / 18) ** 2 + ((ty - 58) / 13) ** 2, ((tx - 62) / 15) ** 2 + ((ty - 47) / 12) ** 2,
                   ((tx - 33) / 13) ** 2 + ((ty - 24) / 11) ** 2)

    def find_instance(prefix, accept):
        # Premiere touffe acceptee parmi les HISM dont le nom commence par prefix (tuiles comprises).
        for comp in actor.get_components_by_class(unreal.HierarchicalInstancedStaticMeshComponent):
            if not comp.get_name().startswith(prefix):
                continue
            n = comp.get_instance_count()
            for i in range(0, n, max(1, n // 400)):
                got = comp.get_instance_transform(i, True)
                xf = got[1] if isinstance(got, tuple) else got
                tx, ty = xf.translation.x / T, xf.translation.y / T
                if 8 < tx < 88 and 8 < ty < 88 and accept(tx, ty):
                    return (tx, ty)
        return None

    def toward_center(name, at_tile, lift):
        dx, dy = 48.0 - at_tile[0], 48.0 - at_tile[1]
        d = math.hypot(dx, dy) or 1.0
        plan.append((name, at_tile, lift, (at_tile[0] + dx / d * 3.0, at_tile[1] + dy / d * 3.0), 120))

    outside = find_instance('GroundCover_MeadowTall_Far', lambda tx, ty: valley_distance(tx, ty) > 3.0)
    if outside:
        toward_center('hors_vallee_eye', outside, 170)
    unreal.log('GROUND_CAPTURE_OUTSIDE %s' % (('tile=%.1f,%.1f' % outside) if outside else 'NONE'))
    # Lande (H6) : une touffe d'eboulis regardee depuis 20 m, oeil cote bord de carte, regard vers
    # le centre (v1 : oeil POSE sur une callune en sous-bois, la lande sortait du cadre).
    # La tuile de lande la plus fournie : une vue de lande la ou il y en a, pas la premiere venue.
    heath, best = None, 0
    for comp in actor.get_components_by_class(unreal.HierarchicalInstancedStaticMeshComponent):
        name = comp.get_name()
        if not (name.startswith('GroundCover_HeathTussock_Near') or name.startswith('GroundCover_Heather_Near')):
            continue
        n = comp.get_instance_count()
        if n > best:
            got = comp.get_instance_transform(n // 2, True)
            xf = got[1] if isinstance(got, tuple) else got
            tx, ty = xf.translation.x / T, xf.translation.y / T
            if 8 < tx < 88 and 8 < ty < 88:
                heath, best = (tx, ty), n
    if heath:
        dx, dy = 48.0 - heath[0], 48.0 - heath[1]
        d = math.hypot(dx, dy) or 1.0
        plan.append(('lande_eye', (heath[0] - dx / d, heath[1] - dy / d), 170, (heath[0] + dx / d * 0.5, heath[1] + dy / d * 0.5), 60))
    unreal.log('GROUND_CAPTURE_LANDE %s' % (('tile=%.1f,%.1f' % heath) if heath else 'NONE'))
    # Sous-bois (H5) : debout dans la tuile de fougeres la plus fournie, regard vers le centre.
    fern, best = None, 0
    for comp in actor.get_components_by_class(unreal.HierarchicalInstancedStaticMeshComponent):
        if not comp.get_name().startswith('GroundCover_Fern_Near'):
            continue
        n = comp.get_instance_count()
        if n > best:
            got = comp.get_instance_transform(n // 2, True)
            xf = got[1] if isinstance(got, tuple) else got
            tx, ty = xf.translation.x / T, xf.translation.y / T
            if 8 < tx < 88 and 8 < ty < 88 and ((tx*T-96000)**2 + (ty*T-110000)**2 < 28000**2):
                fern, best = (tx, ty), n
    if fern:
        dx, dy = 48.0 - fern[0], 48.0 - fern[1]
        d = math.hypot(dx, dy) or 1.0
        plan.append(('sousbois_eye', (fern[0] - dx / d * 0.4, fern[1] - dy / d * 0.4), 170, (fern[0] + dx / d * 0.6, fern[1] + dy / d * 0.6), 40))
    unreal.log('GROUND_CAPTURE_SOUSBOIS %s' % (('tile=%.1f,%.1f' % fern) if fern else 'NONE'))

    if any(x.startswith('soil_') for x in states):
        best_slope = None
        for tx in range(36, 61, 2):
            for ty in range(46, 65, 2):
                hit = trace(V(tx*T, ty*T, 200000), V(tx*T, ty*T, -200000))
                if not hit:
                    continue
                # Finite differences on traced ground: avoid binding-dependent HitResult tuple indices.
                x, y, h = tx*T, ty*T, 150.0
                heights = [ground(x+h,y), ground(x-h,y), ground(x,y+h), ground(x,y-h)]
                if any(z is None for z in heights):
                    continue
                dx, dy = (heights[0]-heights[1])/(2*h), (heights[2]-heights[3])/(2*h)
                norm = math.sqrt(1.0 + dx*dx + dy*dy)
                normal = V(-dx/norm, -dy/norm, 1.0/norm)
                steepness = 1.0 - normal.z
                if 0.025 < steepness < 0.40 and (best_slope is None or steepness > best_slope[0]):
                    best_slope = (steepness, tx, ty, normal)
        if best_slope:
            steepness, tx, ty, normal = best_slope
            # Look across slope with nearby ground in the foreground.
            length = math.hypot(normal.x, normal.y) or 1.0
            plan.append(('pente_eye', (tx, ty), 170,
                         (tx-normal.y/length, ty+normal.x/length), 50))
            plan.append(('pente_face', (tx+normal.x/length*2, ty+normal.y/length*2), 170, (tx,ty), 100))
            unreal.log('SOIL_SLOPE slope_1_minus_nz=%f tile=%d,%d' % (steepness, tx, ty))
        keep = {'prairie_eye', 'riviere_eye', 'lisiere_eye', 'sousbois_eye', 'pente_eye', 'oblique'}
        keep.update(x.strip() for x in os.environ.get('ANASTASIS_SOIL_VIEWS', '').split(',') if x.strip())
        plan = [v for v in plan if v[0] in keep]

    if CONTACT:
        def positions(kind):
            out = []
            for c in contact_components:
                if not c.get_name().startswith('SoilContact_%d_' % kind):
                    continue
                for i in range(c.get_instance_count()):
                    got = c.get_instance_transform(i, True)
                    xf = got[1] if isinstance(got, tuple) else got
                    out.append(xf.translation)
            return out
        coarse, fine = positions(0), positions(2)
        if not coarse or not fine:
            raise RuntimeError('Both source and deposit needed for contact pilot')
        cx, cy = sum(p.x for p in coarse)/len(coarse), sum(p.y for p in coarse)/len(coarse)
        fx, fy = sum(p.x for p in fine)/len(fine), sum(p.y for p in fine)/len(fine)
        ux, uy = cx-fx, cy-fy
        length = math.hypot(ux,uy)
        ux,uy = ux/length,uy/length
        sx,sy = -uy,ux
        plan = []
        # Three successive eye positions across 6 m; fixed target, then lateral and context views.
        for i, side in enumerate((-300,0,300)):
            ex,ey = cx-ux*1700+sx*side, cy-uy*1700+sy*side
            plan.append(('contact_walk%02d'%i,(ex/T,ey/T),170,(cx/T,cy/T),80))
        plan.append(('contact_side',((cx+sx*1700-ux*500)/T,(cy+sy*1700-uy*500)/T),170,(cx/T,cy/T),60))
        plan.append(('contact_context',((cx-ux*3400)/T,(cy-uy*3400)/T),750,(cx/T,cy/T),150))
        with open(os.path.join(OUT,'contact-site.json'),'w') as f:
            json.dump({'coarse_center':[cx,cy],'fine_center':[fx,fy],
                       'counts':{c.get_name():c.get_instance_count() for c in contact_components},
                       'ground_samples':[[p.x,p.y,ground(p.x,p.y)] for p in coarse+fine]},f,indent=1)

    only = {x.strip() for x in os.environ.get('ANASTASIS_SOIL_VIEWS', '').split(',') if x.strip()}
    if only:
        plan = [v for v in plan if v[0] in only]
    if only - {v[0] for v in plan}:
        raise RuntimeError('Requested soil cameras missing: ' + str(only - {v[0] for v in plan}))
    if not plan:
        raise RuntimeError('No requested soil camera available')
    views = []
    for name, eye_t, lift, tgt_t, tlift in plan:
        eye, tgt = at(eye_t[0], eye_t[1], lift), at(tgt_t[0], tgt_t[1], tlift)
        if eye is None or tgt is None:
            unreal.log_warning('GROUND_CAPTURE_SKIP %s sol absent' % name)
            continue
        views.append((name, eye, tgt))
    if SOIL_PARAMETER == 'SoilMatrixStructure':
        pilot = next((v for v in views if v[0] == 'prairie_ground'), None)
        if pilot is None:
            raise RuntimeError('SoilMatrixStructure requires prairie_ground')
        target = pilot[2]
        for mid in soil_mids:
            mid.set_vector_parameter_value('SoilMatrixCenter', unreal.LinearColor(target.x,target.y,target.z,0))
            mid.set_scalar_parameter_value('SoilMatrixRadius',1600.0)
        unreal.log('SOIL_MATRIX_PILOT center=(%.1f,%.1f,%.1f) radius=1600' % (target.x,target.y,target.z))
    with open(os.path.join(OUT, 'cameras.json'), 'w') as f:
        json.dump({'tile_uu': T, 'views': [[n, [e.x, e.y, e.z], [t.x, t.y, t.z]] for n, e, t in views]}, f, indent=1)
    unreal.log('GROUND_CAPTURE_VIEWS %d tile_uu=%.1f' % (len(views), T))
except Exception as exc:  # noqa: BLE001
    finish('GROUND_CAPTURE_FAIL %s' % exc, True)
    raise

look = unreal.MathLibrary.find_look_at_rotation
queue = [(states[0], v) for v in views]
state_i = 0
phase, mark, shot, first = 'boot', time.monotonic(), None, True
frames = []
timings = []
metrics = {}


def tick(dt):
    global phase, mark, shot, first, state_i, queue, frames, timings
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
                for c in STATE_CMDS[states[state_i]]:
                    cmd(c)
                if not states[state_i].startswith('soil_'):
                    actor.call_method('EmbodyCanonical', args=(SEED,))
                texture_state(states[state_i])
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
                metrics['%s_%s' % (name, state)] = {
                    'frame_ms_p50': 1000.0 * f[len(f) // 2], 'frame_ms_p95': 1000.0 * f[int(len(f) * 0.95) - 1 if len(f) > 1 else 0],
                    'frames': len(frames)}
                if timings:
                    for axis, key in ((0, 'game_ms_p50'), (1, 'render_ms_p50'), (2, 'gpu_ms_p50')):
                        v = sorted(x[axis] for x in timings)
                        metrics['%s_%s' % (name, state)][key] = v[len(v) // 2]
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
                unreal.log('GROUND_SHOT_OK %s frame_ms_p50=%.1f p95=%.1f game=%.1f render=%.1f gpu=%.1f' % (
                    os.path.basename(shot), m['frame_ms_p50'], m['frame_ms_p95'], m.get('game_ms_p50', -1),
                    m.get('render_ms_p50', -1), m.get('gpu_ms_p50', -1)))
                queue.pop(0)
                first, frames, timings = False, [], []
                phase, mark = 'aim', time.monotonic()
    except Exception as exc:  # noqa: BLE001
        finish('GROUND_CAPTURE_FAIL tick %s' % exc, True)


handle = unreal.register_slate_post_tick_callback(tick)
