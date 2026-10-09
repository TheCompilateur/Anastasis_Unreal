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

Etats ecotone_reference,ecotone,ecotone_reference2 : couronnes arbres vs obstacles,
poses ecotone_open/edge/inside/oblique proches, prairie temoin.

ANASTASIS_GROUND_VIEWS   optional comma-separated existing view names; missing view fails
ANASTASIS_GROUND_OUT     dossier de sortie (obligatoire)
ANASTASIS_GROUND_STATES  etats captures, dans l'ordre, le premier doit poser l'herbe
                         (defaut "on,off") : on | off | noshadow | notint | on2
                         | on_notex | bare | bare_notex   (GROUND_TEXTURE_001)
                         | natural | reference | reference2 (NaturalHistory A/B, sky pinned at 11)
                         | eco | noeco   (MICRO_ECOLOGY_001 : herbe laissee, seule la micro-ecologie change)
                         | noflowers | flowers | noflowers2   (WILDFLOWERS_001 : meme prairie, fleurs sauvages seules
                           en jeu -- anastasis.Dressing.Wildflowers 0 / 1 ; ciel epingle a 11 h)
                         | nocards | cards | nocards2   (LEAFCARDS_001 : memes arbres, chene vert en lames puis en cartes --
                           anastasis.Dressing.TreeCards 0 / 1 ; ciel epingle a 11 h)

Etats *_notex : le sol est rendu par une instance DYNAMIQUE de MI_AnastasisGround dont le
fondu des textures photo est ferme (TexFadeStart 0, TexFadeEnd 1). Le materiau rend alors
exactement l'ancien sol, aux memes cameras. Rien n'est ecrit dans l'asset : une MID est
transitoire, et l'editeur n'a aucun paquet sale a proposer de sauver en quittant.
"""
import os, time, math, json, hashlib, unreal

OUT = os.environ.get('ANASTASIS_GROUND_OUT')
STATE_CMDS = {
    'horsetail_old': ('anastasis.Dressing.PonticWaterMicro 1', 'anastasis.Dressing.PonticHorsetailCandidate 0'),
    'horsetail_new': ('anastasis.Dressing.PonticWaterMicro 1', 'anastasis.Dressing.PonticHorsetailCandidate 1'),
    'horsetail_old2': ('anastasis.Dressing.PonticWaterMicro 1', 'anastasis.Dressing.PonticHorsetailCandidate 0'),
    'micro_on': ('anastasis.Dressing.PonticWaterMicro 1',),
    'micro_off': ('anastasis.Dressing.PonticWaterMicro 0',),
    'micro_on2': ('anastasis.Dressing.PonticWaterMicro 1',),
    'woodland_reference': ('anastasis.Dressing.NaturalHistory 1', 'anastasis.Dressing.TreeCanopyEcotone 1', 'anastasis.Dressing.WoodlandSequence 0'),
    'woodland': ('anastasis.Dressing.NaturalHistory 1', 'anastasis.Dressing.TreeCanopyEcotone 1', 'anastasis.Dressing.WoodlandSequence 1'),
    'woodland_reference2': ('anastasis.Dressing.NaturalHistory 1', 'anastasis.Dressing.TreeCanopyEcotone 1', 'anastasis.Dressing.WoodlandSequence 0'),
    'riparian_reference': ('anastasis.Dressing.RiparianTransition 0',),
    'riparian': ('anastasis.Dressing.RiparianTransition 1',),
    'riparian_reference2': ('anastasis.Dressing.RiparianTransition 0',),
    'ecotone_reference': ('anastasis.Dressing.NaturalHistory 1', 'anastasis.Dressing.TreeCanopyEcotone 0'),
    'ecotone': ('anastasis.Dressing.NaturalHistory 1', 'anastasis.Dressing.TreeCanopyEcotone 1'),
    'ecotone_reference2': ('anastasis.Dressing.NaturalHistory 1', 'anastasis.Dressing.TreeCanopyEcotone 0'),
    'natural': ('anastasis.Dressing.NaturalHistory 1',),
    'reference': ('anastasis.Dressing.NaturalHistory 0',),
    'reference2': ('anastasis.Dressing.NaturalHistory 0',),
    'on': ('anastasis.Dressing.GroundCover 1', 'anastasis.GroundCover.Shadows 1', 'anastasis.GroundCover.SoilTint 1'),
    'off': ('anastasis.Dressing.GroundCover 0', 'anastasis.GroundCover.Shadows 1', 'anastasis.GroundCover.SoilTint 1'),
    'noshadow': ('anastasis.Dressing.GroundCover 1', 'anastasis.GroundCover.Shadows 0', 'anastasis.GroundCover.SoilTint 1'),
    # Memes touffes, sol non teinte : l'A/B du sol sous l'herbe.
    'notint': ('anastasis.Dressing.GroundCover 1', 'anastasis.GroundCover.Shadows 1', 'anastasis.GroundCover.SoilTint 0'),
    # Repetition de "on" en fin de serie : l'ecart on / on2 mesure la derive de la machine.
    'on2': ('anastasis.Dressing.GroundCover 1', 'anastasis.GroundCover.Shadows 1', 'anastasis.GroundCover.SoilTint 1'),
    # WILDFLOWERS_001 : la prairie et le sol restent, seules les fleurs sauvages changent.
    'noflowers': ('anastasis.Dressing.GroundCover 1', 'anastasis.GroundCover.Shadows 1', 'anastasis.GroundCover.SoilTint 1',
                  'anastasis.Dressing.Wildflowers 0'),
    'flowers': ('anastasis.Dressing.GroundCover 1', 'anastasis.GroundCover.Shadows 1', 'anastasis.GroundCover.SoilTint 1',
                'anastasis.Dressing.Wildflowers 1'),
    'noflowers2': ('anastasis.Dressing.GroundCover 1', 'anastasis.GroundCover.Shadows 1', 'anastasis.GroundCover.SoilTint 1',
                   'anastasis.Dressing.Wildflowers 0'),
    # LEAFCARDS_001 : la foret, l'herbe et le sol restent ; seul le feuillage du chene vert change.
    'nocards': ('anastasis.Dressing.GroundCover 1', 'anastasis.GroundCover.Shadows 1', 'anastasis.GroundCover.SoilTint 1',
                'anastasis.Dressing.TreeCards 0'),
    'cards': ('anastasis.Dressing.GroundCover 1', 'anastasis.GroundCover.Shadows 1', 'anastasis.GroundCover.SoilTint 1',
              'anastasis.Dressing.TreeCards 1'),
    'nocards2': ('anastasis.Dressing.GroundCover 1', 'anastasis.GroundCover.Shadows 1', 'anastasis.GroundCover.SoilTint 1',
                 'anastasis.Dressing.TreeCards 0'),
    'on_notex': ('anastasis.Dressing.GroundCover 1', 'anastasis.GroundCover.Shadows 1', 'anastasis.GroundCover.SoilTint 1'),
    'bare': ('anastasis.Dressing.GroundCover 0', 'anastasis.GroundCover.Shadows 1', 'anastasis.GroundCover.SoilTint 1'),
    'bare_notex': ('anastasis.Dressing.GroundCover 0', 'anastasis.GroundCover.Shadows 1', 'anastasis.GroundCover.SoilTint 1'),
    # PONTIC_GROUND_ASSETS_001 : seule la texture du sol de ruine bascule.
    'ruin_before': ('anastasis.Dressing.GroundCover 1', 'anastasis.GroundCover.Shadows 1', 'anastasis.GroundCover.SoilTint 1'),
    'ruin_after': ('anastasis.Dressing.GroundCover 1', 'anastasis.GroundCover.Shadows 1', 'anastasis.GroundCover.SoilTint 1'),
    'ruin_control': ('anastasis.Dressing.GroundCover 1', 'anastasis.GroundCover.Shadows 1', 'anastasis.GroundCover.SoilTint 1'),
    'path_before': ('anastasis.Dressing.GroundCover 1', 'anastasis.GroundCover.Shadows 1', 'anastasis.GroundCover.SoilTint 1'),
    'path_after': ('anastasis.Dressing.GroundCover 1', 'anastasis.GroundCover.Shadows 1', 'anastasis.GroundCover.SoilTint 1'),
    'path_control': ('anastasis.Dressing.GroundCover 1', 'anastasis.GroundCover.Shadows 1', 'anastasis.GroundCover.SoilTint 1'),
    'gravel_before': ('anastasis.Dressing.GroundCover 1', 'anastasis.GroundCover.Shadows 1', 'anastasis.GroundCover.SoilTint 1'),
    'gravel_after': ('anastasis.Dressing.GroundCover 1', 'anastasis.GroundCover.Shadows 1', 'anastasis.GroundCover.SoilTint 1'),
    'gravel_control': ('anastasis.Dressing.GroundCover 1', 'anastasis.GroundCover.Shadows 1', 'anastasis.GroundCover.SoilTint 1'),
    # MICRO_ECOLOGY_001. L'herbe, sa teinte et les rives vivantes restent. Seule cette couche bouge.
    'eco': ('anastasis.Dressing.GroundCover 1', 'anastasis.GroundCover.Shadows 1', 'anastasis.GroundCover.SoilTint 1',
            'anastasis.Dressing.MicroEcology 1', 'anastasis.MicroEcology.Soil 1'),
    'noeco': ('anastasis.Dressing.GroundCover 1', 'anastasis.GroundCover.Shadows 1', 'anastasis.GroundCover.SoilTint 1',
              'anastasis.Dressing.MicroEcology 0', 'anastasis.MicroEcology.Soil 0'),
}
NO_TEXTURE = ('on_notex', 'bare_notex')
CONTEXT_STATES = ('ruin_before', 'ruin_after', 'ruin_control',
                  'path_before', 'path_after', 'path_control',
                  'gravel_before', 'gravel_after', 'gravel_control')
GROUND_MI = '/Game/Anastasis/Materials/MI_AnastasisGround'
states = [x.strip() for x in os.environ.get('ANASTASIS_GROUND_STATES', 'on,off').split(',') if x.strip()]
LEVEL = '/Game/Anastasis/Maps/Lvl_AnastasisSlice'
SEED = 12345
V = unreal.Vector

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
handle = None
woodland_run = any(s.startswith('woodland') for s in states)
riparian_run = any(s.startswith('riparian') for s in states)
original_riparian = unreal.SystemLibrary.get_console_variable_int_value('anastasis.Dressing.RiparianTransition')
ecotone_run = woodland_run or any(s.startswith('ecotone') for s in states)
natural_run = riparian_run or ecotone_run or any(s in ('natural', 'reference', 'reference2') for s in states)
original_woodland = unreal.SystemLibrary.get_console_variable_int_value('anastasis.Dressing.WoodlandSequence')
flower_run = any(s in ('noflowers', 'flowers', 'noflowers2') for s in states)
cards_run = any(s in ('nocards', 'cards', 'nocards2') for s in states)
horsetail_run = any(s in ('horsetail_old', 'horsetail_new', 'horsetail_old2') for s in states)
micro_run = horsetail_run or any(s in ('micro_on', 'micro_off', 'micro_on2') for s in states)
original_micro = unreal.SystemLibrary.get_console_variable_int_value('anastasis.Dressing.PonticWaterMicro')
original_horsetail = unreal.SystemLibrary.get_console_variable_int_value('anastasis.Dressing.PonticHorsetailCandidate')
original_ecotone = unreal.SystemLibrary.get_console_variable_int_value('anastasis.Dressing.TreeCanopyEcotone')
original_hour = unreal.SystemLibrary.get_console_variable_float_value('anastasis.Sky.Hour')
original_natural = unreal.SystemLibrary.get_console_variable_int_value('anastasis.Dressing.NaturalHistory')
habitat = {}



def finish(msg, error=False):
    if riparian_run:
        unreal.SystemLibrary.execute_console_command(None, 'anastasis.Dressing.RiparianTransition %s' % original_riparian)
    if natural_run or micro_run:
        unreal.SystemLibrary.execute_console_command(None, 'anastasis.Sky.Hour %s' % original_hour)
    if micro_run:
        unreal.SystemLibrary.execute_console_command(None, 'anastasis.Dressing.PonticWaterMicro %s' % original_micro)
        unreal.SystemLibrary.execute_console_command(None, 'anastasis.Dressing.PonticHorsetailCandidate %s' % original_horsetail)
    if natural_run:
        unreal.SystemLibrary.execute_console_command(None, 'anastasis.Dressing.NaturalHistory %s' % original_natural)
    if natural_run:
        unreal.SystemLibrary.execute_console_command(None, 'anastasis.Dressing.WoodlandSequence %s' % original_woodland)
    if ecotone_run:
        unreal.SystemLibrary.execute_console_command(None, 'anastasis.Dressing.TreeCanopyEcotone %s' % original_ecotone)
    (unreal.log_error if error else unreal.log)(msg)
    if handle is not None:
        unreal.unregister_slate_post_tick_callback(handle)
    unreal.SystemLibrary.quit_editor()


try:
    if not OUT:
        raise RuntimeError('ANASTASIS_GROUND_OUT manquant')
    if not states or any(x not in STATE_CMDS for x in states) or states[0] == 'off':
        raise RuntimeError('ANASTASIS_GROUND_STATES invalide : %r' % states)
    os.makedirs(OUT, exist_ok=True)
    les.load_level(LEVEL)
    world = ues.get_editor_world()

    def cmd(c):
        unreal.SystemLibrary.execute_console_command(world, c)

    if natural_run or flower_run or cards_run or micro_run or any(s in CONTEXT_STATES for s in states):
        cmd('anastasis.Sky.Hour 11')
        if not woodland_run:
            cmd('anastasis.Dressing.WoodlandSequence 0')
    for c in ('ShowFlag.Sprites 0', 'ShowFlag.Grid 0', 'viewmode lit') + STATE_CMDS[states[0]]:
        cmd(c)
    cls = unreal.load_class(None, '/Script/Anastasis_UnrealV2.AnastasisWorldEmbodiment')
    found = unreal.GameplayStatics.get_all_actors_of_class(world, cls)
    actor = found[0] if len(found) > 0 else eas.spawn_actor_from_class(cls, V(0, 0, 0), unreal.Rotator(0, 0, 0))
    actor.call_method('EmbodyCanonical', args=(SEED,))

    def texture_state(state):
        # Apres CHAQUE incarnation : EmbodyCanonical repose le materiau de l'asset.
        if state not in NO_TEXTURE and state not in CONTEXT_STATES:
            return
        # Une MID par section, creee par le composant lui-meme (UFUNCTION exposee a Python).
        n = 0
        for comp in actor.get_components_by_class(unreal.ProceduralMeshComponent):
            for i in range(comp.get_num_materials()):
                m = comp.get_material(i)
                if m is None or not m.get_path_name().startswith(GROUND_MI):
                    continue
                mid = comp.create_dynamic_material_instance(i, m)
                if state in NO_TEXTURE:
                    mid.set_scalar_parameter_value('TexFadeStart', 0.0)
                    mid.set_scalar_parameter_value('TexFadeEnd', 1.0)
                else:
                    family = state.split('_', 1)[0]
                    parameter = {'ruin': 'TexContextStrength', 'path': 'TexPathStrength',
                                 'gravel': 'TexGravelStrength'}[family]
                    mid.set_scalar_parameter_value(parameter, 1.0 if state.endswith('_after') else 0.0)
                # Un materiau sans le parametre l'ignore en silence et rendrait un faux A/B.
                get = getattr(mid, 'k2_get_scalar_parameter_value', None) or mid.get_scalar_parameter_value
                parameter = 'TexFadeEnd' if state in NO_TEXTURE else parameter
                expected = 1.0 if state in NO_TEXTURE or state.endswith('_after') else 0.0
                if abs(get(parameter) - expected) > 1e-4:
                    raise RuntimeError('MI_AnastasisGround sans %s : rien a basculer' % parameter)
                n += 1
        if n == 0:
            raise RuntimeError('aucune section ne porte MI_AnastasisGround')
        unreal.log('GROUND_CAPTURE_MATERIAL state=%s sections=%d' % (state, n))

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

    def record_habitat(state):
        if not natural_run:
            return
        actual = unreal.SystemLibrary.get_console_variable_int_value(
            'anastasis.Dressing.WoodlandSequence' if woodland_run else
            'anastasis.Dressing.RiparianTransition' if riparian_run else
            'anastasis.Dressing.TreeCanopyEcotone' if ecotone_run else 'anastasis.Dressing.NaturalHistory')
        if actual != (1 if state in ('natural', 'ecotone', 'woodland', 'riparian') else 0):
            raise RuntimeError('NaturalHistory switch not applied')
        counts = {}
        spatial = []
        non_micro = []
        protected_micro = []
        for comp in actor.get_components_by_class(unreal.HierarchicalInstancedStaticMeshComponent):
            count = comp.get_instance_count()
            if count:
                counts[comp.get_name()] = count
                # UObject names acquire suffixes after a rebuild; they are not spatial identities.
                mesh = comp.get_editor_property('static_mesh')
                if mesh is None:
                    raise RuntimeError('populated HISM without mesh')
                samples = []
                for i in sorted({0, count//2, count-1}):
                    got = comp.get_instance_transform(i, True)
                    xf = got[1] if isinstance(got, tuple) else got
                    samples.append([round(xf.translation.x, 4), round(xf.translation.y, 4), round(xf.translation.z, 4)])
                item = [mesh.get_path_name(), count, samples]
                spatial.append(item)
                if not comp.get_name().startswith('MicroEco_'):
                    non_micro.append(item)
                elif woodland_run and not comp.get_name().startswith(('MicroEco_edge_', 'MicroEco_under_', 'MicroEco_log_', 'MicroEco_stump_', 'MicroEco_roots_')):
                    positions=[]
                    for i in range(count):
                        got=comp.get_instance_transform(i, True)
                        xf=got[1] if isinstance(got, tuple) else got
                        positions.append([xf.translation.x, xf.translation.y, xf.translation.z])
                    protected_micro.append([mesh.get_path_name(), count, sorted(positions)])
        if not any(n.startswith('GroundCover_') for n in counts):
            raise RuntimeError('no ground cover to compare')
        heights = [ground(x*T, y*T) for y in range(4, 93, 4) for x in range(4, 93, 4)]
        if any(z is None for z in heights):
            raise RuntimeError('sampled terrain missing')
        habitat[state] = {'switch': actual, 'instances': counts, 'spatial_inventory': sorted(spatial), 'non_micro_inventory': sorted(non_micro), 'protected_micro': sorted(protected_micro),
                          'sampled_ground_sha256': hashlib.sha256(json.dumps(heights).encode()).hexdigest()}
        unreal.log('NATURAL_HISTORY_SAMPLE state=%s instances=%d ground=%s' % (
            state, sum(counts.values()), habitat[state]['sampled_ground_sha256']))

    record_habitat(states[0])
    micro_inventory = {}
    def record_micro(state):
        if not micro_run:
            return
        items = []
        bank_components = 0
        moss_mesh = None
        moss_count = 0
        for comp in actor.get_components_by_class(unreal.HierarchicalInstancedStaticMeshComponent):
            name = comp.get_name()
            count = comp.get_instance_count()
            mesh = comp.get_editor_property('static_mesh')
            if name.startswith('TrunkContact_Moss'):
                if count:
                    if moss_mesh is not None:
                        raise RuntimeError('multiple populated moss components')
                    moss_mesh = mesh.get_name() if mesh else None
                    moss_count = count
            if name.startswith('MicroEco_bank_Pontic_'):
                bank_components += 1
            if name.startswith('MicroEco_bank_Pontic_') and count:
                if mesh is None:
                    raise RuntimeError('Pontic bank component missing mesh')
                positions = []
                for i in range(count):
                    got = comp.get_instance_transform(i, True)
                    xf = got[1] if isinstance(got, tuple) else got
                    positions.append([round(xf.translation.x, 2), round(xf.translation.y, 2), round(xf.translation.z, 2)])
                items.append([mesh.get_name(), sorted(positions)])
        micro_inventory[state] = {'bank': sorted(items), 'bank_components': bank_components,
                                  'moss_mesh': moss_mesh, 'moss_count': moss_count}
        unreal.log('PONTIC_MICRO_SAMPLE state=%s bank_components=%d moss=%s count=%d' % (
            state, len(items), moss_mesh, moss_count))
    record_micro(states[0])

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
        # Noeud du passage auteur (42,33) : une camera a hauteur humaine regarde
        # la matiere a 3 m, sans confondre le sol avec une carte vue du ciel.
        ('pass_ground', (42.0, 33.0), 170, (42.12, 33.12), 0),
        ('oblique', (40.0, 44.0), 3500, (50.0, 58.0), 0),
        # Vue d'ensemble a ~300 m : l'herbe est coupee a 108 m, ce qu'on voit d'ici c'est le
        # SOL -- la mosaique prairie / laiches / lande de la vue aerienne d'EZ5.
        ('aerien', (30.0, 34.0), 30000, (52.0, 58.0), 0),
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
            # La vue d'ensemble regarde surtout au-dela de la bande de photo (4 m).
            # Cette pose, au coeur du composant Ruin, juge les joints a hauteur humaine.
            gx, gy = cx + vy / vd * rr * 0.12, cy - vx / vd * rr * 0.12
            if ground(gx, gy) is not None:
                plan.append(('hameau_ground', (gx / T, gy / T), 170,
                             ((gx + vx / vd * 280) / T, (gy + vy / vd * 280) / T), 0))
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

    if any(s.startswith('gravel_') for s in states):
        # Un galet de berge vive est un temoin de site, pas la preuve du masque
        # peint. On vise son sol voisin puis l'A/B confirme la photo sous-jacente.
        candidates = []
        for comp in actor.get_components_by_class(unreal.HierarchicalInstancedStaticMeshComponent):
            if not comp.get_name().startswith('Riverbank_Cobble_'):
                continue
            for i in range(0, comp.get_instance_count(), max(1, comp.get_instance_count() // 300)):
                got = comp.get_instance_transform(i, True)
                xf = got[1] if isinstance(got, tuple) else got
                x, y, z = xf.translation.x, xf.translation.y, xf.translation.z
                if 8*T < x < 88*T and 8*T < y < 88*T:
                    candidates.append((math.hypot(x-50*T, y-56*T), x, y, z))
        if not candidates:
            raise RuntimeError('no riverbank cobble site for gravel capture')
        chosen = None
        for _, x, y, z in sorted(candidates)[:200]:
            for dx, dy in ((200, 0), (-200, 0), (0, 200), (0, -200)):
                eye_z = ground(x+dx, y+dy)
                if eye_z is not None and abs(eye_z-z) < 110:
                    chosen = (x, y, z, x+dx, y+dy, eye_z)
                    break
            if chosen:
                break
        if chosen is None:
            raise RuntimeError('no human-height gravel site beside riverbank cobbles')
        x, y, z, ex, ey, ez = chosen
        plan.append(('bank_ground', (ex/T, ey/T, ez+170), 0,
                     (x/T, y/T, z), 0))
        unreal.log('GROUND_CAPTURE_BANK_SITE x=%.1f y=%.1f' % (x, y))

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
            if 8 < tx < 88 and 8 < ty < 88:
                fern, best = (tx, ty), n
    if fern:
        dx, dy = 48.0 - fern[0], 48.0 - fern[1]
        d = math.hypot(dx, dy) or 1.0
        plan.append(('sousbois_eye', (fern[0] - dx / d * 0.4, fern[1] - dy / d * 0.4), 170, (fern[0] + dx / d * 0.6, fern[1] + dy / d * 0.6), 40))
    unreal.log('GROUND_CAPTURE_SOUSBOIS %s' % (('tile=%.1f,%.1f' % fern) if fern else 'NONE'))

    if micro_run:
        # Elect a real instance, then keep its camera fixed through on/off/on.
        tree_points = []
        for comp in actor.get_components_by_class(unreal.HierarchicalInstancedStaticMeshComponent):
            mesh = comp.get_editor_property('static_mesh')
            if mesh is None or not mesh.get_name().startswith('SM_Tree_'):
                continue
            for i in range(comp.get_instance_count()):
                got = comp.get_instance_transform(i, True)
                xf = got[1] if isinstance(got, tuple) else got
                tree_points.append((xf.translation.x, xf.translation.y))
        targets = (('micro_moss', 'TrunkContact_Moss'),
                   ('micro_horsetail', 'MicroEco_bank_Pontic_Horsetail_'),
                   ('micro_coltsfoot', 'MicroEco_bank_Pontic_Coltsfoot_'),
                   ('micro_frog', 'MicroEco_bank_Pontic_Frog_'))
        for label, prefix in targets:
            candidates = []
            for comp in actor.get_components_by_class(unreal.HierarchicalInstancedStaticMeshComponent):
                if not comp.get_name().startswith(prefix):
                    continue
                for i in range(comp.get_instance_count()):
                    got = comp.get_instance_transform(i, True)
                    xf = got[1] if isinstance(got, tuple) else got
                    x, y = xf.translation.x, xf.translation.y
                    if 8*T < x < 88*T and 8*T < y < 88*T:
                        candidates.append((math.hypot(x-50*T, y-56*T), x, y, xf.translation.z))
            if not candidates:
                raise RuntimeError('%s has no placed instance' % label)
            chosen_eye = None
            for _, x, y, target_z in sorted(candidates)[:120]:
                if label == 'micro_moss' and tree_points:
                    tx, ty = min(tree_points, key=lambda p: (p[0]-x)**2 + (p[1]-y)**2)
                    dist = math.hypot(x-tx, y-ty)
                    if dist < 90 or dist > 180:
                        continue
                    ux, uy = (x-tx)/dist, (y-ty)/dist
                    eye_x, eye_y = x+ux*240, y+uy*240
                    eye_z = ground(eye_x, eye_y)
                    if eye_z is not None and abs(eye_z-target_z) < 110:
                        chosen_eye = (eye_x, eye_y, eye_z)
                        break
                    continue
                for radius in (120, 200, 300):
                    for dx, dy in ((-1, 0), (1, 0), (0, -1), (0, 1), (-0.7, -0.7), (0.7, 0.7)):
                        eye_x, eye_y = x+dx*radius, y+dy*radius
                        eye_z = ground(eye_x, eye_y)
                        if eye_z is not None and abs(eye_z-target_z) < 110:
                            chosen_eye = (eye_x, eye_y, eye_z)
                            break
                    if chosen_eye is not None:
                        break
                if chosen_eye is not None:
                    break
            if chosen_eye is None:
                raise RuntimeError('%s has no nearby human-height pose' % label)
            plan.append((label, (chosen_eye[0]/T, chosen_eye[1]/T, chosen_eye[2]+170), 0,
                         (x/T, y/T, target_z + (8 if label in ('micro_moss', 'micro_frog') else 22)), 0))
            unreal.log('PONTIC_MICRO_SITE %s x=%.1f y=%.1f' % (label, x, y))

    if ecotone_run:
        # Actual tree on the open side of a stand; all poses chosen once from the reference.
        trees = {}
        for comp in actor.get_components_by_class(unreal.HierarchicalInstancedStaticMeshComponent):
            mesh = comp.get_editor_property('static_mesh')
            if mesh is None or not mesh.get_name().startswith('SM_Tree_'):
                continue
            for i in range(comp.get_instance_count()):
                got = comp.get_instance_transform(i, True)
                xf = got[1] if isinstance(got, tuple) else got
                x, y = xf.translation.x, xf.translation.y
                trees[(round(x), round(y))] = (x, y)
        points = list(trees.values())
        target = (83*T, 50*T) if woodland_run else (28*T, 58*T)
        candidates = sorted(points, key=lambda p: (p[0]-target[0])**2+(p[1]-target[1])**2)[:1200 if woodland_run else 250]
        chosen = None
        def nearby(x, y):
            return sum((px-x)**2+(py-y)**2 < 2200**2 for px, py in points)
        for x, y in candidates:
            dx, dy = 48*T-x, (48 if woodland_run else 58)*T-y
            length = math.hypot(dx, dy)
            if length < 1:
                continue
            dx, dy = dx/length, dy/length
            outer = (x+dx*1800, y+dy*1800)
            inner = (x-dx*1800, y-dy*1800)
            front, back = nearby(*outer), nearby(*inner)
            if (back < 6 or front > back*0.45) if woodland_run else (back < 3 or front >= back):
                continue
            heights = [ground(x+dx*d, y+dy*d) for d in (-1800, -800, 0, 800, 1800, 3500)]
            if any(z is None for z in heights):
                continue
            if max(heights)-min(heights) > 1700:
                continue
            chosen = (x, y, dx, dy, front, back)
            break
        if chosen is None:
            raise RuntimeError('no observable near forest edge found')
        x, y, dx, dy, front, back = chosen
        if woodland_run:
            # Four human-height positions including a locally open pocket inside the stand.
            gap_candidates=[]
            for inward in (2000,3500,5000):
                for side in (-3000,-1500,1500,3000):
                    gx,gy=x-dx*inward-dy*side,y-dy*inward+dx*side
                    gz=ground(gx,gy)
                    if gz is not None:
                        gap_candidates.append((nearby(gx,gy),inward,abs(side),gx,gy))
            if not gap_candidates: raise RuntimeError('no observable woodland gap')
            gap=min(gap_candidates)
            plan.append(('woodland_gap',(gap[3]/T,gap[4]/T),170,((x-dx*1800)/T,(y-dy*1800)/T),170))
        for name, offset, lift in [('ecotone_open', 1800, 170),
                                   ('ecotone_edge', 800, 170),
                                   ('ecotone_inside', -2200 if woodland_run else -800, 170),
                                   ('ecotone_oblique', 3500, 3500 if woodland_run else 1400)]:
            plan.append((name, ((x+dx*offset)/T, (y+dy*offset)/T), lift,
                         ((x-dx*(4000 if woodland_run else 2200))/T, (y-dy*(4000 if woodland_run else 2200))/T), 170))
        with open(os.path.join(OUT, 'ecotone-site.json'), 'w') as f:
            json.dump({'tree_xy': [x,y], 'outward_xy': [dx,dy],
                       'front_tree_count_22m': front, 'back_tree_count_22m': back,
                       'reference_tree_instances': len(points), 'human_eye_cm':170}, f, indent=1)
        unreal.log('ECOTONE_SITE tree=%.1f,%.1f front=%d back=%d' % (x,y,front,back))

    selected = os.environ.get('ANASTASIS_GROUND_VIEWS', '')
    if selected:
        wanted = set(selected.split(','))
        plan = [p for p in plan if p[0] in wanted]
        if {p[0] for p in plan} != wanted:
            raise RuntimeError('requested ecological view missing: %s' % (wanted - {p[0] for p in plan}))
    views = []
    for name, eye_t, lift, tgt_t, tlift in plan:
        eye = V(eye_t[0]*T, eye_t[1]*T, eye_t[2]) if len(eye_t) == 3 else at(eye_t[0], eye_t[1], lift)
        tgt = V(tgt_t[0]*T, tgt_t[1]*T, tgt_t[2]) if len(tgt_t) == 3 else at(tgt_t[0], tgt_t[1], tlift)
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

look = unreal.MathLibrary.find_look_at_rotation
camera_file = os.environ.get('ANASTASIS_GROUND_CAMERAS')
if camera_file:
    with open(camera_file, encoding='utf-8') as f:
        saved_views = json.load(f)['views']
    views = [(n, V(*e), V(*t)) for n, e, t in saved_views]
selected_views = os.environ.get('ANASTASIS_CAPTURE_VIEWS', '').split(',')
if selected_views != ['']:
    views = [v for v in views if v[0] in selected_views]
# Record the poses actually used, including an explicitly supplied baseline.
with open(os.path.join(OUT, 'cameras.json'), 'w') as f:
    json.dump({'tile_uu': T, 'camera_source': camera_file,
               'views': [[n, [e.x,e.y,e.z], [t.x,t.y,t.z]] for n,e,t in views]}, f, indent=1)
queue = [(states[0], v) for v in views]
state_i = 0
phase, mark, shot, first = 'boot', time.monotonic(), None, True
walk_done, walk_samples = False, []
frames = []
timings = []
metrics = {}


def tick(dt):
    global phase, mark, shot, first, state_i, queue, frames, timings, walk_done
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
                if woodland_run and states[state_i]=='woodland' and not walk_done:
                    phase,mark='walk',time.monotonic()
                    return
                state_i += 1
                if state_i >= len(states):
                    with open(os.path.join(OUT, 'ground-cover.json'), 'w') as f:
                        json.dump(metrics, f, indent=1)
                    if natural_run:
                        with open(os.path.join(OUT, 'habitat.json'), 'w') as f:
                            json.dump(habitat, f, indent=1)
                        hashes = {v['sampled_ground_sha256'] for v in habitat.values()}
                        if len(hashes) != 1:
                            raise RuntimeError('terrain samples changed between states')
                        ref = 'woodland_reference' if woodland_run else 'riparian_reference' if riparian_run else 'ecotone_reference' if ecotone_run else 'reference'
                        if ref+'2' in habitat and habitat[ref]['spatial_inventory'] != habitat[ref+'2']['spatial_inventory']:
                            raise RuntimeError('reference spatial inventory not reproducible')
                        if ecotone_run and any(v['non_micro_inventory'] != habitat[ref]['non_micro_inventory'] for v in habitat.values()):
                            raise RuntimeError('ecotone changed sampled non-micro vegetation')
                        if woodland_run:
                            if any(v['protected_micro'] != habitat[ref]['protected_micro'] for v in habitat.values()):
                                raise RuntimeError('woodland changed bank/meadow positions')
                            if any(sum(v['instances'].values()) > sum(habitat[ref]['instances'].values()) for v in habitat.values()):
                                raise RuntimeError('woodland increased instance budget')
                        if riparian_run:
                            base = habitat[ref]
                            for entry in habitat.values():
                                if sum(entry['instances'].values()) != sum(base['instances'].values()):
                                    raise RuntimeError('riparian transition changed instance budget')
                            if habitat['riparian']['spatial_inventory'] == base['spatial_inventory']:
                                raise RuntimeError('riparian transition had no observable placement effect')
                        marker = 'WOODLAND_CAPTURE' if woodland_run else 'RIPARIAN_CAPTURE' if riparian_run else 'ECOTONE_CAPTURE' if ecotone_run else 'NATURAL_HISTORY_CAPTURE'
                        unreal.log(marker + ' PASS sampled_ground_unchanged=1 views=%d' % len(views))
                    if micro_run:
                        with open(os.path.join(OUT, 'micro-inventory.json'), 'w') as f:
                            json.dump(micro_inventory, f, indent=1)
                        if horsetail_run:
                            old, new, repeat = (micro_inventory[s] for s in ('horsetail_old', 'horsetail_new', 'horsetail_old2'))
                            old_names = {item[0] for item in old['bank']}
                            new_names = {item[0] for item in new['bank']}
                            if old != repeat or 'SM_Pontic_Horsetail_01' not in old_names \
                                    or 'SM_Pontic_Horsetail_02' not in new_names or old['moss_count'] <= 0:
                                raise RuntimeError('horsetail old/new/old missing asset or return to baseline')
                            def normalized(bank):
                                return sorted([['SM_Pontic_Horsetail_01' if name == 'SM_Pontic_Horsetail_02' else name, positions]
                                               for name, positions in bank])
                            if normalized(new['bank']) != old['bank'] or new['moss_mesh'] != old['moss_mesh'] \
                                    or new['moss_count'] != old['moss_count']:
                                raise RuntimeError('horsetail candidate changed placements or other micro assets')
                            unreal.log('PONTIC_HORSETAIL_ABA PASS views=%d spatial_inventory=stable' % len(views))
                        else:
                            on, off, repeat = (micro_inventory[s] for s in ('micro_on', 'micro_off', 'micro_on2'))
                            names = {item[0] for item in on['bank']}
                            if not all('SM_Pontic_' + suffix + '_01' in names for suffix in ('Horsetail', 'Coltsfoot', 'Frog')):
                                raise RuntimeError('one or more bank assets absent from scene')
                            if on != repeat or off['bank'] or on['moss_mesh'] != 'SM_Pontic_Moss_01' \
                                    or off['moss_mesh'] != 'SM_Grass_Sedge_01' or on['moss_count'] <= 0:
                                raise RuntimeError('Pontic micro on/off/on inventory invalid')
                            unreal.log('PONTIC_MICRO_CAPTURE PASS bank_types=3 moss_count=%d views=%d' % (on['moss_count'], len(views)))
                    finish('GROUND_CAPTURE_COMPLETE views=%d states=%d' % (len(views), len(states)))
                    return
                for c in STATE_CMDS[states[state_i]]:
                    cmd(c)
                actor.call_method('EmbodyCanonical', args=(SEED,))
                texture_state(states[state_i])
                record_habitat(states[state_i])
                record_micro(states[state_i])
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
        elif phase == 'walk':
            by_name={n:(eye,tgt) for n,eye,tgt in views}
            a,ta=by_name['ecotone_open']; b,tb=by_name['ecotone_edge']
            u=min(1.0,el/4.0)
            x,y=a.x+(b.x-a.x)*u,a.y+(b.y-a.y)*u
            z=ground(x,y)
            if z is None: raise RuntimeError('walk terrain missing')
            eye=V(x,y,z+170)
            target=V(ta.x+(tb.x-ta.x)*u,ta.y+(tb.y-ta.y)*u,ta.z+(tb.z-ta.z)*u)
            ues.set_level_viewport_camera_info(eye,look(eye,target))
            if len(walk_samples)==0 or u*12>=len(walk_samples):
                walk_samples.append([u,x,y,z+170])
            if u>=1:
                with open(os.path.join(OUT,'woodland-walk.json'),'w') as f:
                    json.dump({'eye_cm':170,'distance_xy_cm':math.hypot(b.x-a.x,b.y-a.y),'samples':walk_samples},f,indent=1)
                shot=os.path.join(OUT,'woodland_walk_end.png').replace('\\','/')
                if os.path.exists(shot): os.remove(shot)
                cmd('HighResShot 1600x900 filename="%s"'%shot)
                phase,mark='walk_shot',time.monotonic()
        elif phase == 'walk_shot':
            if os.path.isfile(shot) and el>1:
                unreal.log('WOODLAND_WALK_COMPLETE eye_cm=170 samples=%d'%len(walk_samples))
                walk_done=True
                phase,mark='aim',time.monotonic()
            elif el>45: raise RuntimeError('woodland walk shot missing')
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
