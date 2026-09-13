"""
ANASTASIS_ASSET_AGENT_001 -- Phase 3.

Builds one real StaticMesh asset (trunk cylinder + tapered canopy cone, merged
into a single mesh, single material slot) to replace the /Engine/BasicShapes/
Cone.Cone placeholder currently used for the Forest presentation archetype
(see Source/Anastasis_UnrealV2/WorldView/AnastasisPresentationResolver.h).

Deliberately re-centered on local (0,0,0) spanning [-50,+50] on Z -- the same
bounding convention AnastasisPresentation::EngineBasicShapeSize already
assumes for engine primitives -- so the existing per-tile transform math in
AnastasisWorldEmbodiment.cpp (base-lift = 0.5 * EngineBasicShapeSize * Scale)
keeps working unmodified. This script only produces the asset; it does not
touch any .h/.cpp file.

Signatures confirmed against this engine build (UE 5.8.2, CL 56702186) via
tools/unreal/introspect_geoscript.py -- do not assume Epic docs for other
versions match exactly.

Run headless:
  UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script="tools/unreal/create_tree_asset.py"
"""
import unreal

PACKAGE_PATH = "/Game/Anastasis/Vegetation"
ASSET_NAME = "SM_Tree_Generic_01"
ASSET_PATH = PACKAGE_PATH + "/" + ASSET_NAME

TRUNK_RADIUS = 9.0
TRUNK_BASE_Z = -50.0
TRUNK_TOP_Z = -8.0

CANOPY_BASE_RADIUS = 34.0
CANOPY_BASE_Z = -8.0
CANOPY_TOP_Z = 50.0


def log(msg):
    unreal.log("[create_tree_asset] " + str(msg))


def vec_add_scaled(a, b, scale):
    return unreal.Vector(a.x + b.x * scale, a.y + b.y * scale, a.z + b.z * scale)


def main():
    log("start")

    mesh = unreal.DynamicMesh()
    prim_opts = unreal.GeometryScriptPrimitiveOptions()

    trunk_height = TRUNK_TOP_Z - TRUNK_BASE_Z
    trunk_xf = unreal.Transform(location=unreal.Vector(0.0, 0.0, TRUNK_BASE_Z))
    mesh = unreal.GeometryScript_Primitives.append_cylinder(
        mesh, prim_opts, trunk_xf,
        radius=TRUNK_RADIUS, height=trunk_height,
        radial_steps=8, height_steps=1, capped=True,
        origin=unreal.GeometryScriptPrimitiveOriginMode.BASE,
    )
    log("trunk appended, height=%s" % trunk_height)

    canopy_height = CANOPY_TOP_Z - CANOPY_BASE_Z
    canopy_xf = unreal.Transform(location=unreal.Vector(0.0, 0.0, CANOPY_BASE_Z))
    mesh = unreal.GeometryScript_Primitives.append_cone(
        mesh, prim_opts, canopy_xf,
        base_radius=CANOPY_BASE_RADIUS, top_radius=0.5, height=canopy_height,
        radial_steps=10, height_steps=1, capped=True,
        origin=unreal.GeometryScriptPrimitiveOriginMode.BASE,
    )
    log("canopy appended, height=%s" % canopy_height)

    bbox = unreal.GeometryScript_MeshQueries.get_mesh_bounding_box(mesh)
    center = unreal.Vector((bbox.min.x + bbox.max.x) * 0.5, (bbox.min.y + bbox.max.y) * 0.5, (bbox.min.z + bbox.max.z) * 0.5)
    log("pre-recenter bounds min=%s max=%s center=%s" % (bbox.min, bbox.max, center))
    mesh = unreal.GeometryScript_MeshTransforms.translate_mesh(mesh, unreal.Vector(-center.x, -center.y, -center.z))
    bbox2 = unreal.GeometryScript_MeshQueries.get_mesh_bounding_box(mesh)
    log("post-recenter bounds min=%s max=%s" % (bbox2.min, bbox2.max))

    if unreal.EditorAssetLibrary.does_asset_exist(ASSET_PATH):
        log("asset already exists, deleting stale copy first")
        unreal.EditorAssetLibrary.delete_asset(ASSET_PATH)

    new_asset, outcome = unreal.GeometryScript_NewAssetUtils.create_new_static_mesh_asset_from_mesh(
        mesh, ASSET_PATH, unreal.GeometryScriptCreateNewStaticMeshAssetOptions()
    )
    log("create_new_static_mesh_asset_from_mesh outcome=%s asset=%s" % (outcome, new_asset))
    if new_asset is None:
        log("FAIL: no asset created")
        return False

    try:
        unreal.EditorStaticMeshLibrary.add_simple_collisions(new_asset, unreal.ScriptingCollisionShapeType.NDOP10_X)
        log("simple collision added")
    except Exception as exc:  # noqa: BLE001 -- best-effort, not required for the proof
        log("WARN: could not add simple collision: %s" % exc)

    unreal.EditorAssetLibrary.save_asset(new_asset.get_path_name())
    log("saved %s" % new_asset.get_path_name())

    final_bounds = new_asset.get_bounding_box()
    log("final static mesh bounds min=%s max=%s" % (final_bounds.min, final_bounds.max))
    log("done")
    return True


main()
