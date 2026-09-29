"""TERRAIN_RELIEF_001 -- captures avant / apres d'une etape de la forge.

Memes vues, meme build, seules les variables de l'etape changent
(ANASTASIS_RELIEF_STEP, defaut la derniere) :
  etape 1  avant = Terraces 1, Escarpments 1   apres = 0, 0   (Bicubic 0, Sharpen 1)
  etape 2  avant = Bicubic 0, Sharpen 1        apres = 1, 0
Toute variable non citee garde son defaut : l'"avant" d'une etape est l'"apres" de la
precedente. Le dressing (arbres, ruines) est masque : on juge le relief, rien d'autre.

HighResShot est servi par le prochain redessin du viewport ; une scene statique
ne se redessine pas, d'ou l'invalidation a chaque tick (cf. terrain-forge-capture.py).
"""
import os, time, unreal

OUT_DIR = os.environ.get('ANASTASIS_RELIEF_OUT', os.path.join(unreal.Paths.project_saved_dir(), 'TerrainReliefEvidence'))
os.makedirs(OUT_DIR, exist_ok=True)
SEED = 12345
LEVEL = '/Game/Anastasis/Maps/Lvl_AnastasisSlice'
FORGE = 'anastasis.Terrain.Forge.'
STEPS = {
    # L'etape 1 a ete capturee avant que l'etape 2 change les defauts : on les epingle.
    '1': [('before', {'Bicubic': 0, 'Sharpen': 1, 'Terraces': 1, 'Escarpments': 1}),
          ('after', {'Bicubic': 0, 'Sharpen': 1, 'Terraces': 0, 'Escarpments': 0})],
    '2': [('before', {'Bicubic': 0, 'Sharpen': 1}), ('after', {'Bicubic': 1, 'Sharpen': 0})],
}
STEP = os.environ.get('ANASTASIS_RELIEF_STEP', '2')
STATES = STEPS[STEP]

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

unreal.log('RELIEF_CAPTURE_BOOT out=%s' % OUT_DIR)
les.load_level(LEVEL)
world = ues.get_editor_world()


def cmd(c):
    unreal.SystemLibrary.execute_console_command(world, c)


def redraw():
    try:
        les.editor_invalidate_viewports()
    except Exception:
        pass


for c in ('ShowFlag.Sprites 0', 'ShowFlag.Grid 0', 'viewmode lit', 'anastasis.Terrain.Surface 2', 'anastasis.Terrain.Forge 1'):
    cmd(c)

cls = unreal.load_class(None, '/Script/Anastasis_UnrealV2.AnastasisWorldEmbodiment')
found = unreal.GameplayStatics.get_all_actors_of_class(world, cls)
actor = found[0] if len(found) > 0 else eas.spawn_actor_from_class(cls, unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0))


def embody(cvars):
    for name, value in cvars.items():
        cmd('%s%s %d' % (FORGE, name, value))
    ok = actor.call_method('EmbodyCanonical', args=(SEED,))
    for comp in actor.get_components_by_class(unreal.HierarchicalInstancedStaticMeshComponent):
        comp.set_visibility(False)
    redraw()
    unreal.log('RELIEF_EMBODY step=%s %s ok=%s'
               % (STEP, ' '.join('%s=%d' % kv for kv in cvars.items()), ok))


def views():
    """Vues calees sur les sites que la forge elle-meme designe (bassin, point haut)."""
    basin = actor.call_method('GetTerrainForgeBasin')
    landmark = actor.call_method('GetTerrainForgeLandmark')
    unreal.log('RELIEF_SITES basin=(%.0f,%.0f,%.0f) landmark=(%.0f,%.0f,%.0f)'
               % (basin.x, basin.y, basin.z, landmark.x, landmark.y, landmark.z))
    look = unreal.MathLibrary.find_look_at_rotation
    # Cadres choisis sur la carte de difference avant/apres de la premiere passe : les
    # terrasses mordaient surtout les versants du rempart de bordure (X~0 et Y~9600).
    # Un oeil decale du bassin tombait contre une paroi : l'oeil est SUR le bassin, a
    # hauteur d'homme, et regarde le rempart ouest.
    eye = unreal.Vector(basin.x, basin.y, basin.z + 170.0)
    rim_w = unreal.Vector(600.0, basin.y - 300.0, basin.z + 350.0)
    slope = unreal.Vector(3000.0, 5200.0, 3200.0)
    over = unreal.Vector(-2200.0, -2200.0, 7800.0)
    return [
        ('A_overview', over, look(over, unreal.Vector(4800.0, 4800.0, 300.0))),
        ('B_ground', eye, look(eye, rim_w)),
        ('C_slope', slope, look(slope, unreal.Vector(6000.0, 8800.0, 900.0))),
    ]


def aim(loc, rot):
    ues.set_level_viewport_camera_info(loc, rot)
    redraw()


# File d'attente : (etat, vue) ; les vues sont calculees apres le premier embody.
queue = []
state_i = 0
view_list = None
phase = 'boot'
mark = time.monotonic()
shot_path = None
handle = None


def finish(msg, error=False):
    (unreal.log_error if error else unreal.log)(msg)
    unreal.unregister_slate_post_tick_callback(handle)
    unreal.SystemLibrary.quit_editor()


def start_state():
    global queue, view_list
    name, cvars = STATES[state_i]
    embody(cvars)
    if view_list is None:
        view_list = views()
    queue = [(name, v) for v in view_list]


def tick(_dt):
    global phase, mark, shot_path, state_i
    elapsed = time.monotonic() - mark
    redraw()
    if elapsed > 90:
        finish('RELIEF_TIMEOUT phase=%s' % phase, True)
        return
    if phase == 'boot':
        if elapsed > 3.0:
            start_state()
            phase = 'aim'
            mark = time.monotonic()
    elif phase == 'aim':
        if not queue:
            state_i += 1
            if state_i >= len(STATES):
                finish('RELIEF_CAPTURE_COMPLETE')
                return
            start_state()
            mark = time.monotonic()
            return
        state, (view, loc, rot) = queue[0]
        aim(loc, rot)
        # Premier plan d'un etat : laisser compiler shaders et Lumen converger.
        if elapsed > (8.0 if len(queue) == len(view_list) else 4.0):
            shot_path = os.path.join(OUT_DIR, '%s_%s.png' % (view, state)).replace('\\', '/')
            if os.path.exists(shot_path):
                os.remove(shot_path)
            cmd('HighResShot 1920x1080 filename="%s"' % shot_path)
            phase = 'wait'
            mark = time.monotonic()
    elif phase == 'wait':
        # Le fichier apparait quand le redessin suivant sert la demande : attendre le
        # fichier, pas une duree. 6 s fixes ne suffisaient pas au premier plan.
        if not os.path.isfile(shot_path):
            if elapsed > 30.0:
                finish('RELIEF_SHOT_MISSING %s' % shot_path, True)
            return
        if elapsed > 1.0:
            unreal.log('RELIEF_SHOT_OK %s bytes=%d' % (os.path.basename(shot_path), os.path.getsize(shot_path)))
            queue.pop(0)
            phase = 'aim'
            mark = time.monotonic()


handle = unreal.register_slate_post_tick_callback(tick)
