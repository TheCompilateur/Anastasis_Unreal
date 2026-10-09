"""Create only the opt-in Pontic horsetail candidate in this worktree.

Agent-authored procedural mesh, deterministic seed, three explicit LODs. This
does not claim botanical or historical accuracy; the scene A/B/A judges it.
"""
import importlib.util
import math
import os
import random

import unreal


HERE = os.path.dirname(os.path.abspath(__file__))
MODULE = importlib.util.spec_from_file_location(
    'ground_cover_geometry_v2', os.path.join(HERE, 'create-ground-cover.py'))
GC = importlib.util.module_from_spec(MODULE)
MODULE.loader.exec_module(GC)
ASSET = '/Game/Anastasis/PonticMicro/SM_Pontic_Horsetail_02'
MATERIAL = '/Game/Anastasis/Materials/M_AnastasisGrass'


def candidate(lod):
    rng = random.Random(20261009)
    buf = GC.Buffers()
    stems, rings, spokes, stem_segments = ((12, 5, 6, 3), (7, 4, 4, 2), (4, 3, 3, 2))[lod]
    for i in range(stems):
        angle = math.tau * i / stems + rng.uniform(-0.12, 0.12)
        radius = rng.uniform(2.0, 13.0)
        base = (radius * math.cos(angle), radius * math.sin(angle), -1.0)
        height = rng.uniform(38.0, 67.0)
        dark = (0.021, 0.048, 0.026)
        light = (0.055, 0.083, 0.038)
        GC.blade(buf, base, angle, height, 0.75, 0.03, 0.08,
                 stem_segments, dark, light, (math.cos(angle), math.sin(angle)))
        for ring in range(rings):
            z = height * (0.21 + 0.13 * ring)
            for spoke in range(spokes):
                yaw = angle + math.tau * spoke / spokes + ring * 0.16
                GC.blade(buf, (base[0], base[1], base[2] + z), yaw,
                         7.5 + 1.5 * ring, 0.42, 0.95, 0.24, 1,
                         dark, light, (math.cos(yaw), math.sin(yaw)))
    return buf.mesh()


def main():
    project = os.path.abspath(unreal.Paths.project_dir())
    root = os.path.abspath(os.path.join(HERE, '..', '..'))
    if not os.path.samefile(root, project):
        raise RuntimeError('wrong project: %s != %s' % (project, root))
    material = unreal.load_asset(MATERIAL)
    if material is None:
        raise RuntimeError('missing material ' + MATERIAL)
    triangles = GC.save_static_mesh([candidate(i) for i in range(3)], ASSET, material)
    unreal.log('PONTIC_HORSETAIL_V2 PASS asset=%s triangles=%s' % (ASSET, triangles))


if __name__ == '__main__':
    try:
        main()
    except Exception as exc:
        unreal.log_error('PONTIC_HORSETAIL_V2 FAIL %s' % exc)
    finally:
        unreal.SystemLibrary.quit_editor()
