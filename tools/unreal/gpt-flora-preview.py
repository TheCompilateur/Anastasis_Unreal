"""GPT_FLORA_002 -- controle logiciel des treize sujets modelises : sprite | face | trois-quarts | profil.

**Python systeme, hors Unreal** (Pillow + numpy). Lit `gpt-flora.json` (ecrit par `gpt-flora-fit.py`), construit chaque
sujet avec `gpt_flora_model.py` (le code meme que l'editeur, la meme graine) et rasterise les triangles avec un z-buffer
et un eclairage de Lambert simple sur la normale de chaque face. Ce n'est pas Unreal : pas de Lumen, pas de vent, pas de
transmission des feuilles. Il juge la forme : la silhouette de face colle-t-elle au sprite (IoU), le volume tient-il
de cote, les branches se lisent-elles, combien de triangles par LOD.

Sorties : docs/visual/sprites-vegetation-gpt-001/preview-<espece>.png, `preview-planche-N.png`, `iou.json`, `triangles.json`.
Lancer : python tools/unreal/gpt-flora-preview.py [espece ...]
"""
import json
import math
import os
import sys

import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import gpt_flora_model as model  # noqa: E402

ROOT = os.path.abspath(os.path.join(HERE, '..', '..'))
SRC = os.path.join(ROOT, 'SourceArt', 'Vegetation', 'gpt')
DOC = os.path.join(ROOT, 'docs', 'visual', 'sprites-vegetation-gpt-001')
LIGHT = np.array([-0.45, -0.55, 0.70], np.float32)
LIGHT /= np.linalg.norm(LIGHT)
EXPOSURE = 2.6


def l2s(c):
    c = np.clip(c, 0, 1)
    return np.where(c <= 0.0031308, c * 12.92, 1.055 * np.power(c, 1 / 2.4) - 0.055) * 255.0


def render(batches, yaw, scale, size, pivot_x):
    """z-buffer + Lambert. Vue depuis -Y tournee de `yaw` degres autour de Z (deux faces)."""
    W, H = size
    rgb = np.zeros((H, W, 3), np.float32)
    mask = np.zeros((H, W), bool)
    zbuf = np.full((H, W), 1e9, np.float32)
    ca, sa = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
    viewdir = np.array([-sa, ca, 0.0], np.float32)         # l'axe de profondeur, dans le monde
    for b in batches:
        if not b.t:
            continue
        P = np.array(b.v, np.float32)
        C = np.array(b.c, np.float32)
        x = P[:, 0] * ca - P[:, 1] * sa
        y = P[:, 0] * sa + P[:, 1] * ca
        sx = pivot_x + x * scale
        sy = (50.0 - P[:, 2]) * scale
        for (ia, ib, ic) in b.t:
            pa, pb, pc = P[ia], P[ib], P[ic]
            n = np.cross(pb - pa, pc - pa)
            nl = float(np.linalg.norm(n))
            if nl < 1e-9:
                continue
            n = n / nl
            if float(np.dot(n, viewdir)) > 0:
                n = -n
            shade = 0.30 + 0.70 * max(0.0, float(np.dot(n, LIGHT)))
            col = (C[ia] + C[ib] + C[ic]) / 3.0 * shade * EXPOSURE
            xs = (sx[ia], sx[ib], sx[ic])
            ys = (sy[ia], sy[ib], sy[ic])
            x0, x1 = max(int(math.floor(min(xs))), 0), min(int(math.ceil(max(xs))) + 1, W)
            y0, y1 = max(int(math.floor(min(ys))), 0), min(int(math.ceil(max(ys))) + 1, H)
            if x1 <= x0 or y1 <= y0:
                continue
            den = (ys[1] - ys[2]) * (xs[0] - xs[2]) + (xs[2] - xs[1]) * (ys[0] - ys[2])
            if abs(den) < 1e-9:
                continue
            gx, gy = np.meshgrid(np.arange(x0, x1) + .5, np.arange(y0, y1) + .5)
            l1 = ((ys[1] - ys[2]) * (gx - xs[2]) + (xs[2] - xs[1]) * (gy - ys[2])) / den
            l2 = ((ys[2] - ys[0]) * (gx - xs[2]) + (xs[0] - xs[2]) * (gy - ys[2])) / den
            l3 = 1 - l1 - l2
            inside = (l1 >= -1e-3) & (l2 >= -1e-3) & (l3 >= -1e-3)
            if not inside.any():
                continue
            depth = l1 * y[ia] + l2 * y[ib] + l3 * y[ic]
            zb = zbuf[y0:y1, x0:x1]
            ok = inside & (depth < zb)
            if not ok.any():
                continue
            zb[ok] = depth[ok]
            rgb[y0:y1, x0:x1][ok] = col
            mask[y0:y1, x0:x1][ok] = True
    return rgb, mask


def tile_image(rgb, mask, bg=120):
    im = np.full(rgb.shape, bg, np.float32)
    im[mask] = l2s(rgb[mask])
    return Image.fromarray(im.astype('uint8'), 'RGB')


def main():
    os.makedirs(DOC, exist_ok=True)
    with open(os.path.join(SRC, 'gpt-flora.json'), encoding='utf-8') as fh:
        meta = json.load(fh)
    only = sys.argv[1:]
    tiles, ious, tris = [], {}, {}
    for spec in meta['species']:
        if only and spec['name'] not in only:
            continue
        spr = Image.open(os.path.join(SRC, 'PNG', spec['name'] + '.png')).convert('RGBA')
        sh = spr.height
        scale = sh / 100.0
        size = (int(spr.width * 1.4), sh)
        pivot = size[0] * .5
        lods = model.build_all(spec)
        tris[spec['name']] = [w.count() + l.count() for w, l in lods]
        wood, leaf = lods[0]
        views = [render([wood, leaf], yaw, scale, size, pivot) for yaw in (0.0, 45.0, 90.0)]
        sa = np.zeros((sh, size[0]), bool)
        ox = int(size[0] * .5 - spec['pivot_px'])
        spa = np.asarray(spr)[..., 3] > 127
        sa[:, ox:ox + spa.shape[1]] = spa
        ra = views[0][1]
        ious[spec['name']] = round(float((sa & ra).sum() / max((sa | ra).sum(), 1)), 3)
        sp_bg = Image.new('RGB', (size[0], sh), (120, 120, 120))
        sp_bg.paste(spr, (ox, 0), spr)
        row = Image.new('RGB', (size[0] * 4, sh))
        row.paste(sp_bg, (0, 0))
        for i, (rgb, mask) in enumerate(views):
            row.paste(tile_image(rgb, mask), (size[0] * (i + 1), 0))
        row.save(os.path.join(DOC, 'preview-%s.png' % spec['name']))
        tiles.append((spec['name'], row))
        print('%-18s IoU face=%.3f  triangles LOD0/1/2 = %s' % (spec['name'], ious[spec['name']], tris[spec['name']]))
    json.dump(ious, open(os.path.join(DOC, 'iou.json'), 'w'), indent=1)
    json.dump(tris, open(os.path.join(DOC, 'triangles.json'), 'w'), indent=1)
    if tiles and not only:
        half = [(n, t.resize((t.width // 2, t.height // 2), Image.LANCZOS)) for n, t in tiles]
        cw, ch = max(t.width for n, t in half), max(t.height for n, t in half)
        for part, chunk in enumerate((half[:7], half[7:])):
            sheet = Image.new('RGB', (cw, ch * len(chunk)), (90, 90, 90))
            for i, (n, t) in enumerate(chunk):
                sheet.paste(t, (0, i * ch))
            sheet.save(os.path.join(DOC, 'preview-planche-%d.png' % (part + 1)))


if __name__ == '__main__':
    main()
