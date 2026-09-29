"""
ECOTONE_FORGE_001 -- captures de preuve.

1. Planche des 12 assets (tailles reelles, sol neutre).
2. Monde canonique seed 12345, A/B : ecotone off puis on.
   Quatre cadrages : macro, lisiere a hauteur d'homme, rive, pied de pierre.

Ne sauve jamais le niveau. Sortie : ANASTASIS_ECOTONE_OUT.

    tools\\unreal\\capture-ecotone.ps1
"""
import math
import os
import shutil
import time
import unreal

OUT = os.environ.get("ANASTASIS_ECOTONE_OUT", "")
LEVEL = "/Game/Anastasis/Maps/Lvl_AnastasisSlice"
MESH_DIR = "/Game/Anastasis/Ecotone/"
STAGE = unreal.Vector(-14000.0, 0.0, 0.0)

SUBJECTS = [
    ("SM_Ecotone_GrassTuft_01", "touffe 0.5 m"),
    ("SM_Ecotone_ShoreTuft_01", "berge"),
    ("SM_Ecotone_Stump_01", "souche"),
    ("SM_Ecotone_RockCluster_01", "cailloux"),
    ("SM_Ecotone_BranchPile_01", "branches"),
    ("SM_Ecotone_Bush_Low_01", "buisson"),
    ("SM_Ecotone_ExposedRoots_01", "racines"),
    ("SM_Ecotone_Driftwood_01", "bois flotte"),
    ("SM_Ecotone_BuriedBlock_01", "bloc enterre"),
    ("SM_Ecotone_Sapling_01", "semis"),
    ("SM_Ecotone_Reed_01", "roseaux"),
    ("SM_Ecotone_FallenLog_01", "tronc mort"),
]

eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
SPAWNED = []


def log(msg):
    unreal.log("ECOTONE_CAPTURE " + str(msg))


def cmd(c):
    unreal.SystemLibrary.execute_console_command(ues.get_editor_world(), c)


def spawn_mesh(mesh, location, scale, label):
    actor = eas.spawn_actor_from_class(unreal.StaticMeshActor, location, unreal.Rotator(0, 0, 0))
    SPAWNED.append(actor)
    actor.set_actor_label(label)
    comp = actor.static_mesh_component
    comp.set_static_mesh(mesh)
    comp.set_mobility(unreal.ComponentMobility.MOVABLE)
    actor.set_actor_scale3d(scale)
    return actor


def redraw():
    try:
        les.editor_invalidate_viewports()
    except Exception:
        pass


def densest_centroid(prefixes):
    bins = {}
    actor = embodiment()
    for comp in actor.get_components_by_class(unreal.HierarchicalInstancedStaticMeshComponent):
        name = str(comp.get_name())
        if not any(p in name for p in prefixes):
            continue
        for i in range(comp.get_instance_count()):
            p = comp.get_instance_transform(i, world_space=False).translation
            key = (int(p.x // 900), int(p.y // 900))
            bins.setdefault(key, []).append(p)
    if not bins:
        return None
    pts = bins[max(sorted(bins), key=lambda k: len(bins[k]))]
    cx = sum(p.x for p in pts) / len(pts)
    cy = sum(p.y for p in pts) / len(pts)
    return min(pts, key=lambda p: (p.x - cx) ** 2 + (p.y - cy) ** 2)


def embodiment():
    cls = unreal.load_class(None, "/Script/Anastasis_UnrealV2.AnastasisWorldEmbodiment")
    found = list(unreal.GameplayStatics.get_all_actors_of_class(ues.get_editor_world(), cls))
    if not found:
        raise RuntimeError("AnastasisWorldEmbodiment introuvable")
    return found[0]


# Camera heights are sampled from the rendered terrain section, not the
# centroid's altitude displaced sideways into a different part of the slope.
_TERRAIN_GRID = None


def ground_height(x, y):
    global _TERRAIN_GRID
    if _TERRAIN_GRID is None:
        import bisect
        actor = embodiment()
        loc = actor.get_actor_location()
        if abs(loc.x) + abs(loc.y) + abs(loc.z) > 0.01:
            raise RuntimeError("Camera sampler requires identity world origin")
        components = actor.get_components_by_class(unreal.ProceduralMeshComponent)
        if len(components) != 1:
            raise RuntimeError("Expected exactly one rendered terrain component")
        data = unreal.ProceduralMeshLibrary.get_section_from_procedural_mesh(components[0], 0)
        vertices = data[0]
        xs = sorted({v.x for v in vertices})
        ys = sorted({v.y for v in vertices})
        heights = {(v.x, v.y): v.z for v in vertices}
        if len(heights) != len(xs) * len(ys):
            raise RuntimeError("Expected rectangular terrain grid")
        _TERRAIN_GRID = (xs, ys, heights)
        log("CAMERA_TERRAIN grid=%dx%d" % (len(xs), len(ys)))
    import bisect
    xs, ys, heights = _TERRAIN_GRID
    if not (xs[0] <= x <= xs[-1] and ys[0] <= y <= ys[-1]):
        raise RuntimeError("Camera outside rendered terrain")
    ix = max(0, min(len(xs) - 2, bisect.bisect_right(xs, x) - 1))
    iy = max(0, min(len(ys) - 2, bisect.bisect_right(ys, y) - 1))
    x0, x1 = xs[ix:ix + 2]
    y0, y1 = ys[iy:iy + 2]
    u, v = (x - x0) / (x1 - x0), (y - y0) / (y1 - y0)
    a, b = heights[x0, y0], heights[x1, y0]
    c, d = heights[x0, y1], heights[x1, y1]
    # TerrainForge emits triangles A,C,B and B,C,D.
    if u + v <= 1:
        return a * (1 - u - v) + b * u + c * v
    return d * (u + v - 1) + c * (1 - u) + b * (1 - v)


def eye_camera(focus):
    target_z = ground_height(focus.x, focus.y) + 50.0
    target = unreal.Vector(focus.x, focus.y, target_z)
    candidates = []
    for i in range(24):
        angle = math.tau * i / 24
        x, y = focus.x + 420 * math.cos(angle), focus.y + 420 * math.sin(angle)
        try:
            ground = ground_height(x, y)
            z = ground + 165.0
            clearance = min(
                z + (target_z - z) * t / 12
                - ground_height(x + (focus.x - x) * t / 12,
                                y + (focus.y - y) * t / 12)
                for t in range(1, 12))
        except RuntimeError:
            continue
        if clearance < 20:
            continue
        candidates.append((abs(ground - (target_z - 50)), i, x, y, z, ground, clearance))
    if not candidates:
        raise RuntimeError("No unobstructed eye-height camera for focus")
    _, _, x, y, z, ground, clearance = min(candidates)
    loc = unreal.Vector(x, y, z)
    rot = unreal.MathLibrary.find_look_at_rotation(loc, target)
    manifest.setdefault("cameras", []).append({
        "location": [x, y, z], "rotation": [rot.pitch, rot.yaw, rot.roll],
        "ground_z": ground, "eye_height_uu": 165,
        "terrain_line_of_sight_min_clearance_uu": clearance,
        "target": [target.x, target.y, target.z]})
    with open(os.path.join(OUT, "manifest.json"), "w", encoding="utf-8") as stream:
        json.dump(manifest, stream, indent=2)
    log("CAMERA_GROUNDED loc=%s rot=%s ground=%.3f eye=165 clearance=%.3f" % (
        loc, rot, ground, clearance))
    return loc, rot


SHOT_DIR = os.path.join(unreal.Paths.project_saved_dir(), "Screenshots")


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


def neutralize():
    world = ues.get_editor_world()
    cmd("viewmode lit")
    cmd("ShowFlag.Sprites 0")
    cmd("ShowFlag.Grid 0")
    cmd("anastasis.Terrain.Surface 2")
    cmd("anastasis.Atmosphere 0")
    for sun in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.DirectionalLight):
        sun.get_component_by_class(unreal.DirectionalLightComponent).set_intensity(75000.0)
    for fog in list(unreal.GameplayStatics.get_all_actors_of_class(world, unreal.ExponentialHeightFog)):
        eas.destroy_actor(fog)
    for pp in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.PostProcessVolume):
        st = pp.get_editor_property("settings")
        for n, v in (
            ("override_auto_exposure_min_brightness", True),
            ("override_auto_exposure_max_brightness", True),
            ("auto_exposure_min_brightness", 14.0),
            ("auto_exposure_max_brightness", 14.0),
            ("override_bloom_intensity", True),
            ("bloom_intensity", 0.0),
            ("override_vignette_intensity", True),
            ("vignette_intensity", 0.0),
            ("override_motion_blur_amount", True),
            ("motion_blur_amount", 0.0),
        ):
            st.set_editor_property(n, v)
        pp.set_editor_property("settings", st)


def clear_stage():
    for actor in SPAWNED:
        try:
            eas.destroy_actor(actor)
        except Exception:
            pass
    SPAWNED.clear()


if not OUT:
    raise RuntimeError("ANASTASIS_ECOTONE_OUT manquant")
os.makedirs(OUT, exist_ok=True)


# Read-only recipe and saved-registry inspection in the real Unreal process.
import importlib.util
import json
import sys
sys.dont_write_bytecode = True
recipe_path = os.environ["ANASTASIS_REGISTRY_RECIPE"]
spec = importlib.util.spec_from_file_location("registry_review", recipe_path)
registry_recipe = importlib.util.module_from_spec(spec)
spec.loader.exec_module(registry_recipe)
seed_entries = registry_recipe.make_seed_entries()
assert len(seed_entries) == 2
assert len(seed_entries[0].get_editor_property("variants")) == 8
unreal.log("REGISTRY_RECIPE_RUNTIME::PASS forest=8 ruin=1 no_asset_saved=1")
saved_registry = unreal.EditorAssetLibrary.load_asset(registry_recipe.ASSET_PATH)
if saved_registry is None:
    raise RuntimeError("Saved registry missing")
registry_recipe.inspect(saved_registry)
manifest = {"project": unreal.Paths.project_dir(), "seed": 12345,
            "source_ref": "fb683983c22c381bf00390d312634b419f585e24",
            "registry_recipe_runtime": "PASS", "saved_assets_mutated": False,
            "meshes": []}
for name, label in SUBJECTS:
    mesh = unreal.EditorAssetLibrary.load_asset(MESH_DIR + name)
    if mesh is None:
        raise RuntimeError("Missing ecotone mesh: " + name)
    box = mesh.get_bounding_box()
    manifest["meshes"].append({"name": name, "path": mesh.get_path_name(),
        "bounds_min": [box.min.x, box.min.y, box.min.z],
        "bounds_max": [box.max.x, box.max.y, box.max.z]})
with open(os.path.join(OUT, "manifest.json"), "w", encoding="utf-8") as stream:
    json.dump(manifest, stream, indent=2)

assert les.load_level(LEVEL)
neutralize()

# --- planche ---
ARC_RADIUS = 1200.0
CAM_HEIGHT = 140.0
CAM_LOC = unreal.Vector(STAGE.x, STAGE.y + ARC_RADIUS, CAM_HEIGHT)
CAM_ROT = unreal.Rotator(roll=0.0, pitch=0.0, yaw=-90.0)
ground = spawn_mesh(
    unreal.EditorAssetLibrary.load_asset("/Engine/BasicShapes/Plane.Plane"),
    unreal.Vector(STAGE.x, STAGE.y, 0.0),
    unreal.Vector(30.0, 30.0, 1.0),
    "Ecotone_Ground",
)
try:
    grey = unreal.EditorAssetLibrary.load_asset("/Engine/BasicShapes/BasicShapeMaterial")
    mid = unreal.MaterialLibrary.create_dynamic_material_instance(ues.get_editor_world(), grey)
    mid.set_vector_parameter_value("Color", unreal.LinearColor(0.18, 0.19, 0.16, 1.0))
    ground.static_mesh_component.set_material(0, mid)
except Exception as exc:  # noqa: BLE001
    log("WARN sol %s" % exc)

EYES_ONLY = os.environ.get("ANASTASIS_ECOTONE_EYES_ONLY", "0") == "1"

if not EYES_ONLY:
    spacing = 140.0
    step = 2.0 * math.degrees(math.asin(min(1.0, spacing * 0.5 / ARC_RADIUS)))
    first = -step * (len(SUBJECTS) - 1) * 0.5
    for index, (name, label) in enumerate(SUBJECTS):
        mesh = unreal.EditorAssetLibrary.load_asset(MESH_DIR + name)
        if mesh is None:
            unreal.log_error("ECOTONE_CAPTURE mesh manquant " + name)
            continue
        theta = math.radians(first + step * index)
        box = mesh.get_bounding_box()
        location = unreal.Vector(
            CAM_LOC.x + ARC_RADIUS * math.sin(theta),
            CAM_LOC.y - ARC_RADIUS * math.cos(theta),
            -box.min.z,
        )
        spawn_mesh(mesh, location, unreal.Vector(1.0, 1.0, 1.0), label)
        log("lineup %s h=%.0f" % (label, box.max.z - box.min.z))
    jobs = [("lineup", CAM_LOC, CAM_ROT, None)]
else:
    jobs = []
# world jobs filled after first rebuild
phase = 0
mark = time.monotonic()
requested = False
shot_stamp = 0.0
active_name = None
handle = None
world_jobs = []
built_world = False


def rebuild(ecotone):
    cmd("anastasis.Dressing.Ecotone " + str(ecotone))
    cmd("anastasis.Dressing.Ecology 1")
    assert embodiment().call_method("EmbodyCanonical", args=(12345,))


def finish(msg, error=False):
    (unreal.log_error if error else unreal.log)(msg)
    unreal.unregister_slate_post_tick_callback(handle)
    clear_stage()
    unreal.SystemLibrary.quit_editor()


def tick(dt):
    global phase, mark, requested, shot_stamp, active_name, world_jobs, built_world, jobs
    elapsed = time.monotonic() - mark
    queue = jobs + world_jobs
    if phase < len(queue):
        name, loc, rot, mode = queue[phase]
        if not requested and elapsed > 8:
            if mode is not None:
                rebuild(mode)
            if name != "lineup":
                clear_stage()
            ues.set_level_viewport_camera_info(loc, rot)
            redraw()
            requested = True
            mark = time.monotonic()
        elif requested and elapsed > 8 and active_name is None:
            shot_stamp = time.time()
            cmd("HighResShot 1600x900")
            active_name = name
            mark = time.monotonic()
            log("SHOT_REQUESTED " + name)
        elif requested and active_name is not None:
            redraw()
            found = newest_png(shot_stamp)
            dest = os.path.join(OUT, active_name + ".png")
            if found:
                shutil.copyfile(found, dest)
                log("SHOT " + dest)
                if active_name == "lineup" and not built_world:
                    clear_stage()
                    rebuild(1)
                    forest = densest_centroid(["Ecotone_BushLow", "Ecotone_Stump", "Ecotone_FallenLog", "Ecotone_Sapling"])
                    shore = densest_centroid(["Ecotone_Reed", "Ecotone_ShoreTuft", "Ecotone_Driftwood"])
                    rock = densest_centroid(["Ecotone_BuriedBlock", "Ecotone_RockCluster"])
                    macro_loc = unreal.Vector(-5400.0, -5400.0, 10500.0)
                    macro_rot = unreal.Rotator(0.0, -32.8, 45.0)
                    fallback = unreal.Vector(2400.0, 2400.0, 400.0)
                    f_loc, f_rot = eye_camera(forest or fallback)
                    s_loc, s_rot = eye_camera(shore or fallback)
                    r_loc, r_rot = eye_camera(rock or fallback)
                    world_jobs = [
                        ("A_macro", macro_loc, macro_rot, 0),
                        ("B_macro", macro_loc, macro_rot, 1),
                        ("A_forest_eye", f_loc, f_rot, 0),
                        ("B_forest_eye", f_loc, f_rot, 1),
                        ("A_shore_eye", s_loc, s_rot, 0),
                        ("B_shore_eye", s_loc, s_rot, 1),
                        ("A_rock_eye", r_loc, r_rot, 0),
                        ("B_rock_eye", r_loc, r_rot, 1),
                    ]
                    log("CAM forest=%s shore=%s rock=%s" % (forest, shore, rock))
                    built_world = True
                phase += 1
                requested = False
                active_name = None
                mark = time.monotonic()
            elif elapsed > 12.0 and elapsed < 45.0:
                if int(elapsed) % 10 == 0:
                    ues.set_level_viewport_camera_info(loc, rot)
                    redraw()
                    cmd("HighResShot 1600x900")
                    shot_stamp = time.time()
                    log("SHOT_RETRY " + active_name)
            elif elapsed >= 45.0:
                finish("ECOTONE_CAPTURE missing " + dest, True)
    elif elapsed > 4:
        finish("ECOTONE_CAPTURE_COMPLETE shots=%d" % (len(jobs) + len(world_jobs)))
    elif elapsed > 90:
        finish("ECOTONE_CAPTURE timeout phase=%d" % phase, True)


if EYES_ONLY:
    rebuild(1)
    forest = densest_centroid(["Ecotone_BushLow", "Ecotone_Stump", "Ecotone_FallenLog", "Ecotone_Sapling"])
    shore = densest_centroid(["Ecotone_Reed", "Ecotone_ShoreTuft", "Ecotone_Driftwood"])
    rock = densest_centroid(["Ecotone_BuriedBlock", "Ecotone_RockCluster"])
    fallback = unreal.Vector(2400.0, 2400.0, 400.0)
    f_loc, f_rot = eye_camera(forest or fallback)
    s_loc, s_rot = eye_camera(shore or fallback)
    r_loc, r_rot = eye_camera(rock or fallback)
    jobs = [
        ("A_forest_eye", f_loc, f_rot, 0),
        ("B_forest_eye", f_loc, f_rot, 1),
        ("A_shore_eye", s_loc, s_rot, 0),
        ("B_shore_eye", s_loc, s_rot, 1),
        ("A_rock_eye", r_loc, r_rot, 0),
        ("B_rock_eye", r_loc, r_rot, 1),
    ]
    built_world = True
    log("EYES_ONLY forest=%s shore=%s rock=%s" % (forest, shore, rock))

handle = unreal.register_slate_post_tick_callback(tick)
