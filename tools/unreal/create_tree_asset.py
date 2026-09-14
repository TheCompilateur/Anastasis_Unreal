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
BARK_MATERIAL_NAME = "M_AnastasisBark"
BARK_MATERIAL_PATH = MATERIAL_DIR + "/" + BARK_MATERIAL_NAME

# Hauteur de la boite englobante imposee a chaque mesh, et ou elle est centree.
NORMALISED_HEIGHT = 100.0
NORMALISED_BASE_Z = -50.0
NORMALISE_TOLERANCE = 0.01

# Palette. Pontique humide : ecorce sombre et mouillee, aiguilles froides et
# desaturees, feuillage plus clair et plus jaune -- c'est ce contraste qui fait
# lire conifere contre feuillu a distance, pas la forme seule.
#
# L'ALPHA N'EST PAS DE LA COULEUR : c'est le masque de feuillage. 0 sur le bois,
# 1 sur les masses foliaires. M_AnastasisVegetation s'en sert pour ne laisser la
# lumiere traverser QUE le feuillage -- sans lui, le modele d'ombrage deux faces
# rendrait aussi les troncs translucides, et un fut qui laisse passer le jour
# cesse de peser.
# L'ecorce est mesuree sur la planche anti-arnaque, pas choisie a l'estime : sous
# le soleil neutre du banc (75000 lux, EV100 14), un lineaire de 0.13 remonte a
# ~0.40 en sRGB, c'est-a-dire du beige. Un fut pontique est mouille et sombre, et
# il doit rester lisible CONTRE le sol clair autant que contre le feuillage.
BARK_DARK = unreal.LinearColor(0.062, 0.046, 0.035, 0.0)   # alpha 0 : du bois, opaque
BARK_OLD = unreal.LinearColor(0.090, 0.076, 0.064, 0.0)   # alpha 0 : du bois, opaque
NEEDLE_DARK = unreal.LinearColor(0.040, 0.094, 0.055, 1.0)   # alpha 1 : du feuillage, traverse par la lumiere
NEEDLE_MID = unreal.LinearColor(0.058, 0.120, 0.066, 1.0)   # alpha 1 : du feuillage, traverse par la lumiere
# Olive, pas citron : la planche de reference donne le feuillu plus CLAIR que le
# conifere, jamais plus vif. Un vert sature ferait un decor, pas une foret humide.
LEAF_GREEN = unreal.LinearColor(0.105, 0.160, 0.068, 1.0)   # alpha 1 : du feuillage, traverse par la lumiere
LEAF_PALE = unreal.LinearColor(0.132, 0.188, 0.082, 1.0)   # alpha 1 : du feuillage, traverse par la lumiere


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


def prim_options(material_id):
    options = unreal.GeometryScriptPrimitiveOptions()
    options.set_editor_property('material_id', material_id)
    return options


# DEUX SLOTS DE MATERIAU, ET C'EST LE POINT DE CETTE PASSE.
#
# Jusqu'ici tout l'arbre etait rendu par un seul materiau. Quand celui-ci est
# passe en MSM_TWO_SIDED_FOLIAGE, l'ecorce a herite d'un modele d'ombrage de
# feuillage : le masque alpha annulait bien sa transmission, mais la reponse
# diffuse du modele s'applique a TOUS les pixels du materiau, et sur fond clair
# les futs remontaient en valeur. C'etait semantiquement faux -- du bois n'est
# pas une feuille.
#
# Les identifiants survivent a append_mesh (verifie sur ce build : deux
# material_id donnent bien static_materials = 2), donc il suffit de marquer
# chaque primitive a la construction.
SLOT_FOLIAGE = 0
SLOT_WOOD = 1
PRIM = prim_options(SLOT_FOLIAGE)
PRIM_WOOD = prim_options(SLOT_WOOD)


def coloured(mesh, color):
    return unreal.GeometryScript_VertexColors.set_mesh_constant_vertex_color(
        mesh, color, FLAGS, clear_existing=True)


def merge(target, part):
    return unreal.GeometryScript_MeshEdits.append_mesh(target, part, unreal.Transform())


def taper(mesh, base_radius, top_radius, z0, z1, steps=9, location=None, rotator=None, prim=None):
    """Un troncon conique, base en z0, sommet en z1. Le tronc et chaque etage de
    couronne sont le meme primitif : seuls les rayons changent. Un cone parfait
    n'est jamais utilise seul -- c'est precisement la silhouette qu'on retire."""
    xf = unreal.Transform(
        location=location if location is not None else unreal.Vector(0.0, 0.0, z0),
        rotation=rotator if rotator is not None else unreal.Rotator(0.0, 0.0, 0.0))
    return unreal.GeometryScript_Primitives.append_cone(
        mesh, prim if prim is not None else PRIM, xf,
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
    """Le fut. None quand il n'y en a pas : un noisetier part en plusieurs
    tiges depuis le sol, et lui coller un tronc unique en ferait un petit arbre
    au lieu d'un arbuste -- exactement la confusion de strate qu'on evite."""
    if not segments:
        return None
    mesh = unreal.DynamicMesh()
    for seg in segments:
        mesh = taper(mesh, *seg[:4], steps=seg[4] if len(seg) > 4 else 9, prim=PRIM_WOOD)
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
            rotator=unreal.Rotator(roll=0.0, pitch=pitch, yaw=yaw), prim=PRIM_WOOD)
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


# Seuil de fracture des normales, en degres.
#
# Sous ce seuil les facettes voisines sont lissees, au-dessus l'arete reste
# franche. 45 deg separe exactement ce qu'il faut : les facettes radiales d'un
# troncon (360/11 = 33 deg) et celles d'un lobe (~36 deg) se lissent, donc une
# couronne reste ronde ; les decrochements de jupe et les jonctions bois/feuille
# (~90 deg) restent durs.
#
# C'est une CORRECTION. La version precedente laissait enable_recompute_normals
# a True dans les options de build, ce qui moyennait tout : les decrochements
# d'etage que cette grammaire construit expres etaient ensuite lisses au rendu,
# et le fut prenait un aspect caoutchouteux. On authore donc les normales ici,
# et on interdit au build de les recalculer.
SPLIT_NORMAL_ANGLE_DEG = 45.0


def shade(mesh):
    split = unreal.GeometryScriptSplitNormalsOptions()
    split.set_editor_property('split_by_opening_angle', True)
    split.set_editor_property('opening_angle_deg', SPLIT_NORMAL_ANGLE_DEG)
    split.set_editor_property('split_by_face_group', False)
    calc = unreal.GeometryScriptCalculateNormalsOptions()
    calc.set_editor_property('angle_weighted', True)
    calc.set_editor_property('area_weighted', True)
    return unreal.GeometryScript_Normals.compute_split_normals(mesh, split, calc)


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
        "name": "SM_Tree_Broadleaf_Understory_01",
        "note": "strate arbustive feuillue -- noisetier / sureau, cepee multi-tiges, sans fut",
        # Aucun tronc : quatre tiges partent du sol. C'est la difference qui fait
        # lire un arbuste plutot qu'un arbre jeune, et la planche de reference
        # separe bien les deux (strate arbustive vs sous-canopee).
        "trunk": [],
        # Tiges courtes et peu ecartees, masse posee BAS et qui les recouvre. Des
        # tiges longues et ouvertes surmontees de lobes hauts ne font pas un
        # noisetier : elles font un eventail, une plante de pot. L'arbuste doit
        # se lire comme un volume au ras du sol, pas comme une main ouverte.
        "limbs": [(2.2, 1.2, 32.0, -50.0, 9.0, 20.0),
                  (2.1, 1.1, 30.0, -50.0, 12.0, 110.0),
                  (2.0, 1.1, 28.0, -50.0, 10.0, 205.0),
                  (1.8, 1.0, 26.0, -50.0, 13.0, 295.0)],
        "tiers": [],
        "lobes": [(15.5, 0.0, 0.0, -14.0, 0.95),
                  (12.5, 6.0, -5.0, -22.0, 0.95),
                  (11.5, -5.0, 6.0, -6.0, 0.92),
                  (10.0, 3.0, 6.0, 2.0, 0.90),
                  (9.0, -6.0, -3.0, -12.0, 0.95),
                  (7.5, 2.0, -6.0, 8.0, 0.92)],
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
    {
        "name": "SM_Tree_Broadleaf_Emergent_01",
        "note": "strate emergente feuillue -- vieux hetre, fut massif, dome haut et large",
        # Le plus large de la grammaire (0.65). Un emergent feuillu n'est pas un
        # arbre de canopee plus grand : c'est un individu qui a eu la place de
        # s'etaler, et il doit se lire comme un point de repere dans le couvert.
        "trunk": [(9.0, 6.5, -50.0, -14.0), (6.5, 5.5, -14.0, -2.0)],
        # Ramure redressee et couronne resserree : a 0.80 de largeur, la premiere
        # version faisait un chapeau de champignon qui APLATISSAIT la ligne de
        # ciel au lieu de la ponctuer -- un emergent doit depasser le couvert,
        # pas le couvrir. La masse remonte donc au lieu de s'etaler.
        "limbs": [(5.0, 2.2, 34.0, -10.0, 22.0, 25.0),
                  (4.6, 2.0, 32.0, -6.0, 27.0, 120.0),
                  (4.2, 1.9, 30.0, -12.0, 20.0, 215.0),
                  (3.8, 1.7, 28.0, -4.0, 30.0, 300.0)],
        "tiers": [],
        "lobes": [(21.0, 0.0, 0.0, 16.0, 0.92),
                  (17.5, 11.0, -6.0, 8.0, 0.94),
                  (16.0, -10.0, 8.0, 14.0, 0.94),
                  (14.0, 5.0, 10.0, 26.0, 0.90),
                  (13.0, -11.0, -7.0, 10.0, 0.94),
                  (11.5, 10.0, -10.0, 22.0, 0.92),
                  (10.0, -3.0, 3.0, 34.0, 0.88),
                  (9.0, 7.0, 5.0, 30.0, 0.90)],
        "bark": BARK_OLD,
        "foliage": LEAF_GREEN,
    },
]

# Repli code de AnastasisPresentationRegistry.cpp : ce chemin doit rester
# valide, sinon un checkout sans data asset ne rend plus rien. Il recoit la
# silhouette de canopee, c'est-a-dire l'arbre le plus representatif.
ALIAS_OF_GENERIC = "SM_Tree_Conifer_Canopy_01"
GENERIC_NAME = "SM_Tree_Generic_01"


def build_family(spec):
    parts = []
    trunk = build_trunk(spec["trunk"], spec["bark"])
    if trunk is not None:
        parts.append(trunk)
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
    # Fractionner APRES la fusion : les aretes qui comptent le plus sont celles
    # entre deux sous-parties (bois contre feuillage), et elles n'existent pas
    # tant que les morceaux sont separes.
    return shade(normalise(mesh, spec["name"]))


def save_static_mesh(mesh, asset_path):
    if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        unreal.EditorAssetLibrary.delete_asset(asset_path)

    options = unreal.GeometryScriptCreateNewStaticMeshAssetOptions()
    # FAUX AMI : a True, le build recalcule et JETTE les normales fractionnees
    # authorees par shade(). Le mesh repartirait tout lisse et .5b n'aurait
    # servi a rien -- sans la moindre erreur pour le signaler.
    options.set_editor_property("enable_recompute_normals", False)
    options.set_editor_property("enable_recompute_tangents", True)
    # Nanite reste OFF : le pipeline est HISM + LOD, et la direction artistique
    # refuse une feature qui n'a pas gagne son existence.
    options.set_editor_property("enable_nanite", False)

    asset, outcome = unreal.GeometryScript_NewAssetUtils.create_new_static_mesh_asset_from_mesh(
        mesh, asset_path, options)
    if asset is None:
        raise Exception("%s: create_new_static_mesh_asset_from_mesh a rendu None (%s)"
                        % (asset_path, outcome))

    # Les materiaux poses sur l'ASSET sont le repli : le registre les surcharge
    # sur le composant HISM. Mais un slot laisse vide rendrait en damier gris si
    # la donnee venait a manquer -- et un tronc en damier est pire qu'un tronc
    # mal ombre. On cable donc les deux ici aussi.
    for slot, path in ((SLOT_FOLIAGE, MATERIAL_PATH), (SLOT_WOOD, BARK_MATERIAL_PATH)):
        material = unreal.EditorAssetLibrary.load_asset(path)
        if material is None:
            continue
        try:
            asset.set_material(slot, material)
        except Exception as exc:  # noqa: BLE001
            log("WARN slot %d non pose sur %s: %s" % (slot, asset_path, exc))

    try:
        slots = len(asset.get_editor_property('static_materials'))
        if slots != 2:
            raise Exception("%s: %d slot(s) de materiau au lieu de 2 -- le bois et le "
                            "feuillage ne sont plus separes" % (asset_path, slots))
        log("  slots=%d (0=feuillage, 1=bois)" % slots)
    except Exception as exc:
        raise Exception(str(exc))

    try:
        unreal.EditorStaticMeshLibrary.add_simple_collisions(
            asset, unreal.ScriptingCollisionShapeType.NDOP10_X)
    except Exception as exc:  # noqa: BLE001 -- best effort, comme la version precedente
        log("WARN collision simple refusee sur %s: %s" % (asset_path, exc))

    unreal.EditorAssetLibrary.save_asset(asset.get_path_name())
    bounds = asset.get_bounding_box()
    log("SAVED %s bounds z=[%.2f,%.2f]" % (asset_path, bounds.min.z, bounds.max.z))
    return asset


# Teinte de transmission. Une feuille a contre-jour vire au vert-jaune : la
# lumiere qui la traverse perd le bleu. On multiplie donc la couleur de base par
# ce facteur plutot que de peindre une seconde couleur a la main -- ainsi un
# conifere sombre transmet sombre et un hetre clair transmet clair, sans avoir a
# tenir deux palettes coherentes entre elles.
#
# CALIBRE, PAS CHOISI. Un premier jet a (2.6, 2.1, 0.8) faisait virer les
# feuillus au citron et effacait la masse sombre des coniferes : la transmission
# ne revelait plus le volume, elle eclaircissait tout. C'est precisement
# l'oversaturation que la direction artistique refuse, et une passe qui gagne en
# spectacle ce qu'elle perd en matiere n'est pas un gain. La chaleur reste donc
# juste au-dessus de 1 : visible la ou la couronne est fine et a contre-jour,
# invisible ailleurs.
TRANSMISSION_WARMTH = unreal.LinearColor(1.5, 1.25, 0.55, 1.0)


def ensure_material():
    """VertexColor -> BaseColor, et surtout : feuillage deux faces.

    Une masse de feuillage opaque lit comme du plastique, quelle que soit sa
    silhouette. Le modele MSM_TWO_SIDED_FOLIAGE laisse la lumiere TRAVERSER la
    couronne, ce que la planche de reference appelle "lumiere filtree" et
    "volumetrie". C'est le seul changement qui transforme l'image sans toucher
    un seul sommet.

    L'alpha des sommets sert de masque : 0 sur le bois, 1 sur le feuillage. Sans
    ce masque, les troncs deviendraient translucides eux aussi.

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

    vc = mel.create_material_expression(mat, unreal.MaterialExpressionVertexColor, -900, 0)
    wired = False
    base_output = None
    for out_name in ('', 'RGB', 'Color'):
        if mel.connect_material_property(vc, out_name, unreal.MaterialProperty.MP_BASE_COLOR):
            wired = 'output=' + repr(out_name)
            base_output = out_name
            break

    # Transmission = couleur de base x chaleur x masque de feuillage.
    warm = mel.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector, -900, 220)
    warm.set_editor_property('constant', TRANSMISSION_WARMTH)
    tinted = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -600, 140)
    mel.connect_material_expressions(vc, base_output if base_output is not None else '', tinted, 'A')
    mel.connect_material_expressions(warm, '', tinted, 'B')
    masked = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -380, 140)
    mel.connect_material_expressions(tinted, '', masked, 'A')
    mel.connect_material_expressions(vc, 'A', masked, 'B')
    r_sss = mel.connect_material_property(masked, '', unreal.MaterialProperty.MP_SUBSURFACE_COLOR)

    rough = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -350, 260)
    rough.set_editor_property('r', 0.82)
    r_rough = mel.connect_material_property(rough, '', unreal.MaterialProperty.MP_ROUGHNESS)

    spec = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -350, 460)
    spec.set_editor_property('r', 0.18)
    r_spec = mel.connect_material_property(spec, '', unreal.MaterialProperty.MP_SPECULAR)

    # Deux faces : les etages de couronne sont des troncons fins vus des deux
    # cotes des qu'on entre sous le couvert. Le modele feuillage l'exige de toute
    # facon -- la transmission n'a de sens que si la face arriere est rendue.
    mat.set_editor_property('two_sided', True)

    shading = 'ABSENT'
    try:
        mat.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_TWO_SIDED_FOLIAGE)
        shading = str(mat.get_editor_property('shading_model'))
    except Exception as exc:  # noqa: BLE001
        log('WARN modele d ombrage non pose: %s' % exc)

    # Relu depuis l'asset, pas suppose : un set_editor_property qui echoue en
    # silence laisserait un materiau opaque et .5a n'aurait servi a rien.
    log("MATERIAL wiring base_color=%s subsurface=%s roughness=%s specular=%s shading=%s"
        % (wired, r_sss, r_rough, r_spec, shading))
    mel.recompile_material(mat)
    unreal.EditorAssetLibrary.save_asset(MATERIAL_PATH)
    log("MATERIAL saved " + MATERIAL_PATH)
    return mat


def ensure_bark_material():
    """Default Lit. Du bois : opaque, mat, sans transmission.

    C'est tout l'interet de la passe -- l'ecorce cesse d'etre rendue par un
    modele d'ombrage de feuillage. Meme langage que le reste du projet : la
    couleur de sommet EST la semantique, aucune texture.
    """
    if unreal.EditorAssetLibrary.does_asset_exist(BARK_MATERIAL_PATH):
        unreal.EditorAssetLibrary.delete_asset(BARK_MATERIAL_PATH)

    mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        BARK_MATERIAL_NAME, MATERIAL_DIR, unreal.Material, unreal.MaterialFactoryNew())
    mel = unreal.MaterialEditingLibrary

    vc = mel.create_material_expression(mat, unreal.MaterialExpressionVertexColor, -600, 0)
    wired = False
    for out_name in ('', 'RGB', 'Color'):
        if mel.connect_material_property(vc, out_name, unreal.MaterialProperty.MP_BASE_COLOR):
            wired = 'output=' + repr(out_name)
            break

    # Plus rugueux et moins speculaire que le feuillage : une ecorce humide
    # n'accroche pas la lumiere comme une feuille cireuse.
    rough = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -350, 260)
    rough.set_editor_property('r', 0.93)
    r_rough = mel.connect_material_property(rough, '', unreal.MaterialProperty.MP_ROUGHNESS)
    spec = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -350, 460)
    spec.set_editor_property('r', 0.10)
    r_spec = mel.connect_material_property(spec, '', unreal.MaterialProperty.MP_SPECULAR)

    # Le bois est un volume ferme : une seule face suffit, et ca evite de payer
    # le rendu des faces arriere sur chaque fut.
    mat.set_editor_property('two_sided', False)

    shading = 'ABSENT'
    try:
        mat.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
        shading = str(mat.get_editor_property('shading_model'))
    except Exception as exc:  # noqa: BLE001
        log('WARN modele d ombrage ecorce non pose: %s' % exc)

    log("BARK_MATERIAL wiring base_color=%s roughness=%s specular=%s shading=%s"
        % (wired, r_rough, r_spec, shading))
    mel.recompile_material(mat)
    unreal.EditorAssetLibrary.save_asset(BARK_MATERIAL_PATH)
    log("BARK_MATERIAL saved " + BARK_MATERIAL_PATH)
    return mat


def main():
    log("start")
    ensure_material()
    ensure_bark_material()

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
