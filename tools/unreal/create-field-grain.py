"""Deterministic, actual 3D grain clump for live field presentation.

Geometry check: py -3 tools/unreal/create-field-grain.py
Asset creation: tools/unreal/create-field-grain.ps1
"""
import hashlib
import importlib.util
import json
import math
import os
from pathlib import Path

PKG = '/Game/Anastasis/FieldGrain'
NAME = 'SM_Field_GrainClump_01'
VERSION = 'field-grain-001-v1'

_source = Path(__file__).with_name('create-village-buildings.py')
_spec = importlib.util.spec_from_file_location('anastasis_mesh_recipe', _source)
_module = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(_module)
Mesh = _module.Mesh

STEM = (.095, .115, .033, 0.0)
LEAF = (.145, .150, .046, 0.0)
HEAD = (.29, .225, .090, 0.0)
AWN = (.24, .19, .070, 0.0)


def geometry():
    mesh = Mesh()
    for i in range(9):
        col, row = i % 3, i // 3
        x = (col - 1) * 36.0 + 5.0 * math.sin(i * 2.13)
        y = (row - 1) * 36.0 + 5.0 * math.cos(i * 1.79)
        height = 73.0 + (i * 17) % 30
        lx = 4.0 * math.sin(i * 2.7)
        ly = 4.0 * math.cos(i * 1.9)
        mesh.tube([(x, y, 0), (x + lx * .45, y + ly * .45, height * .56),
                   (x + lx, y + ly, height - 15)], [1.65, 1.2, .9], STEM, sides=5)
        for side in (-1, 1):
            z = 19.0 if side < 0 else 37.0
            dx = side * (14.0 + i % 3 * 3.0)
            dy = (5.0 if i % 2 else -5.0) * side
            mesh.beam((x, y, z), (x + dx, y + dy, z + 22.0), 4.0, .65, LEAF)
        hx, hy = x + lx, y + ly
        ear = [(hx, hy, height - 15), (hx + .4, hy, height - 12),
               (hx + 1.0, hy, height - 7), (hx + 1.3, hy, height - 2),
               (hx + 1.5, hy, height + 4), (hx + 1.6, hy, height + 9)]
        mesh.tube(ear, [1.5, 3.6, 4.2, 3.8, 2.6, .4], HEAD, sides=6)
        for tier in range(3):
            z = height - 10 + tier * 6
            radius = 4.8 - tier * .7
            for side in (-1, 1):
                mesh.beam((hx, hy, z),
                          (hx + side * radius * 1.8, hy + side * 1.5, z + 8.0),
                          .7, .55, AWN)
    vertices, triangles, colours, normals = mesh.finish()
    # M_AnastasisGrass interprets vertex alpha as height-dependent wind weight.
    colours = [(r, g, b, min(1.0, max(0.0, p[2] / 110.0)))
               for p, (r, g, b, _) in zip(vertices, colours)]
    return vertices, triangles, colours, normals


def validate():
    vertices, triangles, colours, normals = geometry()
    lo = [min(v[k] for v in vertices) for k in range(3)]
    hi = [max(v[k] for v in vertices) for k in range(3)]
    size = [round(b - a, 2) for a, b in zip(lo, hi)]
    assert len(vertices) == len(colours) == len(normals)
    assert 90 <= size[0] <= 135 and 90 <= size[1] <= 135 and 80 <= size[2] <= 125
    assert len(triangles) < 2000 and lo[2] == 0.0
    digest = hashlib.sha256(repr((vertices, triangles, colours)).encode('utf-8')).hexdigest()
    stats = dict(vertices=len(vertices), triangles=len(triangles), size_cm=size, sha256=digest)
    print('FIELD_GRAIN_GEOMETRY ' + json.dumps(stats, sort_keys=True))
    return stats, (vertices, triangles, colours, normals)


def create():
    import unreal as u
    stats, (vertices, triangles, colours, normals) = validate()
    path = PKG + '/' + NAME
    assets = u.EditorAssetLibrary
    material = assets.load_asset('/Game/Anastasis/Materials/M_AnastasisGrass')
    assert material, 'grass material missing'
    asset = assets.load_asset(path) if assets.does_asset_exist(path) else None
    if asset and os.environ.get('ANASTASIS_FIELD_GRAIN_REBUILD') == '1':
        assert assets.get_metadata_tag(asset, 'Recipe') == VERSION
        assert assets.delete_asset(path), 'owned grain mesh rebuild failed'
        asset = None
    if not asset:
        buffers = u.GeometryScriptSimpleMeshBuffers()
        buffers.vertices = [u.Vector(*v) for v in vertices]
        buffers.triangles = [u.IntVector(*t) for t in triangles]
        buffers.normals = [u.Vector(*n) for n in normals]
        buffers.vertex_colors = [u.LinearColor(*c) for c in colours]
        buffers.uv0 = [u.Vector2D(v[0] / 120.0, v[1] / 120.0) for v in vertices]
        dynamic = u.DynamicMesh()
        u.GeometryScript_MeshEdits.append_buffers_to_mesh(dynamic, buffers)
        options = u.GeometryScriptCreateNewStaticMeshAssetOptions()
        options.enable_recompute_normals = False
        options.enable_recompute_tangents = True
        options.enable_nanite = False
        options.enable_collision = False
        asset, outcome = u.GeometryScript_NewAssetUtils.create_new_static_mesh_asset_from_mesh(
            dynamic, path, options)
        assert asset, str(outcome)
        asset.set_material(0, material)
        assets.set_metadata_tag(asset, 'Recipe', VERSION)
        assets.set_metadata_tag(asset, 'GeometrySHA256', stats['sha256'])
        assert assets.save_asset(path)
    assert assets.get_metadata_tag(asset, 'Recipe') == VERSION
    assert assets.get_metadata_tag(asset, 'GeometrySHA256') == stats['sha256']
    assert asset.get_num_triangles(0) == len(triangles)
    bounds = asset.get_bounding_box()
    actual = [round(getattr(bounds.max, a) - getattr(bounds.min, a), 2) for a in ('x', 'y', 'z')]
    assert all(abs(a - b) < .5 for a, b in zip(actual, stats['size_cm'])), (actual, stats['size_cm'])
    u.log('FIELD_GRAIN_ASSET ' + json.dumps(dict(path=path, actual_size_cm=actual, **stats)))
    u.log('FIELD_GRAIN_ASSETS COMPLETE assets=1')


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
