"""VILLAGER_PNG_001 -- chaine hors editeur des PNG d'habitants (Python systeme : Pillow + numpy).

Toute la population est decrite dans SourceArt/Characters/villager-population.json ; ce script n'invente rien.

    python tools/unreal/villager-png.py prompts   # -> docs/unreal/VILLAGER_PNG_001_PROMPTS.md
    python tools/unreal/villager-png.py prep      # SourceArt/Characters/Raw/<id>.png -> SourceArt/Characters/PNG/<Categorie>/<id>.png
    python tools/unreal/villager-png.py board     # planches de comparaison -> docs/visual/villager-png-001/
    python tools/unreal/villager-png.py check     # ressemblance silhouette / visage, par paire, par categorie

prep : alpha natif s'il existe, sinon detourage d'un fond uni (modele de fond quadratique ajuste sur
les bords, propagation depuis les bords, trous fermes captes s'ils ont la couleur exacte du fond).
Puis decontamination des bords (plus de halo clair), recadrage, mise a l'echelle de la STATURE du
manifeste (5,12 px/cm), pieds centres sur une meme ligne, saignement de couleur sous l'alpha nul
(les mips d'Unreal ne ramenent pas de liseré sombre). Ne modifie jamais Raw/.
"""

import json
import sys
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[2]
MANIFEST = ROOT / "SourceArt" / "Characters" / "villager-population.json"
RAW = ROOT / "SourceArt" / "Characters" / "Raw"
OUT = ROOT / "SourceArt" / "Characters" / "PNG"
BOARDS = ROOT / "docs" / "visual" / "villager-png-001"
PROMPTS = ROOT / "docs" / "unreal" / "VILLAGER_PNG_001_PROMPTS.md"

CATEGORY_ORDER = ["Adult_Male", "Adult_Female", "Elder_Male", "Elder_Female", "Child_Male", "Child_Female"]
CATEGORY_FR = {
    "Adult_Male": "Hommes adultes", "Adult_Female": "Femmes adultes",
    "Elder_Male": "Hommes ages", "Elder_Female": "Femmes agees",
    "Child_Male": "Garcons", "Child_Female": "Filles",
}

STYLE = (
    "Exactly one person, full body, alone. An ordinary villager of a Greek Byzantine village on the "
    "Pontic coast of the Black Sea, shortly after 1204. Realistic, sober, hand-painted 3D game character "
    "render with matte textures: same rendering style, lighting and level of detail as the attached "
    "reference sheet, but a completely DIFFERENT person -- do not reuse the face, body or hair of the "
    "reference or of any image generated before. Standing still in a relaxed natural pose, arms hanging "
    "at the sides, hands empty unless stated, body and head turned three-quarters toward the LEFT side "
    "of the image. Camera at chest height, no perspective distortion. The whole figure is visible from "
    "the top of the head to the soles of the feet, centred, filling about 90% of the image height. Soft "
    "daylight from the upper left. TRANSPARENT background (PNG with alpha); if transparency is "
    "impossible, a plain flat uniform pure green chroma-key background (#00FF00) with no green light "
    "spilling on the person. No floor, no ground, no cast "
    "shadow, no text, no frame, no other object. Portrait format 1024x1536. Muted earthy natural-dye "
    "palette (undyed wool and linen, ochre, faded madder red, faded woad blue, olive, brown leather); "
    "clothes worn, mended, dusty. Not a hero and not fashion-beautiful: a real person who has lived. "
    "No armour, no weapon, no jewellery, no fantasy, no anime, no cartoon, no chibi, no oversized head, "
    "no saturated colours, nothing modern."
)


def load_manifest():
    with MANIFEST.open(encoding="utf-8") as f:
        return json.load(f)


def prompt_for(person):
    return f"Full-body image of {person['person']}\n\n{STYLE}"


# --------------------------------------------------------------------------------------------- prompts

def cmd_prompts(m):
    lines = [
        "# VILLAGER_PNG_001 -- fiche de generation des 32 habitants",
        "",
        "Generee par `python tools/unreal/villager-png.py prompts` depuis `SourceArt/Characters/villager-population.json` :",
        "ne pas editer a la main, changer le manifeste et regenerer.",
        "",
        "## Mode d'emploi",
        "",
        "1. **Une conversation ChatGPT neuve par categorie** (six au total) : dans une meme conversation, le",
        "   generateur a tendance a reprendre le visage de l'image precedente.",
        "2. Joindre **une** planche comme reference de style, sans plus :",
        "   - hommes et garcons : `C:\\dev\\Jeux IV Kingdoms\\assets\\references\\npc\\planche-homme-modulaire-rts.png`",
        "   - femmes et filles : `C:\\dev\\Jeux IV Kingdoms\\assets\\references\\npc\\planche-femme-modulaire-rts.png`",
        "3. **Un prompt = une image = une personne.** Coller le bloc tel quel.",
        "4. Avant d'enregistrer, comparer avec les precedents de la categorie. Si le visage ou la silhouette",
        "   rappelle un autre habitant, ou la planche : repondre",
        "   *\"Regenerate: completely different face and body, keep only the style.\"*",
        "5. Verifier : corps entier (tete et pieds visibles), personne seule, pas de sol ni d'ombre portee.",
        "6. Enregistrer sous le **nom exact** de l'en-tete (`CHR_M_Adult_001.png`...) dans",
        "   `C:\\dev\\ANASTASIS_WORKTREES\\villager-png-001\\SourceArt\\Characters\\Raw\\`.",
        "   Fond transparent, ou a defaut vert d'incrustation uni : les deux sont acceptes, le detourage est",
        "   fait ensuite. Eviter un fond gris ou beige : il a la couleur du lin ecru.",
        "7. Un depot partiel suffit pour commencer : chaque image deposee est traitee et verifiee.",
        "",
        "Statures en jeu (le PNG est remis a cette taille, les pieds sur une meme ligne) :",
        "",
        "| Id | Age | Stature |",
        "|---|---|---|",
    ]
    for p in m["people"]:
        lines.append(f"| `{p['id']}` | {p['age']} | {p['stature_cm']} cm |")
    for cat in CATEGORY_ORDER:
        lines += ["", f"## {CATEGORY_FR[cat]} (`{cat}`)", ""]
        for p in (p for p in m["people"] if p["category"] == cat):
            lines += [f"### {p['id']}.png", "", "```text", prompt_for(p), "```", ""]
    PROMPTS.write_text("\n".join(lines), encoding="utf-8")
    print(f"PROMPTS::WROTE {PROMPTS} ({len(m['people'])} prompts)")


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
    subject = ~background
    # Lisiere : un pixel de bord est un MELANGE I = a*F + (1-a)*B. Un seuil absolu sur |I-B| le
    # declare opaque des que le sujet est contraste, et le liseré clair reste. On estime F par la
    # couleur du sujet la plus proche (interieur etendu vers l'exterieur), puis a par projection
    # de I-B sur F-B.
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
    return alpha, bg, (t_lo, t_hi, noise, chroma, holes_left)


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
    s = target_h / (y1 - y0)

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
    ys = np.nonzero(a.any(axis=1))[0]
    top = ys.min()
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
    if len(sys.argv) < 2 or sys.argv[1] not in ("prompts", "prep", "board", "check"):
        print(__doc__)
        sys.exit(2)
    manifest = load_manifest()
    {"prompts": cmd_prompts, "board": cmd_board, "check": cmd_check}.get(sys.argv[1], lambda mm: cmd_prep(mm, sys.argv[2:] or None))(manifest)
