"""
ECOTONE_FORGE_001 -- bibliotheque de details naturels intermediaires.

PROPRIETE DES ASSETS. Ce script est la SOURCE D'AUTORITE des meshes
/Game/Anastasis/Ecotone/SM_Ecotone_* et du materiau
/Game/Anastasis/Materials/M_AnastasisStone. Il les cree, puis les recree a
l'identique a chaque run. Les retoucher a la main dans l'editeur ne survit
pas au prochain run : la forme se change ICI.

CE QUE CE N'EST PAS. Pas une deuxieme foret, pas un pack de props. Douze
silhouettes pour la couche 0,2 m a 3 m : bois mort, vegetaux de transition,
berge, fragments de pierre. Les grands arbres restent
tools/unreal/create_tree_asset.py. Les materiaux d'ecorce et de feuillage
sont LUS, jamais recrees -- ils appartiennent a cette grammaire d'arbres.

PIVOT. Base au sol, tailles reelles en uu (1 m = 100 uu). Le dressing
ecotone pose l'instance avec GroundZ - MinZ * Scale, donc un mesh dont le
bas n'est pas a Z=0 s'enfonce ou flotte d'autant. On n'utilise PAS la
convention arbre Z=[-50,+50] : une touffe de 40 uu n'est pas un tronc de
100 uu mis a l'echelle.

    UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script="tools/unreal/create_ecotone_assets.py"
"""
import unreal

PACKAGE_PATH = "/Game/Anastasis/Ecotone"
MATERIAL_DIR = "/Game/Anastasis/Materials"
BARK_PATH = MATERIAL_DIR + "/M_AnastasisBark"
VEG_PATH = MATERIAL_DIR + "/M_AnastasisVegetation"
STONE_NAME = "M_AnastasisStone"
STONE_PATH = MATERIAL_DIR + "/" + STONE_NAME

SLOT_A = 0
SLOT_WOOD = 1

WOOD = unreal.LinearColor(0.078, 0.058, 0.044, 0.0)
WOOD_BLEACH = unreal.LinearColor(0.118, 0.102, 0.082, 0.0)
NEEDLE = unreal.LinearColor(0.050, 0.110, 0.060, 1.0)
LEAF = unreal.LinearColor(0.100, 0.155, 0.066, 1.0)
GRASS = unreal.LinearColor(0.145, 0.175, 0.055, 1.0)
REED = unreal.LinearColor(0.110, 0.150, 0.062, 1.0)
WET = unreal.LinearColor(0.055, 0.115, 0.058, 1.0)
STONE = unreal.LinearColor(0.210, 0.195, 0.175, 0.0)
STONE_DARK = unreal.LinearColor(0.145, 0.138, 0.128, 0.0)
LICHEN = unreal.LinearColor(0.160, 0.185, 0.125, 0.0)


def log(msg):
    unreal.log("[create_ecotone_assets] " + str(msg))


def color_flags():
    flags = unreal.GeometryScriptColorFlags()
    flags.red = True
    flags.green = True
    flags.blue = True
    flags.alpha = True
    return flags


FLAGS = color_flags()


def prim_options(material_id):
    options = unreal.GeometryScriptPrimitiveOptions()
    options.set_editor_property("material_id", material_id)
    return options


PRIM = prim_options(SLOT_A)
PRIM_WOOD = prim_options(SLOT_WOOD)


def coloured(mesh, color):
    return unreal.GeometryScript_VertexColors.set_mesh_constant_vertex_color(
        mesh, color, FLAGS, clear_existing=True)


def merge(target, part):
    return unreal.GeometryScript_MeshEdits.append_mesh(target, part, unreal.Transform())


def taper(mesh, base_radius, top_radius, z0, z1, steps=8, location=None, rotator=None, prim=None):
    xf = unreal.Transform(
        location=location if location is not None else unreal.Vector(0.0, 0.0, z0),
        rotation=rotator if rotator is not None else unreal.Rotator(0.0, 0.0, 0.0),
    )
    z_base = 0.0 if location is not None else z0
    height = z1 - z0 if location is None else (z1 - z0)
    if location is not None:
        return unreal.GeometryScript_Primitives.append_cone(
            mesh, prim if prim is not None else PRIM, xf,
            base_radius=base_radius, top_radius=max(top_radius, 0.08), height=height,
            radial_steps=steps, height_steps=1, capped=True,
            origin=unreal.GeometryScriptPrimitiveOriginMode.BASE)
    xf = unreal.Transform(
        location=unreal.Vector(0.0, 0.0, z0),
        rotation=rotator if rotator is not None else unreal.Rotator(0.0, 0.0, 0.0),
    )
    return unreal.GeometryScript_Primitives.append_cone(
        mesh, prim if prim is not None else PRIM, xf,
        base_radius=base_radius, top_radius=max(top_radius, 0.08), height=(z1 - z0),
        radial_steps=steps, height_steps=1, capped=True,
        origin=unreal.GeometryScriptPrimitiveOriginMode.BASE)


def limb(mesh, base_r, top_r, length, z, pitch, yaw, prim=None, steps=7):
    xf = unreal.Transform(
        location=unreal.Vector(0.0, 0.0, z),
        rotation=unreal.Rotator(roll=0.0, pitch=pitch, yaw=yaw),
    )
    return unreal.GeometryScript_Primitives.append_cone(
        mesh, prim if prim is not None else PRIM, xf,
        base_radius=base_r, top_radius=max(top_r, 0.08), height=length,
        radial_steps=steps, height_steps=1, capped=True,
        origin=unreal.GeometryScriptPrimitiveOriginMode.BASE)


def box(mesh, dx, dy, dz, x, y, z, yaw=0.0, prim=None):
    xf = unreal.Transform(
        location=unreal.Vector(x, y, z),
        rotation=unreal.Rotator(roll=0.0, pitch=0.0, yaw=yaw),
    )
    return unreal.GeometryScript_Primitives.append_box(
        mesh, prim if prim is not None else PRIM, xf,
        dimension_x=dx, dimension_y=dy, dimension_z=dz,
        steps_x=0, steps_y=0, steps_z=0,
        origin=unreal.GeometryScriptPrimitiveOriginMode.BASE)


def lobe(mesh, radius, cx, cy, cz, squash_z, prim=None):
    # Squash ONLY the sphere. scale_mesh on the accumulator would stretch
    # every stem already merged -- reeds became 13 m tall on the first run.
    part = unreal.DynamicMesh()
    centre = unreal.Vector(cx, cy, cz)
    part = unreal.GeometryScript_Primitives.append_sphere_lat_long(
        part, prim if prim is not None else PRIM, unreal.Transform(location=centre),
        radius=radius, steps_phi=6, steps_theta=8,
        origin=unreal.GeometryScriptPrimitiveOriginMode.CENTER)
    part = unreal.GeometryScript_MeshTransforms.scale_mesh(
        part, unreal.Vector(1.0, 1.0, squash_z), centre)
    return merge(mesh, part)


def shade(mesh):
    split = unreal.GeometryScriptSplitNormalsOptions()
    split.set_editor_property("split_by_opening_angle", True)
    split.set_editor_property("opening_angle_deg", 45.0)
    split.set_editor_property("split_by_face_group", False)
    calc = unreal.GeometryScriptCalculateNormalsOptions()
    calc.set_editor_property("angle_weighted", True)
    calc.set_editor_property("area_weighted", True)
    return unreal.GeometryScript_Normals.compute_split_normals(mesh, split, calc)


def finish(parts):
    mesh = unreal.DynamicMesh()
    for part in parts:
        if part is not None:
            mesh = merge(mesh, part)
    return shade(mesh)


def wood_part(builder, color=WOOD):
    mesh = unreal.DynamicMesh()
    mesh = builder(mesh)
    return coloured(mesh, color)


def leaf_part(builder, color=LEAF):
    mesh = unreal.DynamicMesh()
    mesh = builder(mesh)
    return coloured(mesh, color)


# ---------------------------------------------------------------------------
# Les douze silhouettes. Chaque fonction rend un DynamicMesh.
# ---------------------------------------------------------------------------

def mesh_stump():
    def wood(m):
        m = taper(m, 16.0, 13.0, 0.0, 42.0, steps=10, prim=PRIM)
        m = taper(m, 13.0, 6.0, 42.0, 52.0, steps=8, prim=PRIM)
        for yaw, length, pitch, z, br in (
            (12.0, 32.0, 78.0, 2.0, 5.5),
            (128.0, 28.0, 74.0, 1.0, 4.8),
            (242.0, 30.0, 80.0, 2.0, 5.2),
            (300.0, 18.0, 70.0, 4.0, 3.6),
        ):
            m = limb(m, br, 1.4, length, z, pitch, yaw, prim=PRIM, steps=6)
        return m
    return finish([wood_part(wood, WOOD)])


def mesh_fallen_log():
    def wood(m):
        # Cylindre couche : pitch 90 envoie +Z local vers +X. Base a x=-120, axe a z=12.
        m = limb(m, 12.0, 8.5, 240.0, 12.0, 90.0, 0.0, prim=PRIM, steps=9)
        m = limb(m, 4.2, 1.6, 28.0, 16.0, 38.0, 18.0, prim=PRIM, steps=6)
        m = limb(m, 3.4, 1.2, 22.0, 14.0, 44.0, 200.0, prim=PRIM, steps=6)
        return m
    return finish([wood_part(wood, WOOD)])


def mesh_exposed_roots():
    def wood(m):
        m = taper(m, 9.0, 5.0, 0.0, 18.0, steps=7, prim=PRIM)
        for yaw, length, pitch, br in (
            (8.0, 78.0, 76.0, 4.6),
            (55.0, 62.0, 70.0, 3.8),
            (118.0, 70.0, 74.0, 4.2),
            (188.0, 54.0, 68.0, 3.4),
            (250.0, 66.0, 72.0, 3.9),
            (312.0, 48.0, 64.0, 3.1),
        ):
            m = limb(m, br, 1.1, length, 2.0, pitch, yaw, prim=PRIM, steps=6)
        return m
    return finish([wood_part(wood, WOOD)])


def mesh_branch_pile():
    def wood(m):
        for yaw, pitch, length, z, br in (
            (18.0, 78.0, 62.0, 4.0, 3.2),
            (70.0, 72.0, 54.0, 6.0, 2.8),
            (140.0, 82.0, 48.0, 3.0, 2.6),
            (210.0, 68.0, 58.0, 5.0, 3.0),
            (280.0, 76.0, 44.0, 7.0, 2.4),
            (330.0, 84.0, 36.0, 4.0, 2.2),
        ):
            m = limb(m, br, 0.9, length, z, pitch, yaw, prim=PRIM, steps=6)
        return m
    return finish([wood_part(wood, WOOD)])


def mesh_driftwood():
    def wood(m):
        m = limb(m, 7.5, 5.5, 110.0, 8.0, 90.0, 8.0, prim=PRIM, steps=8)
        m = limb(m, 5.5, 3.2, 70.0, 10.0, 78.0, 22.0, prim=PRIM, steps=7)
        m = limb(m, 2.8, 1.0, 26.0, 12.0, 40.0, 110.0, prim=PRIM, steps=5)
        return m
    return finish([wood_part(wood, WOOD_BLEACH)])


def mesh_bush_low():
    def wood(m):
        for yaw, length, pitch, br in (
            (20.0, 38.0, 12.0, 2.4),
            (110.0, 34.0, 16.0, 2.2),
            (200.0, 36.0, 10.0, 2.1),
            (295.0, 32.0, 14.0, 2.0),
        ):
            m = limb(m, br, 1.0, length, 0.0, pitch, yaw, prim=PRIM_WOOD, steps=6)
        return m

    def leaves(m):
        for radius, cx, cy, cz, squash in (
            (22.0, 0.0, 0.0, 28.0, 0.82),
            (16.0, 12.0, -6.0, 22.0, 0.85),
            (15.0, -10.0, 8.0, 24.0, 0.84),
            (13.0, 4.0, 10.0, 34.0, 0.80),
            (12.0, -8.0, -8.0, 18.0, 0.86),
        ):
            m = lobe(m, radius, cx, cy, cz, squash, prim=PRIM)
        return m
    return finish([wood_part(wood, WOOD), leaf_part(leaves, LEAF)])


def mesh_grass_tuft():
    def blades(m):
        for yaw, h, x, y in (
            (0.0, 48.0, 0.0, 0.0),
            (40.0, 42.0, 4.0, 2.0),
            (95.0, 52.0, -3.0, 4.0),
            (150.0, 38.0, 5.0, -3.0),
            (200.0, 46.0, -5.0, -2.0),
            (255.0, 40.0, 2.0, 5.0),
            (310.0, 36.0, -4.0, 3.0),
            (75.0, 30.0, 3.0, -5.0),
        ):
            m = box(m, 2.4, 1.1, h, x, y, 0.0, yaw, prim=PRIM)
        return m
    return finish([leaf_part(blades, GRASS)])


def mesh_reed():
    def stems(m):
        for yaw, h, r, x, y in (
            (0.0, 175.0, 1.6, 0.0, 0.0),
            (55.0, 158.0, 1.4, 7.0, 4.0),
            (130.0, 168.0, 1.5, -6.0, 6.0),
            (200.0, 148.0, 1.3, 8.0, -5.0),
            (270.0, 162.0, 1.4, -8.0, -3.0),
            (330.0, 140.0, 1.2, 4.0, 8.0),
        ):
            m = taper(m, r, r * 0.45, 0.0, h, steps=5, location=unreal.Vector(x, y, 0.0), prim=PRIM)
            m = lobe(m, 4.2, x, y, h - 2.0, 1.4, prim=PRIM)
        return m
    return finish([leaf_part(stems, REED)])


def mesh_shore_tuft():
    def mass(m):
        for yaw, h, x, y in (
            (10.0, 58.0, 0.0, 0.0),
            (70.0, 50.0, 5.0, 3.0),
            (140.0, 62.0, -4.0, 4.0),
            (210.0, 46.0, 6.0, -3.0),
            (280.0, 54.0, -6.0, -2.0),
            (330.0, 42.0, 2.0, 6.0),
        ):
            m = box(m, 3.0, 1.4, h, x, y, 0.0, yaw, prim=PRIM)
        for radius, cx, cy, cz in (
            (10.0, 2.0, 0.0, 14.0),
            (8.0, -5.0, 3.0, 10.0),
        ):
            m = lobe(m, radius, cx, cy, cz, 0.70, prim=PRIM)
        return m
    return finish([leaf_part(mass, WET)])


def mesh_sapling():
    def wood(m):
        m = taper(m, 3.6, 2.0, 0.0, 70.0, steps=7, prim=PRIM_WOOD)
        return m

    def needles(m):
        m = taper(m, 16.0, 7.0, 28.0, 78.0, steps=9, prim=PRIM)
        m = taper(m, 10.0, 0.6, 70.0, 118.0, steps=8, prim=PRIM)
        return m
    return finish([wood_part(wood, WOOD), leaf_part(needles, NEEDLE)])


def mesh_rock_cluster():
    # Two colours: dark body, paler chips. Merge after colouring each.
    body = coloured(unreal.DynamicMesh(), STONE_DARK)
    body = box(body, 28.0, 22.0, 16.0, 0.0, 0.0, 0.0, 12.0, prim=PRIM)
    chips = coloured(unreal.DynamicMesh(), STONE)
    chips = box(chips, 16.0, 14.0, 11.0, 14.0, -8.0, 0.0, 40.0, prim=PRIM)
    chips = box(chips, 14.0, 12.0, 9.0, -12.0, 10.0, 0.0, -25.0, prim=PRIM)
    lichen = coloured(unreal.DynamicMesh(), LICHEN)
    lichen = box(lichen, 11.0, 10.0, 7.0, 8.0, 12.0, 0.0, 70.0, prim=PRIM)
    lichen = box(lichen, 9.0, 8.0, 6.0, -6.0, -12.0, 0.0, -50.0, prim=PRIM)
    return finish([body, chips, lichen])


def mesh_buried_block():
    body = coloured(unreal.DynamicMesh(), STONE_DARK)
    # Origin BASE at z=-18 so a third of the mass is already below the pivot.
    body = box(body, 38.0, 30.0, 34.0, 0.0, 0.0, -18.0, 8.0, prim=PRIM)
    chips = coloured(unreal.DynamicMesh(), STONE)
    chips = box(chips, 14.0, 11.0, 8.0, 22.0, -6.0, 0.0, 28.0, prim=PRIM)
    chips = box(chips, 11.0, 9.0, 6.0, 30.0, 8.0, 0.0, -18.0, prim=PRIM)
    chips = box(chips, 8.0, 7.0, 5.0, 36.0, -4.0, 0.0, 50.0, prim=PRIM)
    return finish([body, chips])


FAMILIES = [
    ("SM_Ecotone_Stump_01", mesh_stump, "souche, racines etales, 0.5 m", (BARK_PATH,)),
    ("SM_Ecotone_FallenLog_01", mesh_fallen_log, "tronc mort couche, 2.4 m", (BARK_PATH,)),
    ("SM_Ecotone_ExposedRoots_01", mesh_exposed_roots, "racines de talus", (BARK_PATH,)),
    ("SM_Ecotone_BranchPile_01", mesh_branch_pile, "amas de branches", (BARK_PATH,)),
    ("SM_Ecotone_Driftwood_01", mesh_driftwood, "bois depose en rive", (BARK_PATH,)),
    ("SM_Ecotone_Bush_Low_01", mesh_bush_low, "buisson bas 0.6-0.9 m", (VEG_PATH, BARK_PATH)),
    ("SM_Ecotone_GrassTuft_01", mesh_grass_tuft, "touffe herbacee 0.4-0.5 m", (VEG_PATH,)),
    ("SM_Ecotone_Reed_01", mesh_reed, "roseaux 1.5-1.8 m", (VEG_PATH,)),
    ("SM_Ecotone_ShoreTuft_01", mesh_shore_tuft, "vegetal de berge", (VEG_PATH,)),
    ("SM_Ecotone_Sapling_01", mesh_sapling, "jeune conifere 1.2 m", (VEG_PATH, BARK_PATH)),
    ("SM_Ecotone_RockCluster_01", mesh_rock_cluster, "petit amas caillouteux", (STONE_PATH,)),
    ("SM_Ecotone_BuriedBlock_01", mesh_buried_block, "bloc semi-enterre + fragments aval", (STONE_PATH,)),
]


def ensure_stone_material():
    if unreal.EditorAssetLibrary.does_asset_exist(STONE_PATH):
        unreal.EditorAssetLibrary.delete_asset(STONE_PATH)
    mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        STONE_NAME, MATERIAL_DIR, unreal.Material, unreal.MaterialFactoryNew())
    mel = unreal.MaterialEditingLibrary
    vc = mel.create_material_expression(mat, unreal.MaterialExpressionVertexColor, -600, 0)
    for out_name in ("", "RGB", "Color"):
        if mel.connect_material_property(vc, out_name, unreal.MaterialProperty.MP_BASE_COLOR):
            break
    rough = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -350, 260)
    rough.set_editor_property("r", 0.92)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    spec = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -350, 460)
    spec.set_editor_property("r", 0.08)
    mel.connect_material_property(spec, "", unreal.MaterialProperty.MP_SPECULAR)
    mat.set_editor_property("two_sided", False)
    try:
        mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    except Exception as exc:  # noqa: BLE001
        log("WARN shading stone: %s" % exc)
    mel.recompile_material(mat)
    unreal.EditorAssetLibrary.save_asset(STONE_PATH)
    log("STONE_MATERIAL saved " + STONE_PATH)
    return mat


def save_static_mesh(mesh, asset_path, material_paths):
    if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        unreal.EditorAssetLibrary.delete_asset(asset_path)
    options = unreal.GeometryScriptCreateNewStaticMeshAssetOptions()
    options.set_editor_property("enable_recompute_normals", False)
    options.set_editor_property("enable_recompute_tangents", True)
    options.set_editor_property("enable_nanite", False)
    asset, outcome = unreal.GeometryScript_NewAssetUtils.create_new_static_mesh_asset_from_mesh(
        mesh, asset_path, options)
    if asset is None:
        raise Exception("%s: create_new_static_mesh_asset_from_mesh None (%s)" % (asset_path, outcome))
    for slot, path in enumerate(material_paths):
        material = unreal.EditorAssetLibrary.load_asset(path)
        if material is None:
            log("WARN materiau manquant %s" % path)
            continue
        try:
            asset.set_material(slot, material)
        except Exception as exc:  # noqa: BLE001
            log("WARN slot %d: %s" % (slot, exc))
    try:
        unreal.EditorStaticMeshLibrary.add_simple_collisions(
            asset, unreal.ScriptingCollisionShapeType.BOX)
    except Exception as exc:  # noqa: BLE001
        log("WARN collision: %s" % exc)
    unreal.EditorAssetLibrary.save_asset(asset.get_path_name())
    bounds = asset.get_bounding_box()
    height = bounds.max.z - bounds.min.z
    width = max(bounds.max.x - bounds.min.x, bounds.max.y - bounds.min.y)
    log("SAVED %s z=[%.1f,%.1f] h=%.0f w=%.0f" % (
        asset_path, bounds.min.z, bounds.max.z, height, width))
    if height < 4.0:
        raise Exception("%s: mesh degenere, hauteur=%.2f" % (asset_path, height))
    if height > 280.0:
        raise Exception("%s: hors couche 0.2-3 m, hauteur=%.1f uu" % (asset_path, height))
    return asset


def main():
    log("start")
    if not unreal.EditorAssetLibrary.does_asset_exist(BARK_PATH):
        raise Exception("M_AnastasisBark introuvable -- lancer create_tree_asset.py d abord")
    if not unreal.EditorAssetLibrary.does_asset_exist(VEG_PATH):
        raise Exception("M_AnastasisVegetation introuvable -- lancer create_tree_asset.py d abord")
    ensure_stone_material()
    if not unreal.EditorAssetLibrary.does_directory_exist(PACKAGE_PATH):
        unreal.EditorAssetLibrary.make_directory(PACKAGE_PATH)
    for name, builder, note, materials in FAMILIES:
        log("build %s -- %s" % (name, note))
        mesh = builder()
        save_static_mesh(mesh, PACKAGE_PATH + "/" + name, materials)
    log("RESULT::PASS meshes=%d" % len(FAMILIES))
    return True


main()
