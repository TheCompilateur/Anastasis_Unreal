"""
ECOTONE_FORGE_001 -- trois compositions a hauteur d'homme.

Pas le monde 96x96 : la capture in-world a ete tuee par la memoire de l'hote
(plusieurs editeurs agents en parallele). Ces trois planches posent les memes
meshes, aux tailles reelles, dans les trois recits que le dressing place :

  forest  souche + tronc + buisson + touffe + semis
  shore   roseaux + berge + bois flotte + cailloux
  rock    bloc semi-enterre + amas + herbe dans les interstices + racines

    tools\\unreal\\capture-ecotone.ps1 -Boards
"""
import os
import shutil
import time
import unreal

OUT = os.environ.get("ANASTASIS_ECOTONE_OUT", "")
LEVEL = "/Game/Anastasis/Maps/Lvl_AnastasisSlice"
MESH_DIR = "/Game/Anastasis/Ecotone/"
STAGE = unreal.Vector(-14000.0, 0.0, 0.0)

eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
SPAWNED = []
SHOT_DIR = os.path.join(unreal.Paths.project_saved_dir(), "Screenshots")


def log(msg):
    unreal.log("ECOTONE_CAPTURE " + str(msg))


def spawn_mesh(path, location, yaw, label):
    mesh = unreal.EditorAssetLibrary.load_asset(MESH_DIR + path)
    if mesh is None:
        unreal.log_error("mesh manquant " + path)
        return None
    actor = eas.spawn_actor_from_class(
        unreal.StaticMeshActor, location, unreal.Rotator(roll=0.0, pitch=0.0, yaw=yaw))
    SPAWNED.append(actor)
    actor.set_actor_label(label)
    actor.static_mesh_component.set_static_mesh(mesh)
    actor.static_mesh_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    box = mesh.get_bounding_box()
    loc = actor.get_actor_location()
    loc.z = -box.min.z
    actor.set_actor_location(loc, False, False)
    return actor


def newest_png(after):
    best, best_t = None, after
    for root, _dirs, files in os.walk(SHOT_DIR):
        for f in files:
            if f.lower().endswith(".png"):
                full = os.path.join(root, f)
                t = os.path.getmtime(full)
                if t > best_t:
                    best, best_t = full, t
    return best


def redraw():
    try:
        les.editor_invalidate_viewports()
    except Exception:
        pass


if not OUT:
    raise RuntimeError("ANASTASIS_ECOTONE_OUT manquant")
os.makedirs(OUT, exist_ok=True)
assert les.load_level(LEVEL)
world = ues.get_editor_world()
unreal.SystemLibrary.execute_console_command(world, "viewmode lit")
unreal.SystemLibrary.execute_console_command(world, "ShowFlag.Sprites 0")
unreal.SystemLibrary.execute_console_command(world, "ShowFlag.Grid 0")
unreal.SystemLibrary.execute_console_command(world, "anastasis.Atmosphere 0")
for fog in list(unreal.GameplayStatics.get_all_actors_of_class(world, unreal.ExponentialHeightFog)):
    eas.destroy_actor(fog)

ground = eas.spawn_actor_from_class(
    unreal.StaticMeshActor, STAGE, unreal.Rotator(0, 0, 0))
SPAWNED.append(ground)
ground.static_mesh_component.set_static_mesh(
    unreal.EditorAssetLibrary.load_asset("/Engine/BasicShapes/Plane.Plane"))
ground.set_actor_scale3d(unreal.Vector(40.0, 40.0, 1.0))
try:
    grey = unreal.EditorAssetLibrary.load_asset("/Engine/BasicShapes/BasicShapeMaterial")
    mid = unreal.MaterialLibrary.create_dynamic_material_instance(world, grey)
    mid.set_vector_parameter_value("Color", unreal.LinearColor(0.22, 0.24, 0.18, 1.0))
    ground.static_mesh_component.set_material(0, mid)
except Exception:
    pass

# Three pads along X. Each is a causal cluster, not a catalogue.
PADS = {
    "context_forest": (unreal.Vector(STAGE.x - 500.0, STAGE.y, 0.0), [
        ("SM_Ecotone_FallenLog_01", 0.0, 40.0, 25.0),
        ("SM_Ecotone_Stump_01", -70.0, -20.0, 40.0),
        ("SM_Ecotone_Bush_Low_01", 55.0, -35.0, 10.0),
        ("SM_Ecotone_GrassTuft_01", -40.0, 50.0, 80.0),
        ("SM_Ecotone_Sapling_01", 80.0, 30.0, -20.0),
        ("SM_Ecotone_BranchPile_01", 20.0, 55.0, 120.0),
    ]),
    "context_shore": (unreal.Vector(STAGE.x, STAGE.y, 0.0), [
        ("SM_Ecotone_Reed_01", -30.0, -20.0, 0.0),
        ("SM_Ecotone_Reed_01", 10.0, 15.0, 40.0),
        ("SM_Ecotone_ShoreTuft_01", 45.0, -10.0, 15.0),
        ("SM_Ecotone_Driftwood_01", -10.0, 50.0, 70.0),
        ("SM_Ecotone_RockCluster_01", 60.0, 35.0, -30.0),
    ]),
    "context_rock": (unreal.Vector(STAGE.x + 500.0, STAGE.y, 0.0), [
        ("SM_Ecotone_BuriedBlock_01", 0.0, 0.0, 10.0),
        ("SM_Ecotone_RockCluster_01", 50.0, -25.0, 40.0),
        ("SM_Ecotone_GrassTuft_01", 25.0, 30.0, 0.0),
        ("SM_Ecotone_ExposedRoots_01", -55.0, 10.0, 200.0),
        ("SM_Ecotone_GrassTuft_01", -20.0, -40.0, 90.0),
    ]),
}

for name, (origin, pieces) in PADS.items():
    for mesh, dx, dy, yaw in pieces:
        spawn_mesh(mesh, unreal.Vector(origin.x + dx, origin.y + dy, 0.0), yaw, name + "_" + mesh)

jobs = []
for name, (origin, _pieces) in PADS.items():
    cam = unreal.Vector(origin.x - 220.0, origin.y - 320.0, 165.0)
    look = unreal.Vector(origin.x, origin.y, 40.0)
    rot = unreal.MathLibrary.find_look_at_rotation(cam, look)
    jobs.append((name, cam, rot))

phase = 0
mark = time.monotonic()
requested = False
shot_stamp = 0.0
active = None
handle = None


def finish(msg, error=False):
    (unreal.log_error if error else unreal.log)(msg)
    unreal.unregister_slate_post_tick_callback(handle)
    for actor in SPAWNED:
        try:
            eas.destroy_actor(actor)
        except Exception:
            pass
    unreal.SystemLibrary.quit_editor()


def tick(dt):
    global phase, mark, requested, shot_stamp, active
    elapsed = time.monotonic() - mark
    if phase < len(jobs):
        name, loc, rot = jobs[phase]
        if not requested and elapsed > 4:
            ues.set_level_viewport_camera_info(loc, rot)
            redraw()
            requested = True
            mark = time.monotonic()
        elif requested and elapsed > 5 and active is None:
            dest = os.path.join(OUT, name + ".png").replace("\\", "/")
            unreal.SystemLibrary.execute_console_command(
                world, 'HighResShot 1600x900 filename="' + dest + '"')
            active = dest
            mark = time.monotonic()
            log("SHOT_REQUESTED " + name)
        elif requested and active is not None:
            redraw()
            if os.path.isfile(active):
                log("SHOT " + active)
                phase += 1
                requested = False
                active = None
                mark = time.monotonic()
            elif elapsed > 8 and elapsed < 35:
                if int(elapsed) % 7 == 0:
                    ues.set_level_viewport_camera_info(loc, rot)
                    redraw()
                    unreal.SystemLibrary.execute_console_command(
                        world, 'HighResShot 1600x900 filename="' + active.replace("\\", "/") + '"')
                    log("SHOT_RETRY " + active)
            elif elapsed >= 35:
                finish("ECOTONE_CAPTURE missing " + active, True)
    elif elapsed > 3:
        finish("ECOTONE_CAPTURE_COMPLETE shots=%d" % len(jobs))


handle = unreal.register_slate_post_tick_callback(tick)
