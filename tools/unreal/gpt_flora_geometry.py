"""GPT_FLORA_001 -- la geometrie pure des treize especes de la planche GPT : bois et cartes de feuillage.

Pas de `import unreal` ici : ce module sert deux appelants.
  * `gpt-flora-fit.py` (hors editeur) : rend l'arbre en logiciel et mesure sa silhouette contre le sprite ;
  * `create-gpt-flora.py` (editeur) : ecrit les StaticMesh.
Le meme code, la meme graine : ce que le banc mesure est ce que l'editeur construit.

ESPACE. Dessin normalise de create_tree_asset.py : l'arbre tient dans Z = [-50, +50] (100 unites), pied en
(0, 0, -50), X et Y a la meme echelle que Z. La hauteur reelle n'est pas ici : elle vient du registre de
presentation (`height_range_m`), comme pour les sept essences existantes.

UN SPEC (JSON, ecrit par gpt-flora-fit.py) :
  name, kind, mode            famille de la forme, mode de pose des cartes
  trunk   [[x, y, z, r], ...] polyligne du fut (vide pour un arbuste cepee, une fougère, une prairie)
  bands   [[zc, h, cx, cy, hw, hd], ...]  tranches de couronne tirees de la SILHOUETTE du sprite : centre,
                                          hauteur, decalage lateral, demi-largeur de face, demi-profondeur
  card_len, card_aspect, density  taille d'une carte (unites), largeur / longueur, couverture
  cells   [A, B]               cases de l'atlas ; cell_mode 'tint' (texture sans teinte) ou 'abs'
  foliage / bark  [r, g, b]    lineaire ; foliage_b pour la case B
"""
import math
import random

GOLDEN = 2.399963
CARD_ROWS = 2          # rangees de sommets d'une carte : un quad plie, deux triangles


def norm(v):
    n = math.sqrt(sum(c * c for c in v)) or 1.0
    return tuple(c / n for c in v)


def axis_x(spec, z):
    """Position (x, y) du fut a la hauteur z, par interpolation de la polyligne."""
    pts = spec.get('trunk') or []
    if not pts:
        return (0.0, 0.0)
    if z <= pts[0][2]:
        return (pts[0][0], pts[0][1])
    for a, b in zip(pts, pts[1:]):
        if z <= b[2]:
            t = (z - a[2]) / max(b[2] - a[2], 1e-6)
            return (a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t)
    return (pts[-1][0], pts[-1][1])


def build_wood(spec, rng):
    """Futs, branches maitresses : liste de (debut, fin, r0, r1, role)."""
    out = []
    pts = spec.get('trunk') or []
    for a, b in zip(pts, pts[1:]):
        # prolonge de 4 % : pas de fente au coude (meme geste que curved_stem)
        end = tuple(a[i] + (b[i] - a[i]) * 1.04 for i in range(3))
        out.append(((a[0], a[1], a[2]), end, a[3], b[3], 'trunk'))
    bands = spec['bands']
    mode = spec['mode']
    if mode in ('fronds', 'tufts'):
        return out
    if mode == 'shrub':
        for k in range(spec.get('stems', 6)):
            theta = k * GOLDEN + rng.uniform(-.3, .3)
            top = bands[-1][0] + bands[-1][1] * .5
            reach = rng.uniform(.35, .8)
            z1 = -50.0 + (top + 50.0) * rng.uniform(.62, .92)
            band = min(bands, key=lambda b: abs(b[0] - z1))
            start = (rng.uniform(-1, 1), rng.uniform(-1, 1), -50.0)
            end = (band[2] + reach * band[4] * math.cos(theta), band[3] + reach * band[5] * math.sin(theta), z1)
            mid = tuple((start[i] + end[i]) * .5 for i in range(3))
            mid = (mid[0] + (end[0] - start[0]) * .12, mid[1] + (end[1] - start[1]) * .12, mid[2] - 2.0)
            r0 = rng.uniform(.9, 1.5)
            out.append((start, mid, r0, r0 * .7, 'stem'))
            out.append((mid, end, r0 * .7, .30, 'stem'))
        return out
    # arbres : un tronc, puis des branches maitresses, une par tranche de couronne et par azimut
    limb_r = spec.get('limb_r', .5)
    for i, band in enumerate(bands):
        zc, h, cx, cy, hw, hd = band[:6]
        n = 2 if hw < 14 else 3
        base_z = zc - h * .5
        for k in range(n):
            theta = (i * .9 + k * math.tau / n) + rng.uniform(-.4, .4)
            ax, ay = axis_x(spec, base_z)
            start = (ax, ay, base_z)
            reach = rng.uniform(.45, .75)
            end = (cx + reach * hw * math.cos(theta), cy + reach * hd * math.sin(theta), zc + h * rng.uniform(.0, .35))
            mid = ((start[0] + end[0]) * .5, (start[1] + end[1]) * .5, start[2] + (end[2] - start[2]) * .35)
            r0 = max(.45, limb_r * (.55 + .45 * min(hw, 40.0) / 40.0))
            out.append((start, mid, r0, r0 * .62, 'limb'))
            out.append((mid, end, r0 * .62, .22, 'limb'))
    return out


def build_cards(spec, rng):
    """Cartes de feuillage : ancre, axe, longueur, largeur, case, ton, profondeur, hasard de conservation."""
    cards = []
    mode = spec['mode']
    length = spec['card_len']
    aspect = spec['card_aspect']
    density = spec['density']
    cells = spec['cells']
    alt_share = spec.get('alt_share', .3)

    z_top = spec['bands'][0][0] + spec['bands'][0][1] * .5

    def add(anchor, axis, ln, depth, droop):
        axis = norm(axis)
        # la pointe d'une carte ne depasse pas le haut de la silhouette du sprite
        tip_z = anchor[2] + axis[2] * ln - droop * ln
        limit = z_top + (rng.random() - .6) * .3 * ln
        if tip_z > limit and spec['mode'] not in ('fronds', 'tufts'):
            anchor = (anchor[0], anchor[1], anchor[2] - (tip_z - limit))
        cards.append({'anchor': anchor, 'axis': axis, 'len': ln, 'wid': ln * aspect,
                      'roll': rng.uniform(-.35, .35) if mode != 'tufts' else rng.uniform(0, math.tau),
                      'cell': cells[1] if (len(cells) > 1 and rng.random() < alt_share) else cells[0],
                      'depth': depth, 'tone': rng.uniform(.84, 1.14), 'keep': rng.random(),
                      'droop': droop})

    bands = spec['bands']
    if mode == 'fronds':
        hw = bands[0][4]
        n = spec.get('count', 13)
        for k in range(n):
            th = k * GOLDEN + rng.uniform(-.25, .25)
            tilt = math.radians(rng.uniform(6, 46))
            ln = length * rng.uniform(.8, 1.1)
            add((rng.uniform(-1, 1), rng.uniform(-1, 1), -50.0 + 1.0),
                (math.cos(th) * math.sin(tilt), math.sin(th) * math.sin(tilt), math.cos(tilt)), ln,
                .55 + .4 * (k % 3) / 2.0, rng.uniform(.08, .22))
        return cards
    if mode == 'tufts':
        hw = bands[0][4]
        top = bands[0][0] + bands[0][1] * .5
        n = spec.get('count', 34)
        for k in range(n):
            r = hw * math.sqrt((k + .5) / n) * .92
            th = k * GOLDEN
            lean = math.radians(rng.uniform(0, 26))
            ln = (top + 50.0) * rng.uniform(.62, 1.0)
            add((r * math.cos(th), r * math.sin(th), -50.0 + 1.0),
                (math.cos(th) * math.sin(lean), math.sin(th) * math.sin(lean), math.cos(lean)), ln,
                .45 + .5 * (r / max(hw, 1.0)), rng.uniform(.0, .12))
        return cards
    for i, band in enumerate(bands):
        zc, h, cx, cy, hw, hd = band[:6]
        fill = band[6] if len(band) > 6 else 1.0
        per = math.tau * math.sqrt((hw * hw + hd * hd) / 2.0)
        ln = length
        if mode == 'weeping':
            ln = max(h * 2.1, length)
            n_shell = max(5, int(round(density * 2.0 * per / (ln * aspect))))
            for k in range(n_shell):
                th = k * GOLDEN + rng.uniform(-.3, .3)
                rho = rng.uniform(.62, 1.0) if i == 0 else rng.uniform(.85, 1.02)
                anchor = (cx + rho * hw * math.cos(th), cy + rho * hd * math.sin(th), zc + h * .5)
                length_k = min(ln * rng.uniform(.85, 1.15), (anchor[2] + 49.0) / .98)   # jamais sous le pied de l'arbre
                add(anchor, (math.cos(th) * .12, math.sin(th) * .12, -1.0), length_k, .55 + .4 * rho, .0)
            continue
        n_shell = max(3, int(round(density * 3.0 * per * h * max(fill, .35) ** .8 / (ln * ln * aspect))))
        for k in range(n_shell):
            th = k * GOLDEN + rng.uniform(-.4, .4)
            zz = zc + h * rng.uniform(-.5, .5)
            rho = max(.25, 1.0 - ln * .55 / max(hw, 1.0)) * rng.uniform(.80, 1.05)
            anchor = (cx + rho * hw * math.cos(th), cy + rho * hd * math.sin(th), zz)
            lift = .25 if mode != 'column' else .75
            wander = norm((rng.gauss(0, 1), rng.gauss(0, 1), rng.gauss(0, 1)))
            mix = rng.uniform(.2, .6)
            out_x, out_y = math.cos(th) * (1 - mix) + wander[0] * mix, math.sin(th) * (1 - mix) + wander[1] * mix
            if mode == 'column':
                out_x, out_y = out_x * .5, out_y * .5
            add(anchor, (out_x, out_y, lift + wander[2] * mix), ln * rng.uniform(.85, 1.2),
                .55 + .45 * rho, rng.uniform(.08, .26))
        # remplissage du volume : cartes plus sombres, vers l'interieur, qui bouchent le jour a travers la couronne
        for k in range(max(2, n_shell // 2) if fill >= .55 else 0):
            th = k * GOLDEN + 1.1
            zz = zc + h * rng.uniform(-.5, .5)
            rho = rng.uniform(.10, .55)
            anchor = (cx + rho * hw * math.cos(th), cy + rho * hd * math.sin(th), zz)
            add(anchor, (math.cos(th) * .6, math.sin(th) * .6, .5 + rng.uniform(-.2, .3)), ln * rng.uniform(.9, 1.2),
                .25 + .3 * rho, rng.uniform(.1, .3))
    return cards


def card_vertices(card, lod=0):
    """Sommets (CARD_ROWS rangees de deux) et leurs UV locaux (u, t), comme add_card de create-tree-cards.py."""
    ln = card['len'] * (1.0 if lod == 0 else 1.15)
    wid = card['wid'] * (1.0 if lod == 0 else 1.15)
    axis = card['axis']
    ref = (0.0, 0.0, 1.0) if abs(axis[2]) < 0.95 else (1.0, 0.0, 0.0)
    right = norm((axis[1] * ref[2] - axis[2] * ref[1], axis[2] * ref[0] - axis[0] * ref[2],
                  axis[0] * ref[1] - axis[1] * ref[0]))
    up = (right[1] * axis[2] - right[2] * axis[1], right[2] * axis[0] - right[0] * axis[2],
          right[0] * axis[1] - right[1] * axis[0])
    ca, sa = math.cos(card['roll']), math.sin(card['roll'])
    right = tuple(right[i] * ca + up[i] * sa for i in range(3))
    rows = []
    for row in range(CARD_ROWS):
        t = row / float(CARD_ROWS - 1)
        p = [card['anchor'][i] + axis[i] * ln * t for i in range(3)]
        p[2] -= card['droop'] * ln * t * t
        for side in (-1.0, 1.0):
            rows.append(((p[0] + right[0] * .5 * wid * side, p[1] + right[1] * .5 * wid * side,
                          p[2] + right[2] * .5 * wid * side), (0.5 + 0.5 * side * 0.99, 1.0 - t * 0.985)))
    return rows
