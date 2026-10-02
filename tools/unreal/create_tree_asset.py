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

Lancer dans un editeur dedie (la reduction LOD exige StaticMeshEditor) :
  UnrealEditor.exe <uproject> -RenderOffscreen -ExecCmds="py <chemin absolu de ce script>"
Ou dans la console Python de cet editeur. Le commandlet Python est refuse
avant toute ecriture. Fermer l'editeur apres la generation si lance en batch.
"""
import math
import os
import random
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


def lobe_steps(radius):
    """Tessellation proportionnelle au rayon du lobe.

    MESUREE, PAS SUPPOSEE. Avec une tessellation fixe (7 x 10 quel que soit le
    rayon), les feuillus pesaient 74% des triangles pour 49% des instances, et
    un arbuste de 1,4 m coutait 1,54x un emergent conifere de 6,3 m. Le cout
    etait donc INVERSE par rapport a l'importance visuelle : un petit lobe
    payait autant qu'un grand.

    Le plancher n'est pas negociable : au-dela de 45 degres entre deux facettes,
    compute_split_normals casse l'arete au lieu de la lisser, et la couronne
    deviendrait un caillou facette. D'ou steps_phi >= 5 (180/5 = 36 deg) et
    steps_theta fixe a 9 (360/9 = 40 deg) -- tous deux sous le seuil.
    """
    phi = int(min(8, max(5, round(radius / 3.5))))
    return phi, 9


def lobe(mesh, radius, cx, cy, cz, squash_z, steps=None):
    """Une masse de feuillage : sphere ecrasee en Z.

    Il en faut SIX ou SEPT, de rayons decroissants et largement imbriques. Trois
    gros lobes se lisent encore comme trois boules empilees -- un brocoli, pas un
    hetre. Au-dela de cinq, les silhouettes fusionnent et seul le bord de la
    couronne reste irregulier, ce qui est precisement ce qu'on cherche.

    L'ecrasement reste proche de 1 : ecrase trop fort, la couronne devient une
    galette posee sur un baton -- une sucette a nouveau, juste plus plate. Un
    hetre porte une masse HAUTE, pas un parasol."""
    centre = unreal.Vector(cx, cy, cz)
    steps_phi, steps_theta = steps if steps is not None else lobe_steps(radius)
    mesh = unreal.GeometryScript_Primitives.append_sphere_lat_long(
        mesh, PRIM, unreal.Transform(location=centre),
        radius=radius, steps_phi=steps_phi, steps_theta=steps_theta,
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
                  (9.5, -8.0, -4.0, 12.0, 0.95)],
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
        # Quatre lobes, pas six : a 1,4 m de haut, les deux plus petits ne
        # changeaient rien a la silhouette et coutaient un tiers du mesh.
        "lobes": [(15.5, 0.0, 0.0, -14.0, 0.95),
                  (12.5, 6.0, -5.0, -22.0, 0.95),
                  (11.5, -5.0, 6.0, -6.0, 0.92),
                  (10.0, 3.0, 6.0, 2.0, 0.90)],
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


# ---------------------------------------------------------------------------
# FOREST_TERRAIN_P1 -- LES ESSENCES
#
# La grammaire ci-dessus etait pontique (epicea, sapin, hetre, charme). Le cadre est
# la Grece byzantine apres 1204 : en bas, la flore mediterraneenne ; en haut des
# versants, les deux coniferes grecs indigenes. Trois formes par essence, tirees
# d'une graine par nom : deux arbres de la meme essence n'ont pas la meme silhouette.
#
# REALISME, PAS FACETTE. Les rapports viennent des arbres reels, adultes :
#
#   essence          hauteur   fut nu   largeur/hauteur   lecture
#   pin d'Alep       11-18 m   ~50 %    0.5-0.65          tronc penche, couronne claire, irreguliere
#   cypres           12-20 m   < 8 %    0.15-0.18         colonne en flamme
#   chene vert        8-14 m   ~25 %    0.8-0.9           dome dense et sombre, revers argente
#   olivier         4.5-8 m    ~35 %    1.0-1.1           tronc tors, couronne argentee ouverte
#   platane         17-24 m    ~35 %    0.7-0.8           fut clair marbre, grande couronne
#   pin noir        15-23 m    ~52 %    0.4-0.5           fut droit, couronne sombre, tete plate a l'age
#   sapin de Cephalonie 14-22 m ~18 %   0.35-0.4          cone d'etages, seul cone legitime : c'est le sapin
#
# Les troncs sont dessines a leur rayon reel (radius_scale 1.0), plus le facteur 0.55
# de la grammaire pontique : un fut trop fin sous une couronne trop petite etait
# exactement le defaut releve. Hauteurs reelles : registre (HeightRangeM).
# ---------------------------------------------------------------------------
PINE_NEEDLE = unreal.LinearColor(0.085, 0.125, 0.050, 1.0)
PINE_BARK = unreal.LinearColor(0.105, 0.075, 0.055, 0.0)
CYPRESS_FOLIAGE = unreal.LinearColor(0.030, 0.060, 0.032, 1.0)
CYPRESS_BARK = unreal.LinearColor(0.085, 0.070, 0.060, 0.0)
OAK_LEAF = unreal.LinearColor(0.040, 0.062, 0.034, 1.0)
OAK_LEAF_UNDER = unreal.LinearColor(0.085, 0.095, 0.068, 1.0)
OAK_BARK = unreal.LinearColor(0.065, 0.060, 0.055, 0.0)
OLIVE_LEAF = unreal.LinearColor(0.105, 0.120, 0.080, 1.0)
OLIVE_LEAF_UNDER = unreal.LinearColor(0.155, 0.165, 0.130, 1.0)
OLIVE_BARK = unreal.LinearColor(0.115, 0.105, 0.092, 0.0)
OLIVE_BARK_DARK = unreal.LinearColor(0.075, 0.068, 0.060, 0.0)
PLANE_LEAF = unreal.LinearColor(0.085, 0.150, 0.050, 1.0)
PLANE_BARK = unreal.LinearColor(0.200, 0.190, 0.150, 0.0)
PLANE_BARK_PATCH = unreal.LinearColor(0.130, 0.130, 0.100, 0.0)
BLACK_PINE_NEEDLE = unreal.LinearColor(0.035, 0.068, 0.040, 1.0)
BLACK_PINE_BARK = unreal.LinearColor(0.060, 0.055, 0.050, 0.0)
FIR_NEEDLE = unreal.LinearColor(0.032, 0.066, 0.050, 1.0)
FIR_BARK = unreal.LinearColor(0.085, 0.080, 0.075, 0.0)

SHAPES_PER_SPECIES = 3


def curved_stem(rng, base, top_z, lean_deg, azimuth, wobble, r0, r1, segments, sweep=0.0):
    """Un fut en plusieurs troncons, pas un cylindre : penche, et legerement sinueux.

    sweep > 0 redresse le haut (le pin d'Alep penche a la base et remonte vers la
    lumiere). Le pied reste EXACTEMENT a base : c'est le pivot de l'arbre."""
    pts = [tuple(base)]
    lean = math.tan(math.radians(lean_deg))
    for i in range(1, segments + 1):
        t = i / segments
        z = base[2] + (top_z - base[2]) * t
        reach = lean * (z - base[2]) * (1.0 - sweep * t * t)
        jitter = wobble * math.sin(t * math.pi)
        pts.append((base[0] + reach * math.cos(azimuth) + rng.uniform(-jitter, jitter),
                    base[1] + reach * math.sin(azimuth) + rng.uniform(-jitter, jitter), z))
    stems = []
    for i in range(segments):
        a, b = pts[i], pts[i + 1]
        # Prolonge chaque troncon de 4 % : pas de fente au coude.
        b2 = tuple(a[k] + (b[k] - a[k]) * 1.04 for k in range(3)) if i + 1 < segments else b
        stems.append((a, b2, r0 + (r1 - r0) * i / segments, r0 + (r1 - r0) * (i + 1) / segments))
    return stems, pts


def scatter_lobes(rng, centre, rx, rz, count, r_lo, r_hi, squash, shell=(.35, .95), top_bias=0.25):
    """Masses de feuillage dans une enveloppe ellipsoidale : la plus grosse au coeur, les
    autres sur une coquille, decroissantes. Le bord reste irregulier, et c'est voulu."""
    lobes = []
    for k in range(count):
        radius = r_hi - (r_hi - r_lo) * k / max(count - 1, 1) * rng.uniform(.8, 1.0)
        if k == 0:
            lobes.append((radius, centre[0], centre[1], centre[2], squash))
            continue
        theta = k * 2.399963 + rng.uniform(-.5, .5)
        u = (k + .5) / count
        zz = max(-1.0, min(1.0, 1.0 - 2.0 * u + top_bias))
        ring = math.sqrt(max(0.0, 1.0 - zz * zz))
        d = rng.uniform(*shell)
        ex, ez = max(rx - radius, 1.0), max(rz - radius * squash, 1.0)
        lobes.append((radius, centre[0] + ex * d * ring * math.cos(theta),
                      centre[1] + ex * d * ring * math.sin(theta), centre[2] + ez * d * zz, squash))
    return lobes


def species_spec(name, note, bark, foliage, **extra):
    spec = {"name": name, "note": note, "trunk": [], "limbs": [], "tiers": [], "lobes": [],
            "stems": [], "axis": None, "bark": bark, "foliage": foliage, "radius_scale": 1.0}
    spec.update(extra)
    return spec


def aleppo_pine(k, rng):
    lean = (6.0, 11.0, 3.0)[k] + rng.uniform(-2, 2)
    az = rng.uniform(0, math.tau)
    stems, axis = curved_stem(rng, (0, 0, -50), 28.0, lean, az, 1.2, 2.1, 1.0, 5, sweep=.45)
    centre = axis[-2] if k < 2 else axis[-1]
    umbrella = k == 2
    rx, rz = (27.0 + rng.uniform(-3, 3), 30.0) if not umbrella else (33.0, 22.0)
    cz = (4.0, 6.0, 16.0)[k]
    lobes = scatter_lobes(rng, (centre[0], centre[1], cz), rx, rz, 7 if not umbrella else 8,
                          10.0, 15.0, .55 if umbrella else .7, shell=(.45, 1.0), top_bias=.15 if umbrella else 0.0)
    return species_spec("SM_Tree_AleppoPine_%02d" % (k + 1),
                        "pin d'Alep -- fut penche de %.0f deg, couronne claire et irreguliere" % lean,
                        PINE_BARK, PINE_NEEDLE, stems=stems, axis=axis, lobes=lobes, twig_radius=1.0,
                        blade_width=.30, blade_size=(.12, .17), blades_per_lobe=150, inner_groups=5)


def cypress(k, rng):
    stems, axis = curved_stem(rng, (0, 0, -50), 44.0, rng.uniform(0, 2), rng.uniform(0, math.tau), .3, 1.6, .5, 3)
    rmax = (7.0, 8.8, 8.0)[k]
    bend = 3.0 if k == 2 else 0.0
    lobes = []
    count = 12
    for i in range(count):
        t = (i + .5) / count
        z = -44.0 + 90.0 * t
        r = max(2.4, rmax * math.sin(math.pi * min(.999, t ** .75)) ** .8)
        lobes.append((r, rng.uniform(-1, 1) + bend * t * t, rng.uniform(-1, 1), z, 1.9))
    lobes.append((2.4, bend, 0.0, 47.0, 2.2))
    return species_spec("SM_Tree_Cypress_%02d" % (k + 1), "cypres -- colonne en flamme, w/h ~%.2f" % (2 * rmax / 100),
                        CYPRESS_BARK, CYPRESS_FOLIAGE, stems=stems, axis=axis, lobes=lobes, twig_radius=.4,
                        blade_width=.45, blade_size=(.18, .26), blades_per_lobe=90, inner_groups=4)


def limbs_from(rng, origin, count, reach, z_lo, z_hi, r0, r1):
    out = []
    for i in range(count):
        a = i * math.tau / count + rng.uniform(-.4, .4)
        d = rng.uniform(*reach)
        out.append((tuple(origin), (origin[0] + d * math.cos(a), origin[1] + d * math.sin(a), rng.uniform(z_lo, z_hi)), r0, r1))
    return out


def holm_oak(k, rng):
    stems, axis = curved_stem(rng, (0, 0, -50), -26.0, rng.uniform(2, 4), rng.uniform(0, math.tau), .6, 3.2, 2.4, 2)
    stems += limbs_from(rng, axis[-1], 3 + (k % 2), (13, 18), -6, 6, 2.0, 1.0)
    rx = (45.0, 48.0, 43.0)[k]
    lobes = scatter_lobes(rng, (axis[-1][0], axis[-1][1], 2.0), rx, 46.0, 10, 14.0, 19.0, .85, top_bias=.1)
    return species_spec("SM_Tree_HolmOak_%02d" % (k + 1), "chene vert -- fut court, dome dense et sombre",
                        OAK_BARK, OAK_LEAF, stems=stems, axis=axis, lobes=lobes, foliage_alt=OAK_LEAF_UNDER,
                        foliage_alt_share=.3, blade_width=.5, blade_size=(.10, .14), blades_per_lobe=130, inner_groups=5)


def olive(k, rng):
    stems, axis = curved_stem(rng, (0, 0, -50), -12.0, rng.uniform(3, 9), rng.uniform(0, math.tau), 3.0, 6.5, 3.6, 3)
    top = axis[-1]
    leaders = []
    for i in range(2 + (k % 2)):
        a = i * math.tau / (2 + (k % 2)) + rng.uniform(-.5, .5)
        d = rng.uniform(10, 16)
        mid = (top[0] + .5 * d * math.cos(a) + rng.uniform(-2, 2), top[1] + .5 * d * math.sin(a) + rng.uniform(-2, 2), top[2] + 8)
        end = (top[0] + d * math.cos(a), top[1] + d * math.sin(a), top[2] + rng.uniform(14, 20))
        leaders += [(top, mid, 3.2, 2.2), (mid, end, 2.2, 1.3)]
    rx = (60.0, 64.0, 56.0)[k]
    lobes = scatter_lobes(rng, (top[0], top[1], 6.0), rx, 34.0, 9, 12.0, 17.0, .7, shell=(.45, 1.0), top_bias=.1)
    return species_spec("SM_Tree_Olive_%02d" % (k + 1), "olivier -- tronc tors, couronne argentee ouverte",
                        OLIVE_BARK, OLIVE_LEAF, stems=stems + leaders, axis=axis, lobes=lobes, bark_alt=OLIVE_BARK_DARK,
                        foliage_alt=OLIVE_LEAF_UNDER, foliage_alt_share=.45, blade_width=.32, blade_size=(.09, .13),
                        blades_per_lobe=150, inner_groups=4)


def plane_tree(k, rng):
    stems, axis = curved_stem(rng, (0, 0, -50), -14.0, rng.uniform(0, 3), rng.uniform(0, math.tau), .5, 2.8, 2.2, 3)
    stems += limbs_from(rng, axis[-1], 4, (12, 18), 4, 15, 1.9, 1.0)
    rx = (42.0, 46.0, 40.0)[k]
    lobes = scatter_lobes(rng, (axis[-1][0], axis[-1][1], 10.0), rx, 40.0, 10, 14.0, 20.0, .9, top_bias=.1)
    return species_spec("SM_Tree_PlaneTree_%02d" % (k + 1), "platane d'Orient -- fut clair marbre, grande couronne",
                        PLANE_BARK, PLANE_LEAF, stems=stems, axis=axis, lobes=lobes, bark_alt=PLANE_BARK_PATCH,
                        blade_width=.62, blade_size=(.12, .17), blades_per_lobe=140, inner_groups=5)


def black_pine(k, rng):
    stems, axis = curved_stem(rng, (0, 0, -50), 36.0, rng.uniform(0, 2), rng.uniform(0, math.tau), .5, 2.0, .9, 4)
    lobes = []
    if k < 2:
        for layer, (z, reach) in enumerate(((8.0, 18.0), (22.0, 16.0), (36.0, 11.0))):
            for i in range(3):
                a = i * math.tau / 3 + layer * .9 + rng.uniform(-.3, .3)
                p = axis_point({"axis": axis}, z)
                lobes.append((rng.uniform(8, 12) * (1.0 if layer < 2 else .8), p[0] + reach * .55 * math.cos(a),
                              p[1] + reach * .55 * math.sin(a), z + rng.uniform(-2, 2), .62))
        lobes.append((7.0, axis[-1][0], axis[-1][1], 44.0, .7))
    else:
        # Vieux pin noir : la tete s'aplatit et s'elargit.
        lobes = scatter_lobes(rng, (axis[-1][0], axis[-1][1], 22.0), 27.0, 21.0, 9, 9.0, 13.0, .55, shell=(.4, 1.0), top_bias=.05)
    return species_spec("SM_Tree_BlackPine_%02d" % (k + 1), "pin noir -- fut droit, couronne sombre%s" % (", tete plate" if k == 2 else ""),
                        BLACK_PINE_BARK, BLACK_PINE_NEEDLE, stems=stems, axis=axis, lobes=lobes, twig_radius=.8,
                        blade_width=.26, blade_size=(.13, .19), blades_per_lobe=140, inner_groups=5)


def greek_fir(k, rng):
    tiers = []
    n = 6
    for i in range(n):
        base = 19.0 * (1.0 - i / 6.5) + rng.uniform(-1.5, 1.5)
        z0 = -34.0 + i * 13.0 - 2.0
        z1 = 50.0 if i == n - 1 else z0 + 17.0
        tiers.append((base, .4 if i == n - 1 else base * .45, z0, z1))
    return species_spec("SM_Tree_GreekFir_%02d" % (k + 1), "sapin de Cephalonie -- cone d'etages, fut lisible a la base",
                        FIR_BARK, FIR_NEEDLE, trunk=[(1.9, 1.4, -50.0, -30.0)], tiers=tiers, spray_blades=18)


SPECIES_RECIPES = (aleppo_pine, cypress, holm_oak, olive, plane_tree, black_pine, greek_fir)


# ---------------------------------------------------------------------------
# FOREST_TERRAIN_P3 -- LA STRATE ARBUSTIVE (maquis) ET LES RONCES
#
# Meme grammaire que les arbres (normalisation Z = [-50,+50], deux slots, LOD), sans
# empattement de racines : un arbuste part du sol en plusieurs tiges. La hauteur reelle
# est posee par AnastasisUnderstory (lentisque 1-2.5 m, kermes 0.6-1.8 m, genet 1.2-2.8 m,
# ronce 0.6-1.4 m) ; ici seules les proportions comptent.
#
#   lentisque    dome bas et dense, vert sombre lustre, pousses rougeatres     w/h ~1.6
#   chene kermes coussin serre et pique, vert gris                             w/h ~1.9
#   genet        touffe de tiges vertes dressees, fleurs jaunes au sommet      w/h ~0.8
#   ronce        monticule de cannes arquees qui retombent au sol              w/h ~2.0
# ---------------------------------------------------------------------------
LENTISK_LEAF = unreal.LinearColor(0.035, 0.060, 0.030, 1.0)
LENTISK_SHOOT = unreal.LinearColor(0.075, 0.050, 0.035, 1.0)
KERMES_LEAF = unreal.LinearColor(0.060, 0.075, 0.045, 1.0)
KERMES_LEAF_PALE = unreal.LinearColor(0.090, 0.095, 0.070, 1.0)
BROOM_STEM = unreal.LinearColor(0.070, 0.100, 0.035, 0.0)
BROOM_GREEN = unreal.LinearColor(0.070, 0.100, 0.035, 1.0)
BROOM_FLOWER = unreal.LinearColor(0.520, 0.400, 0.040, 1.0)
BRAMBLE_LEAF = unreal.LinearColor(0.045, 0.070, 0.035, 1.0)
BRAMBLE_CANE = unreal.LinearColor(0.090, 0.045, 0.045, 0.0)
SHRUB_WOOD = unreal.LinearColor(0.080, 0.065, 0.050, 0.0)


def shrub_stems(rng, count, spread, height, r0, r1):
    """Tiges qui partent du pied en eventail : un arbuste n'a pas de fut."""
    out = []
    for i in range(count):
        a = i * math.tau / count + rng.uniform(-.4, .4)
        d = spread * rng.uniform(.5, 1.0)
        out.append(((0.0, 0.0, -50.0), (d * math.cos(a), d * math.sin(a), -50.0 + height * rng.uniform(.7, 1.0)), r0, r1))
    return out


def lentisk(k, rng):
    stems = shrub_stems(rng, 5, 18.0, 40.0, 1.6, .6)
    lobes = scatter_lobes(rng, (0.0, 0.0, -16.0), (54.0, 60.0, 50.0)[k], 36.0, 8, 17.0, 24.0, .72, shell=(.4, 1.0), top_bias=.15)
    return species_spec("SM_Shrub_Lentisk_%02d" % (k + 1), "lentisque -- dome bas et dense, pousses rougeatres",
                        SHRUB_WOOD, LENTISK_LEAF, stems=stems, axis=None, lobes=lobes, roots=False, twig_radius=.6,
                        foliage_alt=LENTISK_SHOOT, foliage_alt_share=.15, blade_width=.45, blade_size=(.08, .11),
                        blades_per_lobe=75, inner_groups=3, distant_stems=1)


def kermes_oak(k, rng):
    stems = shrub_stems(rng, 6, 22.0, 30.0, 1.3, .5)
    lobes = scatter_lobes(rng, (0.0, 0.0, -22.0), (58.0, 64.0, 54.0)[k], 30.0, 9, 15.0, 21.0, .6, shell=(.4, 1.0), top_bias=.1)
    return species_spec("SM_Shrub_KermesOak_%02d" % (k + 1), "chene kermes -- coussin serre, vert gris",
                        SHRUB_WOOD, KERMES_LEAF, stems=stems, axis=None, lobes=lobes, roots=False, twig_radius=.5,
                        foliage_alt=KERMES_LEAF_PALE, foliage_alt_share=.3, blade_width=.30, blade_size=(.07, .10),
                        blades_per_lobe=75, inner_groups=3, distant_stems=1)


def broom(k, rng):
    stems = []
    lobes = []
    count = (14, 18, 12)[k]
    for i in range(count):
        a = rng.uniform(0, math.tau)
        lean = rng.uniform(.15, .45)
        top = (math.cos(a) * 100.0 * lean * rng.uniform(.6, 1.0), math.sin(a) * 100.0 * lean * rng.uniform(.6, 1.0),
               -50.0 + 100.0 * rng.uniform(.75, 1.0))
        stems.append(((0.0, 0.0, -50.0), top, 1.0, .35))
        lobes.append((rng.uniform(4.0, 6.5), top[0], top[1], top[2] - 4.0, 1.4))
    return species_spec("SM_Shrub_Broom_%02d" % (k + 1), "genet d'Espagne -- tiges vertes dressees, fleurs jaunes",
                        BROOM_STEM, BROOM_GREEN, stems=stems, axis=None, lobes=lobes, roots=False, twig_radius=.3,
                        foliage_alt=BROOM_FLOWER, foliage_alt_share=.5, blade_width=.35, blade_size=(.30, .45),
                        blades_per_lobe=40, inner_groups=2, distant_stems=4)


def bramble(k, rng):
    stems = []
    lobes = []
    for i in range((7, 8, 6)[k]):
        a = rng.uniform(0, math.tau)
        reach = rng.uniform(60.0, 100.0)
        peak = rng.uniform(55.0, 95.0)
        prev = (0.0, 0.0, -50.0)
        for t in (.3, .6, .85, 1.0):
            z = -50.0 + peak * math.sin(t * math.pi * .95)
            pt = (math.cos(a) * reach * t, math.sin(a) * reach * t, z)
            stems.append((prev, pt, .9 * (1.1 - t), .9 * (1.0 - t) + .2))
            prev = pt
            if t in (.6, .85):
                lobes.append((rng.uniform(12.0, 17.0), pt[0], pt[1], pt[2], .7))
    lobes.append((22.0, 0.0, 0.0, -30.0, .8))
    return species_spec("SM_Shrub_Bramble_%02d" % (k + 1), "ronce -- cannes arquees, monticule",
                        BRAMBLE_CANE, BRAMBLE_LEAF, stems=stems, axis=None, lobes=lobes, roots=False, twig_radius=.4,
                        blade_width=.55, blade_size=(.10, .14), blades_per_lobe=45, inner_groups=2, distant_stems=2)


SHRUB_RECIPES = (lentisk, kermes_oak, broom, bramble)


def shrub_specs():
    out = []
    for recipe in SHRUB_RECIPES:
        for k in range(SHAPES_PER_SPECIES):
            rng = random.Random("%s:%d:forest-terrain-p3" % (recipe.__name__, k))
            out.append(recipe(k, rng))
    return out


def species_specs():
    out = []
    for recipe in SPECIES_RECIPES:
        for k in range(SHAPES_PER_SPECIES):
            rng = random.Random("%s:%d:forest-terrain-p1" % (recipe.__name__, k))
            out.append(recipe(k, rng))
    return out


def axis_point(spec, z):
    """Point de l'axe du tronc a la hauteur z : c'est la que naissent les branches."""
    axis = spec.get('axis')
    if not axis:
        return (0.0, 0.0, -18.0)
    if z <= axis[0][2]:
        return tuple(axis[0])
    for a, b in zip(axis, axis[1:]):
        if z <= b[2]:
            t = (z - a[2]) / max(b[2] - a[2], 1e-6)
            return tuple(a[i] + (b[i] - a[i]) * t for i in range(3))
    return tuple(axis[-1])


def build_stems(spec, limit=None):
    """Futs et branches maitresses dessines comme une chaine de troncons orientes."""
    stems = spec.get('stems') or []
    if limit is not None:
        stems = stems[:limit]
    if not stems:
        return None
    out = unreal.DynamicMesh()
    alt = spec.get('bark_alt')
    for i, (start, end, r0, r1) in enumerate(stems):
        part = branch_between(unreal.DynamicMesh(), start, end, r0, r1, steps=11)
        out = merge(out, coloured(part, alt if (alt is not None and i % 2) else spec['bark']))
    return out


def branch_between(mesh, start, end, radius, tip, steps=8):
    delta = [end[i] - start[i] for i in range(3)]
    length = math.sqrt(sum(v * v for v in delta))
    return taper(mesh, radius, tip, 0, length, steps=steps,
                 location=unreal.Vector(*start),
                 rotator=unreal.Rotator(pitch=-math.degrees(math.acos(delta[2] / length)),
                                       yaw=math.degrees(math.atan2(delta[1], delta[0]))), prim=PRIM_WOOD)


def leaf_blades(centres, color, rng, width=.58, alt=None, alt_share=0.0):
    """Opaque folded blades: real silhouette gaps, no alpha overdraw or external texture.

    Positions and palette are recipe-seeded. Eight triangles per blade, with a raised
    central vein; broad crown volumes become small readable groups at human height.
    """
    vertices, triangles, colors, uv = [], [], [], []
    for cx, cy, cz, size in centres:
        angle = rng.random() * math.tau
        tilt = rng.uniform(-0.65, 0.65)
        axis = (math.cos(angle), math.sin(angle), tilt)
        side = (-math.sin(angle), math.cos(angle), rng.uniform(-0.25, 0.25))
        base = len(vertices)
        # Narrow petiole, asymmetric shoulders and a pointed tip; folded midrib.
        # Eight small triangles replace the large four-triangle diamond.
        outline = ((-1.,0.),(-.52,.65),(.02,1.),(.56,.57),
                   (1.,0.),(.46,-.65),(-.12,-.88),(-.65,-.43))
        for along, across in outline + ((0.,0.),):
            lift = .075 if (along,across)==(0.,0.) else .025*along
            vertices.append(unreal.Vector(cx + size*(along*axis[0]+across*width*side[0]),
                                          cy + size*(along*axis[1]+across*width*side[1]),
                                          cz + size*(along*axis[2]+across*width*side[2]+lift)))
            uv.append(unreal.Vector2D(.5+across*.5,.5+along*.5))
        tint = rng.uniform(.80, 1.18)
        # Deux tons : la face inferieure argentee de l'olivier et du chene vert.
        tone = alt if (alt is not None and rng.random() < alt_share) else color
        colors.extend([unreal.LinearColor(tone.r*tint, tone.g*tint, tone.b*tint, 1)]*9)
        for i in range(8): triangles.append(unreal.IntVector(base+8,base+(i+1)%8,base+i))
    buffers = unreal.GeometryScriptSimpleMeshBuffers(vertices=vertices,triangles=triangles,
                                                     vertex_colors=colors,uv0=uv)
    mesh, unused = unreal.GeometryScript_MeshEdits.append_buffers_to_mesh(
        unreal.DynamicMesh(), buffers, material_id=SLOT_FOLIAGE)
    return mesh


def living_crown(spec, rng):
    """Keep the authored crown envelope, articulate it into boughs and leaf sprays."""
    foliage = unreal.DynamicMesh()
    wood = unreal.DynamicMesh()
    blades = []
    color = spec['foliage']
    groups = spec.get('inner_groups', 7)
    per_lobe = spec.get('blades_per_lobe', 220)
    size_lo, size_hi = spec.get('blade_size', (.105, .155))
    if spec['lobes']:
        for radius,cx,cy,cz,squash in spec['lobes']:
            origin = axis_point(spec, cz - radius*.6)
            if math.dist(origin, (cx, cy, cz)) > 1.0:
                wood = branch_between(wood,origin,(cx,cy,cz),spec.get('twig_radius',.72),.22)
            # VISUAL_CRUSADE_001: branchlets carry clustered leaves instead of
            # closed spheres. Same crown envelope, explicit gaps and tapered tips.
            for k in range(groups):
                theta = k * 2.399963 + rng.uniform(-.30,.30)
                z = 1 - 2*(k+.5)/groups
                ring = math.sqrt(max(0,1-z*z))
                endpoint = (cx+radius*.86*ring*math.cos(theta),
                            cy+radius*.86*ring*math.sin(theta), cz+radius*.86*z*squash)
                joint = tuple(origin[j]*.25 + (cx,cy,cz)[j]*.75 for j in range(3))
                wood = branch_between(wood,joint,endpoint,.16,.035,steps=6)
                # Small porous tufts replace each former solid internal lobe.
                for leaf in range(12):
                    spread=radius*.20
                    point=[endpoint[j]+rng.uniform(-spread,spread) for j in range(3)]
                    blades.append((*point,radius*rng.uniform(size_lo,size_hi)*.85))
            # Keep the authored outer crown coverage. The first branch-only pass
            # looked defoliated; removing opaque cores must not remove the canopy.
            for k in range(per_lobe):
                theta=k*2.399963
                z=1-2*(k+.5)/per_lobe
                ring=math.sqrt(max(0,1-z*z))
                shell=rng.uniform(.65,1.02)
                blades.append((cx+radius*shell*ring*math.cos(theta),
                               cy+radius*shell*ring*math.sin(theta),
                               cz+radius*shell*z*squash,radius*rng.uniform(size_lo,size_hi)))
    else:
        # Thin sprays transmit light. Closed ellipsoids here create black stacked
        # plates from below, even with a two-sided foliage material.
        for tier,(radius,top,z0,z1) in enumerate(spec['tiers']):
            for layer in range(3):
                fraction=(layer+rng.uniform(.1,.5))/3
                reach=radius*(1-fraction)+top*fraction
                z=z0+(z1-z0)*fraction
                for b in range(7):
                    angle=b*math.tau/7+tier*.83+layer*.61+rng.uniform(-.2,.2)
                    length=reach*rng.uniform(.8,1.1)
                    ex,ey=length*math.cos(angle),length*math.sin(angle)
                    bz=z+rng.uniform(-2,2)
                    wood=branch_between(wood,(0,0,bz+2),(ex,ey,bz-1.5),.30,.08)
                    for k in range(spec.get('spray_blades', 24)):
                        t=rng.uniform(.22,1.05)
                        spread=rng.uniform(-1,1)*(length*.23*math.sin(min(t,1)*math.pi)+.4)
                        px=ex*t-math.sin(angle)*spread
                        py=ey*t+math.cos(angle)*spread
                        pz=bz+1.5*math.sin(t*math.pi)-1.8*t+rng.uniform(-1.2,1.2)
                        blades.append((px,py,pz,max(.8,length*rng.uniform(.12,.18))))
    foliage=merge(foliage,leaf_blades(blades,color,rng,spec.get('blade_width',.58),
                                      spec.get('foliage_alt'),spec.get('foliage_alt_share',0.0)))
    return foliage,coloured(wood,spec['bark'])


def build_family(spec):
    parts = []
    rng = random.Random(spec['name'] + ':forest-walk-001')
    rs = spec.get('radius_scale', .55)
    slender = [(s[0]*rs,s[1]*rs,*s[2:]) for s in spec['trunk']]
    trunk = build_trunk(slender, spec["bark"])
    if trunk is not None:
        parts.append(trunk)
    limbs = build_limbs([(s[0]*rs,s[1]*rs,*s[2:]) for s in spec['limbs']], spec["bark"])
    if limbs is not None:
        parts.append(limbs)
    stems = build_stems(spec)
    if stems is not None:
        parts.append(stems)
    foliage, twigs = living_crown(spec,rng)
    parts.extend((foliage,twigs))
    if spec.get('roots', True) and (slender or spec.get('stems')):
        roots=unreal.DynamicMesh()
        radius=slender[0][0] if slender else spec['stems'][0][2]
        for k in range(6):
            angle=k*math.tau/6+.25+rng.uniform(-.18,.18)
            reach=radius*rng.uniform(1.6,2.2)
            joint=(reach*.52*math.cos(angle+.10),reach*.52*math.sin(angle+.10),-49.0)
            end=(reach*math.cos(angle),reach*math.sin(angle),-49.8)
            roots=branch_between(roots,(0,0,-47.4),joint,radius*.40,radius*.16,steps=10)
            roots=branch_between(roots,joint,end,radius*.16,.07,steps=8)
        if spec['tiers']:
            roots=taper(roots,slender[-1][1],.15,slender[-1][3],49,steps=9,prim=PRIM_WOOD)
        parts.append(coloured(roots,spec['bark']))

    mesh = unreal.DynamicMesh()
    for part in parts:
        mesh = merge(mesh, part)
    # Fractionner APRES la fusion : les aretes qui comptent le plus sont celles
    # entre deux sous-parties (bois contre feuillage), et elles n'existent pas
    # tant que les morceaux sont separes.
    return shade(normalise(mesh, spec["name"]))


def distant_crown(spec):
    """Closed crown envelopes below 5.5% screen height preserve forest coverage.

    Decimating disconnected leaf sprays deletes coverage rather than detail. Use
    the existing authored tier/lobe envelopes at this distance, with cheap offset
    groups; this mesh is never the close view.
    """
    mesh = unreal.DynamicMesh()
    rs = spec.get('radius_scale', .55)
    trunk = build_trunk([(s[0]*rs,s[1]*rs,*s[2:]) for s in spec['trunk']], spec['bark'])
    stems = build_stems(spec, limit=spec.get('distant_stems', 4)) if spec.get('distant_stems', 4) > 0 else None
    if trunk is not None:
        mesh = merge(mesh, trunk)
    elif stems is not None:
        mesh = merge(mesh, stems)
    elif spec['limbs']:
        mesh = merge(mesh, build_limbs(spec['limbs'], spec['bark']))
    crown = unreal.DynamicMesh()
    for tier, (radius, top, z0, z1) in enumerate(spec['tiers']):
        for k in range(3):
            angle = k*math.tau/3 + tier*.8
            part = taper(unreal.DynamicMesh(), radius*.65, top*.40, z0, z1, steps=7,
                         location=unreal.Vector(radius*.35*math.cos(angle),
                                                radius*.35*math.sin(angle), z0))
            crown = merge(crown, part)
    # VISUAL_CRUSADE_001: retain distant coverage with irregular subcrowns.
    # A single low-resolution ellipsoid per lobe became a bright polygonal block
    # at medium distance. Three overlapping off-axis masses keep a broken contour.
    rng = random.Random(spec['name'] + ':distant-subcrowns')
    for radius, cx, cy, cz, squash in spec['lobes']:
        for k in range(3):
            angle = k*2.399963 + rng.uniform(-.35,.35)
            r = radius*rng.uniform(.53,.70)
            offset = radius*.43
            part = lobe(unreal.DynamicMesh(), r,
                        cx+offset*math.cos(angle), cy+offset*math.sin(angle),
                        cz+radius*rng.uniform(-.32,.32)*squash,
                        squash*rng.uniform(.80,1.20), (5,9))
            tone = rng.uniform(.66,.86)
            c = spec['foliage']
            mesh = merge(mesh,coloured(part,unreal.LinearColor(c.r*tone,c.g*tone,c.b*tone,1)))
    # Tier conifers keep their closed distant profile; broadleaf subcrowns above
    # already carry their own colors.
    mesh = merge(mesh, coloured(crown, spec['foliage']))
    return shade(normalise(mesh, spec['name'] + ' distant'))


def save_static_mesh(mesh, asset_path, far_mesh):
    existing = unreal.EditorAssetLibrary.load_asset(asset_path) if unreal.EditorAssetLibrary.does_asset_exist(asset_path) else None
    options = unreal.GeometryScriptCreateNewStaticMeshAssetOptions()
    # FAUX AMI : a True, le build recalcule et JETTE les normales fractionnees
    # authorees par shade(). Le mesh repartirait tout lisse et .5b n'aurait
    # servi a rien -- sans la moindre erreur pour le signaler.
    options.set_editor_property("enable_recompute_normals", False)
    options.set_editor_property("enable_recompute_tangents", True)
    # Nanite reste OFF : le pipeline est HISM + LOD, et la direction artistique
    # refuse une feature qui n'a pas gagne son existence.
    options.set_editor_property("enable_nanite", False)

    if existing is not None:
        asset = existing
        copy_options = unreal.GeometryScriptCopyMeshToAssetOptions(enable_recompute_tangents=True)
        unused, outcome = unreal.GeometryScript_AssetUtils.copy_mesh_to_static_mesh(
            mesh, asset, copy_options, unreal.GeometryScriptMeshWriteLOD(lod_index=0))
        if outcome != unreal.GeometryScriptOutcomePins.SUCCESS:
            raise RuntimeError('Render mesh update failed: ' + asset_path)
    else:
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
        if existing is None:
            unreal.EditorStaticMeshLibrary.add_simple_collisions(
                asset, unreal.ScriptingCollisionShapeType.NDOP10_X)
    except Exception as exc:  # noqa: BLE001 -- best effort, comme la version precedente
        log("WARN collision simple refusee sur %s: %s" % (asset_path, exc))

    # Detail belongs near the camera. Explicit screen thresholds and measured output;
    # HISM chooses the LOD, no per-tree actor or distance polling is introduced.
    reductions = unreal.StaticMeshReductionOptions()
    reductions.auto_compute_lod_screen_size = False
    reductions.reduction_settings = [unreal.StaticMeshReductionSettings(percent_triangles=p,screen_size=s)
                                    for p,s in ((1.0,1.0),(.50,.22),(.07,.055))]
    subsystem=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    if subsystem is None:
        editors=unreal.get_editor_subsystem(unreal.AssetEditorSubsystem)
        if editors is None: raise RuntimeError('Tree LOD generation requires a live editor, not a Python commandlet')
        editors.open_editor_for_assets([asset])
        subsystem=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
        editors.close_all_editors_for_asset(asset)
    if subsystem is None: raise RuntimeError('StaticMeshEditor module did not provide its LOD subsystem')
    lod_count=subsystem.set_lods(asset,reductions)
    if lod_count != 3: raise RuntimeError('%s: expected 3 LODs, got %s'%(asset_path,lod_count))
    copy_options = unreal.GeometryScriptCopyMeshToAssetOptions(enable_recompute_tangents=True)
    unused, outcome = unreal.GeometryScript_AssetUtils.copy_mesh_to_static_mesh(
        far_mesh, asset, copy_options, unreal.GeometryScriptMeshWriteLOD(lod_index=2))
    if outcome != unreal.GeometryScriptOutcomePins.SUCCESS:
        raise RuntimeError('%s: distant LOD copy failed: %s' % (asset_path, outcome))
    far_settings = subsystem.get_lod_reduction_settings(asset, 2)
    far_settings.percent_triangles = 1.0
    far_settings.percent_vertices = 1.0
    far_settings.base_lod_model = 2
    subsystem.set_lod_reduction_settings(asset, 2, far_settings)
    log('LODS %s triangles=%s'%(asset_path,[asset.get_num_triangles(i) for i in range(3)]))

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

# Balancement du feuillage, calcule dans l'espace LOCAL du mesh (Z=[-50,+50]) puis
# transforme en monde : la transformation porte l'echelle de l'instance, donc un
# platane de 22 m balance proportionnellement plus qu'un olivier de 6 m.
#
# CORRECTION (FOREST_TERRAIN_P1). La version precedente ecrivait l'offset directement
# dans le World Position Offset en croyant qu'il etait applique avant l'echelle par
# instance. Il ne l'est pas : le WPO est un decalage MONDE, et chaque arbre, du jeune
# chene au sapin emergent, balancait des memes 4 uu.
#
# PAS DE WindDirectionalSource : un sinus du temps ne depend d'aucune signature externe.
# Dephase par PerInstanceRandom pour que la foret ne batte pas a l'unisson.
WIND_SWAY_STRENGTH = 4.0          # repli monde, si la transformation locale manque
WIND_SWAY_LOCAL = 0.6             # unites locales (mesh de 100) au sommet de la couronne
WIND_SWAY_SPEED = 0.6

# Teinte par arbre (FOREST_TERRAIN_P1). Deux flottants par instance, poses par
# AAnastasisWorldEmbodiment sur les HISM d'arbres :
#   PerInstanceCustomData[0]  secheresse du site [0,1] -> couronne vers l'olive paille
#   PerInstanceCustomData[1]  ecart individuel [-1,1]  -> valeur +-12 %
# Tous deux valent 0 hors de ce chemin (acteurs poses a la main, lieux composes) : le
# materiau y rend exactement la couleur de sommet, comme avant.
DRY_TINT = unreal.LinearColor(1.18, 1.06, 0.72, 1.0)
DRY_STRENGTH = 0.35
VALUE_JITTER = 0.12


def enum_member(owner_name, *names):
    owner = getattr(unreal, owner_name, None)
    for name in names:
        value = getattr(owner, name, None) if owner is not None else None
        if value is not None:
            return value
    raise RuntimeError('unreal.%s: aucun de %s' % (owner_name, names))


def open_material(name, path):
    """Reecrit le graphe SANS supprimer l'asset : les meshes gardent leur reference."""
    mel = unreal.MaterialEditingLibrary
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        mat = unreal.EditorAssetLibrary.load_asset(path)
        before = mel.get_num_material_expressions(mat)
        mel.delete_all_material_expressions(mat)
        left = mel.get_num_material_expressions(mat)
        if left:
            for expr in list(mel.get_material_expressions(mat) or []):
                mel.delete_material_expression(mat, expr)
            left = mel.get_num_material_expressions(mat)
        log('%s clear expressions %d -> %d' % (name, before, left))
        if left:
            raise RuntimeError('%s: le graphe n est pas vide (%d)' % (name, left))
        return mat
    return unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        name, MATERIAL_DIR, unreal.Material, unreal.MaterialFactoryNew())


def ensure_material():
    """VertexColor -> BaseColor, et surtout : feuillage deux faces.

    Une masse de feuillage opaque lit comme du plastique, quelle que soit sa
    silhouette. Le modele MSM_TWO_SIDED_FOLIAGE laisse la lumiere TRAVERSER la
    couronne, ce que la planche de reference appelle "lumiere filtree" et
    "volumetrie".

    L'alpha des sommets sert de masque : 0 sur le bois, 1 sur le feuillage. Sans
    ce masque, les troncs deviendraient translucides eux aussi -- et la teinte par
    arbre peindrait aussi l'ecorce.
    """
    if unreal.get_editor_subsystem(unreal.AssetEditorSubsystem) is None:
        raise RuntimeError('Tree generation requires a live editor; no assets were modified')
    mat = open_material(MATERIAL_NAME, MATERIAL_PATH)
    mel = unreal.MaterialEditingLibrary

    def const(value, x, y):
        node = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, x, y)
        node.set_editor_property('r', value)
        return node

    def const3(color, x, y):
        node = mel.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector, x, y)
        node.set_editor_property('constant', color)
        return node

    def op(cls, a, b, x, y, a_out='', b_out=''):
        node = mel.create_material_expression(mat, cls, x, y)
        mel.connect_material_expressions(a, a_out, node, 'A')
        mel.connect_material_expressions(b, b_out, node, 'B')
        return node

    def lerp(a, b, alpha, x, y, alpha_out=''):
        node = mel.create_material_expression(mat, unreal.MaterialExpressionLinearInterpolate, x, y)
        mel.connect_material_expressions(a, '', node, 'A')
        mel.connect_material_expressions(b, '', node, 'B')
        mel.connect_material_expressions(alpha, alpha_out, node, 'Alpha')
        return node

    vc = mel.create_material_expression(mat, unreal.MaterialExpressionVertexColor, -1600, 0)
    # Sortie '' de VertexColor = RGB, comme la version precedente l'a verifie sur ce build.
    base_output = ''

    # Teinte par arbre, sur le feuillage seulement (alpha de sommet).
    tint_state = 'vertex_only'
    base = vc
    try:
        dry = mel.create_material_expression(mat, unreal.MaterialExpressionPerInstanceCustomData, -1600, -300)
        dry.set_editor_property('data_index', 0)
        dry.set_editor_property('const_default_value', 0.0)
        jitter = mel.create_material_expression(mat, unreal.MaterialExpressionPerInstanceCustomData, -1600, -200)
        jitter.set_editor_property('data_index', 1)
        jitter.set_editor_property('const_default_value', 0.0)
        one3 = const3(unreal.LinearColor(1.0, 1.0, 1.0, 1.0), -1400, -420)
        dry_amount = op(unreal.MaterialExpressionMultiply, dry, const(DRY_STRENGTH, -1600, -380), -1400, -320)
        dried = lerp(one3, const3(DRY_TINT, -1400, -500), dry_amount, -1200, -400)
        value = op(unreal.MaterialExpressionAdd, op(unreal.MaterialExpressionMultiply, jitter,
                   const(VALUE_JITTER, -1600, -140), -1400, -180), const(1.0, -1400, -100), -1200, -160)
        tint = op(unreal.MaterialExpressionMultiply, dried, value, -1000, -300)
        foliage_tint = lerp(one3, tint, vc, -850, -250, alpha_out='A')
        base = op(unreal.MaterialExpressionMultiply, vc, foliage_tint, -650, -100)
        base_output = ''
        tint_state = 'per_instance'
    except Exception as exc:  # noqa: BLE001
        log('WARN teinte par instance non construite, couleur de sommet seule: %s' % exc)

    # POLY_REALISM. Une couronne de spheres lisses est un primitif. Une masse large
    # (quelques metres) casse ce degradé a toute distance ; le grain fin et la
    # normale ne vivent qu'entre 8 et 40 m, puis la couronne redevient une masse.
    depth = mel.create_material_expression(mat, unreal.MaterialExpressionPixelDepth, -900, 400)
    pos = mel.create_material_expression(mat, unreal.MaterialExpressionWorldPosition, -900, 480)
    nrm = mel.create_material_expression(mat, unreal.MaterialExpressionVertexNormalWS, -900, 560)
    foliage_h = '''
float fade = 1.0 - smoothstep(800.0, 4000.0, Depth);
float3 p = Position * 0.012;
float broad = sin(p.x * 1.7 + sin(p.y * 1.3)) * sin(p.y * 1.5 + sin(p.z * 1.1));
float3 q = Position * 0.055;
float fine = sin(q.x * 2.1 + sin(q.y * 1.7)) * sin(q.y * 1.9 + sin(q.z * 2.2)) * sin(q.z * 1.6 + q.x);
float h = broad * 0.55 + fine * fade;
'''

    color_node = mel.create_material_expression(mat, unreal.MaterialExpressionCustom, -400, 0)
    color_node.set_editor_property('description', 'FoliageBreak')
    color_node.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    color_node.set_editor_property('code', foliage_h + '''
float3 n = normalize(Normal);
float3 a = cross(ddy(Position), n);
float3 b = cross(n, ddx(Position));
float det = dot(ddx(Position), a);
float3 g = sign(det) * (ddx(h) * a + ddy(h) * b);
FoliageNormal = normalize(max(abs(det), 0.000001) * n - g * (0.35 + 1.6 * fade) * saturate(Mask));
return Colour * (1.0 + (broad * 0.28 + fine * 0.12 * fade) * saturate(Mask));
''')
    extra = unreal.CustomOutput()
    extra.set_editor_property('output_name', 'FoliageNormal')
    extra.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    color_node.set_editor_property('additional_outputs', [extra])
    inputs = []
    for name in ('Colour', 'Mask', 'Position', 'Normal', 'Depth'):
        entry = unreal.CustomInput()
        entry.set_editor_property('input_name', name)
        inputs.append(entry)
    color_node.set_editor_property('inputs', inputs)
    if not mel.connect_material_expressions(base, base_output, color_node, 'Colour'):
        raise RuntimeError('foliage Colour not connected')
    if not mel.connect_material_expressions(vc, 'A', color_node, 'Mask'):
        raise RuntimeError('foliage Mask not connected')
    for source, name in ((pos, 'Position'), (nrm, 'Normal'), (depth, 'Depth')):
        if not mel.connect_material_expressions(source, '', color_node, name):
            raise RuntimeError('foliage input not connected: ' + name)
    mat.set_editor_property('tangent_space_normal', False)
    if not mel.connect_material_property(color_node, 'FoliageNormal', unreal.MaterialProperty.MP_NORMAL):
        raise RuntimeError('foliage normal not connected')

    wired = mel.connect_material_property(color_node, '', unreal.MaterialProperty.MP_BASE_COLOR)

    # Transmission = couleur cassee x chaleur x masque de feuillage.
    tinted = op(unreal.MaterialExpressionMultiply, color_node, const3(TRANSMISSION_WARMTH, -650, 220),
                -450, 140)
    masked = op(unreal.MaterialExpressionMultiply, tinted, vc, -300, 140, b_out='A')
    r_sss = mel.connect_material_property(masked, '', unreal.MaterialProperty.MP_SUBSURFACE_COLOR)
    r_rough = mel.connect_material_property(const(0.82, -350, 260), '', unreal.MaterialProperty.MP_ROUGHNESS)
    r_spec = mel.connect_material_property(const(0.18, -350, 460), '', unreal.MaterialProperty.MP_SPECULAR)

    # VENT -- balancement du feuillage, jamais du tronc.
    time_node = mel.create_material_expression(mat, unreal.MaterialExpressionTime, -1600, 620)
    phase = mel.create_material_expression(mat, unreal.MaterialExpressionPerInstanceRandom, -1600, 780)
    phased_time = op(unreal.MaterialExpressionAdd,
                     op(unreal.MaterialExpressionMultiply, time_node, const(WIND_SWAY_SPEED, -1600, 700), -1400, 660),
                     op(unreal.MaterialExpressionMultiply, phase, const(6.2832, -1600, 860), -1400, 800), -1200, 700)
    sway = mel.create_material_expression(mat, unreal.MaterialExpressionSine, -1050, 700)
    mel.connect_material_expressions(phased_time, '', sway, '')
    # Second axe, a une autre frequence : un balancement en ellipse, pas un metronome.
    sway_b = mel.create_material_expression(mat, unreal.MaterialExpressionSine, -1050, 820)
    mel.connect_material_expressions(op(unreal.MaterialExpressionAdd,
        op(unreal.MaterialExpressionMultiply, phased_time, const(0.83, -1200, 860), -1100, 860),
        const(1.7, -1200, 920), -1000, 900), '', sway_b, '')
    zero = const(0.0, -500, 980)

    wind_state = 'world'
    try:
        weight = vc  # masque de feuillage
        weight_out = 'A'
        try:
            local = mel.create_material_expression(mat, unreal.MaterialExpressionLocalPosition, -1200, 1000)
            local_z = mel.create_material_expression(mat, unreal.MaterialExpressionComponentMask, -1050, 1000)
            for channel, keep in (('r', False), ('g', False), ('b', True), ('a', False)):
                local_z.set_editor_property(channel, keep)
            if not mel.connect_material_expressions(local, '', local_z, ''):
                raise RuntimeError('LocalPosition non connectee')
            height = op(unreal.MaterialExpressionMultiply,
                        op(unreal.MaterialExpressionAdd, local_z, const(50.0, -1050, 1080), -900, 1020),
                        const(0.01, -900, 1100), -750, 1040)
            clamped = mel.create_material_expression(mat, unreal.MaterialExpressionSaturate, -600, 1040)
            mel.connect_material_expressions(height, '', clamped, '')
            weight = op(unreal.MaterialExpressionMultiply, vc, clamped, -450, 1000, a_out='A')
            weight_out = ''
            wind_state = 'local_height'
        except Exception as exc:  # noqa: BLE001
            log('WARN LocalPosition indisponible, balancement uniforme sur la couronne: %s' % exc)
            wind_state = 'local'
        amp = op(unreal.MaterialExpressionMultiply, weight, const(WIND_SWAY_LOCAL, -450, 1100), -300, 1040, a_out=weight_out)
        sx = op(unreal.MaterialExpressionMultiply, sway, amp, -150, 700)
        sy = op(unreal.MaterialExpressionMultiply, op(unreal.MaterialExpressionMultiply, sway_b,
                const(0.6, -1050, 900), -900, 860), amp, -150, 820)
        local_xyz = op(unreal.MaterialExpressionAppendVector,
                       op(unreal.MaterialExpressionAppendVector, sx, sy, 0, 740), zero, 150, 760)
        to_world = mel.create_material_expression(mat, unreal.MaterialExpressionTransform, 300, 760)
        to_world.set_editor_property('transform_source_type', enum_member(
            'MaterialVectorCoordTransformSource', 'TRANSFORMSOURCE_LOCAL', 'TRANSFORMSOURCE_Local', 'LOCAL'))
        to_world.set_editor_property('transform_type', enum_member(
            'MaterialVectorCoordTransform', 'TRANSFORM_WORLD', 'TRANSFORM_World', 'WORLD'))
        if not mel.connect_material_expressions(local_xyz, '', to_world, ''):
            raise RuntimeError('entree de Transform non connectee')
        r_wpo = mel.connect_material_property(to_world, '', unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
        if not r_wpo:
            raise RuntimeError('WPO non connecte')
    except Exception as exc:  # noqa: BLE001
        log('WARN vent local refuse, repli monde (%s uu): %s' % (WIND_SWAY_STRENGTH, exc))
        wind_state = 'world'
        sway_masked = op(unreal.MaterialExpressionMultiply,
                         op(unreal.MaterialExpressionMultiply, sway, const(WIND_SWAY_STRENGTH, -900, 1200), -750, 1200),
                         vc, -600, 1200, b_out='A')
        sway_xyz = op(unreal.MaterialExpressionAppendVector,
                      op(unreal.MaterialExpressionAppendVector, sway_masked, zero, -450, 1200), zero, -300, 1200)
        r_wpo = mel.connect_material_property(sway_xyz, '', unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)

    mat.set_editor_property('two_sided', True)

    shading = 'ABSENT'
    try:
        mat.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_TWO_SIDED_FOLIAGE)
        shading = str(mat.get_editor_property('shading_model'))
    except Exception as exc:  # noqa: BLE001
        log('WARN modele d ombrage non pose: %s' % exc)

    log("MATERIAL wiring base_color=%s tint=%s subsurface=%s roughness=%s specular=%s shading=%s wpo=%s wind=%s"
        % (wired, tint_state, r_sss, r_rough, r_spec, shading, r_wpo, wind_state))
    if not wired:
        raise RuntimeError('MATERIAL base colour not connected')
    errors = list(mel.recompile_material(mat) or [])
    if errors:
        for e in errors:
            unreal.log_error('VEGETATION_COMPILE ' + str(e))
        raise RuntimeError('M_AnastasisVegetation ne compile pas')
    unreal.EditorAssetLibrary.save_asset(MATERIAL_PATH)
    log("MATERIAL saved " + MATERIAL_PATH)
    return mat


def ensure_bark_material():
    """Default Lit. Du bois : opaque, mat, sans transmission.

    C'est tout l'interet de la passe -- l'ecorce cesse d'etre rendue par un
    modele d'ombrage de feuillage. Meme langage que le reste du projet : la
    couleur de sommet EST la semantique, aucune texture.
    """
    if unreal.get_editor_subsystem(unreal.AssetEditorSubsystem) is None:
        raise RuntimeError('Tree generation requires a live editor; no assets were modified')
    mat = open_material(BARK_MATERIAL_NAME, BARK_MATERIAL_PATH)
    mel = unreal.MaterialEditingLibrary

    vc = mel.create_material_expression(mat, unreal.MaterialExpressionVertexColor, -600, 0)
    uv = mel.create_material_expression(mat, unreal.MaterialExpressionTextureCoordinate, -1100, -120)
    position = mel.create_material_expression(mat, unreal.MaterialExpressionWorldPosition, -1100, 100)
    normal = mel.create_material_expression(mat, unreal.MaterialExpressionVertexNormalWS, -1100, 260)
    depth = mel.create_material_expression(mat, unreal.MaterialExpressionPixelDepth, -1100, 400)
    local = mel.create_material_expression(mat, unreal.MaterialExpressionLocalPosition, -1100, 500)
    local_z = mel.create_material_expression(mat, unreal.MaterialExpressionComponentMask, -900, 500)
    for channel, keep in (('r', False), ('g', False), ('b', True), ('a', False)):
        local_z.set_editor_property(channel, keep)
    if not mel.connect_material_expressions(local, '', local_z, ''):
        raise RuntimeError('Bark LocalPosition not connected')
    # Un seul noeud : deux Custom qui partagent les memes entrees perdent un lien
    # au second (le normal compilait sans Colour). Fissures pleines pres du tronc,
    # eteintes a 40 m. Le pied (Z local -50) entre dans la terre sur 8 uu.
    node = mel.create_material_expression(mat, unreal.MaterialExpressionCustom, -450, 0)
    node.set_editor_property('description', 'BarkBreak')
    node.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    node.set_editor_property('code', '''
        float fade = 1.0 - smoothstep(700.0, 4000.0, Depth);
        float foot = saturate((LocalZ + 50.0) / 8.0);
        float2 q = UV * float2(42.0, 7.0);
        float bend = sin(q.y*1.7)*0.16 + sin(q.y*4.1+q.x*0.19)*0.09;
        float ridge = pow(saturate(0.5+0.5*sin((q.x+bend)*6.283185)), 5.0);
        float grain = sin(q.x*31.0+sin(q.y*7.0))*sin(q.y*18.0+q.x);
        float h = (ridge*0.65 + grain*0.055) * fade;
        float3 n = normalize(Normal);
        float3 a = cross(ddy(Position), n), b = cross(n, ddx(Position));
        float det = dot(ddx(Position), a);
        float3 g = sign(det) * (ddx(h) * a + ddy(h) * b);
        BarkNormal = normalize(max(abs(det), 0.000001) * n - g * 1.5);
        return Colour * (0.72 + ridge * 0.42 * fade + grain * 0.06 * fade) * lerp(0.66, 1.0, foot);
    ''')
    extra = unreal.CustomOutput()
    extra.set_editor_property('output_name', 'BarkNormal')
    extra.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    node.set_editor_property('additional_outputs', [extra])
    inputs = []
    for name in ('UV', 'Colour', 'Position', 'Normal', 'Depth', 'LocalZ'):
        entry = unreal.CustomInput()
        entry.set_editor_property('input_name', name)
        inputs.append(entry)
    node.set_editor_property('inputs', inputs)
    wired_colour = None
    for out_name in ('', 'RGB', 'Color'):
        if mel.connect_material_expressions(vc, out_name, node, 'Colour'):
            wired_colour = out_name
            break
    if wired_colour is None:
        raise RuntimeError('Bark Colour not connected')
    for source, name in ((uv, 'UV'), (position, 'Position'), (normal, 'Normal'), (depth, 'Depth'), (local_z, 'LocalZ')):
        if not mel.connect_material_expressions(source, '', node, name):
            raise RuntimeError('Bark input not connected: ' + name)
    wired = mel.connect_material_property(node, '', unreal.MaterialProperty.MP_BASE_COLOR)
    if not mel.connect_material_property(node, 'BarkNormal', unreal.MaterialProperty.MP_NORMAL):
        raise RuntimeError('Bark normal output not connected')
    mat.set_editor_property('tangent_space_normal', False)

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
    errors = list(mel.recompile_material(mat) or [])
    if errors:
        for e in errors:
            unreal.log_error('BARK_COMPILE ' + str(e))
        raise RuntimeError('M_AnastasisBark ne compile pas')
    unreal.EditorAssetLibrary.save_asset(BARK_MATERIAL_PATH)
    log("BARK_MATERIAL saved " + BARK_MATERIAL_PATH)
    return mat


# FOREST_TERRAIN_P3 -- M_AnastasisRock : la pierre des rochers disperses (SM_Rock_*).
# Les meshes de roche portent WorldGrid a l'import ; les lieux composes les teintaient d'un
# aplat (StoneTint). Ici une pierre calcaire grecque : gris chaud, lichens jaunes et gris en
# taches, fissures plus sombres, grain fin en normale -- procedural en espace monde, aucune
# texture, comme l'ecorce. Rugueuse, peu speculaire.
ROCK_MATERIAL_NAME = "M_AnastasisRock"
ROCK_MATERIAL_PATH = MATERIAL_DIR + "/" + ROCK_MATERIAL_NAME


def ensure_rock_material():
    mat = open_material(ROCK_MATERIAL_NAME, ROCK_MATERIAL_PATH)
    mel = unreal.MaterialEditingLibrary
    position = mel.create_material_expression(mat, unreal.MaterialExpressionWorldPosition, -1100, 100)
    normal = mel.create_material_expression(mat, unreal.MaterialExpressionVertexNormalWS, -1100, 260)
    depth = mel.create_material_expression(mat, unreal.MaterialExpressionPixelDepth, -1100, 400)
    node = mel.create_material_expression(mat, unreal.MaterialExpressionCustom, -450, 0)
    node.set_editor_property('description', 'RockBreak')
    node.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    node.set_editor_property('code', '''
        float fade = 1.0 - smoothstep(700.0, 4000.0, Depth);
        float3 p = Position * 0.012;
        float3 w = abs(normalize(Normal)); w = w / (w.x + w.y + w.z);
        float n1 = sin(p.x*1.7+sin(p.y*2.3))*sin(p.y*1.9+sin(p.z*2.9))*sin(p.z*1.3+p.x*0.7);
        float n2 = sin(p.x*7.1+p.z*3.7)*sin(p.y*6.3+p.x*2.1)*sin(p.z*8.3+p.y*1.7);
        float n3 = sin(p.x*23.0+n2*3.0)*sin(p.y*29.0+p.z*7.0);
        float crack = pow(saturate(1.0 - abs(n2) * 5.0), 6.0) * fade;
        float lichen = saturate((n1 * 0.5 + 0.5 - 0.62) * 6.0) * saturate(w.z * 1.6);
        float h = n2 * 0.35 * fade + n3 * 0.12 * fade - crack * 0.8;
        float3 nn = normalize(Normal);
        float3 a = cross(ddy(Position), nn), b = cross(nn, ddx(Position));
        float det = dot(ddx(Position), a);
        float3 g = sign(det) * (ddx(h) * a + ddy(h) * b);
        RockNormal = normalize(max(abs(det), 0.000001) * nn - g * 0.8);
        float3 stone = float3(0.20, 0.185, 0.160) * (0.82 + n1 * 0.10 + n3 * 0.05 * fade);
        stone = lerp(stone, stone * 0.45, crack);
        float3 lichenCol = lerp(float3(0.30, 0.27, 0.12), float3(0.22, 0.22, 0.20), saturate(n2 * 0.5 + 0.5));
        float sit = lerp(0.74, 1.0, saturate(Normal.z * 0.45 + 0.78));
        return lerp(stone, lichenCol, lichen * 0.8) * sit;
    ''')
    extra = unreal.CustomOutput()
    extra.set_editor_property('output_name', 'RockNormal')
    extra.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    node.set_editor_property('additional_outputs', [extra])
    inputs = []
    for name in ('Position', 'Normal', 'Depth'):
        entry = unreal.CustomInput()
        entry.set_editor_property('input_name', name)
        inputs.append(entry)
    node.set_editor_property('inputs', inputs)
    for source, name in ((position, 'Position'), (normal, 'Normal'), (depth, 'Depth')):
        if not mel.connect_material_expressions(source, '', node, name):
            raise RuntimeError('Rock input not connected: ' + name)
    if not mel.connect_material_property(node, '', unreal.MaterialProperty.MP_BASE_COLOR):
        raise RuntimeError('Rock colour not connected')
    if not mel.connect_material_property(node, 'RockNormal', unreal.MaterialProperty.MP_NORMAL):
        raise RuntimeError('Rock normal not connected')
    mat.set_editor_property('tangent_space_normal', False)
    rough = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -350, 260)
    rough.set_editor_property('r', 0.88)
    mel.connect_material_property(rough, '', unreal.MaterialProperty.MP_ROUGHNESS)
    spec = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -350, 460)
    spec.set_editor_property('r', 0.25)
    mel.connect_material_property(spec, '', unreal.MaterialProperty.MP_SPECULAR)
    mat.set_editor_property('two_sided', False)
    errors = list(mel.recompile_material(mat) or [])
    if errors:
        for e in errors:
            unreal.log_error('ROCK_COMPILE ' + str(e))
        raise RuntimeError('M_AnastasisRock ne compile pas')
    unreal.EditorAssetLibrary.save_asset(ROCK_MATERIAL_PATH)
    log("ROCK_MATERIAL saved " + ROCK_MATERIAL_PATH)
    return mat


def main():
    log("start")
    if unreal.get_editor_subsystem(unreal.AssetEditorSubsystem) is None:
        raise RuntimeError('Tree generation requires a live editor; no assets were modified')
    if os.environ.get('ANASTASIS_TREE_MATERIALS_ONLY') == '1':
        ensure_material()
        ensure_bark_material()
        ensure_rock_material()
        log('RESULT::PASS materials_only=1')
        unreal.SystemLibrary.quit_editor()
        return True

    ensure_material()
    ensure_bark_material()

    built = {}
    for spec in FAMILIES:
        mesh = build_family(spec)
        far_mesh = distant_crown(spec)
        path = PACKAGE_PATH + "/" + spec["name"]
        save_static_mesh(mesh, path, far_mesh)
        built[spec["name"]] = (mesh, far_mesh)
        log("  %s -- %s" % (spec["name"], spec["note"]))

    # L'alias garde le chemin du repli code vivant sans dupliquer une septieme
    # silhouette : c'est la meme geometrie, sous le nom que le C++ connait.
    mesh, far_mesh = built[ALIAS_OF_GENERIC]
    save_static_mesh(mesh, PACKAGE_PATH + "/" + GENERIC_NAME, far_mesh)
    log("ALIAS %s <- %s" % (GENERIC_NAME, ALIAS_OF_GENERIC))

    # FOREST_TERRAIN_P1 : les essences mediterraneennes et grecques de montagne.
    species = species_specs()
    for spec in species:
        save_static_mesh(build_family(spec), PACKAGE_PATH + "/" + spec["name"], distant_crown(spec))
        log("  %s -- %s" % (spec["name"], spec["note"]))

    # FOREST_TERRAIN_P3 : arbustes du maquis, ronces, et la pierre des rochers disperses.
    shrubs = shrub_specs()
    for spec in shrubs:
        save_static_mesh(build_family(spec), PACKAGE_PATH + "/" + spec["name"], distant_crown(spec))
        log("  %s -- %s" % (spec["name"], spec["note"]))
    ensure_rock_material()

    log("RESULT::PASS meshes=%d species_meshes=%d shrub_meshes=%d"
        % (len(FAMILIES) + 1 + len(species) + len(shrubs), len(species), len(shrubs)))
    return True


if __name__ == '__main__':
    main()
