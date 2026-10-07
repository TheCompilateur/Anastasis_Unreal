"""WORLD_THEATRE_001 -- lecture perceptuelle du monde, HORS moteur (Python systeme, numpy + Pillow).

Entree : le releve de world-theatre-read.py (reading.json, grilles .f32, placed.csv).
Sortie (dans --out, defaut <releve>/analysis) :
  perception.json   derives par cellule resumes, signatures anti-generatives, vistas et leurs horizons
  map_*.png         cartes : relief ombre, crêtes et bassins, densite des objets, vides, champs de vue
  vista_<id>.png    rendu logiciel de chaque vista (plans NEAR / MID / FAR / EXTREME colores), meme camera
                    que la capture Unreal : on juge une composition en secondes avant de payer l'editeur
Rien n'est ecrit dans le projet.

Usage : python tools/unreal/world-theatre-analyze.py <dossier du releve> [--out <dossier>] [--vistas <json>]
  --vistas : fichier de vistas canoniques (world-theatre-vistas.json) ; sans lui, les candidats sont
             seulement proposes (perception.json -> vista_candidates).

Noms des structures (le jour ou une couche les consomme, ce sont ceux du C++) :
  FWorldPerceptualSample  une cellule : altitude, pente, courbure, relief local (TPI), eau, densites
  FHorizonSignature       par vista : angle d'elevation et distance de l'horizon par azimut, plans de silhouette
  FWorldVista             camera, FOV, plans, repere dominant, vides, bruit visuel
  FLandmarkRelation       repere x vista : visible, angle, part du champ, concurrence
"""
import argparse, csv, json, math, os, sys

import numpy as np
from PIL import Image, ImageDraw

NODATA = -1.0e29
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
        for ring in range(1, 4):
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

    def __init__(self, near, far, near_z, far_z):
        self.near, self.far, self.nz, self.fz = near, far, near_z, far_z

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
                col = colours[min(band_idx[k], 3)] * shade[c, k]
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
    a = ap.parse_args()
    out = a.out or os.path.join(a.reading, 'analysis')
    os.makedirs(out, exist_ok=True)
    meta, near, far, placed = load(a.reading)
    nz, fz = fill_nan(near.ground), fill_nan(far.ground)
    world = World(near, far, nz, fz)
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
            vistas = json.load(f)['vistas']
    report['vista_candidates'] = propose_candidates(meta, near, nz, nf, inside, water_near, placed)
    report['vistas'] = []
    for v in vistas:
        report['vistas'].append(evaluate_vista(out, world, v, placed, meta))
    with open(os.path.join(out, 'perception.json'), 'w', encoding='utf-8') as f:
        json.dump(report, f, indent=1)
    print('WORLD_THEATRE_ANALYZE OK out=%s vistas=%d' % (out, len(report['vistas'])))


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


if __name__ == '__main__':
    main()
