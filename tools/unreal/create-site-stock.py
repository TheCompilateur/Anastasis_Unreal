"""Two small, deterministic site-stock meshes. Run with create-site-stock.ps1.

Geometry-only validation: py -3 tools/unreal/create-site-stock.py
The existing village material supplies the stone and timber grain from vertex colour.
Only the two SiteStock001 assets below may be rebuilt by this recipe.
"""
import hashlib
import importlib.util
import json
import math
import os
from pathlib import Path

PKG = '/Game/Anastasis/SiteStock001'
VERSION = 'site-stock-visual-001-v1'
NAMES = ('SM_Site_TimberBundle_01', 'SM_Site_StoneBundle_01')

_buildings_path = Path(__file__).with_name('create-village-buildings.py')
_spec = importlib.util.spec_from_file_location('anastasis_village_mesh_recipe', _buildings_path)
_buildings = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(_buildings)
Mesh = _buildings.Mesh
tint = _buildings.tint
WOOD = _buildings.WOOD
STONE = _buildings.STONE


def timber_bundle():
    m = Mesh()
    # Three barked timbers resting on two narrow battens, within a 1 m footprint.
    for y in (-17.0, 17.0):
        m.beam((-47, y, 3), (47, y, 3), 7, 6, tint(WOOD, .72))
    for i, (y, z, radius) in enumerate(((-14, 12, 8), (0, 12, 7.5), (14, 12, 8),
                                         (-7, 26, 7.2), (7, 26, 7.5))):
        m.tube([(-42 + i % 2 * 3, y, z), (42 - i % 3 * 2, y + (i % 2) * 1.5, z + (i % 3 - 1) * 1.0)],
               [radius, radius * .96], tint(WOOD, .88 + .045 * i), 9)
    return m.finish()


def rock(m, x, y, z, rx, ry, height, phase, colour):
    # Four uneven sides, narrowed top, individual flat-shaded faces.
    lo = [(x - rx, y - ry * .84, z), (x + rx * .88, y - ry, z),
          (x + rx, y + ry * .82, z), (x - rx * .9, y + ry, z)]
    hi = [(x + .12 * rx - rx * .76, y - ry * .64, z + height * .9),
          (x + rx * .75, y - ry * .72, z + height),
          (x + rx * .76, y + ry * .73, z + height * .85),
          (x - rx * .74, y + ry * .66, z + height * .96)]
    for i in range(4):
        j = (i + 1) % 4
        m.quad(lo[i], lo[j], hi[j], hi[i], tint(colour, .86 + .05 * ((i + phase) % 4)))
    m.quad(hi[0], hi[1], hi[2], hi[3], tint(colour, 1.04))
    m.quad(lo[3], lo[2], lo[1], lo[0], tint(colour, .78))


def stone_bundle():
    m = Mesh()
    lower = [(-21, -13, 18, 14, 15), (4, -14, 17, 15, 14), (24, -10, 13, 11, 14),
             (-26, 10, 15, 12, 14), (-3, 12, 20, 16, 15), (23, 13, 13, 12, 12)]
    for i, (x, y, rx, ry, h) in enumerate(lower):
        rock(m, x, y, 0, rx, ry, h, i, tint(STONE, .84 + .04 * (i % 4)))
    for i, (x, y, rx, ry, h) in enumerate(((-12, -6, 17, 15, 13), (16, 4, 16, 14, 15))):
        rock(m, x, y, 12, rx, ry, h, i + 3, tint(STONE, .91 + .06 * i))
    return m.finish()


def geometry():
    return dict(zip(NAMES, (timber_bundle(), stone_bundle())))


def validate():
    stats = {}
    for name, (verts, tris, colours, normals) in geometry().items():
        lo = [min(p[axis] for p in verts) for axis in range(3)]
        hi = [max(p[axis] for p in verts) for axis in range(3)]
        size = [round(hi[i] - lo[i], 2) for i in range(3)]
        assert len(verts) and len(tris) and len(verts) == len(colours) == len(normals)
        assert lo[2] == 0 and all(math.isfinite(q) for p in normals for q in p)
        assert size[0] <= 105 and size[1] <= 65 and size[2] <= 42, (name, size)
        digest = hashlib.sha256(repr((verts, tris, colours)).encode('utf-8')).hexdigest()
        stats[name] = dict(vertices=len(verts), triangles=len(tris), size_cm=size, sha256=digest)
    print('SITE_STOCK_GEOMETRY ' + json.dumps(stats, sort_keys=True))
    return stats


def create():
    import unreal as u
    eal = u.EditorAssetLibrary
    material = eal.load_asset('/Game/Anastasis/VillageBuildings/M_VillageBuilding_Surface')
    assert material, 'Existing village material is required'
    expected = validate()
    for name, (verts, tris, colours, normals) in geometry().items():
        path = PKG + '/' + name
        asset = eal.load_asset(path) if eal.does_asset_exist(path) else None
        if asset and os.environ.get('ANASTASIS_SITE_STOCK_REBUILD') == '1':
            assert path.startswith(PKG + '/SM_') and name in NAMES
            assert eal.get_metadata_tag(asset, 'Recipe') == VERSION
            assert eal.delete_asset(path), 'Owned asset rebuild failed'
            asset = None
        if asset:
            assert eal.get_metadata_tag(asset, 'Recipe') == VERSION
            assert eal.get_metadata_tag(asset, 'GeometrySHA256') == expected[name]['sha256']
            assert asset.get_num_triangles(0) == len(tris)
        else:
            buffers = u.GeometryScriptSimpleMeshBuffers()
            buffers.vertices = [u.Vector(*p) for p in verts]
            buffers.triangles = [u.IntVector(*t) for t in tris]
            buffers.normals = [u.Vector(*n) for n in normals]
            buffers.vertex_colors = [u.LinearColor(*c) for c in colours]
            buffers.uv0 = [u.Vector2D(p[0] / 100, p[1] / 100) for p in verts]
            dynamic = u.DynamicMesh()
            u.GeometryScript_MeshEdits.append_buffers_to_mesh(dynamic, buffers)
            options = u.GeometryScriptCreateNewStaticMeshAssetOptions()
            options.enable_recompute_normals = False
            options.enable_recompute_tangents = True
            options.enable_nanite = False
            asset, outcome = u.GeometryScript_NewAssetUtils.create_new_static_mesh_asset_from_mesh(dynamic, path, options)
            assert asset, str(outcome)
            asset.set_material(0, material)
            eal.set_metadata_tag(asset, 'Recipe', VERSION)
            eal.set_metadata_tag(asset, 'GeometrySHA256', expected[name]['sha256'])
            assert eal.save_asset(path)
        u.log('SITE_STOCK_ASSET ' + json.dumps(dict(name=name, path=path, **expected[name])))
    u.log('SITE_STOCK_ASSETS COMPLETE assets=2')


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
