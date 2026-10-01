"""AAA visual target lab. Isolated lookdev, 30 m.

Run: tools/unreal/aaa-visual-lab.ps1 -OutDir <new folder>

Writes only /Game/Anastasis/LookDev/AAA_Lab.
Does not save Lvl_AnastasisSlice, does not edit existing materials or meshes,
does not change project renderer settings.
"""
import json
import math
import os
import traceback

import unreal

PKG = '/Game/Anastasis/LookDev/AAA_Lab'
LEVEL = PKG + '/Lvl_AAA_VisualLab'
VERSION = 'aaa-visual-lab-v2'
TEX_PKG = '/Game/Anastasis/Materials/GroundTextures'
# Famille et taille reelle, celles de ground-textures.py. Lecture seule.
PHOTO_FAMILY = {
    'MI_AAA_Soil': ('Worked', 130.0),
    'MI_AAA_Stone': ('Rock', 300.0),
    'MI_AAA_Contact': ('Worked', 130.0),
}
OUT = os.environ.get('ANASTASIS_AAA_LAB_OUT', '')

CVARS = [
    'r.DynamicGlobalIlluminationMethod',
    'r.ReflectionMethod',
    'r.Shadow.Virtual.Enable',
    'r.RayTracing',
    'r.Lumen.HardwareRayTracing',
    'r.Lumen.HardwareRayTracing.LightingMode',
    'r.Nanite',
    'r.Nanite.ProjectEnabled',
    'r.AntiAliasingMethod',
    'r.TemporalAA.Upsampling',
    'r.ScreenPercentage',
    'r.VirtualTextures',
    'r.VT.Enable',
    'r.Streaming.PoolSize',
    'r.MaterialQualityLevel',
    'r.ForwardShading',
    'r.Substrate',
    'r.GenerateMeshDistanceFields',
    'r.AllowStaticLighting',
    'r.ContactShadows',
    'r.AmbientOcclusionLevels',
    'r.Shadow.CSM.MaxCascades',
    'r.VolumetricFog',
    'foliage.DensityScale',
    'r.Shadow.Virtual.OnePassProjection',
]


def log(msg):
    unreal.log('AAA_LAB ' + str(msg))


def dump(name, payload):
    if not OUT:
        log('NO_OUT ' + name)
        return
    path = os.path.join(OUT, name)
    with open(path, 'w', encoding='utf-8') as handle:
        json.dump(payload, handle, indent=2, default=str)
    log('WROTE ' + path)


def read_cvar(name):
    try:
        var = unreal.ConsoleManager.get_console_variable(name)
    except Exception as exc:
        return 'ERR %s' % exc
    if var is None:
        return None
    for meth in ('get_string', 'get_float', 'get_int'):
        fn = getattr(var, meth, None)
        if fn is None:
            continue
        try:
            return fn()
        except Exception:
            continue
    return str(var)


def classify(path, name, tris, lods, nanite, height_cm):
    if '/LookDev/AAA_Lab/' in path.replace('\\', '/'):
        return 'LAB'
    n = name.lower()
    cards = ('grass', 'reed', 'sedge', 'heather', 'tuft', 'heath')
    if any(k in n for k in cards):
        return 'C'
    if 'tree' in n or 'sapling' in n or 'bush' in n:
        if tris is not None and tris < 800:
            return 'D'
        if lods and lods >= 3 and tris and tris >= 4000:
            return 'B'
        return 'C'
    solid = ('rock', 'lithos', 'ruin', 'well', 'house', 'granary', 'shelter', 'chest', 'amphora', 'cauldron', 'drift', 'stump', 'log')
    if any(k in n for k in solid):
        if nanite is True and tris and tris >= 15000:
            return 'B'
        if tris and tris >= 1500:
            return 'B'
        return 'C'
    if tris is not None and tris < 400 and height_cm and height_cm > 50:
        return 'D'
    return 'C'


def audit_assets():
    rows = []
    for path in unreal.EditorAssetLibrary.list_assets('/Game/Anastasis', True, False):
        if '/Characters/' in path or '/Maps/' in path or '/LookDev/' in path:
            continue
        short = path.rsplit('/', 1)[-1].split('.')[0]
        if not short.startswith('SM_'):
            continue
        mesh = unreal.EditorAssetLibrary.load_asset(path)
        if mesh is None:
            rows.append(dict(path=path, error='load_failed', grade='C'))
            continue
        try:
            tris = mesh.get_num_triangles(0)
            lods = mesh.get_num_lods()
        except Exception as exc:
            tris, lods = None, None
            log('STATS %s %s' % (path, exc))
        nanite = None
        try:
            nanite = bool(mesh.get_editor_property('nanite_settings').get_editor_property('enabled'))
        except Exception as exc:
            nanite = 'unread:%s' % type(exc).__name__
        bounds = mesh.get_bounding_box()
        size = [
            round(bounds.max.x - bounds.min.x, 1),
            round(bounds.max.y - bounds.min.y, 1),
            round(bounds.max.z - bounds.min.z, 1),
        ]
        materials = []
        try:
            for slot in mesh.get_editor_property('static_materials'):
                mat = slot.get_editor_property('material_interface')
                materials.append(mat.get_path_name() if mat else None)
        except Exception:
            pass
        grade = classify(path, mesh.get_name(), tris, lods, nanite is True, size[2])
        rows.append(dict(
            path=path, name=mesh.get_name(), triangles_lod0=tris, lods=lods,
            nanite=nanite, size_cm=size, materials=materials, grade=grade,
        ))
    rows.sort(key=lambda row: row.get('path', ''))
    return rows


def audit_materials(paths):
    found = []
    for path in paths:
        mat = unreal.EditorAssetLibrary.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else None
        if mat is None:
            found.append(dict(path=path, present=False))
            continue
        textures = []
        try:
            for tex in unreal.MaterialEditingLibrary.get_used_textures(mat):
                if tex is None:
                    continue
                size = None
                for reader in (
                    lambda t: [t.blueprint_get_size_x(), t.blueprint_get_size_y()],
                    lambda t: [t.get_size_x(), t.get_size_y()],
                ):
                    try:
                        size = reader(tex)
                        break
                    except Exception:
                        continue
                textures.append(dict(name=tex.get_name(), path=tex.get_path_name(), size=size))
        except Exception as exc:
            textures.append(dict(error=str(exc)))
        found.append(dict(path=path, present=True, textures=textures, kind=mat.get_class().get_name()))
    return found


class Mesh(object):
    def __init__(self):
        self.v = []
        self.t = []
        self.c = []
        self.uv = []

    def vert(self, p, col, uv):
        self.v.append((float(p[0]), float(p[1]), float(p[2])))
        self.c.append(col)
        self.uv.append((float(uv[0]), float(uv[1])))
        return len(self.v) - 1

    def quad(self, pts, cols, uvs):
        ids = [self.vert(p, c, u) for p, c, u in zip(pts, cols, uvs)]
        self.t.extend([(ids[0], ids[1], ids[2]), (ids[0], ids[2], ids[3])])

    def finish(self):
        return self.v, self.t, self.c, self.uv


def ground_z(x, y):
    z = 10.0 * math.sin(x * 0.0041) * math.cos(y * 0.0034)
    z += 5.0 * math.sin(x * 0.011 + y * 0.008)
    if abs(x) < 190.0 and -250.0 < y < 950.0:
        z -= 4.0
    return z


def col4(r, g, b, a=1.0):
    return (r, g, b, a)


def build_ground():
    mesh = Mesh()
    n = 70
    half = 1500.0
    pts = []
    cols = []
    for j in range(n + 1):
        row_p = []
        row_c = []
        for i in range(n + 1):
            x = -half + 3000.0 * i / n
            y = -half + 3000.0 * j / n
            z = ground_z(x, y)
            dirt = 0.08
            wet = 0.0
            moss = 0.0
            if abs(x) < 260.0 and -280.0 < y < 1000.0:
                edge = min(1.0, abs(abs(x) - 120.0) / 90.0)
                dirt = 0.12 + 0.4 * edge
                moss = 0.08
            dx, dy = x - 30.0, y + 480.0
            dist = math.sqrt(dx * dx + dy * dy)
            if dist < 210.0:
                wet = 1.0 - dist / 210.0
            if -40.0 < x < 460.0 and -180.0 < y < 40.0:
                dirt = max(dirt, 0.55)
                moss = max(moss, 0.35)
            row_p.append((x, y, z))
            row_c.append(col4(dirt, wet, moss))
        pts.append(row_p)
        cols.append(row_c)
    # A single sheet stayed invisible in the lit capture (exact 0,0,0 under the props).
    # A thin slab has a front face from above whichever winding the renderer keeps.
    thick = 8.0
    for j in range(n):
        for i in range(n):
            p00, p10 = pts[j][i], pts[j][i + 1]
            p11, p01 = pts[j + 1][i + 1], pts[j + 1][i]
            c00, c10 = cols[j][i], cols[j][i + 1]
            c11, c01 = cols[j + 1][i + 1], cols[j + 1][i]

            def drop(p):
                return (p[0], p[1], p[2] - thick)

            top = [p00, p10, p11, p01]
            bot = [drop(p00), drop(p01), drop(p11), drop(p10)]
            mesh.quad(top, [c00, c10, c11, c01], [(p[0] / 100.0, p[1] / 100.0) for p in top])
            mesh.quad(bot, [c00, c01, c11, c10], [(p[0] / 100.0, p[1] / 100.0) for p in bot])
        # south / north skirts so the rim is a real edge, not a paper sheet
        for edge in (
            (pts[j][0], pts[j + 1][0], cols[j][0], cols[j + 1][0]),
            (pts[j + 1][n], pts[j][n], cols[j + 1][n], cols[j][n]),
        ):
            a, b, ca, cb = edge
            da, db = (a[0], a[1], a[2] - thick), (b[0], b[1], b[2] - thick)
            mesh.quad([a, b, db, da], [ca, cb, cb, ca], [(a[0] / 100.0, a[2] / 100.0), (b[0] / 100.0, b[2] / 100.0), (b[0] / 100.0, db[2] / 100.0), (a[0] / 100.0, da[2] / 100.0)])
    for i in range(n):
        for edge in (
            (pts[0][i + 1], pts[0][i], cols[0][i + 1], cols[0][i]),
            (pts[n][i], pts[n][i + 1], cols[n][i], cols[n][i + 1]),
        ):
            a, b, ca, cb = edge
            da, db = (a[0], a[1], a[2] - thick), (b[0], b[1], b[2] - thick)
            mesh.quad([a, b, db, da], [ca, cb, cb, ca], [(a[1] / 100.0, a[2] / 100.0), (b[1] / 100.0, b[2] / 100.0), (b[1] / 100.0, db[2] / 100.0), (a[1] / 100.0, da[2] / 100.0)])
    return mesh.finish()


def build_contact():
    """Pied de mur, bout de poutre, flaque. Mesh, pas un decal : DecalBlendMode est protege.
    La couleur de sommet du sol fait une case d'environ 43 cm, trop large pour un contact.
    """
    mesh = Mesh()
    thick = 3.0
    lift = 1.6

    def oval(cx, cy, rx, ry, yaw_deg, color, nu=10, nv=6):
        yaw = math.radians(yaw_deg)
        co, sn = math.cos(yaw), math.sin(yaw)
        pts = []
        for j in range(nv + 1):
            v = -1.0 + 2.0 * j / nv
            row = []
            for i in range(nu + 1):
                u = -1.0 + 2.0 * i / nu
                lx, ly = u * rx, v * ry
                x = cx + lx * co - ly * sn
                y = cy + lx * sn + ly * co
                row.append((x, y, ground_z(x, y) + lift))
            pts.append(row)
        for j in range(nv):
            for i in range(nu):
                p00, p10 = pts[j][i], pts[j][i + 1]
                p11, p01 = pts[j + 1][i + 1], pts[j + 1][i]
                mx = (p00[0] + p11[0]) * 0.5
                my = (p00[1] + p11[1]) * 0.5
                dx, dy = mx - cx, my - cy
                lx = dx * co + dy * sn
                ly = -dx * sn + dy * co
                if (lx / rx) ** 2 + (ly / ry) ** 2 > 1.02:
                    continue
                top = [p00, p10, p11, p01]
                bot = [(p[0], p[1], p[2] - thick) for p in (p00, p01, p11, p10)]
                mesh.quad(top, [color] * 4, [(p[0] / 100.0, p[1] / 100.0) for p in top])
                mesh.quad(bot, [color] * 4, [(p[0] / 100.0, p[1] / 100.0) for p in bot])

    oval(217, -58, 96, 18, -8, col4(1.0, 0.0, 0.9))
    oval(-65, -354, 26, 16, 18, col4(0.95, 0.2, 0.15))
    oval(30, -480, 58, 40, 14, col4(0.12, 1.0, 0.04))
    return mesh.finish()


def build_stone():
    mesh = Mesh()
    nu, nv = 48, 24
    rings = []
    for j in range(nv + 1):
        v = j / nv
        ring = []
        z = 78.0 * math.sin(v * math.pi * 0.5)
        rad = 70.0 * math.cos(v * math.pi * 0.5)
        rad_y = rad * 0.78
        for i in range(nu):
            u = i / nu
            ang = math.tau * u
            wobble = 1.0 + 0.16 * math.sin(ang * 3.0 + v * 5.0) + 0.07 * math.sin(ang * 7.0 - v * 2.0)
            # One chipped face, so the silhouette is not a smooth lump at 2 m.
            if 0.55 < u < 0.78 and v < 0.62:
                notch = math.sin((u - 0.55) / 0.23 * math.pi) * math.sin(min(v / 0.62, 1.0) * math.pi)
                wobble -= 0.28 * notch
            x = math.cos(ang) * rad * wobble
            y = math.sin(ang) * rad_y * wobble
            if v > 0.15:
                x += 6.0 * math.sin(ang * 2.0 + z * 0.05)
                y += 4.0 * math.cos(ang * 2.0)
            moss = 0.85 if (y < -8.0 and z < 36.0) else (0.25 if z < 18.0 else 0.0)
            dirt = 0.45 if z < 12.0 else 0.05
            ring.append(((x, y, max(0.0, z)), col4(dirt, 0.15 if y < 0 and z < 20 else 0.0, moss), (x / 100.0, z / 100.0)))
        rings.append(ring)
    for j in range(nv):
        for i in range(nu):
            k = (i + 1) % nu
            a, b = rings[j][i], rings[j][k]
            c, d = rings[j + 1][k], rings[j + 1][i]
            mesh.quad([a[0], b[0], c[0], d[0]], [a[1], b[1], c[1], d[1]], [a[2], b[2], c[2], d[2]])
    return mesh.finish()


def build_wall():
    mesh = Mesh()
    planks = 11
    plank_w = 16.0
    gap = 1.4
    height = 210.0
    thick = 8.0
    segs = 8
    total = planks * plank_w + (planks - 1) * gap
    x0 = -total * 0.5
    for p in range(planks):
        x_left = x0 + p * (plank_w + gap)
        plank_h = height * (0.42 if p == 5 else 1.0)
        cup = 0.2 * math.sin(p * 1.7)
        for s in range(segs):
            z0 = plank_h * s / segs
            z1 = plank_h * (s + 1) / segs
            warp0 = cup * math.sin(math.pi * s / segs)
            warp1 = cup * math.sin(math.pi * (s + 1) / segs)
            y_front = -thick * 0.5
            face = [
                (x_left, y_front + warp0, z0),
                (x_left + plank_w, y_front + warp0 * 0.8, z0),
                (x_left + plank_w, y_front + warp1 * 0.8, z1),
                (x_left, y_front + warp1, z1),
            ]
            def paint(z, plank=p):
                moss = 0.75 if z < 28.0 else (0.2 if z < 55.0 else 0.0)
                dirt = 0.7 if z < 22.0 else (0.18 if z > height - 18.0 else 0.0)
                dirt += 0.03 + 0.05 * math.sin(plank * 1.7)
                wet = 0.35 if z < 16.0 else 0.0
                return col4(dirt, wet, moss)
            cols = [paint(pt[2]) for pt in face]
            uvs = [(pt[0] / 100.0, pt[2] / 100.0) for pt in face]
            mesh.quad(face, cols, uvs)
            back = [
                (x_left + plank_w, thick * 0.5, z0),
                (x_left, thick * 0.5, z0),
                (x_left, thick * 0.5, z1),
                (x_left + plank_w, thick * 0.5, z1),
            ]
            mesh.quad(back, cols, [(pt[0] / 100.0, pt[2] / 100.0) for pt in back])
        # thin side edges so the plank reads as a board, not a card
        for side, x in ((0, x_left), (1, x_left + plank_w)):
            edge = [
                (x, thick * 0.5, 0),
                (x, -thick * 0.5, 0),
                (x, -thick * 0.5, plank_h),
                (x, thick * 0.5, plank_h),
            ]
            if side == 1:
                edge = [edge[1], edge[0], edge[3], edge[2]]
            mesh.quad(edge, [col4(0.2, 0.0, 0.1)] * 4, [(0, 0), (0.08, 0), (0.08, 2.1), (0, 2.1)])
        top = [
            (x_left, -thick * 0.5, plank_h),
            (x_left + plank_w, -thick * 0.5, plank_h),
            (x_left + plank_w, thick * 0.5, plank_h),
            (x_left, thick * 0.5, plank_h),
        ]
        mesh.quad(top, [col4(0.22, 0.0, 0.04)] * 4, [
            (x_left / 100.0, 0.0), ((x_left + plank_w) / 100.0, 0.0),
            ((x_left + plank_w) / 100.0, 0.08), (x_left / 100.0, 0.08),
        ])
    return mesh.finish()


def build_beam():
    mesh = Mesh()
    length, w, h = 220.0, 16.0, 14.0
    segs = 12
    rings = []
    for s in range(segs + 1):
        t = s / float(segs)
        rings.append((-length * 0.5 + length * t, 2.2 * math.sin(math.pi * t)))
    y0, y1 = -w * 0.5, w * 0.5
    for s in range(segs):
        x0, sag0 = rings[s]
        x1, sag1 = rings[s + 1]
        z0a, z1a = sag0, sag0 + h
        z0b, z1b = sag1, sag1 + h
        col = col4(0.35 if s in (0, segs - 1) else 0.08, 0.25 if s < 2 else 0.0, 0.12)
        faces = [
            [(x0, y0, z0a), (x1, y0, z0b), (x1, y0, z1b), (x0, y0, z1a)],
            [(x1, y1, z0b), (x0, y1, z0a), (x0, y1, z1a), (x1, y1, z1b)],
            [(x0, y0, z1a), (x1, y0, z1b), (x1, y1, z1b), (x0, y1, z1a)],
            [(x0, y1, z0a), (x1, y1, z0b), (x1, y0, z0b), (x0, y0, z0a)],
        ]
        for face in faces:
            mesh.quad(face, [col] * 4, [(pt[0] / 100.0, pt[2] / 100.0) for pt in face])
    x0, sag0 = rings[0]
    x1, sag1 = rings[-1]
    caps = [
        [(x0, y1, sag0), (x0, y0, sag0), (x0, y0, sag0 + h), (x0, y1, sag0 + h)],
        [(x1, y0, sag1), (x1, y1, sag1), (x1, y1, sag1 + h), (x1, y0, sag1 + h)],
    ]
    for face in caps:
        mesh.quad(face, [col4(0.3, 0.15, 0.1)] * 4, [(0, 0), (0.16, 0), (0.16, 0.14), (0, 0.14)])
    return mesh.finish()


def build_path():
    mesh = Mesh()
    slabs = [
        (-70, -80, 62, 48, 7),
        (10, 40, 74, 52, 8),
        (-30, 180, 58, 44, 6),
        (40, 320, 80, 50, 9),
        (-50, 470, 66, 46, 7),
        (20, 640, 70, 55, 8),
    ]
    for x, y, w, d, h in slabs:
        yaw = 0.15 * math.sin(x * 0.02 + y * 0.01)
        def rot(px, py):
            return (x + px * math.cos(yaw) - py * math.sin(yaw), y + px * math.sin(yaw) + py * math.cos(yaw))
        corners = [rot(-w / 2, -d / 2), rot(w / 2, -d / 2), rot(w / 2, d / 2), rot(-w / 2, d / 2)]
        top = [(px, py, ground_z(px, py) + h) for px, py in corners]
        moss = 0.2 + 0.15 * math.sin(x)
        mesh.quad(top, [col4(0.12, 0.05, moss)] * 4, [(c[0] / 100, c[1] / 100) for c in top])
    return mesh.finish()


def build_scale():
    mesh = Mesh()
    grey = col4(0.0, 0.0, 0.0)

    def box(x0, x1, y0, y1, z0, z1):
        faces = [
            [(x0, y0, z1), (x1, y0, z1), (x1, y1, z1), (x0, y1, z1)],
            [(x0, y0, z0), (x0, y1, z0), (x1, y1, z0), (x1, y0, z0)],
            [(x0, y0, z0), (x1, y0, z0), (x1, y0, z1), (x0, y0, z1)],
            [(x1, y1, z0), (x0, y1, z0), (x0, y1, z1), (x1, y1, z1)],
            [(x0, y0, z0), (x0, y0, z1), (x0, y1, z1), (x0, y1, z0)],
            [(x1, y1, z0), (x1, y1, z1), (x1, y0, z1), (x1, y0, z0)],
        ]
        for face in faces:
            mesh.quad(face, [grey] * 4, [(0, 0), (1, 0), (1, 1), (0, 1)])

    box(-8, 8, -7, 7, 0, 88)
    box(-6, 6, -6, 6, 88, 142)
    box(-11, 11, -8, 8, 104, 128)
    box(-9, 9, -8, 8, 142, 180)
    return mesh.finish()


def scalar(mat, mel, name, default, x, y):
    node = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, x, y)
    node.set_editor_property('parameter_name', name)
    node.set_editor_property('default_value', float(default))
    try:
        node.set_editor_property('group', 'AAA')
    except Exception:
        pass
    return node


def vector_param(mat, mel, name, rgb, x, y):
    node = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, x, y)
    node.set_editor_property('parameter_name', name)
    node.set_editor_property('default_value', unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1))
    try:
        node.set_editor_property('group', 'AAA')
    except Exception:
        pass
    return node


def make_custom(mat, mel, code, out_type, x, y, links, extra=None):
    node = mel.create_material_expression(mat, unreal.MaterialExpressionCustom, x, y)
    node.set_editor_property('output_type', out_type)
    node.set_editor_property('code', code)
    inputs = []
    for name in links:
        pin = unreal.CustomInput()
        pin.set_editor_property('input_name', name)
        inputs.append(pin)
    node.set_editor_property('inputs', inputs)
    if extra:
        outs = []
        for name, out_t in extra:
            pin = unreal.CustomOutput()
            pin.set_editor_property('output_name', name)
            pin.set_editor_property('output_type', out_t)
            outs.append(pin)
        node.set_editor_property('additional_outputs', outs)
    for name, (src, src_pin) in links.items():
        if not mel.connect_material_expressions(src, src_pin, node, name):
            raise RuntimeError('material link failed: %s' % name)
    return node


COLOR_CODE = """
float macro = sin(dot(P.xy, float2(MacroScale, MacroScale * 0.67)));
float meso = sin(dot(P.xy, float2(MacroScale * 3.7, -MacroScale * 2.3)) + macro);
float fiber = sin(P.z * 2.4 + sin(P.x * 0.15));
float3 col = lerp(Base.rgb, Tint.rgb, saturate(macro * 0.5 + 0.5) * ColorVariation);
col = lerp(col, col * lerp(0.94, 1.05, fiber * 0.5 + 0.5), WoodAmount);
float dirt = saturate(Dirt + VC.r * 0.85 + meso * 0.05);
col = lerp(col, DirtColor.rgb, dirt * 0.55);
float moss = saturate(VC.b * MossAmount);
col = lerp(col, MossColor.rgb, moss);
float wet = saturate(Wetness + VC.g);
col = lerp(col, col * float3(0.55, 0.66, 0.62), wet * 0.7);
return col;
"""

# Photos T_Ground_* deja importees. AH albedo de detail (moyenne 0.4), NR normale
# DirectX + rugosite + occlusion. Normale en espace monde : le sinus micro, a 1,7 m,
# dessinait une onde. Le bois sans photo garde un fil court, pas cette onde.
PHOTO_CODE = """
float dist = distance(P, Cam);
float fade = saturate((dist - FadeStart) / max(FadeEnd - FadeStart, 1.0));
float live = saturate(UsePhoto) * (1.0 - fade);
float3 Nn = normalize(N);
float t = 1.0 / max(TexSizeCm, 1.0);
float3 sg = float3(Nn.x >= 0.0 ? 1.0 : -1.0, Nn.y >= 0.0 ? 1.0 : -1.0, Nn.z >= 0.0 ? 1.0 : -1.0);
float3 tw = pow(abs(Nn), 8.0);
tw /= max(tw.x + tw.y + tw.z, 1e-4);
float3 dPx = ddx(P);
float3 dPy = ddy(P);
float3 alb = 0;
float4 nr = 0;
float3 nw = 0;
{
    float w = tw.x;
    float2 uv = float2(P.z * sg.x, P.y) * t;
    float2 gx = float2(dPx.z * sg.x, dPx.y) * t;
    float2 gy = float2(dPy.z * sg.x, dPy.y) * t;
    float4 a = Texture2DSampleGrad(TexAH, TexAHSampler, uv, gx, gy);
    float4 n = Texture2DSampleGrad(TexNR, TexNRSampler, uv, gx, gy);
    float2 xy0 = n.xy * 2.0 - 1.0;
    float z = sqrt(saturate(1.0 - dot(xy0, xy0)));
    float2 xy = xy0 * NormalStrength;
    xy.x *= sg.x;
    nw += float3(xy + Nn.zy, z * Nn.x).zyx * w;
    alb += a.rgb * w;
    nr += n * w;
}
{
    float w = tw.y;
    float2 uv = float2(P.x * sg.y, P.z) * t;
    float2 gx = float2(dPx.x * sg.y, dPx.z) * t;
    float2 gy = float2(dPy.x * sg.y, dPy.z) * t;
    float4 a = Texture2DSampleGrad(TexAH, TexAHSampler, uv, gx, gy);
    float4 n = Texture2DSampleGrad(TexNR, TexNRSampler, uv, gx, gy);
    float2 xy0 = n.xy * 2.0 - 1.0;
    float z = sqrt(saturate(1.0 - dot(xy0, xy0)));
    float2 xy = xy0 * NormalStrength;
    xy.x *= sg.y;
    nw += float3(xy + Nn.xz, z * Nn.y).xzy * w;
    alb += a.rgb * w;
    nr += n * w;
}
{
    float w = tw.z;
    float2 uv = float2(P.x * sg.z, P.y) * t;
    float2 gx = float2(dPx.x * sg.z, dPx.y) * t;
    float2 gy = float2(dPy.x * sg.z, dPy.y) * t;
    float4 a = Texture2DSampleGrad(TexAH, TexAHSampler, uv, gx, gy);
    float4 n = Texture2DSampleGrad(TexNR, TexNRSampler, uv, gx, gy);
    float2 xy0 = n.xy * 2.0 - 1.0;
    float z = sqrt(saturate(1.0 - dot(xy0, xy0)));
    float2 xy = xy0 * NormalStrength;
    xy.x *= sg.z;
    nw += float3(xy + Nn.xy, z * Nn.z) * w;
    alb += a.rgb * w;
    nr += n * w;
}
alb *= 2.5;
nw = dot(nw, nw) > 1e-8 ? normalize(nw) : Nn;
float3 detail = lerp(float3(1.0, 1.0, 1.0), alb, saturate(AlbedoStrength));
float3 fiber = float3(sin(P.z * 2.2), 0.0, 0.0);
fiber = fiber - Nn * dot(fiber, Nn);
float amp = DetailStrength * saturate(WoodAmount) * (1.0 - fade) * 0.04;
float3 woodN = normalize(Nn + fiber * amp);
Normal = normalize(lerp(woodN, nw, live));
Rough = (nr.b - 0.5) * 2.0 * RoughnessPhoto * live;
AO = lerp(1.0, saturate(nr.a * 2.0), saturate(AoStrength) * live);
return lerp(float3(1.0, 1.0, 1.0), detail, live);
"""

ROUGH_CODE = """
float meso = sin(P.x * MacroScale * 5.0) * sin(P.y * MacroScale * 3.7);
float micro = sin(dot(P.xy, float2(MicroScale, MicroScale * 0.67)));
float wave = RoughnessVariation * (0.55 * micro + 0.45 * meso) * (1.0 - saturate(UsePhoto));
float r = Roughness + wave;
float wet = saturate(Wetness + VC.g);
r = lerp(r, min(r, 0.22), wet);
r = saturate(r + VC.r * 0.2 + PhotoRough);
return r;
"""

SPEC_CODE = """
float wet = saturate(Wetness + VC.g);
return lerp(0.45, 0.9, wet);
"""

def enum_of(enum, index, *names):
    for name in names:
        if hasattr(enum, name):
            return getattr(enum, name)
    return list(enum)[index]


def load_ground_tex(family, suffix):
    path = '%s/T_Ground_%s_%s' % (TEX_PKG, family, suffix)
    tex = unreal.EditorAssetLibrary.load_asset(path)
    if tex is None:
        raise RuntimeError('texture sol absente: ' + path)
    return tex


def texture_param(mat, mel, name, texture, sampler, x, y):
    node = mel.create_material_expression(mat, unreal.MaterialExpressionTextureObjectParameter, x, y)
    node.set_editor_property('parameter_name', name)
    node.set_editor_property('texture', texture)
    node.set_editor_property('sampler_type', sampler)
    try:
        node.set_editor_property('group', 'AAA')
    except Exception:
        pass
    return node


def build_master():
    mel = unreal.MaterialEditingLibrary
    assets = unreal.AssetToolsHelpers.get_asset_tools()
    mat = assets.create_asset('M_AAA_Lab_Surface', PKG, unreal.Material, unreal.MaterialFactoryNew())
    if mat is None or mat.get_name() != 'M_AAA_Lab_Surface':
        raise RuntimeError('master material was not created at the expected path')
    mat.set_editor_property('two_sided', True)
    mat.set_editor_property('tangent_space_normal', False)
    try:
        mat.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    except Exception as exc:
        log('shading model left default: %s' % exc)

    wp = mel.create_material_expression(mat, unreal.MaterialExpressionWorldPosition, -1800, 0)
    vc = mel.create_material_expression(mat, unreal.MaterialExpressionVertexColor, -1800, 200)
    cam = mel.create_material_expression(mat, unreal.MaterialExpressionCameraPositionWS, -1800, 400)
    nws = mel.create_material_expression(mat, unreal.MaterialExpressionVertexNormalWS, -1800, 560)
    sampler_color = enum_of(unreal.MaterialSamplerType, 0, 'SAMPLERTYPE_COLOR', 'SAMPLERTYPE_Color')
    sampler_linear = enum_of(unreal.MaterialSamplerType, 3, 'SAMPLERTYPE_LINEAR_COLOR', 'SAMPLERTYPE_LinearColor')
    tex_ah = texture_param(mat, mel, 'TexAH', load_ground_tex('Worked', 'AH'), sampler_color, -1800, 720)
    tex_nr = texture_param(mat, mel, 'TexNR', load_ground_tex('Worked', 'NR'), sampler_linear, -1800, 880)
    base = vector_param(mat, mel, 'BaseColor', (0.16, 0.13, 0.09), -1600, -200)
    tint = vector_param(mat, mel, 'TintB', (0.10, 0.11, 0.09), -1600, -80)
    dirt_c = vector_param(mat, mel, 'DirtColor', (0.05, 0.035, 0.02), -1600, 40)
    moss_c = vector_param(mat, mel, 'MossColor', (0.045, 0.07, 0.028), -1600, 160)
    names = [
        ('ColorVariation', 0.4), ('WoodAmount', 0.0), ('Dirt', 0.1), ('Wetness', 0.0),
        ('MossAmount', 0.3), ('MacroScale', 0.0015), ('MicroScale', 0.25),
        ('DetailStrength', 0.45), ('FadeStart', 400.0), ('FadeEnd', 2200.0),
        ('Roughness', 0.78), ('RoughnessVariation', 0.08), ('Metallic', 0.0), ('AoStrength', 0.35),
        ('UsePhoto', 0.0), ('TexSizeCm', 130.0), ('NormalStrength', 0.55),
        ('AlbedoStrength', 0.75), ('RoughnessPhoto', 0.12),
    ]
    params = {}
    for i, (name, default) in enumerate(names):
        params[name] = scalar(mat, mel, name, default, -1400, -400 + i * 70)

    color_links = {
        'P': (wp, ''), 'VC': (vc, ''),
        'Base': (base, ''), 'Tint': (tint, ''), 'DirtColor': (dirt_c, ''), 'MossColor': (moss_c, ''),
        'ColorVariation': (params['ColorVariation'], ''), 'WoodAmount': (params['WoodAmount'], ''),
        'Dirt': (params['Dirt'], ''), 'Wetness': (params['Wetness'], ''),
        'MossAmount': (params['MossAmount'], ''), 'MacroScale': (params['MacroScale'], ''),
    }
    color = make_custom(mat, mel, COLOR_CODE, unreal.CustomMaterialOutputType.CMOT_FLOAT3, -200, -200, color_links)
    float1 = unreal.CustomMaterialOutputType.CMOT_FLOAT1
    float3 = unreal.CustomMaterialOutputType.CMOT_FLOAT3
    photo = make_custom(mat, mel, PHOTO_CODE, float3, -200, 200, {
        'P': (wp, ''), 'Cam': (cam, ''), 'N': (nws, ''),
        'TexAH': (tex_ah, ''), 'TexNR': (tex_nr, ''),
        'UsePhoto': (params['UsePhoto'], ''), 'TexSizeCm': (params['TexSizeCm'], ''),
        'NormalStrength': (params['NormalStrength'], ''), 'AlbedoStrength': (params['AlbedoStrength'], ''),
        'RoughnessPhoto': (params['RoughnessPhoto'], ''), 'AoStrength': (params['AoStrength'], ''),
        'DetailStrength': (params['DetailStrength'], ''), 'WoodAmount': (params['WoodAmount'], ''),
        'FadeStart': (params['FadeStart'], ''), 'FadeEnd': (params['FadeEnd'], ''),
    }, extra=[('Normal', float3), ('Rough', float1), ('AO', float1)])
    tinted = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, 80, -200)
    if not mel.connect_material_expressions(color, '', tinted, 'A'):
        raise RuntimeError('material link failed: color')
    if not mel.connect_material_expressions(photo, '', tinted, 'B'):
        raise RuntimeError('material link failed: photo albedo')
    rough = make_custom(mat, mel, ROUGH_CODE, float1, -200, 500, {
        'P': (wp, ''), 'VC': (vc, ''), 'MicroScale': (params['MicroScale'], ''),
        'MacroScale': (params['MacroScale'], ''),
        'Roughness': (params['Roughness'], ''), 'RoughnessVariation': (params['RoughnessVariation'], ''),
        'Wetness': (params['Wetness'], ''), 'UsePhoto': (params['UsePhoto'], ''),
        'PhotoRough': (photo, 'Rough'),
    })
    spec = make_custom(mat, mel, SPEC_CODE, float1, -200, 760, {
        'VC': (vc, ''), 'Wetness': (params['Wetness'], ''),
    })
    assert mel.connect_material_property(tinted, '', unreal.MaterialProperty.MP_BASE_COLOR)
    assert mel.connect_material_property(photo, 'Normal', unreal.MaterialProperty.MP_NORMAL)
    assert mel.connect_material_property(rough, '', unreal.MaterialProperty.MP_ROUGHNESS)
    assert mel.connect_material_property(spec, '', unreal.MaterialProperty.MP_SPECULAR)
    assert mel.connect_material_property(params['Metallic'], '', unreal.MaterialProperty.MP_METALLIC)
    assert mel.connect_material_property(photo, 'AO', unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)
    errors = list(mel.recompile_material(mat))
    if errors:
        raise RuntimeError('master material compile: %s' % errors)
    unreal.EditorAssetLibrary.set_metadata_tag(mat, 'Recipe', VERSION)
    unreal.EditorAssetLibrary.save_asset(PKG + '/M_AAA_Lab_Surface')
    return mat


INSTANCES = {
    'MI_AAA_Soil': dict(
        BaseColor=(0.42, 0.30, 0.17), TintB=(0.30, 0.24, 0.15),
        DirtColor=(0.05, 0.034, 0.018), MossColor=(0.14, 0.16, 0.08),
        ColorVariation=0.32, WoodAmount=0.0, Dirt=0.06, Wetness=0.0, MossAmount=0.25,
        MacroScale=0.0035, MicroScale=2.4, DetailStrength=0.16, FadeStart=150.0, FadeEnd=500.0,
        Roughness=0.86, RoughnessVariation=0.10, Metallic=0.0, AoStrength=0.45,
        UsePhoto=1.0, TexSizeCm=130.0, NormalStrength=0.55, AlbedoStrength=0.8, RoughnessPhoto=0.12,
    ),
    'MI_AAA_Stone': dict(
        BaseColor=(0.46, 0.44, 0.40), TintB=(0.30, 0.28, 0.24),
        DirtColor=(0.06, 0.042, 0.026), MossColor=(0.05, 0.09, 0.035),
        ColorVariation=0.28, WoodAmount=0.0, Dirt=0.04, Wetness=0.0, MossAmount=1.0,
        MacroScale=0.012, MicroScale=1.8, DetailStrength=0.2, FadeStart=300.0, FadeEnd=1600.0,
        Roughness=0.70, RoughnessVariation=0.12, Metallic=0.0, AoStrength=0.45,
        UsePhoto=1.0, TexSizeCm=300.0, NormalStrength=0.4, AlbedoStrength=0.65, RoughnessPhoto=0.1,
    ),
    'MI_AAA_Wood': dict(
        BaseColor=(0.40, 0.22, 0.10), TintB=(0.24, 0.12, 0.05),
        DirtColor=(0.045, 0.030, 0.016), MossColor=(0.06, 0.07, 0.03),
        ColorVariation=0.18, WoodAmount=1.0, Dirt=0.04, Wetness=0.0, MossAmount=0.45,
        MacroScale=0.004, MicroScale=3.2, DetailStrength=0.12, FadeStart=180.0, FadeEnd=800.0,
        Roughness=0.58, RoughnessVariation=0.16, Metallic=0.0, AoStrength=0.3,
        UsePhoto=0.0, TexSizeCm=130.0, NormalStrength=0.0, AlbedoStrength=0.0, RoughnessPhoto=0.0,
    ),
    'MI_AAA_Scale': dict(
        BaseColor=(0.22, 0.22, 0.21), TintB=(0.22, 0.22, 0.21),
        DirtColor=(0.22, 0.22, 0.21), MossColor=(0.22, 0.22, 0.21),
        ColorVariation=0.0, WoodAmount=0.0, Dirt=0.0, Wetness=0.0, MossAmount=0.0,
        MacroScale=0.001, MicroScale=0.01, DetailStrength=0.0, Roughness=0.72,
        RoughnessVariation=0.0, Metallic=0.0, AoStrength=0.0,
        UsePhoto=0.0, TexSizeCm=130.0, NormalStrength=0.0, AlbedoStrength=0.0, RoughnessPhoto=0.0,
    ),
    'MI_AAA_Contact': dict(
        BaseColor=(0.14, 0.09, 0.05), TintB=(0.10, 0.07, 0.04),
        DirtColor=(0.04, 0.025, 0.014), MossColor=(0.08, 0.10, 0.04),
        ColorVariation=0.08, WoodAmount=0.0, Dirt=0.65, Wetness=0.0, MossAmount=0.8,
        MacroScale=0.003, MicroScale=1.0, DetailStrength=0.0, FadeStart=400.0, FadeEnd=2200.0,
        Roughness=0.72, RoughnessVariation=0.0, Metallic=0.0, AoStrength=0.55,
        UsePhoto=1.0, TexSizeCm=130.0, NormalStrength=0.35, AlbedoStrength=0.22, RoughnessPhoto=0.08,
    ),
}


def build_instances(master):
    mel = unreal.MaterialEditingLibrary
    assets = unreal.AssetToolsHelpers.get_asset_tools()
    made = {}
    for name, values in INSTANCES.items():
        mi = assets.create_asset(name, PKG, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        if mi is None:
            raise RuntimeError('instance failed: ' + name)
        mel.set_material_instance_parent(mi, master)
        for key, value in values.items():
            if isinstance(value, tuple):
                mel.set_material_instance_vector_parameter_value(
                    mi, key, unreal.LinearColor(value[0], value[1], value[2], 1))
            else:
                mel.set_material_instance_scalar_parameter_value(mi, key, float(value))
        family = PHOTO_FAMILY.get(name)
        if family:
            fam, _size = family
            for suffix, param in (('AH', 'TexAH'), ('NR', 'TexNR')):
                tex = load_ground_tex(fam, suffix)
                mel.set_material_instance_texture_parameter_value(mi, param, tex)
                try:
                    mi.set_texture_parameter_value_editor_only(param, tex)
                except Exception as exc:
                    log('TEX_EDITOR %s %s %s' % (name, param, exc))
                got = mel.get_material_instance_texture_parameter_value(mi, param)
                got_path = got.get_path_name() if got else ''
                if tex.get_path_name() not in got_path:
                    log('TEX_DEFAULT %s %s master keeps its texture, instance read %s' % (name, param, got_path or 'none'))
        unreal.EditorAssetLibrary.set_metadata_tag(mi, 'Recipe', VERSION)
        unreal.EditorAssetLibrary.save_asset(PKG + '/' + name)
        made[name] = mi
    return made


def mesh_normals(verts, tris):
    acc = [[0.0, 0.0, 0.0] for _ in verts]
    for i, j, k in tris:
        a, b, c = verts[i], verts[j], verts[k]
        e1 = (b[0] - a[0], b[1] - a[1], b[2] - a[2])
        e2 = (c[0] - a[0], c[1] - a[1], c[2] - a[2])
        fn = (
            e1[1] * e2[2] - e1[2] * e2[1],
            e1[2] * e2[0] - e1[0] * e2[2],
            e1[0] * e2[1] - e1[1] * e2[0],
        )
        for idx in (i, j, k):
            acc[idx][0] += fn[0]
            acc[idx][1] += fn[1]
            acc[idx][2] += fn[2]
    out = []
    for n in acc:
        length = math.sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]) or 1.0
        out.append((n[0] / length, n[1] / length, n[2] / length))
    return out


def commit_mesh(name, built, material, nanite):
    verts, tris, colors, uvs = built
    path = PKG + '/' + name
    buffers = unreal.GeometryScriptSimpleMeshBuffers()
    buffers.vertices = [unreal.Vector(*p) for p in verts]
    buffers.triangles = [unreal.IntVector(*t) for t in tris]
    buffers.normals = [unreal.Vector(*n) for n in mesh_normals(verts, tris)]
    buffers.vertex_colors = [unreal.LinearColor(*c) for c in colors]
    buffers.uv0 = [unreal.Vector2D(*u) for u in uvs]
    dyn = unreal.DynamicMesh()
    unreal.GeometryScript_MeshEdits.append_buffers_to_mesh(dyn, buffers)
    opts = unreal.GeometryScriptCreateNewStaticMeshAssetOptions()
    opts.enable_recompute_normals = False
    opts.enable_recompute_tangents = True
    opts.enable_nanite = bool(nanite)
    asset, outcome = unreal.GeometryScript_NewAssetUtils.create_new_static_mesh_asset_from_mesh(dyn, path, opts)
    used_nanite = bool(nanite)
    if not asset and nanite:
        dyn = unreal.DynamicMesh()
        unreal.GeometryScript_MeshEdits.append_buffers_to_mesh(dyn, buffers)
        opts.enable_nanite = False
        asset, outcome = unreal.GeometryScript_NewAssetUtils.create_new_static_mesh_asset_from_mesh(dyn, path, opts)
        used_nanite = False
    if not asset:
        raise RuntimeError('mesh %s failed: %s' % (name, outcome))
    asset.set_material(0, material)
    unreal.EditorAssetLibrary.set_metadata_tag(asset, 'Recipe', VERSION)
    unreal.EditorAssetLibrary.save_asset(path)
    asset_tris = asset.get_num_triangles(0)
    bounds = asset.get_bounding_box()
    log('MESH %s src_tris=%s asset_tris=%s nanite=%s bounds=(%.0f,%.0f,%.0f)-(%.0f,%.0f,%.0f)' % (
        name, len(tris), asset_tris, used_nanite,
        bounds.min.x, bounds.min.y, bounds.min.z, bounds.max.x, bounds.max.y, bounds.max.z))
    if asset_tris < 1:
        raise RuntimeError('mesh %s saved with no triangles' % name)
    return asset, used_nanite, asset_tris


def tag(actor, label, folder):
    actor.set_actor_label(label)
    actor.set_folder_path(folder)
    actor.tags = ['AAA_LAB']
    return actor


def set_prop(obj, name, value):
    try:
        obj.set_editor_property(name, value)
        return True
    except Exception as exc:
        log('PROP %s %s' % (name, exc))
        return False


def seat(path, x, y, yaw=0.0):
    mesh = unreal.EditorAssetLibrary.load_asset(path)
    if mesh is None:
        log('MISSING ' + path)
        return None
    bounds = mesh.get_bounding_box()
    z = ground_z(x, y) - bounds.min.z
    return mesh, unreal.Vector(x, y, z), unreal.Rotator(pitch=0, yaw=yaw, roll=0), [
        round(bounds.max.x - bounds.min.x, 1),
        round(bounds.max.y - bounds.min.y, 1),
        round(bounds.max.z - bounds.min.z, 1),
    ]


def build_level(instances, meshes):
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    if not les.new_level(LEVEL):
        raise RuntimeError('new_level failed')
    world = ues.get_editor_world()
    world_path = world.get_path_name()
    if 'AAA_Lab' not in world_path:
        raise RuntimeError('refusing to edit ' + world_path)
    log('LEVEL ' + world_path)

    def spawn(cls, loc, rot):
        actor = eas.spawn_actor_from_class(cls, loc, rot)
        if actor is None:
            raise RuntimeError('spawn failed %s' % cls)
        return actor

    def place_owned(label, mesh, material, x, y, yaw=0.0, z_extra=0.0, planted=True):
        z = ground_z(x, y) + z_extra if planted else z_extra
        actor = spawn(unreal.StaticMeshActor, unreal.Vector(x, y, z), unreal.Rotator(pitch=0, yaw=yaw, roll=0))
        tag(actor, label, 'AAA_LAB/Hero')
        comp = actor.static_mesh_component
        comp.set_static_mesh(mesh)
        comp.set_material(0, material)
        loc = actor.get_actor_location()
        log('PLACE %s (%.0f,%.0f,%.0f) tris=%s' % (
            label, loc.x, loc.y, loc.z, comp.static_mesh.get_num_triangles(0) if comp.static_mesh else 0))
        return actor

    place_owned('AAA_Ground', meshes['SM_AAA_Ground_30m'], instances['MI_AAA_Soil'], 0, 0, planted=False)
    place_owned('AAA_Contact', meshes['SM_AAA_Contact'], instances['MI_AAA_Contact'], 0, 0, planted=False)
    place_owned('AAA_Stone', meshes['SM_AAA_Stone_Hero'], instances['MI_AAA_Stone'], -180, -140)
    place_owned('AAA_Wall', meshes['SM_AAA_Timber_Wall'], instances['MI_AAA_Wood'], 220, -30, yaw=-8)
    place_owned('AAA_Beam', meshes['SM_AAA_Timber_Beam'], instances['MI_AAA_Wood'], 40, -320, yaw=18)
    place_owned('AAA_Path', meshes['SM_AAA_Path_Stones'], instances['MI_AAA_Stone'], 0, 0, planted=False)
    place_owned('AAA_Scale_180cm', meshes['SM_AAA_Scale_180cm'], instances['MI_AAA_Scale'], 80, -560)

    current = []
    for label, path, x, y, yaw in [
        ('CUR_Tree', '/Game/Anastasis/Vegetation/SM_Tree_Broadleaf_Understory_01', -650, 1050, 20),
        ('CUR_Rock', '/Game/Anastasis/Rock/SM_Rock_Boulder_01', -420, 620, 15),
        ('CUR_Ruin', '/Game/Anastasis/Architecture/SM_Ruin_Mur_01', 520, 700, -30),
        ('CUR_Lithos', '/Game/Anastasis/Lithos/SM_Lithos_Outcrop_01', 250, 980, 10),
        ('CUR_Wood', '/Game/Anastasis/Ecotone/SM_Ecotone_Driftwood_01', -300, 380, 40),
        ('CUR_Grass_A', '/Game/Anastasis/GroundCover/SM_Grass_MeadowTall_01', 140, 420, 0),
        ('CUR_Grass_B', '/Game/Anastasis/GroundCover/SM_Grass_MeadowTall_01', 210, 480, 35),
        ('CUR_Grass_C', '/Game/Anastasis/GroundCover/SM_Grass_MeadowTall_01', -90, 240, 10),
    ]:
        seated = seat(path, x, y, yaw)
        if seated is None:
            current.append(dict(label=label, path=path, placed=False))
            continue
        mesh, loc, rot, size = seated
        actor = spawn(unreal.StaticMeshActor, loc, rot)
        tag(actor, label, 'AAA_LAB/Current')
        actor.static_mesh_component.set_static_mesh(mesh)
        current.append(dict(label=label, path=path, placed=True, size_cm=size, location=[loc.x, loc.y, loc.z]))

    def place_height(label, path, x, y, yaw, height_cm, folder, material=None):
        mesh = unreal.EditorAssetLibrary.load_asset(path)
        if mesh is None:
            log('MISSING ' + path)
            current.append(dict(label=label, path=path, placed=False))
            return
        bounds = mesh.get_bounding_box()
        native = max(1.0, bounds.max.z - bounds.min.z)
        scale = float(height_cm) / native
        z = ground_z(x, y) - bounds.min.z * scale
        actor = spawn(unreal.StaticMeshActor, unreal.Vector(x, y, z), unreal.Rotator(pitch=0, yaw=yaw, roll=0))
        tag(actor, label, folder)
        actor.set_actor_scale3d(unreal.Vector(scale, scale, scale))
        comp = actor.static_mesh_component
        comp.set_static_mesh(mesh)
        if material is not None:
            for slot in range(comp.get_num_materials()):
                comp.set_material(slot, material)
        log('HERO %s h=%.0f scale=%.2f (%.0f,%.0f,%.0f)' % (label, height_cm, scale, x, y, z))
        current.append(dict(label=label, path=path, placed=True, height_cm=height_cm, scale=round(scale, 3), location=[x, y, round(z, 1)]))

    # Anneau proche, dans le cone de cam_a (48 deg). L'arbre du fond reste a 1 m.
    place_height('HERO_Tree', '/Game/Anastasis/Vegetation/SM_Tree_Broadleaf_Canopy_01', -310, -40, 15, 393.0, 'AAA_LAB/Hero')
    # Touffe : les memes cartes, empilees, pour que le premier plan ne soit plus un plan.
    grass = '/Game/Anastasis/GroundCover/SM_Grass_MeadowTall_01'
    clump = [(0, 0, 0, 1.15), (22, 8, 40, 0.9), (-18, 12, 15, 1.05), (8, -20, 70, 0.85),
             (-8, -16, 110, 1.2), (26, -6, 150, 0.75), (-24, -4, 200, 0.95), (4, 18, 250, 1.0)]
    for i, (dx, dy, yaw, sc) in enumerate(clump):
        place_height('HERO_Grass_%d' % i, grass, 160 + dx, -420 + dy, yaw, 97.0 * sc, 'AAA_LAB/Hero')
    # Une maison, pas le village : le shader du mur du labo, l'asset de production intact.
    place_height('HERO_House', '/Game/Anastasis/VillageBuildings/SM_House_Refuge_01', -600, 70, 30, 440.0, 'AAA_LAB/Hero', instances['MI_AAA_Wood'])

    sun = spawn(unreal.DirectionalLight, unreal.Vector(0, 0, 500), unreal.Rotator(pitch=-46, yaw=38, roll=0))
    tag(sun, 'AAA_Sun', 'AAA_LAB/Light')
    light = sun.get_component_by_class(unreal.DirectionalLightComponent)
    set_prop(light, 'intensity', 10.0)
    set_prop(light, 'cast_shadows', True)
    set_prop(light, 'contact_shadow_length', 0.2)
    set_prop(light, 'atmosphere_sun_light', True)

    sky = spawn(unreal.SkyLight, unreal.Vector(0, 0, 300), unreal.Rotator())
    tag(sky, 'AAA_SkyLight', 'AAA_LAB/Light')
    sky_comp = sky.get_component_by_class(unreal.SkyLightComponent)
    set_prop(sky_comp, 'mobility', unreal.ComponentMobility.MOVABLE)
    set_prop(sky_comp, 'real_time_capture', True)
    try:
        for comp in sky.get_components_by_class(unreal.BillboardComponent):
            comp.set_visibility(False)
    except Exception as exc:
        log('BILLBOARD sky %s' % exc)

    atmo = spawn(unreal.SkyAtmosphere, unreal.Vector(0, 0, 0), unreal.Rotator())
    tag(atmo, 'AAA_SkyAtmosphere', 'AAA_LAB/Light')

    pp = spawn(unreal.PostProcessVolume, unreal.Vector(0, 0, 100), unreal.Rotator())
    tag(pp, 'AAA_Post', 'AAA_LAB/Light')
    set_prop(pp, 'unbound', True)
    settings = pp.get_editor_property('settings')
    set_prop(settings, 'override_auto_exposure_bias', True)
    set_prop(settings, 'auto_exposure_bias', 1.2)
    set_prop(settings, 'override_auto_exposure_min_brightness', True)
    set_prop(settings, 'auto_exposure_min_brightness', 0.5)
    set_prop(settings, 'override_auto_exposure_max_brightness', True)
    set_prop(settings, 'auto_exposure_max_brightness', 0.5)
    set_prop(settings, 'override_bloom_intensity', True)
    set_prop(settings, 'bloom_intensity', 0.35)
    set_prop(pp, 'settings', settings)

    cameras = [
        ('A', (380, -620, 165), (40, -80, 90), 48),
        ('B', (980, -1280, 250), (40, 180, 110), 55),
        ('C', (1750, -2100, 780), (0, 280, 90), 62),
    ]
    stored = []
    for name, loc, target, fov in cameras:
        rot = unreal.MathLibrary.find_look_at_rotation(unreal.Vector(*loc), unreal.Vector(*target))
        cam = spawn(unreal.CameraActor, unreal.Vector(*loc), rot)
        tag(cam, 'AAA_CAM_' + name, 'AAA_LAB/Cameras')
        set_prop(cam.camera_component, 'field_of_view', float(fov))
        stored.append(dict(name=name, location=list(loc), target=list(target), fov=fov))

    if not les.save_current_level():
        raise RuntimeError('level save failed')
    saved = ues.get_editor_world().get_path_name()
    if 'AAA_Lab' not in saved:
        raise RuntimeError('saved the wrong world ' + saved)
    return current, stored


def try_decal(eas):
    try:
        assets = unreal.AssetToolsHelpers.get_asset_tools()
        mel = unreal.MaterialEditingLibrary
        mat = assets.create_asset('M_AAA_Lab_Stain', PKG, unreal.Material, unreal.MaterialFactoryNew())
        mat.set_editor_property('material_domain', unreal.MaterialDomain.MD_DEFERRED_DECAL)
        mat.set_editor_property('decal_blend_mode', unreal.DecalBlendMode.DBM_STAIN)
        custom = mel.create_material_expression(mat, unreal.MaterialExpressionCustom, -200, 0)
        custom.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT3)
        custom.set_editor_property('code', 'return float3(0.18, 0.14, 0.09);')
        mask = mel.create_material_expression(mat, unreal.MaterialExpressionCustom, -200, 220)
        mask.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT1)
        mask.set_editor_property('code', """
            float r = saturate(1.0 - length((UV - 0.5) * 2.2));
            float n = 0.5 + 0.5 * sin(UV.x * 40.0) * sin(UV.y * 33.0);
            return r * r * (0.35 + 0.25 * n);
        """)
        uv = mel.create_material_expression(mat, unreal.MaterialExpressionTextureCoordinate, -500, 0)
        pin = unreal.CustomInput()
        pin.set_editor_property('input_name', 'UV')
        mask.set_editor_property('inputs', [pin])
        assert mel.connect_material_expressions(uv, '', mask, 'UV')
        assert mel.connect_material_property(custom, '', unreal.MaterialProperty.MP_BASE_COLOR)
        assert mel.connect_material_property(mask, '', unreal.MaterialProperty.MP_OPACITY)
        # Stain uses opacity as coverage. Float4 base is not enough on every version;
        # also drive opacity from the same mask via a second read of a scalar constant
        # modulated in the graph by a component mask when the output is float4.
        errors = list(mel.recompile_material(mat))
        if errors:
            log('DECAL_COMPILE ' + str(errors))
            return False
        unreal.EditorAssetLibrary.save_asset(PKG + '/M_AAA_Lab_Stain')
        actor = eas.spawn_actor_from_class(
            unreal.DecalActor, unreal.Vector(30, -480, ground_z(30, -480) + 40),
            unreal.Rotator(pitch=-90, yaw=15, roll=0))
        tag(actor, 'AAA_Stain', 'AAA_LAB/Hero')
        comp = actor.get_component_by_class(unreal.DecalComponent)
        set_prop(comp, 'decal_size', unreal.Vector(30, 120, 90))
        comp.set_decal_material(mat)
        return True
    except Exception:
        log('DECAL_SKIP ' + traceback.format_exc().splitlines()[-1])
        return False


def capture_clean(world, cam, path):
    """Scene capture, so the proof frame has no editor light billboards or selection box."""
    loc = unreal.Vector(*cam['location'])
    rot = unreal.MathLibrary.find_look_at_rotation(loc, unreal.Vector(*cam['target']))
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actor = eas.spawn_actor_from_class(unreal.SceneCapture2D, loc, rot)
    if actor is None:
        raise RuntimeError('scene capture spawn failed')
    comp = actor.capture_component2d
    rt = unreal.RenderingLibrary.create_render_target2d(
        world, 1920, 1080, unreal.TextureRenderTargetFormat.RTF_RGBA8, unreal.LinearColor(0, 0, 0, 1))
    comp.set_editor_property('fov_angle', float(cam['fov']))
    comp.set_editor_property('capture_source', unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR)
    comp.set_editor_property('texture_target', rt)
    set_prop(comp, 'capture_every_frame', False)
    set_prop(comp, 'capture_on_movement', False)
    comp.capture_scene()
    folder = os.path.dirname(path)
    name = os.path.splitext(os.path.basename(path))[0]
    unreal.RenderingLibrary.export_render_target(world, rt, folder, name)
    actor.destroy_actor()


def screenshot_loop(cameras):
    import time
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    world = ues.get_editor_world()

    def cmd(line):
        unreal.SystemLibrary.execute_console_command(world, line)

    cmd('viewmode lit')
    cmd('ShowFlag.Sprites 0')
    cmd('ShowFlag.Grid 0')
    cmd('ShowFlag.Bounds 0')
    try:
        unreal.get_editor_subsystem(unreal.EditorActorSubsystem).set_selected_level_actors([])
    except Exception as exc:
        log('deselect %s' % exc)
    state = dict(idx=0, requested=False, mark=time.monotonic(), attempts=0, handle=None)

    def tick(dt):
        try:
            if state['idx'] >= len(cameras):
                unreal.unregister_slate_post_tick_callback(state['handle'])
                log('AAA_LAB_COMPLETE cameras=%s' % len(cameras))
                unreal.SystemLibrary.quit_editor()
                return
            cam = cameras[state['idx']]
            les.editor_invalidate_viewports()
            wait = 12.0 if state['idx'] == 0 and not state['requested'] else 6.0
            if not state['requested']:
                loc = unreal.Vector(*cam['location'])
                rot = unreal.MathLibrary.find_look_at_rotation(loc, unreal.Vector(*cam['target']))
                ues.set_level_viewport_camera_info(loc, rot)
                cmd('ShowFlag.Sprites 0')
                cmd('ShowFlag.Grid 0')
                cmd('ShowFlag.Bounds 0')
                cmd('ShowFlag.Selection 0')
                cmd('ShowFlag.SelectionOutline 0')
                try:
                    unreal.get_editor_subsystem(unreal.EditorActorSubsystem).set_selected_level_actors([])
                except Exception:
                    pass
                if time.monotonic() - state['mark'] > wait:
                    png = os.path.join(OUT, 'cam_%s.png' % cam['name'].lower())
                    if not state.get('exported'):
                        try:
                            capture_clean(world, cam, png)
                            state['exported'] = True
                            state['mark'] = time.monotonic()
                        except Exception as exc:
                            log('CAPTURE_FALLBACK %s' % exc)
                            cmd('HighResShot 1920x1080 filename="%s/cam_%s.png"' % (OUT.replace('\\', '/'), cam['name'].lower()))
                            state['requested'] = True
                            state['mark'] = time.monotonic()
                        return
                    if os.path.isfile(png) and os.path.getsize(png) > 1000:
                        log('CAPTURE %s bytes=%s' % (png, os.path.getsize(png)))
                        state['idx'] += 1
                        state['exported'] = False
                        state['requested'] = False
                        state['attempts'] = 0
                        state['mark'] = time.monotonic()
                        return
                    if time.monotonic() - state['mark'] > 4.0:
                        log('CAPTURE_MISSING %s' % png)
                        cmd('HighResShot 1920x1080 filename="%s/cam_%s.png"' % (OUT.replace('\\', '/'), cam['name'].lower()))
                        state['requested'] = True
                        state['exported'] = False
                        state['mark'] = time.monotonic()
            elif time.monotonic() - state['mark'] > 6.0:
                png = os.path.join(OUT, 'cam_%s.png' % cam['name'].lower())
                if not os.path.isfile(png):
                    state['attempts'] += 1
                    if state['attempts'] >= 6:
                        raise RuntimeError('missing screenshot ' + png)
                    state['requested'] = False
                    state['mark'] = time.monotonic()
                    return
                state['idx'] += 1
                state['requested'] = False
                state['attempts'] = 0
                state['mark'] = time.monotonic()
        except Exception:
            log(traceback.format_exc())
            unreal.unregister_slate_post_tick_callback(state['handle'])
            unreal.SystemLibrary.quit_editor()

    state['handle'] = unreal.register_slate_post_tick_callback(tick)


OWNED = (
    'M_AAA_Lab_Surface', 'M_AAA_Lab_Stain',
    'MI_AAA_Soil', 'MI_AAA_Stone', 'MI_AAA_Wood', 'MI_AAA_Scale', 'MI_AAA_Contact',
    'SM_AAA_Ground_30m', 'SM_AAA_Stone_Hero', 'SM_AAA_Timber_Wall',
    'SM_AAA_Timber_Beam', 'SM_AAA_Path_Stones', 'SM_AAA_Scale_180cm', 'SM_AAA_Contact',
    'Lvl_AAA_VisualLab',
)


def reset_owned():
    lib = unreal.EditorAssetLibrary
    for name in OWNED:
        path = PKG + '/' + name
        if not path.startswith('/Game/Anastasis/LookDev/AAA_Lab/'):
            raise RuntimeError(path)
        if lib.does_asset_exist(path) and not lib.delete_asset(path):
            raise RuntimeError('owned asset locked: ' + path)


def main():
    if not OUT:
        raise RuntimeError('ANASTASIS_AAA_LAB_OUT is required')
    unreal.EditorAssetLibrary.make_directory(PKG)
    reset_owned()
    pipeline = dict(
        engine=unreal.SystemLibrary.get_engine_version(),
        project=unreal.Paths.project_dir(),
        cvars={name: read_cvar(name) for name in CVARS},
        changed_project_settings=False,
    )
    dump('pipeline.json', pipeline)
    log('PIPELINE ' + json.dumps(pipeline['cvars']))
    rows = audit_assets()
    materials = audit_materials([
        '/Game/Anastasis/Materials/M_AnastasisGround',
        '/Game/Anastasis/Materials/M_AnastasisGrass',
        '/Game/Anastasis/Materials/M_AnastasisBark',
        '/Game/Anastasis/Materials/M_AnastasisLithos',
        '/Game/Anastasis/Materials/M_AnastasisSlice',
        '/Game/Anastasis/VillageBuildings/M_VillageBuilding_Surface',
        '/Game/Anastasis/RefugeeProps008/M_RefugeeProps_Surface',
        '/Game/Anastasis/CampShelter009/M_Shelter_LinenTimber',
    ])
    grades = {}
    for row in rows:
        grades[row.get('grade', '?')] = grades.get(row.get('grade', '?'), 0) + 1
    dump('asset-audit.json', dict(count=len(rows), grades=grades, meshes=rows, materials=materials))
    log('AUDIT count=%s grades=%s' % (len(rows), grades))

    master = build_master()
    instances = build_instances(master)
    built = {
        'SM_AAA_Ground_30m': (build_ground(), instances['MI_AAA_Soil'], False),
        'SM_AAA_Contact': (build_contact(), instances['MI_AAA_Contact'], False),
        'SM_AAA_Stone_Hero': (build_stone(), instances['MI_AAA_Stone'], True),
        'SM_AAA_Timber_Wall': (build_wall(), instances['MI_AAA_Wood'], True),
        'SM_AAA_Timber_Beam': (build_beam(), instances['MI_AAA_Wood'], True),
        'SM_AAA_Path_Stones': (build_path(), instances['MI_AAA_Stone'], True),
        'SM_AAA_Scale_180cm': (build_scale(), instances['MI_AAA_Scale'], False),
    }
    meshes = {}
    mesh_report = []
    for name, (geo, material, nanite) in built.items():
        asset, used, tris = commit_mesh(name, geo, material, nanite)
        meshes[name] = asset
        mesh_report.append(dict(name=name, triangles=tris, nanite=used, material=material.get_name()))
    dump('lab-meshes.json', mesh_report)
    current, cameras = build_level(instances, meshes)
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    decal = try_decal(eas)
    if decal:
        les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
        if not les.save_current_level():
            raise RuntimeError('level save after decal failed')
    dump('placement.json', dict(current_assets=current, cameras=cameras, decal=decal, extent_m=30))
    dump('cameras.json', cameras)
    screenshot_loop(cameras)


if __name__ == '__main__':
    try:
        main()
    except Exception:
        log(traceback.format_exc())
        unreal.SystemLibrary.quit_editor()
