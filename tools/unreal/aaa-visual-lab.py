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
VERSION = 'aaa-visual-lab-v1'
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
    for j in range(n):
        for i in range(n):
            quad = [pts[j][i], pts[j + 1][i], pts[j + 1][i + 1], pts[j][i + 1]]
            cs = [cols[j][i], cols[j + 1][i], cols[j + 1][i + 1], cols[j][i + 1]]
            uvs = [(p[0] / 100.0, p[1] / 100.0) for p in quad]
            mesh.quad(quad, cs, uvs)
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
            wobble = 1.0 + 0.09 * math.sin(ang * 3.0 + v * 5.0) + 0.05 * math.sin(ang * 7.0 - v * 2.0)
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
        cup = 1.3 * math.sin(p * 1.7)
        for s in range(segs):
            z0 = height * s / segs
            z1 = height * (s + 1) / segs
            warp0 = cup * math.sin(math.pi * s / segs) + 0.4 * math.sin(p + s)
            warp1 = cup * math.sin(math.pi * (s + 1) / segs) + 0.4 * math.sin(p + s + 0.4)
            y_front = -thick * 0.5
            face = [
                (x_left, y_front + warp0, z0),
                (x_left + plank_w, y_front + warp0 * 0.8, z0),
                (x_left + plank_w, y_front + warp1 * 0.8, z1),
                (x_left, y_front + warp1, z1),
            ]
            def paint(z):
                moss = 0.75 if z < 28.0 else (0.2 if z < 55.0 else 0.0)
                dirt = 0.7 if z < 22.0 else (0.15 if z > height - 18.0 else 0.05)
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
                (x, -thick * 0.5, height),
                (x, thick * 0.5, height),
            ]
            if side == 1:
                edge = [edge[1], edge[0], edge[3], edge[2]]
            mesh.quad(edge, [col4(0.2, 0.0, 0.1)] * 4, [(0, 0), (0.08, 0), (0.08, 2.1), (0, 2.1)])
        top = [
            (x_left, -thick * 0.5, height),
            (x_left + plank_w, -thick * 0.5, height),
            (x_left + plank_w, thick * 0.5, height),
            (x_left, thick * 0.5, height),
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


def make_custom(mat, mel, code, out_type, x, y, links):
    node = mel.create_material_expression(mat, unreal.MaterialExpressionCustom, x, y)
    node.set_editor_property('output_type', out_type)
    node.set_editor_property('code', code)
    inputs = []
    for name in links:
        pin = unreal.CustomInput()
        pin.set_editor_property('input_name', name)
        inputs.append(pin)
    node.set_editor_property('inputs', inputs)
    for name, (src, src_pin) in links.items():
        if not mel.connect_material_expressions(src, src_pin, node, name):
            raise RuntimeError('material link failed: %s' % name)
    return node


COLOR_CODE = """
float macro = sin(P.x * MacroScale) * sin(P.y * MacroScale * 0.73 + P.z * MacroScale * 0.21);
float meso = sin(P.x * MacroScale * 4.1 + sin(P.y * MacroScale * 2.2)) * sin(P.z * MacroScale * 3.3 + 1.7);
float micro = sin(P.x * MicroScale) * sin(P.y * MicroScale * 1.37 + P.z * MicroScale * 0.41);
float grain = sin(P.z * 0.55 + 3.0 * sin(P.x * 0.17 + P.y * 0.02));
float3 col = lerp(Base.rgb, Tint.rgb, saturate(macro * 0.5 + 0.5) * ColorVariation);
col = lerp(col, col * lerp(0.68, 1.22, grain * 0.5 + 0.5), WoodAmount);
float dirt = saturate(Dirt + VC.r * 0.85 + meso * 0.08);
col = lerp(col, DirtColor.rgb, dirt * 0.5);
float moss = saturate(VC.b * MossAmount);
col = lerp(col, MossColor.rgb, moss);
float wet = saturate(Wetness + VC.g);
col = lerp(col, col * float3(0.55, 0.66, 0.62), wet * 0.7);
float dist = distance(P, Cam);
float fade = saturate((dist - FadeStart) / max(FadeEnd - FadeStart, 1.0));
col *= lerp(0.90 + 0.10 * micro, 1.0, fade);
return col;
"""

NORMAL_CODE = """
float k = max(MicroScale, 0.0001);
float dist = distance(P, Cam);
float fade = saturate((dist - FadeStart) / max(FadeEnd - FadeStart, 1.0));
float amp = DetailStrength * (1.0 - fade);
float stoneX = cos(P.x * k) * sin(P.y * k);
float stoneY = sin(P.x * k) * cos(P.y * k * 1.37);
float woodX = cos(P.z * k * 2.1 + 3.0 * sin(P.x * 0.15));
float woodY = 0.25 * cos(P.x * k * 0.35);
float hx = lerp(stoneX, woodX, WoodAmount);
float hy = lerp(stoneY, woodY, WoodAmount);
return normalize(float3(amp * k * hx, amp * k * hy, 1.0));
"""

ROUGH_CODE = """
float micro = sin(P.x * MicroScale) * sin(P.z * MicroScale * 0.7 + 0.4);
float r = Roughness + RoughnessVariation * micro;
float wet = saturate(Wetness + VC.g);
r = lerp(r, min(r, 0.22), wet);
r = saturate(r + VC.r * 0.06);
return r;
"""

SPEC_CODE = """
float wet = saturate(Wetness + VC.g);
return lerp(0.45, 0.9, wet);
"""

AO_CODE = """
float cavity = saturate(0.5 + 0.5 * sin(P.x * 0.03) * sin(P.y * 0.025));
return lerp(1.0, 0.84, cavity * AoStrength);
"""


def build_master():
    mel = unreal.MaterialEditingLibrary
    assets = unreal.AssetToolsHelpers.get_asset_tools()
    mat = assets.create_asset('M_AAA_Lab_Surface', PKG, unreal.Material, unreal.MaterialFactoryNew())
    if mat is None or mat.get_name() != 'M_AAA_Lab_Surface':
        raise RuntimeError('master material was not created at the expected path')
    mat.set_editor_property('two_sided', True)
    try:
        mat.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    except Exception as exc:
        log('shading model left default: %s' % exc)

    wp = mel.create_material_expression(mat, unreal.MaterialExpressionWorldPosition, -1800, 0)
    vc = mel.create_material_expression(mat, unreal.MaterialExpressionVertexColor, -1800, 200)
    cam = mel.create_material_expression(mat, unreal.MaterialExpressionCameraPositionWS, -1800, 400)
    base = vector_param(mat, mel, 'BaseColor', (0.16, 0.13, 0.09), -1600, -200)
    tint = vector_param(mat, mel, 'TintB', (0.10, 0.11, 0.09), -1600, -80)
    dirt_c = vector_param(mat, mel, 'DirtColor', (0.05, 0.035, 0.02), -1600, 40)
    moss_c = vector_param(mat, mel, 'MossColor', (0.045, 0.07, 0.028), -1600, 160)
    names = [
        ('ColorVariation', 0.4), ('WoodAmount', 0.0), ('Dirt', 0.1), ('Wetness', 0.0),
        ('MossAmount', 0.3), ('MacroScale', 0.0015), ('MicroScale', 0.25),
        ('DetailStrength', 0.45), ('FadeStart', 350.0), ('FadeEnd', 1600.0),
        ('Roughness', 0.78), ('RoughnessVariation', 0.08), ('Metallic', 0.0), ('AoStrength', 0.35),
    ]
    params = {}
    for i, (name, default) in enumerate(names):
        params[name] = scalar(mat, mel, name, default, -1400, -400 + i * 70)

    color_links = {
        'P': (wp, ''), 'VC': (vc, ''), 'Cam': (cam, ''),
        'Base': (base, ''), 'Tint': (tint, ''), 'DirtColor': (dirt_c, ''), 'MossColor': (moss_c, ''),
        'ColorVariation': (params['ColorVariation'], ''), 'WoodAmount': (params['WoodAmount'], ''),
        'Dirt': (params['Dirt'], ''), 'Wetness': (params['Wetness'], ''),
        'MossAmount': (params['MossAmount'], ''), 'MacroScale': (params['MacroScale'], ''),
        'MicroScale': (params['MicroScale'], ''), 'FadeStart': (params['FadeStart'], ''),
        'FadeEnd': (params['FadeEnd'], ''),
    }
    color = make_custom(mat, mel, COLOR_CODE, unreal.CustomMaterialOutputType.CMOT_FLOAT3, -200, -200, color_links)
    normal = make_custom(mat, mel, NORMAL_CODE, unreal.CustomMaterialOutputType.CMOT_FLOAT3, -200, 200, {
        'P': (wp, ''), 'Cam': (cam, ''), 'MicroScale': (params['MicroScale'], ''),
        'DetailStrength': (params['DetailStrength'], ''), 'WoodAmount': (params['WoodAmount'], ''),
        'FadeStart': (params['FadeStart'], ''), 'FadeEnd': (params['FadeEnd'], ''),
    })
    rough = make_custom(mat, mel, ROUGH_CODE, unreal.CustomMaterialOutputType.CMOT_FLOAT1, -200, 500, {
        'P': (wp, ''), 'VC': (vc, ''), 'MicroScale': (params['MicroScale'], ''),
        'Roughness': (params['Roughness'], ''), 'RoughnessVariation': (params['RoughnessVariation'], ''),
        'Wetness': (params['Wetness'], ''),
    })
    spec = make_custom(mat, mel, SPEC_CODE, unreal.CustomMaterialOutputType.CMOT_FLOAT1, -200, 760, {
        'VC': (vc, ''), 'Wetness': (params['Wetness'], ''),
    })
    ao = make_custom(mat, mel, AO_CODE, unreal.CustomMaterialOutputType.CMOT_FLOAT1, -200, 960, {
        'P': (wp, ''), 'AoStrength': (params['AoStrength'], ''),
    })
    assert mel.connect_material_property(color, '', unreal.MaterialProperty.MP_BASE_COLOR)
    assert mel.connect_material_property(normal, '', unreal.MaterialProperty.MP_NORMAL)
    assert mel.connect_material_property(rough, '', unreal.MaterialProperty.MP_ROUGHNESS)
    assert mel.connect_material_property(spec, '', unreal.MaterialProperty.MP_SPECULAR)
    assert mel.connect_material_property(params['Metallic'], '', unreal.MaterialProperty.MP_METALLIC)
    assert mel.connect_material_property(ao, '', unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)
    errors = list(mel.recompile_material(mat))
    if errors:
        raise RuntimeError('master material compile: %s' % errors)
    unreal.EditorAssetLibrary.set_metadata_tag(mat, 'Recipe', VERSION)
    unreal.EditorAssetLibrary.save_asset(PKG + '/M_AAA_Lab_Surface')
    return mat


INSTANCES = {
    'MI_AAA_Soil': dict(
        BaseColor=(0.11, 0.07, 0.035), TintB=(0.07, 0.075, 0.055),
        DirtColor=(0.04, 0.028, 0.016), MossColor=(0.16, 0.15, 0.13),
        ColorVariation=0.5, WoodAmount=0.0, Dirt=0.18, Wetness=0.0, MossAmount=0.35,
        MacroScale=0.0012, MicroScale=0.28, DetailStrength=0.55, Roughness=0.93,
        RoughnessVariation=0.05, Metallic=0.0, AoStrength=0.3,
    ),
    'MI_AAA_Stone': dict(
        BaseColor=(0.20, 0.19, 0.17), TintB=(0.11, 0.105, 0.095),
        DirtColor=(0.045, 0.032, 0.02), MossColor=(0.04, 0.075, 0.03),
        ColorVariation=0.42, WoodAmount=0.0, Dirt=0.06, Wetness=0.0, MossAmount=1.0,
        MacroScale=0.0035, MicroScale=0.48, DetailStrength=1.1, Roughness=0.64,
        RoughnessVariation=0.14, Metallic=0.0, AoStrength=0.4,
    ),
    'MI_AAA_Wood': dict(
        BaseColor=(0.10, 0.048, 0.02), TintB=(0.055, 0.03, 0.014),
        DirtColor=(0.035, 0.025, 0.015), MossColor=(0.05, 0.055, 0.03),
        ColorVariation=0.22, WoodAmount=1.0, Dirt=0.1, Wetness=0.0, MossAmount=0.55,
        MacroScale=0.002, MicroScale=1.4, DetailStrength=0.22, Roughness=0.56,
        RoughnessVariation=0.1, Metallic=0.0, AoStrength=0.25,
    ),
    'MI_AAA_Scale': dict(
        BaseColor=(0.22, 0.22, 0.21), TintB=(0.22, 0.22, 0.21),
        DirtColor=(0.22, 0.22, 0.21), MossColor=(0.22, 0.22, 0.21),
        ColorVariation=0.0, WoodAmount=0.0, Dirt=0.0, Wetness=0.0, MossAmount=0.0,
        MacroScale=0.001, MicroScale=0.01, DetailStrength=0.0, Roughness=0.72,
        RoughnessVariation=0.0, Metallic=0.0, AoStrength=0.0,
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
    log('MESH %s tris=%s nanite=%s' % (name, len(tris), used_nanite))
    return asset, used_nanite, len(tris)


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
        return actor

    place_owned('AAA_Ground', meshes['SM_AAA_Ground_30m'], instances['MI_AAA_Soil'], 0, 0, planted=False)
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

    sun = spawn(unreal.DirectionalLight, unreal.Vector(0, 0, 500), unreal.Rotator(pitch=-46, yaw=38, roll=0))
    tag(sun, 'AAA_Sun', 'AAA_LAB/Light')
    light = sun.get_component_by_class(unreal.DirectionalLightComponent)
    set_prop(light, 'intensity', 10.0)
    set_prop(light, 'cast_shadows', True)
    set_prop(light, 'contact_shadow_length', 0.1)
    set_prop(light, 'atmosphere_sun_light', True)

    sky = spawn(unreal.SkyLight, unreal.Vector(0, 0, 300), unreal.Rotator())
    tag(sky, 'AAA_SkyLight', 'AAA_LAB/Light')
    sky_comp = sky.get_component_by_class(unreal.SkyLightComponent)
    set_prop(sky_comp, 'mobility', unreal.ComponentMobility.MOVABLE)
    set_prop(sky_comp, 'real_time_capture', True)

    atmo = spawn(unreal.SkyAtmosphere, unreal.Vector(0, 0, 0), unreal.Rotator())
    tag(atmo, 'AAA_SkyAtmosphere', 'AAA_LAB/Light')

    pp = spawn(unreal.PostProcessVolume, unreal.Vector(0, 0, 100), unreal.Rotator())
    tag(pp, 'AAA_Post', 'AAA_LAB/Light')
    set_prop(pp, 'unbound', True)
    settings = pp.get_editor_property('settings')
    set_prop(settings, 'override_auto_exposure_bias', True)
    set_prop(settings, 'auto_exposure_bias', 0.35)
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
                if time.monotonic() - state['mark'] > wait:
                    cmd('HighResShot 1920x1080 filename="%s/cam_%s.png"' % (OUT.replace('\\', '/'), cam['name'].lower()))
                    state['requested'] = True
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
    'MI_AAA_Soil', 'MI_AAA_Stone', 'MI_AAA_Wood', 'MI_AAA_Scale',
    'SM_AAA_Ground_30m', 'SM_AAA_Stone_Hero', 'SM_AAA_Timber_Wall',
    'SM_AAA_Timber_Beam', 'SM_AAA_Path_Stones', 'SM_AAA_Scale_180cm',
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
