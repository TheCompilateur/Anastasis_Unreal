"""Well, house and granary at tile scale (1 tile = 400 cm).

Run through create-village-buildings.ps1. Creates static meshes in
VillageBuildings and refuses recipe drift on existing assets.
Pure geometry validates with ANASTASIS_BUILDINGS_GEOMETRY_ONLY=1.
"""
import math
import os
import json
import random

PKG = '/Game/Anastasis/VillageBuildings'
VERSION = 'village-buildings-001-v1'
STONE = (.42, .39, .34, .75)
WOOD = (.22, .12, .055, .84)
DAUB = (.40, .28, .16, .93)
TILE = (.46, .17, .07, .62)
CLAY = (.34, .14, .06, .66)
IRON = (.05, .048, .045, .36)
ROPE = (.29, .22, .12, .92)
NAMES = ['SM_Well_Stone_01', 'SM_House_Refuge_01', 'SM_Granary_Raised_01']
# Footprints stay inside one 4 m tile. Heights are the readable difference:
# well yoke ~2 m, granary ridge ~2.9 m, house chimney ~4.4 m.
LIMITS = {
    'SM_Well_Stone_01': (140, 220),
    'SM_House_Refuge_01': (200, 460),
    'SM_Granary_Raised_01': (180, 340),
}

def add(a, b):
    return tuple(x + y for x, y in zip(a, b))
def sub(a, b):
    return tuple(x - y for x, y in zip(a, b))
def mul(a, s):
    return tuple(x * s for x in a)
def cross(a, b):
    return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])
def norm(a):
    d = math.sqrt(sum(x * x for x in a))
    if d < 1e-10:
        raise ValueError('zero normal')
    return mul(a, 1 / d)
def tint(c, k):
    return tuple(v * k for v in c[:3]) + (c[3],)

class Mesh:
    def __init__(self):
        self.v = []
        self.t = []
        self.c = []
    def vert(self, p, c):
        self.v.append(tuple(p))
        self.c.append(c)
        return len(self.v) - 1
    def quad(self, a, b, c, d, color):
        ids = [self.vert(p, color) for p in (a, b, c, d)]
        self.t.extend([(ids[0], ids[1], ids[2]), (ids[0], ids[2], ids[3])])
    def tube(self, points, radii, color, sides=8, cap=True):
        rings = []
        previous_u = None
        for j, p in enumerate(points):
            tangent = norm(sub(points[min(j + 1, len(points) - 1)], points[max(0, j - 1)]))
            ref = (0, 0, 1) if abs(tangent[2]) < .9 else (1, 0, 0)
            u = norm(cross(tangent, ref)) if previous_u is None else norm(sub(previous_u, mul(tangent, sum(x * y for x, y in zip(previous_u, tangent)))))
            previous_u = u
            v = cross(tangent, u)
            ring = []
            for k in range(sides):
                ang = math.tau * k / sides
                ring.append(self.vert(add(p, mul(add(mul(u, math.cos(ang)), mul(v, math.sin(ang))), radii[j])), tint(color, .93 + .07 * math.cos(ang))))
            rings.append(ring)
        for a, b in zip(rings, rings[1:]):
            for k in range(sides):
                l = (k + 1) % sides
                self.t.extend([(a[k], a[l], b[k]), (a[l], b[l], b[k])])
        if cap:
            for ring, p, flip in [(rings[0], points[0], True), (rings[-1], points[-1], False)]:
                center = self.vert(p, color)
                for k in range(sides):
                    a, b = ring[k], ring[(k + 1) % sides]
                    self.t.append((center, b, a) if flip else (center, a, b))
    def beam(self, a, b, w, h, color, u=None):
        axis = norm(sub(b, a))
        u = norm(u) if u else norm(cross(axis, (0, 0, 1) if abs(axis[2]) < .9 else (1, 0, 0)))
        v = norm(cross(axis, u))
        corners = []
        for p in (a, b):
            corners.append([add(p, add(mul(u, x * w / 2), mul(v, y * h / 2))) for x, y in [(-1, -1), (1, -1), (1, 1), (-1, 1)]])
        r, s = corners
        self.quad(r[3], r[2], r[1], r[0], color)
        self.quad(s[0], s[1], s[2], s[3], color)
        for k in range(4):
            l = (k + 1) % 4
            self.quad(r[k], r[l], s[l], s[k], tint(color, .94 + .02 * k))
    def lathe(self, profile, color, segments=28, center=(0, 0, 0), wear=0.0):
        rings = []
        for j, (radius, z) in enumerate(profile):
            ring = []
            for k in range(segments):
                ang = math.tau * k / segments
                factor = 1 + wear * math.sin(ang * 7 + z * .31) * math.sin(ang * 3 - z * .07)
                c = tint(color, .91 + .06 * math.sin(z * .35) + .03 * math.sin(ang * 13 + z * .9))
                ring.append(self.vert(add(center, (radius * factor * math.cos(ang), radius * factor * math.sin(ang), z)), c))
            rings.append(ring)
        for a, b in zip(rings, rings[1:]):
            for k in range(segments):
                l = (k + 1) % segments
                self.t.extend([(a[k], a[l], b[k]), (a[l], b[l], b[k])])
    def finish(self):
        normals = [[0., 0., 0.] for _ in self.v]
        for a, b, c in self.t:
            n = cross(sub(self.v[b], self.v[a]), sub(self.v[c], self.v[a]))
            assert sum(x * x for x in n) > 1e-13, 'degenerate triangle'
            for i in (a, b, c):
                for k in range(3):
                    normals[i][k] += n[k]
        normals = [norm(n) for n in normals]
        assert all(math.isfinite(x) for p in self.v for x in p)
        # Authored XY stays on the tile centre. Only the ground plane is dropped to z=0.
        lo = min(p[2] for p in self.v)
        self.v = [(p[0], p[1], p[2] - lo) for p in self.v]
        self.t = [(a, c, b) for a, b, c in self.t]
        for a, b, c in self.t:
            geometric = cross(sub(self.v[c], self.v[a]), sub(self.v[b], self.v[a]))
            summed = add(add(normals[a], normals[b]), normals[c])
            assert sum(x * y for x, y in zip(geometric, summed)) > 0, 'normal/winding disagreement'
        return self.v, self.t, self.c, normals

def plank_x(m, x0, x1, y, z, thick, height, color):
    if x1 - x0 < 8:
        return
    m.beam((x0, y, z), (x1, y, z), thick, height, color)

def plank_y(m, y0, y1, x, z, thick, height, color):
    if y1 - y0 < 8:
        return
    m.beam((x, y0, z), (x, y1, z), thick, height, color)

def openings_at(z, height, openings):
    z0, z1 = z - height / 2, z + height / 2
    return [(a, b) for a, b, oz0, oz1 in openings if z1 > oz0 and z0 < oz1]

def row_x(m, x0, x1, y, z, thick, height, color, openings):
    cursor = x0
    for a, b in openings_at(z, height, openings):
        a = max(a, x0)
        b = min(b, x1)
        if b <= cursor:
            continue
        plank_x(m, cursor, a, y, z, thick, height, color)
        cursor = max(cursor, b)
    plank_x(m, cursor, x1, y, z, thick, height, color)

def courses(z0, z1, height, gap):
    z = z0 + height / 2
    out = []
    while z + height / 2 <= z1 + 0.2:
        out.append(z)
        z += height + gap
    return out

def roof(m, x0, x1, y_eave, z_eave, z_ridge, n, color, rng):
    span = abs(y_eave)
    rise = z_ridge - z_eave
    length = math.hypot(span, rise)
    step = length / n
    for sign in (1, -1):
        sdir = (0.0, -sign * span / length, rise / length)
        for i in range(n):
            t = (i + 0.45) / n
            y = sign * span * (1 - t)
            z = z_eave + rise * t
            m.beam((x0, y, z), (x1, y, z), step * 1.35, 3.6, tint(color, rng.uniform(.78, 1.14)), u=sdir)
        m.beam((x0, sign * span, z_eave), (x0, 0, z_ridge), 8, 10, tint(WOOD, .62))
        m.beam((x1, sign * span, z_eave), (x1, 0, z_ridge), 8, 10, tint(WOOD, .62))
    m.beam((x0, 0, z_ridge + 3), (x1, 0, z_ridge + 3), 16, 7, tint(color, .72))

def well():
    m = Mesh()
    # Open stone ring: inner shaft, lip, outer wall, base annulus.
    m.lathe([
        (34, 16), (36, 58), (42, 84), (56, 98), (78, 106),
        (94, 100), (90, 76), (86, 30), (82, 6), (40, 6),
    ], STONE, 32, wear=.014)
    for y in (-108, 108):
        m.beam((0, y, 0), (0, y, 198), 16, 16, tint(WOOD, .72))
    m.beam((0, -108, 188), (0, 108, 188), 14, 16, tint(WOOD, .64))
    m.tube([(0, 0, 180), (0, 0, 46)], [1.7, 1.7], ROPE, 6)
    m.lathe([
        (2, 22), (9, 22), (13, 30), (15, 44), (14, 50),
        (11, 50), (10, 44), (7, 30), (3, 24),
    ], CLAY, 16, wear=.008)
    return m.finish()

def house():
    m = Mesh()
    rng = random.Random(101)
    # Stone plinth, three set-back courses.
    m.beam((-172, 0, 8), (172, 0, 8), 292, 16, tint(STONE, .90))
    m.beam((-166, 0, 24), (166, 0, 24), 278, 16, tint(STONE, 1.02))
    m.beam((-158, 0, 38), (158, 0, 38), 264, 12, tint(STONE, .84))
    for x in (-150, 150):
        for y in (-116, 116):
            m.beam((x, y, 36), (x, y, 226), 18, 18, tint(WOOD, .58))
    door = [(-46, 46, 40, 190)]
    window = [(52, 96, 128, 170)]
    for z in courses(46, 214, 13, 2.2):
        shade = rng.uniform(.78, 1.16)
        row_x(m, -146, 146, 118, z, 7, 12, tint(WOOD, shade), door)
        row_x(m, -146, 146, -118, z, 7, 12, tint(WOOD, rng.uniform(.78, 1.16)), window)
    # Gable ends in daub, tapering to the ridge.
    for x, thick in ((-152, 8), (152, 8)):
        for z in courses(214, 360, 16, 2):
            half = max(0, 116 * (368 - z) / (368 - 214) - 8)
            plank_y(m, -half, half, x, z, thick, 14, tint(DAUB, rng.uniform(.86, 1.08)))
    for x in (-48, 48):
        m.beam((x, 126, 40), (x, 126, 198), 12, 16, tint(WOOD, .55))
    m.beam((-48, 126, 198), (48, 126, 198), 14, 16, tint(WOOD, .55))
    m.beam((-52, 142, 10), (52, 142, 10), 30, 16, tint(STONE, .95))
    for x in (-74, 74):
        m.beam((x, -126, 128), (x, -126, 172), 8, 12, tint(WOOD, .6))
    m.beam((52, -126, 170), (96, -126, 170), 8, 12, tint(WOOD, .6))
    roof(m, -176, 176, 148, 220, 372, 9, WOOD, rng)
    m.beam((78, -28, 300), (78, -28, 438), 34, 34, tint(STONE, .88))
    return m.finish()

def granary():
    m = Mesh()
    rng = random.Random(202)
    for x in (-96, 96):
        for y in (-70, 70):
            m.beam((x, y, 0), (x, y, 24), 42, 42, tint(STONE, rng.uniform(.86, 1.05)))
            m.beam((x, y, 20), (x, y, 196), 13, 13, tint(WOOD, .6))
    m.beam((-112, 0, 70), (112, 0, 70), 172, 14, tint(WOOD, .7))
    for y in (-55, -18, 18, 55):
        m.beam((-100, y, 56), (100, y, 56), 8, 10, tint(WOOD, .5))
    door = [(-32, 32, 76, 168)]
    for z in courses(82, 178, 12, 2):
        shade = rng.uniform(.8, 1.14)
        row_x(m, -104, 104, 82, z, 6, 11, tint(WOOD, shade), door)
        row_x(m, -104, 104, -82, z, 6, 11, tint(WOOD, rng.uniform(.8, 1.14)), [])
        plank_y(m, -78, 78, -106, z, 6, 11, tint(WOOD, rng.uniform(.8, 1.12)))
        plank_y(m, -78, 78, 106, z, 6, 11, tint(WOOD, rng.uniform(.8, 1.12)))
    for x in (-34, 34):
        m.beam((x, 90, 76), (x, 90, 172), 8, 12, tint(WOOD, .55))
    m.beam((-34, 90, 172), (34, 90, 172), 10, 12, tint(WOOD, .55))
    for i, z in enumerate((18, 42, 66)):
        m.beam((-34, 108 + i * 16, z), (34, 108 + i * 16, z), 18, 8, tint(WOOD, .75))
    roof(m, -122, 122, 104, 186, 292, 7, TILE, rng)
    m.lathe([
        (2, 0), (8, 2), (12, 10), (14, 26), (12, 36), (8, 40), (6, 36), (9, 24), (7, 8), (3, 2),
    ], CLAY, 14, center=(62, 128, 0), wear=.01)
    return m.finish()

BUILDERS = dict(zip(NAMES, (well, house, granary)))

def geometry():
    return {name: fn() for name, fn in BUILDERS.items()}

def validate():
    stats = {}
    for name, (verts, tris, _colors, _normals) in geometry().items():
        xs = [p[0] for p in verts]
        ys = [p[1] for p in verts]
        zs = [p[2] for p in verts]
        reach, height = LIMITS[name]
        size = (max(xs) - min(xs), max(ys) - min(ys), max(zs) - min(zs))
        assert min(zs) == 0, (name, min(zs))
        assert max(abs(x) for x in xs) <= reach, (name, max(abs(x) for x in xs))
        assert max(abs(y) for y in ys) <= reach, (name, max(abs(y) for y in ys))
        assert max(zs) <= height, (name, max(zs))
        assert 200 < len(tris) < 40000, (name, len(tris))
        stats[name] = dict(vertices=len(verts), triangles=len(tris), size_cm=[round(v, 1) for v in size])
        print('BUILDINGS_GEOMETRY ' + json.dumps(dict(name=name, **stats[name])))
    print('BUILDINGS_GEOMETRY COMPLETE assets=3')
    return stats

def create():
    import unreal as u
    mel = u.MaterialEditingLibrary
    eal = u.EditorAssetLibrary
    assets = u.AssetToolsHelpers.get_asset_tools()
    matpath = PKG + '/M_VillageBuilding_Surface'
    mat = eal.load_asset(matpath) if eal.does_asset_exist(matpath) else None
    if not mat:
        mat = assets.create_asset('M_VillageBuilding_Surface', PKG, u.Material, u.MaterialFactoryNew())
        vc = mel.create_material_expression(mat, u.MaterialExpressionVertexColor, -640, 0)
        wp = mel.create_material_expression(mat, u.MaterialExpressionWorldPosition, -640, 220)
        custom = mel.create_material_expression(mat, u.MaterialExpressionCustom, -280, 0)
        custom.set_editor_property('output_type', u.CustomMaterialOutputType.CMOT_FLOAT3)
        inputs = []
        for name in ['P', 'C', 'R']:
            ci = u.CustomInput()
            ci.set_editor_property('input_name', name)
            inputs.append(ci)
        custom.set_editor_property('inputs', inputs)
        custom.set_editor_property('code', """
            float grain=sin(P.x*1.6+sin(P.z*0.9))*sin(P.y*1.9-P.z*1.3);
            float wood=sin(P.x*.24+3*sin(P.z*2.7+sin(P.x*.047)))*sin(P.z*5.2);
            float ashlar=sin(floor(P.x*.04)*1.7+floor(P.z*.06)*2.1);
            float tile=sin(P.x*.55+P.y*2.4)*sin(P.z*3.1);
            float isWood=step(.80,R)*(1-step(.88,R));
            float isStone=step(.70,R)*(1-step(.80,R));
            float isTile=step(.55,R)*(1-step(.70,R));
            float n=lerp(grain, wood, isWood);
            n=lerp(n, ashlar, isStone);
            n=lerp(n, tile, isTile);
            return C*(0.84+0.16*n);
        """)
        assert mel.connect_material_expressions(wp, '', custom, 'P')
        assert mel.connect_material_expressions(vc, '', custom, 'C')
        assert mel.connect_material_expressions(vc, 'A', custom, 'R')
        assert mel.connect_material_property(custom, '', u.MaterialProperty.MP_BASE_COLOR)
        assert mel.connect_material_property(vc, 'A', u.MaterialProperty.MP_ROUGHNESS)
        metal = mel.create_material_expression(mat, u.MaterialExpressionCustom, -270, 340)
        metal.set_editor_property('output_type', u.CustomMaterialOutputType.CMOT_FLOAT1)
        ci = u.CustomInput()
        ci.set_editor_property('input_name', 'R')
        metal.set_editor_property('inputs', [ci])
        metal.set_editor_property('code', 'return 1-step(0.5,R);')
        assert mel.connect_material_expressions(vc, 'A', metal, 'R')
        assert mel.connect_material_property(metal, '', u.MaterialProperty.MP_METALLIC)
        errors = list(mel.recompile_material(mat))
        assert not errors, errors
        eal.set_metadata_tag(mat, 'Recipe', VERSION)
        assert eal.save_asset(matpath)
    stats = {}
    for name, built in geometry().items():
        verts, tris, colors, normals = built
        path = PKG + '/' + name
        asset = eal.load_asset(path) if eal.does_asset_exist(path) else None
        if asset and os.environ.get('ANASTASIS_BUILDINGS_REBUILD', '0') == '1':
            assert path.startswith(PKG + '/SM_') and name in NAMES
            assert eal.get_metadata_tag(asset, 'Recipe') == VERSION
            assert eal.delete_asset(path), 'Owned asset rebuild failed'
            asset = None
        if asset:
            assert eal.get_metadata_tag(asset, 'Recipe') == VERSION, 'Existing asset belongs to a different recipe'
            assert asset.get_num_triangles(0) == len(tris), 'Existing geometry mismatch'
        else:
            b = u.GeometryScriptSimpleMeshBuffers()
            b.vertices = [u.Vector(*p) for p in verts]
            b.triangles = [u.IntVector(*t) for t in tris]
            b.normals = [u.Vector(*n) for n in normals]
            b.vertex_colors = [u.LinearColor(*c) for c in colors]
            b.uv0 = [u.Vector2D(p[0] / 100, p[2] / 100) for p in verts]
            dyn = u.DynamicMesh()
            u.GeometryScript_MeshEdits.append_buffers_to_mesh(dyn, b)
            opts = u.GeometryScriptCreateNewStaticMeshAssetOptions()
            opts.enable_recompute_normals = False
            opts.enable_recompute_tangents = True
            opts.enable_nanite = False
            asset, outcome = u.GeometryScript_NewAssetUtils.create_new_static_mesh_asset_from_mesh(dyn, path, opts)
            assert asset, str(outcome)
            asset.set_material(0, mat)
            collision = u.get_editor_subsystem(u.StaticMeshEditorSubsystem).add_simple_collisions(asset, u.ScriptCollisionShapeType.NDOP26)
            assert collision >= 0, 'collision creation failed'
            eal.set_metadata_tag(asset, 'Recipe', VERSION)
            assert eal.save_asset(path)
        bounds = asset.get_bounding_box()
        stats[name] = dict(
            path=path, vertices=len(verts), triangles=len(tris),
            size_cm=[bounds.max.x - bounds.min.x, bounds.max.y - bounds.min.y, bounds.max.z - bounds.min.z],
            pivot='authored centre, ground at z=0', material=mat.get_path_name())
        u.log('BUILDINGS ASSET ' + json.dumps(stats[name]))
    u.log('BUILDINGS COMPLETE assets=3')
    return stats

def main():
    import unreal
    try:
        create()
    except BaseException:
        import traceback
        unreal.log_error(traceback.format_exc())
        raise
    finally:
        unreal.SystemLibrary.quit_editor()

if __name__ == '__main__':
    try:
        import unreal  # present only inside the editor
        in_editor = True
    except ImportError:
        in_editor = False
    if in_editor:
        main()
    else:
        validate()
