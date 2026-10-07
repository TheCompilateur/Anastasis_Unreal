"""Banc hors Unreal de l'architecture du village (architecture-crusade-001).

Python systeme (Pillow + numpy). Construit les maisonnees de create-village-architecture.py et les
rend en perspective (algorithme du peintre, soleil + ciel, ombre portee approchee au sol), avec des
silhouettes humaines de 170 cm. Sert a juger proportions et silhouettes en secondes ; le materiau,
la lumiere et le contact au terrain se jugent dans Unreal.

    python tools/unreal/architecture-preview.py [sortie] [--only NOM,...]

Sortie par defaut : Saved/ArchitecturePreview/ (planches PNG + contact.png).
"""
import importlib.util
import math
import os
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
spec = importlib.util.spec_from_file_location('arch', os.path.join(HERE, 'create-village-architecture.py'))
arch = importlib.util.module_from_spec(spec)
spec.loader.exec_module(arch)

SUN = np.array([-.45, .62, .64])
SUN /= np.linalg.norm(SUN)


def human(m, x, y, z=0.0, yaw=0.0, h=170.0, color=(.12, .09, .07, arch.FIBER)):
    """Silhouette humaine de reference (170 cm) : jambes, buste, bras, tete."""
    s = h / 170.0
    f = arch.yaw_vec(yaw)
    side = (-f[1], f[0], 0)
    for k in (-1, 1):
        base = arch.add((x, y, z), arch.mul(side, k * 10 * s))
        m.tube([base, arch.add(base, (0, 0, 84 * s))], 6.5 * s, color, 6)
        m.tube([arch.add(arch.add((x, y, z), arch.mul(side, k * 21 * s)), (0, 0, 142 * s)),
                arch.add(arch.add((x, y, z), arch.mul(side, k * 24 * s)), (0, 0, 84 * s))], 4.2 * s, color, 6)
    m.lathe([(13 * s, 80 * s), (17 * s, 92 * s), (16 * s, 120 * s), (21 * s, 138 * s), (19 * s, 146 * s), (7 * s, 150 * s)],
            color, 10, center=(x, y, z))
    m.lathe([(0, 150 * s), (6 * s, 151 * s), (10 * s, 158 * s), (10.5 * s, 166 * s), (8 * s, 172 * s), (0, 174 * s)],
            (.30, .20, .15, arch.FIBER), 10, center=(x, y, z))


def ground(m, half=2600.0, z=-1.0, step=200.0):
    k = -half
    while k < half:
        j = -half
        while j < half:
            m.poly([(k, j, z), (k + step, j, z), (k + step, j + step, z), (k, j + step, z)], (.15, .16, .085, arch.EARTH),
                   outward=(0, 0, 1))
            j += step
        k += step


def look_at(eye, target, fov=55.0, w=1600, h=1000):
    eye = np.array(eye, float)
    fwd = np.array(target, float) - eye
    fwd /= np.linalg.norm(fwd)
    right = np.cross(fwd, [0, 0, 1.0])
    right /= np.linalg.norm(right)
    up = np.cross(right, fwd)
    f = (w / 2) / math.tan(math.radians(fov) / 2)
    return dict(eye=eye, fwd=fwd, right=right, up=up, f=f, w=w, h=h)


def render(meshes, cam, clip=None, shadows=True, sky=(205, 214, 220)):
    V, T, C = [], [], []
    off = 0
    for m in meshes:
        if not m.t:
            continue
        V.append(np.array(m.v, float))
        T.append(np.array(m.t, int) + off)
        C.append(np.array(m.c, float))
        off += len(m.v)
    V = np.concatenate(V)
    T = np.concatenate(T)
    C = np.concatenate(C)
    a, b, c = V[T[:, 0]], V[T[:, 1]], V[T[:, 2]]
    n = np.cross(b - a, c - a)
    ln = np.linalg.norm(n, axis=1)
    ok = ln > 1e-6
    a, b, c, n, T = a[ok], b[ok], c[ok], n[ok] / ln[ok, None], T[ok]
    cen = (a + b + c) / 3
    if clip is not None:
        p, d = np.array(clip[0], float), np.array(clip[1], float)
        keep = (cen - p) @ d < 0
        a, b, c, n, T, cen = a[keep], b[keep], c[keep], n[keep], T[keep], cen[keep]
    eye = cam['eye']
    above = np.maximum(np.maximum(a[:, 2], b[:, 2]), c[:, 2]) > -3.0
    a, b, c, n, T, cen = a[above], b[above], c[above], n[above], T[above], cen[above]
    view = cen - eye
    facing = (view * n).sum(1) < 0
    a, b, c, n, T, cen = a[facing], b[facing], c[facing], n[facing], T[facing], cen[facing]
    col = C[T[:, 0], :3]
    lam = np.clip(n @ SUN, 0, 1)
    skyl = .55 + .45 * np.clip(n[:, 2], -1, 1) * .5 + .2
    # ombre approchee : un triangle est a l'ombre si un echantillon de la scene au-dessus de lui, vers le soleil, l'occulte
    shade = np.ones(len(cen))
    if shadows:
        allc = (V[T[:, 0]] + V[T[:, 1]] + V[T[:, 2]]) / 3
        grid = 40.0
        occ = {}
        for p in allc:
            if p[2] < 2:
                continue
            for k in range(0, 2):
                pass
        # projection des sommets le long du soleil sur une carte de hauteur 2D
        hs = {}
        proj = allc[:, :2] - allc[:, 2:3] * (SUN[:2] / SUN[2])
        keys = np.floor(proj / grid).astype(int)
        for (i, j), z in zip(map(tuple, keys), allc[:, 2]):
            if z > hs.get((i, j), -1e9):
                hs[(i, j)] = z
        pc = cen[:, :2] - cen[:, 2:3] * (SUN[:2] / SUN[2])
        kc = np.floor(pc / grid).astype(int)
        top = np.array([hs.get(tuple(k), -1e9) for k in kc])
        shade = np.where(top > cen[:, 2] + 30, .25, 1.0)
    light = col * (lam * shade * 3.0 + skyl * .62)[:, None] * .40
    dist = np.linalg.norm(view[facing], axis=1)
    fog = np.clip((dist - 2500) / 12000, 0, .5)[:, None]
    light = light * (1 - fog) + (np.array(sky) / 255.0) ** 2.2 * fog
    rgb = np.clip(light, 0, 1) ** (1 / 2.2)

    def proj(p):
        d = p - eye
        z = d @ cam['fwd']
        x = d @ cam['right']
        y = d @ cam['up']
        zz = np.maximum(z, 1.0)
        return np.stack([cam['w'] / 2 + cam['f'] * x / zz, cam['h'] / 2 - cam['f'] * y / zz], 1), z
    pa, za = proj(a)
    pb, zb = proj(b)
    pc_, zc = proj(c)
    W, H = cam['w'], cam['h']
    vis = (za > 5) & (zb > 5) & (zc > 5)
    img = np.zeros((H, W, 3), float)
    sky_rgb = np.array(sky) / 255.0
    for y in range(H):
        t = min(1.0, y / (H / 2))
        img[y] = sky_rgb * (.82 + .18 * t)
    zbuf = np.full((H, W), np.inf)
    xs = np.stack([pa[:, 0], pb[:, 0], pc_[:, 0]], 1)
    ys = np.stack([pa[:, 1], pb[:, 1], pc_[:, 1]], 1)
    x0 = np.clip(np.floor(xs.min(1)), 0, W - 1).astype(int)
    x1 = np.clip(np.ceil(xs.max(1)), 0, W - 1).astype(int)
    y0 = np.clip(np.floor(ys.min(1)), 0, H - 1).astype(int)
    y1 = np.clip(np.ceil(ys.max(1)), 0, H - 1).astype(int)
    onscreen = vis & (xs.max(1) >= 0) & (xs.min(1) < W) & (ys.max(1) >= 0) & (ys.min(1) < H)
    for i in np.nonzero(onscreen)[0]:
        bx0, bx1, by0, by1 = x0[i], x1[i], y0[i], y1[i]
        if bx1 < bx0 or by1 < by0:
            continue
        gx, gy = np.meshgrid(np.arange(bx0, bx1 + 1) + .5, np.arange(by0, by1 + 1) + .5)
        (ax, ay), (bx_, by_), (cx, cy) = pa[i], pb[i], pc_[i]
        den = (by_ - cy) * (ax - cx) + (cx - bx_) * (ay - cy)
        if abs(den) < 1e-9:
            continue
        w0 = ((by_ - cy) * (gx - cx) + (cx - bx_) * (gy - cy)) / den
        w1 = ((cy - ay) * (gx - cx) + (ax - cx) * (gy - cy)) / den
        w2 = 1 - w0 - w1
        inside = (w0 >= -1e-4) & (w1 >= -1e-4) & (w2 >= -1e-4)
        if not inside.any():
            continue
        iz = w0 / za[i] + w1 / zb[i] + w2 / zc[i]
        depth = 1.0 / np.maximum(iz, 1e-9)
        sub = zbuf[by0:by1 + 1, bx0:bx1 + 1]
        win = inside & (depth < sub)
        sub[win] = depth[win]
        img[by0:by1 + 1, bx0:bx1 + 1][win] = rgb[i]
    return Image.fromarray((np.clip(img, 0, 1) * 255).astype(np.uint8))


def label(img, text):
    dr = ImageDraw.Draw(img)
    try:
        font = ImageFont.truetype('arial.ttf', 22)
    except OSError:
        font = ImageFont.load_default()
    dr.rectangle([0, img.height - 36, img.width, img.height], fill=(20, 20, 20))
    dr.text((12, img.height - 31), text, fill=(235, 230, 220), font=font)
    return img


def views_for(name, body):
    lo, hi = body.bounds()
    cx, cy = (lo[0] + hi[0]) / 2, (lo[1] + hi[1]) / 2
    entry = body.meta.get('entry', [0, hi[1] + 200, 0])
    doors = [d for d in body.meta['doors'] if d['name'] != 'gate']
    d0 = doors[0] if doors else dict(at=entry, yaw=90, floor=0)
    out = []
    span = max(hi[0] - lo[0], hi[1] - lo[1])
    out.append(('trois_quarts_170cm', look_at((cx + span * .55, hi[1] + span * .85, 165), (cx, cy, hi[2] * .38), fov=60)))
    out.append(('oblique_haute', look_at((cx + span * .9, hi[1] + span * 1.1, span * .75), (cx, cy, 150), fov=50)))
    out.append(('arriere', look_at((cx - span * .7, lo[1] - span * .8, 165), (cx, cy, hi[2] * .38), fov=60)))
    dx, dy = math.cos(math.radians(d0['yaw'])), math.sin(math.radians(d0['yaw']))
    at = d0['at']
    out.append(('porte', look_at((at[0] + dx * 520 + dy * 220, at[1] + dy * 520 - dx * 220, 165 + at[2]),
                                 (at[0], at[1], at[2] + 110), fov=55)))
    return out


def humans_for(body):
    hm = arch.Mesh()
    entry = body.meta.get('entry')
    if entry:
        human(hm, entry[0] + 60, entry[1], 0, -90)
    for d in body.meta['doors']:
        if d['name'] == 'gate':
            continue
        at = d['at']
        dx, dy = math.cos(math.radians(d['yaw'])), math.sin(math.radians(d['yaw']))
        human(hm, at[0] + dx * 10, at[1] + dy * 10, d['floor'], d['yaw'] + 180)
        break
    for w in body.meta['work'][:2]:
        human(hm, w['at'][0], w['at'][1], w['at'][2], 30, h=158)
    return hm


def main():
    out = sys.argv[1] if len(sys.argv) > 1 and not sys.argv[1].startswith('--') else os.path.join(
        HERE, '..', '..', 'Saved', 'ArchitecturePreview')
    only = None
    if '--only' in sys.argv:
        only = set(sys.argv[sys.argv.index('--only') + 1].split(','))
    os.makedirs(out, exist_ok=True)
    tiles = []
    for name, (body, foot, kind, tier) in arch.geometry(only).items():
        hm = humans_for(body)
        g = arch.Mesh()
        ground(g)
        for vname, cam in views_for(name, body):
            img = render([g, foot, body, hm], cam)
            label(img, '%s  -  %s  -  humain 170 cm' % (name, vname))
            path = os.path.join(out, '%s_%s.png' % (name, vname))
            img.save(path)
            tiles.append(path)
            print('ARCH_PREVIEW ' + path)
        # coupe : la facade d'acces retiree, on lit l'interieur
        lo, hi = body.bounds()
        span = max(hi[0] - lo[0], hi[1] - lo[1])
        cut_y = max(d['at'][1] for d in body.meta['doors'] if d['name'] != 'gate') - 40 if body.meta['doors'] else 0
        hin = arch.Mesh()
        for r in body.meta['sleep'][:1]:
            human(hin, r['at'][0], r['at'][1] + 60, r['at'][2] - 46, 90)
        for hh in body.meta['hearths'][:1]:
            human(hin, hh['at'][0] + 90, hh['at'][1] + 80, hh['at'][2] - 40, 200, h=160)
        cam = look_at(((lo[0] + hi[0]) / 2 + span * .25, cut_y + span * .7, 420), ((lo[0] + hi[0]) / 2, cut_y - 300, 260), fov=62)
        img = render([g, foot, body, hin, hm], cam, clip=((0, cut_y, 0), (0, 1, 0)))
        label(img, '%s  -  coupe sur la facade  -  humain 170 cm' % name)
        path = os.path.join(out, '%s_coupe.png' % name)
        img.save(path)
        tiles.append(path)
        print('ARCH_PREVIEW ' + path)
    print('ARCH_PREVIEW_COMPLETE %d images -> %s' % (len(tiles), os.path.abspath(out)))


if __name__ == '__main__':
    main()
