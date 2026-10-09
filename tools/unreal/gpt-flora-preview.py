"""GPT_FLORA_001 -- controle logiciel des treize especes : rendu face / profil et IoU de silhouette.

**Python systeme, hors Unreal** (Pillow + numpy). Lit `gpt-flora.json` et l'atlas ecrits par
`gpt-flora-fit.py`, construit bois et cartes avec `gpt_flora_geometry.py` (le code meme que l'editeur), et les
rasterise en logiciel avec un z-buffer et le test alpha du materiau Masked (seuil 0,42). Il n'est pas Unreal :
pas de lumiere, pas de vent, pas de MSM_TwoSidedFoliage. Ce qu'il juge : la silhouette de face rend-elle celle
du sprite (IoU), le volume tient-il de profil, la couronne est-elle creuse ou pleine.

Sorties : docs/visual/sprites-vegetation-gpt-001/preview-<espece>.png (sprite | face | profil) et
`preview-planche.png`, plus `iou.json`.
"""
import json
import math
import os
import random
import sys

import numpy as np
from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import gpt_flora_geometry as geo  # noqa: E402

ROOT = os.path.abspath(os.path.join(HERE, '..', '..'))
SRC = os.path.join(ROOT, 'SourceArt', 'Vegetation', 'gpt')
DOC = os.path.join(ROOT, 'docs', 'visual', 'sprites-vegetation-gpt-001')


def s2l(c):
    c = np.asarray(c, np.float32) / 255.0
    return np.where(c <= 0.04045, c / 12.92, ((c + 0.055) / 1.055) ** 2.4)


def l2s(c):
    c = np.clip(c, 0, 1)
    return np.where(c <= 0.0031308, c * 12.92, 1.055 * np.power(c, 1 / 2.4) - 0.055) * 255.0


class Canvas(object):
    def __init__(self, w, h):
        self.w, self.h = w, h
        self.rgb = np.zeros((h, w, 3), np.float32)
        self.a = np.zeros((h, w), np.float32)
        self.z = np.full((h, w), 1e9, np.float32)

    def tri(self, p, d, uv, sampler, tint):
        xs = [q[0] for q in p]
        ys = [q[1] for q in p]
        x0, x1 = max(int(math.floor(min(xs))), 0), min(int(math.ceil(max(xs))) + 1, self.w)
        y0, y1 = max(int(math.floor(min(ys))), 0), min(int(math.ceil(max(ys))) + 1, self.h)
        if x1 <= x0 or y1 <= y0:
            return
        gx, gy = np.meshgrid(np.arange(x0, x1) + .5, np.arange(y0, y1) + .5)
        (ax, ay), (bx, by), (cx, cy) = p
        den = (by - cy) * (ax - cx) + (cx - bx) * (ay - cy)
        if abs(den) < 1e-9:
            return
        l1 = ((by - cy) * (gx - cx) + (cx - bx) * (gy - cy)) / den
        l2 = ((cy - ay) * (gx - cx) + (ax - cx) * (gy - cy)) / den
        l3 = 1 - l1 - l2
        inside = (l1 >= -1e-4) & (l2 >= -1e-4) & (l3 >= -1e-4)
        if not inside.any():
            return
        depth = l1 * d[0] + l2 * d[1] + l3 * d[2]
        if sampler is None:
            rgb = np.broadcast_to(np.asarray(tint, np.float32), gx.shape + (3,))
            alpha = np.ones(gx.shape, np.float32)
        else:
            u = l1 * uv[0][0] + l2 * uv[1][0] + l3 * uv[2][0]
            v = l1 * uv[0][1] + l2 * uv[1][1] + l3 * uv[2][1]
            rgb, alpha = sampler(u, v)
            rgb = rgb * np.asarray(tint, np.float32)
        zb = self.z[y0:y1, x0:x1]
        ok = inside & (alpha >= .42) & (depth < zb)
        if not ok.any():
            return
        zb[ok] = depth[ok]
        self.rgb[y0:y1, x0:x1][ok] = rgb[ok]
        self.a[y0:y1, x0:x1][ok] = 1.0


def make_sampler(atlas_lin, atlas_a, cell, cols, rows, size):
    cj, ci = divmod(cell, cols)

    def sample(u, v):
        px = np.clip(((ci + u) / cols * (atlas_a.shape[1])).astype(int), 0, atlas_a.shape[1] - 1)
        py = np.clip(((cj + v) / rows * (atlas_a.shape[0])).astype(int), 0, atlas_a.shape[0] - 1)
        return atlas_lin[py, px], atlas_a[py, px]
    return sample


def render(spec, atlas_meta, atlas_lin, atlas_a, view, scale, size):
    """view 'front' : x -> ecran x, profondeur y ; 'side' : y -> ecran x, profondeur -x."""
    W, H = size
    cv = Canvas(W, H)
    rng = random.Random(spec['name'] + ':wood')
    wood = geo.build_wood(spec, rng)
    cards = geo.build_cards(spec, random.Random(spec['name'] + ':cards'))
    cols, rows = atlas_meta['cols'], atlas_meta['rows']
    cinfo = {c['cell']: c for c in spec['cell_info']}
    pivot = W * .5

    def proj(p):
        if view == 'front':
            return (pivot + p[0] * scale, (50.0 - p[2]) * scale), p[1]
        return (pivot + p[1] * scale, (50.0 - p[2]) * scale), -p[0]

    bark = np.asarray(spec['bark'], np.float32)
    for (s, e, r0, r1, role) in wood:
        (sx, sy), sd = proj(s)
        (ex, ey), ed = proj(e)
        dx, dy = ex - sx, ey - sy
        ln = math.hypot(dx, dy) or 1.0
        nx, ny = -dy / ln, dx / ln
        q = [(sx + nx * r0 * scale, sy + ny * r0 * scale), (sx - nx * r0 * scale, sy - ny * r0 * scale),
             (ex + nx * r1 * scale, ey + ny * r1 * scale), (ex - nx * r1 * scale, ey - ny * r1 * scale)]
        cv.tri([q[0], q[1], q[2]], [sd, sd, ed], None, None, bark)
        cv.tri([q[1], q[3], q[2]], [sd, ed, ed], None, None, bark)
    fol = np.asarray(spec['foliage'], np.float32)
    for c in cards:
        info = cinfo[c['cell']]
        samp = make_sampler(atlas_lin, atlas_a, c['cell'], cols, rows, 256)
        verts = geo.card_vertices(c)
        tone = c['tone'] * (.50 + .52 * c['depth'])
        # vertex = luma_ref * ALBEDO_GAIN * ton ; texture/0.45 : le produit redonne le pixel du sprite * gain
        k = info['luma_ref'] * spec.get('gain', .75) * tone / .45
        tint = np.array([k, k, k], np.float32)
        pr = [proj(v[0]) for v in verts]
        pts = [x[0] for x in pr]
        dep = [x[1] for x in pr]
        uvs = [v[1] for v in verts]
        for row in range(geo.CARD_ROWS - 1):
            a, b, cc, d = 2 * row, 2 * row + 1, 2 * row + 2, 2 * row + 3
            cv.tri([pts[a], pts[cc], pts[b]], [dep[a], dep[cc], dep[b]], [uvs[a], uvs[cc], uvs[b]], samp, tint)
            cv.tri([pts[b], pts[cc], pts[d]], [dep[b], dep[cc], dep[d]], [uvs[b], uvs[cc], uvs[d]], samp, tint)
    return cv


def main():
    os.makedirs(DOC, exist_ok=True)
    meta = json.load(open(os.path.join(SRC, 'gpt-flora.json')))
    am = meta['atlas']
    atlas = np.asarray(Image.open(os.path.join(SRC, 'gpt-flora-atlas.png')).convert('RGBA')).astype(np.float32)
    atlas_lin = s2l(atlas[..., :3]) / am['detail_mean']       # = texture / 0.45, lineaire
    atlas_a = atlas[..., 3] / 255.0
    only = sys.argv[1:]
    tiles, ious = [], {}
    for spec in meta['species']:
        if only and spec['name'] not in only:
            continue
        spr = Image.open(os.path.join(SRC, 'PNG', spec['name'] + '.png')).convert('RGBA')
        sh = spr.height
        scale = sh / 100.0
        size = (int(spr.width * 1.4), sh)
        cv_f = render(spec, am, atlas_lin, atlas_a, 'front', scale, size)
        cv_s = render(spec, am, atlas_lin, atlas_a, 'side', scale, size)
        # IoU contre le sprite recentre sur le meme pivot
        sa = np.zeros((sh, size[0]), bool)
        ox = int(size[0] * .5 - spec['pivot_px'])
        spa = np.asarray(spr)[..., 3] > 127
        sa[:, ox:ox + spa.shape[1]] = spa
        ra = cv_f.a > .5
        inter = (sa & ra).sum()
        union = (sa | ra).sum()
        ious[spec['name']] = round(float(inter / max(union, 1)), 3)
        # tuile : sprite | face | profil, fond gris moyen, sRGB
        def to_img(cv):
            rgb = l2s(cv.rgb * 1.0)
            im = np.full((sh, size[0], 3), 120, np.float32)
            im[cv.a > .5] = rgb[cv.a > .5]
            return Image.fromarray(im.astype('uint8'), 'RGB')
        sp_bg = Image.new('RGB', (size[0], sh), (120, 120, 120))
        sp_bg.paste(spr, (ox, 0), spr)
        row = Image.new('RGB', (size[0] * 3, sh))
        row.paste(sp_bg, (0, 0))
        row.paste(to_img(cv_f), (size[0], 0))
        row.paste(to_img(cv_s), (size[0] * 2, 0))
        row.save(os.path.join(DOC, 'preview-%s.png' % spec['name']))
        tiles.append((spec['name'], row))
        print('%-18s IoU face = %.3f' % (spec['name'], ious[spec['name']]))
    json.dump(ious, open(os.path.join(DOC, 'iou.json'), 'w'), indent=1)
    if tiles and not only:
        half = [(n, t.resize((t.width // 2, t.height // 2), Image.LANCZOS)) for n, t in tiles]
        cw, ch = max(t.width for n, t in half), max(t.height for n, t in half)
        rows = (len(half) + 1) // 2
        sheet = Image.new('RGB', (cw * 2, ch * rows), (90, 90, 90))
        for i, (n, t) in enumerate(half):
            sheet.paste(t, ((i % 2) * cw, (i // 2) * ch))
        sheet.save(os.path.join(DOC, 'preview-planche.png'))
    return ious


if __name__ == '__main__':
    main()
