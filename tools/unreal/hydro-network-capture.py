"""HYDRO_NETWORK_001 -- preuve du reseau de drainage. Lecture seule : ne sauve ni niveau ni asset.

Lance par tools/unreal/hydro-network-capture.ps1 dans un editeur dedie.
Pour chaque etat de anastasis.Terrain.Drainage (defaut "0,1") :
  - incarne le monde canonique 12345 ;
  - exporte le relief rendu (section 0) et la nappe (section 1) en grille fine :
    drainage_<etat>_grid.json = {fine_w, fine_h, step_uu, ground_z[], water_z[]} ;
  - prend les vues de ANASTASIS_HYDRO_SHOTS (json [[nom,[x,y,z],[tx,ty,tz]],...])
    ou, a defaut, une vue zenithale et une vue oblique du monde entier.
Les lignes ANASTASIS_DRAINAGE du journal (reseau, confluences, lacs) sont ecrites
par le C++ a chaque incarnation ; le .ps1 les recopie.
"""
import unreal, json, os, time, traceback

OUT = os.environ['ANASTASIS_HYDRO_OUT']
STATES = [s for s in os.environ.get('ANASTASIS_HYDRO_STATES', '0,1').split(',') if s]
SHOTS_FILE = os.environ.get('ANASTASIS_HYDRO_SHOTS', '')
# anastasis.Drainage.Debug : 0 rien, 1 largeur, 2 profondeur, 3 vitesse, 4 ordre de Strahler.
DEBUG = os.environ.get('ANASTASIS_HYDRO_DEBUG', '0')
# Commandes console avant chaque incarnation (ex. "anastasis.Terrain.HumanGeography 0").
PRECMDS = [c.strip() for c in os.environ.get('ANASTASIS_HYDRO_PRECMDS', '').split(';') if c.strip()]
os.makedirs(OUT, exist_ok=True)

ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
world = ues.get_editor_world()
actor = next(a for a in eas.get_all_level_actors() if a.get_class().get_name() == 'AnastasisWorldEmbodiment')


def cmd(s):
    unreal.SystemLibrary.execute_console_command(world, s)


# Monde entier : 96 tuiles x 400 uu x echelle 5 = 192 000 uu de cote.
DEFAULT_SHOTS = [
    ('aerial_top', [96000, 96000, 118000], [96000, 96001, 0]),
    ('aerial_oblique', [96000, -60000, 150000], [96000, 90000, 0]),
    # Gros plans, seed 12345 : confluence de plaine, riviere principale, lac ecrit,
    # embouchure du ruisseau (plaine d'inondation), exutoire d'un lac d'altitude.
    ('close_confluence', [83000, 89000, 22000], [108000, 114000, 1200]),
    ('close_mainriver', [70000, 105000, 20000], [94000, 127000, 1000]),
    ('close_lake', [110000, 115000, 24000], [137000, 141000, 300]),
    ('close_brookmouth', [30000, 16000, 18000], [54000, 40000, 900]),
    ('close_upland_outlet', [72000, 30000, 26000], [94000, 51000, 2000]),
]
try:
    if SHOTS_FILE:
        with open(SHOTS_FILE, encoding='utf-8') as f:
            SHOTS = [tuple(s) for s in json.load(f)]
    else:
        SHOTS = DEFAULT_SHOTS
except Exception:
    # Sans vues, rien a faire : quitter plutot que laisser l'editeur attendre son delai.
    unreal.log_error('HYDRO_CAPTURE_FAIL ' + traceback.format_exc())
    unreal.SystemLibrary.quit_editor()
    raise

cmd('ShowFlag.Sprites 0')
cmd('ShowFlag.Grid 0')
# Vue de carte : le brouillard noie le reseau a 2 km d'altitude. Diagnostic, pas un rendu.
cmd('ShowFlag.Fog 0')
cmd('ShowFlag.VolumetricFog 0')


def export_grid(state):
    comp = actor.get_components_by_class(unreal.ProceduralMeshComponent)[0]
    tr = comp.get_world_transform()
    sections = {}
    for section in (0, 1):
        if section >= comp.get_num_sections():
            sections[section] = []
            continue
        verts = unreal.ProceduralMeshLibrary.get_section_from_procedural_mesh(comp, section)[0]
        sections[section] = [unreal.MathLibrary.transform_location(tr, p) for p in verts]
    ground = sections[0]
    xs = sorted({round(p.x, 1) for p in ground})
    ys = sorted({round(p.y, 1) for p in ground})
    fine_w, fine_h = len(xs), len(ys)
    if fine_w * fine_h != len(ground):
        raise RuntimeError('ground is not a regular grid: %d x %d != %d' % (fine_w, fine_h, len(ground)))
    water = sections[1]
    data = {
        'state': state,
        'fine_w': fine_w, 'fine_h': fine_h,
        'x0': xs[0], 'y0': ys[0], 'step_uu': xs[1] - xs[0],
        'ground_z': [round(p.z, 1) for p in ground],
        # La nappe a les memes sommets que le relief (cf. FGeometry::WaterVertices).
        'water_z': [round(p.z, 1) for p in water] if len(water) == len(ground) else [],
        'project_dir': unreal.Paths.project_dir(),
    }
    with open(os.path.join(OUT, 'drainage_%s_grid.json' % state), 'w', encoding='utf-8') as f:
        json.dump(data, f)
    unreal.log('HYDRO_CAPTURE grid state=%s fine=%dx%d water_vertices=%d' % (state, fine_w, fine_h, len(water)))


state_i = -1
shot_i = 0
mark = time.monotonic()
requested = False
handle = None


def prepare():
    global shot_i, mark, requested
    state = STATES[state_i]
    cmd('anastasis.Terrain.Drainage ' + state)
    cmd('anastasis.Drainage.Debug ' + DEBUG)
    for c in PRECMDS:
        cmd(c)
    cmd('anastasis.Drainage.Dump %s/drainage_%s_network.json' % (OUT.replace(chr(92), '/'), state))
    actor.call_method('EmbodyCanonical', args=(12345,))
    export_grid(state)
    shot_i = 0
    mark = time.monotonic()
    requested = False


def tick(dt):
    global state_i, shot_i, mark, requested
    try:
        les.editor_invalidate_viewports()
        if state_i < 0 or shot_i >= len(SHOTS):
            state_i += 1
            if state_i >= len(STATES):
                unreal.unregister_slate_post_tick_callback(handle)
                unreal.log('HYDRO_CAPTURE_COMPLETE')
                unreal.SystemLibrary.quit_editor()
                return
            prepare()
            return
        name, loc, target = SHOTS[shot_i]
        suffix = STATES[state_i] if DEBUG == '0' else '%s_debug%s' % (STATES[state_i], DEBUG)
        path = '%s/%s_%s.png' % (OUT.replace('\\', '/'), name, suffix)
        elapsed = time.monotonic() - mark
        if not requested:
            ues.set_level_viewport_camera_info(
                unreal.Vector(*loc),
                unreal.MathLibrary.find_look_at_rotation(unreal.Vector(*loc), unreal.Vector(*target)))
            if elapsed > 6:
                cmd('HighResShot 1600x1600 filename="%s"' % path)
                requested = True
                mark = time.monotonic()
        elif os.path.isfile(path):
            unreal.log('HYDRO_CAPTURE shot %s' % path)
            shot_i += 1
            requested = False
            mark = time.monotonic()
        elif elapsed > 45:
            raise RuntimeError('missing ' + path)
    except Exception:
        unreal.log_error('HYDRO_CAPTURE_FAIL ' + traceback.format_exc())
        unreal.unregister_slate_post_tick_callback(handle)
        unreal.SystemLibrary.quit_editor()


handle = unreal.register_slate_post_tick_callback(tick)
