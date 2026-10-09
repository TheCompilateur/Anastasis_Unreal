"""Forge one small carried wicker basket as a real Static Mesh.

Geometry-only check: py -3 tools/unreal/create-food-basket.py
Editor creation: tools/unreal/create-food-basket.ps1
"""
import hashlib
import importlib.util
import json
import math
import os
from pathlib import Path

PKG = '/Game/Anastasis/CarriedFood'
NAME = 'SM_Food_Basket_01'
VERSION = 'food-basket-001-v1'

_recipe = Path(__file__).with_name('create-village-buildings.py')
_spec = importlib.util.spec_from_file_location('anastasis_village_mesh_recipe', _recipe)
_buildings = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(_buildings)
Mesh = _buildings.Mesh
tint = _buildings.tint

WICKER = (.31, .20, .105, .84)
RIM = (.23, .135, .065, .84)
FOOD = (.42, .32, .13, .92)


def ellipse(rx, ry, z, segments=28):
    return [(rx * math.cos(math.tau * k / segments),
             ry * math.sin(math.tau * k / segments), z)
            for k in range(segments + 1)]


def geometry():
    m = Mesh()
    # A shallow woven base, narrower than the open rim. Each strand is actual volume.
    for y in (-11, -6, -1, 4, 9):
        span = 17.0 * math.sqrt(max(.1, 1.0 - (y / 15.0) ** 2))
        m.beam((-span, y, 1.5), (span, y, 1.5), 2.6, 2.2, tint(WICKER, .88))
    for x in (-13, -8, -3, 2, 7, 12):
        span = 14.0 * math.sqrt(max(.1, 1.0 - (x / 18.0) ** 2))
        m.beam((x, -span, 3.0), (x, span, 3.0), 2.1, 2.0, tint(WICKER, 1.04))

    # The alternating ribs and hoops remain legible from every side.
    for k in range(24):
        angle = math.tau * k / 24
        dx, dy = math.cos(angle), math.sin(angle)
        points = [(17.0 * dx, 13.2 * dy, 2.0),
                  (19.5 * dx, 14.8 * dy, 14.0),
                  (22.0 * dx, 16.3 * dy, 28.0)]
        m.tube(points, [1.25, 1.35, 1.45], tint(WICKER, .85 + .035 * (k % 5)), 6)
    for level in range(3, 29, 4):
        t = (level - 2.0) / 26.0
        rx, ry = 17.0 + 5.0 * t, 13.2 + 3.1 * t
        m.tube(ellipse(rx, ry, float(level)), [1.2] * 29,
               tint(WICKER, .88 if level % 8 == 3 else 1.08), 5, cap=False)

    # A darker rolled lip and one arch meet the carrier's hand above the opening.
    m.tube(ellipse(22.2, 16.5, 29.0), [2.1] * 29, RIM, 7, cap=False)
    arch = [(0.0, -16.5, 29.0), (0.0, -15.0, 35.0), (0.0, -11.0, 42.0),
            (0.0, -5.0, 46.0), (0.0, 0.0, 47.0), (0.0, 5.0, 46.0),
            (0.0, 11.0, 42.0), (0.0, 15.0, 35.0), (0.0, 16.5, 29.0)]
    m.tube(arch, [2.25] * len(arch), RIM, 8)

    # A modest visible harvest; the mesh is shown only when the real bag is nonempty.
    m.lathe([(1.0, 19.0), (7.0, 18.5), (12.0, 16.5), (14.0, 14.0)],
            FOOD, segments=20, wear=.045)
    return m.finish()


def validate():
    verts, tris, colours, normals = geometry()
    lo = [min(p[i] for p in verts) for i in range(3)]
    hi = [max(p[i] for p in verts) for i in range(3)]
    size = [round(hi[i] - lo[i], 2) for i in range(3)]
    assert len(verts) == len(colours) == len(normals) and len(tris) < 4000
    assert lo[2] == 0.0 and 47 <= size[0] <= 50 and 36 <= size[1] <= 39 and 47 <= size[2] <= 50
    digest = hashlib.sha256(repr((verts, tris, colours)).encode('utf-8')).hexdigest()
    stats = dict(vertices=len(verts), triangles=len(tris), size_cm=size, sha256=digest)
    print('FOOD_BASKET_GEOMETRY ' + json.dumps(stats, sort_keys=True))
    return stats, (verts, tris, colours, normals)


def create():
    import unreal as u
    stats, (verts, tris, colours, normals) = validate()
    path = PKG + '/' + NAME
    eal = u.EditorAssetLibrary
    material = eal.load_asset('/Game/Anastasis/VillageArchitecture/M_AnastasisArchitecture')
    assert material, 'architecture material missing'
    asset = eal.load_asset(path) if eal.does_asset_exist(path) else None
    if asset and os.environ.get('ANASTASIS_FOOD_BASKET_REBUILD') == '1':
        assert eal.get_metadata_tag(asset, 'Recipe') == VERSION
        assert eal.delete_asset(path), 'owned basket rebuild failed'
        asset = None
    if not asset:
        buffers = u.GeometryScriptSimpleMeshBuffers()
        buffers.vertices = [u.Vector(*p) for p in verts]
        buffers.triangles = [u.IntVector(*t) for t in tris]
        buffers.normals = [u.Vector(*n) for n in normals]
        buffers.vertex_colors = [u.LinearColor(*c) for c in colours]
        buffers.uv0 = [u.Vector2D(p[0] / 50.0, p[1] / 50.0) for p in verts]
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
    assert all(abs(a - b) < .5 for a, b in zip(actual, stats['size_cm'])), (actual, stats['size_cm'])
    u.log('FOOD_BASKET_ASSET ' + json.dumps(dict(path=path, actual_size_cm=actual, **stats)))
    u.log('FOOD_BASKET_ASSETS COMPLETE assets=1')


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
