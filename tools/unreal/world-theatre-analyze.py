"""WORLD_THEATRE_001 -- lecture perceptuelle du monde, HORS moteur (Python systeme, numpy + Pillow).

Entree : le releve de world-theatre-read.py (reading.json, grilles .f32, placed.csv).
Sortie (dans --out, defaut <releve>/analysis) :
  perception.json   derives par cellule resumes, signatures anti-generatives, vistas et leurs horizons
  map_*.png         cartes : relief ombre, crêtes et bassins, densite des objets, vides, champs de vue
  vista_<id>.png    rendu logiciel de chaque vista (plans NEAR / MID / FAR / EXTREME colores), meme camera
                    que la capture Unreal : on juge une composition en secondes avant de payer l'editeur
Rien n'est ecrit dans le projet.

Usage : python tools/unreal/world-theatre-analyze.py <dossier du releve> [--out <dossier>] [--vistas <json>]
  --vistas : fichier de vistas canoniques (docs/unreal/world-theatre-001/vistas.json) ; sans lui, les candidats sont
             seulement proposes (perception.json -> vista_candidates).

Noms des structures (le jour ou une couche les consomme, ce sont ceux du C++) :
  FWorldPerceptualSample  une cellule : altitude, pente, courbure, relief local (TPI), eau, densites
  FHorizonSignature       par vista : angle d'elevation et distance de l'horizon par azimut, plans de silhouette
  FWorldVista             camera, FOV, plans, repere dominant, vides, bruit visuel
Axes : X = nord, Y = est (ciel du projet, AnastasisAtmosphereResolver) ; yaw UE 0 = nord, 90 = est.
  FLandmarkRelation       repere x vista : visible, angle, part du champ, concurrence
"""
import argparse, csv, json, math, os, re, sys

import numpy as np
from PIL import Image, ImageDraw

NODATA = -1.0e29
FAR_HALF_M = 75000.0
UU_PER_M = 100.0
# Plans de distance (metres) : NEAR < 60, MID < 1000, FAR < 8000, EXTREME au-dela. Un plan = un role.
BANDS = [('near', 0.0, 60.0), ('mid', 60.0, 1000.0), ('far', 1000.0, 8000.0), ('extreme', 8000.0, 1.0e9)]
BAND_COLOURS = {'near': (214, 176, 112), 'mid': (120, 160, 92), 'far': (78, 110, 140), 'extreme': (168, 182, 205), 'sky': (232, 238, 246)}


# ---------------------------------------------------------------- lecture

class Raster:
    def __init__(self, meta, folder):
        self.ox, self.oy = meta['origin']
        self.cell = meta['cell']
        self.w, self.h = meta['w'], meta['h']
        g = np.fromfile(os.path.join(folder, meta['ground']), dtype='<f4').reshape(self.h, self.w)
        wt = np.fromfile(os.path.join(folder, meta['water']), dtype='<f4').reshape(self.h, self.w)
        self.valid = g > NODATA
        self.ground = np.where(self.valid, g, np.nan).astype(np.float64)
        self.water = np.where(wt > NODATA, wt, np.nan).astype(np.float64)

    def crop(self, cx, cy, half):
        """Garde le carre de demi-cote half (uu) autour de (cx, cy) : au-dela de l'anneau (60 km), des jupes
        etirees jusqu'a l'horizon de la planete remplissent la grille sans rien porter de lisible."""
        i0, j0 = [max(0, int(v)) for v in self.ij(cx - half, cy - half)]
        i1, j1 = [int(v) + 1 for v in self.ij(cx + half, cy + half)]
        i1, j1 = min(self.w, i1), min(self.h, j1)
        self.ground, self.water, self.valid = (a[j0:j1, i0:i1] for a in (self.ground, self.water, self.valid))
        self.ox, self.oy = self.ox + i0 * self.cell, self.oy + j0 * self.cell
        self.h, self.w = self.ground.shape

    def xy(self, i, j):
        return self.ox + (i + 0.5) * self.cell, self.oy + (j + 0.5) * self.cell

    def ij(self, x, y):
        return (x - self.ox) / self.cell - 0.5, (y - self.oy) / self.cell - 0.5

    def contains(self, x, y):
        i, j = self.ij(x, y)
        return 0 <= i < self.w - 1 and 0 <= j < self.h - 1


def load(folder):
    with open(os.path.join(folder, 'reading.json'), encoding='utf-8') as f:
        meta = json.load(f)
    near = Raster(meta['near'], folder)
    far = Raster(meta['far'], folder)
    mp = meta['map']
    far.crop((mp['min'][0] + mp['max'][0]) / 2, (mp['min'][1] + mp['max'][1]) / 2, FAR_HALF_M * UU_PER_M)
    placed = []
    with open(os.path.join(folder, 'placed.csv'), encoding='utf-8') as f:
        for row in csv.DictReader(f):
            placed.append((row['family'], row['mesh'], float(row['x']), float(row['y']), float(row['z']),
                           float(row['height']), float(row['radius'])))
    return meta, near, far, placed


# ---------------------------------------------------------------- filtres (numpy seul)

def fill_nan(a):
    """Remplit les trous par la moyenne des voisins valides, iterativement (bords de grille)."""
    a = a.copy()
    for _ in range(64):
        m = np.isnan(a)
        if not m.any():
            break
        p = np.pad(a, 1, mode='edge')
        stack = np.stack([p[:-2, 1:-1], p[2:, 1:-1], p[1:-1, :-2], p[1:-1, 2:]])
        with np.errstate(invalid='ignore'):
            mean = np.nanmean(stack, axis=0)
        a[m] = mean[m]
    a[np.isnan(a)] = np.nanmean(a)
    return a


def box(a, r):
    if r < 1:
        return a.copy()
    p = np.pad(a, r, mode='edge')
    c = np.cumsum(np.cumsum(p, axis=0), axis=1)
    c = np.pad(c, ((1, 0), (1, 0)))
    k = 2 * r + 1
    s = c[k:, k:] - c[:-k, k:] - c[k:, :-k] + c[:-k, :-k]
    return s / (k * k)


def blur(a, r):
    """Approximation gaussienne : trois flous de boite (sigma ~ r)."""
    rb = max(1, int(round(r / 1.7)))
    return box(box(box(a, rb), rb), rb)


def slope_deg(z, cell):
    gy, gx = np.gradient(z, cell)
    return np.degrees(np.arctan(np.hypot(gx, gy)))


def hillshade(z, cell, az=315.0, alt=40.0):
    gy, gx = np.gradient(z, cell)
    slope = np.arctan(np.hypot(gx, gy))
    aspect = np.arctan2(-gx, gy)
    a, al = np.radians(az), np.radians(alt)
    return np.clip(np.sin(al) * np.cos(slope) + np.cos(al) * np.sin(slope) * np.cos(a - aspect), 0, 1)


# ---------------------------------------------------------------- echantillon perceptuel

def perceptual_fields(r, z):
    """FWorldPerceptualSample par cellule : relief local a deux echelles (TPI), courbure, pente."""
    cell_m = r.cell / UU_PER_M
    fields = {'z': z, 'slope': slope_deg(z, r.cell)}
    for name, metres in (('tpi_small', 300.0), ('tpi_large', 2500.0)):
        rad = max(1, int(round(metres / cell_m)))
        fields[name] = (z - blur(z, rad)) / UU_PER_M
    lap = blur(z, max(1, int(round(150.0 / cell_m))))
    gy, gx = np.gradient(lap, r.cell)
    gyy, _ = np.gradient(gy, r.cell)
    _, gxx = np.gradient(gx, r.cell)
    fields['curvature'] = -(gxx + gyy) * 1e4  # convexe > 0 (crete), concave < 0 (creux)
    return fields


def density_grid(r, points, cell_m):
    """Compte des points par cellule de cell_m metres sur l'emprise de la grille r."""
    k = max(1, int(round(cell_m * UU_PER_M / r.cell)))
    w, h = r.w // k, r.h // k
    g = np.zeros((h, w))
    for x, y in points:
        i, j = r.ij(x, y)
        ii, jj = int(i // k), int(j // k)
        if 0 <= ii < w and 0 <= jj < h:
            g[jj, ii] += 1
    return g, k


def clark_evans(points, area_m2):
    """R = distance moyenne au plus proche voisin observee / attendue sous Poisson. ~1 = semis au hasard."""
    if len(points) < 10 or area_m2 <= 0:
        return None
    p = np.array(points) / UU_PER_M
    # Grille de hachage pour un plus-proche-voisin en O(n).
    cell = math.sqrt(area_m2 / len(p)) * 2.0
    keys = {}
    for idx, (x, y) in enumerate(p):
        keys.setdefault((int(x // cell), int(y // cell)), []).append(idx)
    d = np.empty(len(p))
    for idx, (x, y) in enumerate(p):
        cx, cy = int(x // cell), int(y // cell)
        best = 1e18
        for ring in range(1, 200):
            for dx in range(-ring, ring + 1):
                for dy in range(-ring, ring + 1):
                    for o in keys.get((cx + dx, cy + dy), ()):
                        if o != idx:
                            dd = (p[o, 0] - x) ** 2 + (p[o, 1] - y) ** 2
                            if dd < best:
                                best = dd
            if best < (ring * cell) ** 2:
                break
        d[idx] = math.sqrt(best)
    expected = 0.5 / math.sqrt(len(p) / area_m2)
    return float(np.mean(d) / expected)


def vmr(g):
    """Rapport variance / moyenne des comptes par quadrat : 1 = Poisson, >> 1 = massifs, < 1 = trame reguliere."""
    m = g.mean()
    return float(g.var() / m) if m > 0 else None


def largest_empty_disc_m(occupied, cell_m, inside, max_cells=60):
    """Rayon (m) du plus grand disque sans objet DANS la carte : distance (carree, en cellules) a la cellule
    occupee la plus proche, par dilatations successives ; plafonnee a max_cells."""
    d = np.full(occupied.shape, float(max_cells))
    d[occupied] = 0.0
    occ = occupied.astype(np.float64)
    for r in range(1, max_cells):
        reach = box(occ, r) > 1e-12
        d = np.where((d >= max_cells) & reach, float(r), d)
    d = np.where(inside, d, 0)
    return float(d.max() * cell_m), d * cell_m


# ---------------------------------------------------------------- vue : rayons sur le relief

class World:
    """Relief combine : grille proche (fine) la ou elle existe, lointaine ailleurs."""

    def __init__(self, near, far, near_z, far_z, cover=None):
        self.near, self.far, self.nz, self.fz = near, far, near_z, far_z
        # cover : grille proche, 1 = masse du theatre (apercu logiciel du plan), None = monde tel quel
        self.cover = cover

    def cover_at(self, x, y):
        if self.cover is None:
            return None
        i, j = self.near.ij(x, y)
        i, j = np.round(i).astype(int), np.round(j).astype(int)
        ok = (i >= 0) & (j >= 0) & (i < self.near.w) & (j < self.near.h)
        out = np.zeros(x.shape, bool)
        out[ok] = self.cover[j[ok], i[ok]] > 0
        return out

    def height(self, x, y):
        """x, y : tableaux (uu). Bilineaire sur la grille la plus fine qui couvre le point."""
        out = self._bilinear(self.far, self.fz, x, y)
        i, j = self.near.ij(x, y)
        inside = (i >= 0) & (j >= 0) & (i < self.near.w - 1) & (j < self.near.h - 1)
        if inside.any():
            out = np.where(inside, self._bilinear(self.near, self.nz, x, y), out)
        return out

    @staticmethod
    def _bilinear(r, z, x, y):
        i, j = r.ij(x, y)
        i = np.clip(i, 0, r.w - 1.001)
        j = np.clip(j, 0, r.h - 1.001)
        i0, j0 = np.floor(i).astype(int), np.floor(j).astype(int)
        fi, fj = i - i0, j - j0
        return ((z[j0, i0] * (1 - fi) + z[j0, i0 + 1] * fi) * (1 - fj)
                + (z[j0 + 1, i0] * (1 - fi) + z[j0 + 1, i0 + 1] * fi) * fj)


def ray_distances(max_m=62000.0):
    """Pas de marche croissant : 1 m pres de l'oeil, ~0,5 % de la distance au loin."""
    d, out = 2.0, []
    while d < max_m:
        out.append(d)
        d += max(1.0, d * 0.005)
    return np.array(out) * UU_PER_M


def horizon_signature(world, eye, yaw_deg, fov_deg, columns=241):
    """FHorizonSignature : pour chaque azimut, l'elevation (deg) de l'horizon, sa distance (m), et les
    plans de silhouette -- maxima successifs de l'angle le long du rayon, chacun une crete qui se decoupe."""
    dist = ray_distances()
    az = np.radians(yaw_deg + np.linspace(-fov_deg / 2, fov_deg / 2, columns))
    xs = eye[0] + np.cos(az)[:, None] * dist[None, :]
    ys = eye[1] + np.sin(az)[:, None] * dist[None, :]
    z = world.height(xs.ravel(), ys.ravel()).reshape(xs.shape)
    # Courbure terrestre (rayon 6371 km) : un sommet a 40 km s'abaisse de 125 m.
    z = z - (dist[None, :] ** 2) / (2 * 6.371e8)
    ang = np.degrees(np.arctan2(z - eye[2], dist[None, :]))
    run = np.maximum.accumulate(ang, axis=1)
    horizon = run[:, -1]
    hidx = np.argmax(ang >= horizon[:, None] - 1e-9, axis=1)
    hdist = dist[hidx] / UU_PER_M
    # Plans : un rayon franchit un nouveau maximum ; on compte les sauts de distance > 25 % entre deux
    # maxima successifs qui depassent le precedent d'au moins 0,05 deg (une silhouette lisible).
    layers = []
    for c in range(columns):
        rising = np.nonzero(np.diff(run[c], prepend=-90.0) > 0.05)[0]
        n, last = 0, None
        for k in rising:
            if last is None or dist[k] > dist[last] * 1.25:
                n += 1
            last = k
        layers.append(n)
    return {
        'elevation_deg': horizon.round(3).tolist(),
        'distance_m': hdist.round(0).tolist(),
        'layers': layers,
        'flatness_deg': float(np.std(horizon)),
        'roughness_deg': float(np.mean(np.abs(np.diff(horizon, 2)))),
        'mean_layers': float(np.mean(layers)),
        'horizon_band': band_of(float(np.median(hdist))),
    }


def band_of(m):
    for name, lo, hi in BANDS:
        if lo <= m < hi:
            return name
    return 'extreme'


def render_vista(world, eye, yaw, pitch, fov, width=640, height=360, placed_xy=None):
    """Rendu logiciel par colonnes (voxel space) : chaque pixel prend la couleur du plan de distance
    du premier sol touche ; ombrage par pente vue. Sert a juger plans, horizon et vides, pas la matiere."""
    dist = ray_distances()
    half_h = math.radians(fov / 2)
    focal = (width / 2) / math.tan(half_h)
    cols = np.arange(width) - width / 2 + 0.5
    az = math.radians(yaw) + np.arctan(cols / focal)
    cosc = np.cos(np.arctan(cols / focal))
    xs = eye[0] + np.cos(az)[:, None] * dist[None, :]
    ys = eye[1] + np.sin(az)[:, None] * dist[None, :]
    z = world.height(xs.ravel(), ys.ravel()).reshape(xs.shape)
    z = z - (dist[None, :] ** 2) / (2 * 6.371e8)
    cov = world.cover_at(xs.ravel(), ys.ravel())
    cov = cov.reshape(xs.shape) if cov is not None else np.zeros(xs.shape, bool)
    forest = np.array((44, 66, 38)) / 255.0
    # Ligne ecran du point (pitch compris), distance projetee sur l'axe de visee.
    depth = dist[None, :] * cosc[:, None]
    ang = np.arctan2(z - eye[2], dist[None, :]) - math.radians(pitch)
    row = height / 2 - np.tan(ang) * focal
    img = np.zeros((height, width, 3), dtype=np.float64)
    img[:] = np.array(BAND_COLOURS['sky']) / 255.0
    dmap = np.full((height, width), np.inf)
    band_idx = np.searchsorted([b[2] * UU_PER_M for b in BANDS], dist)
    colours = np.array([BAND_COLOURS[b[0]] for b in BANDS]) / 255.0
    dz = np.diff(z, axis=1, prepend=z[:, :1])
    dd = np.diff(dist, prepend=dist[0])
    shade = np.clip(0.55 + 2.5 * dz / np.maximum(dd[None, :], 1.0), 0.35, 1.25)
    for c in range(width):
        ybuf = height
        r = row[c]
        for k in range(len(dist)):
            top = int(max(0, math.floor(r[k])))
            if top < ybuf:
                col = (forest if cov[c, k] else colours[min(band_idx[k], 3)]) * shade[c, k]
                # Brume : melange vers le ciel avec la distance (lecture des plans, pas une atmosphere).
                fog = 1.0 - math.exp(-dist[k] / UU_PER_M / 30000.0)
                col = col * (1 - fog) + img[0, 0] * fog
                img[top:ybuf, c] = col
                dmap[top:ybuf, c] = dist[k] / UU_PER_M
                ybuf = top
                if ybuf <= 0:
                    break
    return (np.clip(img, 0, 1) * 255).astype(np.uint8), dmap


def project(eye, yaw, pitch, fov, width, height, x, y, z):
    """Position ecran (px) d'un point monde, ou None derriere la camera."""
    dx, dy, dz = x - eye[0], y - eye[1], z - eye[2]
    cy, sy = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
    fwd = dx * cy + dy * sy
    right = -dx * sy + dy * cy
    if fwd <= 1:
        return None
    focal = (width / 2) / math.tan(math.radians(fov / 2))
    ang = math.atan2(dz, math.hypot(fwd, right)) - math.radians(pitch)
    return width / 2 + right / fwd * focal, height / 2 - math.tan(ang) * focal


def visible(world, eye, x, y, z, steps=400):
    """Ligne de vue oeil -> point au-dessus du sol (z) : vrai si aucun sol ne la coupe."""
    t = np.linspace(0.02, 0.98, steps)
    px, py = eye[0] + (x - eye[0]) * t, eye[1] + (y - eye[1]) * t
    pz = eye[2] + (z - eye[2]) * t
    d = np.hypot(px - eye[0], py - eye[1])
    g = world.height(px, py) - d ** 2 / (2 * 6.371e8)
    return bool(np.all(g < pz))


# ---------------------------------------------------------------- programme

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('reading')
    ap.add_argument('--out')
    ap.add_argument('--vistas')
    ap.add_argument('--resolve-vistas', action='store_true', help='ecrit z (sol rendu + eye_m) dans le fichier de vistas')
    ap.add_argument('--compose', action='store_true', help='plan de masses de l avant-pays + apercu logiciel des vistas apres')
    ap.add_argument('--emit-plan', help='ecrit le plan C++ (AnastasisWorldTheatrePlan.inl) a ce chemin')
    ap.add_argument('--luminance', help='dossier de captures (<vista>_<etat>.png) : luminance par plan de distance')
    a = ap.parse_args()
    out = a.out or os.path.join(a.reading, 'analysis')
    os.makedirs(out, exist_ok=True)
    meta, near, far, placed = load(a.reading)
    nz, fz = fill_nan(near.ground), fill_nan(far.ground)
    world = World(near, far, nz, fz)
    if a.luminance:
        with open(a.vistas, encoding='utf-8') as f:
            lv = json.load(f)['vistas']
        res = luminance_by_plane(a.luminance, lv, canopy_world(near, far, nz, fz, placed))
        with open(os.path.join(a.luminance, 'luminance.json'), 'w', encoding='utf-8') as f:
            json.dump(res, f, indent=1)
        print_luminance(res)
        return
    mp = meta['map']
    map_min, map_max = mp['min'], mp['max']
    map_area_m2 = (map_max[0] - map_min[0]) * (map_max[1] - map_min[1]) / UU_PER_M ** 2
    report = {'reading': os.path.abspath(a.reading), 'seed': meta['seed'], 'families': meta['families'],
              'map_size_m': [(map_max[0] - map_min[0]) / UU_PER_M, (map_max[1] - map_min[1]) / UU_PER_M]}

    # -- derives
    nf = perceptual_fields(near, nz)
    ff = perceptual_fields(far, fz)
    water_near = ~np.isnan(near.water) & (near.water > nz - 1)
    inside = np.zeros_like(near.valid)
    ii = np.arange(near.w)
    jj = np.arange(near.h)
    X, Y = np.meshgrid(near.ox + (ii + 0.5) * near.cell, near.oy + (jj + 0.5) * near.cell)
    inside = (X >= map_min[0]) & (X <= map_max[0]) & (Y >= map_min[1]) & (Y <= map_max[1])
    relief_far_m = (np.nanmax(fz) - np.nanmin(fz)) / UU_PER_M
    report['relief'] = {
        'map_relief_m': float((nz[inside].max() - nz[inside].min()) / UU_PER_M),
        'map_slope_p50_deg': float(np.median(nf['slope'][inside])),
        'map_slope_p95_deg': float(np.percentile(nf['slope'][inside], 95)),
        'ring_relief_m': float(relief_far_m),
        'near_ring_slope_p50_deg': float(np.median(nf['slope'][~inside])),
        'map_water_fraction': float(water_near[inside].mean()),
        'near_ring_water_fraction': float(water_near[~inside].mean()),
    }

    # -- distributions : signature anti-generative
    fam = {}
    for f, m, x, y, z, hgt, rad in placed:
        fam.setdefault(f, []).append((x, y))
    sig = {}
    for f, pts in fam.items():
        pin = [(x, y) for x, y in pts if map_min[0] <= x <= map_max[0] and map_min[1] <= y <= map_max[1]]
        entry = {'count': len(pts), 'in_map': len(pin)}
        if len(pin) >= 10:
            entry['clark_evans_R'] = clark_evans(pin, map_area_m2)
            for q in (40, 160, 400):
                g, _ = density_grid(near, pin, q)
                k = max(1, int(round(q * UU_PER_M / near.cell)))
                ins = inside[::k, ::k][:g.shape[0], :g.shape[1]]
                entry['vmr_%dm' % q] = vmr(g[ins])
                entry['empty_quadrats_%dm' % q] = float((g[ins] == 0).mean())
        sig[f] = entry
    report['distribution'] = sig

    # Vides : plus grand disque sans arbre ni arbuste ni rocher ni batiment dans la carte (cellules de 20 m).
    occ_pts = [(x, y) for f, m, x, y, z, h, r in placed if f in ('tree', 'shrub', 'rock', 'building', 'ruin', 'place')]
    occ, k = density_grid(near, occ_pts, near.cell / UU_PER_M)
    occ = occ > 0
    ins_k = inside[:occ.shape[0], :occ.shape[1]]
    disc_m, dmap = largest_empty_disc_m(occ | water_near[:occ.shape[0], :occ.shape[1]], near.cell / UU_PER_M, ins_k)
    report['negative_space'] = {
        'largest_empty_disc_m': disc_m,
        'map_fraction_over_60m_from_object': float((dmap[ins_k] > 60).mean()),
        'map_fraction_over_150m_from_object': float((dmap[ins_k] > 150).mean()),
    }

    # Couture : densite d'arbres dans la bande de 200 m interieure vs exterieure au bord de la carte.
    trees = fam.get('tree', [])
    def band_density(lo, hi):
        n, area = 0, 0.0
        for x, y in trees:
            dx = max(map_min[0] - x, 0, x - map_max[0])
            dy = max(map_min[1] - y, 0, y - map_max[1])
            outside = math.hypot(dx, dy)
            inner = min(x - map_min[0], map_max[0] - x, y - map_min[1], map_max[1] - y)
            sd = outside if outside > 0 else -inner
            if lo <= sd / UU_PER_M < hi:
                n += 1
        w_m = (map_max[0] - map_min[0]) / UU_PER_M
        h_m = (map_max[1] - map_min[1]) / UU_PER_M
        per = 2 * (w_m + h_m)
        area = per * (hi - lo)
        return n / area * 1e4 if area > 0 else 0.0
    report['seam'] = {'trees_per_ha_inner_200m': band_density(-200, 0), 'trees_per_ha_outer_200m': band_density(0, 200),
                      'trees_per_ha_outer_200_2000m': band_density(200, 2000)}

    # -- cartes
    save_maps(out, near, far, nz, fz, nf, ff, water_near, inside, placed, dmap, meta)

    # -- vistas
    vistas = []
    if a.vistas:
        with open(a.vistas, encoding='utf-8') as f:
            vdoc = json.load(f)
        vistas = vdoc['vistas']
        for v in vistas:
            if 'eye_m' in v:
                g = float(world.height(np.array([float(v['x'])]), np.array([float(v['y'])]))[0])
                v['z'] = round(g + v['eye_m'] * UU_PER_M, 1)
        if a.resolve_vistas:
            with open(a.vistas, 'w', encoding='utf-8') as f:
                json.dump(vdoc, f, indent=1, ensure_ascii=False)
    report['vista_candidates'] = propose_candidates(meta, near, nz, nf, inside, water_near, placed)
    report['vistas'] = []
    for v in vistas:
        report['vistas'].append(evaluate_vista(out, world, v, placed, meta))

    # -- diagnostic anti-generatif (PHASE 3)
    report['variant_balance'] = variant_balance(placed)
    mc = {}
    for f, m, *_ in placed:
        mc[m] = mc.get(m, 0) + 1
    report['mesh_counts'] = dict(sorted(mc.items(), key=lambda kv: -kv[1]))
    report['concentricity'] = {'near_1_12km': concentricity(near, nz, meta, 1, 12), 'far_12_60km': concentricity(far, fz, meta, 12, 60)}
    report['diagnostic'] = diagnose(report)

    # -- composition : plan de masses, apercu logiciel des vistas APRES, plan C++ genere
    if a.compose:
        b = [(x, y) for f, m, x, y, z, h, r in placed if f == 'building']
        village = (float(np.mean([p[0] for p in b])), float(np.mean([p[1] for p in b]))) if b else \
            ((mp['min'][0] + mp['max'][0]) / 2, (mp['min'][1] + mp['max'][1]) / 2)
        masses, stats, (apt_s, mask, lab, k) = compose_masses(near, nz, nf, water_near, inside, placed, meta, village, MASS_RULES)
        report['composition'] = {'rules': MASS_RULES, 'stats': stats,
                                 'masses': [{kk: m[kk] for kk in ('id', 'ha', 'centre', 'why')} for m in masses]}
        # Couverture et canopee sur la grille proche (apercu) : le masque de 40 m, remonte a 20 m.
        # (le contour est la verite du plan ; ce masque n'en est que l'approximation pour l'apercu)
        cover = np.zeros_like(nz)
        sizes = np.bincount(lab.ravel())
        cell_ha = (near.cell * k / UU_PER_M) ** 2 / 1e4
        big = [c for c in np.argsort(-sizes) if c > 0 and sizes[c] * cell_ha >= MASS_RULES['min_mass_ha']][:MASS_RULES['max_masses']]
        sel = np.isin(lab, big)
        up = np.kron(sel.astype(float), np.ones((k, k)))[:nz.shape[0], :nz.shape[1]]
        cover[:up.shape[0], :up.shape[1]] = up
        ramp = np.clip(blur(cover, 1) * 1.6 - 0.3, 0, 1)
        nz_after = nz + ramp * MASS_RULES['canopy_uu']
        world_after = World(near, far, nz_after, fz, cover=cover)
        report['vistas_after'] = [evaluate_vista(out, world_after, dict(v, id=v['id'] + '_after'), placed, meta) for v in vistas]
        # Carte du plan : relief ombre, masses, carte.
        hs = hillshade(nz, near.cell)
        rgb = np.stack([hs * 0.85 + 0.1] * 3, -1)
        rgb[cover > 0] = rgb[cover > 0] * np.array([0.35, 0.55, 0.3])
        rgb[water_near] = (0.2, 0.35, 0.7)
        rgb[inside] = rgb[inside] * np.array([1.0, 0.85, 0.85])
        img = Image.fromarray((np.clip(rgb, 0, 1)[::-1] * 255).astype(np.uint8))
        d = ImageDraw.Draw(img)
        for m in masses:
            pts = [((x - near.ox) / near.cell, near.h - 1 - (y - near.oy) / near.cell) for x, y in m['outline']]
            d.line(pts + [pts[0]], fill=(20, 60, 10), width=2)
            for h_ in m.get('holes', []):
                hp = [((x - near.ox) / near.cell, near.h - 1 - (y - near.oy) / near.cell) for x, y in h_]
                d.line(hp + [hp[0]], fill=(200, 200, 60), width=1)
            cx, cy = (m['centre'][0] - near.ox) / near.cell, near.h - 1 - (m['centre'][1] - near.oy) / near.cell
            d.text((cx, cy), m['id'][-2:], fill=(255, 255, 255))
        for v in vistas:
            vx, vy = (v['x'] - near.ox) / near.cell, near.h - 1 - (v['y'] - near.oy) / near.cell
            ex = vx + 60 * math.cos(math.radians(v['yaw']))
            ey = vy - 60 * math.sin(math.radians(v['yaw']))
            d.line((vx, vy, ex, ey), fill=(255, 0, 0), width=2)
            d.text((vx + 3, vy + 3), v['id'][:2], fill=(255, 0, 0))
        img.save(os.path.join(out, 'map_plan.png'))
        # Run 3 : depuis le village, aucune bosse a moins de 3,5 km ne depasse la canopee de la carte. Le repere
        # sera donc REVELE : lisible sur le ciel depuis le point haut (V3) et le bord nord (V5), cache du village.
        eyes = [(v['id'], (v['x'], v['y'], v['z'])) for v in vistas if v['id'][:2] in ('V3', 'V5')]
        hidden_from = [(v['id'], (v['x'], v['y'], v['z'])) for v in vistas if v['id'][:2] in ('V1', 'V2', 'V6', 'V9')]
        # Run 2 : la tour placee sur le seul relief etait cachee par les arbres de la carte. La ligne de vue et
        # le fond de ciel se calculent sur la CANOPEE reelle : sol + hauteur de chaque arbre et arbuste releves.
        canopy = nz.copy()
        for f, m_, x, y, zz, hh, rr in placed:
            if f not in ('tree', 'shrub', 'building', 'ruin'):
                continue
            ci, cj = near.ij(x, y)
            r_cells = max(0, int(rr / near.cell))
            for dj in range(-r_cells, r_cells + 1):
                for di in range(-r_cells, r_cells + 1):
                    ii_, jj_ = int(round(ci)) + di, int(round(cj)) + dj
                    if 0 <= ii_ < near.w and 0 <= jj_ < near.h:
                        canopy[jj_, ii_] = max(canopy[jj_, ii_], zz + hh)
        world_canopy = World(near, far, canopy, fz)
        lm, n_cand = place_landmark(world_canopy, near, nz, nf, water_near, meta, eyes, vistas, LANDMARK_RULES, hidden_from, cover)
        report['landmark_search'] = {'candidates': n_cand, 'top': [{'score': round(t[0], 3), 'x': t[1], 'y': t[2], 'z': t[3],
                                                                       'from': t[4], 'framed_in': t[5]} for t in lm]}
        silhouettes = []
        # Un repere n'est pose que s'il se LIT : au moins LANDMARK_RULES['min_angular_height_deg'] depuis une vista.
        best_h = 0.0
        if lm:
            for _, txt in lm[0][4]:
                mh = re.search(r'haut ([0-9.]+) deg', str(txt))
                if mh:
                    best_h = max(best_h, float(mh.group(1)))
        report['landmark_search']['best_angular_height_deg'] = best_h
        readable = bool(lm) and best_h >= LANDMARK_RULES['min_angular_height_deg']
        report['landmark_search']['verdict'] = 'pose' if readable else 'refuse : illisible (%.2f deg < %.2f)' % (
            best_h, LANDMARK_RULES['min_angular_height_deg'])
        if readable:
            sc, x, y, gz, detail, framed = lm[0]
            silhouettes.append({'id': 'vigla_01', 'kind': 'RuinedTower', 'xy': (x, y), 'yaw': 20.0, 'scale': 1.0,
                                'why': 'tour de guet abandonnee (hypothese de conception, PONT-HIS-01 : poste sur une bosse qui commande '
                                       'l approche ; zone de depart exposee, sans garnison) ; seul repere humain, revele et non visible : '
                                       'choisi parmi %d bosses pour se decouper sur le ciel au-dessus de la canopee : %s ; cadre : %s' % (n_cand, '; '.join('%s %s' % d for d in detail), ', '.join(framed) or 'aucun')})
        plan = {'masses': masses, 'silhouettes': silhouettes}
        with open(os.path.join(out, 'plan.json'), 'w', encoding='utf-8') as f:
            json.dump(plan, f, indent=1)
        if a.emit_plan:
            emit_plan(plan, a.emit_plan, '%s, regles %s' % (os.path.basename(os.path.abspath(a.reading)), json.dumps(MASS_RULES)))
    with open(os.path.join(out, 'perception.json'), 'w', encoding='utf-8') as f:
        json.dump(report, f, indent=1)
    print('WORLD_THEATRE_ANALYZE OK out=%s vistas=%d' % (out, len(report['vistas'])))


def concentricity(r, z, meta, lo_km, hi_km):
    """Le relief autour de la carte est-il organise PAR la carte ? Part de variance de l'altitude expliquee par
    le rayon seul (anneaux de 250 m), profil radial moyen, et nombre de secteurs de 15 deg ou l'altitude monte
    avec le rayon : une cuvette centree sur la zone jouable fait monter le sol dans toutes les directions."""
    mp = meta['map']
    cx, cy = (mp['min'][0] + mp['max'][0]) / 2, (mp['min'][1] + mp['max'][1]) / 2
    ii, jj = np.meshgrid(np.arange(r.w), np.arange(r.h))
    X, Y = r.ox + (ii + .5) * r.cell, r.oy + (jj + .5) * r.cell
    R = np.hypot(X - cx, Y - cy) / UU_PER_M / 1000
    A = np.degrees(np.arctan2(Y - cy, X - cx))
    m = (R >= lo_km) & (R < hi_km)
    zz = z[m] / UU_PER_M
    rr = R[m]
    rb = np.floor(rr * 4).astype(int)
    ab = np.floor((A[m] + 180) / 15).astype(int) % 24
    mr = np.bincount(rb, zz) / np.maximum(np.bincount(rb), 1)
    rising = 0
    for s in range(24):
        sel = ab == s
        if sel.sum() > 10:
            rising += int(np.corrcoef(rr[sel], zz[sel])[0, 1] > 0.5)
    step = max(1, int(round((hi_km - lo_km) * 4 / 12)))
    return {'r2_radius': float(1 - (zz - mr[rb]).var() / zz.var()),
            'sectors_rising_with_radius': '%d/24' % rising,
            'radial_profile_m': [[k / 4, round(float(mr[k]), 1)] for k in range(int(lo_km * 4), int(hi_km * 4), step)]}


def variant_balance(placed):
    """Variantes d'un meme maillage (SM_X_01/02/03) tirees a parts egales : une rotation, pas un choix."""
    counts = {}
    for f, m, *_ in placed:
        base = m.rsplit('_', 1)[0] if m[-2:].isdigit() else m
        counts.setdefault(base, {}).setdefault(m, 0)
        counts[base][m] += 1
    out = {}
    for base, c in counts.items():
        if len(c) >= 2 and sum(c.values()) >= 300:
            v = sorted(c.values())
            out[base] = {'variants': len(v), 'total': sum(v), 'max_over_min': round(v[-1] / max(v[0], 1), 3)}
    return out


def diagnose(report):
    """PHASE 3 -- signatures d'un monde genere, chacune mesuree, avec son seuil. Rien n'est corrige ici."""
    d = []

    def add(key, value, threshold, present, evidence):
        d.append({'signature': key, 'value': value, 'threshold': threshold, 'present': bool(present), 'evidence': evidence})
    dist = report['distribution']
    ns = report['negative_space']
    add('densite_partout', ns['largest_empty_disc_m'], 'plus grand disque vide < 150 m', ns['largest_empty_disc_m'] < 150,
        'part de la carte a plus de 60 m de tout objet : %.1f %%' % (100 * ns['map_fraction_over_60m_from_object']))
    rock = dist.get('rock', {})
    add('distribution_uniforme_rochers', rock.get('empty_quadrats_160m'), 'quadrats de 160 m sans rocher < 5 %',
        (rock.get('empty_quadrats_160m') if rock.get('empty_quadrats_160m') is not None else 1) < 0.05,
        '%d rochers dans la carte' % rock.get('in_map', 0))
    vb = report.get('variant_balance', {})
    rot = {b: v for b, v in vb.items() if v['max_over_min'] < 1.05}
    add('repetition_de_frequence', len(rot), 'familles a variantes equitirees (max/min < 1,05)', len(rot) >= 2,
        ', '.join('%s x%d (%d, %.3f)' % (b, v['variants'], v['total'], v['max_over_min'])
                  for b, v in sorted(rot.items(), key=lambda kv: -kv[1]['total'])[:6]))
    ruin = dist.get('ruin', {})
    add('reperes_concurrents', ruin.get('empty_quadrats_400m'), 'ruines dans > 90 % des quadrats de 400 m',
        (ruin.get('empty_quadrats_400m') if ruin.get('empty_quadrats_400m') is not None else 1) < 0.10,
        '%d ruines dans la carte, dont %d du meme maillage generique' % (ruin.get('in_map', 0), report.get('mesh_counts', {}).get('SM_Ruin_Generic_01', 0)))
    seam = report['seam']
    add('foret_coupee_au_bord', [round(seam['trees_per_ha_inner_200m'], 1), round(seam['trees_per_ha_outer_200m'], 1)],
        'arbres/ha dehors < 10 % de dedans', seam['trees_per_ha_outer_200m'] < 0.1 * seam['trees_per_ha_inner_200m'],
        'bande de 200 m de part et d autre du bord de la carte')
    con = report.get('concentricity', {}).get('near_1_12km', {})
    add('monde_centre_sur_la_carte', con.get('sectors_rising_with_radius'), 'le sol monte avec le rayon dans >= 20 secteurs sur 24',
        int(str(con.get('sectors_rising_with_radius', '0/24')).split('/')[0]) >= 20,
        'profil radial (km, m) : %s' % con.get('radial_profile_m'))
    closed = [v['id'] for v in report.get('vistas', []) if v['horizon']['horizon_band'] in ('near', 'mid')]
    add('horizon_accidentel', closed, 'vista canonique dont l horizon median est a moins de 1 km', bool(closed),
        'horizons medians par vista : %s' % {v['id']: v['horizon']['horizon_band'] for v in report.get('vistas', [])})
    return d


def propose_candidates(meta, near, nz, nf, inside, water, placed):
    """Lieux que la topologie designe : point haut, bassin, sortie d'eau, cols du bord, centre bati."""
    c = []
    zin = np.where(inside, nz, np.nan)
    j, i = np.unravel_index(np.nanargmax(zin), zin.shape)
    c.append(('high_point', near.xy(i, j), float(nz[j, i])))
    tpi = np.where(inside, nf['tpi_large'], np.nan)
    j, i = np.unravel_index(np.nanargmin(tpi), tpi.shape)
    c.append(('deepest_basin', near.xy(i, j), float(nz[j, i])))
    b = [(x, y, z) for f, m, x, y, z, h, r in placed if f == 'building']
    if b:
        bx, by = np.mean([p[0] for p in b]), np.mean([p[1] for p in b])
        c.append(('village', (float(bx), float(by)), float(np.mean([p[2] for p in b]))))
    c.append(('forge_basin', tuple(meta['basin'][:2]), meta['basin'][2]))
    c.append(('forge_landmark', tuple(meta['landmark'][:2]), meta['landmark'][2]))
    return [{'id': n, 'x': p[0], 'y': p[1], 'z': z} for n, p, z in c]


def evaluate_vista(out, world, v, placed, meta):
    """FWorldVista mesuree : horizon, plans visibles, reperes, vide, bruit visuel (logiciel)."""
    eye = (v['x'], v['y'], v['z'])
    yaw, pitch, fov = v['yaw'], v.get('pitch', 0.0), v.get('fov', 75.0)
    sig = horizon_signature(world, eye, yaw, fov)
    img, dmap = render_vista(world, eye, yaw, pitch, fov)
    h, w = dmap.shape
    ground = np.isfinite(dmap)
    share = {name: float(((dmap >= lo) & (dmap < hi)).mean()) for name, lo, hi in BANDS}
    share['sky'] = float((~ground).mean())
    # Objets poses vus : projetes, comptes par plan ; hauteur angulaire = lisibilite d'une silhouette.
    seen = {name: 0 for name, _, _ in BANDS}
    pil = Image.fromarray(img)
    draw = ImageDraw.Draw(pil)
    stride = max(1, len(placed) // 40000)
    for f, m, x, y, z, hgt, rad in placed[::stride]:
        if f not in ('tree', 'building', 'ruin', 'rock'):
            continue
        p = project(eye, yaw, pitch, fov, w, h, x, y, z + hgt * 0.5)
        if not p or not (0 <= p[0] < w and 0 <= p[1] < h):
            continue
        d = math.hypot(x - eye[0], y - eye[1]) / UU_PER_M
        if d > dmap[int(p[1]), int(p[0])] + 30:
            continue
        seen[band_of(d)] += stride
        col = {'tree': (30, 70, 30), 'building': (200, 40, 40), 'ruin': (150, 60, 140), 'rock': (110, 110, 110)}[f]
        draw.point((p[0], p[1]), fill=col)
    # Bruit visuel : energie de gradient de la carte de profondeur (log) hors ciel -- un monde ou tout
    # est a la meme profondeur et emiette en petites ruptures est "bruyant" ; de grands plans = calme.
    ld = np.log(np.where(ground, dmap, 1e5))
    gy, gx = np.gradient(ld)
    noise = float(np.mean(np.hypot(gx, gy)[ground] > 0.15))
    landmarks = []
    for lm in v.get('landmarks', []):
        lz = lm.get('z')
        if lz is None:
            lz = float(world.height(np.array([lm['x']]), np.array([lm['y']]))[0])
        top = lz + lm.get('height', 1000.0)
        p = project(eye, yaw, pitch, fov, w, h, lm['x'], lm['y'], top)
        d = math.hypot(lm['x'] - eye[0], lm['y'] - eye[1]) / UU_PER_M
        vis = visible(world, eye, lm['x'], lm['y'], top)
        landmarks.append({'id': lm['id'], 'distance_m': round(d), 'in_frame': bool(p and 0 <= p[0] < w and 0 <= p[1] < h),
                          'visible': vis, 'screen': [round(p[0]), round(p[1])] if p else None,
                          'angular_height_deg': math.degrees(math.atan2(lm.get('height', 1000.0), max(d * UU_PER_M, 1)))})
        if p:
            draw.ellipse((p[0] - 4, p[1] - 4, p[0] + 4, p[1] + 4), outline=(255, 0, 0) if vis else (90, 90, 90))
    # Horizon trace par-dessus le rendu.
    sig_cols = len(sig['elevation_deg'])
    pts = []
    focal = (w / 2) / math.tan(math.radians(fov / 2))
    for c, e in enumerate(sig['elevation_deg']):
        off = -fov / 2 + fov * c / (sig_cols - 1)
        x = w / 2 + math.tan(math.radians(off)) * focal
        y = h / 2 - math.tan(math.radians(e - pitch)) * focal / math.cos(math.radians(off))
        pts.append((x, y))
    draw.line(pts, fill=(255, 255, 255), width=1)
    pil.save(os.path.join(out, 'vista_%s.png' % v['id']))
    return {'id': v['id'], 'camera': {'x': eye[0], 'y': eye[1], 'z': eye[2], 'yaw': yaw, 'pitch': pitch, 'fov': fov},
            'role': v.get('role'), 'screen_share': share, 'objects_seen_by_band': seen, 'depth_noise': noise,
            'horizon': {k: sig[k] for k in ('flatness_deg', 'roughness_deg', 'mean_layers', 'horizon_band')},
            'horizon_profile': {'elevation_deg': sig['elevation_deg'][::8], 'distance_m': sig['distance_m'][::8]},
            'landmarks': landmarks}


def save_maps(out, near, far, nz, fz, nf, ff, water, inside, placed, dmap, meta):
    def to_img(a, lo=None, hi=None):
        lo = np.nanpercentile(a, 2) if lo is None else lo
        hi = np.nanpercentile(a, 98) if hi is None else hi
        return np.clip((a - lo) / max(hi - lo, 1e-9), 0, 1)
    # Carte lointaine : relief ombre + teinte d'altitude, carte en cadre.
    hs = hillshade(fz, far.cell)
    alt = to_img(fz)
    rgb = np.stack([0.35 + 0.5 * alt, 0.4 + 0.35 * alt, 0.3 + 0.2 * alt], -1) * (0.35 + 0.65 * hs[..., None])
    im = Image.fromarray((np.clip(rgb, 0, 1)[::-1] * 255).astype(np.uint8))
    d = ImageDraw.Draw(im)
    mp = meta['map']
    i0, j0 = far.ij(mp['min'][0], mp['min'][1])
    i1, j1 = far.ij(mp['max'][0], mp['max'][1])
    d.rectangle((i0, far.h - 1 - j1, i1, far.h - 1 - j0), outline=(255, 40, 40))
    im.save(os.path.join(out, 'map_far_relief.png'))
    # Carte proche : relief ombre, cretes (TPI+) et bassins (TPI-), eau, objets.
    hs = hillshade(nz, near.cell)
    t = np.clip(nf['tpi_large'] / 40.0, -1, 1)
    rgb = np.stack([0.5 + 0.4 * np.clip(t, 0, 1), 0.5 - 0.15 * np.abs(t), 0.5 + 0.4 * np.clip(-t, 0, 1)], -1) * (0.3 + 0.7 * hs[..., None])
    rgb[water] = (0.15, 0.3, 0.6)
    img = (np.clip(rgb, 0, 1) * 255).astype(np.uint8)
    for f, m, x, y, z, h, r in placed:
        i, j = near.ij(x, y)
        if 0 <= i < near.w and 0 <= j < near.h:
            col = {'tree': (20, 90, 20), 'shrub': (90, 130, 50), 'rock': (60, 60, 60), 'building': (230, 30, 30),
                   'ruin': (170, 60, 170), 'place': (240, 150, 0)}.get(f)
            if col:
                img[int(j), int(i)] = col
    Image.fromarray(img[::-1]).save(os.path.join(out, 'map_near_topology.png'))
    dm = dmap.copy()
    dm[~inside[:dm.shape[0], :dm.shape[1]]] = 0
    Image.fromarray((to_img(dm, 0, 200) * 255).astype(np.uint8)[::-1]).save(os.path.join(out, 'map_near_negative_space.png'))


def emit_plan(plan, path, source):
    """Ecrit AnastasisWorldTheatrePlan.inl depuis un plan {masses, silhouettes} (coordonnees en uu)."""
    def v2(p):
        return 'FVector2D(%.0f, %.0f)' % (p[0], p[1])
    lines = ['// WORLD_THEATRE_001 -- plan de mise en scene. GENERE par tools/unreal/world-theatre-analyze.py --emit-plan ;',
             '// ne pas retoucher a la main : changer la regle dans l analyse, puis regenerer.',
             '// Inclus dans le corps de AnastasisWorldTheatre::CanonicalPlan() : la variable P (FPlan) est en portee.',
             '// Source : %s' % source]
    for m in plan.get('masses', []):
        lines.append('// %s' % m.get('why', ''))
        lines.append('{ FMass M; M.Id = TEXT("%s"); M.CanopyHeight = %.0f; M.EdgeRamp = %.0f;' % (m['id'], m['canopy'], m.get('edge_ramp', 2500)))
        c = m.get('colour', (0.035, 0.06, 0.03))
        lines.append('  M.Colour = FLinearColor(%.4ff, %.4ff, %.4ff);' % tuple(c))
        lines.append('  M.Outline = { %s };' % ', '.join(v2(p) for p in m['outline']))
        for h_ in m.get('holes', []):
            lines.append('  M.Holes.Add({ %s });' % ', '.join(v2(p) for p in h_))
        lines.append('  P.Masses.Add(MoveTemp(M)); }')
    for s_ in plan.get('silhouettes', []):
        lines.append('// %s' % s_.get('why', ''))
        lines.append('{ FSilhouetteSpec S; S.Id = TEXT("%s"); S.Kind = ESilhouette::%s; S.Location = %s; S.Yaw = %.1f; S.Scale = %.2f; P.Silhouettes.Add(S); }'
                     % (s_['id'], s_['kind'], v2(s_['xy']), s_.get('yaw', 0.0), s_.get('scale', 1.0)))
    with open(path, 'w', encoding='utf-8', newline='\r\n') as f:
        f.write('\n'.join(lines) + '\n')


# ---------------------------------------------------------------- composition (PHASE 4) : plan de masses

def components(mask):
    """Composantes 4-connexes d'un masque booleen : (etiquettes, tailles)."""
    h, w = mask.shape
    lab = np.zeros((h, w), np.int32)
    sizes = [0]
    n = 0
    for j0, i0 in zip(*np.nonzero(mask)):
        if lab[j0, i0]:
            continue
        n += 1
        stack = [(j0, i0)]
        lab[j0, i0] = n
        c = 0
        while stack:
            j, i = stack.pop()
            c += 1
            for jj, ii in ((j - 1, i), (j + 1, i), (j, i - 1), (j, i + 1)):
                if 0 <= jj < h and 0 <= ii < w and mask[jj, ii] and not lab[jj, ii]:
                    lab[jj, ii] = n
                    stack.append((jj, ii))
        sizes.append(c)
    return lab, sizes


def trace_loops(mask):
    """Contours d'une composante : aretes de cellules orientees (interieur a gauche), suivies en tournant a
    gauche aux pincements -- chaque boucle reste simple. Renvoie (contour exterieur, [trous]) en coins (i, j).
    Exterieur = la boucle d'aire signee la plus grande ; les autres boucles sont des clairieres."""
    m = np.pad(mask, 1)
    out = {}
    for j, i in zip(*np.nonzero(mask)):
        if not m[j, i + 1]:
            out.setdefault((i, j), []).append((i + 1, j))
        if not m[j + 1, i + 2]:
            out.setdefault((i + 1, j), []).append((i + 1, j + 1))
        if not m[j + 2, i + 1]:
            out.setdefault((i + 1, j + 1), []).append((i, j + 1))
        if not m[j + 1, i]:
            out.setdefault((i, j + 1), []).append((i, j))
    loops = []
    while out:
        start = next(iter(out))
        loop, p, d = [start], start, None
        while True:
            cands = out.get(p)
            if not cands:
                break
            if d is None or len(cands) == 1:
                q = cands[0]
            else:
                # tourner a gauche (produit vectoriel positif), sinon tout droit, sinon a droite
                def turn(c):
                    e = (c[0] - p[0], c[1] - p[1])
                    return -(d[0] * e[1] - d[1] * e[0])
                q = min(cands, key=turn)
            cands.remove(q)
            if not cands:
                del out[p]
            d = (q[0] - p[0], q[1] - p[1])
            p = q
            if p == start:
                break
            loop.append(p)
        if len(loop) >= 4:
            loops.append(loop)
    if not loops:
        return [], []

    def area(lp):
        a = np.array(lp, float)
        return 0.5 * float(np.sum(a[:, 0] * np.roll(a[:, 1], -1) - np.roll(a[:, 0], -1) * a[:, 1]))
    loops.sort(key=lambda lp: -abs(area(lp)))
    return loops[0], loops[1:]


def simplify(points, tol):
    """Douglas-Peucker sur une boucle fermee (tol en cellules)."""
    if len(points) < 8:
        return points
    pts = np.array(points, float)

    def dp(a, b):
        if b <= a + 1:
            return [a]
        seg = pts[b] - pts[a]
        L = np.hypot(*seg) or 1e-9
        d = np.abs(seg[0] * (pts[a + 1:b, 1] - pts[a, 1]) - seg[1] * (pts[a + 1:b, 0] - pts[a, 0])) / L
        k = int(np.argmax(d))
        if d[k] > tol:
            return dp(a, a + 1 + k) + dp(a + 1 + k, b)
        return [a]
    half = len(pts) // 2
    keep = dp(0, half) + dp(half, len(pts) - 1) + [len(pts) - 1]
    return [tuple(pts[k]) for k in sorted(set(keep))]


MASS_RULES = {
    'seam_min_trees_ha': 25.0,    # bord boise : plus de 25 arbres / ha dans la bande interieure de 200 m
    'seam_reach_m': 700.0,         # la foret de bord s'eteint sur ~700 m si le relief ne la porte pas
    # Run 1 (2026-10-07) : une masse a moins de 1,5 km se lit comme une bache verte (V5, a 400 m) ; la
    # continuite proche releve des vraies instances, pas d'une enveloppe. Couture coupee, portee minimale 2,5 km.
    'seam_weight': 0.0,
    'min_reach_m': 2500.0,
    'max_reach_m': 9000.0,         # au-dela, la perspective aerienne (autre proprietaire) efface tout
    'village_clearing_m': 1500.0,  # terroir du village : champs et paturages, pas de foret
    'massing_m': 160.0,            # une masse se decide a 160 m, pas a l'arbre
    'threshold': 0.18,
    'min_mass_ha': 15.0,
    'max_masses': 14,
    'canopy_uu': 2000.0,
    'edge_ramp_uu': 3000.0,
    # Asymetrie voulue : le relief de l'anneau monte autour de la carte dans TOUTES les directions (cuvette) ;
    # une regle purement topographique dessinerait une couronne de foret centree sur la carte. La foret tient
    # le pied de la chaine (nord-est, ou la pente mene vraiment a la montagne) ; le sud-ouest, ou la vallee
    # s'ouvre a 22-38 km, reste ouvert : c'est le vide qui fait lire l'ouverture.
    'range_azimuth_deg': 45.0,
    'forest_half_cone_deg': 75.0,
    'open_half_cone_deg': 115.0,
}


def place_landmark(world, near, nz, nf, water, meta, eyes, vistas, rules, hidden_from=(), cover=None):
    """Un seul repere humain, la ou il se decoupe sur le ciel : depuis chaque oeil (village, approche), le sommet
    de la tour (sol + hauteur) doit etre visible ET au-dessus de tout le relief qui le suit sur le meme rayon.
    Score = somme sur les yeux de la hauteur angulaire (deg) x (1 + marge sur l'horizon, deg), bonus s'il tombe
    dans le cadre d'une vista canonique. Candidats : bosses (TPI 300 m > 2 m), pente faible, sec, hors carte."""
    mp = meta['map']
    k = 5  # 100 m
    z = nz[::k, ::k]
    h, w = z.shape
    ii, jj = np.meshgrid(np.arange(w), np.arange(h))
    X = near.ox + (ii * k + 0.5 * k) * near.cell
    Y = near.oy + (jj * k + 0.5 * k) * near.cell
    zero = np.zeros_like(X)
    dmap = np.hypot(np.maximum.reduce([mp['min'][0] - X, X - mp['max'][0], zero]),
                    np.maximum.reduce([mp['min'][1] - Y, Y - mp['max'][1], zero])) / UU_PER_M
    cand = ((dmap >= rules['landmark_min_m']) & (dmap <= rules['landmark_max_m']) & (nf['tpi_small'][::k, ::k] > 2.0)
            & (nf['slope'][::k, ::k] < 10.0) & (~water[::k, ::k][:h, :w]))
    js, is_ = np.nonzero(cand)
    top_h = rules['landmark_height_uu']
    dist = ray_distances()
    best = None
    scored = []
    for j, i in zip(js, is_):
        x, y = float(X[j, i]), float(Y[j, i])
        gz = float(z[j, i])
        tot, detail = 0.0, []
        for name, e in eyes:
            d = math.hypot(x - e[0], y - e[1])
            if d < 30000:
                continue
            if not visible(world, e, x, y, gz + top_h, steps=300):
                detail.append((name, 'cache'))
                continue
            ang_top = math.degrees(math.atan2(gz + top_h - e[2], d))
            ang_h = math.degrees(math.atan2(top_h, d))
            # relief au-dela de la tour, sur le meme rayon : la tour doit le depasser (silhouette sur le ciel)
            far_d = dist[dist > d + 2000]
            ux, uy = (x - e[0]) / d, (y - e[1]) / d
            zz = world.height(e[0] + ux * far_d, e[1] + uy * far_d) - far_d ** 2 / (2 * 6.371e8)
            beyond = float(np.max(np.degrees(np.arctan2(zz - e[2], far_d)))) if len(far_d) else -90.0
            margin = ang_top - beyond
            if margin > 0:
                tot += ang_h * (1.0 + min(margin, 2.0))
                detail.append((name, 'ciel %.2f deg, haut %.2f deg' % (margin, ang_h)))
                continue
            # Pas de ciel derriere : le fond est le premier relief que le rayon touche au-dela de la tour, a la
            # hauteur de son milieu. Une tour de pierre claire se lit sur une masse sombre du theatre.
            if cover is None or len(far_d) == 0:
                detail.append((name, 'sur fond de relief'))
                continue
            ang_mid = math.degrees(math.atan2(gz + top_h * 0.5 - e[2], d))
            hit = np.nonzero(np.degrees(np.arctan2(zz - e[2], far_d)) >= ang_mid)[0]
            if not len(hit):
                detail.append((name, 'sur fond de relief'))
                continue
            bx, by = e[0] + ux * far_d[hit[0]], e[1] + uy * far_d[hit[0]]
            bi, bj = near.ij(bx, by)
            bi, bj = int(round(bi)), int(round(bj))
            if 0 <= bi < near.w and 0 <= bj < near.h and cover[bj, bi] > 0:
                tot += 0.8 * ang_h
                detail.append((name, 'sur fond de foret a %.1f km, haut %.2f deg' % (far_d[hit[0]] / 1e5, ang_h)))
            else:
                detail.append((name, 'sur fond de relief nu'))
        if tot <= 0:
            continue
        # Revele, pas simplement visible : bonus si la tour reste cachee depuis le village (on la decouvre en montant).
        hidden = [n for n, e in hidden_from if not visible(world, e, x, y, gz + top_h, steps=300)]
        if hidden_from and len(hidden) == len(hidden_from):
            tot *= 1.0 + rules['concealment_bonus']
            detail.append(('cache depuis', ','.join(hidden)))
        framed = []
        for v in vistas:
            p = project((v['x'], v['y'], v['z']), v['yaw'], v.get('pitch', 0), v.get('fov', 75), 1166, 856, x, y, gz + top_h)
            if p and 40 <= p[0] < 1126 and 40 <= p[1] < 816:
                framed.append(v['id'])
        score = tot * (1.5 if framed else 1.0)
        scored.append((score, x, y, gz, detail, framed))
    scored.sort(key=lambda t: -t[0])
    return scored[:5], int(cand.sum())


LANDMARK_RULES = {
    'landmark_min_m': 600.0,     # hors de la carte, mais assez pres pour qu'une tour de 15 m se lise (> 6 px a 2 km)
    'landmark_max_m': 3500.0,
    'landmark_height_uu': 1500.0,
    'concealment_bonus': 0.5,
    # 0,35 deg = 5 px a 1166 px / 75 deg, 9 px a 1920 : en dessous, une tour de 15 m n'est qu'une poussiere.
    'min_angular_height_deg': 0.35,
}


def compose_masses(near, nz, nf, water, inside, placed, meta, village_xy, rules):
    """Masses forestieres de l'avant-pays : aptitude mesuree -> composantes -> hierarchie -> contours."""
    cell_m = near.cell / UU_PER_M
    k = 2  # grille de 40 m
    z = nz[::k, ::k]
    h, w = z.shape
    slope = nf['slope'][::k, ::k]
    tpi_s = nf['tpi_small'][::k, ::k]
    tpi_l = nf['tpi_large'][::k, ::k]
    wet = water[::k, ::k][:h, :w]
    ins = inside[::k, ::k][:h, :w]
    ii, jj = np.meshgrid(np.arange(w), np.arange(h))
    X = near.ox + (ii * k + 0.5 * k) * near.cell
    Y = near.oy + (jj * k + 0.5 * k) * near.cell
    gy, gx = np.gradient(z, near.cell * k)
    # Face nord (ECO-01 : versants nord humides). Axes du ciel du projet (AnastasisAtmosphereResolver) : X = nord,
    # Y = est. Un versant qui descend vers +X (dz/dx < 0) regarde le nord. (Runs 1-3 : +Y pris pour le nord, corrige.)
    north = np.clip(-gx / np.maximum(np.hypot(gx, gy), 1e-9), -1, 1) * np.clip(slope / 10.0, 0, 1)
    mp = meta['map']
    zero = np.zeros_like(X)
    dist_map_m = np.hypot(np.maximum.reduce([mp['min'][0] - X, X - mp['max'][0], zero]),
                          np.maximum.reduce([mp['min'][1] - Y, Y - mp['max'][1], zero])) / UU_PER_M
    dist_village_m = np.hypot(X - village_xy[0], Y - village_xy[1]) / UU_PER_M
    # Aptitude forestiere (PONT-ECO-01) : versant, creux humide, face nord ; pas le fond plat, pas la crete.
    versant = np.clip((slope - 3.0) / 6.0, 0, 1) * np.clip((32.0 - slope) / 8.0, 0, 1)
    creux = np.clip(-tpi_s / 6.0, 0, 1) * np.clip(slope / 4.0, 0, 1)
    crete = np.clip((tpi_s - 4.0) / 6.0, 0, 1)
    fond = np.clip((3.0 - slope) / 2.0, 0, 1) * np.clip(-tpi_l / 8.0, 0, 1)
    apt = 0.55 * versant + 0.35 * creux + 0.25 * north - 0.8 * crete - 0.7 * fond
    # Couture : la foret de la carte continue la ou la carte est boisee jusqu'au bord.
    trees = np.array([(x, y) for f, m_, x, y, zz, hh, rr in placed if f == 'tree'])
    seam = np.zeros_like(apt)
    seam_report = []
    if len(trees):
        band = 200.0 * UU_PER_M
        bins = np.arange(mp['min'][0], mp['max'][0] + 12000.0, 12000.0)
        for side in range(4):
            if side == 0:
                sel = trees[:, 1] < mp['min'][1] + band; along = trees[sel, 0]
            elif side == 1:
                sel = trees[:, 1] > mp['max'][1] - band; along = trees[sel, 0]
            elif side == 2:
                sel = trees[:, 0] < mp['min'][0] + band; along = trees[sel, 1]
            else:
                sel = trees[:, 0] > mp['max'][0] - band; along = trees[sel, 1]
            hist, _ = np.histogram(along, bins=bins)
            dens = hist / (120.0 * 200.0 / 1e4)  # arbres / ha
            wooded = np.clip((dens - rules['seam_min_trees_ha']) / 30.0, 0, 1)
            seam_report.append([round(float(d), 1) for d in dens])
            if side in (0, 1):
                idx = np.clip(((X - bins[0]) / 12000.0).astype(int), 0, len(wooded) - 1)
                on_side = (Y < mp['min'][1]) if side == 0 else (Y > mp['max'][1])
                within = (X >= mp['min'][0]) & (X <= mp['max'][0])
            else:
                idx = np.clip(((Y - bins[0]) / 12000.0).astype(int), 0, len(wooded) - 1)
                on_side = (X < mp['min'][0]) if side == 2 else (X > mp['max'][0])
                within = (Y >= mp['min'][1]) & (Y <= mp['max'][1])
            seam = np.maximum(seam, np.where(on_side & within, wooded[idx] * np.exp(-dist_map_m / rules['seam_reach_m']), 0))
    az = np.degrees(np.arctan2(Y - (mp['min'][1] + mp['max'][1]) / 2, X - (mp['min'][0] + mp['max'][0]) / 2))
    off = np.abs((az - rules['range_azimuth_deg'] + 180.0) % 360.0 - 180.0)
    w_dir = np.clip((rules['open_half_cone_deg'] - off) / (rules['open_half_cone_deg'] - rules['forest_half_cone_deg']), 0, 1)
    # La couture ne depend pas de la direction : la foret de la carte continue la ou elle touche le bord.
    apt = np.maximum(apt * w_dir - (1 - w_dir) * 0.5, rules['seam_weight'] * seam - 0.2 * fond)
    allowed = (~ins) & (dist_map_m >= rules['min_reach_m']) & (dist_map_m <= rules['max_reach_m']) & (~wet) & (dist_village_m > rules['village_clearing_m'])
    apt = np.where(allowed, apt, 0.0)
    rad = max(1, int(round(rules['massing_m'] / (cell_m * k))))
    # Flou normalise sur la seule zone permise : la carte (exclue) ne tire pas la lisiere vers le bas.
    apt_s = blur(apt, rad) / np.maximum(blur(allowed.astype(float), rad), 1e-6)
    mask = (apt_s > rules['threshold']) & allowed
    lab, sizes = components(mask)
    cell_ha = (cell_m * k) ** 2 / 1e4
    order = sorted(range(1, len(sizes)), key=lambda c: -sizes[c])
    masses = []
    for c in order:
        ha = sizes[c] * cell_ha
        if ha < rules['min_mass_ha'] or len(masses) >= rules['max_masses']:
            break
        comp = lab == c
        loop, holes = trace_loops(comp)
        if len(loop) < 4:
            continue
        to_uu = lambda lp: [(near.ox + p[0] * k * near.cell, near.oy + p[1] * k * near.cell) for p in simplify(lp, 1.2)]
        outline = to_uu(loop)
        # Clairieres de plus d'un hectare : la masse garde ses vides (cretes nues, combes ouvertes).
        hole_list = [to_uu(h) for h in holes if len(h) >= 4 and abs(0.5 * sum(h[n][0] * h[(n + 1) % len(h)][1] - h[(n + 1) % len(h)][0] * h[n][1]
                                                                     for n in range(len(h)))) * (cell_m * k) ** 2 >= 1e4]
        cy, cx = np.argwhere(comp).mean(0)
        reasons = ['versant %.0f%%' % (100 * np.mean(versant[comp] > 0.5)),
                   'creux %.0f%%' % (100 * np.mean(creux[comp] > 0.3)),
                   'face nord %.0f%%' % (100 * np.mean(north[comp] > 0.3)),
                   'couture %.0f%%' % (100 * np.mean(seam[comp] > 0.3)),
                   'a %.1f km de la carte' % float(np.median(dist_map_m[comp]) / 1000.0)]
        masses.append({'id': 'mass_%02d' % (len(masses) + 1), 'ha': round(ha, 1), 'outline': outline, 'holes': hole_list,
                       'canopy': rules['canopy_uu'], 'edge_ramp': rules['edge_ramp_uu'],
                       'centre': (float(near.ox + (cx * k + 0.5 * k) * near.cell), float(near.oy + (cy * k + 0.5 * k) * near.cell)),
                       'why': 'masse %d (%.0f ha) : %s' % (len(masses) + 1, ha, ', '.join(reasons))})
    stats = {'allowed_ha': float(allowed.sum() * cell_ha), 'forest_ha': float(sum(m['ha'] for m in masses)),
             'components': len(sizes) - 1, 'kept': len(masses), 'seam_trees_per_ha_by_side_S_N_W_E': seam_report}
    return masses, stats, (apt_s, mask, lab, k)


# ---------------------------------------------------------------- lumiere (v2.1) : la valeur par plan de distance

def canopy_world(near, far, nz, fz, placed):
    """Relief + hauteur des objets poses (arbres, arbustes, batiments, ruines) : ce qui arrete vraiment le regard."""
    canopy = nz.copy()
    for f, m_, x, y, zz, hh, rr in placed:
        if f not in ('tree', 'shrub', 'building', 'ruin'):
            continue
        ci, cj = near.ij(x, y)
        r_cells = max(0, int(rr / near.cell))
        for dj in range(-r_cells, r_cells + 1):
            for di in range(-r_cells, r_cells + 1):
                ii_, jj_ = int(round(ci)) + di, int(round(cj)) + dj
                if 0 <= ii_ < near.w and 0 <= jj_ < near.h:
                    canopy[jj_, ii_] = max(canopy[jj_, ii_], zz + hh)
    return World(near, far, canopy, fz)


def srgb_to_linear(c):
    c = c / 255.0
    return np.where(c <= 0.04045, c / 12.92, ((c + 0.055) / 1.055) ** 2.4)


# Plans de lecture de la lumiere : PROCHE < 60 m, MOYEN 60 m - 1 km, LOIN 1 - 8 km, HORIZON > 8 km, CIEL.
def luminance_by_plane(folder, vistas, world):
    """Pour chaque image <vista>_<etat>.png : luminance relative (sRGB linearise, Rec.709) mediane par plan,
    saturation mediane, rapport bleu/rouge (froideur). La profondeur vient du rendu logiciel de la MEME camera
    (relief + canopee releves) : approximative aux bords d'un plan, robuste en mediane."""
    out = {}
    for v in vistas:
        files = sorted(f for f in os.listdir(folder) if f.startswith(v['id'] + '_') and f.endswith('.png'))
        if not files:
            continue
        img0 = Image.open(os.path.join(folder, files[0]))
        W, H = img0.size
        probe = os.path.join(folder, v['id'] + '_depth.png')
        if os.path.exists(probe):
            # Sonde M_WorldTheatreDepthProbe : gris = (log2(metres) + 1) / 18, ecrit apres le tonemapper. Profondeur VRAIE
            # du pixel, feuillage compris. >= 0,995 : au-dela de 120 km, c'est le ciel.
            g = np.asarray(Image.open(probe).convert('L')).astype(float) / 255.0
            dmap = np.where(g >= 0.995, np.inf, 2.0 ** (g * 18.0 - 1.0))
            source = 'sonde'
        else:
            _, dmap = render_vista(world, (v['x'], v['y'], v['z']), v['yaw'], v.get('pitch', 0.0), v.get('fov', 75.0), W // 2, H // 2)
            dmap = np.kron(dmap, np.ones((2, 2)))[:H, :W]
            if dmap.shape != (H, W):
                dmap = np.pad(dmap, ((0, H - dmap.shape[0]), (0, W - dmap.shape[1])), mode='edge')
            source = 'logiciel'
        masks = {name: (dmap >= lo) & (dmap < hi) for name, lo, hi in BANDS}
        masks['sky'] = ~np.isfinite(dmap)
        out[v['id']] = {'_depth': source}
        for f in files:
            state = f[len(v['id']) + 1:-4]
            if state == 'depth':
                continue
            rgb = srgb_to_linear(np.asarray(Image.open(os.path.join(folder, f)).convert('RGB')).astype(float))
            Y = 0.2126 * rgb[..., 0] + 0.7152 * rgb[..., 1] + 0.0722 * rgb[..., 2]
            mx, mn = rgb.max(-1), rgb.min(-1)
            sat = np.where(mx > 1e-4, (mx - mn) / np.maximum(mx, 1e-4), 0)
            row = {}
            for name, m in masks.items():
                if m.mean() < 0.005:
                    continue
                row[name] = {'share': round(float(m.mean()), 3), 'Y': round(float(np.median(Y[m])), 4),
                             'sat': round(float(np.median(sat[m])), 3),
                             'cool': round(float(rgb[..., 2][m].mean() / max(rgb[..., 0][m].mean(), 1e-4)), 3)}
            ref = row.get('mid') or row.get('near')
            for name in ('far', 'extreme'):
                if name in row and ref:
                    row[name]['vs_mid'] = round(row[name]['Y'] / max(ref['Y'], 1e-4), 2)
            out[v['id']][state] = row
    return out


def print_luminance(res):
    for vid, states in res.items():
        for state, row in states.items():
            if state.startswith('_'):
                continue
            cells = ['%s Y=%.3f s=%.2f c=%.2f%s' % (k, r['Y'], r['sat'], r['cool'], (' x%.2f' % r['vs_mid']) if 'vs_mid' in r else '')
                     for k, r in row.items()]
            print('%-24s %-5s %s' % (vid, state, ' | '.join(cells)))


if __name__ == '__main__':
    main()
