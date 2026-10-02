"""TSR_FLICKER_001 -- mesure du scintillement, hors editeur (Python systeme, Pillow + numpy).

    python tools/unreal/tsr-flicker-metrics.py Saved/TsrFlickerEvidence/<Label>

Lit les sequences <vue>_<etat>/fNN.png de tsr-flicker-pie.py. Par sequence, sur la luminance
(Rec. 709, 0-255) :

  motion    moyenne de |L(t+1) - L(t)| : tout ce qui change d'une image a la suivante,
            vent compris.
  flicker   moyenne de |L(t+1) - 2 L(t) + L(t-1)| : la derivee seconde temporelle. Un
            mouvement regulier (le vent qui balance une branche) s'y annule presque ; un
            pixel qui clignote d'une image a l'autre y ressort entier.
  blink     part des pixels dont la mediane temporelle de |derivee seconde| depasse 4/255 :
            les pixels qui scintillent la plupart du temps, pas un a-coup isole.
  *_detail  les memes, sur le quart de l'image au plus fort gradient spatial (l'image
            moyenne) : herbe, aiguilles, branches -- la ou vit la geometrie fine.

Une carte du flicker (moyenne de |derivee seconde|, x8) par sequence : flicker_<seq>.png.

Verdict par vue : temoin = |d0 - d0b| (meme etat, avant et apres) ; l'effet de d1 n'est
revendique que si |d1 - moyenne(d0, d0b)| depasse 2 x temoin ET 0,25 niveau (un temoin
nul, temps gele, ne rend pas visible un ecart infime). Ecrit metrics.json.
"""
import json
import sys
from pathlib import Path

import numpy as np
from PIL import Image

BLINK = 4.0
# Un temoin nul (temps gele : l'image est deterministe) rendrait revendicable un ecart infime.
# En dessous d'un quart de niveau sur 255, l'ecart ne se voit pas : il ne se revendique pas.
MIN_EFFECT = 0.25


def luminance(path):
    a = np.asarray(Image.open(path).convert('RGB'), dtype=np.float32)
    return 0.2126 * a[..., 0] + 0.7152 * a[..., 1] + 0.0722 * a[..., 2]


def measure(seq_dir):
    files = sorted(seq_dir.glob('f*.png'))
    if len(files) < 3:
        return None
    frames = np.stack([luminance(f) for f in files])
    d1 = np.abs(np.diff(frames, axis=0))
    d2 = np.abs(frames[2:] - 2.0 * frames[1:-1] + frames[:-2])
    mean = frames.mean(axis=0)
    gy, gx = np.gradient(mean)
    grad = np.hypot(gx, gy)
    detail = grad >= np.percentile(grad, 75)
    d2_mean = d2.mean(axis=0)
    blink = np.median(d2, axis=0) > BLINK
    heat = np.clip(d2_mean * 8.0, 0, 255).astype(np.uint8)
    Image.fromarray(heat).save(seq_dir.parent / ('flicker_%s.png' % seq_dir.name))
    return {
        'frames': len(files),
        'motion': float(d1.mean()),
        'flicker': float(d2_mean.mean()),
        'blink': float(blink.mean()),
        'flicker_detail': float(d2_mean[detail].mean()),
        'blink_detail': float(blink[detail].mean()),
        'luma': float(mean.mean()),
    }


def main():
    if len(sys.argv) != 2:
        print(__doc__)
        return 2
    root = Path(sys.argv[1])
    meta = json.loads((root / 'tsr-flicker.json').read_text(encoding='utf-8')) if (root / 'tsr-flicker.json').exists() else {}
    seqs = {}
    for d in sorted(p for p in root.iterdir() if p.is_dir()):
        m = measure(d)
        if m:
            info = meta.get('sequences', {}).get(d.name, {})
            m['view'] = info.get('view', d.name.rsplit('_', 1)[0])
            m['state'] = info.get('state', d.name.rsplit('_', 1)[-1])
            m['gap_s'] = info.get('gap_s', {}).get('mean')
            seqs[d.name] = m
            print('%-24s frames=%2d motion=%6.2f flicker=%6.2f blink=%6.3f flicker_detail=%6.2f blink_detail=%6.3f luma=%6.1f gap=%s'
                  % (d.name, m['frames'], m['motion'], m['flicker'], m['blink'], m['flicker_detail'], m['blink_detail'],
                     m['luma'], ('%.3fs' % m['gap_s']) if m['gap_s'] else '?'))
    verdicts = {}
    for view in sorted({m['view'] for m in seqs.values()}):
        by = {m['state']: m for m in seqs.values() if m['view'] == view}
        if not all(k in by for k in ('d0', 'd1', 'd0b')):
            continue
        v = {}
        for key in ('flicker', 'blink', 'flicker_detail', 'blink_detail'):
            base = (by['d0'][key] + by['d0b'][key]) / 2.0
            witness = abs(by['d0'][key] - by['d0b'][key])
            effect = by['d1'][key] - base
            v[key] = {'d0': by['d0'][key], 'd1': by['d1'][key], 'd0b': by['d0b'][key], 'effect': effect,
                      'effect_pct': 100.0 * effect / base if base else 0.0, 'witness': witness,
                      'claimable': abs(effect) > max(2.0 * witness, MIN_EFFECT)}
        verdicts[view] = v
        f = v['flicker_detail']
        print('TSR_VERDICT %-12s flicker_detail d0=%.2f d1=%.2f d0b=%.2f effet=%+.2f (%+.1f %%) temoin=%.2f %s'
              % (view, f['d0'], f['d1'], f['d0b'], f['effect'], f['effect_pct'], f['witness'],
                 'REVENDICABLE' if f['claimable'] else 'DANS_LE_BRUIT'))
    (root / 'metrics.json').write_text(json.dumps({'sequences': seqs, 'verdicts': verdicts}, indent=1), encoding='utf-8')
    print('TSR_METRICS sequences=%d views=%d dir=%s' % (len(seqs), len(verdicts), root))
    return 0


if __name__ == '__main__':
    sys.exit(main())
