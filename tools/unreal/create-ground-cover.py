"""
GROUND_COVER_001 -- la strate herbacee des espaces ouverts : trois touffes et leur materiau.

PROPRIETE DES ASSETS. Ce script est la SOURCE D'AUTORITE de
  /Game/Anastasis/GroundCover/SM_Grass_MeadowTall_01    H1 prairie haute (clairiere naturelle)
  /Game/Anastasis/GroundCover/SM_Grass_MeadowShort_01   H2 prairie basse (terre seche, tassee)
  /Game/Anastasis/GroundCover/SM_Grass_Sedge_01         H3 prairie humide (laiches, carex)
  /Game/Anastasis/GroundCover/SM_Grass_HeathTussock_01  H6a touffe d'eboulis (lande, graminee dure)
  /Game/Anastasis/GroundCover/SM_Grass_Heather_01       H6b callune de lande, epis mauves
  /Game/Anastasis/GroundCover/SM_Grass_Fern_01          H5a fougere en volant, frondes divisees
  /Game/Anastasis/GroundCover/SM_Grass_HartsTongue_01   H5b scolopendre, lanieres entieres
  /Game/Anastasis/GroundCover/SM_Grass_WoodHerb_01      H5c herbacee d'ombre, luzule et anemones
  /Game/Anastasis/Materials/M_AnastasisGrass
Il les recree a l'identique a chaque run (graine fixe). Une touffe se change ICI, dans les
chiffres : une retouche a la main ne survit pas au run suivant.

LES PLANCHES, PAS L'ESTIME. docs/visual/reference/ :
  pontique-etat-zero-5-lisieres-transitions  clairiere : herbes hautes, epis dores sur vert sourd ;
                                             prairie humide : graminees, carex ; "aucune ligne fixe"
  pontique-etat-zero-1-biome-fondateur       clairieres naturelles : prairies sauvages
  pontique-etat-zero-2-anatomie-sol          terre seche / tassee ; vegetation rivulaire : laiches
  pontique-etat-zero-4-hydrologie-territoire zone humide : prairies saturees
Ce que toutes montrent : jamais un tapis. Des touffes de hauteurs melees, vert a la base,
ocre et paille aux pointes, de la vieille herbe seche au pied, la terre qui perce entre elles.
Fin d'ete, pas pelouse.

GEOMETRIE. Des lames opaques en arc (aucune texture, aucun alpha), groupees en sous-touffes
dans une tache de ~1,2 m. Opaque parce que le projet n'a aucune texture de vegetation et que
la couleur de sommet EST la semantique (meme langage que M_AnastasisVegetation). Pivot a la
base, Z=0 au sol, en centimetres reels : l'incarnation pose la touffe sur le sol rendu, sans
la convention [-50,+50] des arbres.

COULEUR DE SOMMET. RGB = couleur, degrade base -> pointe. ALPHA = hauteur absolue / 100 cm :
le vent la lit au carre, une herbe rase ne bouge presque pas, une haute ondule.

NORMALES. Tirees vers le ciel (UP_BLEND) : une prairie se lit comme une masse eclairee par le
haut, pas comme mille lames qui clignotent selon leur orientation.

LODS. Ecrits a la main, pas reduits : une reduction automatique effondre des lames fines en
eclats. LOD0 (la touffe qui remplit l'ecran, sous ~7 m) garde l'arc en plusieurs segments.
LOD1 garde 45 % des lames, a peine plus larges. LOD2, 30 % et un seul triangle, ne s'affiche
que lorsque la touffe est deja petite (~60 m), dans le fondu.

Lancer par tools/unreal/create-ground-cover.ps1 (editeur dedie, discret, qui se ferme), ou
dans la console Python d'un editeur ouvert. Le commandlet Python est refuse : la pose des
LOD exige StaticMeshEditor.
"""
import math
import os
import random
import unreal
import sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import weather_materials


PACKAGE_PATH = '/Game/Anastasis/GroundCover'
MATERIAL_DIR = '/Game/Anastasis/Materials'
MATERIAL_NAME = 'M_AnastasisGrass'
MATERIAL_PATH = MATERIAL_DIR + '/' + MATERIAL_NAME
SEED = 20260930

UP_BLEND = 0.72
# Seuils d'ecran des LOD. Une touffe de ~90 cm de rayon : ~0.22 vers 7 m, ~0.025 vers 60 m.
# Le premier plan (prairie_low, 60 cm) reste en LOD0. Le triangle large de LOD2 n'arrive
# qu'une fois la touffe petite, la ou le fondu de distance la retire deja.
LOD_SCREEN = (1.0, 0.22, 0.025)
# Fondu de distance, en uu : PARAMETRES du materiau (FadeStart / FadeEnd). Chaque HISM les regle
# par MID (AnastasisWorldEmbodiment::PlaceGroundCover) : touffes proches 40 -> 55 m, lointaines
# 70 -> 105 m, chacune coupee juste apres. Les valeurs ci-dessous ne sont que les defauts du
# materiau nu, alignes sur le tier lointain. v2 finissait a 75 m : une lisiere d'herbe nette
# barrait chaque vue a hauteur d'homme.
FADE_START = 7000.0
FADE_END = 10500.0
WIND_AMPLITUDE = 9.0
WIND_SPEED = 1.6
TRANSMISSION_WARMTH = (1.35, 1.2, 0.6)


def log(msg):
    unreal.log('[create-ground-cover] ' + str(msg))


def c(r, g, b):
    return (r, g, b)


# Palettes lineaires. v1 (calee sous le feuillu de M_AnastasisVegetation) sortait brune et
# sombre contre le sol clair de MI_AnastasisGround : la prairie se lisait comme des taches
# posees sur le sol, pas comme le sol lui-meme (capture prairie_eye, GROUND_COVER_001 v1).
# L'herbe doit rester a peine plus sombre que le sol qu'elle couvre, la pointe au niveau
# du feuillu, l'ocre de fin d'ete franc sans virer au beige.
FAMILIES = [
    {
        'name': 'SM_Grass_MeadowTall_01',
        'note': 'H1 prairie haute : 38-72 cm, epis ocre, 15 % de paille',
        'radius': 62.0, 'tufts': 10, 'blades': 220, 'spread': 15.0, 'fill': 0.35,
        # Largeur ~0,5-1 cm : a 60 cm une lame de 2 cm faisait un ruban de ~30 px.
        # Base assombrie : le pied entre dans le sol, la pointe garde la luminance du pre.
        'height': (38.0, 72.0), 'width': (0.55, 1.05), 'lean': (0.05, 0.35), 'bend': (0.25, 0.85),
        'segments': 7,
        # EZ5 clairiere : un vert sourd sous des epis dores, pas une savane. v2 a 40 % d'ocre
        # se lisait brun-sec (capture prairie_eye v2) : l'ocre reste, minoritaire.
        'base': c(0.026, 0.040, 0.014),
        # FOREST_TERRAIN_P4 : prairie mediterraneenne de debut d'ete -- le vert vire a l'olive et
        # la part d'ocre et de paille passe de 38 a 55 %. Meme luminance : la prairie reste au
        # niveau du sol (MI_AnastasisGround) et des couronnes, elle change de saison, pas de valeur.
        'tips': [(0.45, c(0.135, 0.165, 0.058)), (0.35, c(0.265, 0.215, 0.082)), (0.20, None)],
        'straw': (c(0.150, 0.128, 0.064), c(0.320, 0.270, 0.135)),
        'heads': 18, 'head_height': (62.0, 92.0), 'head_color': c(0.250, 0.205, 0.105),
    },
    {
        'name': 'SM_Grass_MeadowShort_01',
        'note': 'H2 prairie basse : lames de 11-28 cm, plus verte, la terre perce',
        'radius': 62.0, 'tufts': 12, 'blades': 240, 'spread': 14.0, 'fill': 0.45,
        'height': (12.0, 30.0), 'width': (0.42, 0.80), 'lean': (0.10, 0.50), 'bend': (0.30, 1.00),
        'segments': 6,
        'base': c(0.028, 0.042, 0.015),
        'tips': [(0.58, c(0.145, 0.175, 0.060)), (0.30, c(0.230, 0.205, 0.085)), (0.12, None)],
        'straw': (c(0.140, 0.122, 0.064), c(0.285, 0.245, 0.130)),
        'heads': 0,
    },
    {
        'name': 'SM_Grass_Sedge_01',
        'note': 'H3 prairie humide : laiches de 45-80 cm de lame, retombant a ~35-60 cm, vert bleute',
        'radius': 55.0, 'tufts': 5, 'blades': 120, 'spread': 10.0, 'fill': 0.2,
        'height': (45.0, 80.0), 'width': (0.90, 1.50), 'lean': (0.15, 0.40), 'bend': (0.80, 1.60),
        'segments': 7,
        'base': c(0.020, 0.038, 0.022),
        'tips': [(0.80, c(0.095, 0.150, 0.088)), (0.20, c(0.185, 0.158, 0.078))],
        'straw': (c(0.110, 0.100, 0.060), c(0.230, 0.200, 0.110)),
        'heads': 0,
    },
    # LANDE (H6). EZ1 : "pente subalpine : coniferes, landes", "rochers, eboulis : affleurements,
    # mousses, lichens". Plus petite, plus serree, plus seche que la prairie : un versant de
    # lande se lit par touffes isolees sur la roche, pas par nappe.
    {
        'name': 'SM_Grass_HeathTussock_01',
        'note': 'H6a touffe d eboulis : graminee dure et serree, 14-38 cm, 30 % de paille',
        # v1 : rayon 32, 150 lames -- ~0,5 m2 par touffe, ~12 % du versant couvert (mesure) :
        # le versant se lisait nu. Plus large et plus fournie, toujours plus serree qu'une prairie.
        'radius': 46.0, 'tufts': 4, 'blades': 230, 'spread': 8.0, 'fill': 0.15,
        'height': (16.0, 42.0), 'width': (0.5, 0.9), 'lean': (0.15, 0.60), 'bend': (0.40, 1.20),
        'segments': 3,
        'base': c(0.046, 0.052, 0.022),
        'tips': [(0.45, c(0.200, 0.185, 0.090)), (0.25, c(0.110, 0.140, 0.050)), (0.30, None)],
        'straw': (c(0.140, 0.120, 0.064), c(0.300, 0.255, 0.135)),
        'heads': 6, 'head_height': (30.0, 48.0), 'head_color': c(0.210, 0.180, 0.100),
    },
    {
        # Callune (Calluna vulgaris) : petit buisson raide brun-vert ; en fin d'ete, ses epis
        # mauves sont la seule couleur florale franche de la lande -- et la premiere de la carte.
        'name': 'SM_Grass_Heather_01',
        'note': 'H6b callune : rameaux raides 16-38 cm, brun-vert, epis mauves',
        'radius': 44.0, 'tufts': 5, 'blades': 230, 'spread': 9.0, 'fill': 0.3,
        'height': (16.0, 38.0), 'width': (0.35, 0.60), 'lean': (0.05, 0.35), 'bend': (0.05, 0.30),
        'segments': 3,
        'base': c(0.040, 0.030, 0.020),
        'tips': [(0.70, c(0.050, 0.075, 0.035)), (0.30, c(0.085, 0.065, 0.040))],
        'straw': (c(0.070, 0.055, 0.035), c(0.120, 0.095, 0.060)),
        # Mauve sourd, pas magenta : v1 a (0.200, 0.085, 0.170) sortait rose vif sous le soleil du banc.
        'heads': 60, 'head_height': (18.0, 36.0), 'head_color': c(0.150, 0.070, 0.135),
        'head_size': (6.0, 0.9),
    },
    # SOUS-BOIS (H5). EZ3 : "strate herbacee : fougeres, graminees, plantes vivaces, luzules,
    # asperules, anemones" ; palette "fougeres, scolopendre" ; "sous-bois sombre, ambiance humide".
    {
        # Fougere en volant (fougere male / aigle) : 6-9 frondes divisees qui montent puis
        # s'arquent. Pas une herbe : une tige (rachis) et des paires de folioles qui
        # raccourcissent vers la pointe. Fin d'ete : un quart des frondes bronze.
        'name': 'SM_Grass_Fern_01',
        'note': 'H5a fougere : 6-9 frondes de 50-95 cm, 14-18 paires de folioles, 25 % bronze',
        'kind': 'fern',
        'fronds': (6, 9), 'length': (50.0, 95.0), 'lift': (0.30, 0.60), 'droop': (1.10, 1.50),
        'pairs': (14, 18), 'pinna_len': (9.0, 16.0), 'pinna_w': (2.0, 2.8),
        'base': c(0.030, 0.058, 0.020),
        'tip': c(0.078, 0.140, 0.042),
        'bronze': (0.25, c(0.060, 0.050, 0.022), c(0.165, 0.112, 0.046)),
    },
    {
        # Scolopendre : rosette de lanieres entieres, larges, vert sombre luisant, qui
        # retombent. Les lames du pipeline d'herbe, mais larges et peu nombreuses.
        'name': 'SM_Grass_HartsTongue_01',
        'note': 'H5b scolopendre : 14 lanieres de 25-45 cm, 3,5-5,5 cm de large, vert sombre',
        'radius': 18.0, 'tufts': 1, 'blades': 14, 'spread': 3.0,
        'height': (25.0, 45.0), 'width': (3.5, 5.5), 'lean': (0.45, 0.85), 'bend': (0.50, 1.00),
        'segments': 5,
        'base': c(0.022, 0.048, 0.020),
        'tips': [(0.85, c(0.045, 0.098, 0.035)), (0.15, c(0.080, 0.090, 0.040))],
        'straw': (c(0.060, 0.055, 0.030), c(0.110, 0.090, 0.050)),
        'heads': 0,
    },
    {
        # Herbacee d'ombre : touffe basse de luzule, vert sombre, quelques etoiles blanches
        # d'anemone des bois (le petit blanc de la planche EZ3, "fougeres & herbacees").
        'name': 'SM_Grass_WoodHerb_01',
        'note': 'H5c herbacee d ombre : luzule 12-28 cm vert sombre, 10 etoiles blanches',
        'radius': 30.0, 'tufts': 4, 'blades': 110, 'spread': 7.0, 'fill': 0.2,
        'height': (12.0, 28.0), 'width': (0.8, 1.4), 'lean': (0.20, 0.70), 'bend': (0.40, 1.10),
        'segments': 3,
        'base': c(0.024, 0.044, 0.018),
        'tips': [(0.80, c(0.055, 0.098, 0.036)), (0.20, c(0.090, 0.100, 0.045))],
        'straw': (c(0.070, 0.060, 0.035), c(0.120, 0.100, 0.060)),
        'heads': 10, 'head_height': (14.0, 24.0), 'head_color': c(0.62, 0.62, 0.56),
        'head_size': (2.4, 1.3),
    },
]


class Buffers(object):
    def __init__(self):
        self.v, self.n, self.col, self.uv, self.t = [], [], [], [], []

    def vertex(self, p, normal, rgb, alpha, uv):
        self.v.append(unreal.Vector(p[0], p[1], p[2]))
        self.n.append(unreal.Vector(normal[0], normal[1], normal[2]))
        self.col.append(unreal.LinearColor(rgb[0], rgb[1], rgb[2], alpha))
        self.uv.append(unreal.Vector2D(uv[0], uv[1]))
        return len(self.v) - 1

    def tri(self, a, b, cc):
        self.t.append(unreal.IntVector(a, b, cc))

    def mesh(self):
        buffers = unreal.GeometryScriptSimpleMeshBuffers(
            vertices=self.v, normals=self.n, triangles=self.t, vertex_colors=self.col, uv0=self.uv)
        mesh, unused = unreal.GeometryScript_MeshEdits.append_buffers_to_mesh(
            unreal.DynamicMesh(), buffers, material_id=0)
        return mesh


def lerp3(a, b, t):
    return tuple(a[i] + (b[i] - a[i]) * t for i in range(3))


def scaled(rgb, k):
    return tuple(min(1.0, v * k) for v in rgb)


def up_normal(outward):
    ox, oy = outward
    n = (ox * (1.0 - UP_BLEND), oy * (1.0 - UP_BLEND), UP_BLEND)
    length = math.sqrt(sum(v * v for v in n))
    return tuple(v / length for v in n)


def blade(buf, base, yaw, height, width, lean, bend, segments, col_base, col_tip, outward):
    """Lame en arc : l'angle depuis la verticale croit de lean a lean+bend le long de la lame."""
    dx, dy = math.cos(yaw), math.sin(yaw)
    sx, sy = -dy, dx
    normal = up_normal(outward)
    step = height / segments
    p = [base[0], base[1], base[2]]
    left_right = []
    for s in range(segments + 1):
        t = s / float(segments)
        half = 0.5 * width * (1.0 - t) ** 0.6
        # L'exposant garde le pied sombre : la pointe claire ne descend pas jusqu'au sol.
        rgb = lerp3(col_base, col_tip, t ** 1.8)
        alpha = max(0.0, min(1.0, p[2] / 100.0))
        if s < segments:
            a = buf.vertex((p[0] - sx * half, p[1] - sy * half, p[2]), normal, rgb, alpha, (0.0, t))
            b = buf.vertex((p[0] + sx * half, p[1] + sy * half, p[2]), normal, rgb, alpha, (1.0, t))
            left_right.append((a, b))
        else:
            tip = buf.vertex(tuple(p), normal, rgb, alpha, (0.5, 1.0))
        angle = lean + bend * (s + 0.5) / segments
        p[0] += dx * math.sin(angle) * step
        p[1] += dy * math.sin(angle) * step
        p[2] += math.cos(angle) * step
    for s in range(segments - 1):
        a0, a1 = left_right[s]
        b0, b1 = left_right[s + 1]
        buf.tri(a0, b0, a1)
        buf.tri(a1, b0, b1)
    a0, a1 = left_right[-1]
    buf.tri(a0, tip, a1)


def seed_head(buf, base, yaw, height, lean, color_stem, color_head, outward, size=(9.0, 0.55)):
    """Tige fine puis epi en fuseau : ce qui fait lire une prairie haute a 20 m.

    size = (longueur, rayon) de l'epi : fin et long pour une graminee, court et renfle pour
    la grappe de la callune.
    """
    blade(buf, base, yaw, height, 0.6, lean, 0.15, 2, color_stem, color_stem, outward)
    angle = lean + 0.15
    top = (base[0] + math.cos(yaw) * math.sin(angle) * height,
           base[1] + math.sin(yaw) * math.sin(angle) * height,
           base[2] + math.cos(angle) * height)
    length, radius = size
    axis = (math.cos(yaw) * math.sin(angle), math.sin(yaw) * math.sin(angle), math.cos(angle))
    normal = up_normal(outward)
    alpha = max(0.0, min(1.0, top[2] / 100.0))
    bottom = buf.vertex(top, normal, color_head, alpha, (0.5, 0.0))
    tip_p = tuple(top[i] + axis[i] * length for i in range(3))
    tip = buf.vertex(tip_p, normal, color_head, min(1.0, tip_p[2] / 100.0), (0.5, 1.0))
    ring = []
    for k in range(4):
        a = yaw + k * math.pi / 2.0
        q = (top[0] + axis[0] * length * 0.45 + math.cos(a) * radius,
             top[1] + axis[1] * length * 0.45 + math.sin(a) * radius,
             top[2] + axis[2] * length * 0.45)
        ring.append(buf.vertex(q, normal, scaled(color_head, 1.08), alpha, (k / 4.0, 0.5)))
    for k in range(4):
        buf.tri(bottom, ring[k], ring[(k + 1) % 4])
        buf.tri(ring[k], tip, ring[(k + 1) % 4])


def draw_blades(spec, rng):
    """Tire toutes les lames UNE fois ; les LOD en prennent un sous-ensemble fixe."""
    tufts = []
    for k in range(spec['tufts']):
        r = spec['radius'] * 0.62 * math.sqrt(rng.random())
        a = rng.random() * math.tau
        tufts.append((r * math.cos(a), r * math.sin(a), rng.uniform(0.75, 1.15)))
    blades = []
    for k in range(spec['blades']):
        tx, ty, vigour = tufts[k % len(tufts)]
        if rng.random() < spec.get('fill', 0.0):
            # Remplissage : sans lui, la tache se lit comme quelques oursins sur sol nu.
            r, a = spec['radius'] * math.sqrt(rng.random()), rng.random() * math.tau
            tx, ty = 0.0, 0.0
            ox, oy = r * math.cos(a), r * math.sin(a)
        else:
            ox, oy = rng.gauss(0.0, spec['spread']), rng.gauss(0.0, spec['spread'])
        x, y = tx + ox, ty + oy
        d = math.hypot(x, y)
        if d > spec['radius']:
            x, y = x * spec['radius'] / d, y * spec['radius'] / d
        splay = math.hypot(ox, oy) / (2.5 * spec['spread'])
        yaw = math.atan2(oy, ox) + rng.gauss(0.0, 0.6)
        h = rng.uniform(*spec['height']) * vigour
        lean = rng.uniform(*spec['lean']) + 0.35 * min(1.0, splay)
        bend = rng.uniform(*spec['bend'])
        roll = rng.random()
        tip, acc = None, 0.0
        for weight, colour in spec['tips']:
            acc += weight
            if roll <= acc:
                tip = colour
                break
        if tip is None and spec['tips'][-1][1] is None:
            base, tip = spec['straw']
        else:
            base = spec['base']
            tip = tip or spec['tips'][0][1]
        k_tint = rng.uniform(0.85, 1.15)
        out = (x / d, y / d) if d > 1e-3 else (math.cos(yaw), math.sin(yaw))
        blades.append({'base': (x, y, -6.0), 'yaw': yaw, 'height': h, 'width': rng.uniform(*spec['width']),
                       'lean': lean, 'bend': bend, 'col_base': scaled(base, k_tint), 'col_tip': scaled(tip, k_tint),
                       'outward': out, 'keep': rng.random()})
    heads = []
    for k in range(spec.get('heads', 0)):
        tx, ty, vigour = tufts[rng.randrange(len(tufts))]
        x, y = tx + rng.gauss(0.0, spec['spread'] * 0.6), ty + rng.gauss(0.0, spec['spread'] * 0.6)
        heads.append({'base': (x, y, -6.0), 'yaw': rng.random() * math.tau,
                      'height': rng.uniform(*spec['head_height']), 'lean': rng.uniform(0.04, 0.22),
                      'keep': rng.random()})
    return blades, heads


def draw_fern(spec, rng):
    """Tire les frondes UNE fois ; les LOD en simplifient le dessin, pas la disposition."""
    n = rng.randint(*spec['fronds'])
    fronds = []
    for k in range(n):
        yaw = (k + rng.uniform(-0.3, 0.3)) * math.tau / n
        bronze = rng.random() < spec['bronze'][0]
        tint = rng.uniform(0.88, 1.12)
        base, tip = (spec['bronze'][1], spec['bronze'][2]) if bronze else (spec['base'], spec['tip'])
        fronds.append({'yaw': yaw, 'length': rng.uniform(*spec['length']), 'lift': rng.uniform(*spec['lift']),
                       'droop': rng.uniform(*spec['droop']), 'pairs': rng.randint(*spec['pairs']),
                       'pinna_len': rng.uniform(*spec['pinna_len']), 'pinna_w': rng.uniform(*spec['pinna_w']),
                       'col_base': scaled(base, tint), 'col_tip': scaled(tip, tint),
                       'origin': (rng.gauss(0.0, 2.0), rng.gauss(0.0, 2.0), -3.0)})
    return fronds


def fern_frond(buf, f, lod):
    """Rachis en arc, puis paires de folioles posees presque a plat, plus courtes vers la pointe.

    LOD0 : toutes les paires, folioles a 2 segments. LOD1 : une paire sur deux, elargies, 1 segment.
    LOD2 : la fronde entiere devient une seule lame large, de la largeur de ses folioles.
    """
    yaw, length = f['yaw'], f['length']
    dx, dy = math.cos(yaw), math.sin(yaw)
    out = (dx, dy)
    if lod == 2:
        blade(buf, f['origin'], yaw, length, 1.6 * f['pinna_len'], f['lift'] + 0.2, f['droop'] - f['lift'],
              2, f['col_base'], f['col_tip'], out)
        return
    nodes = f['pairs'] + 3
    step = length / nodes
    p = list(f['origin'])
    pts = []
    for i in range(nodes + 1):
        t = i / float(nodes)
        pts.append((tuple(p), t))
        ang = f['lift'] + (f['droop'] - f['lift']) * t ** 1.5
        p[0] += dx * math.sin(ang) * step
        p[1] += dy * math.sin(ang) * step
        p[2] += math.cos(ang) * step
    # Rachis : un ruban fin le long de l'arc (stipe nu sur les trois premiers noeuds).
    sx, sy = -dy, dx
    normal = up_normal(out)
    prev = None
    for (q, t) in pts:
        half = 0.35 * (1.0 - 0.7 * t)
        rgb = lerp3(f['col_base'], f['col_tip'], t)
        alpha = max(0.0, min(1.0, q[2] / 100.0))
        a = buf.vertex((q[0] - sx * half, q[1] - sy * half, q[2]), normal, rgb, alpha, (0.0, t))
        b = buf.vertex((q[0] + sx * half, q[1] + sy * half, q[2]), normal, rgb, alpha, (1.0, t))
        if prev:
            buf.tri(prev[0], a, prev[1])
            buf.tri(prev[1], a, b)
        prev = (a, b)
    stride = 1 if lod == 0 else 2
    widen = 1.0 if lod == 0 else 1.7
    segments = 2 if lod == 0 else 1
    for i in range(3, nodes, stride):
        t = (i - 3) / float(max(1, nodes - 4))
        # Fronde triangulaire : folioles longues a la base, courtes a la pointe.
        plen = f['pinna_len'] * (1.0 - 0.85 * t) + 1.0
        pw = f['pinna_w'] * (1.0 - 0.5 * t) * widen
        rgb_b = lerp3(f['col_base'], f['col_tip'], 0.4 + 0.6 * t)
        for side in (-1, 1):
            blade(buf, pts[i][0], yaw + side * 1.25, plen, pw, 1.30, 0.25, segments, rgb_b, f['col_tip'], out)


def build_fern_lod(spec, fronds, lod):
    buf = Buffers()
    for f in fronds:
        fern_frond(buf, f, lod)
    if not buf.t:
        raise RuntimeError('%s LOD%d: aucun triangle' % (spec['name'], lod))
    return buf.mesh(), len(buf.t)


def build_lod(spec, blades, heads, lod):
    keep = (1.0, 0.45, 0.30)[lod]
    # LOD2 ne double plus la largeur : un triangle deux fois plus gros remplissait encore l'oeil.
    widen = (1.0, 1.15, 1.35)[lod]
    segments = max(1, spec['segments'] - (0, 1, spec['segments'])[lod])
    buf = Buffers()
    for b in blades:
        if b['keep'] >= keep:
            continue
        blade(buf, b['base'], b['yaw'], b['height'], b['width'] * widen, b['lean'], b['bend'],
              segments, b['col_base'], b['col_tip'], b['outward'])
    if lod < 2:
        stem = lerp3(spec['base'], spec['head_color'], 0.6) if heads else None
        for h in heads:
            if h['keep'] >= (1.0, 0.6)[lod]:
                continue
            seed_head(buf, h['base'], h['yaw'], h['height'], h['lean'], stem, spec['head_color'],
                      (math.cos(h['yaw']), math.sin(h['yaw'])), spec.get('head_size', (9.0, 0.55)))
    if not buf.t:
        raise RuntimeError('%s LOD%d: aucun triangle' % (spec['name'], lod))
    return buf.mesh(), len(buf.t)


def save_static_mesh(meshes, asset_path, material):
    if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        unreal.EditorAssetLibrary.delete_asset(asset_path)
    options = unreal.GeometryScriptCreateNewStaticMeshAssetOptions()
    # A True, le build jetterait les normales tirees vers le ciel.
    options.set_editor_property('enable_recompute_normals', False)
    options.set_editor_property('enable_recompute_tangents', True)
    options.set_editor_property('enable_nanite', False)
    try:
        # On traverse une prairie : aucune collision, ni sur l'asset ni sur le HISM.
        options.set_editor_property('enable_collision', False)
    except Exception as exc:  # noqa: BLE001
        log('WARN enable_collision absent de ce build : %s' % exc)
    asset, outcome = unreal.GeometryScript_NewAssetUtils.create_new_static_mesh_asset_from_mesh(
        meshes[0], asset_path, options)
    if asset is None:
        raise RuntimeError('%s: create_new_static_mesh_asset_from_mesh a rendu None (%s)' % (asset_path, outcome))
    asset.set_material(0, material)
    slots = len(asset.get_editor_property('static_materials'))
    if slots != 1:
        raise RuntimeError('%s: %d slots de materiau au lieu de 1' % (asset_path, slots))

    reductions = unreal.StaticMeshReductionOptions()
    reductions.auto_compute_lod_screen_size = False
    reductions.reduction_settings = [unreal.StaticMeshReductionSettings(percent_triangles=1.0, screen_size=s)
                                     for s in LOD_SCREEN]
    subsystem = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    if subsystem is None:
        raise RuntimeError('LOD: StaticMeshEditorSubsystem absent (editeur vivant requis)')
    if subsystem.set_lods(asset, reductions) != 3:
        raise RuntimeError('%s: 3 LOD attendus' % asset_path)
    copy_options = unreal.GeometryScriptCopyMeshToAssetOptions(enable_recompute_tangents=True)
    try:
        copy_options.set_editor_property('enable_recompute_normals', False)
    except Exception as exc:  # noqa: BLE001
        log('WARN enable_recompute_normals absent des options de copie : %s' % exc)
    for lod in (1, 2):
        unused, outcome = unreal.GeometryScript_AssetUtils.copy_mesh_to_static_mesh(
            meshes[lod], asset, copy_options, unreal.GeometryScriptMeshWriteLOD(lod_index=lod))
        if outcome != unreal.GeometryScriptOutcomePins.SUCCESS:
            raise RuntimeError('%s: copie LOD%d refusee (%s)' % (asset_path, lod, outcome))
        settings = subsystem.get_lod_reduction_settings(asset, lod)
        settings.percent_triangles = 1.0
        settings.percent_vertices = 1.0
        settings.base_lod_model = lod
        subsystem.set_lod_reduction_settings(asset, lod, settings)
    triangles = [asset.get_num_triangles(i) for i in range(asset.get_num_lods())]
    unreal.EditorAssetLibrary.save_asset(asset.get_path_name())
    bounds = asset.get_bounding_box()
    log('SAVED %s lods=%d triangles=%s bounds=[%.1f %.1f %.1f]..[%.1f %.1f %.1f]'
        % (asset_path, asset.get_num_lods(), triangles, bounds.min.x, bounds.min.y, bounds.min.z,
           bounds.max.x, bounds.max.y, bounds.max.z))
    return triangles


WIND_CODE = '''
float fade = saturate((distance(Cam, Obj) - FadeStart) / max(FadeEnd - FadeStart, 1.0));
float phase = T * %(speed).2f + dot(Position.xy, float2(0.0021, 0.0013)) + Rand * 1.2;
float gust = sin(phase) * 0.7 + sin(phase * 2.3 + 1.7) * 0.3;
float k = Height * Height * (1.0 - fade);
float3 wind = float3(gust, gust * 0.45, 0.0) * %(amp).1f * k;
float2 d = normalize(Weather.xy + float2(0.00001,0));
float front = dot(Position.xy,d)*0.0012 - T*0.65;
float packet = 0.65 + 0.25*sin(front) + 0.10*sin(front*1.71 + 1.2);
float flutter = 0.7 + 0.2*sin(T*2.8 + Rand*6.283) + 0.1*sin(T*5.1 + Rand*9);
float3 coherent = float3(d,0) * 5.0 * saturate(Weather.z) * packet * flutter * k;
return lerp(wind, coherent, Enabled) + (Obj - Position) * fade;
''' % {'speed': WIND_SPEED, 'amp': WIND_AMPLITUDE}


def ensure_material():
    """Deux faces, feuillage : la lumiere traverse la lame a contre-jour (EZ5, clairiere).

    WPO = vent (hauteur^2, dephase par instance et par position : des vagues, pas un unisson)
    + fondu de distance (la touffe s'enfonce vers son pivot entre FADE_START et FADE_END).
    """
    mel = unreal.MaterialEditingLibrary
    mat = unreal.load_asset(MATERIAL_PATH)
    if mat:
        # Rewire in place: preserve every existing mesh material reference.
        mel.delete_all_material_expressions(mat)
    else:
        mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            MATERIAL_NAME, MATERIAL_DIR, unreal.Material, unreal.MaterialFactoryNew())

    vc = mel.create_material_expression(mat, unreal.MaterialExpressionVertexColor, -900, 0)
    base_out = None
    for out_name in ('', 'RGB', 'Color'):
        if mel.connect_material_property(vc, out_name, unreal.MaterialProperty.MP_BASE_COLOR):
            base_out = out_name
            break
    if base_out is None:
        raise RuntimeError('BaseColor non cable')

    warm = mel.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector, -900, 220)
    warm.set_editor_property('constant', unreal.LinearColor(TRANSMISSION_WARMTH[0], TRANSMISSION_WARMTH[1],
                                                            TRANSMISSION_WARMTH[2], 1.0))
    sss = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -600, 140)
    mel.connect_material_expressions(vc, base_out, sss, 'A')
    mel.connect_material_expressions(warm, '', sss, 'B')
    if not mel.connect_material_property(sss, '', unreal.MaterialProperty.MP_SUBSURFACE_COLOR):
        raise RuntimeError('Subsurface non cable')

    for value, prop, y in ((0.9, unreal.MaterialProperty.MP_ROUGHNESS, 300),
                           (0.2, unreal.MaterialProperty.MP_SPECULAR, 380)):
        k = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -350, y)
        k.set_editor_property('r', value)
        if not mel.connect_material_property(k, '', prop):
            raise RuntimeError('%s non cable' % prop)

    # NORMALE. Les normales du mesh sont tirees vers le ciel ; mais un materiau deux faces
    # RETOURNE la normale sur la face arriere, qui pointe alors vers le sol : la moitie des
    # lames rendait noire (v1). Normale monde x TwoSidedSign annule ce retournement.
    vnormal = mel.create_material_expression(mat, unreal.MaterialExpressionVertexNormalWS, -900, 420)
    sign = mel.create_material_expression(mat, unreal.MaterialExpressionTwoSidedSign, -900, 470)
    facing = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -600, 440)
    mel.connect_material_expressions(vnormal, '', facing, 'A')
    mel.connect_material_expressions(sign, '', facing, 'B')
    if not mel.connect_material_property(facing, '', unreal.MaterialProperty.MP_NORMAL):
        raise RuntimeError('Normale non cablee')
    mat.set_editor_property('tangent_space_normal', False)

    wind = mel.create_material_expression(mat, unreal.MaterialExpressionCustom, -350, 600)
    wind.set_editor_property('description', 'Grass wind and distance fade')
    wind.set_editor_property('code', WIND_CODE)
    wind.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    sources = (('Position', unreal.MaterialExpressionWorldPosition, ''),
               ('Obj', unreal.MaterialExpressionObjectPositionWS, ''),
               ('Cam', unreal.MaterialExpressionCameraPositionWS, ''),
               ('T', unreal.MaterialExpressionTime, ''),
               ('Rand', unreal.MaterialExpressionPerInstanceRandom, ''),
               ('Height', None, 'A'),
               ('FadeStart', 'FadeStart', FADE_START),
               ('FadeEnd', 'FadeEnd', FADE_END),
               ('Weather', 'MPC:WeatherWind', ''),
               ('Enabled', 'MPC:WeatherCoupling', ''))
    inputs = []
    for name, cls, out in sources:
        entry = unreal.CustomInput()
        entry.set_editor_property('input_name', name)
        inputs.append(entry)
    wind.set_editor_property('inputs', inputs)
    for i, (name, cls, out) in enumerate(sources):
        if isinstance(cls, str) and cls.startswith('MPC:'):
            node = weather_materials.parameter(mat, cls[4:])
            out = ''
        elif isinstance(cls, str):
            # Parametre scalaire : le MID de chaque HISM le surcharge.
            node = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -800, 520 + 70 * i)
            node.set_editor_property('parameter_name', cls)
            node.set_editor_property('default_value', out)
            out = ''
        else:
            node = vc if cls is None else mel.create_material_expression(mat, cls, -800, 520 + 70 * i)
        if not mel.connect_material_expressions(node, out, wind, name):
            raise RuntimeError('Entree du vent non cablee : ' + name)
    if not mel.connect_material_property(wind, '', unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET):
        raise RuntimeError('WPO non cable')

    mat.set_editor_property('two_sided', True)
    mat.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_TWO_SIDED_FOLIAGE)
    mat.set_editor_property('used_with_instanced_static_meshes', True)
    mel.recompile_material(mat)
    unreal.EditorAssetLibrary.save_asset(MATERIAL_PATH)
    log('MATERIAL %s shading=%s two_sided=%s ism=%s' % (
        MATERIAL_PATH, mat.get_editor_property('shading_model'), mat.get_editor_property('two_sided'),
        mat.get_editor_property('used_with_instanced_static_meshes')))
    return mat


def main():
    log('start')
    if unreal.get_editor_subsystem(unreal.AssetEditorSubsystem) is None:
        raise RuntimeError('editeur vivant requis ; aucun asset modifie')
    # Les touffes d'abord, le materiau ensuite : supprimer M_AnastasisGrass pendant que les
    # touffes le referencent laisse un paquet que ForceDeleteObjects ne sait plus decharger
    # (ensure ObjectTools.cpp:4045 au deuxieme run, v2).
    for spec in FAMILIES:
        path = PACKAGE_PATH + '/' + spec['name']
        if unreal.EditorAssetLibrary.does_asset_exist(path):
            unreal.EditorAssetLibrary.delete_asset(path)
    material = ensure_material()
    report = []
    for spec in FAMILIES:
        rng = random.Random('%d/%s' % (SEED, spec['name']))
        if spec.get('kind') == 'fern':
            fronds = draw_fern(spec, rng)
            built = [build_fern_lod(spec, fronds, lod) for lod in range(3)]
        else:
            blades, heads = draw_blades(spec, rng)
            built = [build_lod(spec, blades, heads, lod) for lod in range(3)]
        triangles = save_static_mesh([m for m, n in built], PACKAGE_PATH + '/' + spec['name'], material)
        report.append('%s=%s' % (spec['name'], triangles))
        log('  %s -- %s' % (spec['name'], spec['note']))
    log('GROUND_COVER_ASSETS::PASS %s' % ' '.join(report))


if __name__ == '__main__':
    try:
        main()
    except Exception as exc:  # noqa: BLE001
        unreal.log_error('GROUND_COVER_ASSETS::FAIL %s' % exc)
    finally:
        if os.environ.get('ANASTASIS_GROUND_COVER_QUIT') == '1':
            unreal.SystemLibrary.quit_editor()
