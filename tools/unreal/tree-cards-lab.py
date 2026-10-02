"""LEAFCARDS_001 -- A/B du chene vert : lames opaques (SM_Tree_HolmOak_01) contre cartes (SM_Tree_HolmOak_Card_01).

Meme graine de tronc et de branches, meme hauteur (11 m), meme lumiere (Lvl_AnastasisSlice : soleil
75 000 lux, EV100 fige, ciel epingle a 11 h). La couronne est la SEULE variable. Rien n'est sauve :
des acteurs transitoires hors de l'emprise du monde, le niveau n'est jamais enregistre.

A est a gauche, B a droite, vu de face (de dos, l'ordre s'inverse : le journal le dit).

Vues (1600 x 900, sortie <vue>.png) :
  near_front   arbre seul, a 9 m, de face, a hauteur d'oeil
  near_back    de dos par rapport au soleil : la lumiere traverse-t-elle la couronne ?
  mid_front    a 25 m
  far_front    a 70 m : la couronne tient-elle sa masse, ou se depegarnit-elle (le defaut de tree-canopy-002) ?
  under_A / under_B  sous la couronne, a 2,5 m du fut, regard a +60 degres
  stand_eye    deux petites futaies de 20 arbres cote a cote, a 40 m, a hauteur d'oeil
  stand_air    les memes, de 22 m de haut

Journal : TREE_CARDS_VIEW <vue> gpu_ms_p50=..., TREE_CARDS_MESH <mesh> triangles=[LOD0,LOD1,LOD2],
TREE_CARDS_COMPLETE.

Variables : ANASTASIS_TREE_CARDS_OUT (dossier de sortie, obligatoire), ANASTASIS_TREE_CARDS_MESH_B
(chemin du mesh B, defaut le chene a cartes).
"""
import math
import os
import random
import time

import unreal

OUT = os.environ['ANASTASIS_TREE_CARDS_OUT']
LEVEL = '/Game/Anastasis/Maps/Lvl_AnastasisSlice'
MESH_A = '/Game/Anastasis/Vegetation/SM_Tree_HolmOak_01'
MESH_B = os.environ.get('ANASTASIS_TREE_CARDS_MESH_B', '/Game/Anastasis/Vegetation/Cards/SM_Tree_HolmOak_Card_01')
STAGE = unreal.Vector(-14000.0, 0.0, 0.0)
HEIGHT = 1100.0  # 11 m : la mediane du chene vert (registre)
V = unreal.Vector

eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
os.makedirs(OUT, exist_ok=True)


def log(msg):
    unreal.log('TREE_CARDS ' + str(msg))


les.load_level(LEVEL)
world = ues.get_editor_world()
SPAWNED = []


def cmd(c):
    unreal.SystemLibrary.execute_console_command(world, c)


def spawn_mesh(mesh, location, scale, label, yaw=0.0):
    actor = eas.spawn_actor_from_class(unreal.StaticMeshActor, location, unreal.Rotator(0, 0, yaw))
    SPAWNED.append(actor)
    actor.set_actor_label(label)
    comp = actor.static_mesh_component
    comp.set_static_mesh(mesh)
    comp.set_mobility(unreal.ComponentMobility.MOVABLE)
    actor.set_actor_scale3d(scale)
    return actor


mesh_a = unreal.EditorAssetLibrary.load_asset(MESH_A)
mesh_b = unreal.EditorAssetLibrary.load_asset(MESH_B)
if mesh_a is None or mesh_b is None:
    raise RuntimeError('mesh introuvable : A=%s B=%s' % (mesh_a, mesh_b))
for path, mesh in ((MESH_A, mesh_a), (MESH_B, mesh_b)):
    log('MESH %s triangles=%s' % (path, [mesh.get_num_triangles(i) for i in range(mesh.get_num_lods())]))

# L'anneau d'horizon couvre le point de scene : masque pour cette capture, comme capture-tree-lineup.py.
for embodiment in unreal.GameplayStatics.get_all_actors_of_class(
        world, unreal.load_class(None, '/Script/Anastasis_UnrealV2.AnastasisWorldEmbodiment')):
    for comp in embodiment.get_components_by_class(unreal.ProceduralMeshComponent):
        if comp.get_name() == 'HorizonTerrain':
            comp.set_visibility(False)
            comp.set_cast_shadow(False)
timings_actor = None
found = unreal.GameplayStatics.get_all_actors_of_class(
    world, unreal.load_class(None, '/Script/Anastasis_UnrealV2.AnastasisWorldEmbodiment'))
if len(found) > 0:
    timings_actor = found[0]

# Sol neutre, vaste : on juge le feuillage, pas le biome.
ground = spawn_mesh(unreal.EditorAssetLibrary.load_asset('/Engine/BasicShapes/Plane.Plane'),
                    unreal.Vector(STAGE.x, STAGE.y + 6000.0, 0.0), unreal.Vector(300.0, 300.0, 1.0), 'TreeCards_Ground')
try:
    grey = unreal.EditorAssetLibrary.load_asset('/Engine/BasicShapes/BasicShapeMaterial')
    mid = unreal.MaterialLibrary.create_dynamic_material_instance(world, grey)
    mid.set_vector_parameter_value('Color', unreal.LinearColor(0.085, 0.100, 0.040, 1.0))  # prairie sombre
    ground.static_mesh_component.set_material(0, mid)
except Exception as exc:  # noqa: BLE001
    log('WARN sol neutre non applique : %s' % exc)

# Direction du soleil : la lumiere voyage le long du vecteur avant de la DirectionalLight.
sun_dir = V(0.5, 0.5, -0.7)
for light in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.DirectionalLight):
    sun_dir = light.get_actor_rotation().get_forward_vector()
    break
fh = V(sun_dir.x, sun_dir.y, 0.0)
length = math.hypot(fh.x, fh.y) or 1.0
fh = V(fh.x / length, fh.y / length, 0.0)
log('SUN forward=(%.2f,%.2f,%.2f) horizontal=(%.2f,%.2f)' % (sun_dir.x, sun_dir.y, sun_dir.z, fh.x, fh.y))


def right_of(d):
    return V(-d.y, d.x, 0.0)


centre = V(STAGE.x, STAGE.y, 0.0)
rf = right_of(fh)
scale = V(HEIGHT / 100.0, HEIGHT / 100.0, HEIGHT / 100.0)
pos_a = V(centre.x - rf.x * 450.0, centre.y - rf.y * 450.0, HEIGHT * 0.5)
pos_b = V(centre.x + rf.x * 450.0, centre.y + rf.y * 450.0, HEIGHT * 0.5)
spawn_mesh(mesh_a, pos_a, scale, 'TreeCards_A', 20.0)
spawn_mesh(mesh_b, pos_b, scale, 'TreeCards_B', 20.0)

# Deux futaies de 20 arbres, memes positions et memes tailles pour A et B.
rng = random.Random(20261002)
stand_origin = V(STAGE.x, STAGE.y + 12000.0, 0.0)
for stand, mesh, dx in (('A', mesh_a, -3000.0), ('B', mesh_b, 3000.0)):
    rng_s = random.Random(20261002)
    for i in range(5):
        for j in range(4):
            h = HEIGHT * rng_s.uniform(0.75, 1.30)
            jitter_x, jitter_y = rng_s.uniform(-200, 200), rng_s.uniform(-200, 200)
            loc = V(stand_origin.x + dx + (i - 2) * 800.0 + jitter_x, stand_origin.y + (j - 1.5) * 800.0 + jitter_y, h * 0.5)
            spawn_mesh(mesh, loc, V(h / 100.0, h / 100.0, h / 100.0), 'TreeCards_Stand%s_%d_%d' % (stand, i, j),
                       rng_s.uniform(0, 360))


def level_cam(loc, d, pitch):
    yaw = math.degrees(math.atan2(d.y, d.x))
    return loc, unreal.Rotator(0.0, pitch, yaw)


EYE = 170.0
views = []
back = V(-fh.x, -fh.y, 0.0)
for name, dist, direction in (('near_front', 900.0, fh), ('near_back', 900.0, back), ('mid_front', 2500.0, fh), ('far_front', 7000.0, fh)):
    # Camera en amont de la paire, regard le long de `direction`.
    loc = V(centre.x - direction.x * dist, centre.y - direction.y * dist, EYE)
    views.append((name,) + level_cam(loc, direction, 4.0 if dist > 2000 else 12.0))
for label, p in (('under_A', pos_a), ('under_B', pos_b)):
    loc = V(p.x - fh.x * 250.0, p.y - fh.y * 250.0, EYE)
    views.append((label,) + level_cam(loc, fh, 62.0))
views.append(('stand_eye',) + level_cam(V(stand_origin.x, stand_origin.y - 4500.0, EYE), V(0, 1, 0), 6.0))
views.append(('stand_air',) + level_cam(V(stand_origin.x, stand_origin.y - 3800.0, 2200.0), V(0, 1, 0), -22.0))

for name, loc, rot in views:
    log('VIEW %s loc=(%.0f,%.0f,%.0f) pitch=%.1f yaw=%.1f' % (name, loc.x, loc.y, loc.z, rot.pitch, rot.yaw))
log('ORDER near_front/mid_front/far_front : A (lames) a gauche, B (cartes) a droite ; near_back : inverse')

for c in ('ShowFlag.Sprites 0', 'ShowFlag.Grid 0', 'viewmode lit', 'anastasis.Sky.Hour 11'):
    cmd(c)

queue = list(views)
phase, mark, shot, first = 'aim', time.monotonic(), None, True
gpu = []
handle = None


def redraw():
    try:
        les.editor_invalidate_viewports()
    except Exception:
        pass


def finish(msg, error=False):
    global phase
    phase = 'done'
    (unreal.log_error if error else unreal.log)(msg)
    # Demonter la scene : un niveau sale ecrit un PackageRestoreData qui bloque l'editeur suivant.
    for actor in SPAWNED:
        try:
            eas.destroy_actor(actor)
        except Exception:
            pass
    unreal.SystemLibrary.quit_editor()


def tick(_dt):
    global phase, mark, shot, first, gpu
    if phase == 'done':
        return
    redraw()
    elapsed = time.monotonic() - mark
    if phase == 'aim':
        if not queue:
            finish('TREE_CARDS_COMPLETE views=%d' % len(views))
            return
        name, loc, rot = queue[0]
        ues.set_level_viewport_camera_info(loc, rot)
        if elapsed > 2.5 and timings_actor is not None:
            try:
                gpu.append(timings_actor.call_method('GetFrameTimingsMs').z)
            except Exception:
                pass
        if elapsed > (16.0 if first else 6.0):
            shot = os.path.join(OUT, '%s.png' % name).replace('\\', '/')
            if os.path.exists(shot):
                os.remove(shot)
            cmd('HighResShot 1600x900 filename="%s"' % shot)
            phase, mark = 'wait', time.monotonic()
    elif phase == 'wait':
        if not os.path.isfile(shot):
            if elapsed > 40.0:
                finish('TREE_CARDS_SHOT_MISSING %s' % shot, True)
            return
        if elapsed > 1.5:
            g = sorted(gpu)
            unreal.log('TREE_CARDS_VIEW %s bytes=%d gpu_ms_p50=%.2f' % (
                os.path.basename(shot), os.path.getsize(shot), g[len(g) // 2] if g else -1.0))
            gpu = []
            queue.pop(0)
            first, phase, mark = False, 'aim', time.monotonic()


handle = unreal.register_slate_post_tick_callback(tick)
