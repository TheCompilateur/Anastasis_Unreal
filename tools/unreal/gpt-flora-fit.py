"""GPT_FLORA_001 -- des sprites GPT aux specs de maillage : silhouette, bois, atlas, et controle logiciel.

**Python systeme, hors Unreal** (Pillow + numpy). Entree : les treize PNG detoures de
`SourceArt/Vegetation/gpt/PNG/` (planche generee par GPT, decoupee par `gpt-flora-cut.py`).
Sorties :
  SourceArt/Vegetation/gpt/gpt-flora.json     un spec par espece (voir gpt_flora_geometry.py)
  SourceArt/Vegetation/gpt/gpt-flora-atlas.png  8 x 4 cases de 256 : deux gerbes par espece
  docs/visual/sprites-vegetation-gpt-001/      planche de l'atlas, rendu logiciel face / profil, IoU

COMMENT UN SPRITE DEVIENT UN VOLUME. Un sprite est une vue de face : sa silhouette dit, a chaque hauteur,
jusqu'ou la couronne s'etend a gauche et a droite du fut. On en tire des TRANCHES (centre, demi-largeur) et
on admet la couronne de revolution (demi-profondeur = 0,92 x demi-largeur). Les cartes de feuillage sont
posees sur la coquille de ce volume, plus un remplissage interieur plus sombre ; le fut est mesure sur le
sprite (largeur, axe), les branches partent de l'axe vers les tranches. Les PIXELS du sprite coloriaient la
gerbe : chaque case de l'atlas est une pastille du sprite (couleur, ombres, fleurs), decoupee par un
masque alpha dessine a l'echelle de la feuille de l'espece.

CE QUE CE N'EST PAS. Pas une reconstruction neuronale : la face cachee est une hypothese (revolution), pas une
mesure. Le controle logiciel dit ce que le banc sait dire : la silhouette de face colle-t-elle au sprite (IoU),
le volume se lit-il de profil. Le verdict de beaute est celui d'Alexandre, a l'image.

Lancer : python tools/unreal/gpt-flora-fit.py   (deterministe : graines fixes)
"""
import json
import math
import os
import random
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFilter

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import gpt_flora_geometry as geo  # noqa: E402

ROOT = os.path.abspath(os.path.join(HERE, '..', '..'))
SRC = os.path.join(ROOT, 'SourceArt', 'Vegetation', 'gpt')
PNG_DIR = os.path.join(SRC, 'PNG')
DOC = os.path.join(ROOT, 'docs', 'visual', 'sprites-vegetation-gpt-001')
CELL = 256
COLS, ROWS = 8, 4
DETAIL_MEAN = 0.45          # egal a DETAIL_MEAN de leaf-atlas.py et de create-tree-cards.py
CLIP = 0.42
ALBEDO_GAIN = 0.75          # les sprites sont des images eclairees : l'albedo est plus bas que le pixel

# nom : (famille, mode de pose, feuille / cellule, carte / largeur du houppier, densite, hauteur adulte m, accent)
# leaf = taille de la feuille en fraction de la pastille ; card = longueur de carte en fraction de la largeur max.
TABLE = {
    'marronnier_fleuri': dict(kind='broadleaf', mode='crown', leaf=.30, card=.15, density=1.5, height_m=14.0, accent='bloom', limb_r=.55),
    'bouleau': dict(kind='broadleaf', mode='crown', leaf=.11, card=.12, density=1.7, height_m=15.0, accent=None, limb_r=.38),
    'chene': dict(kind='broadleaf', mode='crown', crown_low=.42, leaf=.17, card=.14, density=1.5, height_m=17.0, accent=None, limb_r=.75),
    'pin_sombre': dict(kind='conifer', mode='crown', leaf=.12, card=.13, density=1.5, height_m=20.0, accent=None, limb_r=.45),
    'pin_sylvestre': dict(kind='conifer', mode='crown', leaf=.12, card=.14, density=1.5, height_m=24.0, accent=None, limb_r=.40),
    'cypres': dict(kind='scale', mode='column', leaf=.08, card=.22, density=1.7, height_m=16.0, accent=None, limb_r=.3),
    'genevrier': dict(kind='scale', mode='shrub', leaf=.10, card=.13, density=1.6, height_m=4.5, accent=None, stems=6),
    'noisetier': dict(kind='broadleaf', mode='shrub', leaf=.17, card=.17, density=1.5, height_m=5.0, accent=None, stems=7),
    'saule_pleureur': dict(kind='strands', gain=.60, mode='weeping', leaf=.10, card=.40, density=1.6, height_m=14.0, accent=None, limb_r=.6),
    'arbuste_baies': dict(kind='broadleaf', mode='shrub', leaf=.17, card=.19, density=1.6, height_m=2.5, accent=None, stems=8),
    'rhododendron': dict(kind='broadleaf', mode='shrub', leaf=.22, card=.20, density=1.6, height_m=3.0, accent='bloom', stems=8),
    'fougere': dict(kind='frond', mode='fronds', leaf=.12, card=.95, density=1.0, height_m=1.0, accent=None, count=30),
    'prairie_fleurie': dict(kind='blades', mode='tufts', leaf=.10, card=1.0, density=1.0, height_m=1.1, accent='flowers', count=64),
}
ORDER = list(TABLE)


def log(msg):
    print('[gpt-flora-fit] ' + str(msg))


def s2l(c):
    c = np.asarray(c, np.float32) / 255.0
    return np.where(c <= 0.04045, c / 12.92, ((c + 0.055) / 1.055) ** 2.4)


def l2s(c):
    c = np.clip(c, 0, 1)
    return np.where(c <= 0.0031308, c * 12.92, 1.055 * np.power(c, 1 / 2.4) - 0.055) * 255.0


# ------------------------------------------------------------------------------------------ analyse
def runs_at(mask_row, x):
    """Plage contigue de vrais autour de x (ou None)."""
    n = len(mask_row)
    x = int(min(max(x, 0), n - 1))
    if not mask_row[x]:
        # plus proche vrai dans +-6 px
        near = [i for i in range(max(0, x - 6), min(n, x + 7)) if mask_row[i]]
        if not near:
            return None
        x = min(near, key=lambda i: abs(i - x))
    a = x
    while a > 0 and mask_row[a - 1]:
        a -= 1
    b = x
    while b < n - 1 and mask_row[b + 1]:
        b += 1
    return a, b + 1


def analyse(name, img):
    cfg = TABLE[name]
    a = np.asarray(img).astype(np.float32)
    h, w = a.shape[:2]
    al = a[..., 3] / 255.0
    m = al > 0.5
    unit = 100.0 / h                      # unites de dessin par pixel
    ys, xs = np.where(m)
    # pivot : centre de masse du pied (hors touffe d'herbe : on prend la bande 0.80h..0.95h)
    band = m[int(.80 * h):int(.95 * h)]
    bx = np.where(band)[1]
    pivot = float(np.median(bx)) if bx.size else w / 2.0
    crown_w = float(np.percentile(xs, 99) - np.percentile(xs, 1))
    info = {'h': h, 'w': w, 'pivot': pivot, 'crown_w': crown_w}
    # fut
    trunk, trunk_px, crown_y = [], [], int(.92 * h)
    has_trunk = cfg['mode'] in ('crown', 'column', 'weeping')
    if has_trunk:
        y0 = int(.84 * h)
        run = runs_at(m[y0], pivot)
        wid0 = (run[1] - run[0]) if run else 6
        cx = (run[0] + run[1]) / 2.0 if run else pivot
        pts = [(cx, y0, wid0)]
        y = y0
        wmin = wid0
        while y > int(.12 * h):
            y -= 3
            r = runs_at(m[y], cx)
            if r is None:
                break
            wd = r[1] - r[0]
            if wd > max(1.5 * wmin, 14):
                break
            wmin = min(wmin, wd)
            cx = .7 * cx + .3 * (r[0] + r[1]) / 2.0
            pts.append((cx, y, wmin))              # largeur monotone : un fut ne s'elargit pas vers le haut
        crown_y = pts[-1][1] if len(pts) > 1 else int(.55 * h)
        crown_y = min(crown_y, int(.78 * h))
        if 'crown_low' in cfg:                   # le releve du fut s'arrete trop haut : fut au plus a cette fraction
            crown_y = max(crown_y, int((1.0 - cfg['crown_low']) * h))
        if cfg['mode'] == 'column':
            crown_y = int(.93 * h)
        for (px, py, wd) in pts:
            if py >= crown_y:
                trunk_px.append((px, py, wd))
        # pixels de fut pour la couleur de l'ecorce
        cols = []
        for (px, py, wd) in trunk_px[:: max(1, len(trunk_px) // 8)]:
            x0, x1 = int(px - wd * .3), int(px + wd * .3) + 1
            cols.append(a[py, x0:x1, :3])
        info['bark_px'] = np.concatenate(cols, 0) if cols else None
        info['trunk_w_px'] = float(np.median([t[2] for t in trunk_px])) if trunk_px else 6.0
    else:
        crown_y = int(.93 * h)
    info['crown_y'] = crown_y
    # tranches de couronne
    top_y = int(ys.min())
    nb = 10 if cfg['mode'] in ('crown', 'weeping', 'shrub') else (12 if cfg['mode'] == 'column' else 1)
    bands = []
    ya_all, yb_all = top_y, crown_y if cfg['mode'] != 'shrub' else int(.94 * h)
    if cfg['mode'] in ('fronds', 'tufts'):
        ya_all, yb_all = top_y, int(.96 * h)
    edges = np.linspace(ya_all, yb_all, nb + 1)
    depth_ratio = .92
    for i in range(nb):
        ya, yb = int(edges[i]), max(int(edges[i + 1]), int(edges[i]) + 1)
        sel = m[ya:yb]
        bx = np.where(sel)[1]
        if bx.size < 6:
            continue
        xl, xr = np.percentile(bx, 1.5), np.percentile(bx, 98.5)
        zc = 50.0 - (ya + yb) * .5 * unit
        hh = (yb - ya) * unit
        hw = max((xr - xl) * .5 * unit, .6)
        cxu = ((xl + xr) * .5 - pivot) * unit
        fill = float(sel[:, int(xl):int(xr) + 1].sum()) / max(1.0, (xr - xl + 1) * (yb - ya))
        bands.append([round(zc, 3), round(hh, 3), round(cxu, 3), 0.0, round(hw, 3), round(hw * depth_ratio, 3), round(fill, 3)])
    # le fut en unites de dessin
    trunk_u = []
    if trunk_px:
        base_r = trunk_px[0][2] * .5 * unit
        for k, (px, py, wd) in enumerate(reversed(trunk_px)):      # du pied vers le haut
            z = 50.0 - py * unit
            r = max(wd * .5 * unit, .5)
            trunk_u.append([round((px - pivot) * unit, 3), 0.0, round(z, 3), round(r * (1.25 if k == 0 else 1.0), 3)])
        trunk_u[0][2] = -50.0
        trunk_u.insert(0, [trunk_u[0][0], 0.0, -50.0, round(base_r * 1.35, 3)])
        # prolonger le fut dans la couronne (les branches en naissent) : jusqu'a 60 % ou 92 % de la couronne
        if bands:
            top_b = bands[0][0] + bands[0][1] * .5
            low_b = bands[-1][0] - bands[-1][1] * .5
            reach = .92 if cfg['kind'] == 'conifer' else .62
            z_top = low_b + (top_b - low_b) * reach
            near = min(bands, key=lambda b: abs(b[0] - z_top))
            last = trunk_u[-1]
            r_top = max(last[3] * .45, .4)
            trunk_u.append([round(near[2] * .55, 3), 0.0, round(z_top, 3), round(r_top, 3)])
            if cfg['kind'] == 'conifer':
                trunk_u[-1][3] = .3
    # au plus 6 points : le fut est une polyligne de quelques troncons, pas un releve pixel par pixel
    if len(trunk_u) > 6:
        keep = sorted(set(int(round(i)) for i in np.linspace(0, len(trunk_u) - 1, 6)))
        trunk_u = [trunk_u[i] for i in keep]
    if cfg['mode'] == 'column' and trunk_u:
        trunk_u = trunk_u[:2]
        trunk_u[-1][2] = min(trunk_u[-1][2], -30.0)
    cap = (0.08 if name == 'chene' else 0.07) * crown_w * unit
    for t in trunk_u:
        t[3] = round(min(t[3], cap), 3)
    info['bands'] = bands
    info['trunk'] = trunk_u
    return info


# ----------------------------------------------------------------------------------------- pastilles
def box_mean(a, k):
    """Moyenne glissante k x k par sommes cumulees ; sortie de meme forme, valide au centre."""
    c = np.cumsum(np.cumsum(np.pad(a, ((1, 0), (1, 0))), 0), 1)
    out = (c[k:, k:] - c[:-k, k:] - c[k:, :-k] + c[:-k, :-k]) / float(k * k)
    return out


def pick_patch(a, name, which, accent_score, rng):
    h, w = a.shape[:2]
    al = a[..., 3] / 255.0
    ys, xs = np.where(al > .5)
    cw = float(np.percentile(xs, 99) - np.percentile(xs, 1))
    cfg = TABLE[name]
    ps = int(max(24, min(cw * cfg['card'] * 1.0, .5 * min(h, w))))
    dense = box_mean((al > .9).astype(np.float32), ps)
    sy, sx = dense.shape
    cand_y, cand_x = np.where(dense > (.96 if cfg['mode'] not in ('tufts', 'fronds') else .55))
    if cand_y.size == 0:
        cand_y, cand_x = np.where(dense > dense.max() * .85)
    top = float(ys.min())
    bot = float(ys.max())
    cyc = cand_y + ps / 2.0
    if which == 'A':
        sel = cyc < top + .55 * (bot - top)
    else:
        sel = cyc >= top + .35 * (bot - top)
    if accent_score is not None and which == 'B':
        sc = box_mean(accent_score, ps)[:sy, :sx]
        vals = sc[cand_y, cand_x]
        k = int(np.argmax(vals))
        return int(cand_x[k]), int(cand_y[k]), ps
    if not sel.any():
        sel = np.ones_like(cand_y, bool)
    idx = np.where(sel)[0]
    k = int(rng.choice(idx))
    return int(cand_x[k]), int(cand_y[k]), ps


def accent_map(a, fol_hue):
    rgb = a[..., :3] / 255.0
    mx, mn = rgb.max(2), rgb.min(2)
    s = np.where(mx > 1e-3, (mx - mn) / np.maximum(mx, 1e-3), 0)
    r, g, b = rgb[..., 0], rgb[..., 1], rgb[..., 2]
    hue = np.zeros_like(mx)
    d = np.maximum(mx - mn, 1e-4)
    hue = np.where(mx == r, ((g - b) / d) % 6, np.where(mx == g, (b - r) / d + 2, (r - g) / d + 4)) * 60.0
    dh = np.abs((hue - fol_hue + 180) % 360 - 180)
    accent = (((dh > 55) & (s > .22)) | ((s < .28) & (mx > .72))) & (a[..., 3] > 200)
    return accent.astype(np.float32)


# ----------------------------------------------------------------------------------------- masques
def ellipse_poly(cx, cy, rx, ry, ang, n=18):
    pts = []
    ca, sa = math.cos(ang), math.sin(ang)
    for i in range(n):
        t = math.tau * i / n
        # feuille : pointue vers +x
        x = rx * math.cos(t) * (1.0 + .35 * math.cos(t))
        y = ry * math.sin(t) * (1.0 - .25 * math.cos(t))
        pts.append((cx + x * ca - y * sa, cy + x * sa + y * ca))
    return pts


def mask_canvas():
    S = 4
    im = Image.new('L', (CELL * S, CELL * S), 0)
    return im, ImageDraw.Draw(im), S


def draw_leaf(d, S, cx, cy, ln, wid, ang):
    d.polygon([(x * S, y * S) for x, y in ellipse_poly(cx, cy, ln * .5, wid * .5, ang)], fill=255)


def envelope_w(t, kind):
    if kind == 'scale':
        return .20 + .26 * math.sin(math.pi * min(1.0, t * .95 + .03)) ** .6
    return .12 + .34 * math.sin(math.pi * min(1.0, t * .88 + .10)) ** .7


def cell_mask(kind, leaf, rng):
    """Alpha d'une gerbe ; l'ancre est en bas (y = CELL), le bout en haut (y = 0)."""
    im, d, S = mask_canvas()
    leaf_px = max(10.0, leaf * CELL)
    if kind in ('broadleaf', 'scale'):
        # rameau central un peu courbe
        n = int({'broadleaf': 70, 'scale': 150}[kind] * (.26 / max(leaf, .06)) ** .6)
        n = min(max(n, 40), 260)
        bend = rng.uniform(-.10, .10)
        for i in range(n):
            t = rng.random() ** .85
            ew = envelope_w(t, kind)
            ox = bend * t * t + rng.uniform(-1, 1) * ew
            cx = (.5 + ox) * CELL
            cy = (1.0 - .02 - t * .96) * CELL
            ang = math.atan2(-1.0, ox * 2.0) + rng.uniform(-1.1, 1.1) if kind == 'broadleaf' else rng.uniform(0, math.tau)
            ln = leaf_px * rng.uniform(.8, 1.25)
            draw_leaf(d, S, cx, cy, ln, ln * (.46 if kind == 'broadleaf' else .75), ang)
        draw_leaf(d, S, (.5 + bend) * CELL, .06 * CELL, leaf_px * 1.1, leaf_px * .55, -math.pi / 2)
    elif kind == 'conifer':
        # rameau + touffes d'aiguilles en eventail
        x = .5 * CELL
        for i in range(70):
            t = (i + .5) / 70
            px = (.5 + .02 * math.sin(t * 7)) * CELL
            py = (1.0 - t * .97) * CELL
            side = 1 if i % 2 else -1
            for f in range(5):
                a0 = -math.pi / 2 + side * (math.radians(55) + rng.uniform(-.35, .35))
                r = leaf_px * (1.6 + .8 * math.sin(math.pi * t)) * rng.uniform(.7, 1.1)
                x1, y1 = px + r * math.cos(a0), py + r * math.sin(a0)
                d.line([(px * S, py * S), (x1 * S, y1 * S)], fill=255, width=int(S * 3.2))
                # aiguilles courtes autour
                for g in range(5):
                    tt = rng.uniform(.3, 1.0)
                    qx, qy = px + (x1 - px) * tt, py + (y1 - py) * tt
                    a1 = a0 + rng.uniform(-1.0, 1.0)
                    r2 = leaf_px * rng.uniform(.6, 1.0)
                    d.line([(qx * S, qy * S), ((qx + r2 * math.cos(a1)) * S, (qy + r2 * math.sin(a1)) * S)],
                           fill=255, width=int(S * 2.4))
        d.line([(.5 * CELL * S, CELL * S), (.5 * CELL * S, .04 * CELL * S)], fill=255, width=S * 4)
    elif kind == 'strands':
        n = 6
        for k in range(n):
            x0 = (.12 + .76 * (k + rng.uniform(-.2, .2)) / (n - 1)) * CELL
            ph = rng.uniform(0, math.tau)
            pts = []
            for j in range(41):
                t = j / 40
                pts.append(((x0 + 9 * math.sin(ph + t * 5.0)) * S, (1.0 - t) * CELL * S))
            d.line(pts, fill=255, width=int(S * 5.0))
            for j in range(4, 40, 2):
                t = j / 40
                px, py = x0 + 9 * math.sin(ph + t * 5.0), (1.0 - t) * CELL
                for side in (-1, 1):
                    ang = -math.pi / 2 + side * math.radians(rng.uniform(55, 80))
                    draw_leaf(d, S, px + side * 4, py, leaf_px * rng.uniform(1.0, 1.5), leaf_px * .30, ang)
    elif kind == 'frond':
        bend = rng.uniform(-.06, .06)
        rach = [((.5 + bend * (j / 40) ** 2) * CELL, (1.0 - j / 40 * .97) * CELL) for j in range(41)]
        d.line([(x * S, y * S) for x, y in rach], fill=255, width=S * 4)
        for j in range(3, 40):
            t = j / 40
            px, py = rach[j]
            ln = CELL * .46 * math.sin(math.pi * min(1.0, t * .9 + .08)) ** .8 * (1.0 - .35 * t)
            for side in (-1, 1):
                ang = -math.pi / 2 + side * math.radians(64 - 8 * t)
                draw_leaf(d, S, px + .5 * ln * math.cos(ang), py + .5 * ln * math.sin(ang), ln, max(5.0, ln * .20), ang)
    elif kind == 'blades':
        for k in range(15):
            x0 = (.10 + .80 * (k + rng.uniform(-.3, .3)) / 14) * CELL
            hgt = rng.uniform(.55, .98) * CELL
            lean = rng.uniform(-.28, .28) + (x0 / CELL - .5) * .8
            pts_l, pts_r = [], []
            for j in range(0, 21):
                t = j / 20
                px = x0 + lean * hgt * t * t
                py = CELL - hgt * t
                wd = (9.0 * (1 - t) ** .8 + .6) * .5
                pts_l.append(((px - wd) * S, py * S))
                pts_r.append(((px + wd) * S, py * S))
            d.polygon(pts_l + pts_r[::-1], fill=255)
    return im.resize((CELL, CELL), Image.LANCZOS)


def draw_flowers(rgba, a_rgb, mask, accent_px, rng):
    """Prairie / fleurs : disques de couleur tires des pixels d'accent du sprite, poses au bout des tiges."""
    if accent_px is None or len(accent_px) == 0:
        return
    d = ImageDraw.Draw(rgba)
    m = np.asarray(mask) > 128
    ys, xs = np.where(m[: int(CELL * .6)])
    if ys.size == 0:
        return
    for _ in range(26):
        k = int(rng.integers(0, ys.size))
        c = accent_px[int(rng.integers(0, len(accent_px)))]
        r = rng.uniform(4.5, 8.5)
        x, y = float(xs[k]), float(ys[k])
        d.ellipse([x - r, y - r, x + r, y + r], fill=(int(c[0]), int(c[1]), int(c[2]), 255))


def fill_transparent(rgba):
    """Couleur des pixels transparents d'une pastille : propagee depuis les opaques (pas le gris du fond)."""
    rgb = rgba[..., :3].astype(np.float32)
    w = (rgba[..., 3] > 200).astype(np.float32)
    if w.sum() < 1:
        return rgb
    out = rgb * w[..., None]
    wt = w.copy()
    for _ in range(40):
        if wt.min() > 0.5:
            break
        pad = np.pad(out, ((1, 1), (1, 1), (0, 0)), mode='edge')
        padw = np.pad(wt, 1, mode='edge')
        acc = sum(pad[1 + dy:1 + dy + out.shape[0], 1 + dx:1 + dx + out.shape[1]] for dy in (-1, 0, 1) for dx in (-1, 0, 1))
        accw = sum(padw[1 + dy:1 + dy + out.shape[0], 1 + dx:1 + dx + out.shape[1]] for dy in (-1, 0, 1) for dx in (-1, 0, 1))
        new = acc / np.maximum(accw[..., None], 1e-6)
        fill = (wt < .5) & (accw > 0)
        out = np.where(fill[..., None], new, out)
        wt = np.where(fill, 1.0, wt)
    return out


def build_cell(name, which, a, rng, np_rng, accent):
    cfg = TABLE[name]
    h, w = a.shape[:2]
    x, y, ps = pick_patch(a, name, which, accent, np_rng)
    patch = fill_transparent(a[y:y + ps, x:x + ps])
    patch = np.asarray(Image.fromarray(patch.astype('uint8')).resize((CELL, CELL), Image.LANCZOS)).astype(np.float32)
    mask = cell_mask(cfg['kind'], cfg['leaf'] * (1.0 if which == 'A' else .9), rng)
    return patch, mask, (x, y, ps)


# -------------------------------------------------------------------------------------------- atlas
def build_all():
    os.makedirs(DOC, exist_ok=True)
    atlas = np.zeros((ROWS * CELL, COLS * CELL, 4), np.float32)      # sRGB 0..255 + alpha
    specs, previews = [], []
    cell_i = 0
    sprites = {}
    for n in ORDER:
        sprites[n] = Image.open(os.path.join(PNG_DIR, n + '.png')).convert('RGBA')
    for name in ORDER:
        cfg = TABLE[name]
        img = sprites[name]
        a = np.asarray(img).astype(np.float32)
        info = analyse(name, img)
        seed = sum(ord(c) for c in name) * 7919
        rng = random.Random(seed)
        np_rng = np.random.default_rng(seed)
        lin_all = s2l(a[..., :3])
        solid = a[..., 3] > 230
        # teinte : mediane lineaire des pixels du feuillage (au-dessus du fut), accent compris
        yy = np.arange(a.shape[0])[:, None]
        foliage_sel = solid & (yy < info['crown_y'] + 1) if cfg['mode'] in ('crown', 'column', 'weeping') else solid
        fol = np.median(lin_all[foliage_sel], axis=0) if foliage_sel.any() else np.array([.05, .1, .04])
        fol_hue = None
        acc = None
        if cfg['accent']:
            mx, mn = fol.max(), fol.min()
            r, g, b = fol
            d = max(mx - mn, 1e-4)
            hue = (((g - b) / d) % 6 if mx == r else ((b - r) / d + 2 if mx == g else (r - g) / d + 4)) * 60.0
            acc = accent_map(a, hue)
        cells = []
        cell_info = []
        for which in ('A', 'B'):
            patch, mask, src_box = build_cell(name, which, a, rng, np_rng, acc if which == 'B' else None)
            rgba = Image.fromarray(patch.astype('uint8'), 'RGB').convert('RGBA')
            if cfg['accent'] == 'flowers' and which == 'B':
                ap = None
                if acc is not None:
                    ay, ax = np.where(acc > .5)
                    if ay.size:
                        ap = a[ay, ax, :3]
                m_np = np.asarray(mask)
                draw_flowers(rgba, a, mask, ap, np_rng)
                # les tiges de la case B sont celles de A : on redessine un masque 'blades' complet + fleurs
            lin = s2l(np.asarray(rgba).astype(np.float32)[..., :3])
            alpha = np.asarray(mask).astype(np.float32) / 255.0
            sel = alpha > CLIP
            luma = lin[..., 0] * .2126 + lin[..., 1] * .7152 + lin[..., 2] * .0722
            mean_l = float(luma[sel].mean()) if sel.any() else .1
            ref = max(mean_l, float(np.percentile(luma[sel], 95)) / 2.0) if sel.any() else mean_l
            stored = np.clip(lin * (DETAIL_MEAN / ref), 0, 1)
            # couleur propre sous le masque : dilater les pixels opaques (mips sans halo sombre)
            rgb8 = l2s(stored)
            ci, cj = cell_i % COLS, cell_i // COLS
            atlas[cj * CELL:(cj + 1) * CELL, ci * CELL:(ci + 1) * CELL, :3] = rgb8
            atlas[cj * CELL:(cj + 1) * CELL, ci * CELL:(ci + 1) * CELL, 3] = alpha * 255.0
            cells.append(cell_i)
            cell_info.append({'cell': cell_i, 'which': which, 'luma_ref': round(ref, 5), 'mean_luma': round(mean_l, 5),
                              'src': list(src_box), 'coverage': round(float(sel.mean()), 3)})
            cell_i += 1
        # ecorce
        bark = np.array([.07, .055, .042])
        if info.get('bark_px') is not None and len(info['bark_px']):
            bark = np.median(s2l(info['bark_px']), axis=0)
            top = .30 if name == 'bouleau' else .10
            luma = float(bark @ np.array([.2126, .7152, .0722]))
            bark = luma + .55 * (bark - luma)         # l'ecorce du sprite est dorée par la lumiere : on la ramene vers le gris-brun
            bark = np.maximum(bark, .01) * min(1.0, top / max(float(bark.max()), 1e-6))
        bands = info['bands']
        crown_w_u = max(2 * b[4] for b in bands)
        card_len = round(max(3.0, cfg['card'] * crown_w_u), 3)
        if cfg['mode'] in ('fronds', 'tufts'):
            card_len = round(cfg['card'] * 100.0 * (1.0 if cfg['mode'] == 'fronds' else 1.0), 3)
        if cfg['mode'] == 'column':
            card_len = round(max(4.0, cfg['card'] * crown_w_u * 1.3), 3)
        spec = {'name': name, 'kind': cfg['kind'], 'mode': cfg['mode'], 'height_m': cfg['height_m'],
                'trunk': info['trunk'], 'bands': bands, 'card_len': card_len,
                'card_aspect': {'frond': .60, 'blades': .55, 'strands': .30}.get(cfg['kind'], 1.0),
                'density': cfg['density'], 'cells': cells, 'cell_info': cell_info, 'alt_share': .30,
                'gain': cfg.get('gain', ALBEDO_GAIN),
                'foliage': [round(float(c * cfg.get('gain', ALBEDO_GAIN)), 5) for c in fol], 'bark': [round(float(c), 5) for c in bark],
                'limb_r': cfg.get('limb_r', .5), 'stems': cfg.get('stems', 6), 'count': cfg.get('count', 13),
                'sprite_px': [info['w'], info['h']], 'pivot_px': round(info['pivot'], 1)}
        specs.append(spec)
    # case pleine (masses de LOD2), derniere case
    last = COLS * ROWS - 1
    ci, cj = last % COLS, last // COLS
    atlas[cj * CELL:(cj + 1) * CELL, ci * CELL:(ci + 1) * CELL] = [l2s(np.array([DETAIL_MEAN]))[0]] * 3 + [255.0]
    out = Image.fromarray(np.clip(atlas, 0, 255).astype('uint8'), 'RGBA')
    # couleur des pixels transparents : moyenne de la case (pas de halo noir dans les mips)
    arr = np.asarray(out).copy()
    for cj in range(ROWS):
        for ci in range(COLS):
            blk = arr[cj * CELL:(cj + 1) * CELL, ci * CELL:(ci + 1) * CELL]
            op = blk[..., 3] > 107
            if op.any():
                mean_rgb = blk[..., :3][op].mean(0)
                blk[..., :3][~op] = mean_rgb
    out = Image.fromarray(arr, 'RGBA')
    out.save(os.path.join(SRC, 'gpt-flora-atlas.png'))
    json.dump({'atlas': {'cols': COLS, 'rows': ROWS, 'cell': CELL, 'solid_cell': last, 'detail_mean': DETAIL_MEAN,
                         'clip': CLIP}, 'species': specs}, open(os.path.join(SRC, 'gpt-flora.json'), 'w'), indent=1)
    sheet = Image.new('RGB', out.size, (200, 200, 200))
    dd = ImageDraw.Draw(sheet)
    for y in range(0, out.size[1], 32):
        for x in range(0, out.size[0], 32):
            if (x // 32 + y // 32) % 2:
                dd.rectangle([x, y, x + 31, y + 31], fill=(160, 160, 160))
    sheet.paste(out, (0, 0), out)
    sheet.save(os.path.join(DOC, 'atlas-controle.png'))
    log('atlas %dx%d, %d cases, %d especes' % (out.size[0], out.size[1], cell_i, len(specs)))
    return specs, out


if __name__ == '__main__':
    specs, atlas = build_all()
    for s in specs:
        log('%-18s mode=%-8s bandes=%2d fut=%2d card_len=%6.2f foliage=%s' % (
            s['name'], s['mode'], len(s['bands']), len(s['trunk']), s['card_len'], s['foliage']))
