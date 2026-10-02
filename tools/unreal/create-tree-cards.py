"""LEAFCARDS_001 -- le chene vert en cartes de feuilles : atlas, materiau, mesh.

PROPRIETE DES ASSETS. Ce script est la SOURCE D'AUTORITE de
  /Game/Anastasis/Vegetation/Cards/T_Leaf_Atlas          l'atlas de grappes (tools/unreal/leaf-atlas.py)
  /Game/Anastasis/Materials/M_AnastasisFoliageCard       le materiau des cartes
  /Game/Anastasis/Vegetation/Cards/SM_Tree_HolmOak_Card_01..03  le chene vert en cartes (trois formes)
Il les recree a chaque run. Il ne touche NI `SM_Tree_*`, NI `M_AnastasisVegetation` : l'arbre actuel
reste le temoin du A/B (`tree-cards-lab.ps1`).

POURQUOI DES CARTES. Le feuillage actuel (`create_tree_asset.py::leaf_blades`) : des lames opaques de
huit triangles, semees par centaines autour d'enveloppes. De pres, des eclats ; de loin, un grain
facette. Les amincir a depegarni les couronnes (`handoffs/tree-canopy-002.md`). Ici : une couronne =
quelques dizaines de CARTES (un quad plie en deux bandes), chacune portant une gerbe de rameaux et de
feuilles dessinee dans l'atlas, decoupee par un masque alpha. Trois techniques font l'essentiel :
  1. NORMALES SPHERIQUES. Chaque sommet de carte recoit la normale de la couronne entiere (du centre
     vers le sommet, tiree vers le ciel), pas celle de son quad : la couronne s'eclaire comme un dome
     doux, pas comme des centaines de plaques ; les cartes ne clignotent plus selon leur orientation.
  2. OCCLUSION PEINTE. La couleur de sommet s'assombrit vers l'interieur de la couronne et s'eclaircit
     vers le haut : le volume se lit sans que la lumiere ait a le calculer.
  3. TRANSMISSION. Meme modele deux faces que M_AnastasisVegetation : la lumiere traverse la gerbe.

LE TRONC ET LES BRANCHES SONT CEUX DE L'ARBRE EXISTANT (memes fonctions de create_tree_asset.py, meme
graine) : seule la couronne change, donc le A/B ne compare que le feuillage.

MATERIAU : un DUPLICATA de M_AnastasisVegetation (vent de l'ensemble de la foret, teinte par instance,
transmission), auquel on ajoute l'albedo de l'atlas, le masque alpha (Masked), et la normale du sommet.
Le duplicata est refait a chaque run : le vent d'un arbre ne diverge donc jamais de celui de l'autre.

LOD. LOD0 : toutes les cartes. LOD1 : 60 % des cartes, 1,25 fois plus grandes (une couronne
d'un pas qui garde sa couverture : `distant_crown` l'avait constate, decimer des morceaux disjoints
supprime de la surface, pas du detail). LOD2 : des masses fermees, UV sur le bloc opaque de l'atlas.

Lancer par tools/unreal/create-tree-cards.ps1 (editeur dedie, discret, qui se ferme).
"""
import math
import os
import random
import sys

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import create_tree_asset as cta  # noqa: E402 -- memes fonctions que l'arbre existant : seule la couronne change

ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..'))
ATLAS_SRC = os.path.join(ROOT, 'SourceArt', 'Vegetation', 'leaf-atlas.png')
CARDS_PKG = '/Game/Anastasis/Vegetation/Cards'
TEX_NAME = 'T_Leaf_Atlas'
TEX_PATH = CARDS_PKG + '/' + TEX_NAME
SRC_MATERIAL = cta.MATERIAL_PATH
CARD_MATERIAL_NAME = 'M_AnastasisFoliageCard'
CARD_MATERIAL_PATH = cta.MATERIAL_DIR + '/' + CARD_MATERIAL_NAME
SHAPES = (1, 2, 3)         # les trois formes du chene vert, comme SM_Tree_HolmOak_01..03


def mesh_name(k):
    return 'SM_Tree_HolmOak_Card_%02d' % k


def mesh_path(k):
    return CARDS_PKG + '/' + mesh_name(k)

DETAIL_MEAN = 0.45          # egal a DETAIL_MEAN de leaf-atlas.py
CLIP = 0.42                 # seuil du masque alpha
SOLID_UV = (0.9965, 0.9965)  # bloc opaque (coin bas droit) de l'atlas, pour les masses fermees
UP_BLEND = 0.30             # part du ciel dans la normale spherique
LOD_SCREEN = (1.0, 0.22, 0.055)
OAK_CELLS = (0, 1)          # cases de l'atlas utilisees par le chene vert

eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary


def log(msg):
    unreal.log('[create-tree-cards] ' + str(msg))


# ------------------------------------------------------------------------------------------ atlas
def import_atlas():
    if not os.path.isfile(ATLAS_SRC):
        raise RuntimeError('atlas absent : lancer python tools/unreal/leaf-atlas.py (%s)' % ATLAS_SRC)
    task = unreal.AssetImportTask()
    task.set_editor_property('filename', ATLAS_SRC)
    task.set_editor_property('destination_path', CARDS_PKG)
    task.set_editor_property('destination_name', TEX_NAME)
    task.set_editor_property('replace_existing', True)
    task.set_editor_property('automated', True)
    task.set_editor_property('save', False)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    tex = eal.load_asset(TEX_PATH)
    if not isinstance(tex, unreal.Texture2D):
        raise RuntimeError('%s : import sans Texture2D' % TEX_PATH)
    tex.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_BC7)
    tex.set_editor_property('srgb', True)
    tex.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_WORLD)
    tex.set_editor_property('mip_gen_settings', unreal.TextureMipGenSettings.TMGS_FROM_TEXTURE_GROUP)
    tex.set_editor_property('address_x', unreal.TextureAddress.TA_CLAMP)
    tex.set_editor_property('address_y', unreal.TextureAddress.TA_CLAMP)
    tex.set_editor_property('compression_no_alpha', False)
    tex.set_editor_property('never_stream', True)
    # Couverture alpha conservee dans les mips (seuil = celui du masque) : sans elle les feuilles
    # maigrissent avec la distance et la couronne se depegarnit -- le defaut meme que ce travail corrige.
    coverage = False
    for name in ('do_scale_mips_for_alpha_coverage', 'b_do_scale_mips_for_alpha_coverage'):
        try:
            tex.set_editor_property(name, True)
            coverage = True
            break
        except Exception:  # noqa: BLE001
            continue
    try:
        tex.set_editor_property('alpha_coverage_thresholds', unreal.Vector4(0.0, 0.0, 0.0, CLIP))
    except Exception as exc:  # noqa: BLE001
        log('WARN seuil de couverture alpha non reglable : %s' % exc)
        coverage = False
    eal.save_asset(TEX_PATH)
    log('TEXTURE %s %dx%d coverage=%s' % (TEX_PATH, tex.blueprint_get_size_x(), tex.blueprint_get_size_y(), coverage))
    return tex


# ---------------------------------------------------------------------------------------- materiau
def ensure_card_material(tex):
    """Duplicata de M_AnastasisVegetation + atlas + masque + normale du sommet."""
    if eal.does_asset_exist(CARD_MATERIAL_PATH):
        eal.delete_asset(CARD_MATERIAL_PATH)
    if not eal.does_asset_exist(SRC_MATERIAL):
        raise RuntimeError('%s absent : lancer create_tree_asset.py' % SRC_MATERIAL)
    mat = eal.duplicate_asset(SRC_MATERIAL, CARD_MATERIAL_PATH)
    if mat is None:
        raise RuntimeError('duplicate_asset a rendu None')

    def const(value, x, y):
        node = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, x, y)
        node.set_editor_property('r', value)
        return node

    def mul(a, b, x, y, a_out='', b_out=''):
        node = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, x, y)
        if not mel.connect_material_expressions(a, a_out, node, 'A'):
            raise RuntimeError('Multiply A non connecte')
        if not mel.connect_material_expressions(b, b_out, node, 'B'):
            raise RuntimeError('Multiply B non connecte')
        return node

    sample = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSample, -1900, 900)
    sample.set_editor_property('texture', tex)
    albedo = mul(sample, const(1.0 / DETAIL_MEAN, -1700, 980), -1500, 900, a_out='RGB')

    color_node = mel.get_material_property_input_node(mat, unreal.MaterialProperty.MP_BASE_COLOR)
    if color_node is None:
        raise RuntimeError('M_AnastasisVegetation : BaseColor sans noeud')
    base = mul(color_node, albedo, -250, 0)
    if not mel.connect_material_property(base, '', unreal.MaterialProperty.MP_BASE_COLOR):
        raise RuntimeError('BaseColor non recable')

    sss = mel.get_material_property_input_node(mat, unreal.MaterialProperty.MP_SUBSURFACE_COLOR)
    if sss is not None:
        sss_out = mel.get_material_property_input_node_output_name(mat, unreal.MaterialProperty.MP_SUBSURFACE_COLOR)
        wired = mel.connect_material_property(mul(sss, albedo, -150, 160, a_out=sss_out or ''), '',
                                              unreal.MaterialProperty.MP_SUBSURFACE_COLOR)
        if not wired:
            raise RuntimeError('SubsurfaceColor non recable')

    # Normale du sommet (spherique, peinte dans le mesh), pas le bruit procedural de l'arbre : TwoSidedSign
    # annule le retournement de la face arriere (meme correctif que M_AnastasisGrass).
    vnormal = mel.create_material_expression(mat, unreal.MaterialExpressionVertexNormalWS, -900, 1250)
    sign = mel.create_material_expression(mat, unreal.MaterialExpressionTwoSidedSign, -900, 1320)
    facing = mul(vnormal, sign, -600, 1280)
    mat.set_editor_property('tangent_space_normal', False)
    if not mel.connect_material_property(facing, '', unreal.MaterialProperty.MP_NORMAL):
        raise RuntimeError('Normal non recablee')

    if not mel.connect_material_property(sample, 'A', unreal.MaterialProperty.MP_OPACITY_MASK):
        raise RuntimeError('OpacityMask non cable')
    mat.set_editor_property('blend_mode', unreal.BlendMode.BLEND_MASKED)
    mat.set_editor_property('opacity_mask_clip_value', CLIP)
    mat.set_editor_property('two_sided', True)
    mat.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_TWO_SIDED_FOLIAGE)

    errors = list(mel.recompile_material(mat) or [])
    if errors:
        for e in errors:
            unreal.log_error('FOLIAGE_CARD_COMPILE ' + str(e))
        raise RuntimeError('%s ne compile pas' % CARD_MATERIAL_NAME)
    eal.save_asset(CARD_MATERIAL_PATH)
    log('MATERIAL %s blend=%s clip=%.2f' % (CARD_MATERIAL_PATH, mat.get_editor_property('blend_mode'), CLIP))
    return mat


# ------------------------------------------------------------------------------------------ cartes
class Buffers(object):
    def __init__(self):
        self.v, self.n, self.col, self.uv, self.t = [], [], [], [], []

    def vertex(self, p, normal, rgba, uv):
        self.v.append(unreal.Vector(p[0], p[1], p[2]))
        self.n.append(unreal.Vector(normal[0], normal[1], normal[2]))
        self.col.append(unreal.LinearColor(rgba[0], rgba[1], rgba[2], rgba[3]))
        self.uv.append(unreal.Vector2D(uv[0], uv[1]))
        return len(self.v) - 1

    def tri(self, a, b, c):
        self.t.append(unreal.IntVector(a, b, c))

    def mesh(self, material_id):
        buffers = unreal.GeometryScriptSimpleMeshBuffers(
            vertices=self.v, normals=self.n, triangles=self.t, vertex_colors=self.col, uv0=self.uv)
        mesh, unused = unreal.GeometryScript_MeshEdits.append_buffers_to_mesh(
            unreal.DynamicMesh(), buffers, material_id=material_id)
        return mesh


def norm(v):
    length = math.sqrt(sum(c * c for c in v)) or 1.0
    return tuple(c / length for c in v)


def cross(a, b):
    return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])


def spherical_normal(p, centre, radii):
    """La normale de la couronne entiere au point p : ellipsoide, tiree vers le ciel."""
    d = tuple((p[i] - centre[i]) / radii[i] for i in range(3))
    n = norm(d)
    return norm((n[0] * (1.0 - UP_BLEND), n[1] * (1.0 - UP_BLEND), n[2] * (1.0 - UP_BLEND) + UP_BLEND))


def crown_frame(spec):
    """Centre et rayons de l'ensemble des lobes (espace de dessin, avant normalisation)."""
    lobes = spec['lobes']
    lo = [min(l[1 + i] - l[0] for l in lobes) for i in range(3)]
    hi = [max(l[1 + i] + l[0] for l in lobes) for i in range(3)]
    centre = tuple(0.5 * (lo[i] + hi[i]) for i in range(3))
    radii = tuple(max(0.5 * (hi[i] - lo[i]), 1.0) for i in range(3))
    return centre, radii


def draw_cards(spec, rng):
    """Une liste de cartes : ancre, axe, taille, case de l'atlas, ton, hasard de conservation (LOD1)."""
    centre, radii = crown_frame(spec)
    cards = []
    per_lobe = spec.get('cards_per_lobe', 78)
    for radius, cx, cy, cz, squash in spec['lobes']:
        for k in range(per_lobe):
            theta = k * 2.399963 + rng.uniform(-0.35, 0.35)
            z = 1.0 - 2.0 * (k + 0.5) / per_lobe
            ring = math.sqrt(max(0.0, 1.0 - z * z))
            shell = rng.uniform(0.35, 1.02)
            direction = norm((ring * math.cos(theta), ring * math.sin(theta), z * squash))
            anchor = (cx + radius * shell * direction[0] * 0.55, cy + radius * shell * direction[1] * 0.55,
                      cz + radius * shell * direction[2] * 0.55)
            # Axe de la gerbe : vers l'exterieur de la couronne, un peu releve ; la gerbe s'ouvre comme
            # un rameau, pas comme une plaque posee sur la surface.
            outward = norm(tuple((anchor[i] - centre[i]) / radii[i] for i in range(3)))
            # Gerbes dirigees vers l'exterieur MAIS pas toutes : a 70 m, des gerbes purement radiales font
            # une etoile (un palmier). Une part de hasard casse la symetrie ; le ciel releve un peu l'axe.
            wander = norm((rng.gauss(0, 1), rng.gauss(0, 1), rng.gauss(0, 1)))
            mix = rng.uniform(0.25, 0.75)
            axis = norm((outward[0] * (1 - mix) + wander[0] * mix, outward[1] * (1 - mix) + wander[1] * mix,
                         outward[2] * (1 - mix) + wander[2] * mix + 0.25))
            depth = min(1.0, math.dist(tuple(anchor[i] / radii[i] for i in range(3)),
                                       tuple(centre[i] / radii[i] for i in range(3))))
            size = radius * rng.uniform(0.45, 0.80)
            cards.append({'anchor': anchor, 'axis': axis, 'size': size, 'roll': rng.uniform(0.0, math.tau),
                          'cell': OAK_CELLS[rng.randrange(len(OAK_CELLS))], 'depth': depth,
                          'tone': rng.uniform(0.82, 1.16), 'alt': rng.random() < spec.get('foliage_alt_share', 0.0),
                          'keep': rng.random(), 'droop': rng.uniform(0.10, 0.28)})
    return cards, centre, radii


def add_card(buf, card, spec, centre, radii, lod):
    size = card['size'] * (1.0 if lod == 0 else 1.15)
    axis = card['axis']
    ref = (0.0, 0.0, 1.0) if abs(axis[2]) < 0.95 else (1.0, 0.0, 0.0)
    right = norm(cross(axis, ref))
    up_in_plane = cross(right, axis)
    ca, sa = math.cos(card['roll']), math.sin(card['roll'])
    right = tuple(right[i] * ca + up_in_plane[i] * sa for i in range(3))
    base = spec['foliage_alt'] if (card['alt'] and spec.get('foliage_alt') is not None) else spec['foliage']
    # Occlusion peinte : sombre au coeur de la couronne, clair en surface et vers le haut.
    height_lift = max(0.0, min(1.0, 0.5 + 0.5 * (card['anchor'][2] - centre[2]) / radii[2]))
    tone = card['tone'] * (0.50 + 0.52 * card['depth']) * (0.90 + 0.18 * height_lift)
    rgba = (min(1.0, base.r * tone), min(1.0, base.g * tone), min(1.0, base.b * tone), 1.0)
    u0, v0 = (card['cell'] % 2) * 0.5, (card['cell'] // 2) * 0.5
    ids = []
    # Trois rangees de deux sommets : la gerbe s'incurve vers le bas a mesure qu'elle s'eloigne.
    for row in range(3):
        t = row / 2.0
        centre_p = tuple(card['anchor'][i] + axis[i] * size * t for i in range(3))
        centre_p = (centre_p[0], centre_p[1], centre_p[2] - card['droop'] * size * t * t)
        half = 0.5 * size
        for side in (-1.0, 1.0):
            p = tuple(centre_p[i] + right[i] * half * side for i in range(3))
            uv = (u0 + 0.5 * (0.5 + 0.5 * side * 0.99), v0 + 0.5 * (1.0 - t * 0.985))
            ids.append(buf.vertex(p, spherical_normal(p, centre, radii), rgba, uv))
    for row in range(2):
        a, b, c, d = ids[2 * row], ids[2 * row + 1], ids[2 * row + 2], ids[2 * row + 3]
        buf.tri(a, c, b)
        buf.tri(b, c, d)


def card_mesh(spec, cards, centre, radii, lod):
    buf = Buffers()
    keep = (1.0, 0.75, 0.0)[lod]
    for card in cards:
        if card['keep'] < keep:
            add_card(buf, card, spec, centre, radii, lod)
    return buf.mesh(cta.SLOT_FOLIAGE)


def lump_mesh(spec, rng):
    """LOD2 : masses fermees (comme distant_crown), UV sur le bloc opaque de l'atlas."""
    buf = Buffers()
    for radius, cx, cy, cz, squash in spec['lobes']:
        for k in range(3):
            angle = k * 2.399963 + rng.uniform(-0.35, 0.35)
            r = radius * rng.uniform(0.60, 0.80)
            offset = radius * 0.43
            ox, oy = cx + offset * math.cos(angle), cy + offset * math.sin(angle)
            oz = cz + radius * rng.uniform(-0.32, 0.32) * squash
            sq = squash * rng.uniform(0.80, 1.20)
            tone = rng.uniform(0.66, 0.90)
            c = spec['foliage']
            rgba = (c.r * tone, c.g * tone, c.b * tone, 1.0)
            rows, cols = 5, 9
            grid = []
            for i in range(rows + 1):
                phi = math.pi * i / rows
                for j in range(cols):
                    th = math.tau * j / cols
                    n = (math.sin(phi) * math.cos(th), math.sin(phi) * math.sin(th), math.cos(phi))
                    p = (ox + r * n[0], oy + r * n[1], oz + r * sq * n[2])
                    grid.append(buf.vertex(p, norm((n[0], n[1], n[2] / max(sq, 0.2))), rgba, SOLID_UV))
            for i in range(rows):
                for j in range(cols):
                    a = grid[i * cols + j]
                    b = grid[i * cols + (j + 1) % cols]
                    cc = grid[(i + 1) * cols + j]
                    d = grid[(i + 1) * cols + (j + 1) % cols]
                    buf.tri(a, cc, b)
                    buf.tri(b, cc, d)
    return buf.mesh(cta.SLOT_FOLIAGE)


# ------------------------------------------------------------------------------------------- arbre
def wood_parts(spec, rng):
    """Tronc, branches, rameaux et racines : exactement ceux de create_tree_asset.build_family."""
    parts = []
    rs = spec.get('radius_scale', 0.55)
    slender = [(s[0] * rs, s[1] * rs, *s[2:]) for s in spec['trunk']]
    trunk = cta.build_trunk(slender, spec['bark'])
    if trunk is not None:
        parts.append(trunk)
    limbs = cta.build_limbs([(s[0] * rs, s[1] * rs, *s[2:]) for s in spec['limbs']], spec['bark'])
    if limbs is not None:
        parts.append(limbs)
    stems = cta.build_stems(spec)
    if stems is not None:
        parts.append(stems)
    blades, twigs = cta.living_crown(spec, rng)  # les lames sont ecartees, les rameaux gardes
    parts.append(twigs)
    if spec.get('roots', True) and (slender or spec.get('stems')):
        roots = unreal.DynamicMesh()
        radius = slender[0][0] if slender else spec['stems'][0][2]
        for k in range(6):
            angle = k * math.tau / 6 + 0.25 + rng.uniform(-0.18, 0.18)
            reach = radius * rng.uniform(1.6, 2.2)
            joint = (reach * 0.52 * math.cos(angle + 0.10), reach * 0.52 * math.sin(angle + 0.10), -49.0)
            end = (reach * math.cos(angle), reach * math.sin(angle), -49.8)
            roots = cta.branch_between(roots, (0, 0, -47.4), joint, radius * 0.40, radius * 0.16, steps=10)
            roots = cta.branch_between(roots, joint, end, radius * 0.16, 0.07, steps=8)
        parts.append(cta.coloured(roots, spec['bark']))
    return parts


def build_lods(spec):
    # MEME graine que build_family : memes futs, memes branches, memes rameaux.
    rng = random.Random(spec['name'] + ':forest-walk-001')
    parts = wood_parts(spec, rng)
    wood = unreal.DynamicMesh()
    for part in parts:
        wood = cta.merge(wood, part)
    # Les normales fractionnees du bois ; PAS celles des cartes, qui sont peintes (spheriques).
    wood = cta.shade(wood)
    cards, centre, radii = draw_cards(spec, random.Random(spec['name'] + ':cards'))
    lods = []
    for lod in (0, 1):
        # append_mesh MODIFIE sa cible : un maillage neuf par LOD, le bois n'est que la source.
        mesh = cta.merge(unreal.DynamicMesh(), wood)
        mesh = cta.merge(mesh, card_mesh(spec, cards, centre, radii, lod))
        lods.append(mesh)
    far_wood = unreal.DynamicMesh()
    stems = cta.build_stems(spec, limit=spec.get('distant_stems', 4)) if spec.get('distant_stems', 4) > 0 else None
    if stems is not None:
        far_wood = cta.merge(far_wood, stems)
    far = cta.merge(cta.shade(far_wood), lump_mesh(spec, random.Random(spec['name'] + ':lumps')))
    lods.append(far)
    # Une seule normalisation, calculee sur LOD0 puis appliquee aux autres : Z = [-50, +50] pour les trois.
    box0 = unreal.GeometryScript_MeshQueries.get_mesh_bounding_box(lods[0])
    factor = cta.NORMALISED_HEIGHT / (box0.max.z - box0.min.z)
    out = []
    for mesh in lods:
        mesh = unreal.GeometryScript_MeshTransforms.scale_mesh(
            mesh, unreal.Vector(factor, factor, factor), unreal.Vector(0.0, 0.0, box0.min.z))
        mesh = unreal.GeometryScript_MeshTransforms.translate_mesh(
            mesh, unreal.Vector(0.0, 0.0, cta.NORMALISED_BASE_Z - box0.min.z))
        out.append(mesh)
    after = unreal.GeometryScript_MeshQueries.get_mesh_bounding_box(out[0])
    if abs(after.min.z - cta.NORMALISED_BASE_Z) > cta.NORMALISE_TOLERANCE:
        raise RuntimeError('normalisation ratee : bas=%.4f' % after.min.z)
    log('%s cartes=%d lobes=%d cadre=%s' % (spec['name'], len(cards), len(spec['lobes']), [round(c, 1) for c in centre]))
    return out


def save_card_mesh(lods, asset_path, card_material):
    if eal.does_asset_exist(asset_path):
        eal.delete_asset(asset_path)
    options = unreal.GeometryScriptCreateNewStaticMeshAssetOptions()
    options.set_editor_property('enable_recompute_normals', False)   # les normales spheriques sont a nous
    options.set_editor_property('enable_recompute_tangents', True)
    options.set_editor_property('enable_nanite', False)
    asset, outcome = unreal.GeometryScript_NewAssetUtils.create_new_static_mesh_asset_from_mesh(lods[0], asset_path, options)
    if asset is None:
        raise RuntimeError('%s : create_new_static_mesh_asset_from_mesh a rendu None (%s)' % (asset_path, outcome))
    bark = eal.load_asset(cta.BARK_MATERIAL_PATH)
    asset.set_material(cta.SLOT_FOLIAGE, card_material)
    if bark is not None:
        asset.set_material(cta.SLOT_WOOD, bark)
    slots = len(asset.get_editor_property('static_materials'))
    if slots != 2:
        raise RuntimeError('%s : %d slot(s) de materiau au lieu de 2' % (asset_path, slots))
    try:
        unreal.EditorStaticMeshLibrary.add_simple_collisions(asset, unreal.ScriptingCollisionShapeType.NDOP10_X)
    except Exception as exc:  # noqa: BLE001
        log('WARN collision simple refusee : %s' % exc)
    reductions = unreal.StaticMeshReductionOptions()
    reductions.auto_compute_lod_screen_size = False
    reductions.reduction_settings = [unreal.StaticMeshReductionSettings(percent_triangles=1.0, screen_size=s)
                                     for s in LOD_SCREEN]
    subsystem = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    if subsystem is None:
        raise RuntimeError('StaticMeshEditorSubsystem absent (editeur vivant requis)')
    if subsystem.set_lods(asset, reductions) != 3:
        raise RuntimeError('%s : 3 LOD attendus' % asset_path)
    copy_options = unreal.GeometryScriptCopyMeshToAssetOptions(enable_recompute_tangents=True)
    try:
        copy_options.set_editor_property('enable_recompute_normals', False)
    except Exception as exc:  # noqa: BLE001
        log('WARN enable_recompute_normals absent des options de copie : %s' % exc)
    for lod in (1, 2):
        unused, outcome = unreal.GeometryScript_AssetUtils.copy_mesh_to_static_mesh(
            lods[lod], asset, copy_options, unreal.GeometryScriptMeshWriteLOD(lod_index=lod))
        if outcome != unreal.GeometryScriptOutcomePins.SUCCESS:
            raise RuntimeError('%s : copie LOD%d refusee (%s)' % (asset_path, lod, outcome))
        settings = subsystem.get_lod_reduction_settings(asset, lod)
        settings.percent_triangles = 1.0
        settings.percent_vertices = 1.0
        settings.base_lod_model = lod
        subsystem.set_lod_reduction_settings(asset, lod, settings)
    triangles = [asset.get_num_triangles(i) for i in range(asset.get_num_lods())]
    eal.save_asset(asset.get_path_name())
    bounds = asset.get_bounding_box()
    log('SAVED %s lods=%d triangles=%s bounds z=[%.2f,%.2f] xy=[%.1f,%.1f]' % (
        asset_path, asset.get_num_lods(), triangles, bounds.min.z, bounds.max.z,
        bounds.max.x - bounds.min.x, bounds.max.y - bounds.min.y))
    return triangles


def main():
    log('start')
    if unreal.get_editor_subsystem(unreal.AssetEditorSubsystem) is None:
        raise RuntimeError('editeur vivant requis ; aucun asset modifie')
    # Les meshes d'abord, le materiau ensuite : supprimer un materiau pendant qu'un mesh le reference laisse
    # un paquet que ForceDeleteObjects ne sait plus decharger (cf. create-ground-cover.py).
    for k in SHAPES:
        if eal.does_asset_exist(mesh_path(k)):
            eal.delete_asset(mesh_path(k))
    tex = import_atlas()
    material = ensure_card_material(tex)
    specs = {s['name']: s for s in cta.species_specs()}
    report = []
    for k in SHAPES:
        spec = specs['SM_Tree_HolmOak_%02d' % k]
        triangles = save_card_mesh(build_lods(spec), mesh_path(k), material)
        reference = eal.load_asset(cta.PACKAGE_PATH + '/' + spec['name'])
        if reference is not None:
            log('REFERENCE %s triangles=%s' % (spec['name'], [reference.get_num_triangles(i) for i in range(reference.get_num_lods())]))
        report.append('%s=%s' % (mesh_name(k), triangles))
    log('TREE_CARDS_ASSETS::PASS %s' % ' '.join(report))


if __name__ == '__main__':
    try:
        main()
    except Exception as exc:  # noqa: BLE001
        import traceback
        unreal.log_error('TREE_CARDS_ASSETS::FAIL %s\n%s' % (exc, traceback.format_exc()))
    finally:
        if os.environ.get('ANASTASIS_TREE_CARDS_QUIT') == '1':
            unreal.SystemLibrary.quit_editor()
