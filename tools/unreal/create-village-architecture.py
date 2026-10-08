"""Architecture du village a l'echelle humaine (ARCHITECTURE_SCALE_001, architecture-crusade-001).

Un kit de construction (murs de moellons, ossature bois et torchis, toitures de tuile canal ou de
planches, ouvertures, escaliers, galeries, mobilier) et une grammaire qui l'assemble en maisonnees :
maison pauvre, maison moyenne de reference, ferme, grenier communautaire, puits, atelier, chapelle.
Chaque maisonnee remplit sa parcelle de simulation (20 m) : corps + assise terrassee separee.

Trois usages :

    python create-village-architecture.py            hors Unreal : construit, valide l'echelle,
                                                      ecrit docs/unreal/architecture/architecture-kit-001.json
    ANASTASIS_ARCH_GEOMETRY_ONLY=1 dans l'editeur    idem, sans rien ecrire dans Content/
    create-village-architecture.ps1 [-Rebuild]        editeur dedie : ecrit /Game/Anastasis/VillageArchitecture

Toutes les dimensions sont en cm. Repere : pivot au centre de la parcelle, z=0 = cour, +Y = acces.
"""
import json
import math
import os
import random

PKG = '/Game/Anastasis/VillageArchitecture'
VERSION = 'settlement-morphogenesis-001-v1'
MATERIAL = 'M_AnastasisArchitecture'

# --- Convention d'echelle (ARCHITECTURE_SCALE_001) ------------------------------------------
HUMAN = 170.0
PARCEL = 2000.0
PARCEL_MARGIN = 100.0
DOOR_CLEAR_MIN = 185.0
DOOR_WIDTH_MIN = 85.0
CEILING_MIN = 225.0
PITCH_RANGE = (22.0, 36.0)
OVERHANG_RANGE = (50.0, 90.0)
STONE_WALL = (50.0, 65.0)
LIVING_ROOM_MIN_M2 = 16.0
FOOTING_DEPTH = 480.0

# --- Classes de materiau, portees par l'alpha du sommet (M_AnastasisArchitecture) -------------
IRON, FIBER, EARTH, RUBBLE, ASHLAR, PLASTER, DAUB, WOOD, PLANK, TILE = (
    .05, .15, .25, .35, .45, .55, .65, .75, .85, .95)
CLASS_NAMES = {IRON: 'iron', FIBER: 'fiber', EARTH: 'earth', RUBBLE: 'rubble', ASHLAR: 'ashlar',
               PLASTER: 'plaster', DAUB: 'daub', WOOD: 'wood', PLANK: 'plank', TILE: 'tile'}

# Albedos lineaires sous EV100 14 : rien au-dessus de 0,5 (loi 1 du realisme).
C_IRON = (.045, .043, .040, IRON)
C_ROPE = (.27, .21, .13, FIBER)
C_STRAW = (.40, .33, .17, FIBER)
C_WOOL = (.25, .07, .05, FIBER)
C_LINEN = (.40, .37, .30, FIBER)
C_EARTH = (.19, .15, .10, EARTH)
C_STONE = (.25, .235, .21, ASHLAR)   # moellon pose : bloc plein (pas de joints dessines)
C_MORTAR = (.20, .185, .16, RUBBLE)  # hourdage et noyau : le motif de joints vit ici
C_ASHLAR = (.36, .33, .28, ASHLAR)
C_BRICK = (.33, .15, .08, TILE)
C_PLASTER = (.45, .42, .36, PLASTER)
C_DAUB = (.30, .22, .14, DAUB)
C_WOOD = (.16, .12, .085, WOOD)
C_PLANK = (.20, .15, .10, PLANK)
C_TILE = (.36, .165, .085, TILE)
C_CLAY = (.33, .16, .08, TILE)


def add(a, b): return (a[0] + b[0], a[1] + b[1], a[2] + b[2])
def sub(a, b): return (a[0] - b[0], a[1] - b[1], a[2] - b[2])
def mul(a, s): return (a[0] * s, a[1] * s, a[2] * s)
def dot(a, b): return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]
def cross(a, b): return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])
def length(a): return math.sqrt(dot(a, a))
def lerp(a, b, t): return add(a, mul(sub(b, a), t))


def norm(a):
    d = length(a)
    if d < 1e-9:
        raise ValueError('zero vector')
    return mul(a, 1.0 / d)


def tint(c, k):
    return (c[0] * k, c[1] * k, c[2] * k, c[3])


def yaw_vec(deg):
    r = math.radians(deg)
    return (math.cos(r), math.sin(r), 0.0)


class Mesh:
    """Polygones plats (aretes vives) ou anneaux lisses ; UV planaires en metres, par face."""

    def __init__(self):
        self.v, self.t, self.c, self.uv = [], [], [], []
        self.meta = dict(doors=[], floors=[], rooms=[], hearths=[], sleep=[], storage=[], work=[],
                         walls=[], roofs=[], windows=[], ceilings=[])

    # -- primitives ------------------------------------------------------------------------
    def _frame(self, n, uaxis):
        if uaxis is not None and abs(dot(norm(uaxis), n)) < .95:
            ua = norm(sub(uaxis, mul(n, dot(uaxis, n))))
        elif abs(n[2]) > .7:
            ua = (1.0, 0.0, 0.0)
        else:
            ua = norm(cross((0.0, 0.0, 1.0), n))
        return ua, cross(n, ua)

    def poly(self, pts, color, uaxis=None, outward=None):
        if len(pts) < 3:
            return
        nx = ny = nz = 0.0
        for i, p in enumerate(pts):
            q = pts[(i + 1) % len(pts)]
            nx += (p[1] - q[1]) * (p[2] + q[2])
            ny += (p[2] - q[2]) * (p[0] + q[0])
            nz += (p[0] - q[0]) * (p[1] + q[1])
        n = (nx, ny, nz)
        if length(n) < 1e-6:
            return
        n = norm(n)
        if outward is not None and dot(n, outward) < 0:
            pts = list(reversed(pts))
            n = mul(n, -1)
        ua, va = self._frame(n, uaxis)
        base = len(self.v)
        for p in pts:
            self.v.append(tuple(p))
            self.c.append(color)
            self.uv.append((dot(p, ua) / 100.0, dot(p, va) / 100.0))
        for i in range(1, len(pts) - 1):
            self.t.append((base, base + i, base + i + 1))

    def quad(self, a, b, c, d, color, uaxis=None, outward=None):
        self.poly([a, b, c, d], color, uaxis, outward)

    def obox(self, center, axes, half, color, skip=(), uaxis=None, colors=None):
        """Boite orientee. axes = 3 vecteurs unitaires, half = demi-tailles. skip = faces omises
        ('-0','+0','-1','+1','-2','+2'). colors = couleur par face (dict) si besoin."""
        ex, ey, ez = (mul(axes[0], half[0]), mul(axes[1], half[1]), mul(axes[2], half[2]))

        def corner(sx, sy, sz):
            return add(center, add(mul(ex, sx), add(mul(ey, sy), mul(ez, sz))))
        for k in range(3):
            for s in (-1, 1):
                key = ('+' if s > 0 else '-') + str(k)
                if key in skip:
                    continue
                i, j = [x for x in range(3) if x != k]
                pts = []
                for a, b in ((-1, -1), (1, -1), (1, 1), (-1, 1)):
                    sv = [0, 0, 0]
                    sv[k], sv[i], sv[j] = s, a, b
                    pts.append(corner(*sv))
                out = mul(axes[k], s)
                ua = uaxis if (uaxis is not None and abs(dot(uaxis, out)) < .5) else None
                self.poly(pts, (colors or {}).get(key, color), ua, out)

    def box(self, lo, hi, color, skip=(), uaxis=None):
        c = mul(add(lo, hi), .5)
        h = mul(sub(hi, lo), .5)
        if min(h) <= 0.05:
            return
        self.obox(c, ((1, 0, 0), (0, 1, 0), (0, 0, 1)), h, color, skip, uaxis)

    def beam(self, a, b, w, h, color, side=None, skip_ends=False):
        """Poutre de a a b, section w (cote) x h (dessus). UV : U le long du fil du bois."""
        axis = sub(b, a)
        L = length(axis)
        if L < 1.0:
            return
        axis = mul(axis, 1.0 / L)
        if side is None:
            side = cross((0, 0, 1), axis) if abs(axis[2]) < .95 else (1, 0, 0)
        side = norm(sub(side, mul(axis, dot(side, axis))))
        up = cross(axis, side)
        skip = ('-0', '+0') if skip_ends else ()
        self.obox(mul(add(a, b), .5), (axis, side, up), (L / 2, w / 2, h / 2), color, skip, uaxis=axis)

    def ring(self, center, axis_u, axis_v, radius, segments, start=0.0, end=math.tau):
        pts = []
        for k in range(segments + 1):
            a = start + (end - start) * k / segments
            pts.append(add(center, add(mul(axis_u, radius * math.cos(a)), mul(axis_v, radius * math.sin(a)))))
        return pts

    def lathe(self, profile, color, segments=16, center=(0, 0, 0), wear=0.0, rng=None, cap_top=False):
        """Solide de revolution autour de Z ; profile = [(rayon, z)] de bas en haut, sommets partages."""
        base = len(self.v)
        rings = []
        for j, (r, z) in enumerate(profile):
            ring = []
            for k in range(segments):
                ang = math.tau * k / segments
                f = 1 + wear * math.sin(ang * 5 + z * .21) * math.sin(ang * 3 - z * .05)
                p = add(center, (r * f * math.cos(ang), r * f * math.sin(ang), z))
                ring.append(len(self.v))
                self.v.append(p)
                self.c.append(tint(color, .94 + .06 * math.sin(z * .2 + ang * 2)))
                self.uv.append((ang * max(r, 5) / 100.0, z / 100.0))
            rings.append(ring)
        for a, b in zip(rings, rings[1:]):
            for k in range(segments):
                l = (k + 1) % segments
                self.t.extend([(a[k], a[l], b[k]), (a[l], b[l], b[k])])
        if cap_top and profile[-1][0] > .5:
            r, z = profile[-1]
            self.poly([add(center, (r * math.cos(math.tau * k / segments), r * math.sin(math.tau * k / segments), z))
                       for k in range(segments)], color, outward=(0, 0, 1))
        return base

    def tube(self, points, radius, color, sides=6, cap=True):
        rings = []
        prev_u = None
        for j, p in enumerate(points):
            tangent = norm(sub(points[min(j + 1, len(points) - 1)], points[max(0, j - 1)]))
            ref = (0, 0, 1) if abs(tangent[2]) < .9 else (1, 0, 0)
            u = norm(cross(tangent, ref)) if prev_u is None else norm(sub(prev_u, mul(tangent, dot(prev_u, tangent))))
            prev_u = u
            v = cross(tangent, u)
            ring = []
            r = radius[j] if isinstance(radius, (list, tuple)) else radius
            for k in range(sides):
                a = math.tau * k / sides
                ring.append(len(self.v))
                self.v.append(add(p, mul(add(mul(u, math.cos(a)), mul(v, math.sin(a))), r)))
                self.c.append(tint(color, .92 + .08 * math.cos(a)))
                self.uv.append((j * .5, k / sides))
            rings.append(ring)
        for a, b in zip(rings, rings[1:]):
            for k in range(sides):
                l = (k + 1) % sides
                self.t.extend([(a[k], b[k], a[l]), (a[l], b[k], b[l])])
        if cap:
            for ring, flip in ((rings[0], True), (rings[-1], False)):
                pts = [self.v[i] for i in ring]
                self.poly(list(reversed(pts)) if flip else pts, color)

    def extend(self, other, yaw=0.0, offset=(0, 0, 0), mirror_x=False, meta=True):
        c, s = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))

        def tr(p):
            x = -p[0] if mirror_x else p[0]
            return (x * c - p[1] * s + offset[0], x * s + p[1] * c + offset[1], p[2] + offset[2])
        base = len(self.v)
        self.v.extend(tr(p) for p in other.v)
        self.c.extend(other.c)
        self.uv.extend(other.uv)
        for a, b, d in other.t:
            self.t.append((base + a, base + d, base + b) if mirror_x else (base + a, base + b, base + d))
        if meta:
            for key, items in other.meta.items():
                for it in items:
                    it = dict(it)
                    for k in ('at', 'a', 'b', 'lo', 'hi'):
                        if k in it:
                            it[k] = list(tr(tuple(it[k])))
                    if 'yaw' in it:
                        it['yaw'] = (it['yaw'] + yaw) % 360
                    self.meta[key].append(it)

    # -- sortie ------------------------------------------------------------------------
    def bounds(self):
        xs, ys, zs = zip(*self.v)
        return (min(xs), min(ys), min(zs)), (max(xs), max(ys), max(zs))

    def finish(self):
        """Normales par sommet (plates sur les polygones, lisses sur les anneaux), sens Unreal."""
        normals = [[0.0, 0.0, 0.0] for _ in self.v]
        for a, b, c in self.t:
            n = cross(sub(self.v[b], self.v[a]), sub(self.v[c], self.v[a]))
            for i in (a, b, c):
                normals[i][0] += n[0]
                normals[i][1] += n[1]
                normals[i][2] += n[2]
        out_n = []
        for n in normals:
            d = math.sqrt(n[0] ** 2 + n[1] ** 2 + n[2] ** 2)
            out_n.append((n[0] / d, n[1] / d, n[2] / d) if d > 1e-12 else (0.0, 0.0, 1.0))
        tris = [(a, c, b) for a, b, c in self.t
                if length(cross(sub(self.v[b], self.v[a]), sub(self.v[c], self.v[a]))) > 1e-6]
        return dict(v=self.v, t=tris, c=self.c, uv=self.uv, n=out_n)


# ============================================================================================
# KIT : murs, ouvertures, charpente, toitures, details
# ============================================================================================

def _wall_axes(p0, p1):
    d = sub(p1, p0)
    L = length(d)
    u = mul(d, 1.0 / L)
    n = (u[1], -u[0], 0.0)    # a droite de la marche p0 -> p1 : l'exterieur si on tourne en sens horaire
    return u, n, L


def _strips(L, openings, z0, z1):
    """Decoupe un mur [0,L] x [z0,z1] autour des ouvertures (s0,s1,oz0,oz1) -> rectangles pleins."""
    cuts = sorted({0.0, L} | {max(0.0, min(L, o[0])) for o in openings} | {max(0.0, min(L, o[1])) for o in openings})
    rects = []
    for sa, sb in zip(cuts, cuts[1:]):
        if sb - sa < .5:
            continue
        mid = (sa + sb) / 2
        holes = sorted([(o[2], o[3]) for o in openings if o[0] < mid < o[1]])
        z = z0
        for h0, h1 in holes:
            if h0 > z + .5:
                rects.append((sa, sb, z, min(h0, z1)))
            z = max(z, h1)
        if z1 > z + .5:
            rects.append((sa, sb, z, z1))
    return rects


def wall_core(m, p0, p1, z0, z1, thick, openings, color, inner_color=None, offset=0.0):
    """Masse du mur : epaisseur reelle, tableaux d'ouverture visibles. offset = decalage vers l'exterieur."""
    u, n, L = _wall_axes(p0, p1)
    for sa, sb, za, zb in _strips(L, openings, z0, z1):
        c = add(add(p0, mul(u, (sa + sb) / 2)), add(mul(n, offset), (0, 0, (za + zb) / 2)))
        cols = {'+1': color, '-1': inner_color or color}
        m.obox(c, (u, n, (0, 0, 1)), ((sb - sa) / 2, thick / 2, (zb - za) / 2), color, colors=cols)


def stone(m, center, axes, half, color, rng, bevel=None):
    """Moellon : lit plein a l'arriere, face avant retrecie (chanfrein) et legerement bombee -- pas un pave."""
    u, n, w = axes
    hx, hn, hz = half
    bv = bevel if bevel is not None else min(hx, hz) * rng.uniform(.18, .32)
    back = add(center, mul(n, -hn))
    front = add(center, mul(n, hn))
    mid = add(center, mul(n, hn - bv * .55))
    def rect(c, ex, ez, jit=0.0):
        return [add(c, add(mul(u, sx * ex + rng.uniform(-jit, jit)), mul(w, sz * ez + rng.uniform(-jit, jit))))
                for sx, sz in ((-1, -1), (1, -1), (1, 1), (-1, 1))]
    r0 = rect(back, hx, hz)
    r1 = rect(mid, hx, hz, jit=bv * .25)
    r2 = rect(front, hx - bv, hz - bv, jit=bv * .2)
    for ring_a, ring_b in ((r0, r1), (r1, r2)):
        for k in range(4):
            l = (k + 1) % 4
            m.poly([ring_a[k], ring_a[l], ring_b[l], ring_b[k]], color, outward=add(mul(n, .3), sub(mul(add(ring_a[k], ring_a[l]), .5), center)))
    m.poly(r2, color, outward=n)


def face_stones(m, p0, p1, z0, z1, thick, openings, rng, side=1, course=(18, 30), length_cm=(24, 58),
                proud=(0.6, 3.2), color=C_STONE, gap=2.4, offset=0.0, density=1.0):
    """Moellons en saillie sur un parement : assises irregulieres, joints, aucun sur les ouvertures."""
    u, n, L = _wall_axes(p0, p1)
    face = mul(n, side * thick / 2 + offset * side)
    z = z0 + gap / 2
    while z < z1 - 8:
        h = min(rng.uniform(*course), z1 - z - gap / 2)
        if h < 8:
            break
        s = rng.uniform(-length_cm[1] * .5, 0)
        while s < L:
            ln = rng.uniform(*length_cm)
            a, b = max(s + gap / 2, 1.0), min(s + ln - gap / 2, L - 1.0)
            blocked = any(a < o[1] + 3 and b > o[0] - 3 and z < o[3] + 2 and z + h > o[2] - 2 for o in openings)
            if b - a > 8 and not blocked and rng.random() < density:
                pr = rng.uniform(*proud)
                c = add(add(p0, mul(u, (a + b) / 2)), add(face, (0, 0, z + h / 2)))
                c = add(c, mul(n, side * (pr / 2 - 2)))
                tilt = rng.uniform(-.05, .05)
                ax = norm(add(u, (0, 0, tilt)))
                up = norm(cross(mul(n, side), ax)) if side > 0 else norm(cross(ax, n))
                up = up if up[2] > 0 else mul(up, -1)
                stone(m, c, (ax, mul(n, side), up), ((b - a) / 2, pr / 2 + 2, h / 2 - gap / 4),
                      tint(color, rng.uniform(.78, 1.18)), rng)
            s += ln
        z += h + gap


def quoins(m, corner, d_a, d_b, z0, z1, rng, color=C_ASHLAR, course=(26, 34)):
    """Chaine d'angle en pierre de taille : boutisses et carreaux alternes sur deux faces."""
    z = z0
    k = 0
    while z < z1 - 12:
        h = min(rng.uniform(*course), z1 - z)
        long_a = 52 if k % 2 == 0 else 30
        long_b = 30 if k % 2 == 0 else 52
        tk = tint(color, rng.uniform(.86, 1.1))
        for d, ln, other in ((d_a, long_a, d_b), (d_b, long_b, d_a)):
            c = add(corner, add(mul(d, ln / 2 - 1), add(mul(other, -1.2), (0, 0, z + h / 2))))
            m.obox(c, (d, mul(other, -1), (0, 0, 1)), (ln / 2 + 1, 3.0, h / 2 - .8), tk)
        z += h
        k += 1


def rubble_wall(m, p0, p1, z0, z1, thick, openings, rng, render=0.0, inner=C_PLASTER, stones=True,
                density=1.0, quoin_ends=(False, False), course=(18, 30)):
    """Mur de moellons hourdes. render = part du parement exterieur enduit (0..1, par plaques)."""
    m.meta['walls'].append(dict(kind='rubble', a=list(p0), b=list(p1), thick=thick, z0=z0, z1=z1))
    core = C_MORTAR if render < .5 else C_PLASTER
    wall_core(m, p0, p1, z0, z1, thick, openings, core, inner_color=inner)
    if stones:
        face_stones(m, p0, p1, z0, z1, thick, openings, rng, side=1, course=course,
                    density=density * (1.0 - render), color=C_STONE)
    u, n, L = _wall_axes(p0, p1)
    for end, flag in ((0, quoin_ends[0]), (1, quoin_ends[1])):
        if flag:
            corner = add(p1 if end else p0, mul(n, thick / 2))
            quoins(m, add(corner, mul(u, 1 if end == 0 else -1)), mul(u, 1 if end == 0 else -1), mul(n, -1), z0, z1, rng)


def timber_frame_wall(m, p0, p1, z0, z1, thick, openings, rng, infill=C_DAUB, post_every=150, braces=True,
                      render_patches=0.0, frame=C_WOOD):
    """Pan de bois : sabliere basse, poteaux, lisse mediane, decharges ; remplissage torchis en retrait."""
    m.meta['walls'].append(dict(kind='timber', a=list(p0), b=list(p1), thick=thick, z0=z0, z1=z1))
    u, n, L = _wall_axes(p0, p1)
    up = (0, 0, 1)
    pw = 16.0
    infill_t = thick - 7
    # remplissage (torchis ou enduit), en retrait de 3,5 cm sur chaque face
    rects = _strips(L, openings, z0 + 14, z1 - 14)
    for sa, sb, za, zb in rects:
        col = infill
        if render_patches > 0 and rng.random() < render_patches:
            col = tint(C_PLASTER, rng.uniform(.9, 1.04))
        c = add(add(p0, mul(u, (sa + sb) / 2)), (0, 0, (za + zb) / 2))
        m.obox(c, (u, n, up), ((sb - sa) / 2, infill_t / 2, (zb - za) / 2), tint(col, rng.uniform(.9, 1.08)))

    def member(s0, s1, za, zb, w=pw, col=frame):
        a = add(add(p0, mul(u, s0)), (0, 0, za))
        b = add(add(p0, mul(u, s1)), (0, 0, zb))
        m.beam(a, b, thick, w, tint(col, rng.uniform(.82, 1.12)), side=n)

    # sablieres
    member(0, L, z0 + 7, z0 + 7, 14)
    member(0, L, z1 - 7, z1 - 7, 14)
    # poteaux : extremites, jambages d'ouverture, intermediaires
    posts = {pw / 2, L - pw / 2}
    for o in openings:
        posts.add(max(pw / 2, o[0] - pw / 2))
        posts.add(min(L - pw / 2, o[1] + pw / 2))
    s = post_every
    while s < L - post_every * .5:
        if not any(o[0] - pw < s < o[1] + pw for o in openings) and all(abs(s - p) > 60 for p in posts):
            posts.add(s)
        s += post_every
    posts = sorted(posts)
    for p in posts:
        a = add(add(p0, mul(u, p)), (0, 0, z0 + 14))
        b = add(add(p0, mul(u, p)), (0, 0, z1 - 14))
        m.beam(a, b, pw, thick, tint(frame, rng.uniform(.8, 1.1)), side=u)
    # lisse mediane et decharges, hors ouvertures
    zm = z0 + (z1 - z0) * .5
    for pa, pb in zip(posts, posts[1:]):
        if any(o[0] < (pa + pb) / 2 < o[1] for o in openings):
            continue
        member(pa + pw / 2, pb - pw / 2, zm, zm, 12)
        if braces and (pa == posts[0] or pb == posts[-1]) and pb - pa > 70:
            if pa == posts[0]:
                member(pa + pw / 2, pb - pw / 2, z0 + 16, zm - 8, 12)
            else:
                member(pa + pw / 2, pb - pw / 2, zm - 8, z0 + 16, 12)
    # linteaux et appuis des ouvertures
    for o in openings:
        member(o[0] - pw, o[1] + pw, o[3] + 7, o[3] + 7, 14)
        if o[2] > z0 + 20:
            member(o[0] - pw, o[1] + pw, o[2] - 6, o[2] - 6, 12)


def plank_wall(m, p0, p1, z0, z1, thick, openings, rng, color=C_PLANK, board=(20, 32)):
    """Bardage de planches verticales sur ossature (grenier, grange, appentis)."""
    m.meta['walls'].append(dict(kind='plank', a=list(p0), b=list(p1), thick=thick, z0=z0, z1=z1))
    u, n, L = _wall_axes(p0, p1)
    s = 0.0
    while s < L - 4:
        w = min(rng.uniform(*board), L - s)
        mid = s + w / 2
        holes = sorted([(o[2], o[3]) for o in openings if o[0] < mid < o[1]])
        z = z0
        top = z1 - rng.uniform(0, 6)
        bot = z0 + rng.uniform(0, 5)
        segs = []
        for h0, h1 in holes:
            if h0 > z + 1:
                segs.append((max(z, bot), h0))
            z = max(z, h1)
        if top > z + 1:
            segs.append((max(z, bot), top))
        for za, zb in segs:
            c = add(add(p0, mul(u, mid)), (0, 0, (za + zb) / 2))
            m.obox(c, (u, n, (0, 0, 1)), (w / 2 - .7, 1.4, (zb - za) / 2), tint(color, rng.uniform(.75, 1.2)),
                   uaxis=(0, 0, 1))
        s += w
    # ossature interieure
    for zz in (z0 + 10, (z0 + z1) / 2, z1 - 10):
        a = add(add(p0, mul(n, -thick / 2 + 6)), (0, 0, zz))
        m.beam(a, add(a, mul(u, L)), 10, 12, tint(C_WOOD, rng.uniform(.8, 1.1)), side=n)


def door(m, p, u, n, w, h, floor_z, rng, leaf_open=65.0, thick=60.0, threshold=C_ASHLAR, frame=C_WOOD,
         name='door', wide=False, leaves=1):
    """Baie de porte posee dans un mur : cadre, seuil, linteau, vantail(s) ouvert(s) vers l'interieur.
    p = milieu de la baie au nu exterieur, u = le long du mur, n = vers l'exterieur."""
    up = (0, 0, 1)
    # seuil de pierre
    m.obox(add(p, add(mul(n, -thick / 2 + 4), (0, 0, floor_z - 6))), (u, n, up), (w / 2 + 14, thick / 2 + 6, 7),
           tint(threshold, rng.uniform(.85, 1.05)))
    # cadre : jambages et linteau debordant dans la maconnerie
    for s in (-1, 1):
        a = add(add(p, mul(u, s * (w / 2 + 6))), (0, 0, floor_z))
        m.beam(a, add(a, (0, 0, h + 4)), 12, 14, tint(frame, rng.uniform(.75, .95)), side=u)
    lc = add(add(p, mul(n, -6)), (0, 0, floor_z + h + 11))
    m.obox(lc, (u, n, up), (w / 2 + 34, 9, 11), tint(frame, rng.uniform(.7, .9)), uaxis=u)
    # vantaux de planches avec traverses et pentures, entrouverts vers l'interieur
    leaf_w = w / leaves
    for k in range(leaves):
        hinge_s = -w / 2 + 2 if k == 0 else w / 2 - 2
        sgn = 1 if k == 0 else -1
        ang = math.radians(leaf_open * (1 if k == 0 else 1.1))
        du = add(mul(u, sgn * math.cos(ang)), mul(n, -math.sin(ang)))
        hinge = add(add(p, mul(u, hinge_s)), add(mul(n, -14), (0, 0, floor_z + 1)))
        nl = cross(up, du)
        boards = max(3, int(leaf_w / 22))
        for b in range(boards):
            s0 = b * leaf_w / boards
            c = add(hinge, add(mul(du, s0 + leaf_w / boards / 2), (0, 0, (h - 2) / 2)))
            m.obox(c, (du, nl, up), (leaf_w / boards / 2 - .4, 1.6, (h - 2) / 2), tint(C_PLANK, rng.uniform(.75, 1.1)),
                   uaxis=up)
        for zz in (24, h / 2, h - 26):
            c = add(hinge, add(mul(du, leaf_w / 2), add(mul(nl, -3.4), (0, 0, zz))))
            m.obox(c, (du, nl, up), (leaf_w / 2 - 4, 1.8, 6.5), tint(C_PLANK, .8))
            c2 = add(hinge, add(mul(du, leaf_w * .3), add(mul(nl, 2.0), (0, 0, zz))))
            m.obox(c2, (du, nl, up), (leaf_w * .3, .5, 2.4), C_IRON)
    m.meta['doors'].append(dict(name=name, at=list(add(p, (0, 0, floor_z))), width=w, clear=h, floor=floor_z,
                                yaw=math.degrees(math.atan2(n[1], n[0])), wide=wide))


def window(m, p, u, n, w, h, sill_z, rng, thick=60.0, shutters=True, frame=C_WOOD, sill=C_ASHLAR, bars=False,
           open_angles=(110.0, 25.0)):
    up = (0, 0, 1)
    m.obox(add(p, add(mul(n, -thick / 2 + 6), (0, 0, sill_z - 5))), (u, n, up), (w / 2 + 10, thick / 2 + 4, 5),
           tint(sill, rng.uniform(.85, 1.05)))
    m.obox(add(p, add(mul(n, -4), (0, 0, sill_z + h + 8))), (u, n, up), (w / 2 + 22, 8, 8), tint(frame, .8), uaxis=u)
    for s in (-1, 1):
        a = add(add(p, add(mul(u, s * (w / 2 + 4)), mul(n, -5))), (0, 0, sill_z))
        m.beam(a, add(a, (0, 0, h)), 8, 9, tint(frame, .85), side=u)
    if bars:
        for k in range(1, 3):
            a = add(add(p, add(mul(u, -w / 2 + w * k / 3), mul(n, -10))), (0, 0, sill_z))
            m.beam(a, add(a, (0, 0, h)), 2.2, 2.2, C_IRON)
    if shutters:
        for k, s in enumerate((-1, 1)):
            ang = math.radians(open_angles[k % len(open_angles)])
            hinge = add(add(p, mul(u, s * w / 2)), add(mul(n, 3), (0, 0, sill_z + 1)))
            du = add(mul(u, -s * math.cos(ang)), mul(n, math.sin(ang)))
            nl = cross(up, du)
            for b in range(2):
                c = add(hinge, add(mul(du, w / 4 * (b + .5)), (0, 0, (h - 2) / 2)))
                m.obox(c, (du, nl, up), (w / 8 - .4, 1.4, (h - 2) / 2), tint(C_PLANK, rng.uniform(.7, 1.05)), uaxis=up)
    m.meta['windows'].append(dict(at=list(add(p, (0, 0, sill_z))), width=w, height=h, sill=sill_z))


# --- toitures --------------------------------------------------------------------------------

class RoofFace:
    """Un pan : ligne d'egout E0->E1, direction de pente d (montante, unitaire, dans le plan du pan),
    vmax(s) = longueur rampante disponible a l'abscisse s (pignon, croupe ou faitage)."""

    def __init__(self, e0, e1, d, vmax):
        self.e0, self.e1, self.d = e0, e1, d
        self.u, _, self.L = _wall_axes(e0, e1)
        self.u = norm(sub(e1, e0))
        self.vmax = vmax
        self.n = norm(cross(self.u, d))
        if self.n[2] < 0:
            self.n = mul(self.n, -1)

    def at(self, s, v, lift=0.0):
        return add(add(add(self.e0, mul(self.u, s)), mul(self.d, v)), mul(self.n, lift))

    def outline(self, steps=24):
        top = [self.at(self.L * k / steps, self.vmax(self.L * k / steps)) for k in range(steps, -1, -1)]
        return [self.at(0, 0), self.at(self.L, 0)] + top


def _roof_faces(kind, x0, x1, y0, y1, z_plate, pitch, ov):
    t = math.tan(math.radians(pitch))
    ex0, ex1, ey0, ey1 = x0 - ov, x1 + ov, y0 - ov, y1 + ov
    ze = z_plate - ov * t
    half = (ey1 - ey0) / 2
    ym = (ey0 + ey1) / 2
    zr = ze + half * t
    run = half / math.cos(math.radians(pitch))
    faces = []
    if kind == 'gable':
        d_f = norm((0, -half, zr - ze))
        d_b = norm((0, half, zr - ze))
        faces.append(RoofFace((ex0, ey1, ze), (ex1, ey1, ze), d_f, lambda s: run))
        faces.append(RoofFace((ex1, ey0, ze), (ex0, ey0, ze), d_b, lambda s: run))
        ridge = [((ex0, ym, zr), (ex1, ym, zr))]
        hips = []
    else:
        hx = (ex1 - ex0) / 2
        inset = min(half, hx)
        rx0, rx1 = ex0 + inset, ex1 - inset
        L = ex1 - ex0
        d_f = norm((0, -half, zr - ze))
        d_b = norm((0, half, zr - ze))

        def trap(s, L=L):
            return run * max(0.0, min(1.0, min(s, L - s) / inset))
        faces.append(RoofFace((ex0, ey1, ze), (ex1, ey1, ze), d_f, trap))
        faces.append(RoofFace((ex1, ey0, ze), (ex0, ey0, ze), d_b, trap))
        Ly = ey1 - ey0
        zr_side = ze + inset * t
        run_side = inset / math.cos(math.radians(pitch))
        d_l = norm((inset, 0, zr_side - ze))
        d_r = norm((-inset, 0, zr_side - ze))

        def tri(s, Ly=Ly):
            return run_side * max(0.0, min(1.0, min(s, Ly - s) / half))
        faces.append(RoofFace((ex0, ey0, ze), (ex0, ey1, ze), d_l, tri))
        faces.append(RoofFace((ex1, ey1, ze), (ex1, ey0, ze), d_r, tri))
        zr = ze + inset * t
        ridge = [((rx0, ym, zr), (rx1, ym, zr))] if rx1 - rx0 > 1 else []
        hips = [((ex0, ey0, ze), (rx0, ym, zr)), ((ex0, ey1, ze), (rx0, ym, zr)),
                ((ex1, ey0, ze), (rx1, ym, zr)), ((ex1, ey1, ze), (rx1, ym, zr))]
    return faces, ridge, hips, ze, zr


def canal_tiles(m, face, rng, pitch_row=21.0, radius=7.2, tile_len=44.0, step=36.0, missing=.02, color=C_TILE):
    """Tuile canal : couvrants convexes en rangs, chaque tuile chevauchant la suivante (profil en ecailles)."""
    s = pitch_row / 2
    while s < face.L - 4:
        vmax = face.vmax(s)
        if vmax < 15:
            s += pitch_row
            continue
        v = -4.0
        row_tint = rng.uniform(.82, 1.14)
        while v < vmax - 6:
            ln = min(tile_len, vmax - v + 2)
            if rng.random() > missing:
                jit = rng.uniform(-1.6, 1.6)
                r0, r1 = radius * 1.08, radius * .9
                lift0, lift1 = 2.8, 1.0 + (tile_len - step) * .05
                segs = 5
                base = len(m.v)
                rings = []
                for e, (vv, r, lf) in enumerate(((v, r0, lift0), (v + ln, r1, lift1))):
                    ring = []
                    for k in range(segs + 1):
                        a = math.pi * k / segs
                        p = face.at(s + jit + r * math.cos(a), vv, lf + r * math.sin(a))
                        ring.append(len(m.v))
                        m.v.append(p)
                        m.c.append(tint(color, row_tint * rng.uniform(.9, 1.1)))
                        m.uv.append((k / segs * .5, vv / 100.0))
                    rings.append(ring)
                a, b = rings
                for k in range(segs):
                    m.t.extend([(a[k], a[k + 1], b[k]), (a[k + 1], b[k + 1], b[k])])
                # nez de la tuile : son epaisseur se lit a l'egout
                lip = [face.at(s + jit + r0 * math.cos(math.pi * k / segs), v, lift0 + r0 * math.sin(math.pi * k / segs))
                       for k in range(segs + 1)]
                inner = [face.at(s + jit + (r0 - 1.6) * math.cos(math.pi * k / segs), v, lift0 + (r0 - 1.6) * math.sin(math.pi * k / segs))
                         for k in range(segs, -1, -1)]
                m.poly(lip + inner, tint(color, .7), outward=mul(face.d, -1))
            v += step
        s += pitch_row
    # tuiles de dessous (courants) : bande sombre continue juste sous les couvrants
    m.poly([face.at(0, 0, 1.0), face.at(face.L, 0, 1.0)] +
           [face.at(face.L * k / 16, face.vmax(face.L * k / 16), 1.0) for k in range(16, -1, -1)],
           tint(color, .62), outward=face.n)


def board_roof(m, face, rng, color=C_PLANK, board=(18, 28), courses=2, stones=True):
    """Toit de planches pontique : deux rangs de planches chevauchants, lambourdes, pierres de lestage."""
    s = 0.0
    while s < face.L - 3:
        w = rng.uniform(*board)
        mid = s + w / 2
        vmax = face.vmax(mid)
        if vmax > 10:
            seg = vmax / courses
            for k in range(courses):
                v0 = k * seg - (0 if k == 0 else 12)
                v1 = (k + 1) * seg + rng.uniform(0, 6)
                v1 = min(v1, vmax)
                tilt = rng.uniform(-.6, .6)
                c = face.at(mid, (v0 + v1) / 2, 2.0 + k * 1.6 + tilt)
                m.obox(c, (face.u, face.d, face.n), (w / 2 - .5, (v1 - v0) / 2, 1.2),
                       tint(color, rng.uniform(.65, 1.15)), uaxis=face.d)
        s += w
    if stones:
        for frac in (.32, .74):
            v = face.vmax(face.L / 2) * frac
            a = face.at(4, v, 6)
            b = face.at(face.L - 4, v, 6)
            m.beam(a, b, 9, 7, tint(C_WOOD, .7), side=face.d)
            sx = 30.0
            while sx < face.L - 30:
                if face.vmax(sx) > v + 10:
                    m.lathe([(0, 0), (9, 1), (11, 6), (8, 11), (0, 13)], tint(C_STONE, rng.uniform(.8, 1.1)), 7,
                            center=face.at(sx, v + 9, 3), wear=.08)
                sx += rng.uniform(70, 140)


def roof(m, kind, x0, x1, y0, y1, z_plate, pitch, ov, cover, rng, rafter_every=62.0, ties=True,
         chimney=None, open_gable=None):
    """Charpente + couverture. Retourne (z_egout, z_faitage). Rect = sablieres hautes."""
    faces, ridge, hips, ze, zr = _roof_faces(kind, x0, x1, y0, y1, z_plate, pitch, ov)
    m.meta['roofs'].append(dict(kind=kind, cover=cover, pitch=pitch, overhang=ov, z_eave=ze, z_ridge=zr,
                                lo=[x0 - ov, y0 - ov, ze], hi=[x1 + ov, y1 + ov, zr]))
    # voligeage : dessus (sous la couverture) et sous-face (lue depuis l'interieur et sous le debord)
    for f in faces:
        out = f.outline()
        m.poly(out, tint(C_PLANK, .8), outward=f.n)
        m.poly([add(p, mul(f.n, -2.6)) for p in out], tint(C_PLANK, rng.uniform(.75, .95)), outward=mul(f.n, -1))
        # chevrons sous le voligeage, saillants au debord
        s = rafter_every / 2
        while s < f.L:
            vm = f.vmax(s)
            if vm > 25:
                a = f.at(s, -6, -9)
                b = f.at(s, vm - 4, -9)
                m.beam(a, b, 9, 12, tint(C_WOOD, rng.uniform(.8, 1.1)), side=f.u)
            s += rafter_every
        # rive d'egout
        m.beam(f.at(0, 1, -4), f.at(f.L, 1, -4), 3, 14, tint(C_PLANK, .75), side=f.d)
    for f in faces:
        if cover == 'tile':
            canal_tiles(m, f, rng)
        else:
            board_roof(m, f, rng)
    # faitage et aretiers
    for a, b in ridge:
        m.beam(add(a, (0, 0, -20)), add(b, (0, 0, -20)), 16, 20, tint(C_WOOD, .8))
        if cover == 'tile':
            L = length(sub(b, a))
            k = 0.0
            while k < L:
                p0 = lerp(a, b, k / L)
                p1 = lerp(a, b, min(1.0, (k + 46) / L))
                m.tube([add(p0, (0, 0, 4)), add(p1, (0, 0, 5))], [12, 10.5], tint(C_TILE, rng.uniform(.75, 1.0)), 7, cap=True)
                k += 40
        else:
            m.beam(add(a, (0, 0, 5)), add(b, (0, 0, 5)), 30, 4, tint(C_PLANK, .7))
    for a, b in hips:
        m.beam(add(a, (0, 0, -16)), add(b, (0, 0, -16)), 12, 18, tint(C_WOOD, .75))
        if cover == 'tile':
            L = length(sub(b, a))
            k = 0.0
            while k < L - 10:
                p0 = lerp(a, b, k / L)
                p1 = lerp(a, b, min(1.0, (k + 46) / L))
                m.tube([add(p0, (0, 0, 6)), add(p1, (0, 0, 7))], [11, 9.5], tint(C_TILE, rng.uniform(.75, 1.0)), 7, cap=True)
                k += 40
    # entraits (lus depuis l'interieur)
    if ties:
        ym = (y0 + y1) / 2
        n_t = max(2, int((x1 - x0) / 240))
        for k in range(n_t + 1):
            x = x0 + 20 + (x1 - x0 - 40) * k / n_t
            m.beam((x, y0 + 4, z_plate + 8), (x, y1 - 4, z_plate + 8), 16, 18, tint(C_WOOD, rng.uniform(.8, 1.0)))
            if kind == 'gable' or x0 + (y1 - y0) / 2 < x < x1 - (y1 - y0) / 2:
                m.beam((x, ym, z_plate + 16), (x, ym, zr - 26), 14, 14, tint(C_WOOD, .85))
    if chimney:
        cx, cy, z_from, w = chimney
        t_ = math.tan(math.radians(pitch))
        dy_ = min(cy - (y0 - ov), (y1 + ov) - cy)
        dx_ = min(cx - (x0 - ov), (x1 + ov) - cx) if kind == 'hip' else 1e9
        z_roof = ze + min(dx_, dy_) * t_
        top = min(zr, z_roof) + 95
        m.box((cx - w / 2, cy - w / 2, z_from), (cx + w / 2, cy + w / 2, top), tint(C_STONE, .9))
        face_stones(m, (cx - w / 2, cy + w / 2, 0), (cx + w / 2, cy + w / 2, 0), ze - 40, top - 4, 0.1, [], rng,
                    side=1, course=(14, 20), length_cm=(18, 30), offset=0)
        face_stones(m, (cx + w / 2, cy - w / 2, 0), (cx - w / 2, cy - w / 2, 0), ze - 40, top - 4, 0.1, [], rng,
                    side=1, course=(14, 20), length_cm=(18, 30), offset=0)
        m.box((cx - w / 2 - 6, cy - w / 2 - 6, top), (cx + w / 2 + 6, cy + w / 2 + 6, top + 6), tint(C_ASHLAR, .8))
        for s in (-1, 1):
            m.box((cx - w / 2 - 2, cy + s * (w / 2 - 4) - 4, top + 6), (cx + w / 2 + 2, cy + s * (w / 2 - 4) + 4, top + 26),
                  tint(C_BRICK, .9))
        m.box((cx - w / 2 - 8, cy - w / 2 - 8, top + 26), (cx + w / 2 + 8, cy + w / 2 + 8, top + 31), tint(C_ASHLAR, .75))
        m.box((cx - w / 2 + 6, cy - w / 2 + 6, top + 6.1), (cx + w / 2 - 6, cy + w / 2 - 6, top + 6.3), (0.02, 0.018, .016, IRON))
    return ze, zr


def gable_infill(m, x, y0, y1, z_plate, z_ridge, rng, kind='plank', thick=6.0, outward=1):
    """Triangle de pignon : planches verticales ou torchis, du plateau des sablieres au faitage."""
    ym = (y0 + y1) / 2
    half = (y1 - y0) / 2
    s = y0
    while s < y1 - 2:
        w = rng.uniform(18, 28)
        mid = min(s + w / 2, y1 - 1)
        top = z_plate + (z_ridge - z_plate) * (1 - abs(mid - ym) / half) - 6
        if top > z_plate + 4:
            c = (x, mid, (z_plate + top) / 2)
            col = C_PLANK if kind == 'plank' else C_DAUB
            m.obox(c, ((0, 1, 0), (outward, 0, 0), (0, 0, 1)), (w / 2 - .5, thick / 2, (top - z_plate) / 2),
                   tint(col, rng.uniform(.75, 1.15)), uaxis=(0, 0, 1))
        s += w


# --- interieur et vie --------------------------------------------------------------------

def floor_planks(m, x0, x1, y0, y1, z, rng, joists=True, board=(18, 26), along='x'):
    s = y0 if along == 'x' else x0
    end = y1 if along == 'x' else x1
    while s < end - 2:
        w = min(rng.uniform(*board), end - s)
        if along == 'x':
            m.box((x0, s + .4, z - 3.5), (x1, s + w - .4, z), tint(C_PLANK, rng.uniform(.75, 1.15)), uaxis=(1, 0, 0))
        else:
            m.box((s + .4, y0, z - 3.5), (s + w - .4, y1, z), tint(C_PLANK, rng.uniform(.75, 1.15)), uaxis=(0, 1, 0))
        s += w
    if joists:
        k = x0 + 30 if along == 'x' else y0 + 30
        while k < (x1 if along == 'x' else y1) - 20:
            if along == 'x':
                m.beam((k, y0, z - 13), (k, y1, z - 13), 14, 18, tint(C_WOOD, rng.uniform(.8, 1.05)))
            else:
                m.beam((x0, k, z - 13), (x1, k, z - 13), 14, 18, tint(C_WOOD, rng.uniform(.8, 1.05)))
            k += 60


def earth_floor(m, x0, x1, y0, y1, z, rng):
    m.box((x0, y0, z - 10), (x1, y1, z), tint(C_EARTH, rng.uniform(.9, 1.05)))


def hearth(m, x, y, z, yaw, rng, hood=True):
    """Foyer en pierre adosse : sole surelevee, contrecoeur, chaudron pendu. yaw = direction de la piece."""
    f = yaw_vec(yaw)
    s = (-f[1], f[0], 0.0)
    m.obox((x, y, z + 14), (s, f, (0, 0, 1)), (62, 42, 14), tint(C_ASHLAR, .8))
    m.obox(add((x, y, z + 29), mul(f, -2)), (s, f, (0, 0, 1)), (52, 34, 1.2), (0.03, .028, .025, EARTH))
    for k in (-1, 1):
        m.obox(add(add((x, y, z + 40), mul(s, k * 48)), mul(f, -6)), (s, f, (0, 0, 1)), (8, 26, 12), tint(C_STONE, .7))
    if hood:
        back = add((x, y, z), mul(f, -46))
        m.obox(add(back, (0, 0, 110)), (s, f, (0, 0, 1)), (70, 6, 110), tint(C_STONE, .65))
    pot = add((x, y, z), mul(f, 4))
    m.tube([add(pot, (0, 0, 190)), add(pot, (0, 0, 68))], .9, C_IRON, 4, cap=False)
    m.lathe([(2, 40), (13, 41), (17, 48), (18, 58), (16, 66), (14, 68)], C_IRON, 12, center=pot)
    m.meta['hearths'].append(dict(at=[x, y, z + 40], yaw=yaw))


def sedir(m, x0, x1, y0, y1, z, rng, color=C_WOOL):
    """Banquette-lit maconnee et planchee (sommeil et veillee), avec natte et couvertures."""
    m.box((x0, y0, z), (x1, y1, z + 38), tint(C_PLANK, rng.uniform(.8, 1.0)), uaxis=(1, 0, 0))
    m.box((x0 + 3, y0 + 3, z + 38), (x1 - 3, y1 - 3, z + 46), tint(C_LINEN, rng.uniform(.8, 1.0)))
    L = max(x1 - x0, y1 - y0)
    along_x = (x1 - x0) >= (y1 - y0)
    k = 0.0
    while k < L - 80:
        if along_x:
            m.box((x0 + k + 10, y0 + 6, z + 46), (x0 + k + rng.uniform(60, 90), y1 - 8, z + 53),
                  tint(color, rng.uniform(.6, 1.2)))
        else:
            m.box((x0 + 6, y0 + k + 10, z + 46), (x1 - 8, y0 + k + rng.uniform(60, 90), z + 53),
                  tint(color, rng.uniform(.6, 1.2)))
        k += rng.uniform(110, 160)
        m.meta['sleep'].append(dict(at=[x0 + (k if along_x else (x1 - x0) / 2), y0 + ((y1 - y0) / 2 if along_x else k), z + 46]))


def pithos(m, x, y, z, rng, h=110.0, sunk=0.0):
    """Jarre de stockage (pithos), eventuellement enterree."""
    s = h / 110.0
    prof = [(6 * s, 0), (24 * s, 8 * s), (36 * s, 34 * s), (38 * s, 58 * s), (32 * s, 84 * s), (20 * s, 100 * s),
            (17 * s, 104 * s), (20 * s, 110 * s), (15 * s, 111 * s), (12 * s, 104 * s)]
    m.lathe([(r, zz - sunk) for r, zz in prof], tint(C_CLAY, rng.uniform(.8, 1.1)), 14, center=(x, y, z), wear=.012)
    m.meta['storage'].append(dict(kind='pithos', at=[x, y, z], litres=round(300 * s ** 3)))


def amphora(m, x, y, z, rng, lean=0.0):
    m.lathe([(2, 0), (8, 6), (15, 20), (17, 36), (14, 52), (7, 62), (5, 70), (7, 74), (4, 75)],
            tint(C_CLAY, rng.uniform(.8, 1.15)), 10, center=(x, y, z), wear=.01)


def sack(m, x, y, z, rng):
    m.lathe([(0, 0), (22, 2), (26, 16), (24, 34), (16, 48), (6, 56), (0, 57)], tint(C_LINEN, rng.uniform(.65, .9)), 9,
            center=(x, y, z), wear=.06)


def basket(m, x, y, z, rng, r=24):
    m.lathe([(0, 0), (r * .8, 1), (r, 10), (r * 1.08, 30), (r * 1.02, 32), (r * .96, 30)], tint(C_STRAW, rng.uniform(.7, 1.0)),
            12, center=(x, y, z), wear=.02)


def chest(m, x, y, z, yaw, rng, w=110, d=56, h=58):
    f = yaw_vec(yaw)
    s = (-f[1], f[0], 0)
    m.obox((x, y, z + h / 2), (s, f, (0, 0, 1)), (w / 2, d / 2, h / 2), tint(C_PLANK, rng.uniform(.6, .9)), uaxis=s)
    m.obox((x, y, z + h + 2), (s, f, (0, 0, 1)), (w / 2 + 2, d / 2 + 2, 3), tint(C_PLANK, .7), uaxis=s)
    for k in (-1, 1):
        m.obox(add((x, y, z + h / 2), mul(s, k * w * .3)), (s, f, (0, 0, 1)), (3, d / 2 + .6, h / 2 + 2), C_IRON)
    m.meta['storage'].append(dict(kind='chest', at=[x, y, z]))


def low_table(m, x, y, z, rng):
    m.lathe([(0, 0), (16, 0), (14, 4), (14, 26), (46, 27), (48, 31), (0, 32)], tint(C_PLANK, rng.uniform(.7, .9)), 14,
            center=(x, y, z))


def shelf(m, a, b, z, depth, rng, items=True):
    u = norm(sub(b, a))
    n = (u[1], -u[0], 0)
    L = length(sub(b, a))
    c = add(lerp(a, b, .5), (0, 0, z))
    m.obox(c, (u, n, (0, 0, 1)), (L / 2, depth / 2, 2.2), tint(C_PLANK, .75), uaxis=u)
    for k in (.08, .92):
        p = add(lerp(a, b, k), (0, 0, z - 12))
        m.obox(p, (u, n, (0, 0, 1)), (2, depth / 2 - 2, 10), tint(C_WOOD, .7))
    if items:
        k = 14.0
        while k < L - 14:
            p = add(lerp(a, b, k / L), (0, 0, z + 2.2))
            if rng.random() < .55:
                m.lathe([(0, 0), (7, 0), (9, 6), (8, 14), (5, 18), (6, 20)], tint(C_CLAY, rng.uniform(.7, 1.1)), 8, center=p)
            elif rng.random() < .5:
                m.lathe([(0, 0), (10, 0), (12, 5), (12, 7), (0, 7)], tint(C_CLAY, rng.uniform(.7, 1.0)), 9, center=p)
            k += rng.uniform(18, 32)


def ladder(m, a, b, rng, w=44):
    u = norm(sub(b, a))
    side = norm(cross((0, 0, 1), u)) if abs(u[2]) < .99 else (1, 0, 0)
    for s in (-1, 1):
        m.beam(add(a, mul(side, s * w / 2)), add(b, mul(side, s * w / 2)), 6, 7, tint(C_WOOD, rng.uniform(.8, 1.0)))
    L = length(sub(b, a))
    k = 25.0
    while k < L - 10:
        p = lerp(a, b, k / L)
        m.beam(add(p, mul(side, -w / 2)), add(p, mul(side, w / 2)), 4, 4, tint(C_WOOD, .9))
        k += 30


def woodpile(m, x0, y0, z, length_x, depth, height, rng, yaw=0.0, roofed=False):
    """Bois de chauffage fendu, empile en rangs, buches de longueur reelle (50 cm)."""
    sub_m = Mesh()
    r = 7.0
    zz = 0.0
    row = 0
    while zz < height - r:
        x = r + (row % 2) * r
        while x < length_x - r:
            rr = r * rng.uniform(.75, 1.2)
            y_a = rng.uniform(-3, 3)
            sub_m.tube([(x, y_a, zz + rr), (x + rng.uniform(-2, 2), depth + y_a, zz + rr)], rr,
                       tint(C_WOOD, rng.uniform(.9, 1.5)), 6, cap=True)
            x += 2 * r * rng.uniform(.95, 1.1)
        zz += 2 * r * .9
        row += 1
    if roofed:
        sub_m.box((-10, -10, height + 6), (length_x + 10, depth + 20, height + 9), tint(C_PLANK, .7))
    m.extend(sub_m, yaw, (x0, y0, z), meta=False)


def wattle_fence(m, pts, rng, h=120.0, post_every=55.0, gaps=()):
    """Clayonnage : piquets et baguettes tressees, suit une polyligne ; gaps = (indice segment, s0, s1)."""
    for i, (a, b) in enumerate(zip(pts, pts[1:])):
        u, n, L = _wall_axes(a, b)
        holes = [(g[1], g[2]) for g in gaps if g[0] == i]
        posts = []
        s = 0.0
        while s <= L + .1:
            if not any(h0 < s < h1 for h0, h1 in holes):
                posts.append(s)
                p = add(a, mul(u, s))
                top = h + rng.uniform(-6, 14)
                m.tube([add(p, (0, 0, -20)), add(p, (rng.uniform(-2, 2), rng.uniform(-2, 2), top))], 3.4,
                       tint(C_WOOD, rng.uniform(.8, 1.2)), 5, cap=True)
            s += post_every
        for k in range(int(h / 9)):
            z = 14 + k * 9
            spans = []
            cur = 0.0
            for h0, h1 in sorted(holes):
                spans.append((cur, h0))
                cur = h1
            spans.append((cur, L))
            for s0, s1 in spans:
                if s1 - s0 < 20:
                    continue
                pts_w = []
                steps = max(2, int((s1 - s0) / (post_every / 2)))
                for j in range(steps + 1):
                    s = s0 + (s1 - s0) * j / steps
                    weave = 2.6 * (1 if (j + k) % 2 else -1)
                    pts_w.append(add(add(a, mul(u, s)), add(mul(n, weave), (0, 0, z + rng.uniform(-1, 1)))))
                m.tube(pts_w, 1.9, tint(C_WOOD, rng.uniform(1.0, 1.5)), 4, cap=False)


def drystone_wall(m, pts, rng, h=130.0, thick=55.0, gaps=(), coping=True):
    """Mur de cour en pierre seche, couronne de pierres posees de chant."""
    for i, (a, b) in enumerate(zip(pts, pts[1:])):
        u, n, L = _wall_axes(a, b)
        openings = [(g[1], g[2], -10, h + 40) for g in gaps if g[0] == i]
        hh = h + rng.uniform(-10, 10)
        wall_core(m, a, b, -20, hh, thick, openings, C_MORTAR)
        for side in (1, -1):
            face_stones(m, a, b, -10, hh, thick, openings, rng, side=side, course=(16, 30), length_cm=(26, 60),
                        proud=(1.0, 4.0))
        if coping:
            s = 0.0
            while s < L - 8:
                w = rng.uniform(10, 18)
                if not any(o[0] - 5 < s < o[1] + 5 for o in openings):
                    c = add(add(a, mul(u, s + w / 2)), (0, 0, hh + 12))
                    m.obox(c, (u, n, (0, 0, 1)), (w / 2 - 1, thick / 2 - 4, rng.uniform(10, 15)),
                           tint(C_STONE, rng.uniform(.75, 1.1)))
                s += w


def gate(m, p, u, w, rng, h=200.0, roofed=True):
    """Portail de cour charretier : poteaux, linteau, auvent de tuiles, deux battants entrouverts."""
    n = (u[1], -u[0], 0)
    for s in (-1, 1):
        a = add(p, mul(u, s * (w / 2 + 12)))
        m.beam(add(a, (0, 0, -30)), add(a, (0, 0, h + 30)), 22, 22, tint(C_WOOD, rng.uniform(.75, .9)), side=u)
    m.beam(add(add(p, mul(u, -w / 2 - 50)), (0, 0, h + 34)), add(add(p, mul(u, w / 2 + 50)), (0, 0, h + 34)), 24, 20,
           tint(C_WOOD, .8), side=n)
    if roofed:
        rm = Mesh()
        roof(rm, 'gable', -w / 2 - 30, w / 2 + 30, -30, 30, h + 44, 30.0, 50.0, 'tile', rng, rafter_every=50, ties=False)
        rm.meta = Mesh().meta
        yaw = math.degrees(math.atan2(u[1], u[0]))
        m.extend(rm, yaw, p, meta=False)
    for k, s in enumerate((-1, 1)):
        hinge = add(p, mul(u, s * w / 2))
        ang = math.radians(rng.uniform(15, 45))
        du = add(mul(u, -s * math.cos(ang)), mul(n, -math.sin(ang)))
        nl = cross((0, 0, 1), du)
        for b in range(int(w / 2 / 20)):
            c = add(hinge, add(mul(du, 20 * b + 10), (0, 0, 90 + rng.uniform(-3, 3))))
            m.obox(c, (du, nl, (0, 0, 1)), (9.5, 1.6, 80), tint(C_PLANK, rng.uniform(.7, 1.05)), uaxis=(0, 0, 1))
        for zz in (40, 140):
            m.obox(add(hinge, add(mul(du, w / 4), add(mul(nl, -3), (0, 0, zz)))), (du, nl, (0, 0, 1)), (w / 4 - 3, 1.6, 7),
                   tint(C_PLANK, .75))
    m.meta['doors'].append(dict(name='gate', at=list(p), width=w, clear=h, floor=0.0,
                                yaw=math.degrees(math.atan2(n[1], n[0])), wide=True))


def bread_oven(m, x, y, z, yaw, rng):
    """Four a pain domestique : socle de pierre, coupole enduite, bouche cintree, petit auvent."""
    f = yaw_vec(yaw)
    s = (-f[1], f[0], 0)
    m.obox((x, y, z + 45), (s, f, (0, 0, 1)), (100, 100, 45), tint(C_STONE, .9))
    face_stones(m, add((x, y, 0), add(mul(s, -100), mul(f, 100))), add((x, y, 0), add(mul(s, 100), mul(f, 100))),
                z, z + 88, 0.1, [], rng, side=1, offset=0)
    m.lathe([(92, 0), (90, 26), (82, 52), (64, 78), (38, 96), (12, 104), (0, 105)], tint(C_DAUB, 1.05), 18,
            center=(x, y, z + 90), wear=.02)
    mouth = add((x, y, z + 90), mul(f, 84))
    m.obox(add(mouth, (0, 0, 22)), (s, f, (0, 0, 1)), (26, 8, 22), tint(C_ASHLAR, .85))
    m.obox(add(add(mouth, mul(f, 8.2)), (0, 0, 20)), (s, f, (0, 0, 1)), (17, .3, 17), (0.015, .013, .012, IRON))
    m.meta['work'].append(dict(kind='oven', at=[x + f[0] * 150, y + f[1] * 150, z], yaw=yaw))


def chopping_block(m, x, y, z, rng):
    m.lathe([(24, 0), (25, 20), (23, 44), (0, 45)], tint(C_WOOD, 1.3), 10, center=(x, y, z), cap_top=False)
    m.beam((x + 6, y - 4, z + 46), (x + 30, y + 30, z + 80), 3.4, 3.4, tint(C_WOOD, 1.1))
    m.obox((x + 6, y - 4, z + 47), ((.6, .8, 0), (-.8, .6, 0), (0, 0, 1)), (8, 1.2, 5), C_IRON)
    m.meta['work'].append(dict(kind='chop', at=[x, y + 70, z]))


def trough(m, x, y, z, yaw, rng, L=200.0):
    f = yaw_vec(yaw)
    s = (-f[1], f[0], 0)
    for k in (-1, 1):
        m.obox(add((x, y, z + 32), mul(f, k * 26)), (s, f, (0, 0, 1)), (L / 2, 8, 32), tint(C_ASHLAR, rng.uniform(.8, .95)))
        m.obox(add((x, y, z + 32), mul(s, k * (L / 2 - 8))), (s, f, (0, 0, 1)), (8, 18, 32), tint(C_ASHLAR, .85))
    m.obox((x, y, z + 6), (s, f, (0, 0, 1)), (L / 2, 34, 6), tint(C_ASHLAR, .8))
    m.obox((x, y, z + 44), (s, f, (0, 0, 1)), (L / 2 - 16, 18, .5), (0.02, 0.03, 0.03, PLASTER))


def haystack(m, x, y, z, rng, r=150.0, h=330.0):
    m.lathe([(r * .82, 0), (r, h * .25), (r * .96, h * .5), (r * .7, h * .72), (r * .35, h * .9), (12, h)],
            tint(C_STRAW, rng.uniform(.8, 1.0)), 16, center=(x, y, z), wear=.04)
    m.tube([(x, y, z + h - 40), (x + 4, y, z + h + 70)], 4, tint(C_WOOD, .9), 5)


# ============================================================================================
# GRAMMAIRE : maisonnees completes
# ============================================================================================

def _rect_walls(x0, x1, y0, y1):
    """Quatre murs en sens horaire vu du dessus : la normale (droite de la marche) pointe dehors."""
    return {'front': ((x1, y1, 0), (x0, y1, 0)), 'back': ((x0, y0, 0), (x1, y0, 0)),
            'left': ((x0, y1, 0), (x0, y0, 0)), 'right': ((x1, y0, 0), (x1, y1, 0))}


def house_poor(seed=11):
    """Maison pauvre : une piece unique 6,4 x 5,2 m. Soubassement de pierre seche, pan de bois et torchis,
    toit de planches lestees, foyer au sol, couchage sur banquette, appentis a bois, courette de clayonnage."""
    rng = random.Random(seed)
    m = Mesh()
    x0, x1, y0, y1 = -320.0, 320.0, -470.0, 50.0
    T = 50.0
    socle = 95.0
    plate = 265.0
    floor = 18.0
    W = _rect_walls(x0, x1, y0, y1)
    door_s = 170.0
    dw, dh = 96.0, 192.0
    openings_front = [(door_s, door_s + dw, floor, floor + dh), (430, 492, 120, 178)]
    openings_back = [(250, 300, 128, 176)]
    # soubassement en pierre seche (a hauteur de genou), murs de bois au-dessus
    for key, (a, b) in W.items():
        ops = openings_front if key == 'front' else openings_back if key == 'back' else []
        rubble_wall(m, a, b, -15, socle, T, [o for o in ops if o[2] < socle], rng, render=0.0,
                    inner=C_MORTAR, quoin_ends=(True, False))
        timber_frame_wall(m, add(a, (0, 0, 0)), b, socle, plate, 20.0, ops, rng, infill=C_DAUB,
                          render_patches=.35, post_every=140)
    u = (-1.0, 0.0, 0.0)
    door(m, (x1 - door_s - dw / 2, y1 + T / 2, 0), u, (0, 1, 0), dw, dh, floor, rng, thick=T, leaf_open=72)
    window(m, (x1 - 461, y1 + 10, 0), u, (0, 1, 0), 56, 56, 120, rng, thick=20)
    window(m, (x0 + 275, y0 - 10, 0), (1, 0, 0), (0, -1, 0), 46, 46, 128, rng, thick=20, open_angles=(30, 95))
    earth_floor(m, x0 + T / 2, x1 - T / 2, y0 + T / 2, y1 - T / 2, floor, rng)
    m.meta['floors'].append(dict(z=floor, kind='earth'))
    # toit de planches a deux pans, faitage le long de X
    ze, zr = roof(m, 'gable', x0, x1, y0, y1, plate, 32.0, 72.0, 'board', rng)
    for x, o in ((x0, -1), (x1, 1)):
        gable_infill(m, x, y0, y1, plate, zr - 18, rng, kind='plank', outward=o)
    m.meta['ceilings'].append(dict(room='piece', z=plate + 8 - 9, floor=floor))
    # interieur : foyer au sol contre le mur du fond, banquette, jarres, coffre, etagere
    hearth(m, -60, y0 + T / 2 + 48, floor, 90, rng, hood=False)
    sedir(m, x0 + T / 2 + 2, x0 + T / 2 + 92, y0 + T / 2 + 2, y1 - T / 2 - 80, floor, rng)
    pithos(m, x1 - 70, y0 + 80, floor, rng, h=96, sunk=18)
    amphora(m, x1 - 120, y0 + 64, floor, rng)
    chest(m, x1 - 80, y1 - 110, floor, 180, rng, w=90)
    shelf(m, (x0 + 110, y0 + T / 2 + 6, 0), (x0 + 250, y0 + T / 2 + 6, 0), 150, 26, rng)
    basket(m, -170, -150, floor, rng)
    low_table(m, 10, -180, floor, rng)
    m.meta['rooms'].append(dict(name='piece', use='vie+sommeil+foyer', area_m2=round((x1 - x0 - 2 * T) * (y1 - y0 - 2 * T) / 1e4, 1),
                                floor=floor, ceiling=plate - 1))
    m.meta['work'].append(dict(kind='spin', at=[-100, -60, floor]))
    # appentis a bois sur le pignon droit
    lean_x0, lean_x1 = x1, x1 + 190
    for y in (y0 + 20, y1 - 20):
        m.beam((lean_x1 - 10, y, 0), (lean_x1 - 10, y, 178), 14, 14, tint(C_WOOD, rng.uniform(.7, .9)))
    m.beam((lean_x1 - 10, y0 + 10, 178), (lean_x1 - 10, y1 - 10, 178), 14, 16, tint(C_WOOD, .8))
    lean = RoofFace((lean_x1 + 30, y1 + 30, 165), (lean_x1 + 30, y0 - 30, 165), norm((-1, 0, .42)),
                    lambda s: (lean_x1 + 30 - lean_x0 + 10) / math.cos(math.atan(.42)))
    m.poly(lean.outline(4), tint(C_PLANK, .8), outward=lean.n)
    board_roof(m, lean, rng, courses=1, stones=False)
    woodpile(m, lean_x0 + 20, y0 + 40, 0, 140, 50, 150, rng, yaw=90)
    woodpile(m, lean_x0 + 90, y0 + 40, 0, 140, 50, 120, rng, yaw=90)
    # courette fermee de clayonnage, ouverte vers l'acces
    fence = [(x0 - 120, y1 + 80, 0), (x0 - 120, y1 + 560, 0), (x1 + 160, y1 + 560, 0), (x1 + 160, y1 + 80, 0)]
    wattle_fence(m, fence, rng, h=115, gaps=((1, 280, 420),))
    chopping_block(m, x1 + 60, y1 + 200, 0, rng)
    woodpile(m, x0 - 60, y1 + 280, 0, 180, 45, 90, rng, yaw=90)
    basket(m, x0 + 80, y1 + 160, 0, rng, r=30)
    m.meta['entry'] = [x1 - door_s - dw / 2, y1 + 160, 0]
    return m


def two_storey(m, rng, x0, x1, y0, y1, roof_kind='hip', cover='tile', gallery=True, stair_side='right',
               chimney_side='left', ground_use='etable'):
    """Maison a deux niveaux (reference) : rez de pierre (reserve, etable), etage a pan de bois enduit,
    galerie couverte en facade, escalier exterieur de pierre, foyer et cheminee, toit de tuile canal."""
    T = 60.0
    g_floor = 14.0
    g_top = 290.0         # dessus du plancher de l'etage = 300
    u_floor = 300.0
    plate = 555.0
    W = _rect_walls(x0, x1, y0, y1)
    width = x1 - x0
    # --- rez-de-chaussee en moellons ---
    gd_s = width * .5 - 70 if stair_side == 'right' else width * .5 - 70
    gdw, gdh = 140.0, 212.0
    ops = {
        'front': [(gd_s, gd_s + gdw, g_floor, g_floor + gdh), (width - 150, width - 128, 130, 200)],
        'back': [(150, 172, 130, 200), (width - 172, width - 150, 130, 200)],
        'left': [], 'right': [],
    }
    for key, (a, b) in W.items():
        rubble_wall(m, a, b, -15, g_top + 12, T, ops[key], rng, render=0.0, inner=C_MORTAR,
                    quoin_ends=(True, False), course=(20, 32))
    door(m, (x1 - gd_s - gdw / 2, y1 + T / 2, 0), (-1, 0, 0), (0, 1, 0), gdw, gdh, g_floor, rng, thick=T, leaf_open=48,
         name='rez', leaves=2, wide=True)
    for s0, s1, z0_, z1_ in ops['front'][1:]:
        window(m, (x1 - (s0 + s1) / 2, y1 + T / 2, 0), (-1, 0, 0), (0, 1, 0), s1 - s0, z1_ - z0_, z0_, rng, thick=T,
               shutters=False, bars=True)
    for s0, s1, z0_, z1_ in ops['back']:
        window(m, (x0 + (s0 + s1) / 2, y0 - T / 2, 0), (1, 0, 0), (0, -1, 0), s1 - s0, z1_ - z0_, z0_, rng, thick=T,
               shutters=False, bars=True)
    earth_floor(m, x0 + T / 2, x1 - T / 2, y0 + T / 2, y1 - T / 2, g_floor, rng)
    m.meta['floors'].append(dict(z=g_floor, kind='earth'))
    floor_planks(m, x0 + T / 2, x1 - T / 2, y0 + T / 2, y1 - T / 2, u_floor, rng, along='x')
    m.meta['floors'].append(dict(z=u_floor, kind='plank'))
    m.meta['ceilings'].append(dict(room='rez', z=u_floor - 22, floor=g_floor))
    # --- etage a pan de bois, remplissage enduit a la chaux ---
    uw = 100.0
    ud_s = width - 140 - uw if stair_side == 'right' else 140
    up_ops = {
        'front': [(ud_s, ud_s + uw, u_floor, u_floor + 200), (120, 186, 400, 488), (width * .5 - 30, width * .5 + 36, 400, 488)],
        'back': [(160, 222, 405, 485), (width - 230, width - 168, 405, 485)],
        'left': [(y1 - y0) / 2 - 30, ] and [((y1 - y0) / 2 - 28, (y1 - y0) / 2 + 28, 410, 480)],
        'right': [],
    }
    if chimney_side == 'left':
        up_ops['left'] = []
    for key, (a, b) in W.items():
        timber_frame_wall(m, a, b, g_top + 12, plate, 22.0, up_ops[key], rng, infill=C_PLASTER, render_patches=0.0,
                          post_every=130)
    door(m, (x1 - ud_s - uw / 2, y1 + 11, 0), (-1, 0, 0), (0, 1, 0), uw, 196, u_floor, rng, thick=22, leaf_open=80,
         name='etage')
    for s0, s1, z0_, z1_ in up_ops['front'][1:]:
        window(m, (x1 - (s0 + s1) / 2, y1 + 11, 0), (-1, 0, 0), (0, 1, 0), s1 - s0, z1_ - z0_, z0_, rng, thick=22)
    for s0, s1, z0_, z1_ in up_ops['back']:
        window(m, (x0 + (s0 + s1) / 2, y0 - 11, 0), (1, 0, 0), (0, -1, 0), s1 - s0, z1_ - z0_, z0_, rng, thick=22,
               open_angles=(160, 160))
    for s0, s1, z0_, z1_ in up_ops['left']:
        window(m, (x0 - 11, y1 - (s0 + s1) / 2, 0), (0, -1, 0), (-1, 0, 0), s1 - s0, z1_ - z0_, z0_, rng, thick=22)
    # --- galerie en facade : plancher en encorbellement, poteaux, garde-corps ---
    gy1 = y1 + 165 if gallery else y1
    if gallery:
        floor_planks(m, x0 - 10, x1 + 10, y1 + 11, gy1, u_floor, rng, joists=False, along='y')
        jx = x0 + 10
        while jx < x1:
            m.beam((jx, y1 - 40, u_floor - 12), (jx, gy1 + 6, u_floor - 12), 14, 18, tint(C_WOOD, rng.uniform(.75, .95)))
            jx += 70
        for px in [x0 + 10 + (x1 - x0 - 20) * k / 4 for k in range(5)]:
            m.beam((px, gy1 - 10, -10), (px, gy1 - 10, plate), 18, 18, tint(C_WOOD, rng.uniform(.7, .9)))
            m.box((px - 26, gy1 - 36, -15), (px + 26, gy1 + 16, 18), tint(C_ASHLAR, rng.uniform(.8, 1.0)))
            for s in (-1, 1):
                m.beam((px, gy1 - 10, plate - 70), (px + s * 60, gy1 - 10, plate - 8), 10, 12, tint(C_WOOD, .8))
        m.beam((x0 - 10, gy1 - 10, plate - 6), (x1 + 10, gy1 - 10, plate - 6), 20, 18, tint(C_WOOD, .8))
        m.beam((x0, gy1 - 6, u_floor + 95), (x1, gy1 - 6, u_floor + 95), 8, 10, tint(C_WOOD, .9))
        m.beam((x0, gy1 - 6, u_floor + 40), (x1, gy1 - 6, u_floor + 40), 6, 7, tint(C_WOOD, .9))
        bx = x0 + 12
        while bx < x1 - 10:
            stair_gap = (stair_side == 'right' and bx > x1 - 140) or (stair_side == 'left' and bx < x0 + 140)
            if not stair_gap:
                m.beam((bx, gy1 - 6, u_floor), (bx, gy1 - 6, u_floor + 95), 5, 5, tint(C_WOOD, rng.uniform(.8, 1.1)))
            bx += 18
        m.meta['work'].append(dict(kind='gallery', at=[(x0 + x1) / 2, (y1 + gy1) / 2, u_floor]))
    # --- escalier exterieur de pierre le long du pignon, palier au niveau de la galerie ---
    sx = x1 + 30 if stair_side == 'right' else x0 - 30
    sdir = -1 if stair_side == 'right' else 1
    steps = 15
    rise = u_floor / steps
    run = 30.0
    y_start = y0 + 70
    for k in range(steps):
        z_top = rise * (k + 1)
        ya = y_start + k * run
        m.box((min(sx, sx - sdir * 110), ya, -20), (max(sx, sx - sdir * 110), ya + run + 2, z_top),
              tint(C_STONE, rng.uniform(.8, 1.0)))
        m.box((min(sx, sx - sdir * 110) - 2, ya - 2, z_top - 6), (max(sx, sx - sdir * 110) + 2, ya + run + 4, z_top),
              tint(C_ASHLAR, rng.uniform(.8, 1.05)))
    land_y0 = y_start + steps * run
    m.box((min(sx, sx - sdir * 110), land_y0, -20), (max(sx, sx - sdir * 110), gy1, u_floor), tint(C_STONE, .9))
    outer = max(sx, sx - sdir * 110) if stair_side == 'right' else min(sx, sx - sdir * 110)
    for k in range(steps + 1):
        ya = y_start + k * run
        yb = ya + run if k < steps else gy1
        zt = rise * (k + 1) - 8 if k < steps else u_floor - 8
        if stair_side == 'right':
            face_stones(m, (outer, ya, 0), (outer, yb, 0), -15, zt, 0.0, [], rng, side=-1, course=(18, 28),
                        length_cm=(22, 40))
        else:
            face_stones(m, (outer, yb, 0), (outer, ya, 0), -15, zt, 0.0, [], rng, side=-1, course=(18, 28),
                        length_cm=(22, 40))
    m.box((min(sx, sx - sdir * 110) - 3, land_y0, u_floor - 6), (max(sx, sx - sdir * 110) + 3, gy1 + 4, u_floor + 1),
          tint(C_ASHLAR, .9))
    # --- toiture ---
    roof_y1 = gy1 + (0 if gallery else 0)
    ch = None
    if chimney_side == 'left':
        ch = (x0 + 70, (y0 + y1) / 2 - 40, u_floor, 62)
    elif chimney_side == 'back':
        ch = ((x0 + x1) / 2 - 120, y0 + 60, u_floor, 62)
    ze, zr = roof(m, roof_kind, x0, x1, y0, roof_y1, plate, 30.0, 70.0, cover, rng, chimney=ch)
    if roof_kind == 'gable':
        for x, o in ((x0, -1), (x1, 1)):
            gable_infill(m, x, y0, roof_y1, plate, zr - 20, rng, kind='plank', outward=o)
    # --- interieur de l'etage : piece de vie (foyer) et chambre, cloison de planches ---
    part_x = x0 + (x1 - x0) * .58
    plank_wall(m, (part_x, y0 + T / 2, 0), (part_x, y1 - 12, 0), u_floor, plate, 10.0,
               [((y1 - y0) * .5 - 60, (y1 - y0) * .5 + 30, u_floor, u_floor + 192)], rng)
    if ch:
        hx, hy = ch[0], ch[1]
        hearth(m, hx + 6 if chimney_side == 'left' else hx, hy if chimney_side == 'left' else hy + 50, u_floor,
               0 if chimney_side == 'left' else 90, rng, hood=True)
    sedir(m, x0 + 40, part_x - 20, y0 + T / 2 + 4, y0 + T / 2 + 90, u_floor, rng)
    sedir(m, part_x + 12, x1 - 20, y0 + T / 2 + 4, y0 + T / 2 + 100, u_floor, rng, color=C_LINEN)
    chest(m, x1 - 100, y1 - 70, u_floor, 180, rng)
    chest(m, part_x + 90, y1 - 70, u_floor, 180, rng, w=90)
    low_table(m, (x0 + part_x) / 2 + 20, (y0 + y1) / 2 + 20, u_floor, rng)
    shelf(m, (part_x - 20, y1 - 18, 0), (part_x - 200, y1 - 18, 0), u_floor + 150, 26, rng)
    shelf(m, (x0 + 30, y0 + 40, 0), (x0 + 30, y0 + 200, 0), u_floor + 165, 24, rng)
    living = (part_x - x0 - 22) * (y1 - y0 - 44) / 1e4
    m.meta['rooms'].append(dict(name='salle', use='vie+foyer+veillee', area_m2=round(living, 1), floor=u_floor, ceiling=plate - 1))
    m.meta['rooms'].append(dict(name='chambre', use='sommeil', area_m2=round((x1 - part_x - 22) * (y1 - y0 - 44) / 1e4, 1),
                                floor=u_floor, ceiling=plate - 1))
    m.meta['ceilings'].append(dict(room='etage', z=plate - 1, floor=u_floor))
    # --- rez : reserve et etable ---
    if ground_use == 'etable':
        m.box((x0 + 40, y0 + 40, g_floor), (x0 + 360, y0 + 110, g_floor + 70), tint(C_PLANK, .7))
        m.box((x0 + 46, y0 + 46, g_floor + 40), (x0 + 354, y0 + 104, g_floor + 66), tint(C_STRAW, .8))
        for k in range(4):
            m.box((x0 + 60 + k * 70, y0 + 140, g_floor), (x0 + 120 + k * 70, y0 + 230, g_floor + 8),
                  tint(C_STRAW, rng.uniform(.7, 1.0)))
        m.meta['work'].append(dict(kind='stable', at=[x0 + 200, y0 + 200, g_floor]))
    for k in range(5):
        pithos(m, x1 - 90 - (k % 3) * 85, y0 + 90 + (k // 3) * 90, g_floor, rng, h=rng.uniform(95, 125), sunk=20)
    for k in range(4):
        sack(m, x1 - 330 + k * 48, y1 - 80, g_floor, rng)
    basket(m, x0 + 400, y1 - 90, g_floor, rng)
    ladder(m, (x0 + 120, y1 - 70, g_floor), (x0 + 120, y1 - 220, u_floor + 10), rng)
    m.meta['rooms'].append(dict(name='rez', use=ground_use + '+reserve', area_m2=round((x1 - x0 - 2 * T) * (y1 - y0 - 2 * T) / 1e4, 1),
                                floor=g_floor, ceiling=u_floor - 22))
    return dict(plate=plate, z_ridge=zr, gallery_y=gy1)


def house_medium(seed=23):
    """Maison moyenne, reference architecturale : 9,6 x 6,8 m, deux niveaux, galerie, cour murée basse."""
    rng = random.Random(seed)
    m = Mesh()
    x0, x1, y0, y1 = -480.0, 480.0, -720.0, -40.0
    info = two_storey(m, rng, x0, x1, y0, y1, roof_kind='hip', cover='tile', stair_side='right', chimney_side='left')
    # cour en avant, mur de pierre seche bas avec entree, auge et four
    gy = info['gallery_y']
    court = [(x0 - 100, gy + 20, 0), (x0 - 100, 820, 0), (x1 + 160, 820, 0), (x1 + 160, y0 + 60, 0)]
    drystone_wall(m, court[:2], rng, h=110, thick=50)
    drystone_wall(m, [court[1], court[2], court[3]], rng, h=110, thick=50, gaps=((0, 330, 600),))
    gate(m, (x0 - 100 + 465, 820, 0), (1, 0, 0), 230, rng, h=205, roofed=False)
    bread_oven(m, x0 + 40, 580, 0, 0, rng)
    trough(m, x1 + 40, 520, 0, 90, rng)
    woodpile(m, x0 - 70, y0 + 60, 0, 300, 55, 160, rng, yaw=90, roofed=False)
    chopping_block(m, x0 + 260, 420, 0, rng)
    for k in range(3):
        amphora(m, x1 - 40 - k * 40, gy + 50, 0, rng)
    m.meta['entry'] = [x1 - 70 - 70, gy + 120, 0]
    return m


def house_farm(seed=37):
    """Ferme : maison a deux niveaux (toit a deux pans) + aile de grange-etable, cour close, four, meule."""
    rng = random.Random(seed)
    m = Mesh()
    hx0, hx1, hy0, hy1 = -800.0, 60.0, -760.0, -100.0
    info = two_storey(m, rng, hx0, hx1, hy0, hy1, roof_kind='gable', cover='tile', stair_side='right', chimney_side='back',
                      ground_use='reserve')
    # aile de grange : rez de pierre, fenil de planches, grand toit de planches lestees ; faitage dans le long
    bm = Mesh()
    gx0, gx1, gy0, gy1 = -570.0, 570.0, -270.0, 270.0   # repere local : long axe X, porte charretiere sur +Y
    T = 60.0
    ops = {'front': [(450, 690, 0, 255)], 'back': [(560, 590, 150, 210)], 'left': [], 'right': []}
    for key, (a, b) in _rect_walls(gx0, gx1, gy0, gy1).items():
        rubble_wall(bm, a, b, -15, 270, T, ops[key], rng, render=0.0, inner=C_MORTAR, quoin_ends=(True, False),
                    course=(22, 34))
    door(bm, (0, gy1 + T / 2, 0), (-1, 0, 0), (0, 1, 0), 240, 255, 0, rng, thick=T, leaf_open=55, name='grange',
         leaves=2, wide=True)
    for key, (a, b) in _rect_walls(gx0, gx1, gy0, gy1).items():
        plank_wall(bm, a, b, 270, 470, 12.0, [(420, 720, 290, 430)] if key == 'front' else [], rng)
    floor_planks(bm, gx0 + T / 2, gx1 - T / 2, gy0 + T / 2, gy1 - T / 2, 282, rng, along='x')
    ze, zr = roof(bm, 'gable', gx0, gx1, gy0, gy1, 470, 28.0, 80.0, 'board', rng)
    for x, o in ((gx0, -1), (gx1, 1)):
        gable_infill(bm, x, gy0, gy1, 470, zr - 18, rng, kind='plank', outward=o)
    for k in range(6):
        bm.box((gx0 + 60 + k * 170, gy0 + 50, 282), (gx0 + 200 + k * 170, gy0 + 230, 282 + rng.uniform(60, 130)),
               tint(C_STRAW, rng.uniform(.75, 1.0)))
    bm.box((gx0 + 60, gy0 + 60, 0), (gx0 + 480, gy0 + 120, 70), tint(C_PLANK, .7))
    bm.meta['rooms'].append(dict(name='grange', use='etable+fenil', area_m2=round((gx1 - gx0 - 2 * T) * (gy1 - gy0 - 2 * T) / 1e4, 1),
                                 floor=0.0, ceiling=260))
    bm.meta['storage'].append(dict(kind='hay', at=[0, 0, 282]))
    bx0, bx1, by0, by1 = 260.0, 800.0, -780.0, 360.0
    m.extend(bm, 90, ((bx0 + bx1) / 2, (by0 + by1) / 2, 0))
    m.meta['work'].append(dict(kind='stable', at=[bx0 - 160, (by0 + by1) / 2, 0]))
    # cour close de pierre seche, portail charretier couvert vers l'acces
    drystone_wall(m, [(hx0 - 40, hy1 + 60, 0), (hx0 - 40, 780, 0), (bx1, 780, 0), (bx1, by1 + 40, 0)], rng, h=150, thick=55,
                  gaps=((1, 760, 1040),))
    gate(m, (hx0 - 40 + 900, 780, 0), (1, 0, 0), 260, rng, h=215)
    bread_oven(m, hx0 + 140, 600, 0, 0, rng)
    haystack(m, -420, 520, 0, rng)
    trough(m, bx0 - 120, by1 + 20, 0, 0, rng)
    woodpile(m, hx1 + 20, hy0 + 40, 0, 320, 55, 170, rng, yaw=90, roofed=True)
    chopping_block(m, -120, 320, 0, rng)
    m.meta['entry'] = [hx0 - 40 + 900, 640, 0]
    return m


def storehouse(seed=53):
    """Grenier communautaire : rez de pierre a grande porte, fenil de planches, potence de levage ;
    serender pontique (grenier sur pilotis) dans la cour."""
    rng = random.Random(seed)
    m = Mesh()
    x0, x1, y0, y1 = -700.0, 420.0, -640.0, 160.0
    T = 65.0
    W = _rect_walls(x0, x1, y0, y1)
    width = x1 - x0
    ops = {'front': [(width / 2 - 120, width / 2 + 120, 12, 272), (110, 132, 140, 220), (width - 132, width - 110, 140, 220)],
           'back': [], 'left': [], 'right': [(330, 352, 140, 220)]}
    for key, (a, b) in W.items():
        rubble_wall(m, a, b, -15, 310, T, ops[key], rng, render=0.0, inner=C_MORTAR, quoin_ends=(True, False),
                    course=(22, 36))
    door(m, (x1 - width / 2, y1 + T / 2, 0), (-1, 0, 0), (0, 1, 0), 240, 260, 12, rng, thick=T, leaf_open=58,
         name='grenier', leaves=2, wide=True)
    for s0, s1, z0_, z1_ in ops['front'][1:]:
        window(m, (x1 - (s0 + s1) / 2, y1 + T / 2, 0), (-1, 0, 0), (0, 1, 0), s1 - s0, z1_ - z0_, z0_, rng, thick=T,
               shutters=False, bars=True)
    earth_floor(m, x0 + T / 2, x1 - T / 2, y0 + T / 2, y1 - T / 2, 12, rng)
    for key, (a, b) in W.items():
        plank_wall(m, a, b, 310, 560, 14.0, [(width / 2 - 70, width / 2 + 70, 330, 500)] if key == 'front' else [], rng)
    floor_planks(m, x0 + T / 2, x1 - T / 2, y0 + T / 2, y1 - T / 2, 322, rng, along='x')
    ze, zr = roof(m, 'gable', x0, x1, y0, y1, 560, 27.0, 80.0, 'tile', rng)
    for x, o in ((x0, -1), (x1, 1)):
        gable_infill(m, x, y0, y1, 560, zr - 18, rng, kind='plank', outward=o)
    # potence de levage au-dessus de la porte du fenil
    xm = x1 - width / 2
    m.beam((xm, y1 - 120, 540), (xm, y1 + 150, 540), 18, 20, tint(C_WOOD, .8))
    m.beam((xm, y1 + 10, 470), (xm, y1 + 110, 540), 12, 12, tint(C_WOOD, .8))
    m.lathe([(0, -6), (14, -5), (16, 0), (14, 5), (0, 6)], tint(C_WOOD, .7), 10, center=(xm, y1 + 135, 520))
    m.tube([(xm, y1 + 140, 520), (xm, y1 + 140, 300)], 1.4, C_ROPE, 5)
    # interieur : jarres alignees, sacs, mesures
    for k in range(10):
        pithos(m, x0 + 110 + (k % 5) * 150, y0 + 110 + (k // 5) * 120, 12, rng, h=rng.uniform(110, 135), sunk=30)
    for k in range(8):
        sack(m, x1 - 120 - (k % 4) * 52, y1 - 140 - (k // 4) * 55, 12, rng)
    for k in range(6):
        sack(m, x0 + 120 + k * 50, y0 + 120, 322, rng)
    ladder(m, (x1 - 120, y0 + 90, 12), (x1 - 120, y0 + 240, 330), rng)
    m.meta['rooms'].append(dict(name='reserve', use='stock commun', area_m2=round((width - 2 * T) * (y1 - y0 - 2 * T) / 1e4, 1),
                                floor=12, ceiling=300))
    m.meta['ceilings'].append(dict(room='reserve', z=300, floor=12))
    m.meta['work'].append(dict(kind='distribution', at=[x1 - width / 2, y1 + 160, 0]))
    # rampe de terre et de pierre devant la porte
    m.box((xm - 170, y1 + 30, -20), (xm + 170, y1 + 110, 10), tint(C_ASHLAR, .85))
    # serender : grenier pontique sur pilotis, rondelles anti-rongeurs
    sx, sy = 650.0, 440.0
    for dx in (-110, 110):
        for dy in (-140, 140):
            m.box((sx + dx - 20, sy + dy - 20, -10), (sx + dx + 20, sy + dy + 20, 30), tint(C_ASHLAR, .9))
            m.beam((sx + dx, sy + dy, 30), (sx + dx, sy + dy, 190), 18, 18, tint(C_WOOD, .8))
            m.lathe([(0, -3), (34, -2), (36, 0), (34, 2), (0, 3)], tint(C_STONE, .9), 10, center=(sx + dx, sy + dy, 160))
    sm = Mesh()
    floor_planks(sm, -140, 140, -170, 170, 205, rng, along='x')
    for key, (a, b) in _rect_walls(-130, 130, -160, 160).items():
        plank_wall(sm, a, b, 205, 370, 10.0, [(100, 160, 220, 340)] if key == 'front' else [], rng, board=(14, 20))
    roof(sm, 'gable', -130, 130, -160, 160, 370, 32.0, 60.0, 'board', rng, ties=False)
    m.extend(sm, 90, (sx, sy, 0), meta=False)
    ladder(m, (sx - 100, sy + 260, 0), (sx - 60, sy + 170, 205), rng)
    m.meta['storage'].append(dict(kind='serender', at=[sx, sy, 205]))
    m.meta['entry'] = [x1 - width / 2, y1 + 250, 0]
    return m


def well(seed=61):
    """Puits de village : margelle de pierre, treuil a manivelle sur montants, aire dallee, abreuvoir."""
    rng = random.Random(seed)
    m = Mesh()
    # aire dallee
    for k in range(70):
        a = rng.uniform(0, math.tau)
        r = math.sqrt(rng.uniform(0, 1)) * 250
        x, y = r * math.cos(a), r * math.sin(a)
        if r < 105:
            continue
        w = rng.uniform(30, 55)
        ang = rng.uniform(0, 180)
        m.obox((x, y, 2), (yaw_vec(ang), yaw_vec(ang + 90), (0, 0, 1)), (w / 2 - 1.5, w * .35 - 1.5, 3),
               tint(C_ASHLAR, rng.uniform(.75, 1.05)))
    m.lathe([(52, -10), (54, 66), (58, 84), (70, 92), (96, 94), (100, 88), (98, 50), (102, 6), (104, -10)],
            C_STONE, 28, wear=.02)
    m.lathe([(52, 60), (40, 60), (40, -120)], tint(C_MORTAR, .5), 20)
    for z in range(0, 80, 22):
        for k in range(14):
            a = math.tau * (k + (z // 22) * .5) / 14
            c = (101 * math.cos(a), 101 * math.sin(a), z + 11)
            m.obox(c, (yaw_vec(math.degrees(a) + 90), yaw_vec(math.degrees(a)), (0, 0, 1)), (19, 4, 9.5),
                   tint(C_STONE, rng.uniform(.75, 1.15)), skip=('-1',))
    for y in (-118, 118):
        m.beam((0, y, -20), (0, y, 228), 18, 18, tint(C_WOOD, rng.uniform(.7, .85)))
        m.box((-26, y - 26, -10), (26, y + 26, 20), tint(C_ASHLAR, .9))
    m.tube([(0, -128, 195), (0, 128, 195)], 15, tint(C_WOOD, .9), 10)
    m.beam((0, 128, 195), (0, 150, 195), 4, 4, C_IRON)
    m.beam((0, 150, 195), (0, 150, 160), 4, 4, C_IRON)
    m.beam((0, 150, 160), (0, 165, 160), 3, 3, tint(C_WOOD, .8))
    m.beam((-60, -140, 236), (-60, 140, 236), 10, 12, tint(C_WOOD, .7))
    m.beam((60, -140, 236), (60, 140, 236), 10, 12, tint(C_WOOD, .7))
    wr = RoofFace((-85, 160, 236), (-85, -160, 236), norm((1, 0, .5)), lambda s: 95)
    board_roof(m, wr, rng, courses=1, stones=False)
    wr2 = RoofFace((85, -160, 236), (85, 160, 236), norm((-1, 0, .5)), lambda s: 95)
    board_roof(m, wr2, rng, courses=1, stones=False)
    m.tube([(0, 0, 182), (0, 0, 70)], 1.4, C_ROPE, 5, cap=False)
    m.lathe([(10, 54), (16, 56), (19, 66), (19, 78), (15, 80)], tint(C_PLANK, .8), 12)
    trough(m, 0, -300, 0, 0, rng, L=240)
    amphora(m, 150, 90, 0, rng)
    amphora(m, 170, 50, 0, rng)
    m.meta['work'].append(dict(kind='draw_water', at=[0, 150, 0]))
    m.meta['entry'] = [0, 260, 0]
    return m


def workshop(seed=71):
    """Atelier de forgeron (laboratoire) : appentis ouvert sur la rue, forge, enclume, reserve de charbon."""
    rng = random.Random(seed)
    m = Mesh()
    x0, x1, y0, y1 = -360.0, 360.0, -300.0, 260.0
    T = 55.0
    W = _rect_walls(x0, x1, y0, y1)
    for key in ('back', 'left', 'right'):
        a, b = W[key]
        rubble_wall(m, a, b, -15, 290, T, [], rng, render=.0, inner=C_MORTAR, quoin_ends=(True, False))
    for px in (x0 + 20, 0, x1 - 20):
        m.beam((px, y1 - 10, 0), (px, y1 - 10, 300), 22, 22, tint(C_WOOD, .8))
        m.box((px - 26, y1 - 36, -10), (px + 26, y1 + 16, 18), tint(C_ASHLAR, .9))
    m.beam((x0 - 20, y1 - 10, 300), (x1 + 20, y1 - 10, 300), 22, 22, tint(C_WOOD, .8))
    earth_floor(m, x0 + T / 2, x1 - T / 2, y0 + T / 2, y1, 4, rng)
    roof(m, 'gable', x0, x1, y0, y1, 300, 26.0, 75.0, 'tile', rng, chimney=(-180, y0 + 70, 0, 70))
    m.box((-260, y0 + 30, 0), (-100, y0 + 150, 90), tint(C_STONE, .85))
    m.box((-240, y0 + 50, 90), (-120, y0 + 130, 92), (0.02, .018, .016, EARTH))
    m.lathe([(18, 0), (16, 50), (24, 56), (24, 64), (0, 66)], tint(C_WOOD, 1.2), 10, center=(-60, -40, 4))
    m.box((-90, -52, 70), (-30, -28, 84), C_IRON)
    m.box((150, y0 + 40, 0), (320, y0 + 120, 80), tint(C_PLANK, .7))
    for k in range(6):
        m.beam((160 + k * 25, y0 + 80, 82), (170 + k * 25, y0 + 80, 160), 3, 3, C_IRON)
    for k in range(5):
        sack(m, 200 + (k % 3) * 45, 120 + (k // 3) * 50, 4, rng)
    m.meta['work'].append(dict(kind='forge', at=[-60, 0, 4]))
    m.meta['rooms'].append(dict(name='atelier', use='forge', area_m2=round((x1 - x0) * (y1 - y0) / 1e4, 1), floor=4, ceiling=300))
    m.meta['doors'].append(dict(name='baie', at=[0, y1, 4], width=x1 - x0 - 40, clear=290, floor=4, yaw=90, wide=True))
    m.meta['ceilings'].append(dict(room='atelier', z=300, floor=4))
    m.meta['entry'] = [0, y1 + 200, 0]
    return m


def chapel(seed=83):
    """Chapelle a nef unique et abside (laboratoire) : appareil cloisonne pierre et brique, toit de tuile,
    clocher-arcade sur le pignon ouest. Echelle communautaire, fonction lisible de loin."""
    rng = random.Random(seed)
    m = Mesh()
    x0, x1, y0, y1 = -520.0, 520.0, -330.0, 330.0
    T = 65.0
    plate = 560.0
    W = _rect_walls(x0, x1, y0, y1)
    ops = {'right': [(260, 400, 10, 290)], 'front': [(300, 344, 330, 470), (700, 744, 330, 470)],
           'back': [(300, 344, 330, 470), (700, 744, 330, 470)], 'left': []}
    for key, (a, b) in W.items():
        wall_core(m, a, b, -15, plate, T, ops[key], C_MORTAR, inner_color=C_PLASTER)
        u, n, L = _wall_axes(a, b)
        z = 0.0
        while z < plate - 30:
            face_stones(m, a, b, z, z + 26, T, ops[key], rng, side=1, course=(24, 26), length_cm=(34, 46), proud=(1.0, 2.0),
                        color=C_ASHLAR, gap=9)
            z += 26
            if z < plate - 20:
                face_stones(m, a, b, z, z + 13, T, ops[key], rng, side=1, course=(5, 5), length_cm=(28, 32), proud=(1.2, 1.6),
                            color=C_BRICK, gap=2.5)
            z += 13
    door(m, (x1 + T / 2, 0, 0), (0, 1, 0), (1, 0, 0), 140, 280, 10, rng, thick=T, leaf_open=60, name='nef', leaves=2)
    # tympan cintre de brique au-dessus de la porte
    for k in range(13):
        a = math.pi * k / 12
        c = (x1 + T / 2 + 2, 70 * math.cos(a) * 1.2, 310 + 70 * math.sin(a))
        m.obox(c, ((0, -math.sin(a), math.cos(a)), (1, 0, 0), (0, math.cos(a), math.sin(a))), (5, 3, 16), tint(C_BRICK, rng.uniform(.8, 1.0)))
    for s0, s1, z0_, z1_ in ops['front']:
        window(m, (x1 - (s0 + s1) / 2, y1 + T / 2, 0), (-1, 0, 0), (0, 1, 0), s1 - s0, z1_ - z0_, z0_, rng, thick=T, shutters=False)
    for s0, s1, z0_, z1_ in ops['back']:
        window(m, (x0 + (s0 + s1) / 2, y0 - T / 2, 0), (1, 0, 0), (0, -1, 0), s1 - s0, z1_ - z0_, z0_, rng, thick=T, shutters=False)
    # abside semi-circulaire a l'est (-X)
    seg = 12
    for k in range(seg):
        a0 = math.pi / 2 + math.pi * k / seg
        a1 = math.pi / 2 + math.pi * (k + 1) / seg
        r = 250.0
        p0 = (x0 + r * math.cos(a0) * .9, r * math.sin(a0), 0)
        p1 = (x0 + r * math.cos(a1) * .9, r * math.sin(a1), 0)
        rubble_wall(m, p0, p1, -15, plate - 90, T, [], rng, render=0, inner=C_PLASTER, stones=False)
        u_, n_, L_ = _wall_axes(p0, p1)
        mid_ = lerp(p0, p1, .5)
        side_ = 1 if dot(n_, sub(mid_, (x0, 0, 0))) > 0 else -1
        z = 0.0
        while z < plate - 110:
            face_stones(m, p0, p1, z, z + 26, T, [], rng, side=side_, course=(24, 26), length_cm=(30, 40), color=C_ASHLAR, gap=9)
            if z + 26 < plate - 110:
                face_stones(m, p0, p1, z + 26, z + 39, T, [], rng, side=side_, course=(5, 5), length_cm=(28, 32),
                            proud=(1.2, 1.6), color=C_BRICK, gap=2.5)
            z += 39
    m.lathe([(225, plate - 90), (190, plate - 50), (120, plate - 10), (0, plate + 10)], C_TILE, 24, center=(x0, 0, 0))
    floor_planks(m, x0, x1 - T / 2, y0 + T / 2, y1 - T / 2, 10, rng, joists=False)
    roof(m, 'gable', x0, x1, y0, y1, plate, 26.0, 55.0, 'tile', rng)
    # clocher-arcade sur le pignon ouest
    bx = x1 + 10
    m.box((bx - 30, -120, plate + 150), (bx + 30, 120, plate + 400), tint(C_ASHLAR, .95))
    m.box((bx - 31, -55, plate + 260), (bx + 31, 55, plate + 370), (0.015, .013, .012, IRON))
    m.lathe([(2, 0), (22, 4), (26, 30), (16, 50), (0, 52)], tint((.30, .22, .10, IRON), 1.0), 12, center=(bx, 0, plate + 290))
    m.box((bx - 38, -132, plate + 400), (bx + 38, 132, plate + 412), tint(C_ASHLAR, .8))
    m.beam((bx, 0, plate + 412), (bx, 0, plate + 480), 8, 8, C_IRON)
    m.beam((bx, -26, plate + 460), (bx, 26, plate + 460), 7, 7, C_IRON)
    # iconostase basse et icone
    m.box((x0 + 140, -260, 10), (x0 + 160, 260, 230), tint(C_PLANK, .7))
    m.box((x0 + 161, -40, 120), (x0 + 163, 40, 210), (0.36, 0.27, 0.08, PLANK))
    m.meta['rooms'].append(dict(name='nef', use='culte+assemblee', area_m2=round((x1 - x0) * (y1 - y0 - 2 * T) / 1e4, 1),
                                floor=10, ceiling=plate - 1))
    m.meta['ceilings'].append(dict(room='nef', z=plate - 1, floor=10))
    m.meta['entry'] = [x1 + 300, 0, 0]
    return m


def footing(body, seed, margin=70.0):
    """Assise d'une maisonnee : plateforme de cour en terre battue + soutenement en pierre seche jusqu'a
    -FOOTING_DEPTH. Sur une pente, le cote aval devient une terrasse ; le cote amont s'enterre."""
    rng = random.Random(seed)
    lo, hi = body.bounds()
    lim = PARCEL / 2 - PARCEL_MARGIN - 12.0   # saillie des moellons et des chaperons comprise
    x0, y0 = max(lo[0] - margin, -lim), max(lo[1] - margin, -lim)
    x1, y1 = min(hi[0] + margin, lim), min(hi[1] + margin, lim)
    m = Mesh()
    top = -3.0   # la cour affleure : le terrain la borde, aucun rebord sur sol plat
    m.box((x0, y0, -FOOTING_DEPTH), (x1, y1, top), C_MORTAR, skip=('-2',))
    m.poly([(x0 + 1, y0 + 1, top + .2), (x1 - 1, y0 + 1, top + .2), (x1 - 1, y1 - 1, top + .2), (x0 + 1, y1 - 1, top + .2)],
           tint(C_EARTH, 1.0), outward=(0, 0, 1))
    for key, (a, b) in _rect_walls(x0, x1, y0, y1).items():
        # Parement du soutenement : visible seulement la ou le terrain descend (aval d'une pente).
        face_stones(m, a, b, -FOOTING_DEPTH + 10, top - 14, 0.0, [], rng, side=1, course=(26, 44), length_cm=(40, 90),
                    proud=(1.5, 5.0), color=C_STONE, gap=3.0)
    m.meta['footprint'] = [x0, y0, x1, y1]
    return m


BUILDINGS = {
    'SM_Arch_House_Poor_01': (house_poor, 'house', 1),
    'SM_Arch_House_Medium_01': (house_medium, 'house', 2),
    'SM_Arch_House_Farm_01': (house_farm, 'house', 3),
    'SM_Arch_Storehouse_01': (storehouse, 'granary', 1),
    'SM_Arch_Well_01': (well, 'well', 1),
    'SM_Arch_Workshop_01': (workshop, 'workshop', 1),
    'SM_Arch_Chapel_01': (chapel, 'chapel', 1),
}


# ============================================================================================
# KIT EXPORTE : pieces isolees pour le level design (meme materiau, meme echelle)
# ============================================================================================

def _kit(fn):
    rng = random.Random(hash(fn.__name__) & 0xffff)
    m = Mesh()
    fn(m, rng)
    return m


def kit_pieces():
    def wall_rubble(m, r): rubble_wall(m, (150, 0, 0), (-150, 0, 0), -10, 290, 60, [], r, quoin_ends=(False, False))
    def wall_rubble_door(m, r):
        rubble_wall(m, (150, 0, 0), (-150, 0, 0), -10, 290, 60, [(100, 200, 14, 214)], r)
        door(m, (0, 30, 0), (-1, 0, 0), (0, 1, 0), 100, 200, 14, r)
    def wall_rubble_window(m, r):
        rubble_wall(m, (150, 0, 0), (-150, 0, 0), -10, 290, 60, [(120, 180, 110, 180)], r)
        window(m, (0, 30, 0), (-1, 0, 0), (0, 1, 0), 60, 70, 110, r)
    def wall_rubble_corner(m, r):
        rubble_wall(m, (0, 0, 0), (-300, 0, 0), -10, 290, 60, [], r, quoin_ends=(True, False))
        rubble_wall(m, (0, -300, 0), (0, 0, 0), -10, 290, 60, [], r, quoin_ends=(False, True))
    def wall_rubble_plastered(m, r): rubble_wall(m, (150, 0, 0), (-150, 0, 0), -10, 290, 60, [], r, render=1.0, stones=False)
    def wall_timber(m, r): timber_frame_wall(m, (150, 0, 0), (-150, 0, 0), 0, 260, 22, [], r, infill=C_PLASTER)
    def wall_timber_daub(m, r): timber_frame_wall(m, (150, 0, 0), (-150, 0, 0), 0, 260, 22, [], r, infill=C_DAUB, render_patches=.4)
    def wall_timber_window(m, r):
        timber_frame_wall(m, (150, 0, 0), (-150, 0, 0), 0, 260, 22, [(115, 185, 100, 180)], r, infill=C_PLASTER)
        window(m, (0, 11, 0), (-1, 0, 0), (0, 1, 0), 70, 80, 100, r, thick=22)
    def wall_timber_door(m, r):
        timber_frame_wall(m, (150, 0, 0), (-150, 0, 0), 0, 260, 22, [(100, 200, 0, 200)], r, infill=C_PLASTER)
        door(m, (0, 11, 0), (-1, 0, 0), (0, 1, 0), 100, 196, 0, r, thick=22)
    def wall_plank(m, r): plank_wall(m, (150, 0, 0), (-150, 0, 0), 0, 260, 12, [], r)
    def wall_drystone(m, r): drystone_wall(m, [(-150, 0, 0), (150, 0, 0)], r)
    def fence_wattle(m, r): wattle_fence(m, [(-150, 0, 0), (150, 0, 0)], r)
    def roof_tile(m, r): roof(m, 'gable', -150, 150, -200, 200, 0, 30.0, 70.0, 'tile', r, ties=False)
    def roof_board(m, r): roof(m, 'gable', -150, 150, -200, 200, 0, 32.0, 70.0, 'board', r, ties=False)
    def floor_plank(m, r): floor_planks(m, -150, 150, -150, 150, 20, r)
    def stair_stone(m, r):
        for k in range(8):
            m.box((-55, k * 30, -10), (55, k * 30 + 32, (k + 1) * 20), tint(C_STONE, r.uniform(.8, 1.0)))
            m.box((-57, k * 30 - 2, (k + 1) * 20 - 6), (57, k * 30 + 34, (k + 1) * 20), tint(C_ASHLAR, r.uniform(.8, 1.0)))
    def hearth_piece(m, r): hearth(m, 0, 0, 0, 90, r)
    def oven(m, r): bread_oven(m, 0, 0, 0, 0, r)
    def gate_piece(m, r): gate(m, (0, 0, 0), (1, 0, 0), 240, r)
    def pithos_group(m, r):
        for k in range(3):
            pithos(m, k * 80 - 80, r.uniform(-20, 20), 0, r, h=r.uniform(95, 130))
    def woodpile_piece(m, r): woodpile(m, -100, 0, 0, 200, 50, 140, r, roofed=True)
    def chimney_piece(m, r): roof(m, 'gable', -150, 150, -150, 150, 0, 30.0, 60.0, 'tile', r, ties=False, chimney=(-80, -40, -100, 60))
    def door_leaf(m, r): door(m, (0, 0, 0), (1, 0, 0), (0, 1, 0), 100, 200, 0, r, leaf_open=0)
    return {
        'SM_Kit_Wall_Rubble_300': wall_rubble, 'SM_Kit_Wall_Rubble_Door_300': wall_rubble_door,
        'SM_Kit_Wall_Rubble_Window_300': wall_rubble_window, 'SM_Kit_Wall_Rubble_Corner': wall_rubble_corner,
        'SM_Kit_Wall_Rubble_Plastered_300': wall_rubble_plastered, 'SM_Kit_Wall_Timber_300': wall_timber,
        'SM_Kit_Wall_Timber_Daub_Repaired_300': wall_timber_daub, 'SM_Kit_Wall_Timber_Window_300': wall_timber_window,
        'SM_Kit_Wall_Timber_Door_300': wall_timber_door, 'SM_Kit_Wall_Plank_300': wall_plank,
        'SM_Kit_Wall_Drystone_300': wall_drystone, 'SM_Kit_Fence_Wattle_300': fence_wattle,
        'SM_Kit_Roof_Tile_300': roof_tile, 'SM_Kit_Roof_Board_300': roof_board, 'SM_Kit_Floor_Plank_300': floor_plank,
        'SM_Kit_Stair_Stone': stair_stone, 'SM_Kit_Hearth': hearth_piece, 'SM_Kit_Oven': oven, 'SM_Kit_Gate': gate_piece,
        'SM_Kit_Pithos_Group': pithos_group, 'SM_Kit_Woodpile': woodpile_piece, 'SM_Kit_Roof_Chimney': chimney_piece,
        'SM_Kit_Door_Closed': door_leaf,
    }


# ============================================================================================
# VALIDATION DE L'ECHELLE (ARCHITECTURE_SCALE_001)
# ============================================================================================

def check_building(name, body, foot):
    errors = []
    lo, hi = body.bounds()
    flo, fhi = foot.bounds()
    lim = PARCEL / 2 - PARCEL_MARGIN
    for v, label in ((lo[0], 'x-'), (lo[1], 'y-'), (-hi[0], 'x+'), (-hi[1], 'y+')):
        if v < -lim - 8.0:
            errors.append('ARCH-09 emprise %s depasse la parcelle (%.0f > %.0f)' % (label, -v, lim))
    for v in (flo[0], flo[1], -fhi[0], -fhi[1]):
        if v < -lim - 8.0:  # moellons en saillie du soutenement
            errors.append('ARCH-09 assise hors parcelle')
    if flo[2] > -FOOTING_DEPTH + 1:
        errors.append('ARCH-10 assise trop courte')
    for d in body.meta['doors']:
        if d['clear'] < DOOR_CLEAR_MIN:
            errors.append('ARCH-01 porte %s : %.0f cm libres < %.0f' % (d['name'], d['clear'], DOOR_CLEAR_MIN))
        if d['width'] < DOOR_WIDTH_MIN:
            errors.append('ARCH-02 porte %s : %.0f cm de large' % (d['name'], d['width']))
    for c in body.meta['ceilings']:
        if c['z'] - c['floor'] < CEILING_MIN:
            errors.append('ARCH-03 %s : %.0f cm sous plafond' % (c['room'], c['z'] - c['floor']))
    for w in body.meta['walls']:
        if w['kind'] == 'rubble' and not (STONE_WALL[0] <= w['thick'] <= STONE_WALL[1]):
            errors.append('ARCH-04 mur de pierre de %.0f cm' % w['thick'])
    for r in body.meta['roofs']:
        if not (PITCH_RANGE[0] <= r['pitch'] <= PITCH_RANGE[1]):
            errors.append('ARCH-05 pente %.0f' % r['pitch'])
        if not (OVERHANG_RANGE[0] <= r['overhang'] <= OVERHANG_RANGE[1]):
            errors.append('ARCH-06 debord %.0f' % r['overhang'])
    for w in body.meta['windows']:
        if w['width'] > 80 or w['sill'] < 90:
            errors.append('ARCH-07 fenetre %.0f cm, allege %.0f' % (w['width'], w['sill']))
    living = [r for r in body.meta['rooms'] if 'foyer' in r['use']]
    for r in living:
        if r['area_m2'] < LIVING_ROOM_MIN_M2:
            errors.append('ARCH-08 %s : %.1f m2' % (r['name'], r['area_m2']))
    if living and not body.meta['hearths']:
        errors.append('ARCH-08 piece a foyer sans foyer')
    return errors


def describe(name, body, foot, kind, tier):
    lo, hi = body.bounds()
    roofs = body.meta['roofs']
    return dict(
        name=name, kind=kind, tier=tier,
        size_cm=[round(hi[0] - lo[0]), round(hi[1] - lo[1]), round(hi[2] - lo[2])],
        bounds=[[round(v) for v in lo], [round(v) for v in hi]],
        footprint=[round(v) for v in foot.meta['footprint']],
        height_humans=round(hi[2] / HUMAN, 2),
        triangles=len(body.t), footing_triangles=len(foot.t),
        doors=[dict(name=d['name'], width=d['width'], clear=d['clear'], at=[round(v) for v in d['at']]) for d in body.meta['doors']],
        rooms=body.meta['rooms'], hearths=[[round(v) for v in h['at']] for h in body.meta['hearths']],
        sleep_slots=len(body.meta['sleep']), storage=len(body.meta['storage']),
        work=[dict(kind=w['kind'], at=[round(v) for v in w['at']]) for w in body.meta['work']],
        roof=[dict(kind=r['kind'], cover=r['cover'], pitch=r['pitch'], ridge=round(r['z_ridge'])) for r in roofs],
        entry=[round(v) for v in body.meta.get('entry', [0, 0, 0])],
        classes=sorted({CLASS_NAMES[c[3]] for c in body.c}),
    )


def geometry(names=None):
    out = {}
    for name, (fn, kind, tier) in BUILDINGS.items():
        if names and name not in names:
            continue
        body = fn()
        foot = footing(body, seed=len(name) * 7 + tier)
        out[name] = (body, foot, kind, tier)
    return out


def validate(write=True):
    report = dict(version=VERSION, convention=dict(human=HUMAN, parcel=PARCEL, parcel_margin=PARCEL_MARGIN,
                                                    door_clear_min=DOOR_CLEAR_MIN, door_width_min=DOOR_WIDTH_MIN,
                                                    ceiling_min=CEILING_MIN, pitch=PITCH_RANGE, overhang=OVERHANG_RANGE),
                  buildings=[], kit=[])
    failures = []
    for name, (body, foot, kind, tier) in geometry().items():
        errs = check_building(name, body, foot)
        d = describe(name, body, foot, kind, tier)
        d['errors'] = errs
        report['buildings'].append(d)
        failures += ['%s: %s' % (name, e) for e in errs]
        print('ARCH_GEOMETRY ' + json.dumps(dict(name=name, size_cm=d['size_cm'], tris=d['triangles'],
                                                 footing=d['footing_triangles'], doors=[(x['name'], x['clear']) for x in d['doors']],
                                                 errors=len(errs))))
        assert body.finish()['t'] and foot.finish()['t']
    for name, fn in kit_pieces().items():
        m = _kit(fn)
        lo, hi = m.bounds()
        report['kit'].append(dict(name=name, triangles=len(m.t), size_cm=[round(hi[i] - lo[i]) for i in range(3)]))
    if write:
        root = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
        out = os.path.join(root, 'docs', 'unreal', 'architecture', 'architecture-kit-001.json')
        os.makedirs(os.path.dirname(out), exist_ok=True)
        with open(out, 'w', encoding='utf-8') as f:
            json.dump(report, f, indent=1, ensure_ascii=False)
    for f in failures:
        print('ARCH_SCALE_FAIL ' + f)
    print('ARCH_GEOMETRY %s buildings=%d kit=%d failures=%d' % ('PASS' if not failures else 'FAIL',
                                                                len(report['buildings']), len(report['kit']), len(failures)))
    return report, failures


# ============================================================================================
# UNREAL
# ============================================================================================

MATERIAL_CODE = r"""
// M_AnastasisArchitecture : un seul materiau pour tout le bati. La classe vient de l'alpha du sommet,
// la teinte du RGB, le motif des UV planaires (metres), l'usure de la physique du batiment :
// remontee capillaire et rejaillissement au pied des murs, mousse et poussiere sur ce qui regarde le ciel,
// enduit qui tombe la ou l'eau le travaille, bois grise par le soleil. Neglect (0..1) = jours de vacance.
float cls = A * 10.0;
float isIron = step(cls, 1.0);
float isFiber = step(1.0, cls) * step(cls, 2.0);
float isEarth = step(2.0, cls) * step(cls, 3.0);
float isRubble = step(3.0, cls) * step(cls, 4.0);
float isAshlar = step(4.0, cls) * step(cls, 5.0);
float isPlaster = step(5.0, cls) * step(cls, 6.0);
float isDaub = step(6.0, cls) * step(cls, 7.0);
float isWood = step(7.0, cls) * step(cls, 8.0);
float isPlank = step(8.0, cls) * step(cls, 9.0);
float isTile = step(9.0, cls);
float2 uv = UV;
float3 p = WP / 100.0;
// bruit de valeur
#ifndef ARCH_H2
#define ARCH_H2 1
#define H2(q) frac(sin(dot(q, float2(127.1, 311.7))) * 43758.5453)
#endif
float2 i0 = floor(uv * 7.0); float2 f0 = frac(uv * 7.0); f0 = f0 * f0 * (3.0 - 2.0 * f0);
float n7 = lerp(lerp(H2(i0), H2(i0 + float2(1, 0)), f0.x), lerp(H2(i0 + float2(0, 1)), H2(i0 + float2(1, 1)), f0.x), f0.y);
float2 i1 = floor(uv * 1.7); float2 f1 = frac(uv * 1.7); f1 = f1 * f1 * (3.0 - 2.0 * f1);
float n2 = lerp(lerp(H2(i1), H2(i1 + float2(1, 0)), f1.x), lerp(H2(i1 + float2(0, 1)), H2(i1 + float2(1, 1)), f1.x), f1.y);
float2 i2 = floor(uv * 23.0); float2 f2 = frac(uv * 23.0); f2 = f2 * f2 * (3.0 - 2.0 * f2);
float n23 = lerp(lerp(H2(i2), H2(i2 + float2(1, 0)), f2.x), lerp(H2(i2 + float2(0, 1)), H2(i2 + float2(1, 1)), f2.x), f2.y);
float fbm = n2 * .5 + n7 * .3 + n23 * .2;
// bois : fil le long de U, fentes de sechage, noeuds
float grain = sin((uv.y * 90.0 + n7 * 6.0 + sin(uv.x * 3.0) * 2.0)) * .5 + .5;
float check = smoothstep(.93, .99, sin(uv.y * 31.0 + n2 * 4.0) * .5 + .5) * step(.6, n7);
float woodTone = .78 + .22 * grain - .35 * check + .12 * (n2 - .5);
// moellons : cellules de Voronoi grossieres, joints de mortier creux
float2 sc = uv * float2(2.6, 3.6);
float2 cell = floor(sc); float dmin = 8.0; float dsec = 8.0; float cid = 0.0;
for (int j = -1; j <= 1; j++) for (int k = -1; k <= 1; k++) {
  float2 g = cell + float2(k, j);
  float2 o = float2(H2(g), H2(g + 17.3)) * .8 + .1;
  float2 d = g + o - sc; float dd = dot(d, d);
  if (dd < dmin) { dsec = dmin; dmin = dd; cid = H2(g + 3.1); } else if (dd < dsec) { dsec = dd; }
}
float joint = smoothstep(.0, .12, sqrt(dsec) - sqrt(dmin));
float stoneTone = (.82 + .3 * cid) * (.85 + .15 * n23);
float rubble = lerp(.72, stoneTone, joint) * (.92 + .16 * n7);
// pierre de taille : grain fin, aretes epaufrees
float ashlar = (.78 + .3 * H2(floor(WP.xy / 37.0) + floor(WP.z / 23.0))) * (.88 + .12 * n23) - .08 * n7;
// enduit a la chaux : nuages, faiencage, lacunes qui decouvrent le moellon
float plasterTone = .9 + .1 * n2 - .06 * n23;
// tuile : teinte par tuile, coulures sombres, lichen
float tileTone = .8 + .35 * H2(floor(uv * float2(4.7, 2.3))) - .1 * n23;
// physique du batiment : hauteur au-dessus de la cour (pivot), orientation
float hz = WP.z - OP.z;
float up = saturate(N.z);
float side = 1.0 - abs(N.z);
float damp = saturate(1.0 - (hz - 10.0) / (45.0 + 30.0 * n2)) * side * (1.0 - isTile) * (1.0 - isIron);
float splash = saturate(1.0 - hz / 28.0) * side;
float t = saturate(Neglect);
// Weathering (0..1) : l'age depuis l'achevement (biographie, settlement-morphogenesis-001), pas l'abandon.
float age = saturate(Weathering);
// enduit tombe : bas des murs (pluie, gel) + plaques, plus avec l'abandon
float loss = smoothstep(.66 - .25 * t - .14 * age - .25 * saturate(1.0 - hz / 120.0), .78 - .2 * t - .1 * age, n2 * .7 + n7 * .3);
float tone = 1.0;
tone += isWood * (woodTone - 1.0) + isPlank * (woodTone * .95 - 1.0);
tone += isRubble * (rubble - 1.0) + isAshlar * (ashlar - 1.0);
tone += isTile * (tileTone - 1.0) + isEarth * (.8 + .4 * fbm - 1.0) + isDaub * (.82 + .3 * n7 - 1.0);
tone += isFiber * (.82 + .3 * n23 - 1.0);
tone += isPlaster * (plasterTone - 1.0);
float3 col = C * tone;
float3 stoneUnder = float3(.27, .25, .22) * rubble;
col = lerp(col, stoneUnder, isPlaster * loss);
// bois grise au soleil (plus sur ce qui regarde le ciel), surtout s'il est neglige
float lum = dot(col, float3(.3, .59, .11));
col = lerp(col, float3(lum, lum, lum) * float3(.95, .97, 1.0) * 1.1, (isWood + isPlank) * saturate(.12 + .3 * age + .45 * up + .4 * t));
// humidite au pied des murs : plus sombre, lisere de sels
col *= 1.0 - .38 * damp;
col = lerp(col, float3(.42, .40, .36), smoothstep(.02, 0.0, abs(damp - .15)) * .25 * side * (isPlaster + isRubble));
col = lerp(col, float3(.16, .12, .08), splash * .45 * (1.0 - isTile));
// mousse et lichen : faces qui regardent le ciel, creux humides, tuiles nord ; davantage a l'abandon
float mossMask = saturate((up - .45) * 2.0) * smoothstep(.62 - .3 * t - .18 * age, .82, fbm) * (1.0 - isIron) * (1.0 - isEarth) * (1.0 - isFiber);
mossMask += isTile * smoothstep(.7 - .25 * t - .2 * age, .86, n7 * .6 + n2 * .4) * .7;
col = lerp(col, float3(.07, .10, .04) * (.7 + .6 * n23), saturate(mossMask) * (.45 + .25 * age + .3 * t));
// patine : salissure qui coule sous les debords et s'accumule avec les annees
col *= 1.0 - .1 * age * (1.0 - up) * (.6 + .4 * n2);
// abandon : ternit
float l2 = dot(col, float3(.3, .59, .11));
col = lerp(col, float3(l2, l2, l2) * .85, t * .45);
return col;
"""

ROUGH_CODE = r"""
float cls = A * 10.0;
float r = .9;
r = lerp(r, .55, step(cls, 1.0));
r = lerp(r, .72, step(9.0, cls));
float hz = WP.z - OP.z;
float damp = saturate(1.0 - (hz - 10.0) / 60.0) * (1.0 - abs(N.z));
return saturate(r - .25 * damp);
"""

NORMAL_CODE = r"""
// relief de surface par differences finies du motif (moellons et bois), en espace tangent
float cls = A * 10.0;
float isRubble = step(3.0, cls) * step(cls, 4.0);
float isWood = step(7.0, cls) * step(cls, 9.0);
float isPlaster = step(5.0, cls) * step(cls, 7.0);
float isTile = step(9.0, cls);
#ifndef ARCH_H2
#define ARCH_H2 1
#define H2(q) frac(sin(dot(q, float2(127.1, 311.7))) * 43758.5453)
#endif
float hgt(float2 q) {
  return 0.0;
}
float2 e = float2(.004, 0);
float2 sc = UV * float2(2.6, 3.6);
float hs[3];
float2 offs[3] = { float2(0, 0), float2(.012, 0), float2(0, .012) };
for (int s = 0; s < 3; s++) {
  float2 q = sc + offs[s] * float2(2.6, 3.6);
  float2 cell = floor(q); float dmin = 8.0; float dsec = 8.0;
  for (int j = -1; j <= 1; j++) for (int k = -1; k <= 1; k++) {
    float2 g = cell + float2(k, j);
    float2 o = float2(H2(g), H2(g + 17.3)) * .8 + .1;
    float2 d = g + o - q; float dd = dot(d, d);
    if (dd < dmin) { dsec = dmin; dmin = dd; } else if (dd < dsec) { dsec = dd; }
  }
  float stone = smoothstep(0.0, .18, sqrt(dsec) - sqrt(dmin));
  float2 uq = UV + offs[s];
  float wood = sin(uq.y * 90.0 + sin(uq.x * 3.0) * 2.0) * .5 + .5;
  float2 pi = floor(uq * 9.0); float2 pf = frac(uq * 9.0); pf = pf * pf * (3.0 - 2.0 * pf);
  float pn = lerp(lerp(H2(pi), H2(pi + float2(1, 0)), pf.x), lerp(H2(pi + float2(0, 1)), H2(pi + float2(1, 1)), pf.x), pf.y);
  hs[s] = isRubble * stone * 1.0 + isWood * wood * .35 + isPlaster * pn * .45 + isTile * pn * .25;
}
float2 g = float2(hs[1] - hs[0], hs[2] - hs[0]) * 2.2;
return normalize(float3(-g.x, -g.y, 1.0));
"""


def _custom(u, mel, mat, code, out_type, inputs, x, y):
    node = mel.create_material_expression(mat, u.MaterialExpressionCustom, x, y)
    node.set_editor_property('output_type', out_type)
    ins = []
    for name in inputs:
        ci = u.CustomInput()
        ci.set_editor_property('input_name', name)
        ins.append(ci)
    node.set_editor_property('inputs', ins)
    node.set_editor_property('code', code)
    return node


def build_material(u):
    mel = u.MaterialEditingLibrary
    eal = u.EditorAssetLibrary
    path = PKG + '/' + MATERIAL
    if eal.does_asset_exist(path):
        mat = eal.load_asset(path)
        if os.environ.get('ANASTASIS_ARCH_REBUILD', '0') != '1' and eal.get_metadata_tag(mat, 'Recipe') == VERSION:
            u.log('ARCH_MATERIAL exists recipe=' + VERSION)
            return mat
        mel.delete_all_material_expressions(mat)
    else:
        mat = u.AssetToolsHelpers.get_asset_tools().create_asset(MATERIAL, PKG, u.Material, u.MaterialFactoryNew())
    vc = mel.create_material_expression(mat, u.MaterialExpressionVertexColor, -900, 0)
    uv = mel.create_material_expression(mat, u.MaterialExpressionTextureCoordinate, -900, 160)
    wp = mel.create_material_expression(mat, u.MaterialExpressionWorldPosition, -900, 260)
    op = mel.create_material_expression(mat, u.MaterialExpressionObjectPositionWS, -900, 360)
    nrm = mel.create_material_expression(mat, u.MaterialExpressionVertexNormalWS, -900, 460)
    neg = mel.create_material_expression(mat, u.MaterialExpressionScalarParameter, -900, 560)
    neg.set_editor_property('parameter_name', 'Neglect')
    neg.set_editor_property('default_value', 0.0)
    wea = mel.create_material_expression(mat, u.MaterialExpressionScalarParameter, -900, 640)
    wea.set_editor_property('parameter_name', 'Weathering')
    wea.set_editor_property('default_value', 0.35)
    col = _custom(u, mel, mat, MATERIAL_CODE, u.CustomMaterialOutputType.CMOT_FLOAT3,
                  ['C', 'A', 'UV', 'WP', 'OP', 'N', 'Neglect', 'Weathering'], -400, 0)
    rough = _custom(u, mel, mat, ROUGH_CODE, u.CustomMaterialOutputType.CMOT_FLOAT1, ['A', 'WP', 'OP', 'N'], -400, 300)
    nmap = _custom(u, mel, mat, NORMAL_CODE.replace('float hgt(float2 q) {\n  return 0.0;\n}\n', ''),
                   u.CustomMaterialOutputType.CMOT_FLOAT3, ['A', 'UV'], -400, 500)
    metal = _custom(u, mel, mat, 'return step(A, 0.1);', u.CustomMaterialOutputType.CMOT_FLOAT1, ['A'], -400, 700)
    for node, pin in ((col, 'C'),):
        assert mel.connect_material_expressions(vc, '', node, pin)
    for node in (col, rough, nmap, metal):
        assert mel.connect_material_expressions(vc, 'A', node, 'A')
    for node in (col, nmap):
        assert mel.connect_material_expressions(uv, '', node, 'UV')
    for node in (col, rough):
        assert mel.connect_material_expressions(wp, '', node, 'WP')
        assert mel.connect_material_expressions(op, '', node, 'OP')
        assert mel.connect_material_expressions(nrm, '', node, 'N')
    assert mel.connect_material_expressions(neg, '', col, 'Neglect')
    assert mel.connect_material_expressions(wea, '', col, 'Weathering')
    assert mel.connect_material_property(col, '', u.MaterialProperty.MP_BASE_COLOR)
    assert mel.connect_material_property(rough, '', u.MaterialProperty.MP_ROUGHNESS)
    assert mel.connect_material_property(nmap, '', u.MaterialProperty.MP_NORMAL)
    assert mel.connect_material_property(metal, '', u.MaterialProperty.MP_METALLIC)
    u.log('ARCH_MATERIAL_COMPILE_BEGIN ' + path)
    mel.recompile_material(mat)
    u.log('ARCH_MATERIAL_COMPILE_END ' + path)
    eal.set_metadata_tag(mat, 'Recipe', VERSION)
    assert eal.save_asset(path)
    return mat


def write_mesh(u, path, built, mat, complex_collision=True, lods=True):
    eal = u.EditorAssetLibrary
    if eal.does_asset_exist(path):
        assert eal.delete_asset(path), 'delete failed ' + path
    b = u.GeometryScriptSimpleMeshBuffers()
    b.vertices = [u.Vector(*p) for p in built['v']]
    b.triangles = [u.IntVector(*t) for t in built['t']]
    b.normals = [u.Vector(*n) for n in built['n']]
    b.vertex_colors = [u.LinearColor(*c) for c in built['c']]
    b.uv0 = [u.Vector2D(*q) for q in built['uv']]
    dyn = u.DynamicMesh()
    u.GeometryScript_MeshEdits.append_buffers_to_mesh(dyn, b)
    opts = u.GeometryScriptCreateNewStaticMeshAssetOptions()
    opts.enable_recompute_normals = False
    opts.enable_recompute_tangents = True
    opts.enable_nanite = False
    opts.enable_collision = False
    asset, outcome = u.GeometryScript_NewAssetUtils.create_new_static_mesh_asset_from_mesh(dyn, path, opts)
    assert asset, str(outcome)
    asset.set_material(0, mat)
    sms = u.get_editor_subsystem(u.StaticMeshEditorSubsystem)
    if complex_collision:
        body = asset.get_editor_property('body_setup')
        body.set_editor_property('collision_trace_flag', u.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
    else:
        sms.add_simple_collisions(asset, u.ScriptCollisionShapeType.BOX)
    lod_note = 'lod0'
    if lods:
        try:
            opts_r = u.StaticMeshReductionOptions()
            settings = []
            for pct, screen in ((1.0, 1.0), (.45, .35), (.18, .12)):
                s = u.StaticMeshReductionSettings()
                s.set_editor_property('percent_triangles', pct)
                s.set_editor_property('screen_size', screen)
                settings.append(s)
            opts_r.set_editor_property('reduction_settings', settings)
            opts_r.set_editor_property('auto_compute_lod_screen_size', False)
            n = sms.set_lods(asset, opts_r)
            lod_note = 'lods=%d' % n
        except Exception as exc:  # LOD optionnel : la preuve dit lequel
            lod_note = 'lod0 (%s)' % type(exc).__name__
    eal.set_metadata_tag(asset, 'Recipe', VERSION)
    assert eal.save_asset(path)
    return asset, lod_note


def create():
    import unreal as u
    report, failures = validate(write=False)
    assert not failures, failures
    if os.environ.get('ANASTASIS_ARCH_GEOMETRY_ONLY', '0') == '1':
        u.log('ARCH COMPLETE geometry-only')
        return
    mat = build_material(u)
    for name, (body, foot, kind, tier) in geometry().items():
        a, note = write_mesh(u, PKG + '/' + name, body.finish(), mat, complex_collision=True)
        u.log('ARCH ASSET ' + json.dumps(dict(path=PKG + '/' + name, tris=len(body.t), lods=note)))
        fa, fnote = write_mesh(u, PKG + '/' + name + '_Footing', foot.finish(), mat, complex_collision=True, lods=False)
        u.log('ARCH ASSET ' + json.dumps(dict(path=PKG + '/' + name + '_Footing', tris=len(foot.t), lods=fnote)))
    for name, fn in kit_pieces().items():
        m = _kit(fn)
        a, note = write_mesh(u, PKG + '/Kit/' + name, m.finish(), mat, complex_collision=True, lods=False)
        u.log('ARCH KIT ' + json.dumps(dict(path=PKG + '/Kit/' + name, tris=len(m.t))))
    u.log('ARCH COMPLETE buildings=%d kit=%d' % (len(BUILDINGS), len(kit_pieces())))


def main():
    import unreal
    try:
        create()
    except BaseException:
        import traceback
        unreal.log_error('ARCH FAILED ' + traceback.format_exc())
        raise
    finally:
        unreal.SystemLibrary.quit_editor()


if __name__ == '__main__':
    try:
        import unreal  # noqa: F401  present seulement dans l'editeur
        in_editor = True
    except ImportError:
        in_editor = False
    if in_editor:
        main()
    else:
        validate()
