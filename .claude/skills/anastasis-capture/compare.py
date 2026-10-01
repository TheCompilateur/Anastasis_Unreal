"""Compare deux captures ANASTASIS : ecart chiffre et carte de l'ecart.

    python compare.py A.png B.png [--heatmap diff.png] [--seuil 16]

Reference de variance (2026-09-30, meme etat, deux runs de capture-slice) : ~3,6 % des
pixels au-dela de 16/255. Un effet ne se revendique qu'au-dela de cette variance.
"""
import argparse
import sys

from PIL import Image, ImageChops, ImageStat


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("a")
    ap.add_argument("b")
    ap.add_argument("--heatmap", help="ecrit la carte de l'ecart, amplifiee x8")
    ap.add_argument("--seuil", type=int, default=16, help="ecart par canal (0-255) qui compte")
    args = ap.parse_args()

    a = Image.open(args.a).convert("RGB")
    b = Image.open(args.b).convert("RGB")
    if a.size != b.size:
        print(f"TAILLES_DIFFERENTES {a.size} {b.size} : comparaison impossible")
        return 2

    diff = ImageChops.difference(a, b)
    total = a.size[0] * a.size[1]
    # Max des trois canaux, puis seuil : un pixel compte s'il s'ecarte sur au moins un canal.
    r, g, bl = diff.split()
    worst = ImageChops.lighter(ImageChops.lighter(r, g), bl)
    over = sum(worst.point(lambda v: 255 if v > args.seuil else 0).histogram()[255:])

    print(f"taille            {a.size[0]}x{a.size[1]}")
    print(f"ecart moyen RGB   {[round(v, 2) for v in ImageStat.Stat(diff).mean]}")
    print(f"pixels > {args.seuil:<3}     {over} / {total} = {100 * over / total:.2f} %")
    print(f"luminosite A      {[round(v, 1) for v in ImageStat.Stat(a).mean]}")
    print(f"luminosite B      {[round(v, 1) for v in ImageStat.Stat(b).mean]}")
    print("reference         ~3,6 % entre deux runs du meme etat (variance de capture)")

    if args.heatmap:
        worst.point(lambda v: min(255, v * 8)).save(args.heatmap)
        print(f"carte             {args.heatmap}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
