"""VILLAGER_PNG_001 -- chaine hors editeur des PNG d'habitants (Python systeme : Pillow + numpy).

Les habitants sont DESSINES sur les planches de SourceArt/Characters/Sheets/ ; le manifeste
SourceArt/Characters/villager-population.json dit ou est chacun. Ce script n'invente personne.

    python tools/unreal/villager-png.py sheets    # planches -> Raw/<id>.png + villager-extract.json (statures)
    python tools/unreal/villager-png.py prep      # Raw/<id>.png -> PNG/<Categorie>/<id>.png (canevas commun)
    python tools/unreal/villager-png.py board     # planches de comparaison -> docs/visual/villager-png-001/
    python tools/unreal/villager-png.py check     # ressemblance silhouette / visage, par paire, par categorie

sheets : chaque panneau est detoure sur son fond creme ; les etiquettes CHR_* sous les figures
donnent les colonnes (coupees aux k-1 plus grands ecarts : les etiquettes de la serie 2 se touchent
presque) ; l'ombre portee au sol du dessin est retiree (le moteur fait la sienne) ; une piece coupee
par le bord de colonne est l'outil d'un voisin et part ; une figure qui regarde a droite est
retournee (les cartes supposent un portrait tourne a gauche). Stature = stature moyenne de la
categorie x hauteur du CORPS dessinee / mediane du panneau (la pointe d'une lance ne compte pas).

prep : alpha natif s'il existe, sinon detourage d'un fond uni. Puis decontamination des bords,
recadrage, mise a l'echelle du CORPS a la stature (4 px/cm), pieds centres sur une meme ligne,
saignement de couleur sous l'alpha nul (mips propres). Ne modifie jamais Raw/.
"""

import json
import sys
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[2]
CHARACTERS = ROOT / "SourceArt" / "Characters"
MANIFEST = CHARACTERS / "villager-population.json"
EXTRACT = CHARACTERS / "villager-extract.json"
RAW = CHARACTERS / "Raw"
OUT = CHARACTERS / "PNG"
BOARDS = ROOT / "docs" / "visual" / "villager-png-001"

CATEGORY_ORDER = ["Adult_Male", "Adult_Female", "Elder_Male", "Elder_Female", "Child_Male", "Child_Female"]


def load_manifest():
    """Le manifeste, et sous "people" la population extraite (villager-extract.json, ecrit par sheets)."""
    with MANIFEST.open(encoding="utf-8") as f:
        m = json.load(f)
    m["people"] = json.loads(EXTRACT.read_text(encoding="utf-8"))["people"] if EXTRACT.exists() else []
    return m


# ------------------------------------------------------------------------------------------------ prep

def shift(a, dy, dx):
    """Decalage sans enroulement, bords remplis de False/0."""
    out = np.zeros_like(a)
    h, w = a.shape[:2]
    ys, yd = (slice(dy, h), slice(0, h - dy)) if dy >= 0 else (slice(0, h + dy), slice(-dy, h))
    xs, xd = (slice(dx, w), slice(0, w - dx)) if dx >= 0 else (slice(0, w + dx), slice(-dx, w))
    out[ys, xs] = a[yd, xd]
    return out


def dilate(mask):
    out = mask.copy()
    for dy, dx in ((1, 0), (-1, 0), (0, 1), (0, -1), (1, 1), (1, -1), (-1, 1), (-1, -1)):
        out |= shift(mask, dy, dx)
    return out


def dilate_n(mask, steps):
    for _ in range(steps):
        mask = dilate(mask)
    return mask


def erode(mask, steps=1):
    for _ in range(steps):
        mask = ~dilate(~mask)
    return mask


def propagate(seed, allowed):
    """Composantes de `allowed` qui touchent `seed` (4-voisinage), par le remplissage natif de Pillow."""
    # copy() : une image fromarray partage le tampon numpy en lecture seule, le remplissage y serait perdu.
    img = Image.fromarray(np.where(allowed, 255, 0).astype(np.uint8)).copy()
    for y, x in np.argwhere(seed & allowed):
        if img.getpixel((int(x), int(y))) == 255:
            ImageDraw.floodfill(img, (int(x), int(y)), 128, thresh=0)
    return np.asarray(img) == 128


def fit_background(rgb, border):
    """Modele quadratique du fond par canal, ajuste sur une bande de bord."""
    h, w = rgb.shape[:2]
    yy, xx = np.mgrid[0:h, 0:w].astype(np.float64)
    yn, xn = yy / h - 0.5, xx / w - 0.5
    basis = np.stack([np.ones_like(xn), xn, yn, xn * xn, yn * yn, xn * yn], axis=-1)
    a = basis[border]
    model = np.empty_like(rgb, dtype=np.float64)
    for c in range(3):
        coef, *_ = np.linalg.lstsq(a, rgb[..., c][border].astype(np.float64), rcond=None)
        model[..., c] = basis @ coef
    return model


def key_flat_background(rgb):
    """Rend (alpha float 0..1, couleur de fond estimee par pixel)."""
    h, w = rgb.shape[:2]
    band = max(4, min(h, w) // 64)
    border = np.zeros((h, w), bool)
    border[:band, :] = border[-band:, :] = True
    border[:, :band] = border[:, -band:] = True
    # Le personnage ne touche pas les bords d'une image correcte ; si un pixel de bord est loin de la
    # mediane du bord, on l'exclut de l'ajustement (pied coupe, signature).
    med = np.median(rgb[border].astype(np.float64), axis=0)
    border &= np.linalg.norm(rgb.astype(np.float64) - med, axis=-1) < 40
    bg = fit_background(rgb, border)
    d = np.linalg.norm(rgb.astype(np.float64) - bg, axis=-1)
    noise = float(np.percentile(d[border], 99))
    t_lo, t_hi = max(6.0, noise * 1.5), max(22.0, noise * 4.0)
    candidate = d < t_hi
    outside = propagate(border & candidate, candidate)
    # Trous fermes (entre bras et corps) : seulement s'ils ont exactement la couleur du fond.
    # Trous fermes (main sur la hanche) : seulement sur un fond franchement colore (vert d'incrustation).
    # Sur un fond gris, une piece de lin ecru a la couleur du fond : la percer serait pire que
    # laisser un trou plein, qui est signale. Et on ne garde que les zones dont le coeur survit a
    # trois erosions : une zone fine de cette couleur est un reflet.
    chroma = float(np.ptp(bg[border].mean(axis=0))) > 60.0
    holes = (d < t_lo) & ~outside
    holes = propagate(erode(holes, 3), holes)
    if not chroma:
        holes_left = int(holes.sum())
        holes[:] = False
    else:
        holes_left = 0
    background = outside | holes
    return soft_alpha(rgb, bg, background), bg, (t_lo, t_hi, noise, chroma, holes_left)


def soft_alpha(rgb, bg, background):
    """Lisiere : un pixel de bord est un MELANGE I = a*F + (1-a)*B. Un seuil absolu sur |I-B| le
    declare opaque des que le sujet est contraste, et le liseré clair reste. On estime F par la
    couleur du sujet la plus proche (interieur etendu vers l'exterieur), puis a par projection
    de I-B sur F-B."""
    subject = ~background
    alpha = subject.astype(np.float64)
    band = dilate_n(subject, 2) & ~erode(subject, 2)
    core = erode(subject, 3)
    if core.any():
        f = bleed(rgb, core.astype(np.float64), steps=8)
        fb = f - bg
        den = (fb * fb).sum(axis=-1)
        a = ((rgb.astype(np.float64) - bg) * fb).sum(axis=-1) / np.maximum(den, 1e-6)
        # Sujet trop proche du fond pour que la projection ait un sens : on garde le masque binaire.
        usable = band & (den > 25.0 ** 2)
        alpha[usable] = np.clip(a[usable], 0.0, 1.0)
    alpha[erode(background, 3)] = 0.0
    return alpha


def decontaminate(rgb, alpha, bg):
    """I = a*F + (1-a)*B  =>  F = (I - (1-a)*B) / a, sur les bords semi-transparents."""
    rgb = rgb.astype(np.float64)
    edge = (alpha > 0.02) & (alpha < 0.98)
    a = alpha[edge][:, None]
    rgb[edge] = np.clip((rgb[edge] - (1.0 - a) * bg[edge]) / a, 0, 255)
    return rgb


def bleed(rgb, alpha, steps=24):
    """Etend la couleur des pixels visibles sous l'alpha nul, pour que les mips ne tirent pas vers le noir."""
    rgb = rgb.astype(np.float64).copy()
    known = alpha > 0.5
    for _ in range(steps):
        acc = np.zeros_like(rgb)
        cnt = np.zeros(alpha.shape, np.float64)
        for dy, dx in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            k = shift(known, dy, dx)
            acc += shift(rgb, dy, dx) * k[..., None]
            cnt += k
        new = (~known) & (cnt > 0)
        rgb[new] = acc[new] / cnt[new][:, None]
        known |= new
    if (~known).any():
        rgb[~known] = rgb[known].mean(axis=0)
    return rgb


def prep_one(m, p):
    src = RAW / f"{p['id']}.png"
    img = Image.open(src)
    rgba = np.asarray(img.convert("RGBA")).astype(np.float64)
    rgb, a8 = rgba[..., :3], rgba[..., 3]
    native = (a8 < 250).mean() > 0.05
    holes_left = 0
    if native:
        alpha = a8 / 255.0
        # Couleur de fond inconnue : on decontamine contre la couleur moyenne de la frange externe,
        # qui est ce que le generateur a melange au sujet.
        fringe = (alpha > 0.02) & (alpha < 0.5)
        bg_col = rgb[fringe].mean(axis=0) if fringe.any() else np.zeros(3)
        bg = np.broadcast_to(bg_col, rgb.shape)
        mode, info = "alpha", ""
    else:
        alpha, bg, (t_lo, t_hi, noise, chroma, holes_left) = key_flat_background(rgb)
        mode = "keyed_chroma" if chroma else "keyed_neutre"
        info = f" t=({t_lo:.0f},{t_hi:.0f}) bruit_fond={noise:.1f}"
        if holes_left:
            info += f" trous_fermes_non_traites={holes_left}px"
    fg = decontaminate(rgb, alpha, bg)

    solid = alpha > 0.1
    if not solid.any():
        raise RuntimeError("aucun sujet detecte")
    ys, xs = np.nonzero(solid)
    y0, y1, x0, x1 = ys.min(), ys.max() + 1, xs.min(), xs.max() + 1
    h_img, w_img = alpha.shape
    touches = [s for s, t in (("haut", y0 == 0), ("bas", y1 == h_img), ("gauche", x0 == 0), ("droite", x1 == w_img)) if t]
    # Centre des pieds : 4 % du bas de la silhouette.
    foot_rows = solid[max(y0, y1 - max(2, (y1 - y0) // 25)):y1]
    foot_cx = np.nonzero(foot_rows)[1].mean()

    cv = m["canvas"]
    px_per_cm = cv["height"] / cv["cm_per_canvas_height"]
    target_h = p["stature_cm"] * px_per_cm
    # Le CORPS a la stature : une lance qui depasse la tete ne doit pas rapetisser le garde.
    s = target_h / (y1 - body_top(solid))

    crop = np.dstack([fg[y0:y1, x0:x1], alpha[y0:y1, x0:x1, None] * 255.0]).clip(0, 255).astype(np.uint8)
    pil = Image.fromarray(crop, "RGBA").convert("RGBa")
    nw, nh = max(1, round((x1 - x0) * s)), max(1, round((y1 - y0) * s))
    pil = pil.resize((nw, nh), Image.LANCZOS).convert("RGBA")

    canvas = Image.new("RGBA", (cv["width"], cv["height"]), (0, 0, 0, 0))
    # Les pieds sur l'axe du canevas : c'est le pivot du billboard. Un baton ou un manteau
    # peut deporter la silhouette ; on recentre alors sur la boite, et on le dit.
    left = round(cv["width"] / 2 - (foot_cx - x0) * s)
    top = cv["height"] - cv["foot_margin_px"] - nh
    recentred = left < 0 or left + nw > cv["width"]
    if recentred:
        left = (cv["width"] - nw) // 2
    if left < 0 or top < 0:
        raise RuntimeError(f"debordement du canevas ({nw}x{nh} a {left},{top}) : silhouette trop large pour {cv['width']} px")
    canvas.alpha_composite(pil, (left, top))

    out = np.asarray(canvas).astype(np.float64)
    out_rgb = bleed(out[..., :3], out[..., 3] / 255.0)
    final = np.dstack([out_rgb, out[..., 3:]]).clip(0, 255).astype(np.uint8)
    dst = OUT / p["category"] / f"{p['id']}.png"
    dst.parent.mkdir(parents=True, exist_ok=True)
    Image.fromarray(final, "RGBA").save(dst, optimize=True)

    halo = halo_score(final)
    status = "OK" if not touches and halo < 0.15 and not holes_left else "A_REVOIR"
    print(f"PREP::{status} {p['id']} mode={mode}{info} source={w_img}x{h_img} sujet={x1 - x0}x{y1 - y0} "
          f"echelle={s:.3f} -> {nw}x{nh} halo={halo:.3f}" + (f" coupe={','.join(touches)}" if touches else "")
          + (" pieds_decentres" if recentred else ""))
    return status


def halo_score(rgba):
    """Part des pixels de lisiere nettement plus clairs que l'interieur voisin (halo blanc residuel)."""
    a = rgba[..., 3] > 127
    edge = a & ~erode(a)  # couronne interne de 1 px
    inner = erode(a, 4)
    if not edge.any() or not inner.any():
        return 0.0
    lum = rgba[..., :3].astype(np.float64) @ np.array([0.2126, 0.7152, 0.0722])
    # Luminance interieure locale : moyenne des pixels interieurs dans un voisinage de 6 px.
    acc, cnt = np.zeros_like(lum), np.zeros_like(lum)
    for dy in range(-6, 7, 3):
        for dx in range(-6, 7, 3):
            k = shift(inner, dy, dx)
            acc += shift(lum, dy, dx) * k
            cnt += k
    ok = edge & (cnt > 0)
    ref = acc[ok] / cnt[ok]
    return float(((lum[ok] - ref) > 40).mean())


def cmd_prep(m, only=None):
    counts = {"OK": 0, "A_REVOIR": 0, "ERREUR": 0, "ABSENT": 0}
    for p in m["people"]:
        if only and p["id"] not in only:
            continue
        if not (RAW / f"{p['id']}.png").exists():
            counts["ABSENT"] += 1
            continue
        try:
            counts[prep_one(m, p)] += 1
        except Exception as e:  # une image fautive ne bloque pas les autres
            counts["ERREUR"] += 1
            print(f"PREP::ERREUR {p['id']} {e}")
    stray = sorted(f.name for f in RAW.glob("*.png") if f.stem not in {p["id"] for p in m["people"]})
    if stray:
        print(f"PREP::IGNORE noms hors manifeste: {', '.join(stray)}")
    print("PREP::TOTAL " + " ".join(f"{k}={v}" for k, v in counts.items()))


# ---------------------------------------------------------------------------------------------- sheets

def body_top(solid):
    """Premiere ligne assez large pour etre une tete : une hampe, une pointe de lance ou les dents d'une
    fourche au-dessus de la tete sont plus etroites (seuil 4,5 % de la hauteur de la silhouette)."""
    rows = np.nonzero(solid.any(axis=1))[0]
    y0, y1 = rows.min(), rows.max() + 1
    widths = solid[y0:y1].sum(axis=1)
    wide = np.nonzero(widths >= max(3.0, 0.045 * (y1 - y0)))[0]
    return int(y0 + (wide[0] if len(wide) else 0))


def components(mask):
    """Composantes 4-connexes d'un masque, par le remplissage natif de Pillow."""
    img = Image.fromarray(np.where(mask, 255, 0).astype(np.uint8)).copy()
    out = []
    while True:
        arr = np.asarray(img)
        left = np.argwhere(arr == 255)
        if len(left) == 0:
            return out
        y, x = left[0]
        ImageDraw.floodfill(img, (int(x), int(y)), 128, thresh=0)
        comp = np.asarray(img) == 128
        out.append(comp)
        img.paste(64, mask=Image.fromarray((comp * 255).astype(np.uint8)))


def label_band(fg, lines=1):
    """Lignes de la bande d'etiquettes : les `lines` derniers blocs de lignes non vides, au bas du
    panneau. Un bloc de moins de 5 lignes est un filet de cadre, pas du texte : on le saute."""
    rows = fg.any(axis=1)
    blocks = []
    y = len(rows) - 1
    while y >= 0 and len(blocks) < lines:
        while y >= 0 and not rows[y]:
            y -= 1
        end = y
        while y >= 0 and rows[y]:
            y -= 1
        if end - y >= 5:
            blocks.append((y + 1, end))
    return blocks[-1][0], blocks[0][1]


def label_centres(fg_band, k):
    """Centres des k etiquettes : la bande est coupee a ses k-1 plus grands ecarts horizontaux."""
    cols = np.nonzero(fg_band.any(axis=0))[0]
    gaps = np.diff(cols)
    cut = sorted(np.argsort(gaps)[::-1][:k - 1])
    starts = [cols[0]] + [cols[i + 1] for i in cut]
    ends = [cols[i] for i in cut] + [cols[-1]]
    return [(a + b) / 2.0 for a, b in zip(starts, ends)], [int(g) for g in sorted(gaps)[::-1][:k]]


def key_sheet(rgb):
    """Detourage d'une colonne de planche (fond creme uni, peint). Differences avec key_flat_background :

    - l'ombre au sol du dessin est du FOND des la propagation : bas de la silhouette, meme teinte
      que le fond, juste plus sombre. Retiree apres coup, elle murait l'espace entre les jambes,
      qui restait creme (CHR_M_Adult_005, premier decoupage), et reliait deux voisins ;
    - les trous fermes de la couleur exacte du fond sont perces : sur une planche le creme est
      plat, un lin ecru a de la texture et de l'ombre, il ne tient pas trois erosions a moins de
      t_lo du fond (une erosion suffit a ecarter un reflet d'un pixel ; les mailles d'un filet passent).
    """
    h, w = rgb.shape[:2]
    rgb = rgb.astype(np.float64)
    band = max(4, min(h, w) // 64)
    border = np.zeros((h, w), bool)
    border[:band, :] = border[-band:, :] = True
    border[:, :band] = border[:, -band:] = True
    med = np.median(rgb[border], axis=0)
    border &= np.linalg.norm(rgb - med, axis=-1) < 40
    bg = fit_background(rgb, border)
    d = np.linalg.norm(rgb - bg, axis=-1)
    noise = float(np.percentile(d[border], 99))
    t_lo, t_hi = max(6.0, noise * 1.5), max(22.0, noise * 4.0)
    rows = np.nonzero((d >= t_hi).any(axis=1))[0]
    y0, y1 = (rows.min(), rows.max() + 1) if len(rows) else (0, h)
    low = np.zeros((h, w), bool)
    low[max(y0, y1 - int(0.12 * (y1 - y0))):] = True
    ratio = rgb / np.maximum(bg, 1.0)
    # Teinte du fond a 0,10 pres : 0,07 laissait des restes d'ombre contre les pieds (M_Elder_001).
    shadow = low & (ratio.min(axis=-1) > 0.55) & (ratio.max(axis=-1) < 0.998)         & ((ratio.max(axis=-1) - ratio.min(axis=-1)) < 0.10)
    candidate = (d < t_hi) | shadow
    outside = propagate(border & candidate, candidate)
    # Une erosion suffit : les mailles d'un filet de peche (M_Adult_009, M_Adult_014, M_Elder_010)
    # font 2 a 5 px et restaient creme avec trois.
    holes = ((d < t_lo) | shadow) & ~outside
    holes = propagate(erode(holes, 1), holes)
    background = outside | holes
    return soft_alpha(rgb, bg, background), bg, int((shadow & background).sum())


def extract_panel(rgb, centres, bounds):
    """Detoure un panneau d'un bloc, puis donne chaque piece a l'etiquette la plus proche.

    Couper en colonnes au milieu de deux etiquettes coupait le sac de CHR_M_Adult_005 et le baton de
    CHR_M_Adult_006 (premier decoupage) : un objet porte deborde souvent du milieu. Ici une piece
    entiere suit son centre de masse ; seule une piece qui couvre DEUX centres d'etiquette (deux
    figures qui se touchent) est coupee au milieu, et c'est signale.
    Rend {indice: (rgba de la taille du panneau, nb pieces ecartees, fusion)} et le nombre de pixels
    d'ombre au sol retires."""
    alpha, bg, shadow_px = key_sheet(rgb)
    owned = [np.zeros(alpha.shape, bool) for _ in centres]
    fused = [False] * len(centres)
    for c in components(alpha > 0.5):
        ys, xs = np.nonzero(c)
        if xs.max() - xs.min() < 5 or len(xs) < 12:   # filet entre deux cases, poussiere
            continue
        inside = [i for i, cx in enumerate(centres) if xs.min() <= cx <= xs.max()]
        heavy = [i for i in inside if (c[:, bounds[i]:bounds[i + 1]]).sum() > 0.15 * len(xs)]
        if len(heavy) >= 2:
            for i in heavy:
                part = np.zeros_like(c)
                part[:, bounds[i]:bounds[i + 1]] = c[:, bounds[i]:bounds[i + 1]]
                owned[i] |= part
                fused[i] = True
            continue
        cx = xs.mean()
        owned[int(np.argmin([abs(cx - k) for k in centres]))] |= c
    out = {}
    for i, mask in enumerate(owned):
        if not mask.any():
            raise RuntimeError(f"figure {i + 1} : aucune piece")
        pieces = sorted(components(mask), key=lambda c: -c.sum())
        keep = pieces[0].copy()
        dropped = 0
        for c in pieces[1:]:
            if c.sum() >= 0.004 * pieces[0].sum():
                keep |= c
            else:
                dropped += 1
        a = np.where(dilate_n(keep, 2), alpha, 0.0)
        fg = decontaminate(rgb, a, bg)
        out[i] = (np.dstack([fg, a * 255.0]).clip(0, 255).astype(np.uint8), dropped, fused[i])
    return out, shadow_px


def cmd_sheets(m):
    RAW.mkdir(parents=True, exist_ok=True)
    people = []
    overrides = {k: v for k, v in m.get("overrides", {}).items() if not k.startswith("_")}
    for sheet in m["sheets"]:
        rgb_sheet = np.asarray(Image.open(CHARACTERS / sheet["file"]).convert("RGB")).astype(np.float64)
        for panel in sheet["panels"]:
            x0, y0, x1, y1 = panel["rect"]
            rgb = rgb_sheet[y0:y1, x0:x1]
            ids = panel["ids"]
            bg_col = np.median(rgb[:6].reshape(-1, 3), axis=0)
            fg = np.linalg.norm(rgb - bg_col, axis=-1) > 60
            b0, b1 = label_band(fg, panel.get("label_lines", 1))
            if not 6 <= b1 - b0 + 1 <= 24 * panel.get("label_lines", 1):
                raise RuntimeError(f"{sheet['file']} {panel['category']} : bande d'etiquettes introuvable ({b0}-{b1})")
            # Les centres se lisent sur la DERNIERE ligne d'etiquette (le role, en serie 4, est centre aussi).
            centres, gaps = label_centres(fg[b1 - 4:b1 + 1], len(ids))
            bounds = [0] + [int((a + b) / 2) for a, b in zip(centres, centres[1:])] + [x1 - x0]
            figures = rgb[:max(0, b0 - 2)]
            extracted, shadow_px = extract_panel(figures, centres, bounds)
            measured = []
            for i, pid in enumerate(ids):
                rgba, dropped, cut = extracted[i]
                solid = rgba[..., 3] > 25
                ys, xs = np.nonzero(solid)
                box = rgba[max(0, ys.min() - 3):ys.max() + 4, max(0, xs.min() - 3):xs.max() + 4]
                box = np.pad(box, ((3, 3), (3, 3), (0, 0)))
                o = overrides.get(pid, {})
                if o.get("facing") == "right":
                    box = box[:, ::-1]
                Image.fromarray(np.ascontiguousarray(box), "RGBA").save(RAW / f"{pid}.png")
                body = ys.max() + 1 - body_top(solid)
                measured.append((pid, body, o, dropped, shadow_px, cut, box.shape))
            standing = [b for _, b, o, *_ in measured if o.get("pose", "debout") == "debout"] or [b for _, b, *_ in measured]
            median = float(np.median(standing))
            mean_cm = m["categories"][panel["category"]]["stature_cm"]
            for pid, body, o, dropped, shadow_px, cut, shape in measured:
                # Assis, la figure est souvent dessinee aussi haute qu'un voisin debout (serie 5) :
                # son dessin ne dit rien de sa taille. Assis = 72 % de la stature debout de la categorie.
                stature = round(0.72 * mean_cm) if o.get("pose") == "assis" else round(mean_cm * body / median)
                people.append({"id": pid, "category": panel["category"], "stature_cm": stature,
                               "body_px": int(body), "sheet": sheet["file"],
                               "in_game": o.get("pose", "debout") == "debout", "facing": o.get("facing", "left"),
                               # Metier simule dont le portrait porte l'objet ; vide = role que la simulation n'a pas.
                               "jobs": [o["job"]] if o.get("job") else [],
                               "pose": o.get("pose", "debout"), "note": o.get("note", "")})
                print(f"SHEETS::{'A_REVOIR' if cut else 'OK'} {pid} corps={body}px stature={stature}cm "
                      f"boite={shape[1]}x{shape[0]} pieces_ecartees={dropped}"
                      + (" fusion_avec_un_voisin_coupee" if cut else "") + (f" pose={o['pose']}" if o.get("pose") else "")
                      + (" retournee" if o.get("facing") == "right" else ""))
            print(f"SHEETS::PANNEAU {sheet['file']} {panel['category']} n={len(ids)} ecarts_etiquettes={gaps} "
                  f"mediane_corps={median:.0f}px ombre_retiree={shadow_px}px")
    EXTRACT.write_text(json.dumps({"_doc": "Ecrit par villager-png.py sheets -- ne pas editer : changer le manifeste et relancer.",
                                   "people": people}, indent=1, ensure_ascii=False), encoding="utf-8")
    counts = {c: sum(1 for p in people if p["category"] == c) for c in CATEGORY_ORDER}
    print("SHEETS::TOTAL " + " ".join(f"{c}={n}" for c, n in counts.items()) + f" total={len(people)}"
          + f" en_jeu={sum(1 for p in people if p['in_game'] and p['jobs'] and not p['category'].startswith('Child'))}"
          + " " + " ".join(f"{j}={sum(1 for p in people if j in p['jobs'] and p['in_game'] and not p['category'].startswith('Child'))}" for j in ("settler", "farmer")))


# ----------------------------------------------------------------------------------------------- board

def processed(m):
    out = []
    for p in m["people"]:
        f = OUT / p["category"] / f"{p['id']}.png"
        if f.exists():
            out.append((p, Image.open(f).convert("RGBA")))
    return out


def font(size):
    for name in ("arial.ttf", "DejaVuSans.ttf"):
        try:
            return ImageFont.truetype(name, size)
        except OSError:
            pass
    return ImageFont.load_default()


def lineup(items, bg, title, scale=0.5, silhouette=False):
    cw, ch = items[0][1].size
    w, h = int(cw * scale * 0.62), int(ch * scale)
    cols = 8
    rows = (len(items) + cols - 1) // cols
    sheet = Image.new("RGBA", (cols * w + 40, rows * (h + 34) + 70), bg)
    d = ImageDraw.Draw(sheet)
    d.text((20, 18), title, fill=(30, 30, 30) if sum(bg[:3]) > 380 else (230, 230, 230), font=font(22))
    for i, (p, im) in enumerate(items):
        r, c = divmod(i, cols)
        im = im.resize((int(cw * scale), h), Image.LANCZOS)
        if silhouette:
            k = np.asarray(im)[..., 3]
            im = Image.fromarray(np.dstack([np.full(k.shape + (3,), 25, np.uint8), k]), "RGBA")
        cell = Image.new("RGBA", (w, h), (0, 0, 0, 0))
        cell.alpha_composite(im, ((w - im.width) // 2, 0))
        x, y = 20 + c * w, 60 + r * (h + 34)
        sheet.alpha_composite(cell, (x, y))
        d.line([(x + 4, y + h - 8 * scale), (x + w - 4, y + h - 8 * scale)], fill=(120, 110, 95), width=1)
        d.text((x + 4, y + h + 4), p["id"].replace("CHR_", ""), fill=(60, 60, 60) if sum(bg[:3]) > 380 else (200, 200, 200), font=font(13))
    return sheet.convert("RGB")


def head_crop(p, im, m):
    """Le haut de la silhouette sur ~1/7 de la stature : la tete, a echelle egale pour tous."""
    a = np.asarray(im)[..., 3] > 127
    top = body_top(a)
    px_per_cm = m["canvas"]["height"] / m["canvas"]["cm_per_canvas_height"]
    hh = int(28 * px_per_cm)
    row = a[top:top + hh]
    xs = np.nonzero(row.any(axis=0))[0]
    cx = int(xs.mean())
    return im.crop((cx - hh // 2, top - 6, cx + hh // 2, top + hh - 6))


def cmd_board(m):
    items = processed(m)
    if not items:
        print("BOARD::RIEN aucun PNG traite")
        return
    BOARDS.mkdir(parents=True, exist_ok=True)
    order = {c: i for i, c in enumerate(CATEGORY_ORDER)}
    items.sort(key=lambda t: (order[t[0]["category"]], t[0]["id"]))
    n = len(items)
    lineup(items, (196, 186, 166, 255), f"Population VILLAGER_PNG_001 -- {n} habitants, a l'echelle (fond terre)").save(BOARDS / "A_population_fond_terre.png")
    lineup(items, (245, 245, 245, 255), "Fond blanc : un halo sombre ou une frange se voient ici").save(BOARDS / "B_controle_fond_clair.png")
    lineup(items, (28, 30, 34, 255), "Fond sombre : un halo clair se voit ici").save(BOARDS / "C_controle_fond_sombre.png")
    lineup(items, (235, 232, 225, 255), "Silhouettes seules : des clones se ressembleraient ici", silhouette=True).save(BOARDS / "D_silhouettes.png")
    # Visages, par categorie, a la meme echelle.
    heads = [(p, head_crop(p, im, m)) for p, im in items]
    size = 160
    cols = 8
    rows = (len(heads) + cols - 1) // cols
    sheet = Image.new("RGB", (cols * size + 40, rows * (size + 26) + 60), (210, 204, 192))
    d = ImageDraw.Draw(sheet)
    d.text((20, 16), "Visages a meme echelle : chacun doit etre reconnaissable", fill=(30, 30, 30), font=font(22))
    for i, (p, h) in enumerate(heads):
        r, c = divmod(i, cols)
        h = h.resize((size, size), Image.LANCZOS)
        cell = Image.new("RGBA", (size, size), (210, 204, 192, 255))
        cell.alpha_composite(h)
        sheet.paste(cell.convert("RGB"), (20 + c * size, 50 + r * (size + 26)))
        d.text((24 + c * size, 52 + r * (size + 26) + size), p["id"].replace("CHR_", ""), fill=(40, 40, 40), font=font(13))
    sheet.save(BOARDS / "E_visages.png")
    print(f"BOARD::WROTE {n} habitants -> {BOARDS}")


# ----------------------------------------------------------------------------------------------- check

def norm_mask(im, w=48, h=128):
    a = np.asarray(im)[..., 3] > 127
    ys, xs = np.nonzero(a)
    crop = Image.fromarray((a[ys.min():ys.max() + 1, xs.min():xs.max() + 1] * 255).astype(np.uint8))
    return np.asarray(crop.resize((w, h), Image.BILINEAR)) > 127


def head_vec(p, im, m):
    g = head_crop(p, im, m).convert("RGBA").resize((40, 40), Image.LANCZOS)
    arr = np.asarray(g).astype(np.float64)
    lum = arr[..., :3] @ np.array([0.2126, 0.7152, 0.0722])
    lum = np.where(arr[..., 3] > 127, lum, lum[arr[..., 3] > 127].mean() if (arr[..., 3] > 127).any() else 0)
    v = (lum - lum.mean()).ravel()
    return v / (np.linalg.norm(v) + 1e-9)


def cmd_check(m):
    """Seuils : au-dela, la paire est signalee a l'oeil, pas declaree clone d'office."""
    items = processed(m)
    sil_max, face_max = 0.90, 0.92
    flagged = 0
    for cat in CATEGORY_ORDER:
        group = [(p, im) for p, im in items if p["category"] == cat]
        if len(group) < 2:
            continue
        masks = [norm_mask(im) for _, im in group]
        faces = [head_vec(p, im, m) for p, im in group]
        worst = (0.0, 0.0)
        for i in range(len(group)):
            for j in range(i + 1, len(group)):
                iou = (masks[i] & masks[j]).sum() / max(1, (masks[i] | masks[j]).sum())
                corr = float(faces[i] @ faces[j])
                worst = (max(worst[0], iou), max(worst[1], corr))
                if iou > sil_max or corr > face_max:
                    flagged += 1
                    print(f"CHECK::SIMILAIRE {group[i][0]['id']} ~ {group[j][0]['id']} silhouette_iou={iou:.3f} visage_corr={corr:.3f}")
        print(f"CHECK::CATEGORIE {cat} n={len(group)} silhouette_iou_max={worst[0]:.3f} visage_corr_max={worst[1]:.3f}")
    print(f"CHECK::{'PASS' if flagged == 0 else 'A_REVOIR'} paires_signalees={flagged} (seuils silhouette>{sil_max} visage>{face_max})")


if __name__ == "__main__":
    if len(sys.argv) < 2 or sys.argv[1] not in ("sheets", "prep", "board", "check"):
        print(__doc__)
        sys.exit(2)
    manifest = load_manifest()
    {"sheets": cmd_sheets, "board": cmd_board, "check": cmd_check}.get(sys.argv[1], lambda mm: cmd_prep(mm, sys.argv[2:] or None))(manifest)
