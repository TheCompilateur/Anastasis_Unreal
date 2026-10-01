"""Mesures d'atmosphere sur des captures de ciel -- HORS EDITEUR (Python systeme, Pillow + numpy).

ATMOSPHERE_COHERENCE_001. Regarder les images reste obligatoire ; ce script dit OU regarder et
rend deux passages comparables. Il lit un dossier de captures (capture-sky.ps1, en particulier
-Preset cycle) et ecrit, dans ce meme dossier :

  metrics.json            une entree par image
  metrics.txt             le tableau, une ligne par image, drapeaux en fin de ligne
  contact_<vue>.png       planche par vue : une ligne par heure, une colonne par humidite

Mesures (luma Rec.709 sur l'image 8 bits telle qu'affichee) :
  p50, p99            mediane et 99e centile
  white               part des pixels a luma > 235 (surexposition, brouillard "nucleaire")
  wall                part des deux tiers BAS de l'image (la ou doit etre le sol ; le ciel clair
                      est lisse et clair, legitimement) en blocs 16x16 clairs (moyenne > 150),
                      sans detail (ecart-type < 3) et peu colores (max - min RGB < 40 : le
                      sable lisse au soleil n'est pas du brouillard) : un mur, pas une profondeur
  depth               contraste local (ecart-type moyen des blocs) dans le tiers bas, le tiers
                      median et le tiers haut de l'image ; une scene qui s'enfonce dans la brume
                      les voit decroitre, un mur les voit tous nuls
Drapeaux : WALL (wall > 0.35 et p50 > 150), HAZE (tiers median sans relief, depth < 4, dans une
image qui n'est pas nocturne, p50 > 80 : un voile uniforme), CLIPPED (white > 0.20), BLACK
(p99 < 12). Seuils empiriques, poses sur les captures de day-night-weather-001 (WALL : l'aube de
la vue haute ; HAZE : la vallee a 18 h 30 et l'aube ; rien a 10 h ni a 23 h) et a recaler sur les
premieres planches du cycle. Ils signalent, ils ne jugent pas.

Usage : python tools/unreal/atmosphere-metrics.py <dossier> [<dossier> ...]
Les noms attendus sont <vue>_<etat>.png ; un etat "h06_dry" est range a l'heure 06, colonne dry.
"""
import json, os, re, sys

import numpy as np
from PIL import Image

BLOCK = 16
VIEWS = ('ov_sw', 'valley_long', 'ridge_long', 'sun_ridge')
HOUR_ORDER = ['06', '09', '12', '16', '18', '20', '00', '03']


def load(path):
    rgb = np.asarray(Image.open(path).convert('RGB')).astype(np.float64)
    return rgb, 0.2126 * rgb[..., 0] + 0.7152 * rgb[..., 1] + 0.0722 * rgb[..., 2]


def blocks(img):
    h, w = (img.shape[0] // BLOCK) * BLOCK, (img.shape[1] // BLOCK) * BLOCK
    b = img[:h, :w].reshape((h // BLOCK, BLOCK, w // BLOCK, BLOCK) + img.shape[2:])
    return b.mean(axis=(1, 3)), b.std(axis=(1, 3))


def measure(path):
    rgb, lum = load(path)
    mean, std = blocks(lum)
    chroma = np.ptp(blocks(rgb)[0], axis=-1)
    rows = std.shape[0]
    low = slice(rows // 3, rows)
    thirds = [std[(2 * rows) // 3:], std[rows // 3:(2 * rows) // 3], std[:rows // 3]]
    m = {
        'p50': round(float(np.percentile(lum, 50)), 1),
        'p99': round(float(np.percentile(lum, 99)), 1),
        'white': round(float((lum > 235).mean()), 3),
        'wall': round(float(((mean[low] > 150) & (std[low] < 3) & (chroma[low] < 40)).mean()), 3),
        'depth_low_mid_high': [round(float(t.mean()), 2) for t in thirds],
    }
    flags = []
    if m['wall'] > 0.35 and m['p50'] > 150:
        flags.append('WALL')
    if m['depth_low_mid_high'][1] < 4 and m['p50'] > 80:
        flags.append('HAZE')
    if m['white'] > 0.20:
        flags.append('CLIPPED')
    if m['p99'] < 12:
        flags.append('BLACK')
    m['flags'] = flags
    return m


def split_name(name):
    stem = os.path.splitext(name)[0]
    for v in VIEWS:
        if stem.startswith(v + '_'):
            return v, stem[len(v) + 1:]
    return None, stem


def contact(folder, view, entries):
    """Une ligne par heure, une colonne par variante (dry, humid, sat...), vignettes 400x225."""
    cells = {}
    cols = []
    for state, path in entries:
        m = re.match(r'h(\d\d)_(.+)$', state)
        hour, col = (m.group(1), m.group(2)) if m else (state, '')
        cells[(hour, col)] = path
        if col not in cols:
            cols.append(col)
    hours = [h for h in HOUR_ORDER if any(k[0] == h for k in cells)]
    hours += sorted({k[0] for k in cells} - set(hours))
    tw, th = 400, 225
    sheet = Image.new('RGB', (tw * len(cols), th * len(hours)), (0, 0, 0))
    for r, hour in enumerate(hours):
        for c, col in enumerate(cols):
            if (hour, col) in cells:
                sheet.paste(Image.open(cells[(hour, col)]).convert('RGB').resize((tw, th)), (c * tw, r * th))
    out = os.path.join(folder, 'contact_%s.png' % view)
    sheet.save(out)
    return out, hours, cols


def run(folder):
    pngs = sorted(f for f in os.listdir(folder) if f.lower().endswith('.png') and not f.startswith('contact_'))
    report, lines, by_view = {}, [], {}
    for f in pngs:
        view, state = split_name(f)
        m = measure(os.path.join(folder, f))
        report[f] = dict(m, view=view, state=state)
        lines.append('%-34s p50=%5.1f p99=%5.1f white=%.3f wall=%.3f depth=%s %s' % (
            f, m['p50'], m['p99'], m['white'], m['wall'],
            '/'.join('%.1f' % d for d in m['depth_low_mid_high']), ' '.join(m['flags'])))
        if view:
            by_view.setdefault(view, []).append((state, os.path.join(folder, f)))
    with open(os.path.join(folder, 'metrics.json'), 'w') as fh:
        json.dump(report, fh, indent=1)
    with open(os.path.join(folder, 'metrics.txt'), 'w') as fh:
        fh.write('\n'.join(lines) + '\n')
    for view, entries in by_view.items():
        out, hours, cols = contact(folder, view, entries)
        lines.append('CONTACT %s rows=%s cols=%s' % (out, ','.join(hours), ','.join(cols)))
    flagged = sum(1 for m in report.values() if m['flags'])
    print('\n'.join(lines))
    print('ATMOSPHERE_METRICS images=%d flagged=%d dir=%s' % (len(report), flagged, folder))


if __name__ == '__main__':
    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(2)
    for d in sys.argv[1:]:
        run(d)
