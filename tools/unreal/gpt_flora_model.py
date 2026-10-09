"""GPT_FLORA_002 -- les treize sujets de la planche GPT modelises en vraie geometrie, sans une seule texture.

Pas de `import unreal` ici : ce module sert deux appelants.
  * `gpt-flora-preview.py` (hors editeur) : rend le maillage en logiciel, face / profil / trois-quarts ;
  * `create-gpt-flora.py` (editeur) : ecrit les StaticMesh.
Le meme code, la meme graine : ce que le banc regarde est ce que l'editeur construit.

CE QUE C'EST. Chaque sujet est BATI : un fut mesure sur le sprite, des branches ramifiees qui poussent par
colonisation d'espace vers un nuage de points tire DANS le volume que dessine la silhouette (le houppier est
de revolution : la face cachee est une hypothese, pas une mesure), des rameaux a section decroissante (loi du
tuyau), et au bout de chaque rameau de VRAIES feuilles (maillages plies), aiguilles en pinceau, fleurs, baies.
La fougere est une rachis et des pennes, la prairie des brins et des fleurs. Les couleurs sont celles MESUREES
sur le sprite (gpt-flora-fit.py) ; l'image n'est jamais collee.

CE QUE CE N'EST PAS. Ce n'est pas la feuille du dessin : une feuille ici est un losange plie de quatre
triangles, pas le contour exact de la feuille de l'image. Le bois est une tuyauterie a cinq pans.

ESPACE. Dessin normalise de create_tree_asset.py : l'arbre tient dans Z = [-50, +50], pied en (0, 0, -50), X et
Y a la meme echelle. Deux lots de triangles par LOD : `wood` (alpha de sommet 0, slot 1 : M_AnastasisBark) et
`leaf` (alpha 1, slot 0 : M_AnastasisVegetation, deux faces, lumiere qui traverse).
"""
import math
import random

TAU = math.tau
UP = (0.0, 0.0, 1.0)


# ------------------------------------------------------------------------------------------------ vecteurs
def add(a, b): return (a[0] + b[0], a[1] + b[1], a[2] + b[2])
def sub(a, b): return (a[0] - b[0], a[1] - b[1], a[2] - b[2])
def mul(a, k): return (a[0] * k, a[1] * k, a[2] * k)
def dot(a, b): return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]
def cross(a, b): return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])
def length(a): return math.sqrt(dot(a, a))
def lerp(a, b, t): return (a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t, a[2] + (b[2] - a[2]) * t)


def norm(a):
    n = length(a)
    return (a[0] / n, a[1] / n, a[2] / n) if n > 1e-9 else UP


def perp(t):
    ref = UP if abs(t[2]) < 0.9 else (1.0, 0.0, 0.0)
    return norm(cross(t, ref))


def rand_unit(rng):
    while True:
        v = (rng.uniform(-1, 1), rng.uniform(-1, 1), rng.uniform(-1, 1))
        n = length(v)
        if 0.05 < n <= 1.0:
            return mul(v, 1.0 / n)


def tint(col, k):
    return (min(1.0, col[0] * k), min(1.0, col[1] * k), min(1.0, col[2] * k))


def vary(col, rng, amount=0.18, hue=0.06):
    k = 1.0 + rng.uniform(-amount, amount)
    return (min(1.0, col[0] * k * (1.0 + rng.uniform(-hue, hue))), min(1.0, col[1] * k),
            min(1.0, col[2] * k * (1.0 + rng.uniform(-hue, hue))))


# ----------------------------------------------------------------------------------------------- un lot
class Batch(object):
    """Sommets (position, couleur RGB) et triangles d'un slot de materiau."""

    def __init__(self, alpha):
        self.alpha = alpha
        self.v, self.c, self.t = [], [], []

    def vert(self, p, col):
        self.v.append(p)
        self.c.append(col)
        return len(self.v) - 1

    def tri(self, a, b, c):
        self.t.append((a, b, c))

    def tri_out(self, a, b, c, outward):
        """Triangle dont la normale regarde du cote de `outward`."""
        pa, pb, pc = self.v[a], self.v[b], self.v[c]
        n = cross(sub(pb, pa), sub(pc, pa))
        if dot(n, outward) < 0.0:
            b, c = c, b
        self.t.append((a, b, c))

    def count(self):
        return len(self.t)


# ------------------------------------------------------------------------------------------------- bois
def tube(mb, pts, rads, sides, col, tip=True):
    """Tuyau a section decroissante le long d'une polyligne, repere a transport parallele (pas de vrille)."""
    n = len(pts)
    if n < 2:
        return
    T = [norm(sub(pts[min(i + 1, n - 1)], pts[max(i - 1, 0)])) for i in range(n)]
    N = perp(T[0])
    rings = []
    for i in range(n):
        if i > 0:
            N = sub(N, mul(T[i], dot(N, T[i])))
            N = norm(N) if length(N) > 1e-6 else perp(T[i])
        B = cross(T[i], N)
        c = col(i / float(n - 1)) if callable(col) else col
        ring = []
        for k in range(sides):
            a = TAU * k / sides
            off = add(mul(N, math.cos(a) * rads[i]), mul(B, math.sin(a) * rads[i]))
            ring.append((mb.vert(add(pts[i], off), c), off))
        rings.append(ring)
    for i in range(n - 1):
        for k in range(sides):
            a0, o = rings[i][k]
            a1, _ = rings[i][(k + 1) % sides]
            b0, _ = rings[i + 1][k]
            b1, _ = rings[i + 1][(k + 1) % sides]
            mb.tri_out(a0, a1, b1, o)
            mb.tri_out(a0, b1, b0, o)
    if tip and rads[-1] > 0.04:
        c = col(1.0) if callable(col) else col
        apex = mb.vert(add(pts[-1], mul(T[-1], rads[-1] * 1.4)), c)
        for k in range(sides):
            a0, o = rings[-1][k]
            a1, _ = rings[-1][(k + 1) % sides]
            mb.tri_out(a0, a1, apex, o)


# ---------------------------------------------------------------------------- colonisation d'espace
def grow(nodes, attractors, step, influence, kill, tropism, rng, max_nodes, max_iter=140):
    """Colonisation d'espace (Runions et al.) : `nodes` = [(pos, parent)] est etendu en place.

    Chaque point d'attraction tire le noeud le plus proche situe dans `influence` ; un noeud tire pousse une
    branche d'un `step` dans la moyenne de ses tirages (plus `tropism`) ; un point a moins de `kill` d'un noeud est
    consomme. Deterministe : l'ordre des insertions fixe tout."""
    cell = influence
    grid = {}

    def key(p):
        return (int(math.floor(p[0] / cell)), int(math.floor(p[1] / cell)), int(math.floor(p[2] / cell)))

    for i, (p, _par) in enumerate(nodes):
        grid.setdefault(key(p), []).append(i)
    alive = list(attractors)
    for _ in range(max_iter):
        if not alive or len(nodes) >= max_nodes:
            break
        pull = {}
        for a in alive:
            k = key(a)
            best, bd = -1, influence * influence
            for dx in (-1, 0, 1):
                for dy in (-1, 0, 1):
                    for dz in (-1, 0, 1):
                        for i in grid.get((k[0] + dx, k[1] + dy, k[2] + dz), ()):
                            d = sub(a, nodes[i][0])
                            dd = dot(d, d)
                            if dd < bd:
                                best, bd = i, dd
            if best >= 0:
                d = norm(sub(a, nodes[best][0]))
                s = pull.get(best)
                pull[best] = (add(s[0], d), s[1] + 1) if s else (d, 1)
        if not pull:
            break
        for i, (s, cnt) in pull.items():
            d = norm(add(mul(s, 1.0 / cnt), tropism))
            p = add(nodes[i][0], mul(d, step))
            # pas de noeud jumeau : un noeud a moins de 0,45 pas d'un voisin n'ajoute rien
            kk = key(p)
            twin = False
            for dx in (-1, 0, 1):
                for dy in (-1, 0, 1):
                    for dz in (-1, 0, 1):
                        for j in grid.get((kk[0] + dx, kk[1] + dy, kk[2] + dz), ()):
                            if length(sub(nodes[j][0], p)) < step * 0.45:
                                twin = True
            if twin:
                continue
            nodes.append((p, i))
            grid.setdefault(kk, []).append(len(nodes) - 1)
            if len(nodes) >= max_nodes:
                break
        kill2 = kill * kill
        keep = []
        for a in alive:
            k = key(a)
            dead = False
            for dx in (-1, 0, 1):
                for dy in (-1, 0, 1):
                    for dz in (-1, 0, 1):
                        for i in grid.get((k[0] + dx, k[1] + dy, k[2] + dz), ()):
                            d = sub(a, nodes[i][0])
                            if dot(d, d) < kill2:
                                dead = True
            if not dead:
                keep.append(a)
        if len(keep) == len(alive) and not pull:
            break
        alive = keep
    return nodes


def pipe_radii(nodes, r_tip, exponent=2.3):
    """Loi du tuyau : r_parent^n = somme des r_enfant^n ; les extremites valent r_tip."""
    n = len(nodes)
    acc = [0.0] * n
    rad = [0.0] * n
    kids = [0] * n
    for i in range(n - 1, -1, -1):
        par = nodes[i][1]
        r = r_tip if kids[i] == 0 else max(r_tip, acc[i] ** (1.0 / exponent))
        rad[i] = r
        if par >= 0:
            acc[par] += r ** exponent
            kids[par] += 1
    return rad, kids


def branch_paths(nodes, rad, kids):
    """Decoupe l'arbre en polylignes : on suit l'enfant le plus gros, les autres ouvrent un nouveau chemin."""
    children = [[] for _ in nodes]
    for i, (_p, par) in enumerate(nodes):
        if par >= 0:
            children[par].append(i)
    paths = []
    starts = [(i, None) for i, (_p, par) in enumerate(nodes) if par < 0]
    stack = list(starts)
    while stack:
        i, from_parent = stack.pop()
        path = [from_parent] if from_parent is not None else []
        cur = i
        while True:
            path.append(cur)
            ch = children[cur]
            if not ch:
                break
            ch.sort(key=lambda c: -rad[c])
            for other in ch[1:]:
                stack.append((other, cur))
            cur = ch[0]
        paths.append(path)
    return paths


# ---------------------------------------------------------------------------------------------- feuilles
def _frame(direction, up_hint, rng, up_bias=0.5):
    x = norm(direction)
    s = cross(x, up_hint)
    s = norm(s) if length(s) > 1e-4 else perp(x)
    z = norm(cross(s, x))
    if z[2] < 0.0:
        s = mul(s, -1.0)
        z = mul(z, -1.0)
    roll = rng.uniform(-0.7, 0.7)
    cs, sn = math.cos(roll), math.sin(roll)
    s2 = add(mul(s, cs), mul(z, sn))
    z2 = add(mul(z, cs), mul(s, -sn))
    return x, s2, z2


KITES = {
    # (abscisse de la plus grande largeur, demi-largeur, relevage des bords : le V de la nervure)
    'diamond': (.38, .50, .16),
    'round': (.42, .56, .18),
    'lanceolate': (.45, .17, .08),
    'oblong': (.50, .30, .12),
}
SHAPES = {
    # (x le long de la nervure 0..1, y en travers, z pli) ; M = point de nervure releve ; triangles en eventail
    'lobed': ([(0, 0, 0), (.30, -.40, 0), (.62, -.34, 0), (1, 0, 0), (.62, .34, 0), (.30, .40, 0)], (.5, 0, .12)),
}


def leaf(mb, origin, x, s, z, size, shape, col, col_tip=None):
    if shape in KITES:
        px, hw, lift = KITES[shape]
        ct = col if col_tip is None else col_tip
        b = mb.vert(origin, tint(col, .72))
        l = mb.vert(add(origin, add(add(mul(x, px * size), mul(s, -hw * size)), mul(z, lift * size))), col)
        t = mb.vert(add(origin, mul(x, size)), ct)
        r = mb.vert(add(origin, add(add(mul(x, px * size), mul(s, hw * size)), mul(z, lift * size))), col)
        mb.tri(b, l, t)
        mb.tri(b, t, r)
        return
    pts, mid = SHAPES[shape]
    ids = []
    for (lx, ly, lz) in pts:
        p = add(origin, add(add(mul(x, lx * size), mul(s, ly * size)), mul(z, lz * size)))
        t = lx
        ids.append(mb.vert(p, col if col_tip is None else lerp(col, col_tip, t)))
    m = mb.vert(add(origin, add(add(mul(x, mid[0] * size), mul(s, mid[1] * size)), mul(z, mid[2] * size))), col)
    for i in range(len(ids)):
        mb.tri(ids[i], ids[(i + 1) % len(ids)], m)


def leaflets(mb, origin, x, s, z, size, col, rng, count=5):
    """Feuille palmee (marronnier) : un eventail de folioles lancees."""
    for k in range(count):
        ang = math.radians((k - (count - 1) / 2.0) * 34.0)
        d = add(mul(x, math.cos(ang)), mul(s, math.sin(ang)))
        l = size * (0.62 + 0.38 * math.cos(ang * 1.4))
        # foliole = cerf-volant plie a quatre sommets
        side = norm(cross(d, z))
        pb = origin
        pm = add(origin, add(mul(d, l * .42), mul(z, l * .05)))
        pl = add(pm, mul(side, -l * .20))
        pr = add(pm, mul(side, l * .20))
        pt = add(origin, add(mul(d, l), mul(z, -l * .12)))
        c = vary(col, rng, .12)
        a, b, cc, dd = mb.vert(pb, c), mb.vert(pl, c), mb.vert(pt, tint(c, 1.15)), mb.vert(pr, c)
        mb.tri(a, b, cc)
        mb.tri(a, cc, dd)


def tuft(mb, origin, direction, size, count, spread_deg, col, rng, base_w=0.10):
    """Pinceau d'aiguilles : `count` triangles minces en eventail autour de `direction`."""
    d0 = norm(direction)
    u = perp(d0)
    w = cross(d0, u)
    sp = math.radians(spread_deg)
    for i in range(count):
        phi = TAU * (i + rng.uniform(-.25, .25)) / count
        d = norm(add(mul(d0, math.cos(sp * rng.uniform(.7, 1.1))), mul(add(mul(u, math.cos(phi)), mul(w, math.sin(phi))), math.sin(sp))))
        l = size * rng.uniform(.82, 1.18)
        side = norm(cross(d, UP if abs(d[2]) < .9 else (1, 0, 0)))
        cb = vary(col, rng, .14)
        a = mb.vert(add(origin, mul(side, size * base_w)), tint(cb, .62))
        b = mb.vert(add(origin, mul(side, -size * base_w)), tint(cb, .62))
        t = mb.vert(add(origin, mul(d, l)), tint(cb, 1.28))
        mb.tri(a, b, t)


def blob(mb, c, r, col, rng, flat=1.0):
    """Octaedre : baie, bouton."""
    pts = [add(c, (r, 0, 0)), add(c, (-r, 0, 0)), add(c, (0, r, 0)), add(c, (0, -r, 0)), add(c, (0, 0, r * flat)), add(c, (0, 0, -r * flat))]
    ids = [mb.vert(p, col) for p in pts]
    for a, b, tpl in ((0, 2, 4), (2, 1, 4), (1, 3, 4), (3, 0, 4), (2, 0, 5), (1, 2, 5), (3, 1, 5), (0, 3, 5)):
        mb.tri_out(ids[a], ids[b], ids[tpl], sub(mb.v[ids[a]], c))


def cone(mb, base, axis, radius, height, sides, col, col_tip=None):
    axis = norm(axis)
    u = perp(axis)
    w = cross(axis, u)
    apex = mb.vert(add(base, mul(axis, height)), col_tip or col)
    ring = []
    for k in range(sides):
        a = TAU * k / sides
        off = add(mul(u, math.cos(a) * radius), mul(w, math.sin(a) * radius))
        ring.append((mb.vert(add(base, off), col), off))
    for k in range(sides):
        a0, o = ring[k]
        a1, _ = ring[(k + 1) % sides]
        mb.tri_out(a0, a1, apex, o)


def flower_star(mb, c, normal, radius, petals, col, col_centre, rng):
    """Fleur ouverte : une corolle de `petals` triangles, bord relevee, un coeur."""
    n = norm(normal)
    u = perp(n)
    w = cross(n, u)
    centre = mb.vert(c, col_centre)
    rim = []
    for k in range(petals):
        a = TAU * (k + rng.uniform(-.12, .12)) / petals
        rim.append(mb.vert(add(c, add(add(mul(u, math.cos(a) * radius), mul(w, math.sin(a) * radius)), mul(n, radius * .22))),
                           vary(col, rng, .08)))
    for k in range(len(rim)):
        mb.tri(centre, rim[k], rim[(k + 1) % len(rim)])


# ----------------------------------------------------------------------------------------- volume du sprite
def crown_sampler(spec, shell_lo, rng, fill_floor=0.0):
    """Tire des points dans la couronne de revolution des tranches du sprite (anneau [shell_lo, 1] du rayon)."""
    bands = [b for b in spec['bands'] if (b[6] if len(b) > 6 else 1.0) >= fill_floor]
    weights = []
    for b in bands:
        zc, h, cx, cy, hw, hd = b[:6]
        fill = b[6] if len(b) > 6 else 1.0
        weights.append(max(1e-6, math.pi * hw * hd * h * max(fill, .25)))
    total = sum(weights)

    def sample():
        r = rng.uniform(0, total)
        for b, w in zip(bands, weights):
            r -= w
            if r <= 0:
                break
        zc, h, cx, cy, hw, hd = b[:6]
        rho = math.sqrt(shell_lo * shell_lo + rng.random() * (1.0 - shell_lo * shell_lo))
        th = rng.uniform(0, TAU)
        return (cx + rho * hw * math.cos(th), cy + rho * hd * math.sin(th), zc + rng.uniform(-.5, .5) * h)

    return sample


def shell_area(spec):
    a = 0.0
    for b in spec['bands']:
        zc, h, cx, cy, hw, hd = b[:6]
        fill = b[6] if len(b) > 6 else 1.0
        a += TAU * math.sqrt((hw * hw + hd * hd) / 2.0) * h * max(fill, .3)
    return a


# ------------------------------------------------------------------------------------------------ recettes
# kind : tree | shrub | willow | column | fern | meadow.   leaves = nombre de feuilles (ou de pinceaux) au LOD0.
RECIPES = {
    'marronnier_fleuri': dict(kind='tree', leaf='palmate', leaves=380, coverage=1.5, shell=.45, step=.06, nodes=420, r_tip=.16, leaf_color=(0.035, 0.070, 0.020),
                              flowers='candles', flower_count=36, tropism=(0, 0, .10), size_lo=5.5, size_hi=9.5),
    'bouleau': dict(kind='tree', leaf='diamond', leaves=2000, coverage=1.6, shell=.40, step=.055, nodes=420, r_tip=.12,
                    tropism=(0, 0, -.06), size_lo=2.4, size_hi=4.2, bark='birch'),
    'chene': dict(kind='tree', leaf='lobed', leaves=800, coverage=1.6, shell=.45, step=.06, nodes=380, r_tip=.18,
                  tropism=(0, 0, -.02), size_lo=3.4, size_hi=6.4, gnarl=.5),
    'pin_sombre': dict(kind='tree', leaf='needles', leaves=620, coverage=1.3, shell=.15, step=.06, nodes=480, r_tip=.12,
                       tropism=(0, 0, .05), size_lo=5.0, size_hi=8.0),
    'pin_sylvestre': dict(kind='tree', leaf='needles', leaves=520, coverage=1.3, shell=.15, step=.065, nodes=440, r_tip=.12,
                          tropism=(0, 0, .04), size_lo=5.0, size_hi=8.0, bark='scots'),
    'cypres': dict(kind='column', leaf='flame', leaves=720, size_lo=4.0, size_hi=6.5),
    'genevrier': dict(kind='shrub', leaf='needles', leaves=800, coverage=1.6, shell=.25, step=.07, nodes=340, r_tip=.12,
                      tropism=(0, 0, .02), size_lo=4.4, size_hi=6.0, stems=5),
    'noisetier': dict(kind='shrub', leaf='round', leaves=1500, coverage=1.6, shell=.30, step=.06, nodes=380, r_tip=.12,
                      tropism=(0, 0, .0), size_lo=3.0, size_hi=5.2, stems=7),
    'saule_pleureur': dict(kind='willow', leaf='lanceolate', leaves=700, coverage=1.0, shell=.55, step=.07, nodes=300, r_tip=.12,
                           tropism=(0, 0, -.10), size_lo=3.4, size_hi=4.6, strands=90),
    'arbuste_baies': dict(kind='shrub', leaf='diamond', leaves=1300, coverage=1.6, shell=.35, step=.06, nodes=330, r_tip=.12,
                          tropism=(0, 0, .0), size_lo=3.2, size_hi=5.2, berries=70, stems=8),
    'rhododendron': dict(kind='shrub', leaf='oblong', leaves=520, coverage=1.6, shell=.40, step=.065, nodes=320, r_tip=.14, leaf_color=(0.022, 0.050, 0.020),
                         tropism=(0, 0, .02), size_lo=5.0, size_hi=8.5, trusses=24, stems=8),
    'fougere': dict(kind='fern', fronds=26),
    'prairie_fleurie': dict(kind='meadow', blades=250, flowers=58),
}


def _bark_color_fn(recipe, base):
    style = recipe.get('bark')
    if style == 'birch':
        white = (0.45, 0.43, 0.39)
        dark = (0.03, 0.027, 0.025)

        def fn(t):
            return dark if (int(t * 22) % 3 == 0 and t > .06) else white
        return fn
    if style == 'scots':
        orange = (0.20, 0.075, 0.03)

        def fn(t):
            return lerp(base, orange, max(0.0, min(1.0, (t - .35) / .5)))
        return fn
    return base


def _leaf_size(spec, recipe, n):
    area = shell_area(spec)
    size = math.sqrt(recipe['coverage'] * area / max(n, 1) / 0.40)
    return max(recipe['size_lo'], min(recipe['size_hi'], size))


def _skeleton(spec, recipe, rng):
    """Les noeuds de la charpente : fut mesure (sous-divise) ou tiges de l'arbuste, puis colonisation d'espace."""
    kind = recipe['kind']
    top = max(b[0] + b[1] * .5 for b in spec['bands'])
    low = min(b[0] - b[1] * .5 for b in spec['bands'])
    crown_h = top - low
    crown_w = max(b[4] for b in spec['bands'])
    step = recipe['step'] * crown_h
    nodes, fixed_r = [], []
    if kind in ('tree', 'willow'):
        trunk = spec.get('trunk') or []
        pts = [(x, y, z) for x, y, z, r in trunk] or [(0.0, 0.0, -50.0), (0.0, 0.0, low)]
        rs = [r for x, y, z, r in trunk] or [2.0, 1.6]
        chain, chain_r = [], []
        for i in range(len(pts) - 1):
            a, b = pts[i], pts[i + 1]
            n = max(1, int(math.ceil(length(sub(b, a)) / (step * 1.4))))
            for k in range(n):
                t = k / float(n)
                chain.append(lerp(a, b, t))
                chain_r.append(rs[i] + (rs[i + 1] - rs[i]) * t)
        chain.append(pts[-1])
        chain_r.append(rs[-1])
        for i, (p, r) in enumerate(zip(chain, chain_r)):
            nodes.append((p, i - 1))
            fixed_r.append(r)
    else:
        # arbuste : des tiges qui montent du sol, penchees vers l'exterieur, sur lesquelles la colonisation s'accroche
        for k in range(recipe.get('stems', 6)):
            a = TAU * k / recipe.get('stems', 6) + rng.uniform(-.3, .3)
            lean = rng.uniform(.10, .35)
            base = (math.cos(a) * rng.uniform(0, 2.0), math.sin(a) * rng.uniform(0, 2.0), -50.0)
            nodes.append((base, -1))
            fixed_r.append(1.1)
            h = crown_h * rng.uniform(.30, .50)
            n = max(2, int(h / (step * 1.3)))
            prev = len(nodes) - 1
            for j in range(1, n + 1):
                t = j / float(n)
                nodes.append(((base[0] + math.cos(a) * lean * h * t, base[1] + math.sin(a) * lean * h * t, base[2] + h * t), prev))
                fixed_r.append(1.1 * (1 - .4 * t))
                prev = len(nodes) - 1
    n_fixed = len(nodes)
    sample = crown_sampler(spec, recipe['shell'], rng, fill_floor=(.2 if recipe.get('leaf') == 'needles' and kind == 'tree' else 0.0))
    attractors = [sample() for _ in range(int(recipe['nodes'] * 2.4))]
    influence = max(crown_w * .62, step * 5.0)
    kill = step * 2.0
    trop = recipe['tropism']
    if recipe.get('gnarl'):
        trop = (rng.uniform(-1, 1) * recipe['gnarl'] * .25, rng.uniform(-1, 1) * recipe['gnarl'] * .25, trop[2])
    grow(nodes, attractors, step, influence, kill, trop, rng, recipe['nodes'] + n_fixed)
    rad, kids = pipe_radii(nodes, recipe['r_tip'])
    ref = (max(fixed_r) * .50) if kind != 'shrub' else 1.0
    mx = max([rad[i] for i in range(n_fixed, len(nodes)) if 0 <= nodes[i][1] < n_fixed] or [0.0])
    k_scale = min(1.0, ref / mx) if mx > 0 else 1.0
    for i in range(n_fixed, len(nodes)):
        rad[i] = max(recipe['r_tip'], rad[i] * k_scale)
    for i in range(n_fixed):
        rad[i] = max(fixed_r[i], rad[i] * (1.0 if kind == 'shrub' else k_scale))
    return nodes, rad, kids, n_fixed, sample, (top, low, crown_h, crown_w)


def build_tree(spec, recipe, lods):
    """Arbre ou arbuste : un squelette, puis le bois et les feuilles de chaque LOD. Rend [(wood, leaf)] par LOD."""
    kind = recipe['kind']
    bark = tuple(spec['bark'])
    fol = tuple(recipe.get('leaf_color') or spec['foliage'])
    grow_rng = random.Random('%s:model:grow' % spec['name'])
    nodes, rad, kids, n_fixed, sample, (top, low, crown_h, crown_w) = _skeleton(spec, recipe, grow_rng)
    paths = branch_paths(nodes, rad, kids)
    colour = _bark_color_fn(recipe, bark)
    out = []
    for lod in lods:
        rng = random.Random('%s:model:leaves:%d' % (spec['name'], lod))
        wood = Batch(0.0)
        leaves = Batch(1.0)
        r_cut = (0.0, .30, 99.0)[lod]
        for path in paths:
            pts = [nodes[i][0] for i in path]
            rs = [rad[i] for i in path]
            if len(pts) < 2 or max(rs) < r_cut:
                continue
            sides = 7 if max(rs) > 3.0 else (5 if max(rs) > 1.4 else 3)
            if lod >= 1:
                sides = max(3, sides - 2)
            if callable(colour):
                tube(wood, pts, rs, sides, colour if path[0] == 0 else colour(0.5))
            else:
                tube(wood, pts, rs, sides, colour)
        if lod == 2:
            out.append((wood, lumps(spec, random.Random(spec['name'] + ':lumps'))))
            continue
        n_leaf = int(recipe['leaves'] * (1.0 if lod == 0 else .45))
        size = _leaf_size(spec, recipe, max(n_leaf, 1)) * (1.0 if lod == 0 else 1.45)
        shape = recipe['leaf']
        terminal = [i for i in range(len(nodes)) if kids[i] == 0 and nodes[i][0][2] > low - 8]
        twigs = [i for i in range(len(nodes)) if kids[i] > 0 and rad[i] <= recipe['r_tip'] * 2.2 and nodes[i][0][2] > low - 8]
        eligible = terminal + twigs
        placed = 0
        if eligible and n_leaf > 0:
            per = max(1, int(math.ceil(n_leaf / float(len(eligible)))))
            order = list(eligible)
            rng.shuffle(order)
            for i in order:
                if placed >= n_leaf:
                    break
                p = nodes[i][0]
                par = nodes[i][1]
                d = norm(sub(p, nodes[par][0])) if par >= 0 else UP
                for _k in range(per):
                    if placed >= n_leaf:
                        break
                    if kind == 'willow':
                        dd = norm((rng.uniform(-.3, .3) + d[0] * .3, rng.uniform(-.3, .3) + d[1] * .3, -.9))
                    else:
                        dd = norm(add(add(mul(d, .55), mul(rand_unit(rng), .85)), (0, 0, .15)))
                    c = vary(fol, rng, .20)
                    if shape == 'palmate':
                        x, s, z = _frame(dd, UP, rng)
                        leaflets(leaves, add(p, mul(dd, size * .08)), x, s, z, size, c, rng)
                    elif shape == 'needles':
                        tuft(leaves, p, dd, size, 8 if lod == 0 else 5, 32, c, rng)
                    else:
                        x, s, z = _frame(dd, UP, rng)
                        leaf(leaves, add(p, mul(dd, size * .08)), x, s, z, size, shape, c, tint(c, 1.18))
                    placed += 1
        if lod == 0:
            _flowers(spec, recipe, leaves, sample, rng, top, crown_h)
            if kind == 'willow':
                _willow_strands(spec, recipe, nodes, wood, leaves, rng, low, crown_w, bark, fol, 1.0)
        elif kind == 'willow' and lod == 1:
            _willow_strands(spec, recipe, nodes, wood, leaves, rng, low, crown_w, bark, fol, .5)
        out.append((wood, leaves))
    return out


def _flowers(spec, recipe, leaves, sample, rng, top, crown_h):
    if recipe.get('flowers') == 'candles':
        cc = (0.46, 0.42, 0.20)
        for _ in range(recipe['flower_count'] * 3):
            p = sample()
            if p[2] < top - crown_h * .5:
                continue
            cone(leaves, p, (rng.uniform(-.2, .2), rng.uniform(-.2, .2), 1.0), 1.5, 6.5, 6, vary(cc, rng, .15), tint(cc, 1.25))
    if recipe.get('berries'):
        bc = (0.22, 0.27, 0.05)
        for _ in range(recipe['berries']):
            blob(leaves, sample(), .55, vary(bc, rng, .2), rng)
    if recipe.get('trusses'):
        pink = (0.36, 0.04, 0.36)       # le materiau rechauffe la lumiere transmise : un rose vire a l orange s il n a pas de bleu
        n = 0
        while n < recipe['trusses']:
            base = sample()
            if base[2] < top - crown_h * .7:
                continue
            n += 1
            for f in range(11):
                a = TAU * f / 11.0 + rng.uniform(-.2, .2)
                rr = rng.uniform(.0, 2.6)
                c = add(base, (math.cos(a) * rr, math.sin(a) * rr, rng.uniform(.0, 2.0)))
                flower_star(leaves, c, (math.cos(a) * .4, math.sin(a) * .4, 1.0), 2.4, 5, vary(pink, rng, .2), (0.5, 0.35, 0.10), rng)


def _willow_strands(spec, recipe, nodes, wood, leaves, rng, low, crown_w, bark, fol, share):
    outer = [i for i in range(len(nodes)) if nodes[i][0][2] > low * .2 and length((nodes[i][0][0], nodes[i][0][1], 0)) > crown_w * .30]
    rng.shuffle(outer)
    for i in outer[:int(recipe['strands'] * share)]:
        p = nodes[i][0]
        n_seg = 6
        ln = rng.uniform(.55, 1.0) * min(40.0, p[2] + 46.0)
        sw = rng.uniform(-1, 1)
        pts = []
        for k in range(n_seg + 1):
            t = k / float(n_seg)
            pts.append((p[0] + sw * 1.2 * math.sin(t * 5.0) + (p[0] / max(crown_w, 1)) * 2.5 * t,
                        p[1] + sw * 1.2 * math.cos(t * 5.0) + (p[1] / max(crown_w, 1)) * 2.5 * t, p[2] - ln * t))
        tube(wood, pts, [.16 * (1 - .6 * k / n_seg) for k in range(n_seg + 1)], 3, bark)
        for k in range(1, n_seg + 1):
            for side in (-1, 1):
                d = norm((side * .8, rng.uniform(-.3, .3), -.6))
                x, s, z = _frame(d, UP, rng)
                leaf(leaves, pts[k], x, s, z, recipe['size_hi'] * rng.uniform(.9, 1.3), 'lanceolate', vary(fol, rng, .2), tint(fol, 1.2))


def lumps(spec, rng):
    """LOD2 : masses fermees par tranche (ellipsoides), pour la couverture au loin."""
    leaves = Batch(1.0)
    fol = tuple(spec['foliage'])
    for band in spec['bands'][::2]:
        zc, h, cx, cy, hw, hd = band[:6]
        fill = band[6] if len(band) > 6 else 1.0
        if fill < .3:
            continue
        rz = max(h * 1.4, .8)
        col = tint(fol, rng.uniform(.7, .95))
        rows, cols = 4, 7
        grid = []
        for i in range(rows + 1):
            phi = math.pi * i / rows
            for j in range(cols):
                th = TAU * j / cols
                grid.append(leaves.vert((cx + hw * math.sin(phi) * math.cos(th), cy + hd * math.sin(phi) * math.sin(th),
                                         zc + rz * math.cos(phi)), col))
        for i in range(rows):
            for j in range(cols):
                a = grid[i * cols + j]
                b = grid[i * cols + (j + 1) % cols]
                c = grid[(i + 1) * cols + j]
                d = grid[(i + 1) * cols + (j + 1) % cols]
                leaves.tri(a, c, b)
                leaves.tri(b, c, d)
    return leaves


# ------------------------------------------------------------------------------------------------- cypres
def build_column(spec, recipe, lods):
    """Colonne : une fleche centrale effilee et des pinceaux de feuillage-flamme sur toute la hauteur."""
    out = []
    fol = tuple(spec['foliage'])
    bark = tuple(spec['bark'])
    bands = spec['bands']
    top = max(b[0] + b[1] * .5 for b in bands)
    for lod in lods:
        rng = random.Random('%s:model:leaves:%d' % (spec['name'], lod))
        wood = Batch(0.0)
        leaves = Batch(1.0)
        pts = [(b[2], b[3], b[0]) for b in sorted(bands, key=lambda b: b[0])]
        pts = [(0.0, 0.0, -50.0)] + pts
        rs = [2.2 * (1 - .85 * k / float(len(pts) - 1)) for k in range(len(pts))]
        tube(wood, pts, rs, 5 if lod == 0 else 3, bark)
        if lod == 2:
            out.append((wood, lumps(spec, random.Random(spec['name'] + ':lumps'))))
            continue
        sample = crown_sampler(spec, .3, rng)
        n = int(recipe['leaves'] * (1.0 if lod == 0 else .45))
        size = recipe['size_lo'] * (1.0 if lod == 0 else 1.5)
        for _ in range(n):
            p = sample()
            outward = norm((p[0] - 0.0, p[1], 0.0)) if length((p[0], p[1], 0)) > 1e-3 else UP
            tuft(leaves, p, norm(add(mul(outward, .55), (0, 0, 1.0))), size * rng.uniform(.9, 1.4), 4, 24,
                 vary(fol, rng, .22), rng, base_w=.32)
        out.append((wood, leaves))
    return out


# ------------------------------------------------------------------------------------------------- fougere
def build_fern(spec, recipe, lods):
    out = []
    fol = tuple(spec['foliage'])
    stem = (0.07, 0.07, 0.025)
    reach = max(b[4] for b in spec['bands']) * .95
    for lod in lods:
        rng = random.Random('%s:model:%d' % (spec['name'], lod))
        wood = Batch(0.0)
        leaves = Batch(1.0)
        if lod == 2:
            out.append((wood, lumps(spec, random.Random(spec['name'] + ':lumps'))))
            continue
        n = recipe['fronds'] if lod == 0 else 10
        pairs = 24 if lod == 0 else 10
        seg = 9 if lod == 0 else 5
        for f in range(n):
            az = f * 2.399963 + rng.uniform(-.2, .2)
            ring = .2 + .8 * ((f % 5) / 4.0)
            L = 96.0 * (.55 + .45 * ring) * rng.uniform(.92, 1.05)
            out_r = reach * (.30 + .70 * ring)
            base = (rng.uniform(-1.2, 1.2), rng.uniform(-1.2, 1.2), -50.0)
            d = (math.cos(az), math.sin(az), 0.0)
            pts = []
            for k in range(seg + 1):
                t = k / float(seg)
                pts.append((base[0] + d[0] * out_r * (t ** 1.4) * 1.1, base[1] + d[1] * out_r * (t ** 1.4) * 1.1,
                            base[2] + L * math.sin(t * 1.35) * .95 - L * .35 * (t ** 3.0)))
            tube(wood, pts, [.55 * (1 - .8 * (k / float(seg))) for k in range(seg + 1)], 3, stem)
            side = norm(cross(norm(sub(pts[-1], pts[0])), UP))
            for j in range(pairs):
                t = .10 + .90 * j / float(max(1, pairs - 1))
                u = t * seg
                i0 = min(int(u), seg - 1)
                pr = lerp(pts[i0], pts[i0 + 1], u - i0)
                tang = norm(sub(pts[i0 + 1], pts[i0]))
                ln = L * .30 * (math.sin(math.pi * min(1.0, t * .9 + .05)) ** .75) + 2.0
                for sgn in (-1, 1):
                    dirp = norm(add(add(mul(side, sgn * .9), mul(tang, .60)), (0, 0, -.10)))
                    sd = norm(cross(dirp, UP))
                    pm = add(pr, mul(dirp, ln * .45))
                    c = vary(fol, rng, .16)
                    a = leaves.vert(pr, tint(c, .7))
                    b = leaves.vert(add(pm, mul(sd, ln * .20)), c)
                    cc = leaves.vert(add(pr, mul(dirp, ln)), tint(c, 1.25))
                    dd = leaves.vert(add(pm, mul(sd, -ln * .20)), c)
                    leaves.tri(a, b, cc)
                    leaves.tri(a, cc, dd)
        out.append((wood, leaves))
    return out


# ------------------------------------------------------------------------------------------------- prairie
def build_meadow(spec, recipe, lods):
    out = []
    fol = tuple(spec['foliage'])
    hw = max(b[4] for b in spec['bands'])
    palette = [('umbel', (0.60, 0.58, 0.50)), ('daisy', (0.62, 0.60, 0.52)), ('button', (0.55, 0.40, 0.04)),
               ('bell', (0.12, 0.10, 0.45)), ('seed', (0.28, 0.22, 0.09))]
    for lod in lods:
        rng = random.Random('%s:model:%d' % (spec['name'], lod))
        wood = Batch(0.0)
        leaves = Batch(1.0)
        if lod == 2:
            out.append((wood, lumps(spec, random.Random(spec['name'] + ':lumps'))))
            continue
        n_bl = recipe['blades'] if lod == 0 else recipe['blades'] // 2
        for k in range(n_bl):
            r = hw * math.sqrt((k + .5) / n_bl) * .95
            th = k * 2.399963
            base = (r * math.cos(th), r * math.sin(th), -50.0)
            h = rng.uniform(.30, .95) * 95.0 * (1.0 - .25 * (r / hw))
            lean = rng.uniform(.05, .35) * (1.0 + r / hw)
            az = th + rng.uniform(-.8, .8)
            w = rng.uniform(.6, 1.1) * (1.0 if lod == 0 else 1.5)
            n_s = 3
            left, right = [], []
            c0 = vary(tint(fol, .55), rng, .15)
            for i in range(n_s + 1):
                t = i / float(n_s)
                bend = lean * h * (t ** 1.7)
                p = (base[0] + math.cos(az) * bend, base[1] + math.sin(az) * bend, base[2] + h * t * (1 - .12 * t))
                wd = w * (1.0 - t) ** .8
                sd = (-math.sin(az), math.cos(az), 0.0)
                c = lerp(c0, tint(fol, 1.5), t)
                if i == n_s:
                    tip = leaves.vert(p, lerp(c, (0.30, 0.24, 0.08), .45))
                    left.append(tip)
                    right.append(tip)
                else:
                    left.append(leaves.vert(add(p, mul(sd, wd)), c))
                    right.append(leaves.vert(add(p, mul(sd, -wd)), c))
            for i in range(n_s):
                leaves.tri(left[i], right[i], right[i + 1])
                leaves.tri(left[i], right[i + 1], left[i + 1])
        n_fl = recipe['flowers'] if lod == 0 else recipe['flowers'] // 3
        for k in range(n_fl):
            r = hw * math.sqrt(rng.random()) * .9
            th = rng.uniform(0, TAU)
            base = (r * math.cos(th), r * math.sin(th), -50.0)
            h = rng.uniform(.5, 1.0) * 92.0
            kind, col = palette[k % len(palette)]
            lean = (rng.uniform(-.25, .25), rng.uniform(-.25, .25))
            tp = (base[0] + lean[0] * h, base[1] + lean[1] * h, base[2] + h)
            tube(wood, [base, lerp(base, tp, .5), tp], [.30, .24, .18], 3, tint(fol, .7), tip=False)
            c = vary(col, rng, .12)
            if kind == 'umbel':
                flower_star(leaves, tp, (0, 0, 1), 3.6, 6, c, tint(c, .9), rng)
            elif kind == 'daisy':
                flower_star(leaves, tp, (lean[0], lean[1], 1), 3.4, 8, c, (0.55, 0.40, 0.03), rng)
            elif kind == 'button':
                flower_star(leaves, tp, (0, 0, 1), 2.2, 5, c, tint(c, .8), rng)
            elif kind == 'bell':
                cone(leaves, add(tp, (0, 0, 3.0)), (0, 0, -1), 1.7, 3.0, 5, c, tint(c, 1.3))
            else:
                cone(leaves, tp, (0, 0, 1), 1.1, 5.5, 4, c, tint(c, 1.1))
        out.append((wood, leaves))
    return out


# ------------------------------------------------------------------------------------------------- entree
def build_all(spec, lods=(0, 1, 2)):
    """[(wood, leaf)] pour chaque LOD demande (0 detail, 1 moyen, 2 lointain) ; graines fixes par espece."""
    recipe = RECIPES[spec['name']]
    kind = recipe['kind']
    if kind == 'fern':
        return build_fern(spec, recipe, lods)
    if kind == 'meadow':
        return build_meadow(spec, recipe, lods)
    if kind == 'column':
        return build_column(spec, recipe, lods)
    return build_tree(spec, recipe, lods)
