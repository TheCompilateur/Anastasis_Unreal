"""
ANASTASIS_ASSET_AGENT_002 -- Phase 3.

Builds one real StaticMesh asset (two broken wall-fragment boxes of uneven
height + three scattered tumbled stone blocks, merged into a single mesh,
single material slot) to replace the /Engine/BasicShapes/Cylinder.Cylinder
placeholder currently used for the Ruin archetype.

Architecture note (this worktree is based on main @ 30120fc, not the older
e3f452d): the mesh-path-per-archetype is no longer a C++ constant. It is a
UAnastasisPresentationRegistry data asset at
/Game/Anastasis/Presentation/DA_AnastasisPresentation (see
Source/Anastasis_UnrealV2/WorldView/AnastasisPresentationRegistry.h). This
script only builds and saves the mesh; wiring it into the Ruin entry's
Variants[0].Mesh happens in a separate step (set_ruin_variant.py), matching
"an asset pass edits meshes... there -- no resolver change, no recompile".

Canon grounding (docs/visual/reference/pontique-grammaire-architecturale-
batiment.png "SOUBASSEMENT" panel: "Pierre locale, assise seche, drainage,
contact terrain"; pontique-usure-materiaux-vieillissement.png "PIERRE" row:
seche / humide / moussue) supports dry-stacked irregular local fieldstone,
not a specific ruin composition -- no canon board depicts a ruin directly,
so this is a conservative silhouette (broken wall fragment + rubble), not a
literal reproduction of any plate.

Deliberately spans the full [-50,+50] Z range after recentering -- the same
bounding convention AnastasisPresentation::EngineBasicShapeSize already
assumes -- via the taller wall-fragment box reaching to +50, so the existing
per-tile transform math in AnastasisPresentationResolver.cpp (base-lift =
0.5 * EngineBasicShapeSize * Scale) keeps working unmodified, exactly like
SM_Tree_Generic_01 in ASSET_AGENT_001. This script only produces the asset;
it does not touch any .h/.cpp file.

Run headless:
  UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script="tools/unreal/create_ruin_asset.py"
"""
import unreal

PACKAGE_PATH = "/Game/Anastasis/Architecture"
ASSET_NAME = "SM_Ruin_Generic_01"
ASSET_PATH = PACKAGE_PATH + "/" + ASSET_NAME

BASE_Z = -50.0

# (dim_x, dim_y, dim_z, loc_x, loc_y, yaw_deg) -- origin=BASE, so loc_z is always BASE_Z.
BLOCKS = [
    (22.0, 9.0, 100.0, -20.0, 0.0, 4.0),    # wall fragment A -- tallest, spans to +50
    (20.0, 9.0, 68.0, 1.0, 1.0, -3.0),      # wall fragment B -- broken/uneven crest next to A
    (26.0, 22.0, 16.0, 15.0, -10.0, 25.0),  # tumbled block 1
    (20.0, 18.0, 13.0, 8.0, 15.0, -40.0),   # tumbled block 2
    (16.0, 16.0, 11.0, -15.0, -18.0, 60.0), # tumbled block 3
]


def log(msg):
    unreal.log("[create_ruin_asset] " + str(msg))


def main():
    log("start")

    mesh = unreal.DynamicMesh()
    prim_opts = unreal.GeometryScriptPrimitiveOptions()

    for dim_x, dim_y, dim_z, loc_x, loc_y, yaw in BLOCKS:
        xf = unreal.Transform(
            location=unreal.Vector(loc_x, loc_y, BASE_Z),
            rotation=unreal.Rotator(0.0, yaw, 0.0),
        )
        mesh = unreal.GeometryScript_Primitives.append_box(
            mesh, prim_opts, xf,
            dimension_x=dim_x, dimension_y=dim_y, dimension_z=dim_z,
            steps_x=0, steps_y=0, steps_z=0,
            origin=unreal.GeometryScriptPrimitiveOriginMode.BASE,
        )
        log("appended block dim=(%s,%s,%s) loc=(%s,%s,%s) yaw=%s" % (dim_x, dim_y, dim_z, loc_x, loc_y, BASE_Z, yaw))

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
