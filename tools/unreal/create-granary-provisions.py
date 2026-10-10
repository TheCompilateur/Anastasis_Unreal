"""Forge one closed provision crate for the granary; the simulation owns its contents.

Geometry-only check: py -3 tools/unreal/create-granary-provisions.py
Editor creation: tools/unreal/create-granary-provisions.ps1
"""
import hashlib
import importlib.util
import json
from pathlib import Path

PKG = '/Game/Anastasis/GranaryProvisions'
NAME = 'SM_Granary_ProvisionCrate_01'
VERSION = 'granary-provisions-001-v1'

recipe = Path(__file__).with_name('create-village-buildings.py')
spec = importlib.util.spec_from_file_location('anastasis_village_mesh_recipe', recipe)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
Mesh, tint = module.Mesh, module.tint

WOOD = (.24, .145, .075, .87)
EDGE = (.15, .085, .045, .83)
IRON = (.065, .063, .06, .5)
ROPE = (.34, .245, .12, .9)


def geometry():
    m = Mesh()
    # A 110 x 76 x 68 cm closed, raised wooden bin. Its closed lid makes no crop claim.
    for x in (-43, 43):
        m.beam((x, -33, 4), (x, 33, 4), 13, 8, EDGE)
    for y in (-27, -9, 9, 27):
        m.beam((-50, y, 12), (50, y, 12), 17, 4, tint(WOOD, 1.03))
    for z in (22, 36, 50):
        for y in (-35, 35):
            m.beam((-53, y, z), (53, y, z), 12.5, 13, tint(WOOD, .88 + .04 * (z // 14)))
        for x in (-53, 53):
            m.beam((x, -34, z), (x, 34, z), 11.5, 13, tint(WOOD, .94 + .03 * (z // 14)))
    for x in (-51, 51):
        for y in (-33, 33):
            m.beam((x, y, 14), (x, y, 58), 7, 7, EDGE)
    # Planked, slightly proud lid, held by two dark straps.
    for y in (-29, -15, -1, 13, 27):
        m.beam((-56, y, 61), (56, y, 61), 13, 6, tint(WOOD, 1.05 if y % 2 else .96))
    for x in (-35, 35):
        m.beam((x, -37, 65), (x, 37, 65), 5, 3.5, IRON)
        for y in (-29, 29):
            m.lathe([(.5, 0), (1.9, 0.5), (1.9, 1.5), (.5, 2.1)],
                    IRON, segments=8, center=(x, y, 67))
    # Rope grips project beyond the short ends and remain legible in close view.
    for x in (-56, 56):
        m.tube([(x, -13, 44), (x + (5 if x > 0 else -5), -13, 48),
                (x + (7 if x > 0 else -7), 0, 52),
                (x + (5 if x > 0 else -5), 13, 48), (x, 13, 44)],
               [1.8] * 5, ROPE, sides=6)
    return m.finish()


def validate():
    verts, tris, colours, normals = geometry()
    lo = [min(p[i] for p in verts) for i in range(3)]
    hi = [max(p[i] for p in verts) for i in range(3)]
    size = [round(hi[i] - lo[i], 2) for i in range(3)]
    assert len(verts) == len(colours) == len(normals) and len(tris) < 2500
    assert lo[2] == 0.0 and 120 <= size[0] <= 130 and 76 <= size[1] <= 85 and 68 <= size[2] <= 72
    digest = hashlib.sha256(repr((verts, tris, colours)).encode('utf-8')).hexdigest()
    stats = dict(vertices=len(verts), triangles=len(tris), size_cm=size, sha256=digest)
    print('GRANARY_PROVISIONS_GEOMETRY ' + json.dumps(stats, sort_keys=True))
    return stats, (verts, tris, colours, normals)


def create():
    import unreal as u
    stats, (verts, tris, colours, normals) = validate()
    path = PKG + '/' + NAME
    eal = u.EditorAssetLibrary
    material = eal.load_asset('/Game/Anastasis/VillageArchitecture/M_AnastasisArchitecture')
    assert material, 'architecture material missing'
    asset = eal.load_asset(path) if eal.does_asset_exist(path) else None
    if not asset:
        buffers = u.GeometryScriptSimpleMeshBuffers()
        buffers.vertices = [u.Vector(*p) for p in verts]
        buffers.triangles = [u.IntVector(*t) for t in tris]
        buffers.normals = [u.Vector(*n) for n in normals]
        buffers.vertex_colors = [u.LinearColor(*c) for c in colours]
        buffers.uv0 = [u.Vector2D(p[0] / 100.0, p[1] / 100.0) for p in verts]
        dynamic = u.DynamicMesh()
        u.GeometryScript_MeshEdits.append_buffers_to_mesh(dynamic, buffers)
        options = u.GeometryScriptCreateNewStaticMeshAssetOptions()
        options.enable_recompute_normals = False
        options.enable_recompute_tangents = True
        options.enable_nanite = False
        options.enable_collision = False
        asset, outcome = u.GeometryScript_NewAssetUtils.create_new_static_mesh_asset_from_mesh(dynamic, path, options)
        assert asset, str(outcome)
        asset.set_material(0, material)
        eal.set_metadata_tag(asset, 'Recipe', VERSION)
        eal.set_metadata_tag(asset, 'GeometrySHA256', stats['sha256'])
        assert eal.save_asset(path)
    assert eal.get_metadata_tag(asset, 'Recipe') == VERSION
    assert eal.get_metadata_tag(asset, 'GeometrySHA256') == stats['sha256']
    assert asset.get_num_triangles(0) == len(tris)
    bounds = asset.get_bounding_box()
    actual = [round(getattr(bounds.max, a) - getattr(bounds.min, a), 2) for a in ('x', 'y', 'z')]
    assert all(abs(a - b) < .5 for a, b in zip(actual, stats['size_cm']))
    u.log('GRANARY_PROVISIONS_ASSET ' + json.dumps(dict(path=path, actual_size_cm=actual, **stats)))
    u.log('GRANARY_PROVISIONS_ASSETS COMPLETE assets=1')


if __name__ == '__main__':
    try:
        import unreal
    except ImportError:
        validate()
    else:
        try:
            create()
        except BaseException:
            import traceback
            unreal.log_error(traceback.format_exc())
            raise
        finally:
            unreal.SystemLibrary.quit_editor()
