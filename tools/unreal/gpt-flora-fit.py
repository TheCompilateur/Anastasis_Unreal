"""GPT_FLORA_002 -- mesure des sprites : silhouette en tranches, fut, couleurs. Le modele est ailleurs.

**Python systeme, hors Unreal** (Pillow + numpy). Entree : les treize PNG detoures de `SourceArt/Vegetation/gpt/PNG/`
(planche generee par GPT, decoupee par `gpt-flora-cut.py`). Sortie : `SourceArt/Vegetation/gpt/gpt-flora.json`, un spec par
espece, que lisent `gpt_flora_model.py` (qui BATIT chaque sujet) puis `create-gpt-flora.py` (qui l'ecrit dans Unreal).

CE QUE CE SCRIPT MESURE, ET RIEN D'AUTRE. Le sprite est une vue de face :
  * `bands`  : a chaque hauteur, jusqu'ou la couronne s'etend a gauche et a droite du fut (centre, demi-largeur) et la part
               de la tranche que le dessin remplit : le volume dans lequel le modele fait pousser ses branches ;
  * `trunk`  : l'axe et la largeur du fut sous la couronne ;
  * `foliage`, `bark` : la teinte mediane du feuillage et de l'ecorce, en lineaire (le dessin est un pixel eclaire :
               l'albedo reel est plus bas, d'ou un gain) ;
  * `pivot_px`, `sprite_px` : pour comparer le modele au sprite (IoU, `gpt-flora-preview.py`).
Aucune image n'est copiee ni collee dans le jeu : la planche ne donne que la forme, les proportions et les couleurs.

Lancer : python tools/unreal/gpt-flora-fit.py   (deterministe)
"""
import json
import os
import sys

import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, '..', '..'))
SRC = os.path.join(ROOT, 'SourceArt', 'Vegetation', 'gpt')
PNG_DIR = os.path.join(SRC, 'PNG')
ALBEDO_GAIN = 0.75          # le sprite est une image eclairee : l'albedo est plus bas que le pixel

# nom : mode d'analyse, hauteur adulte (m), remontee du bas de la couronne (fraction de la hauteur), gain de teinte.
TABLE = {
    'marronnier_fleuri': dict(mode='crown', height_m=14.0, crown_low=.30),
    'bouleau': dict(mode='crown', height_m=15.0, crown_low=.30),
    'chene': dict(mode='crown', height_m=17.0, crown_low=.28, bark_gain=.55),
    'pin_sombre': dict(mode='crown', height_m=20.0, crown_low=.20),
    'pin_sylvestre': dict(mode='crown', height_m=24.0),
    'cypres': dict(mode='column', height_m=16.0),
    'genevrier': dict(mode='shrub', height_m=4.5),
    'noisetier': dict(mode='shrub', height_m=5.0),
    'saule_pleureur': dict(mode='weeping', height_m=14.0, crown_low=.22, gain=.60),
    'arbuste_baies': dict(mode='shrub', height_m=2.5),
    'rhododendron': dict(mode='shrub', height_m=3.0),
    'fougere': dict(mode='ground', height_m=1.0),
    'prairie_fleurie': dict(mode='ground', height_m=1.1),
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
    if cfg['mode'] == 'ground':
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
            reach = .92 if name in ('pin_sombre', 'pin_sylvestre') else .62       # un pin garde sa fleche jusqu'au sommet
            z_top = low_b + (top_b - low_b) * reach
            near = min(bands, key=lambda b: abs(b[0] - z_top))
            last = trunk_u[-1]
            r_top = max(last[3] * .45, .4)
            trunk_u.append([round(near[2] * .55, 3), 0.0, round(z_top, 3), round(r_top, 3)])
            if name in ('pin_sombre', 'pin_sylvestre'):
                trunk_u[-1][3] = .3
    # au plus 6 points : le fut est une polyligne de quelques troncons, pas un releve pixel par pixel
    if len(trunk_u) > 6:
        keep = sorted(set(int(round(i)) for i in np.linspace(0, len(trunk_u) - 1, 6)))
        trunk_u = [trunk_u[i] for i in keep]
    if cfg['mode'] == 'column' and trunk_u:
        trunk_u = trunk_u[:2]
        trunk_u[-1][2] = min(trunk_u[-1][2], -30.0)
    cap = {'chene': 0.10, 'saule_pleureur': 0.045}.get(name, 0.07) * crown_w * unit
    for t in trunk_u:
        t[3] = round(min(t[3], cap), 3)
    info['bands'] = bands
    info['trunk'] = trunk_u
    return info



def build_all():
    specs = []
    for name in ORDER:
        cfg = TABLE[name]
        img = Image.open(os.path.join(PNG_DIR, name + '.png')).convert('RGBA')
        a = np.asarray(img).astype(np.float32)
        info = analyse(name, img)
        lin_all = s2l(a[..., :3])
        solid = a[..., 3] > 230
        yy = np.arange(a.shape[0])[:, None]
        foliage_sel = solid & (yy < info['crown_y'] + 1) if cfg['mode'] in ('crown', 'column', 'weeping') else solid
        fol = np.median(lin_all[foliage_sel], axis=0) if foliage_sel.any() else np.array([.05, .1, .04])
        bark = np.array([.07, .055, .042])
        if info.get('bark_px') is not None and len(info['bark_px']):
            bark = np.median(s2l(info['bark_px']), axis=0)
            top = .30 if name == 'bouleau' else .10
            luma = float(bark @ np.array([.2126, .7152, .0722]))
            bark = luma + .55 * (bark - luma)         # l'ecorce du sprite est doree par la lumiere : retour vers le gris-brun
            bark = np.maximum(bark, .01) * min(1.0, top / max(float(bark.max()), 1e-6))
        gain = cfg.get('gain', ALBEDO_GAIN)
        specs.append({'name': name, 'mode': cfg['mode'], 'height_m': cfg['height_m'], 'trunk': info['trunk'],
                      'bands': info['bands'], 'gain': gain,
                      'foliage': [round(float(c * gain), 5) for c in fol],
                      'bark': [round(float(c) * cfg.get('bark_gain', 1.0), 5) for c in bark],
                      'sprite_px': [info['w'], info['h']], 'pivot_px': round(info['pivot'], 1)})
    with open(os.path.join(SRC, 'gpt-flora.json'), 'w', encoding='utf-8') as fh:
        json.dump({'species': specs}, fh, indent=1)
    return specs


if __name__ == '__main__':
    for s in build_all():
        log('%-18s mode=%-8s bandes=%2d fut=%2d foliage=%s bark=%s' % (
            s['name'], s['mode'], len(s['bands']), len(s['trunk']), s['foliage'], s['bark']))
