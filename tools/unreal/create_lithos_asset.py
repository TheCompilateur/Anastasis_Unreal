"""
LITHOS_FORGE_001 -- formations geologiques d'ANASTASIS.

SOURCE D'AUTORITE des meshes /Game/Anastasis/Lithos/SM_Lithos_* et du
materiau /Game/Anastasis/Materials/M_AnastasisLithos. Regeneration
idempotente : retoucher a la main dans l'editeur ne survit pas.

Ce ne sont PAS des rochers posés sur le sol. Chaque mesh a un plan de
contact a Z = -50 (convention EngineBasicShapeSize / lift 50*scale) et
continue EN DESSOUS, pour que le Landscape l'avale. +X pointe dans la
paroi (amont) ; -X est la face lue depuis la vallee.

Lancer :
  tools\\unreal\\create-lithos-asset.ps1
"""
import unreal

PACKAGE_PATH = "/Game/Anastasis/Lithos"
MATERIAL_DIR = "/Game/Anastasis/Materials"
MATERIAL_NAME = "M_AnastasisLithos"
MATERIAL_PATH = MATERIAL_DIR + "/" + MATERIAL_NAME
CONTACT_Z = -50.0

# Palette pontique : calcaire humide, pas du granite chrome. L'alpha n'est
# pas utilise ; le materiau lit RGB seulement.
BEDROCK = unreal.LinearColor(0.145, 0.132, 0.118, 1.0)
STRATUM_A = unreal.LinearColor(0.310, 0.278, 0.242, 1.0)
STRATUM_B = unreal.LinearColor(0.392, 0.348, 0.298, 1.0)
STRATUM_C = unreal.LinearColor(0.248, 0.228, 0.205, 1.0)
FRACTURE = unreal.LinearColor(0.086, 0.078, 0.070, 1.0)
TALUS = unreal.LinearColor(0.265, 0.238, 0.210, 1.0)
MOSS = unreal.LinearColor(0.132, 0.168, 0.102, 1.0)
CREST = unreal.LinearColor(0.430, 0.385, 0.328, 1.0)


def log(msg):
    unreal.log("[create_lithos_asset] " + str(msg))


def color_flags():
    flags = unreal.GeometryScriptColorFlags()
    flags.red = True
    flags.green = True
    flags.blue = True
    flags.alpha = True
    return flags


FLAGS = color_flags()
PRIM = unreal.GeometryScriptPrimitiveOptions()


def coloured(mesh, color):
    return unreal.GeometryScript_VertexColors.set_mesh_constant_vertex_color(
        mesh, color, FLAGS, clear_existing=True)


def merge(target, part):
    return unreal.GeometryScript_MeshEdits.append_mesh(target, part, unreal.Transform())


def box(dx, dy, dz, x, y, z_bottom, yaw=0.0, pitch=0.0, color=STRATUM_A):
    mesh = unreal.DynamicMesh()
    xf = unreal.Transform(
        location=unreal.Vector(x, y, z_bottom),
        rotation=unreal.Rotator(pitch, yaw, 0.0),
    )
    mesh = unreal.GeometryScript_Primitives.append_box(
        mesh, PRIM, xf,
        dimension_x=dx, dimension_y=dy, dimension_z=dz,
        steps_x=0, steps_y=0, steps_z=0,
        origin=unreal.GeometryScriptPrimitiveOriginMode.BASE,
    )
    return coloured(mesh, color)


def assemble(parts):
    mesh = parts[0]
    for part in parts[1:]:
        mesh = merge(mesh, part)
    return mesh


def save_static_mesh(mesh, asset_path):
    if not unreal.EditorAssetLibrary.does_directory_exist(PACKAGE_PATH):
        unreal.EditorAssetLibrary.make_directory(PACKAGE_PATH)
    if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        unreal.EditorAssetLibrary.delete_asset(asset_path)

    options = unreal.GeometryScriptCreateNewStaticMeshAssetOptions()
    options.set_editor_property("enable_recompute_normals", True)
    options.set_editor_property("enable_recompute_tangents", True)
    options.set_editor_property("enable_nanite", False)

    asset, outcome = unreal.GeometryScript_NewAssetUtils.create_new_static_mesh_asset_from_mesh(
        mesh, asset_path, options)
    if asset is None:
        raise Exception("%s: create_new_static_mesh_asset_from_mesh a rendu None (%s)"
                        % (asset_path, outcome))

    material = unreal.EditorAssetLibrary.load_asset(MATERIAL_PATH)
    if material is not None:
        try:
            asset.set_material(0, material)
        except Exception as exc:  # noqa: BLE001
            log("WARN materiau non pose sur %s: %s" % (asset_path, exc))

    try:
        unreal.EditorStaticMeshLibrary.add_simple_collisions(
            asset, unreal.ScriptingCollisionShapeType.NDOP10_X)
    except Exception as exc:  # noqa: BLE001
        log("WARN collision simple refusee sur %s: %s" % (asset_path, exc))

    unreal.EditorAssetLibrary.save_asset(asset.get_path_name())
    bounds = asset.get_bounding_box()
    log("SAVED %s bounds z=[%.2f,%.2f] x=[%.2f,%.2f] y=[%.2f,%.2f]"
        % (asset_path, bounds.min.z, bounds.max.z, bounds.min.x, bounds.max.x,
           bounds.min.y, bounds.max.y))
    if bounds.min.z > CONTACT_Z - 1.0:
        raise Exception("%s: pas de jupe enterree (min.z=%.2f, contact=%.2f) -- "
                        "la formation se lirait posee"
                        % (asset_path, bounds.min.z, CONTACT_Z))
    if bounds.max.z < CONTACT_Z + 12.0:
        raise Exception("%s: trop bas au-dessus du contact (max.z=%.2f)"
                        % (asset_path, bounds.max.z))
    return asset


def ensure_material():
    if unreal.EditorAssetLibrary.does_asset_exist(MATERIAL_PATH):
        log("MATERIAL already exists " + MATERIAL_PATH)
        return unreal.EditorAssetLibrary.load_asset(MATERIAL_PATH)

    mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        MATERIAL_NAME, MATERIAL_DIR, unreal.Material, unreal.MaterialFactoryNew())
    mel = unreal.MaterialEditingLibrary

    vc = mel.create_material_expression(mat, unreal.MaterialExpressionVertexColor, -600, 0)
    wired = False
    for out_name in ('', 'RGB', 'Color'):
        if mel.connect_material_property(vc, out_name, unreal.MaterialProperty.MP_BASE_COLOR):
            wired = 'output=' + repr(out_name)
            break

    rough = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -350, 260)
    rough.set_editor_property('r', 0.91)
    r_rough = mel.connect_material_property(rough, '', unreal.MaterialProperty.MP_ROUGHNESS)
    spec = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -350, 460)
    spec.set_editor_property('r', 0.08)
    r_spec = mel.connect_material_property(spec, '', unreal.MaterialProperty.MP_SPECULAR)

    mat.set_editor_property('two_sided', False)
    try:
        mat.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    except Exception as exc:  # noqa: BLE001
        log('WARN shading model: %s' % exc)
    for flag in ('used_with_instanced_static_meshes', 'b_used_with_instanced_static_meshes'):
        try:
            mat.set_editor_property(flag, True)
            break
        except Exception:
            continue

    log("MATERIAL wiring base_color=%s roughness=%s specular=%s" % (wired, r_rough, r_spec))
    mel.recompile_material(mat)
    unreal.EditorAssetLibrary.save_asset(MATERIAL_PATH)
    log("MATERIAL saved " + MATERIAL_PATH)
    return mat


def vertical_wall():
    # Grande paroi stratifiee, face vers -X, masse amont +X, crete irreguliere.
    layers = [
        (38, 168, 22, 6, -8, CONTACT_Z - 32, 2.0, BEDROCK),
        (36, 172, 28, 4, -4, CONTACT_Z, 1.5, STRATUM_C),
        (34, 158, 24, 10, 10, CONTACT_Z + 26, -2.0, STRATUM_A),
        (32, 148, 22, 14, -14, CONTACT_Z + 48, 3.5, STRATUM_B),
        (28, 132, 26, 18, 8, CONTACT_Z + 68, -1.0, STRATUM_A),
        (22, 96, 18, 22, -22, CONTACT_Z + 92, 4.0, CREST),
    ]
    parts = [box(dx, dy, dz, x, y, z, yaw, 0.0, col)
             for dx, dy, dz, x, y, z, yaw, col in layers]
    parts.append(box(8, 70, 88, 2, 18, CONTACT_Z + 4, 8.0, 0.0, FRACTURE))
    return assemble(parts)


def inclined_wall():
    # Paroi deja penchee dans le mesh, pour les pentes intermediaires.
    layers = [
        (42, 154, 20, 8, 0, CONTACT_Z - 28, 0.0, 18.0, BEDROCK),
        (40, 160, 26, 4, -6, CONTACT_Z, -3.0, 16.0, STRATUM_C),
        (36, 140, 24, 12, 12, CONTACT_Z + 22, 4.0, 15.0, STRATUM_A),
        (34, 128, 22, 18, -10, CONTACT_Z + 44, -2.0, 14.0, STRATUM_B),
        (28, 100, 20, 24, 16, CONTACT_Z + 64, 5.0, 13.0, CREST),
    ]
    parts = [box(dx, dy, dz, x, y, z, yaw, pitch, col)
             for dx, dy, dz, x, y, z, yaw, pitch, col in layers]
    return assemble(parts)


def stratum():
    # Plaques horizontales decalees : la lecture de couche.
    plates = [
        (70, 180, 14, 8, -6, CONTACT_Z - 24, 1.0, BEDROCK),
        (78, 196, 12, 0, 4, CONTACT_Z, -2.0, STRATUM_C),
        (64, 170, 11, 16, -18, CONTACT_Z + 12, 3.0, STRATUM_A),
        (86, 154, 10, -8, 12, CONTACT_Z + 23, -1.5, STRATUM_B),
        (58, 142, 12, 22, -8, CONTACT_Z + 33, 4.0, STRATUM_A),
        (48, 110, 9, 10, 20, CONTACT_Z + 45, -3.0, CREST),
    ]
    parts = [box(dx, dy, dz, x, y, z, yaw, 0.0, col)
             for dx, dy, dz, x, y, z, yaw, col in plates]
    return assemble(parts)


def cornice():
    # Tablette en surplomb vers -X, ancree dans la paroi +X.
    parts = [
        box(48, 130, 40, 18, 0, CONTACT_Z - 22, 2.0, 0.0, BEDROCK),
        box(44, 138, 26, 10, -8, CONTACT_Z, -1.0, 0.0, STRATUM_C),
        box(92, 148, 16, -18, 6, CONTACT_Z + 24, 3.0, 0.0, STRATUM_A),
        box(70, 110, 12, -28, -12, CONTACT_Z + 40, -4.0, 0.0, CREST),
        box(18, 40, 20, -40, 22, CONTACT_Z + 12, 12.0, 0.0, FRACTURE),
    ]
    return assemble(parts)


def outcrop():
    # Masse basse, allongee, largement enterree : le socle qui perce.
    parts = [
        box(90, 140, 40, 8, 0, CONTACT_Z - 28, 6.0, 0.0, BEDROCK),
        box(70, 110, 32, -6, 16, CONTACT_Z - 2, -8.0, 0.0, STRATUM_C),
        box(48, 72, 26, 18, -22, CONTACT_Z + 12, 14.0, 0.0, STRATUM_A),
        box(32, 50, 18, -12, 8, CONTACT_Z + 28, -18.0, 0.0, MOSS),
    ]
    return assemble(parts)


def fractured():
    # Deux masses separees par une cassure diagonale.
    parts = [
        box(36, 88, 110, 8, -48, CONTACT_Z - 30, 7.0, 0.0, STRATUM_C),
        box(34, 80, 96, 14, 42, CONTACT_Z - 22, -5.0, 0.0, STRATUM_A),
        box(18, 36, 70, 4, -4, CONTACT_Z - 10, 28.0, 0.0, FRACTURE),
        box(22, 40, 24, 20, -62, CONTACT_Z + 70, 11.0, 0.0, CREST),
        box(20, 34, 18, 16, 58, CONTACT_Z + 62, -9.0, 0.0, STRATUM_B),
        box(28, 44, 16, 6, 8, CONTACT_Z - 28, 16.0, 0.0, BEDROCK),
    ]
    return assemble(parts)


def detached_block():
    # Bloc anguleux, pas une boule. Deja un peu bascule.
    parts = [
        box(56, 72, 48, 4, 0, CONTACT_Z - 22, 18.0, 8.0, STRATUM_A),
        box(34, 40, 28, -10, 18, CONTACT_Z + 18, -22.0, 6.0, STRATUM_B),
        box(24, 30, 18, 16, -14, CONTACT_Z - 8, 40.0, -4.0, FRACTURE),
        box(18, 22, 14, -8, -20, CONTACT_Z - 18, -30.0, 10.0, BEDROCK),
    ]
    return assemble(parts)


def talus_cluster():
    # Fragments decroissants, le plus gros amont, les petits aval (-X).
    parts = [
        box(48, 56, 44, 16, 4, CONTACT_Z - 16, 12.0, 0.0, STRATUM_C),
        box(34, 40, 32, 2, -18, CONTACT_Z - 8, -20.0, 0.0, STRATUM_A),
        box(28, 32, 24, -10, 16, CONTACT_Z - 4, 28.0, 0.0, TALUS),
        box(22, 26, 18, -22, -8, CONTACT_Z, -35.0, 0.0, TALUS),
        box(18, 20, 14, -32, 12, CONTACT_Z + 2, 50.0, 0.0, BEDROCK),
        box(14, 16, 12, -40, -14, CONTACT_Z + 4, -15.0, 0.0, FRACTURE),
        box(12, 14, 10, -28, 22, CONTACT_Z + 6, 70.0, 0.0, TALUS),
    ]
    return assemble(parts)


def transition():
    # Coin qui s'enfonce : roche -> sol. Face basse vers -X.
    parts = [
        box(80, 110, 24, 10, 0, CONTACT_Z - 16, 4.0, 8.0, BEDROCK),
        box(56, 88, 18, -8, 12, CONTACT_Z, -6.0, 10.0, STRATUM_C),
        box(36, 60, 14, -22, -10, CONTACT_Z + 10, 12.0, 12.0, MOSS),
        box(22, 36, 10, -32, 8, CONTACT_Z + 16, -18.0, 8.0, TALUS),
    ]
    return assemble(parts)


def summit():
    # Formation de crete, asymetrique, une aiguille courte pas un obelisque.
    parts = [
        box(70, 90, 28, 8, 0, CONTACT_Z - 26, 8.0, 0.0, BEDROCK),
        box(48, 62, 36, 4, -10, CONTACT_Z, -6.0, 0.0, STRATUM_C),
        box(32, 40, 44, 12, 8, CONTACT_Z + 28, 10.0, 0.0, STRATUM_A),
        box(18, 22, 38, 6, -6, CONTACT_Z + 68, -12.0, 0.0, CREST),
        box(24, 30, 20, -8, 18, CONTACT_Z + 16, 22.0, 0.0, STRATUM_B),
        box(14, 18, 16, 18, -16, CONTACT_Z + 50, -25.0, 0.0, FRACTURE),
    ]
    return assemble(parts)


FORMATIONS = [
    ("SM_Lithos_VerticalWall_01", vertical_wall, "grande paroi verticale stratifiee"),
    ("SM_Lithos_InclinedWall_01", inclined_wall, "paroi inclinee"),
    ("SM_Lithos_Stratum_01", stratum, "strates horizontales"),
    ("SM_Lithos_Cornice_01", cornice, "corniche en surplomb"),
    ("SM_Lithos_Outcrop_01", outcrop, "affleurement semi-enterre"),
    ("SM_Lithos_Fractured_01", fractured, "roche fracturee"),
    ("SM_Lithos_DetachedBlock_01", detached_block, "gros bloc detache"),
    ("SM_Lithos_TalusCluster_01", talus_cluster, "cluster d'eboulis"),
    ("SM_Lithos_Transition_01", transition, "transition roche-sol"),
    ("SM_Lithos_Summit_01", summit, "formation de sommet"),
]


def main():
    log("start")
    try:
        ensure_material()
        for name, builder, note in FORMATIONS:
            mesh = builder()
            save_static_mesh(mesh, PACKAGE_PATH + "/" + name)
            log("  %s -- %s" % (name, note))
        log("RESULT::PASS meshes=%d" % len(FORMATIONS))
        return True
    except Exception as exc:
        log("RESULT::FAIL %s" % exc)
        raise
    finally:
        try:
            unreal.SystemLibrary.quit_editor()
        except Exception as exc:  # noqa: BLE001
            log("WARN quit_editor: %s" % exc)


main()
