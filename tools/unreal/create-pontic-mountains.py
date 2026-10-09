"""PONTIC_MOUNTAINS_001: eight real 3D distant-mountain StaticMeshes.

The contact sheet at docs/visual/reference/pontic-mountain-assets.png is a
visual brief, not geometry. This script owns
/Game/Anastasis/PonticMountains/SM_PonticMountain_01..08 and their material.
Run in a dedicated headless editor through create-pontic-mountains.ps1.
All meshes are deterministic, closed, collision-free and carry linear albedo in
vertex colours. The base rim is sunk under the continental horizon at runtime.
"""
import math
import os
import unreal

ROOT = '/Game/Anastasis/PonticMountains'
MAT_PATH = ROOT + '/M_PonticMountains'
NAMES = ('WoodedFoothill', 'RockyMidridge', 'GreenMassif', 'MistyChain',
         'JaggedRidge', 'IncisedValley', 'DrySouthSlope', 'SnowSummit')
# Width, depth and maximum height in metres; the same local pivot is used by
# the placement code. Spec-specific form below changes more than the palette.
SIZES = ((3000, 1800, 500), (2800, 1800, 700), (3900, 2500, 850),
         (3700, 2700, 900), (3000, 1800, 1400), (3600, 2600, 750),
         (3400, 1900, 550), (3200, 2200, 2100))
NX, NY = 97, 55
# Crest ordinates are read as regional silhouettes from the eight panels of
# the supplied contact sheet. They guide a true volumetric ridge, not a card.
CRESTS = (
    ((-1, .08), (-.78, .36), (-.54, .52), (-.28, .78), (-.05, .97), (.20, .80), (.49, .55), (.77, .35), (1, .08)),
    ((-1, .08), (-.74, .28), (-.49, .48), (-.27, .42), (-.06, .72), (.14, .96), (.31, .69), (.51, .85), (.71, .48), (1, .08)),
    ((-1, .08), (-.78, .39), (-.57, .65), (-.37, .59), (-.11, .74), (.13, .91), (.34, .77), (.56, .83), (.78, .41), (1, .08)),
    ((-1, .08), (-.73, .37), (-.55, .56), (-.33, .51), (-.10, .82), (.08, .65), (.32, 1.0), (.56, .66), (.81, .40), (1, .08)),
    ((-1, .05), (-.82, .25), (-.68, .49), (-.55, .34), (-.42, .72), (-.32, .51), (-.19, .90), (-.08, .58), (.04, 1.0), (.16, .62), (.28, .87), (.40, .55), (.54, .93), (.65, .45), (.80, .67), (1, .05)),
    ((-1, .05), (-.78, .41), (-.58, .82), (-.37, .76), (-.21, .43), (-.08, .19), (.08, .12), (.20, .35), (.42, .83), (.61, .91), (.79, .48), (1, .05)),
    ((-1, .07), (-.77, .28), (-.53, .48), (-.31, .69), (-.08, .83), (.16, .77), (.39, .64), (.63, .39), (1, .06)),
    ((-1, .05), (-.80, .28), (-.61, .47), (-.46, .83), (-.29, .50), (-.11, .66), (.05, 1.0), (.20, .72), (.38, .48), (.58, .80), (.74, .42), (1, .05)),
)


def clamp(x, a=0.0, b=1.0):
    return max(a, min(b, x))


def smooth(a, b, x):
    t = clamp((x - a) / (b - a))
    return t * t * (3.0 - 2.0 * t)


def hash2(x, y, seed):
    k = (x * 374761393 + y * 668265263 + seed * 1442695041) & 0xffffffff
    k ^= k >> 13
    k = (k * 1274126177) & 0xffffffff
    return ((k ^ (k >> 16)) & 0xffffffff) / 4294967295.0


def noise(x, y, seed):
    ix, iy = math.floor(x), math.floor(y)
    tx, ty = smooth(0, 1, x - ix), smooth(0, 1, y - iy)
    a = hash2(ix, iy, seed) * (1 - tx) + hash2(ix + 1, iy, seed) * tx
    b = hash2(ix, iy + 1, seed) * (1 - tx) + hash2(ix + 1, iy + 1, seed) * tx
    return a * (1 - ty) + b * ty


def fbm(x, y, seed):
    return (0.52 * noise(x, y, seed) + 0.27 * noise(x * 2.13, y * 2.13, seed + 11)
            + 0.14 * noise(x * 4.29, y * 4.29, seed + 23)
            + 0.07 * noise(x * 8.61, y * 8.61, seed + 37))


def gaussian(x, y, cx, cy, sx, sy):
    return math.exp(-0.5 * (((x - cx) / sx) ** 2 + ((y - cy) / sy) ** 2))


def crest(kind, x):
    points = CRESTS[kind]
    for a, b in zip(points, points[1:]):
        if x <= b[0]:
            t = clamp((x - a[0]) / (b[0] - a[0]))
            if kind in (0, 2, 3, 6):
                t = t * t * (3 - 2 * t)
            return a[1] * (1 - t) + b[1] * t
    return points[-1][1]


def relief(kind, x, y):
    """Normalized plan [-1,1]^2; broad structure precedes sub-km erosion."""
    seed = 101 + kind * 43
    warp = (fbm(x * 2.2, y * 2.2, seed) - 0.5) * 0.14
    u, v = x + warp, y + warp * 0.6
    ridge = 1.0 - abs(2.0 * fbm(u * 4.0, v * 2.2, seed + 71) - 1.0)
    veins = max(0.0, 1.0 - abs(2.0 * fbm(u * 10.0, v * 6.0, seed + 91) - 1.0))
    if kind == 0:  # rounded, forested foothills
        h = (0.36 * gaussian(u, v, -0.42, 0.04, 0.46, 0.58)
             + 0.48 * gaussian(u, v, 0.30, 0.17, 0.40, 0.44) + 0.11 * ridge)
    elif kind == 1:  # exposed limestone midridge
        h = (0.40 * gaussian(u, v, -0.25, 0.10, 0.53, 0.54)
             + 0.38 * gaussian(u, v, 0.36, 0.08, 0.32, 0.40) + 0.21 * ridge + 0.08 * veins)
    elif kind == 2:  # broad green massif with two distinct watersheds
        h = (0.43 * gaussian(u, v, -0.50, 0.05, 0.43, 0.65)
             + 0.52 * gaussian(u, v, 0.30, 0.12, 0.51, 0.60) + 0.10 * ridge)
    elif kind == 3:  # two depth planes in one continuous mesh
        h = (0.34 * gaussian(u, v, -0.35, -0.39, 0.55, 0.27)
             + 0.54 * gaussian(u, v, 0.28, 0.25, 0.47, 0.25) + 0.17 * ridge)
    elif kind == 4:  # serrated teeth with short radial gullies
        teeth = sum((0.36 + 0.16 * (i % 3)) * gaussian(u, v, -0.75 + i * 0.25,
                     0.15 + 0.10 * math.sin(i * 2.1), 0.083, 0.35) for i in range(7))
        h = 0.12 * gaussian(u, v, 0, 0, 0.95, 0.65) + teeth + 0.045 * ridge + 0.065 * veins
    elif kind == 5:  # opposing slopes cut by a real V valley
        banks = (0.60 * gaussian(u, v, -0.69, 0.11, 0.38, 0.55)
                 + 0.61 * gaussian(u, v, 0.70, 0.17, 0.39, 0.55))
        h = max(0.0, banks + 0.11 * ridge - 0.18 * gaussian(u, v, 0, -0.12, 0.22, 0.80))
    elif kind == 6:  # south-facing, low dry shoulder
        h = (0.64 * gaussian(u, v, -0.20, 0.25, 0.86, 0.52)
             + 0.14 * ridge + 0.08 * veins) * (0.90 - 0.15 * v)
    else:  # iconic snow summit and two lower companion peaks
        h = (0.23 * gaussian(u, v, 0.0, 0.04, 0.80, 0.61)
             + 0.94 * gaussian(u, v, 0.07, 0.20, 0.15, 0.34)
             + 0.41 * gaussian(u, v, -0.48, 0.08, 0.22, 0.41)
             + 0.38 * gaussian(u, v, 0.55, 0.11, 0.22, 0.39)
             + 0.055 * ridge + 0.075 * veins)
    spine = crest(kind, u)
    crest_y = 0.17 + 0.07 * math.sin(5 * u + kind)
    h += (0.60 if kind in (1, 4, 7) else 0.43) * spine * math.exp(
        -0.5 * ((v - crest_y) / (0.34 if kind in (1, 4, 7) else 0.48)) ** 2)
    # A broad edge fade keeps each asset embedded in, not perched on, the
    # continental ring. Small-scale variation modulates an existing mass.
    envelope = smooth(1.0, 0.56, max(abs(x), abs(y)))
    return max(0.0, h * envelope * (0.80 + 0.34 * fbm(u * 7.1, v * 7.1, seed + 121)))


def colour(kind, height, slope, x, y):
    seed = 201 + kind * 67
    patch = fbm(x * 8.0, y * 8.0, seed)
    tree = smooth(0.08, 0.35, height) * (1.0 - smooth(0.52, 0.83, height))
    tree *= (1.0 - smooth(0.25, 0.72, slope))
    if kind in (0, 2, 3, 5):
        tree *= 0.92
    elif kind in (1, 4, 7):
        tree *= 0.48
    else:
        tree *= 0.20
    rock = smooth(0.28, 0.62, slope) + 0.35 * smooth(0.54, 0.90, height)
    rock = clamp(rock)
    if kind in (1, 4, 7):
        rock = clamp(rock + 0.25)
    snow = (smooth(0.38, 0.70, height + 0.11 * (patch - 0.5))
            * (1.0 - 0.78 * smooth(0.55, 0.90, slope))) if kind == 7 else 0.0
    soil = (0.18, 0.15, 0.105) if kind == 6 else (0.145, 0.15, 0.097)
    forest = (0.052, 0.102, 0.058)
    stone = (0.245, 0.235, 0.211) if kind in (1, 4) else (0.205, 0.203, 0.188)
    ice = (0.65, 0.68, 0.71)
    c = tuple(soil[i] * (1 - tree) + forest[i] * tree for i in range(3))
    c = tuple(c[i] * (1 - rock) + stone[i] * rock for i in range(3))
    c = tuple(c[i] * (1 - snow) + ice[i] * snow for i in range(3))
    tone = 0.52 + 1.05 * patch
    return tuple(clamp(v * tone) for v in c)


def make_mesh(kind):
    width, depth, top = SIZES[kind]
    heights = [[relief(kind, 2 * i / (NX - 1) - 1, 2 * j / (NY - 1) - 1)
                for i in range(NX)] for j in range(NY)]
    maximum = max(max(row) for row in heights)
    if maximum <= 0.01:
        raise RuntimeError('relief plat: %s' % NAMES[kind])
    vertices, colours, uv, triangles = [], [], [], []
    for j in range(NY):
        y = 2 * j / (NY - 1) - 1
        for i in range(NX):
            x = 2 * i / (NX - 1) - 1
            h = heights[j][i] / maximum
            dx = heights[j][min(i + 1, NX - 1)] - heights[j][max(i - 1, 0)]
            dy = heights[min(j + 1, NY - 1)][i] - heights[max(j - 1, 0)][i]
            sx = abs(dx * top / maximum / (width / (NX - 1) * (2 if 0 < i < NX - 1 else 1)))
            sy = abs(dy * top / maximum / (depth / (NY - 1) * (2 if 0 < j < NY - 1 else 1)))
            slope = clamp(math.hypot(sx, sy) / 1.5)
            r, g, b = colour(kind, h, slope, x, y)
            vertices.append(unreal.Vector(x * width * 50, y * depth * 50, h * top * 100))
            colours.append(unreal.LinearColor(r, g, b, 1))
            uv.append(unreal.Vector2D(i / (NX - 1), j / (NY - 1)))
    for j in range(NY - 1):
        for i in range(NX - 1):
            a = j * NX + i
            triangles.extend((unreal.IntVector(a, a + 1, a + NX),
                              unreal.IntVector(a + 1, a + NX + 1, a + NX)))
    # Close the underside; hidden by the ring but useful when inspecting the
    # standalone asset from below. No collision or navigation relevance.
    perimeter = ([i for i in range(NX)] + [j * NX + NX - 1 for j in range(1, NY)]
                 + [(NY - 1) * NX + i for i in range(NX - 2, -1, -1)]
                 + [j * NX for j in range(NY - 2, 0, -1)])
    bottom = []
    for a in perimeter:
        va = vertices[a]
        bottom.append(len(vertices))
        vertices.append(unreal.Vector(va.x, va.y, -18000))
        colours.append(colours[a])
        uv.append(uv[a])
    centre = len(vertices)
    vertices.append(unreal.Vector(0, 0, -18000))
    colours.append(unreal.LinearColor(0.065, 0.068, 0.057, 1))
    uv.append(unreal.Vector2D(0.5, 0.5))
    for k, a in enumerate(perimeter):
        b, ba, bb = perimeter[(k + 1) % len(perimeter)], bottom[k], bottom[(k + 1) % len(bottom)]
        triangles.extend((unreal.IntVector(a, ba, b), unreal.IntVector(b, ba, bb),
                          unreal.IntVector(centre, bb, ba)))
    # At 5-15 km a forest must still alter the ridge silhouette; vertex tint
    # alone reads as a flat dark mound. Sparse closed 3D canopies are fused into
    # the same StaticMesh, with no extra actor or draw call per tree.
    density = (0.78, 0.18, 0.84, 0.38, 0.04, 0.48, 0.10, 0.03)[kind]
    if density:
        for j in range(2, NY - 2):
            for i in range(2, NX - 2):
                n = hash2(i, j, 509 + kind * 67)
                u = 2 * (i + 0.7 * (hash2(i, j, 601) - 0.5)) / (NX - 1) - 1
                v = 2 * (j + 0.7 * (hash2(i, j, 607) - 0.5)) / (NY - 1) - 1
                h = relief(kind, u, v) / maximum
                slope = abs(heights[j][i + 1] - heights[j][i - 1])
                slope += abs(heights[j + 1][i] - heights[j - 1][i])
                cover = smooth(0.07, 0.27, h) * (1 - smooth(0.53, 0.78, h))
                cover *= 1 - smooth(0.08, 0.28, slope)
                if n > density * cover:
                    continue
                tx, ty = u * width * 50, v * depth * 50
                z = h * top * 100
                height = (18 + 13 * hash2(i, j, 613)) * 100
                radius = (5.5 + 4.5 * hash2(i, j, 617)) * 100
                shade = 0.68 + 0.55 * hash2(i, j, 619)
                c = unreal.LinearColor(0.043 * shade, 0.092 * shade, 0.052 * shade, 1)
                base = len(vertices)
                for level, rad, dz in ((0, radius, 0), (1, radius * 0.60, height * 0.64)):
                    for q in range(5):
                        angle = (q / 5.0) * math.tau
                        vertices.append(unreal.Vector(tx + math.cos(angle) * rad,
                                                      ty + math.sin(angle) * rad, z + dz))
                        colours.append(c)
                        uv.append(unreal.Vector2D(q / 5.0, level))
                tip = len(vertices)
                vertices.append(unreal.Vector(tx, ty, z + height))
                colours.append(c)
                uv.append(unreal.Vector2D(0.5, 1))
                foot = len(vertices)
                vertices.append(unreal.Vector(tx, ty, z))
                colours.append(c)
                uv.append(unreal.Vector2D(0.5, 0))
                for q in range(5):
                    nxt = (q + 1) % 5
                    triangles.extend((unreal.IntVector(base + q, base + nxt, base + 5 + q),
                                      unreal.IntVector(base + nxt, base + 5 + nxt, base + 5 + q),
                                      unreal.IntVector(base + 5 + q, base + 5 + nxt, tip),
                                      unreal.IntVector(foot, base + nxt, base + q)))
    buffers = unreal.GeometryScriptSimpleMeshBuffers(
        vertices=vertices, triangles=triangles, vertex_colors=colours, uv0=uv)
    mesh, unused = unreal.GeometryScript_MeshEdits.append_buffers_to_mesh(unreal.DynamicMesh(), buffers)
    return mesh, len(vertices), len(triangles)


def material():
    mel, eal = unreal.MaterialEditingLibrary, unreal.EditorAssetLibrary
    if eal.does_asset_exist(MAT_PATH):
        # The graph is stable. Rebuilding a loaded Material in UE 5.8.2 can
        # assert !IsRooted in DeleteAllMaterialExpressions; retain its identity.
        m = eal.load_asset(MAT_PATH)
        if m is None:
            raise RuntimeError('materiau existant illisible')
        return m
    m = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        'M_PonticMountains', ROOT, unreal.Material, unreal.MaterialFactoryNew())
    vc = mel.create_material_expression(m, unreal.MaterialExpressionVertexColor, -500, 0)
    if not mel.connect_material_property(vc, '', unreal.MaterialProperty.MP_BASE_COLOR):
        raise RuntimeError('materiau: vertex colour non connecte')
    for value, prop, y in ((0.90, unreal.MaterialProperty.MP_ROUGHNESS, 140),
                           (0.28, unreal.MaterialProperty.MP_SPECULAR, 220)):
        node = mel.create_material_expression(m, unreal.MaterialExpressionConstant, -250, y)
        node.set_editor_property('r', value)
        mel.connect_material_property(node, '', prop)
    m.set_editor_property('two_sided', False)
    errors = list(mel.recompile_material(m) or [])
    if errors:
        raise RuntimeError('materiau: %s' % errors)
    eal.save_asset(MAT_PATH)
    return m


def main():
    if unreal.get_editor_subsystem(unreal.AssetEditorSubsystem) is None:
        raise RuntimeError('editeur vivant requis')
    eal = unreal.EditorAssetLibrary
    eal.make_directory(ROOT)
    mat = material()
    for kind, name in enumerate(NAMES):
        path = ROOT + '/SM_PonticMountain_%02d_%s' % (kind + 1, name)
        mesh, vertices, faces = make_mesh(kind)
        if eal.does_asset_exist(path):
            # Preserve the package identity once the CDO carries a hard cook
            # reference; deleting a loaded asset would leave a stale object.
            asset = eal.load_asset(path)
            copy = unreal.GeometryScriptCopyMeshToAssetOptions(enable_recompute_tangents=True)
            try:
                copy.set_editor_property('enable_recompute_normals', True)
            except Exception:
                pass
            unused, outcome = unreal.GeometryScript_AssetUtils.copy_mesh_to_static_mesh(
                mesh, asset, copy, unreal.GeometryScriptMeshWriteLOD(lod_index=0))
            if outcome != unreal.GeometryScriptOutcomePins.SUCCESS:
                raise RuntimeError('%s: copy_mesh_to_static_mesh %s' % (path, outcome))
        else:
            opts = unreal.GeometryScriptCreateNewStaticMeshAssetOptions()
            opts.set_editor_property('enable_nanite', False)
            opts.set_editor_property('enable_recompute_normals', True)
            opts.set_editor_property('enable_recompute_tangents', True)
            try:
                opts.set_editor_property('enable_collision', False)
            except Exception as exc:
                unreal.log_warning('PONTIC_MOUNTAIN_COLLISION_OPTION_UNAVAILABLE %s' % exc)
            asset, outcome = unreal.GeometryScript_NewAssetUtils.create_new_static_mesh_asset_from_mesh(mesh, path, opts)
        if asset is None:
            raise RuntimeError('%s: %s' % (path, outcome))
        asset.set_material(0, mat)
        if asset.get_num_triangles(0) != faces:
            raise RuntimeError('%s: asset triangles=%d source=%d' % (path, asset.get_num_triangles(0), faces))
        bounds = asset.get_bounding_box()
        if bounds.max.z < 30000 or bounds.min.z > -17900:
            raise RuntimeError('%s: invalid 3D bounds %s..%s' % (path, bounds.min, bounds.max))
        if not eal.save_asset(path):
            raise RuntimeError('%s: save failed' % path)
        unreal.log('PONTIC_MOUNTAIN_ASSET %02d %s vertices=%d triangles=%d height_cm=%.0f' % (
            kind + 1, path, vertices, faces, bounds.max.z))
    unreal.log('PONTIC_MOUNTAINS::PASS count=8')


try:
    main()
except Exception as exc:
    import traceback
    unreal.log_error('PONTIC_MOUNTAINS::FAIL %s\n%s' % (exc, traceback.format_exc()))
finally:
    if os.environ.get('ANASTASIS_PONTIC_MOUNTAINS_QUIT') == '1':
        unreal.SystemLibrary.quit_editor()
