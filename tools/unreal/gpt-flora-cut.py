"""GPT_FLORA_001 -- decoupe la planche GPT (treize sujets sur fond gris) en treize PNG detoures.

**Python systeme, hors Unreal** (Pillow + numpy). Entree : `SourceArt/Vegetation/gpt/planche-01.webp`, une
image generee par GPT : treize sujets de vegetation poses sur un fond gris uniforme (5 + 4 + 4). Sortie :
`SourceArt/Vegetation/gpt/PNG/<espece>.png`, RGBA, recadre au pixel, couleur decontaminee du gris.

METHODE. Distance a la couleur du fond (gris neutre, ~117) ou chroma du pixel -> alpha lisse ; composantes
connexes sur un masque au quart de resolution (treize attendues, sinon le script refuse) ; ordre : trois
rangees, de gauche a droite ; couleur = (pixel - (1 - alpha) * fond) / alpha, ce qui retire le halo gris des
bords de feuilles. Les noms sont ceux de la planche telle que posee par Alexandre le 2026-10-09 : changer la
planche change `NAMES`.

Lancer : python tools/unreal/gpt-flora-cut.py [planche.webp]
"""
import os
import sys

import numpy as np
from PIL import Image, ImageFilter

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, '..', '..'))
SRC = os.path.join(ROOT, 'SourceArt', 'Vegetation', 'gpt')
NAMES = ['marronnier_fleuri', 'bouleau', 'chene', 'pin_sombre', 'pin_sylvestre',
         'cypres', 'genevrier', 'noisetier', 'saule_pleureur',
         'arbuste_baies', 'rhododendron', 'fougere', 'prairie_fleurie']
BG = np.array([117., 115., 114.])
S = 4                    # sous-echantillonnage du masque de composantes


def main():
    sheet = sys.argv[1] if len(sys.argv) > 1 else os.path.join(SRC, 'planche-01.webp')
    out = os.path.join(SRC, 'PNG')
    os.makedirs(out, exist_ok=True)
    a = np.asarray(Image.open(sheet).convert('RGB')).astype(np.float32)
    H, W, _ = a.shape
    f = np.maximum(np.abs(a - BG).max(2), (a.max(2) - a.min(2)) * 1.3)
    alpha = np.clip((f - 16.0) / 24.0, 0, 1)
    alpha = alpha * alpha * (3 - 2 * alpha)
    h, w = H // S, W // S
    m = (alpha[:h * S, :w * S] > 0.5).reshape(h, S, w, S).max(axis=(1, 3))
    lab = np.where(m, np.arange(1, h * w + 1).reshape(h, w), 0)
    for _ in range(2000):                                    # propagation du plus grand identifiant voisin
        p = np.pad(lab, 1)
        nb = np.max([p[dy:dy + h, dx:dx + w] for dy in range(3) for dx in range(3)], axis=0)
        new = np.where(m, nb, 0)
        if (new == lab).all():
            break
        lab = new
    ids, counts = np.unique(lab[lab > 0], return_counts=True)
    big = [(int(i), int(c)) for i, c in zip(ids, counts) if c * S * S > 4000]
    if len(big) != len(NAMES):
        raise SystemExit('GPT_FLORA_CUT::FAIL %d sujets trouves, %d attendus' % (len(big), len(NAMES)))
    boxes = []
    for i, c in big:
        ys, xs = np.where(lab == i)
        boxes.append((int(xs.min() * S), int(ys.min() * S), int((xs.max() + 1) * S), int((ys.max() + 1) * S), i))
    rows = [[], [], []]
    for b in boxes:
        rows[0 if b[3] < 420 else (1 if b[3] < 800 else 2)].append(b)
    order = [b for r in rows for b in sorted(r, key=lambda b: b[0])]
    for name, b in zip(NAMES, order):
        reg = Image.fromarray(((lab == b[4]) * 255).astype('uint8')).resize((w * S, h * S), Image.NEAREST)
        reg = np.asarray(reg.filter(ImageFilter.MaxFilter(25))).astype(np.float32) / 255
        rg = np.zeros((H, W), np.float32)
        rg[:reg.shape[0], :reg.shape[1]] = reg
        al = alpha * rg
        ac = np.clip(al, 1e-3, 1)[..., None]
        col = np.clip((a - (1 - ac) * BG) / ac, 0, 255)
        col = np.where(al[..., None] > 0.02, col, a)
        ys, xs = np.where(al > 0.5)
        x0, x1 = max(xs.min() - 4, 0), min(xs.max() + 5, W)
        y0, y1 = max(ys.min() - 4, 0), min(ys.max() + 5, H)
        rgba = np.dstack([col, al * 255])[y0:y1, x0:x1].astype('uint8')
        Image.fromarray(rgba, 'RGBA').save(os.path.join(out, name + '.png'))
        print('%-18s %dx%d' % (name, x1 - x0, y1 - y0))
    print('GPT_FLORA_CUT::PASS %d sujets' % len(NAMES))


if __name__ == '__main__':
    main()
