"""HORIZON_RING_001 -- captures de l'horizon, une seule CVar variable.

Lance par capture-horizon.ps1, une fois par etat. Variables d'environnement :
  ANASTASIS_HORIZON_OUT   dossier de sortie
  ANASTASIS_HORIZON_CVAR  valeur de anastasis.Terrain.Horizon (0 = rien au-dela du bord)
  ANASTASIS_HORIZON_TAG   suffixe des images (A, B...)
  ANASTASIS_HORIZON_PRE   commandes console en plus, separees par ';' (etape brume)
  ANASTASIS_HORIZON_ATMOSPHERE  1 = appliquer le profil d'atmosphere comme en PIE
  ANASTASIS_HORIZON_MODE  standard (les cinq vues ci-dessous) ou skyline (CONTINENTAL_001 : huit vues
                          a hauteur d'oeil depuis le bassin, tous les 45 degres, S000..S315)

Cinq cameras, calees sur la carte REELLE (bornes du composant ExperimentalTerrain, pas
de l'acteur, que l'anneau agrandit) et sur le bassin / le point haut de la forge :
  H1 a 30 m au-dessus du bassin, vers le bord le plus proche
  H2 a 15 m au-dessus du point haut, vers l'exterieur
  H3 au bord meme de la carte, a hauteur d'homme, regard dehors
  H4 vue generale
  H5 a 150 m au-dessus du bassin, vers le coin le plus lointain
"""
import os, time, unreal

OUT = os.environ['ANASTASIS_HORIZON_OUT']
CVAR = os.environ.get('ANASTASIS_HORIZON_CVAR', '1')
TAG = os.environ.get('ANASTASIS_HORIZON_TAG', 'X')
PRE = [c.strip() for c in os.environ.get('ANASTASIS_HORIZON_PRE', '').split(';') if c.strip()]
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
les.load_level('/Game/Anastasis/Maps/Lvl_AnastasisSlice')
world = ues.get_editor_world()
look = unreal.MathLibrary.find_look_at_rotation


def cmd(c):
    unreal.SystemLibrary.execute_console_command(world, c)


def redraw():
    try:
        les.editor_invalidate_viewports()
    except Exception:
        pass


for c in ['ShowFlag.Sprites 0', 'ShowFlag.Grid 0', 'viewmode lit', 'anastasis.Terrain.Horizon ' + CVAR] + PRE:
    cmd(c)

cls = unreal.load_class(None, '/Script/Anastasis_UnrealV2.AnastasisWorldEmbodiment')
found = unreal.GameplayStatics.get_all_actors_of_class(world, cls)
unreal.log('HORIZON_ACTORS existing=%d names=%s' % (len(found), ','.join(a.get_name() for a in found)))
actor = found[0] if len(found) > 0 else eas.spawn_actor_from_class(cls, unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0))
actor.call_method('EmbodyCanonical', args=(12345,))
unreal.log('HORIZON_ACTOR_SELECTED %s' % actor.get_name())

# ANASTASIS_HORIZON_ATMOSPHERE=1 : l'atmosphere du JEU, pas celle du niveau. En PIE, le
# GameMode fait Apply() (soleil, ciel, brume, exposition du profil DA_AnastasisAtmosphere)
# puis ApplyMist() apres l'incarnation ; l'editeur, lui, montre l'eclairage enregistre
# dans la map. Meme ordre ici, rien n'est sauve.
if os.environ.get('ANASTASIS_HORIZON_ATMOSPHERE', '0') == '1':
    acls = unreal.load_class(None, '/Script/Anastasis_UnrealV2.AnastasisWorldAtmosphere')
    atmo = eas.spawn_actor_from_class(acls, unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0))
    applied = atmo.call_method('Apply')
    pockets = atmo.call_method('ApplyMist')
    unreal.log('HORIZON_ATMOSPHERE applied=%s mist_pockets=%s' % (applied, pockets))
# ANASTASIS_HORIZON_ATMO_PROPS : diagnostic, `sky.<propriete>=<valeur>;fog.<propriete>=<valeur>` appliques apres Apply().
# Sert a attribuer la blancheur des montagnes lointaines (perspective aerienne ou brouillard exponentiel) ; rien n'est sauve.
for item in [x.strip() for x in os.environ.get('ANASTASIS_HORIZON_ATMO_PROPS', '').split(';') if x.strip()]:
    target, _, assign = item.partition('.')
    prop, _, value = assign.partition('=')
    if target == 'sky':
        for sky in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.SkyAtmosphere):
            sky.get_component_by_class(unreal.SkyAtmosphereComponent).set_editor_property(prop, float(value))
    elif target == 'fog':
        for fog in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.ExponentialHeightFog):
            fog.get_component_by_class(unreal.ExponentialHeightFogComponent).set_editor_property(prop, float(value))
    unreal.log('HORIZON_ATMO_PROP %s' % item)
for fog in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.ExponentialHeightFog):
    fc = fog.get_component_by_class(unreal.ExponentialHeightFogComponent)
    unreal.log('HORIZON_FOG %s z=%.0f density=%.4f falloff=%.3f start=%.0f max_opacity=%.2f volumetric=%s' % (
        fog.get_name(), fog.get_actor_location().z, fc.get_editor_property('fog_density'),
        fc.get_editor_property('fog_height_falloff'), fc.get_editor_property('start_distance'),
        fc.get_editor_property('fog_max_opacity'), fc.get_editor_property('enable_volumetric_fog')))
for sky in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.SkyAtmosphere):
    sc = sky.get_component_by_class(unreal.SkyAtmosphereComponent)
    unreal.log('HORIZON_SKY %s aerial_scale=%.2f height_fog_contribution=%.2f' % (
        sky.get_name(), sc.get_editor_property('aerial_pespective_view_distance_scale'),
        sc.get_editor_property('height_fog_contribution')))
basin = actor.call_method('GetTerrainForgeBasin')
landmark = actor.call_method('GetTerrainForgeLandmark')

terrain = None
for comp in actor.get_components_by_class(unreal.ProceduralMeshComponent):
    unreal.log('HORIZON_COMP %s visible=%s' % (comp.get_name(), comp.is_visible()))
    if comp.get_name() == 'ExperimentalTerrain':
        terrain = comp
o, e, _ = unreal.SystemLibrary.get_component_bounds(terrain)
x0, x1, y0, y1 = o.x - e.x, o.x + e.x, o.y - e.y, o.y + e.y
unreal.log('HORIZON_MAP x=[%.0f,%.0f] y=[%.0f,%.0f] z=[%.0f,%.0f]' % (x0, x1, y0, y1, o.z - e.z, o.z + e.z))


def ground(x, y):
    hit = unreal.SystemLibrary.line_trace_single(world, unreal.Vector(x, y, 1.0e6), unreal.Vector(x, y, -1.0e5),
                                                 unreal.TraceTypeQuery.ECC_VISIBILITY, True, [], unreal.DrawDebugTrace.NONE, True)
    h = hit[1] if isinstance(hit, tuple) else hit
    try:
        return h.to_tuple()[4].z
    except Exception:
        return h.impact_point.z


def level(yaw, pitch=0.0):
    return unreal.Rotator(0.0, pitch, yaw)


def nearest_out(p):
    d = {0.0: x1 - p.x, 180.0: p.x - x0, 90.0: y1 - p.y, 270.0: p.y - y0}
    return min(d, key=d.get)


cx, cy = 0.5 * (x0 + x1), 0.5 * (y0 + y1)
views = []
e1 = unreal.Vector(basin.x, basin.y, ground(basin.x, basin.y) + 3000.0)
views.append(('H1_bassin_vers_le_bord', e1, level(nearest_out(basin), -1.0)))
e2 = unreal.Vector(landmark.x, landmark.y, landmark.z + 1500.0)
views.append(('H2_point_haut_vers_l_exterieur', e2, level(nearest_out(landmark), -3.0)))
e3 = unreal.Vector(cx, y1 - 100.0, ground(cx, y1 - 100.0) + 170.0)
views.append(('H3_bord_regard_dehors', e3, level(90.0, -1.0)))
E = max(e.x, e.y)
over = unreal.Vector(o.x - E * 1.05, o.y - E * 1.05, o.z + E * 0.95)
views.append(('H4_vue_generale', over, look(over, unreal.Vector(o.x, o.y, o.z - e.z * 0.5))))
far = unreal.Vector(x1 if basin.x < cx else x0, y1 if basin.y < cy else y0, basin.z)
e5 = unreal.Vector(basin.x, basin.y, basin.z + 15000.0)
views.append(('H5_altitude_vers_le_coin', e5, level(look(e5, far).yaw, -4.0)))

if os.environ.get('ANASTASIS_HORIZON_MODE', 'standard') == 'skyline':
    # La ligne de crete du continent, dans toutes les directions, a 1,7 m du sol du bassin.
    eye = unreal.Vector(basin.x, basin.y, ground(basin.x, basin.y) + 170.0)
    views = [('S%03d' % a, eye, level(float(a), 3.0)) for a in range(0, 360, 45)]

only_view = os.environ.get('ANASTASIS_HORIZON_VIEW', '')
if only_view:
    views = [entry for entry in views if entry[0] == only_view]
    if not views:
        raise ValueError('Vue horizon inconnue: ' + only_view)

for n, l, r in views:
    unreal.log('HORIZON_VIEW %s loc=(%.0f,%.0f,%.0f) pitch=%.1f yaw=%.1f' % (n, l.x, l.y, l.z, r.pitch, r.yaw))

queue = list(views)
phase, mark, shot, first = 'aim', time.monotonic(), None, True
gpu = []  # GPU ms de stat unit (GetFrameTimingsMs), echantillonne pendant l'attente de la vue


def finish(msg, error=False):
    # Pas de desenregistrement depuis le rappel lui-meme : l'editeur 5.8 plantait dans
    # python311 a la sortie (EXCEPTION_ACCESS_VIOLATION apres HORIZON_COMPLETE, captures
    # deja ecrites). Le rappel se contente de ne plus rien faire.
    global phase
    phase = 'done'
    (unreal.log_error if error else unreal.log)(msg)
    unreal.SystemLibrary.quit_editor()


def tick(_dt):
    global phase, mark, shot, first, gpu
    if phase == 'done':
        return
    redraw()
    elapsed = time.monotonic() - mark
    if phase == 'aim' and elapsed > 2.5:
        try:
            gpu.append(actor.call_method('GetFrameTimingsMs').z)
        except Exception:
            pass
    if phase == 'aim':
        if not queue:
            finish('HORIZON_COMPLETE')
            return
        name, loc, rot = queue[0]
        ues.set_level_viewport_camera_info(loc, rot)
        if elapsed > (14.0 if first else 5.0):
            shot = os.path.join(OUT, '%s_%s.png' % (name, TAG)).replace('\\', '/')
            if os.path.exists(shot):
                os.remove(shot)
            cmd('HighResShot 1920x1080 filename="%s"' % shot)
            phase, mark = 'wait', time.monotonic()
    elif phase == 'wait':
        if not os.path.isfile(shot):
            if elapsed > 40.0:
                finish('HORIZON_SHOT_MISSING %s' % shot, True)
            return
        if elapsed > 1.5:
            g = sorted(gpu)
            unreal.log('HORIZON_SHOT_OK %s bytes=%d gpu_ms_p50=%.2f' % (os.path.basename(shot), os.path.getsize(shot),
                                                                       g[len(g) // 2] if g else -1.0))
            gpu = []
            queue.pop(0)
            first, phase, mark = False, 'aim', time.monotonic()


unreal.register_slate_post_tick_callback(tick)
