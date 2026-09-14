"""
ANASTASIS_UNREAL_TREE_FORM_001 -- la grammaire d'arbres d'ANASTASIS.

PROPRIETE DES ASSETS. Ce script est la SOURCE D'AUTORITE des meshes
/Game/Anastasis/Vegetation/SM_Tree_* et du materiau
/Game/Anastasis/Materials/M_AnastasisVegetation, meme convention que
tools/unreal/observe-slice.py pour M_AnastasisSlice. Il les cree, puis les
recree a l'identique a chaque run (idempotent par regeneration). Les retoucher
a la main dans l'editeur ne survit pas au prochain run : la forme d'un arbre se
change ICI, dans les chiffres, pas au clic.

CE QU'IL REMPLACE. Il produisait un seul SM_Tree_Generic_01 : un cylindre de
tronc plus un cone effile. A distance, la jupe du cone (rayon 34 pour une
hauteur de 100) mangeait le tronc et la foret se lisait comme un champ de cones
verts identiques mis a l'echelle. Six silhouettes construites remplacent ce
cone, une par strate de la planche de reference
docs/visual/reference/pontique-etat-zero-3-stratification-forestiere.png
(emergente / canopee / sous-canopee / arbustive, conifere et feuillu).

CONVENTION DE PIVOT -- NE PAS LA CASSER. Chaque mesh est normalise pour que sa
boite englobante couvre EXACTEMENT Z = [-50, +50], soit les 100 uu que
AnastasisPresentation::EngineBasicShapeSize suppose. Deux chemins de code en
dependent et un test scelle le verifie
(Anastasis.Terrain.DressingRestsOnRenderedGround, qui recalcule le lift comme
0.5 * 100 * Scale) :

    lift legacy   = 0.5 * EngineBasicShapeSize * Scale
    lift ecologie = -MeshBounds.Min.Z * Scale

Les deux ne coincident que si Min.Z vaut -50. La normalisation est donc
verifiee, pas supposee : un ecart superieur a NORMALISE_TOLERANCE fait echouer
le script au lieu de produire des arbres qui flottent.

En XY le mesh n'est PAS recentre, contrairement a la version precedente : une
couronne asymetrique recentree deplacerait le tronc hors du pivot, et l'arbre
ne pousserait plus la ou la simulation l'a place. Le tronc tient l'axe.

COULEUR. Chaque sous-partie porte une couleur de sommet (ecorce / aiguilles /
feuillage) et le materiau la lit. C'est le meme langage que M_AnastasisSlice
pour le sol : la couleur EST la semantique, aucune texture decorative. Sans ce
materiau, le MID de teinte unique de l'archetype peindrait aussi le tronc en
vert et le tronc cesserait d'exister visuellement.

Signatures confirmees contre CE build (UE 5.8.2, CL 56702186) -- ne pas
supposer que la doc Epic d'une autre version correspond.

Lancer headless :
  UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script="tools/unreal/create_tree_asset.py"
"""
import unreal

PACKAGE_PATH = "/Game/Anastasis/Vegetation"
MATERIAL_DIR = "/Game/Anastasis/Materials"
MATERIAL_NAME = "M_AnastasisVegetation"
MATERIAL_PATH = MATERIAL_DIR + "/" + MATERIAL_NAME

# Hauteur de la boite englobante imposee a chaque mesh, et ou elle est centree.
NORMALISED_HEIGHT = 100.0
NORMALISED_BASE_Z = -50.0
NORMALISE_TOLERANCE = 0.01

# Palette. Pontique humide : ecorce sombre et mouillee, aiguilles froides et
# desaturees, feuillage plus clair et plus jaune -- c'est ce contraste qui fait
# lire conifere contre feuillu a distance, pas la forme seule.
# L'ecorce est mesuree sur la planche anti-arnaque, pas choisie a l'estime : sous
# le soleil neutre du banc (75000 lux, EV100 14), un lineaire de 0.13 remonte a
# ~0.40 en sRGB, c'est-a-dire du beige. Un fut pontique est mouille et sombre, et
# il doit rester lisible CONTRE le sol clair autant que contre le feuillage.
BARK_DARK = unreal.LinearColor(0.062, 0.046, 0.035, 1.0)
BARK_OLD = unreal.LinearColor(0.090, 0.076, 0.064, 1.0)
NEEDLE_DARK = unreal.LinearColor(0.040, 0.094, 0.055, 1.0)
NEEDLE_MID = unreal.LinearColor(0.058, 0.120, 0.066, 1.0)
# Olive, pas citron : la planche de reference donne le feuillu plus CLAIR que le
# conifere, jamais plus vif. Un vert sature ferait un decor, pas une foret humide.
LEAF_GREEN = unreal.LinearColor(0.105, 0.160, 0.068, 1.0)
LEAF_PALE = unreal.LinearColor(0.132, 0.188, 0.082, 1.0)


def log(msg):
    unreal.log("[create_tree_asset] " + str(msg))


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


def taper(mesh, base_radius, top_radius, z0, z1, steps=9, location=None, rotator=None):
    """Un troncon conique, base en z0, sommet en z1. Le tronc et chaque etage de
    couronne sont le meme primitif : seuls les rayons changent. Un cone parfait
    n'est jamais utilise seul -- c'est precisement la silhouette qu'on retire."""
    xf = unreal.Transform(
        location=location if location is not None else unreal.Vector(0.0, 0.0, z0),
        rotation=rotator if rotator is not None else unreal.Rotator(0.0, 0.0, 0.0))
    return unreal.GeometryScript_Primitives.append_cone(
        mesh, PRIM, xf,
        base_radius=base_radius, top_radius=max(top_radius, 0.05), height=(z1 - z0),
        radial_steps=steps, height_steps=1, capped=True,
        origin=unreal.GeometryScriptPrimitiveOriginMode.BASE)


def lobe(mesh, radius, cx, cy, cz, squash_z):
    """Une masse de feuillage : sphere ecrasee en Z.

    Il en faut SIX ou SEPT, de rayons decroissants et largement imbriques. Trois
    gros lobes se lisent encore comme trois boules empilees -- un brocoli, pas un
    hetre. Au-dela de cinq, les silhouettes fusionnent et seul le bord de la
    couronne reste irregulier, ce qui est precisement ce qu'on cherche.

    L'ecrasement reste proche de 1 : ecrase trop fort, la couronne devient une
    galette posee sur un baton -- une sucette a nouveau, juste plus plate. Un
    hetre porte une masse HAUTE, pas un parasol."""
    centre = unreal.Vector(cx, cy, cz)
    mesh = unreal.GeometryScript_Primitives.append_sphere_lat_long(
        mesh, PRIM, unreal.Transform(location=centre),
        radius=radius, steps_phi=7, steps_theta=10,
        origin=unreal.GeometryScriptPrimitiveOriginMode.CENTER)
    return unreal.GeometryScript_MeshTransforms.scale_mesh(
        mesh, unreal.Vector(1.0, 1.0, squash_z), centre)


def build_trunk(segments, color):
    mesh = unreal.DynamicMesh()
    for seg in segments:
        mesh = taper(mesh, *seg[:4], steps=seg[4] if len(seg) > 4 else 9)
    return coloured(mesh, color)


def build_limbs(limbs, color):
    """Branches maitresses inclinees. Le pitch tourne +Z local vers +X, donc un
    pitch positif monte toujours : une branche ne peut pas partir sous terre."""
    if not limbs:
        return None
    mesh = unreal.DynamicMesh()
    for base_r, top_r, length, z, pitch, yaw in limbs:
        mesh = taper(
            mesh, base_r, top_r, 0.0, length, steps=7,
            location=unreal.Vector(0.0, 0.0, z),
            rotator=unreal.Rotator(roll=0.0, pitch=pitch, yaw=yaw))
    return coloured(mesh, color)


def build_tiers(tiers, color):
    mesh = unreal.DynamicMesh()
    for base_r, top_r, z0, z1 in tiers:
        mesh = taper(mesh, base_r, top_r, z0, z1, steps=11)
    return coloured(mesh, color)


def build_crown_lobes(lobes, color):
    mesh = unreal.DynamicMesh()
    for radius, cx, cy, cz, squash in lobes:
        mesh = lobe(mesh, radius, cx, cy, cz, squash)
    return coloured(mesh, color)


def normalise(mesh, name):
    """Ramene la boite a Z = [-50, +50] par une mise a l'echelle UNIFORME : les
    proportions dessinees ci-dessous survivent, seule la taille globale bouge.
    Un ecart residuel est une erreur, pas un arrondi a ignorer."""
    box = unreal.GeometryScript_MeshQueries.get_mesh_bounding_box(mesh)
    height = box.max.z - box.min.z
    if height <= 1.0:
        raise Exception("%s: mesh degenere, hauteur=%s" % (name, height))
    factor = NORMALISED_HEIGHT / height
    mesh = unreal.GeometryScript_MeshTransforms.scale_mesh(
        mesh, unreal.Vector(factor, factor, factor), unreal.Vector(0.0, 0.0, box.min.z))
    mesh = unreal.GeometryScript_MeshTransforms.translate_mesh(
        mesh, unreal.Vector(0.0, 0.0, NORMALISED_BASE_Z - box.min.z))
    after = unreal.GeometryScript_MeshQueries.get_mesh_bounding_box(mesh)
    low = abs(after.min.z - NORMALISED_BASE_Z)
    high = abs(after.max.z - (NORMALISED_BASE_Z + NORMALISED_HEIGHT))
    if low > NORMALISE_TOLERANCE or high > NORMALISE_TOLERANCE:
        raise Exception("%s: normalisation ratee, bas=%.4f haut=%.4f" % (name, low, high))
    width = max(after.max.x - after.min.x, after.max.y - after.min.y)
    log("%s normalise: z=[%.2f,%.2f] largeur=%.1f ratio_largeur/hauteur=%.2f"
        % (name, after.min.z, after.max.z, width, width / NORMALISED_HEIGHT))
    return mesh


# ---------------------------------------------------------------------------
# LA GRAMMAIRE
#
# Espace de dessin : base a -50, hauteur visee ~100, la normalisation corrige
# l'ecart. Ce qui compte dans ces chiffres n'est pas la valeur absolue mais
# trois rapports, et ce sont eux la direction artistique :
#
#   fut degage   quelle fraction de la hauteur est du tronc nu. Il monte avec
#                l'age : un semis est feuillu jusqu'au sol, un arbre emergent a
#                perdu ses branches basses. C'est ce qui fait lire l'age.
#   largeur      couronne / hauteur. Les coniferes pontiques (epicea d'Orient,
#                sapin) sont des fleches : 0.26 a 0.43. Les feuillus (hetre,
#                charme) sont des domes : 0.50 et plus.
#   etages       un cone unique n'a pas d'etage. Ici la couronne est une pile de
#                troncons qui se chevauchent, et chaque troncon REPART plus large
#                que ne finit celui du dessous : c'est ce decrochement, pas le
#                chevauchement, qui fait une jupe visible a moyenne distance.
#                Sans lui les etages se fondent et la silhouette redevient un cone.
# ---------------------------------------------------------------------------
FAMILIES = [
    {
        "name": "SM_Tree_Conifer_Understory_01",
        "note": "strate arbustive / jeune semis -- fleche fine, feuillue jusqu'en bas",
        "trunk": [(2.4, 1.5, -50.0, -28.0, 7)],
        "limbs": [],
        "tiers": [(13.5, 5.0, -32.0, 0.0), (8.5, 0.4, -4.0, 50.0)],
        "lobes": [],
        "bark": BARK_DARK,
        "foliage": NEEDLE_MID,
    },
    {
        "name": "SM_Tree_Conifer_Subcanopy_01",
        "note": "sous-canopee -- epicea intermediaire, trois etages, tronc lisible",
        "trunk": [(3.4, 2.1, -50.0, -20.0)],
        "limbs": [],
        "tiers": [(17.5, 8.5, -25.0, -3.0), (13.5, 6.0, -6.0, 20.0), (9.0, 0.4, 16.0, 50.0)],
        "lobes": [],
        "bark": BARK_DARK,
        "foliage": NEEDLE_MID,
    },
    {
        "name": "SM_Tree_Conifer_Canopy_01",
        "note": "canopee -- epicea d'Orient dominant, fut degage sur 44%, quatre etages",
        "trunk": [(4.2, 2.4, -50.0, -6.0)],
        "limbs": [(3.0, 1.2, 17.0, -10.0, 38.0, 25.0)],
        "tiers": [(19.5, 9.0, -14.0, 4.0), (16.0, 7.0, 0.0, 20.0),
                  (12.0, 5.0, 16.0, 36.0), (7.5, 0.4, 32.0, 50.0)],
        "lobes": [],
        "bark": BARK_DARK,
        "foliage": NEEDLE_DARK,
    },
    {
        "name": "SM_Tree_Conifer_Emergent_01",
        "note": "strate emergente -- sapin ancien, fut massif sur 54%, cime emoussee",
        "trunk": [(7.6, 5.2, -50.0, -18.0), (5.2, 4.4, -18.0, 4.0)],
        "limbs": [(4.4, 1.8, 24.0, -14.0, 34.0, 20.0),
                  (3.8, 1.6, 21.0, -6.0, 40.0, 155.0),
                  (3.2, 1.4, 18.0, -11.0, 36.0, 268.0)],
        # Cime emoussee, pas une aiguille : un arbre senescent a perdu sa fleche.
        "tiers": [(22.0, 13.0, -2.0, 13.0), (18.0, 10.0, 10.0, 28.0),
                  (13.0, 6.5, 25.0, 42.0), (8.5, 3.2, 39.0, 50.0)],
        "lobes": [],
        "bark": BARK_OLD,
        "foliage": NEEDLE_DARK,
    },
    {
        "name": "SM_Tree_Broadleaf_Subcanopy_01",
        "note": "sous-canopee feuillue -- charme / erable, petit dome asymetrique",
        "trunk": [(3.2, 2.3, -50.0, -16.0)],
        "limbs": [(2.4, 1.2, 15.0, -18.0, 42.0, 60.0),
                  (2.2, 1.1, 14.0, -17.0, 45.0, 230.0)],
        "tiers": [],
        "lobes": [(18.0, 0.0, 0.0, 10.0, 0.95),
                  (14.0, 7.0, -5.0, 2.0, 0.95),
                  (12.5, -6.0, 6.0, 18.0, 0.92),
                  (11.0, 3.0, 8.0, 26.0, 0.90),
                  (9.5, -8.0, -4.0, 12.0, 0.95),
                  (8.5, 5.0, -7.0, 22.0, 0.92)],
        "bark": BARK_DARK,
        "foliage": LEAF_GREEN,
    },
    {
        "name": "SM_Tree_Broadleaf_Canopy_01",
        "note": "canopee feuillue -- hetre / chene mature, fut fourchu, large dome",
        "trunk": [(5.4, 3.6, -50.0, -8.0)],
        "limbs": [(3.4, 1.6, 27.0, -12.0, 35.0, 15.0),
                  (3.0, 1.5, 25.0, -14.0, 40.0, 140.0),
                  (2.6, 1.3, 22.0, -10.0, 44.0, 255.0)],
        "tiers": [],
        "lobes": [(21.0, 0.0, 0.0, 12.0, 0.92),
                  (17.0, 11.0, -5.0, 4.0, 0.94),
                  (15.5, -9.0, 8.0, 18.0, 0.92),
                  (13.5, 4.0, 10.0, 28.0, 0.88),
                  (12.0, -11.0, -6.0, 10.0, 0.94),
                  (10.5, 7.0, -10.0, 22.0, 0.92),
                  (9.0, -3.0, 2.0, 34.0, 0.86)],
        "bark": BARK_OLD,
        "foliage": LEAF_PALE,
    },
]

# Repli code de AnastasisPresentationRegistry.cpp : ce chemin doit rester
# valide, sinon un checkout sans data asset ne rend plus rien. Il recoit la
# silhouette de canopee, c'est-a-dire l'arbre le plus representatif.
ALIAS_OF_GENERIC = "SM_Tree_Conifer_Canopy_01"
GENERIC_NAME = "SM_Tree_Generic_01"


def build_family(spec):
    parts = []
    parts.append(build_trunk(spec["trunk"], spec["bark"]))
    limbs = build_limbs(spec["limbs"], spec["bark"])
    if limbs is not None:
        parts.append(limbs)
    if spec["tiers"]:
        parts.append(build_tiers(spec["tiers"], spec["foliage"]))
    if spec["lobes"]:
        parts.append(build_crown_lobes(spec["lobes"], spec["foliage"]))

    mesh = unreal.DynamicMesh()
    for part in parts:
        mesh = merge(mesh, part)
    return normalise(mesh, spec["name"])


def save_static_mesh(mesh, asset_path):
    if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        unreal.EditorAssetLibrary.delete_asset(asset_path)

    options = unreal.GeometryScriptCreateNewStaticMeshAssetOptions()
    options.set_editor_property("enable_recompute_normals", True)
    options.set_editor_property("enable_recompute_tangents", True)
    # Nanite reste OFF : le pipeline est HISM + LOD, et la direction artistique
    # refuse une feature qui n'a pas gagne son existence.
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
            # Pas bloquant : le registre pose de toute facon le materiau sur le
            # composant HISM (MaterialOverride). Ceci ne sert qu'a rendre le
            # mesh correct quand on l'ouvre seul dans l'editeur.
            log("WARN materiau par defaut non pose sur %s: %s" % (asset_path, exc))

    try:
        unreal.EditorStaticMeshLibrary.add_simple_collisions(
            asset, unreal.ScriptingCollisionShapeType.NDOP10_X)
    except Exception as exc:  # noqa: BLE001 -- best effort, comme la version precedente
        log("WARN collision simple refusee sur %s: %s" % (asset_path, exc))

    unreal.EditorAssetLibrary.save_asset(asset.get_path_name())
    bounds = asset.get_bounding_box()
    log("SAVED %s bounds z=[%.2f,%.2f]" % (asset_path, bounds.min.z, bounds.max.z))
    return asset


def ensure_material():
    """VertexColor -> BaseColor, rugosite et specular fixes.

    Deliberement distinct de M_AnastasisSlice, qui appartient a observe-slice.py
    et sert le sol : partager un materiau entre le sol et la vegetation lierait
    deux chantiers qui avancent separement. Le langage, lui, est le meme --
    la couleur de sommet EST la semantique.
    """
    if unreal.EditorAssetLibrary.does_asset_exist(MATERIAL_PATH):
        unreal.EditorAssetLibrary.delete_asset(MATERIAL_PATH)

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
    rough.set_editor_property('r', 0.82)
    r_rough = mel.connect_material_property(rough, '', unreal.MaterialProperty.MP_ROUGHNESS)

    spec = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -350, 460)
    spec.set_editor_property('r', 0.18)
    r_spec = mel.connect_material_property(spec, '', unreal.MaterialProperty.MP_SPECULAR)

    # Deux faces : les etages de couronne sont des troncons fins vus des deux
    # cotes des qu'on entre sous le couvert.
    mat.set_editor_property('two_sided', True)

    log("MATERIAL wiring base_color=%s roughness=%s specular=%s" % (wired, r_rough, r_spec))
    mel.recompile_material(mat)
    unreal.EditorAssetLibrary.save_asset(MATERIAL_PATH)
    log("MATERIAL saved " + MATERIAL_PATH)
    return mat


def main():
    log("start")
    ensure_material()

    built = {}
    for spec in FAMILIES:
        mesh = build_family(spec)
        path = PACKAGE_PATH + "/" + spec["name"]
        save_static_mesh(mesh, path)
        built[spec["name"]] = mesh
        log("  %s -- %s" % (spec["name"], spec["note"]))

    # L'alias garde le chemin du repli code vivant sans dupliquer une septieme
    # silhouette : c'est la meme geometrie, sous le nom que le C++ connait.
    save_static_mesh(built[ALIAS_OF_GENERIC], PACKAGE_PATH + "/" + GENERIC_NAME)
    log("ALIAS %s <- %s" % (GENERIC_NAME, ALIAS_OF_GENERIC))

    log("RESULT::PASS meshes=%d" % (len(FAMILIES) + 1))
    return True


main()
