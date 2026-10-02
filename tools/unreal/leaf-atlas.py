"""LEAFCARDS_001 -- l'atlas de grappes de feuilles des arbres, dessine, pas telecharge.

**Python systeme, hors Unreal** (Pillow + numpy). Ecrit `SourceArt/Vegetation/leaf-atlas.png`
(RGBA, 1024 x 1024, quatre cases de 512) et une planche de controle
`docs/visual/tree-leafcards-001/leaf-atlas-preview.png`. `create-tree-cards.py` l'importe.

POURQUOI. Le feuillage actuel est fait de lames opaques de huit triangles, semees par centaines
(`create_tree_asset.py::leaf_blades`). A distance elles forment des eclats facettes, et les essais
pour les amincir ont depegarni les couronnes (`handoffs/tree-canopy-002.md`, REJECT). Une couronne
AAA est faite de CARTES : un quad portant une grappe de rameau et de feuilles, decoupe par un
masque alpha. Mille feuilles dessinees pour deux triangles.

LA TEXTURE NE PORTE PAS LA TEINTE. Meme contrat que les textures du sol (`ground-textures.py`) :
l'albedo est stocke a une moyenne de DETAIL_MEAN ; le materiau le ramene a 1 et le multiplie par
la couleur de sommet, qui reste la source de la teinte et de la semantique. Une essence change de
vert dans le script de l'arbre, pas dans l'atlas. Ce que l'atlas porte : la forme de la feuille,
sa nervure, ses nuances, la lumiere qui traverse (clair/sombre par feuille).

CASES (origine en haut a gauche) :
  0  chene vert / kermes : feuille ovale a pointe, dessus sombre et luisant, rameau fourni
  1  chene vert, grappe dense : les memes feuilles, plus serrees, sur un rameau court
  2  olivier : feuille etroite et lancelee, dessous argente sur un tiers des feuilles
  3  platane / feuillu large : feuille palmee a cinq lobes, plus grande

Dans chaque case le rameau monte du bas au centre vers le haut : c'est l'axe de la carte.

Lancer : python tools/unreal/leaf-atlas.py   (sortie deterministe : graine fixe)
"""
import math
import os
import random
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFilter

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
OUT_PNG = os.path.join(ROOT, 'SourceArt', 'Vegetation', 'leaf-atlas.png')
OUT_PREVIEW = os.path.join(ROOT, 'docs', 'visual', 'tree-leafcards-001', 'leaf-atlas-preview.png')
SEED = 20261002
CELL = 512
SUPER = 3                      # surechantillonnage : bords nets sans crenelage
DETAIL_MEAN = 0.45             # a garder egal a MEAN du materiau (create-tree-cards.py)
SOLID = 16                     # cote du bloc opaque (coin bas droit), SOLID_UV dans create-tree-cards.py


def bezier(p0, p1, p2, n):
    out = []
    for i in range(n + 1):
        t = i / n
        a = (1 - t) ** 2
        b = 2 * (1 - t) * t
        c = t * t
        out.append((a * p0[0] + b * p1[0] + c * p2[0], a * p0[1] + b * p1[1] + c * p2[1]))
    return out


def leaf_polygon(length, width, base_bias, tip_sharp, lobes=0, lobe_depth=0.0):
    """Contour d'une feuille le long de +X, base a l'origine. width = demi-largeur maximale.

    base_bias : ou la feuille est la plus large (0 = pres de la base, 1 = pres de la pointe).
    tip_sharp : > 1 effile la pointe. lobes : nombre de lobes palmes (platane).
    """
    n = 28
    top, bottom = [], []
    for i in range(n + 1):
        t = i / n
        # Profil : montee rapide puis descente en pointe, bosse placee par base_bias.
        peak = 0.25 + 0.5 * base_bias
        if t < peak:
            s = math.sin(0.5 * math.pi * t / peak) ** 0.8
        else:
            s = math.cos(0.5 * math.pi * (t - peak) / (1 - peak)) ** tip_sharp
        w = width * s
        if lobes:
            # Echancrures : le contour oscille, plus profond vers la base large.
            w *= 1.0 - lobe_depth * (0.5 - 0.5 * math.cos(2 * math.pi * lobes * t)) * math.sin(math.pi * t)
        top.append((length * t, w))
        bottom.append((length * t, -w))
    return top + bottom[::-1]


def draw_leaf(img, origin, angle, length, width, base_bias, tip_sharp, rgb, midrib, lobes=0, lobe_depth=0.0, rng=None):
    """Dessine une feuille : fond, degrade base-pointe, nervure et nervures secondaires."""
    poly = leaf_polygon(length, width, base_bias, tip_sharp, lobes, lobe_depth)
    ca, sa = math.cos(angle), math.sin(angle)
    pts = [(origin[0] + x * ca - y * sa, origin[1] + x * sa + y * ca) for x, y in poly]
    layer = Image.new('RGBA', img.size, (0, 0, 0, 0))
    d = ImageDraw.Draw(layer)
    d.polygon(pts, fill=(rgb[0], rgb[1], rgb[2], 255))
    # Degrade : la base est un peu plus sombre, la pointe un peu plus claire.
    steps = 6
    for k in range(steps):
        t0, t1 = k / steps, (k + 1) / steps
        shade = 0.88 + 0.22 * (k / (steps - 1))
        col = tuple(min(255, int(c * shade)) for c in rgb)
        seg = [(origin[0] + x * ca - y * sa, origin[1] + x * sa + y * ca)
               for x, y in poly if t0 * length <= x <= t1 * length]
        if len(seg) >= 3:
            d.polygon(seg, fill=col + (255,))
    # Nervure centrale et nervures secondaires.
    tip = (origin[0] + length * 0.96 * ca, origin[1] + length * 0.96 * sa)
    d.line([origin, tip], fill=midrib + (255,), width=max(2, int(width * 0.10)))
    for k in range(1, 6):
        t = k / 6.5
        px, py = origin[0] + length * t * ca, origin[1] + length * t * sa
        reach = width * (0.85 - 0.5 * t)
        for side in (-1, 1):
            a2 = angle + side * 0.95
            end = (px + reach * math.cos(a2), py + reach * math.sin(a2))
            d.line([(px, py), end], fill=tuple(int(c * 0.9) for c in rgb) + (255,), width=max(1, int(width * 0.045)))
    img.alpha_composite(layer)


def twig(img, base, tip, bend, width, rgb):
    pts = bezier(base, ((base[0] + tip[0]) / 2 + bend, (base[1] + tip[1]) / 2), tip, 24)
    d = ImageDraw.Draw(img)
    d.line(pts, fill=rgb + (255,), width=max(2, int(width)), joint='curve')
    return pts


def scale(v):
    return v * SUPER


def cell_canvas():
    return Image.new('RGBA', (CELL * SUPER, CELL * SUPER), (0, 0, 0, 0))


def colour(base, rng, spread=0.10):
    k = rng.uniform(1 - spread, 1 + spread)
    shift = rng.uniform(-0.06, 0.06)
    r, g, b = base
    return (min(255, int(r * k * (1 + shift))), min(255, int(g * k)), min(255, int(b * k * (1 - shift))))


def spray(rng, twigs, per_twig, leaf_len, leaf_aspect, base_col, midrib, silver_share=0.0, silver_col=None, wood=(74, 56, 40),
          bias=0.35, tip_sharp=1.3, fan=0.62, tone_spread=0.22):
    """Une gerbe : plusieurs rameaux en eventail depuis le pied de la carte, feuilles alternes.

    Une carte maigre (un seul rameau, 12 % de couverture) ne fait pas une masse de couronne : la
    gerbe en couvre 40 %, avec des tons de feuille tres varies (lumiere qui traverse).
    """
    img = cell_canvas()
    root = (scale(CELL * 0.50), scale(CELL * 0.985))
    for tw in range(twigs):
        spread = (tw / max(1, twigs - 1) - 0.5) * 2.0 if twigs > 1 else 0.0
        ang = spread * fan + rng.uniform(-0.08, 0.08)
        length = CELL * rng.uniform(0.78, 0.93) * (1.0 - 0.20 * abs(spread))
        tip = (root[0] + scale(math.sin(ang) * length), root[1] - scale(math.cos(ang) * length))
        spine = twig(img, root, tip, scale(rng.uniform(-16, 16)), scale(4.0 if twigs > 1 else 5.0), wood)
        for i in range(per_twig):
            t = 0.16 + 0.84 * (i / max(1, per_twig - 1)) + rng.uniform(-0.02, 0.02)
            idx = min(len(spine) - 1, int(t * (len(spine) - 1)))
            px, py = spine[idx]
            # Direction locale du rameau : les feuilles s'ouvrent de part et d'autre.
            j = min(len(spine) - 1, idx + 1)
            heading = math.atan2(spine[j][1] - spine[idx][1], spine[j][0] - spine[idx][0])
            side = -1 if i % 2 == 0 else 1
            angle = heading + side * rng.uniform(0.55, 1.05)
            L = scale(leaf_len * rng.uniform(0.78, 1.18) * (1.0 - 0.38 * (i / max(1, per_twig))))
            W = L * leaf_aspect * rng.uniform(0.90, 1.10)
            pet = (px + math.cos(angle) * scale(4), py + math.sin(angle) * scale(4))
            ImageDraw.Draw(img).line([(px, py), pet], fill=wood + (255,), width=scale(2))
            silver = silver_col is not None and rng.random() < silver_share
            col = silver_col if silver else base_col
            # Tons tres variables : la lumiere traverse certaines feuilles et en laisse d'autres sombres.
            tone = rng.uniform(1 - tone_spread, 1 + tone_spread)
            col = tuple(min(255, int(c * tone)) for c in col)
            draw_leaf(img, pet, angle, L, W, bias, tip_sharp, colour(col, rng, 0.08),
                      tuple(min(255, int(c * 1.0)) for c in midrib), rng=rng)
        draw_leaf(img, tip, heading if per_twig else -math.pi / 2, scale(leaf_len * 0.75), scale(leaf_len * 0.75 * leaf_aspect),
                  bias, tip_sharp, colour(base_col, rng, 0.08), midrib, rng=rng)
    return img


def oak_cluster(rng, dense):
    """Case 0/1 : gerbe de chene vert, feuilles ovales a pointe ; la 1 est plus serree et plus courte."""
    if dense:
        return spray(rng, twigs=5, per_twig=24, leaf_len=CELL * 0.082, leaf_aspect=0.30,
                     base_col=(118, 150, 84), midrib=(196, 214, 150), fan=0.62)
    return spray(rng, twigs=4, per_twig=22, leaf_len=CELL * 0.092, leaf_aspect=0.30,
                 base_col=(118, 150, 84), midrib=(196, 214, 150), fan=0.60)


def olive_cluster(rng):
    """Case 2 : feuilles lancelees, dessus vert-gris, dessous argente sur un tiers d'entre elles."""
    return spray(rng, twigs=4, per_twig=26, leaf_len=CELL * 0.110, leaf_aspect=0.10,
                 base_col=(104, 128, 92), midrib=(160, 180, 128), silver_share=0.34, silver_col=(158, 170, 150),
                 wood=(80, 70, 58), bias=0.45, tip_sharp=1.1, fan=0.55)


def plane_cluster(rng):
    """Case 3 : trois grandes feuilles palmees sur un petiole, d'un platane."""
    img = cell_canvas()
    base = (scale(CELL * 0.50), scale(CELL * 0.97))
    tip = (scale(CELL * 0.50), scale(CELL * 0.42))
    spine = twig(img, base, tip, 0.0, scale(6.0), (92, 84, 66))
    for i, (cx, cy, rot, size) in enumerate(((0.50, 0.40, 0.0, 0.40), (0.26, 0.62, -0.7, 0.32), (0.74, 0.62, 0.7, 0.32))):
        centre = (scale(CELL * cx), scale(CELL * cy))
        if i:
            ImageDraw.Draw(img).line([spine[len(spine) // 2], centre], fill=(92, 84, 66, 255), width=scale(4))
        colb = colour((108, 146, 74), rng, 0.10)
        # Cinq lobes : un grand en tete, deux lateraux, deux petits en bas.
        for k, (ang, ln, wd) in enumerate(((0.0, 1.0, 0.34), (0.95, 0.78, 0.26), (-0.95, 0.78, 0.26),
                                           (1.95, 0.52, 0.20), (-1.95, 0.52, 0.20))):
            a = -math.pi / 2 + rot + ang
            L = scale(CELL * size) * ln
            draw_leaf(img, centre, a, L, L * wd, 0.45, 1.3, colb, (190, 208, 148), rng=rng)
    return img


def finish(img):
    """Reduit, comble les bords transparents (couleur de la feuille voisine), normalise l'albedo."""
    small = img.resize((CELL, CELL), Image.LANCZOS)
    arr = np.asarray(small).astype(np.float32) / 255.0
    rgb, a = arr[..., :3], arr[..., 3]
    # Bords : sans cela, le mip lointain melange la couleur noire des texels vides aux feuilles.
    filled = a > 0.5
    work = rgb.copy()
    for _ in range(10):
        acc = np.zeros_like(work)
        cnt = np.zeros(filled.shape, dtype=np.float32)
        for dy in (-1, 0, 1):
            for dx in (-1, 0, 1):
                sh = np.roll(np.roll(filled, dy, axis=0), dx, axis=1)
                col = np.roll(np.roll(work, dy, axis=0), dx, axis=1)
                acc += col * sh[..., None]
                cnt += sh
        grow = (~filled) & (cnt > 0)
        work[grow] = acc[grow] / cnt[grow][..., None]
        filled = filled | grow
    return work, a


def main():
    rng = random.Random(SEED)
    cells = [oak_cluster(rng, False), oak_cluster(rng, True), olive_cluster(rng), plane_cluster(rng)]
    atlas = np.zeros((CELL * 2, CELL * 2, 4), dtype=np.float32)
    total_sum, total_n = np.zeros(3), 0
    finished = [finish(c) for c in cells]
    for rgb, a in finished:
        m = a > 0.5
        total_sum += rgb[m].sum(axis=0)
        total_n += int(m.sum())
    mean_luma = float((total_sum / max(total_n, 1)).mean())
    # Moyenne de l'albedo des feuilles = DETAIL_MEAN : la teinte reste celle du sommet.
    gain = DETAIL_MEAN / mean_luma
    for i, (rgb, a) in enumerate(finished):
        y0, x0 = (i // 2) * CELL, (i % 2) * CELL
        atlas[y0:y0 + CELL, x0:x0 + CELL, :3] = np.clip(rgb * gain, 0, 1)
        atlas[y0:y0 + CELL, x0:x0 + CELL, 3] = a
    # Un bloc opaque de 16 x 16 texels dans le coin bas droit de l'atlas : les meshes qui ne sont pas des
    # cartes (la couronne lointaine, une masse fermee) y envoient leurs UV (SOLID_UV) et restent pleins.
    atlas[-SOLID:, -SOLID:, :3] = DETAIL_MEAN
    atlas[-SOLID:, -SOLID:, 3] = 1.0
    os.makedirs(os.path.dirname(OUT_PNG), exist_ok=True)
    Image.fromarray((atlas * 255).astype(np.uint8), 'RGBA').save(OUT_PNG)
    # Planche de controle : l'atlas sur un ciel, albedo remonte a l'echelle (/DETAIL_MEAN * un vert de feuille).
    os.makedirs(os.path.dirname(OUT_PREVIEW), exist_ok=True)
    preview = Image.new('RGBA', (CELL * 2, CELL * 2), (128, 168, 214, 255))
    tint = np.array([0.30, 0.40, 0.18], dtype=np.float32) / DETAIL_MEAN
    colored = atlas.copy()
    colored[..., :3] = np.clip(atlas[..., :3] * tint * 1.6, 0, 1)
    preview.alpha_composite(Image.fromarray((colored * 255).astype(np.uint8), 'RGBA'))
    preview.convert('RGB').save(OUT_PREVIEW)
    cover = [float((a > 0.5).mean()) for rgb, a in finished]
    print('LEAF_ATLAS gain=%.3f coverage=%s -> %s' % (gain, ['%.2f' % c for c in cover], OUT_PNG))


if __name__ == '__main__':
    sys.exit(main())
