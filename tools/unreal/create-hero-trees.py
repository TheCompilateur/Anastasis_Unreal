"""
Quatre arbres heros a leur taille reelle, plus une enveloppe de canopee.

Les meshes de production (SM_Tree_*) restent dans une boite de 100 cm, puis
l'incarnation les etire a 8-20 m : les facettes grandissent avec eux. Ce script
ne les touche pas. Il ecrit seulement :

  /Game/Anastasis/Vegetation/Hero/SM_Hero_AleppoPine
  /Game/Anastasis/Vegetation/Hero/SM_Hero_Cypress
  /Game/Anastasis/Vegetation/Hero/SM_Hero_HolmOak
  /Game/Anastasis/Vegetation/Hero/SM_Hero_Olive
  /Game/Anastasis/Vegetation/Hero/SM_CanopyShell

Chaque heros est construit en centimetres, pied en Z = 0, hauteur = la stature
mediane de l'espece. L'incarnation ne lui applique plus qu'un facteur proche de 1.
L'enveloppe est un volume de couronne, sans tronc, pour la masse au-dela de 70 m.

Lancer dans un editeur dedie (la reduction LOD exige StaticMeshEditor) :
  tools/unreal/create-hero-trees.ps1
"""
import math
import os
import random

import unreal

PACKAGE = '/Game/Anastasis/Vegetation/Hero'
FOLIAGE = '/Game/Anastasis/Materials/M_AnastasisVegetation'
BARK = '/Game/Anastasis/Materials/M_AnastasisBark'
SLOT_FOLIAGE = 0
SLOT_WOOD = 1
HERO_LOBE = (12, 20)
FAR_LOBE = (5, 9)


def log(msg):
    unreal.log('HERO_TREES ' + str(msg))


def flags():
    f = unreal.GeometryScriptColorFlags()
    f.red = f.green = f.blue = f.alpha = True
    return f


FLAGS = flags()


def prim(slot):
    options = unreal.GeometryScriptPrimitiveOptions()
    options.set_editor_property('material_id', slot)
    return options


PRIM_LEAF = prim(SLOT_FOLIAGE)
PRIM_WOOD = prim(SLOT_WOOD)


def coloured(mesh, color):
    return unreal.GeometryScript_VertexColors.set_mesh_constant_vertex_color(mesh, color, FLAGS, clear_existing=True)


def merge(target, part):
    if part is None:
        return target
    return unreal.GeometryScript_MeshEdits.append_mesh(target, part, unreal.Transform())


def shade(mesh):
    split = unreal.GeometryScriptSplitNormalsOptions()
    split.set_editor_property('split_by_opening_angle', True)
    split.set_editor_property('opening_angle_deg', 45.0)
    split.set_editor_property('split_by_face_group', False)
    calc = unreal.GeometryScriptCalculateNormalsOptions()
    calc.set_editor_property('angle_weighted', True)
    calc.set_editor_property('area_weighted', True)
    return unreal.GeometryScript_Normals.compute_split_normals(mesh, split, calc)


def taper(base_radius, top_radius, z0, z1, steps, location, rotator, material):
    mesh = unreal.DynamicMesh()
    xf = unreal.Transform(location=location, rotation=rotator)
    return unreal.GeometryScript_Primitives.append_cone(
        mesh, material, xf, base_radius=base_radius, top_radius=max(top_radius, 0.4),
        height=max(z1 - z0, 1.0), radial_steps=steps, height_steps=1, capped=True,
        origin=unreal.GeometryScriptPrimitiveOriginMode.BASE)


def branch(start, end, radius, tip, steps=16):
    delta = [end[i] - start[i] for i in range(3)]
    length = math.sqrt(sum(v * v for v in delta)) or 1.0
    rotator = unreal.Rotator(
        pitch=-math.degrees(math.acos(max(-1.0, min(1.0, delta[2] / length)))),
        yaw=math.degrees(math.atan2(delta[1], delta[0])), roll=0.0)
    return taper(radius, tip, 0.0, length, steps, unreal.Vector(*start), rotator, PRIM_WOOD)


def lobe(radius, cx, cy, cz, squash, steps):
    mesh = unreal.DynamicMesh()
    phi, theta = steps
    mesh = unreal.GeometryScript_Primitives.append_sphere_lat_long(
        mesh, PRIM_LEAF, unreal.Transform(location=unreal.Vector(cx, cy, cz)),
        radius=radius, steps_phi=phi, steps_theta=theta,
        origin=unreal.GeometryScriptPrimitiveOriginMode.CENTER)
    return unreal.GeometryScript_MeshTransforms.scale_mesh(mesh, unreal.Vector(1.0, 1.0, squash), unreal.Vector(cx, cy, cz))


def curved_stem(rng, top_z, lean_deg, azimuth, wobble, r0, r1, segments, sweep=0.0):
    pts = [(0.0, 0.0, 0.0)]
    lean = math.tan(math.radians(lean_deg))
    for i in range(1, segments + 1):
        t = i / segments
        z = top_z * t
        reach = lean * z * (1.0 - sweep * t * t)
        jitter = wobble * math.sin(t * math.pi)
        pts.append((reach * math.cos(azimuth) + rng.uniform(-jitter, jitter),
                    reach * math.sin(azimuth) + rng.uniform(-jitter, jitter), z))
    stems = []
    for i in range(segments):
        a, b = pts[i], pts[i + 1]
        b2 = tuple(a[k] + (b[k] - a[k]) * 1.04 for k in range(3)) if i + 1 < segments else b
        stems.append((a, b2, r0 + (r1 - r0) * i / segments, r0 + (r1 - r0) * (i + 1) / segments))
    return stems, pts


def scatter(rng, centre, rx, rz, count, r_lo, r_hi, squash, shell=(0.35, 0.95), top_bias=0.15):
    lobes = []
    for k in range(count):
        radius = r_hi - (r_hi - r_lo) * k / max(count - 1, 1) * rng.uniform(0.85, 1.0)
        if k == 0:
            lobes.append((radius, centre[0], centre[1], centre[2], squash))
            continue
        theta = k * 2.399963 + rng.uniform(-0.4, 0.4)
        u = (k + 0.5) / count
        zz = max(-1.0, min(1.0, 1.0 - 2.0 * u + top_bias))
        ring = math.sqrt(max(0.0, 1.0 - zz * zz))
        d = rng.uniform(*shell)
        ex, ez = max(rx - radius, 1.0), max(rz - radius * squash, 1.0)
        lobes.append((radius, centre[0] + ex * d * ring * math.cos(theta),
                      centre[1] + ex * d * ring * math.sin(theta), centre[2] + ez * d * zz, squash))
    return lobes


def blades(centres, color, alt, alt_share, rng, width):
    vertices, triangles, colors, uv = [], [], [], []
    for cx, cy, cz, size in centres:
        angle = rng.random() * math.tau
        tilt = rng.uniform(-0.55, 0.55)
        axis = (math.cos(angle), math.sin(angle), tilt)
        side = (-math.sin(angle), math.cos(angle), rng.uniform(-0.2, 0.2))
        base = len(vertices)
        for along, across, lift, u, v in ((-1, 0, 0, 0.5, 0), (0, width, 0, 1, 0.5),
                                         (1, 0, 0, 0.5, 1), (0, -width, 0, 0, 0.5),
                                         (0, 0, 0.18, 0.5, 0.5)):
            vertices.append(unreal.Vector(
                cx + size * (along * axis[0] + across * side[0]),
                cy + size * (along * axis[1] + across * side[1]),
                cz + size * (along * axis[2] + across * side[2] + lift)))
            uv.append(unreal.Vector2D(u, v))
        tint = rng.uniform(0.82, 1.12)
        tone = alt if (alt is not None and rng.random() < alt_share) else color
        colors.extend([unreal.LinearColor(tone.r * tint, tone.g * tint, tone.b * tint, 1)] * 5)
        for i in range(4):
            triangles.append(unreal.IntVector(base + 4, base + (i + 1) % 4, base + i))
    buffers = unreal.GeometryScriptSimpleMeshBuffers(vertices=vertices, triangles=triangles, vertex_colors=colors, uv0=uv)
    mesh, unused = unreal.GeometryScript_MeshEdits.append_buffers_to_mesh(unreal.DynamicMesh(), buffers, material_id=SLOT_FOLIAGE)
    return mesh


def blade_centres(rng, lobes, per_lobe, size_lo, size_hi, far):
    centres = []
    keep = max(12, per_lobe // 5) if far else per_lobe
    for radius, cx, cy, cz, squash in lobes:
        for k in range(keep):
            theta = rng.random() * math.tau
            phi = math.acos(rng.uniform(-0.35, 1.0))
            d = radius * rng.uniform(0.55, 1.05)
            centres.append((cx + d * math.sin(phi) * math.cos(theta),
                            cy + d * math.sin(phi) * math.sin(theta),
                            cz + d * math.cos(phi) * squash,
                            rng.uniform(size_lo, size_hi)))
    return centres


def fit_height(mesh, target_cm):
    """Pied a Z = 0, hauteur = stature mediane. Le facteur reste proche de 1 a l'instance."""
    box = unreal.GeometryScript_MeshQueries.get_mesh_bounding_box(mesh)
    mesh = unreal.GeometryScript_MeshTransforms.translate_mesh(mesh, unreal.Vector(0.0, 0.0, -box.min.z))
    box = unreal.GeometryScript_MeshQueries.get_mesh_bounding_box(mesh)
    height = box.max.z - box.min.z
    if height <= 1.0:
        raise RuntimeError('hero mesh degenerate height=%s' % height)
    factor = target_cm / height
    mesh = unreal.GeometryScript_MeshTransforms.scale_mesh(mesh, unreal.Vector(factor, factor, factor), unreal.Vector(0.0, 0.0, 0.0))
    return mesh


def build(spec, rng, far):
    steps = FAR_LOBE if far else HERO_LOBE
    trunk_steps = 10 if far else 18
    wood = unreal.DynamicMesh()
    for start, end, r0, r1 in spec['stems']:
        wood = merge(wood, coloured(branch(start, end, r0, r1, trunk_steps), spec['bark']))
    crown = unreal.DynamicMesh()
    for radius, cx, cy, cz, squash in spec['lobes']:
        crown = merge(crown, lobe(radius, cx, cy, cz, squash, steps))
    crown = coloured(crown, spec['foliage'])
    leaf = blades(blade_centres(rng, spec['lobes'], spec['blades'], spec['blade_cm'][0], spec['blade_cm'][1], far),
                  spec['foliage'], spec.get('alt'), spec.get('alt_share', 0.0), rng, spec['blade_width'])
    mesh = merge(merge(unreal.DynamicMesh(), wood), merge(crown, leaf))
    return shade(fit_height(mesh, spec['height_cm']))


def save_mesh(mesh, far_mesh, name):
    path = PACKAGE + '/' + name
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        unreal.EditorAssetLibrary.delete_asset(path)
    options = unreal.GeometryScriptCreateNewStaticMeshAssetOptions()
    options.set_editor_property('enable_recompute_normals', False)
    options.set_editor_property('enable_recompute_tangents', True)
    options.set_editor_property('enable_nanite', False)
    asset, outcome = unreal.GeometryScript_NewAssetUtils.create_new_static_mesh_asset_from_mesh(mesh, path, options)
    if asset is None:
        raise RuntimeError('%s: asset None (%s)' % (path, outcome))
    for slot, material_path in ((SLOT_FOLIAGE, FOLIAGE), (SLOT_WOOD, BARK)):
        material = unreal.EditorAssetLibrary.load_asset(material_path)
        if material is not None:
            asset.set_material(slot, material)
    if name != 'SM_CanopyShell':
        try:
            unreal.EditorStaticMeshLibrary.add_simple_collisions(asset, unreal.ScriptingCollisionShapeType.NDOP10_X)
        except Exception as exc:  # noqa: BLE001
            log('WARN collision %s: %s' % (name, exc))
    reductions = unreal.StaticMeshReductionOptions()
    reductions.auto_compute_lod_screen_size = False
    reductions.reduction_settings = [unreal.StaticMeshReductionSettings(percent_triangles=p, screen_size=s)
                                    for p, s in ((1.0, 1.0), (0.45, 0.28), (0.08, 0.06))]
    subsystem = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    if subsystem is None:
        editors = unreal.get_editor_subsystem(unreal.AssetEditorSubsystem)
        if editors is None:
            raise RuntimeError('LOD generation requires a live editor')
        editors.open_editor_for_assets([asset])
        subsystem = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
        editors.close_all_editors_for_asset(asset)
    if subsystem is None:
        raise RuntimeError('StaticMeshEditor subsystem missing')
    if subsystem.set_lods(asset, reductions) != 3:
        raise RuntimeError('%s: expected 3 LODs' % path)
    copy_options = unreal.GeometryScriptCopyMeshToAssetOptions(enable_recompute_tangents=True)
    unused, outcome = unreal.GeometryScript_AssetUtils.copy_mesh_to_static_mesh(
        far_mesh, asset, copy_options, unreal.GeometryScriptMeshWriteLOD(lod_index=2))
    if outcome != unreal.GeometryScriptOutcomePins.SUCCESS:
        raise RuntimeError('%s: distant LOD failed: %s' % (path, outcome))
    unreal.EditorAssetLibrary.save_asset(asset.get_path_name())
    bounds = asset.get_bounding_box()
    log('SAVED %s height_cm=%.0f radius_cm=%.0f tris=%s' % (
        name, bounds.max.z - bounds.min.z,
        max(bounds.max.x - bounds.min.x, bounds.max.y - bounds.min.y) * 0.5,
        [asset.get_num_triangles(i) for i in range(3)]))
    return bounds


def recipes():
    c = unreal.LinearColor
    pine_leaf, pine_bark = c(0.085, 0.125, 0.050, 1), c(0.105, 0.075, 0.055, 0)
    cypress_leaf, cypress_bark = c(0.030, 0.060, 0.032, 1), c(0.085, 0.070, 0.060, 0)
    oak_leaf, oak_under, oak_bark = c(0.040, 0.062, 0.034, 1), c(0.085, 0.095, 0.068, 1), c(0.065, 0.060, 0.055, 0)
    olive_leaf, olive_under = c(0.105, 0.120, 0.080, 1), c(0.155, 0.165, 0.130, 1)
    olive_bark = c(0.115, 0.105, 0.092, 0)
    out = []
    rng = random.Random('hero-aleppo')
    stems, axis = curved_stem(rng, 620.0, 8.0, 0.6, 40.0, 32.0, 14.0, 6, sweep=0.45)
    out.append(dict(name='SM_Hero_AleppoPine', height_cm=1450.0, stems=stems, bark=pine_bark, foliage=pine_leaf,
                    lobes=scatter(rng, axis[-1], 420.0, 280.0, 8, 160.0, 240.0, 0.62, top_bias=0.05),
                    blades=70, blade_cm=(11.0, 18.0), blade_width=0.28))
    rng = random.Random('hero-cypress')
    stems, axis = curved_stem(rng, 1500.0, 1.0, 0.2, 8.0, 26.0, 8.0, 7)
    lobes = []
    for i in range(14):
        t = (i + 0.5) / 14.0
        z = 180.0 + 1280.0 * t
        radius = max(40.0, 150.0 * math.sin(math.pi * min(0.999, t ** 0.75)) ** 0.8)
        lobes.append((radius, rng.uniform(-12, 12), rng.uniform(-12, 12), z, 1.7))
    out.append(dict(name='SM_Hero_Cypress', height_cm=1600.0, stems=stems, bark=cypress_bark, foliage=cypress_leaf,
                    lobes=lobes, blades=40, blade_cm=(14.0, 22.0), blade_width=0.42))
    rng = random.Random('hero-holm')
    stems, axis = curved_stem(rng, 280.0, 3.0, 1.1, 18.0, 48.0, 28.0, 4)
    top = axis[-1]
    for i in range(4):
        a = i * math.tau / 4.0
        end = (top[0] + 180.0 * math.cos(a), top[1] + 180.0 * math.sin(a), top[2] + rng.uniform(20.0, 80.0))
        stems.append((top, end, 22.0, 10.0))
    out.append(dict(name='SM_Hero_HolmOak', height_cm=1100.0, stems=stems, bark=oak_bark, foliage=oak_leaf,
                    alt=oak_under, alt_share=0.3,
                    lobes=scatter(rng, (top[0], top[1], top[2] + 40.0), 480.0, 360.0, 9, 180.0, 260.0, 0.8),
                    blades=80, blade_cm=(9.0, 15.0), blade_width=0.48))
    rng = random.Random('hero-olive')
    stems, axis = curved_stem(rng, 180.0, 7.0, 2.2, 35.0, 55.0, 28.0, 4)
    top = axis[-1]
    for i in range(3):
        a = i * math.tau / 3.0 + 0.4
        mid = (top[0] + 90.0 * math.cos(a), top[1] + 90.0 * math.sin(a), top[2] + 50.0)
        end = (top[0] + 170.0 * math.cos(a), top[1] + 170.0 * math.sin(a), top[2] + 110.0)
        stems += [(top, mid, 28.0, 16.0), (mid, end, 16.0, 8.0)]
    out.append(dict(name='SM_Hero_Olive', height_cm=625.0, stems=stems, bark=olive_bark, foliage=olive_leaf,
                    alt=olive_under, alt_share=0.45,
                    lobes=scatter(rng, (top[0], top[1], top[2] + 30.0), 340.0, 180.0, 8, 110.0, 170.0, 0.65),
                    blades=70, blade_cm=(8.0, 14.0), blade_width=0.32))
    return out


def shell_spec():
    rng = random.Random('hero-shell')
    lobes = scatter(rng, (0.0, 0.0, 620.0), 780.0, 420.0, 7, 220.0, 360.0, 0.55, shell=(0.4, 1.0), top_bias=0.2)
    return lobes


def build_shell(far):
    steps = FAR_LOBE if far else (8, 14)
    crown = unreal.DynamicMesh()
    leaf_color = unreal.LinearColor(0.045, 0.070, 0.038, 1)
    for radius, cx, cy, cz, squash in shell_spec():
        crown = merge(crown, lobe(radius, cx, cy, cz, squash, steps))
    crown = coloured(crown, leaf_color)
    rng = random.Random('hero-shell-blades' if not far else 'hero-shell-far')
    leaves = blades(blade_centres(rng, shell_spec(), 30 if far else 90, 40.0, 70.0, False),
                    leaf_color, None, 0.0, rng, 0.4)
    mesh = shade(merge(crown, leaves))
    box = unreal.GeometryScript_MeshQueries.get_mesh_bounding_box(mesh)
    return unreal.GeometryScript_MeshTransforms.translate_mesh(mesh, unreal.Vector(0.0, 0.0, -box.min.z))


def main():
    log('start')
    if unreal.get_editor_subsystem(unreal.AssetEditorSubsystem) is None:
        raise RuntimeError('Hero trees require a live editor; no assets were modified')
    if unreal.EditorAssetLibrary.load_asset(FOLIAGE) is None or unreal.EditorAssetLibrary.load_asset(BARK) is None:
        raise RuntimeError('vegetation materials missing')
    unreal.EditorAssetLibrary.make_directory(PACKAGE)
    for spec in recipes():
        rng = random.Random(spec['name'])
        save_mesh(build(spec, rng, False), build(spec, random.Random(spec['name'] + '-far'), True), spec['name'])
    save_mesh(build_shell(False), build_shell(True), 'SM_CanopyShell')
    log('HERO_TREES::PASS')


try:
    main()
except Exception as exc:  # noqa: BLE001
    unreal.log_error('HERO_TREES::FAIL %s' % exc)
finally:
    if os.environ.get('ANASTASIS_HERO_TREES_QUIT') == '1':
        unreal.SystemLibrary.quit_editor()
