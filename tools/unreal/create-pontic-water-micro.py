"""Regenerate four small Pontic micro-life meshes in a dedicated empty-map editor.

Uses the existing opaque vertex-colour foliage materials. The frog is a still,
visual animal; no behaviour, collision, or historical species identification.
"""
import importlib.util
import math
import os
import random

import unreal


ROOT = os.path.dirname(os.path.abspath(__file__))
SPEC = importlib.util.spec_from_file_location('ground_cover_geometry', os.path.join(ROOT, 'create-ground-cover.py'))
GC = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(GC)
PACKAGE = '/Game/Anastasis/PonticMicro'
SEED = 20261007


def add_blade(buf, base, yaw, height, width, lean, bend, segments, low, high):
    return GC.blade(buf, base, yaw, height, width, lean, bend, segments, low, high,
                    (math.cos(yaw), math.sin(yaw)))


def moss(lod):
    rng = random.Random(SEED + 1)
    buf = GC.Buffers()
    count = (180, 90, 45)[lod]
    for i in range(count):
        angle = rng.random() * math.tau
        radius = 28.0 * math.sqrt(rng.random())
        base = (radius * math.cos(angle), radius * math.sin(angle), -1.0)
        length = rng.uniform(3.0, 8.0)
        tint = rng.uniform(0.8, 1.2)
        add_blade(buf, base, angle + rng.uniform(-0.8, 0.8), length,
                  rng.uniform(2.2, 4.2), 0.18, 0.95, 2 if lod == 0 else 1,
                  (0.060 * tint, 0.095 * tint, 0.030 * tint),
                  (0.110 * tint, 0.160 * tint, 0.055 * tint))
    return buf.mesh()


def horsetail(lod):
    rng = random.Random(SEED + 2)
    buf = GC.Buffers()
    count = (13, 8, 5)[lod]
    for i in range(count):
        a = math.tau * i / count + rng.uniform(-0.18, 0.18)
        r = rng.uniform(2, 14)
        base = (r * math.cos(a), r * math.sin(a), -1.0)
        height = rng.uniform(32, 62)
        stem = (0.022, 0.048, 0.025)
        tip = (0.050, 0.078, 0.034)
        add_blade(buf, base, a, height, 0.6, 0.04, 0.11, 4 if lod == 0 else 2, stem, tip)
        for ring in range((4, 3, 2)[lod]):
            z = height * (0.27 + 0.15 * ring)
            for spoke in range((6, 4, 3)[lod]):
                yaw = a + math.tau * spoke / (6, 4, 3)[lod]
                add_blade(buf, (base[0], base[1], z), yaw, 8.0 + ring * 0.9,
                          0.35, 1.18, 0.18, 1, stem, tip)
    return buf.mesh()


def broad_leaf(buf, root, yaw, length, width, color):
    """Raised, folded heart-shaped leaf; open gaps keep the rosette from a green disc."""
    dx, dy = math.cos(yaw), math.sin(yaw)
    sx, sy = -dy, dx
    rows = []
    outline = ((0.0, 0.48, 0.0), (0.13, 0.63, 3.0), (0.29, 0.86, 6.0),
               (0.47, 1.0, 10.0), (0.67, 0.91, 12.0), (0.84, 0.62, 11.0),
               (1.0, 0.12, 9.0))
    for j, (forward, span, rise) in enumerate(outline):
        row = []
        for side in (-1, 0, 1):
            f = forward + (0.1 if j == 0 and side == 0 else 0.0)
            p = (root[0] + dx * length * f + sx * side * width * span * 0.5,
                 root[1] + dy * length * f + sy * side * width * span * 0.5,
                 root[2] + rise + (1 - abs(side)) * 1.8)
            green = tuple(v * (1.12 if side == 0 else 0.89) for v in color)
            row.append(buf.vertex(p, GC.up_normal((dx, dy)), green,
                                  max(0.0, min(1.0, p[2] / 100.0)), (side * 0.5 + 0.5, j / (len(outline)-1))))
        rows.append(row)
    for j in range(len(rows)-1):
        for k in range(2):
            a, b, c, d = rows[j][k], rows[j][k + 1], rows[j + 1][k], rows[j + 1][k + 1]
            buf.tri(a, c, b)
            buf.tri(b, c, d)


def coltsfoot(lod):
    rng = random.Random(SEED + 3)
    buf = GC.Buffers()
    count = (11, 7, 4)[lod]
    for i in range(count):
        yaw = math.tau * i / count + rng.uniform(-0.18, 0.18)
        radius = rng.uniform(1.0, 8.0)
        root = (radius * math.cos(yaw), radius * math.sin(yaw), -1.0)
        green = (0.065 * rng.uniform(0.85, 1.12), 0.115, 0.047)
        broad_leaf(buf, root, yaw, rng.uniform(15, 24), rng.uniform(9, 14), green)
    return buf.mesh()


def ellipsoid(buf, centre, radii, color, sides, rings):
    cx, cy, cz = centre
    rx, ry, rz = radii
    row_ids = []
    for j in range(1, rings):
        theta = math.pi * j / rings
        row = []
        for k in range(sides):
            phi = math.tau * k / sides
            nx, ny, nz = math.sin(theta) * math.cos(phi), math.sin(theta) * math.sin(phi), math.cos(theta)
            p = (cx + rx * nx, cy + ry * ny, cz + rz * nz)
            n = (nx / rx, ny / ry, nz / rz)
            length = math.sqrt(sum(v * v for v in n))
            row.append(buf.vertex(p, tuple(v / length for v in n), color, 0.0,
                                  (k / sides, j / rings)))
        row_ids.append(row)
    top = buf.vertex((cx, cy, cz + rz), (0, 0, 1), color, 0.0, (0.5, 0))
    bottom = buf.vertex((cx, cy, cz - rz), (0, 0, -1), color, 0.0, (0.5, 1))
    for k in range(sides):
        q = (k + 1) % sides
        buf.tri(top, row_ids[0][k], row_ids[0][q])
        for j in range(len(row_ids) - 1):
            a, b = row_ids[j][k], row_ids[j][q]
            c, d = row_ids[j + 1][k], row_ids[j + 1][q]
            buf.tri(a, c, b)
            buf.tri(b, c, d)
        buf.tri(bottom, row_ids[-1][q], row_ids[-1][k])


def frog(lod):
    buf = GC.Buffers()
    sides, rings = ((12, 6), (8, 4), (6, 3))[lod]
    body = (0.040, 0.052, 0.025)
    limb = (0.035, 0.047, 0.022)
    ellipsoid(buf, (0, 0, 4.7), (9.0, 5.0, 3.7), body, sides, rings)
    ellipsoid(buf, (7.5, 0, 4.6), (4.1, 4.3, 2.9), (0.048, 0.060, 0.030), sides, rings)
    for side in (-1, 1):
        ellipsoid(buf, (-5.5, side * 4.9, 2.5), (3.8, 2.1, 2.2), limb, sides, rings)
        ellipsoid(buf, (-9.5, side * 6.2, 1.3), (4.1, 1.2, 0.9), limb, sides, rings)
        ellipsoid(buf, (6.1, side * 5.1, 1.9), (3.3, 1.0, 1.0), limb, sides, rings)
        ellipsoid(buf, (7.2, side * 3.5, 7.0), (1.9, 1.6, 1.5), body, sides, rings)
        ellipsoid(buf, (8.0, side * 4.0, 7.5), (0.75, 0.6, 0.65), (0.012, 0.011, 0.008), sides, rings)
    return buf.mesh()


def main():
    foliage = unreal.load_asset('/Game/Anastasis/Materials/M_AnastasisGrass')
    animal = unreal.load_asset('/Game/Anastasis/Materials/M_AnastasisVegetation')
    if not foliage or not animal:
        raise RuntimeError('existing vertex-colour materials missing')
    specs = (
        ('SM_Pontic_Moss_01', moss, foliage),
        ('SM_Pontic_Horsetail_01', horsetail, foliage),
        ('SM_Pontic_Coltsfoot_01', coltsfoot, foliage),
        ('SM_Pontic_Frog_01', frog, animal),
    )
    report = []
    for name, generator, material in specs:
        path = PACKAGE + '/' + name
        tris = GC.save_static_mesh([generator(i) for i in range(3)], path, material)
        report.append('%s=%s' % (name, tris))
    unreal.log('PONTIC_WATER_MICRO_ASSETS PASS ' + ' '.join(report))


if __name__ == '__main__':
    try:
        main()
    except Exception as exc:
        unreal.log_error('PONTIC_WATER_MICRO_ASSETS FAIL %s' % exc)
    finally:
        if os.environ.get('ANASTASIS_PONTIC_MICRO_QUIT') == '1':
            unreal.SystemLibrary.quit_editor()
