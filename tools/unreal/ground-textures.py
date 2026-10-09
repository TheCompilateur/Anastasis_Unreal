"""Textures photo du sol d'ANASTASIS : telechargement CC0 et empaquetage.

SE LANCE HORS UNREAL, avec le Python du systeme (Pillow + numpy) :

    python tools/unreal/ground-textures.py
    python tools/unreal/ground-textures.py Ruin  # seulement la nouvelle variante
    python tools/unreal/ground-textures.py Path Gravel

Il prepare les images que tools/unreal/ground-material.py importe ensuite dans
/Game/Anastasis/Materials/GroundTextures. Rien n'est ecrit dans Content/ ici.

SOURCES. Quatre textures de base et une variante locale Ruin de Poly Haven
(https://polyhaven.com), licence CC0, en 2K. Choisies contre la direction
artistique P1.6 (ecologie pontique humide, "Mediterranean dryness" interdite) :

  Grass   sparse_grass       2.0 m   herbe clairsemee, sol, racines
  Litter  forest_leaves_02   3.0 m   feuilles, mousse, brindilles
  Worked  brown_mud_02       1.3 m   terre compacte humide
  Rock    mossy_rock         3.0 m   roche moussue, lichen
  Ruin    cobblestone_floor_13 2.0 m  pierres usees en terre, sites remanies
  Path    stony_dirt_path   2.2 m  terre tassee et cailloux du passage
  Gravel  river_small_rocks 2.9 m  galets de rive a courant rapide

Les fichiers bruts vont dans Saved/GroundTextures/raw (ignore par git), les images
empaquetees dans Saved/GroundTextures/packed. Les assets importes, eux, sont versionnes :
ce sont eux la verite du projet, pas ces fichiers.

EMPAQUETAGE, deux images par famille :

  T_Ground_<F>_AH  RGB = albedo de DETAIL, A = hauteur        (sRGB)
  T_Ground_<F>_NR  RG  = normale DirectX XY, B = rugosite de detail, A = occlusion de detail
                   (lineaire)

L'albedo n'est PAS la couleur de la photo. GROUND_SURFACE_001 a recale les albedos du
sol sur des valeurs physiques, portees par la couleur de sommet et les teintes de famille
du materiau. Une photo posee telle quelle effacerait ce recalage et la semantique de la
simulation. On stocke donc la photo DIVISEE PAR SA MOYENNE LOCALE (passe-haut a
HIGHPASS_CM, voir pack), en lineaire, la valeur entiere et la chromie relative attenuee a
CHROMA : une modulation neutre en moyenne, qui ajoute le detail sous le metre sans deplacer la
teinte calee. Moyenne ramenee a DETAIL_MEAN (0.4) pour garder 2.5x de marge avant
ecretage ; le materiau multiplie par 1/DETAIL_MEAN. Au loin, les mips convergent vers la
moyenne : le detail s'eteint de lui-meme, sans fondu.

La rugosite suit la meme logique : 0.5 + (r - moyenne), un ecart centre que le materiau
ajoute a ses rugosites calees. L'occlusion aussi : ao / moyenne * 0.5, neutre a 0.5.
"""
import hashlib
import json
import os
import sys
import urllib.request

import numpy as np
from PIL import Image

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
OUT = os.path.join(ROOT, 'Saved', 'GroundTextures')
RAW = os.path.join(OUT, 'raw')
PACKED = os.path.join(OUT, 'packed')

BASE = 'https://dl.polyhaven.org/file/ph-assets/Textures'
DETAIL_MEAN = 0.4
CHROMA = 0.5
# Echelle de coupure du passe-haut, en cm reels. Voir lowpass().
HIGHPASS_CM = 15.0
LUMA = np.array([0.2126, 0.7152, 0.0722])

# famille -> (asset Poly Haven, taille reelle en cm, nom du fichier de couleur)
# Le fichier de couleur ne suit pas une seule convention chez Poly Haven
# (_diff_ / _diffuse_) : on le fige ici plutot que de le deviner.
FAMILIES = {
    'Grass': ('sparse_grass', 200.0, 'diff'),
    'Litter': ('forest_leaves_02', 300.0, 'diffuse'),
    'Worked': ('brown_mud_02', 130.0, 'diff'),
    'Rock': ('mossy_rock', 300.0, 'diff'),
    'Ruin': ('cobblestone_floor_13', 200.0, 'diff'),
    'Path': ('stony_dirt_path', 220.0, 'diff'),
    'Gravel': ('river_small_rocks', 290.0, 'diff'),
}
MAPS = (('color', 'jpg'), ('nor_dx', 'png'), ('rough', 'jpg'), ('disp', 'png'), ('ao', 'jpg'))


def url_for(asset, kind, ext, color_name):
    name = color_name if kind == 'color' else kind
    return '%s/%s/2k/%s/%s_%s_2k.%s' % (BASE, ext, asset, asset, name, ext)


def fetch(url, dst):
    if os.path.exists(dst) and os.path.getsize(dst) > 0:
        return
    tmp = dst + '.part'
    req = urllib.request.Request(url, headers={'User-Agent': 'anastasis-ground-textures'})
    with urllib.request.urlopen(req, timeout=120) as r, open(tmp, 'wb') as f:
        while True:
            chunk = r.read(1 << 20)
            if not chunk:
                break
            f.write(chunk)
    os.replace(tmp, dst)


def load01(path, channels):
    """Image en float [0,1], 8 ou 16 bits. channels=1 -> 2D, 3 -> HxWx3."""
    im = Image.open(path)
    a = np.asarray(im)
    if a.dtype == np.uint16 or im.mode.startswith('I;16') or im.mode == 'I':
        a = a.astype(np.float64) / 65535.0
    else:
        a = a.astype(np.float64) / 255.0
    if channels == 1:
        if a.ndim == 3:
            a = a[..., 0]
        return a
    if a.ndim == 2:
        a = np.stack([a] * 3, axis=-1)
    return a[..., :3]


def srgb_to_lin(c):
    return np.where(c <= 0.04045, c / 12.92, ((c + 0.055) / 1.055) ** 2.4)


def lin_to_srgb(c):
    c = np.clip(c, 0.0, 1.0)
    return np.where(c <= 0.0031308, c * 12.92, 1.055 * np.power(c, 1.0 / 2.4) - 0.055)


def to8(a):
    return np.clip(np.round(a * 255.0), 0, 255).astype(np.uint8)


def sha(path):
    h = hashlib.sha256()
    with open(path, 'rb') as f:
        for chunk in iter(lambda: f.read(1 << 20), b''):
            h.update(chunk)
    return h.hexdigest()


def lowpass(a, sigma_px):
    """Flou gaussien PERIODIQUE (par FFT) : la photo est une tuile, son bord droit
    continue son bord gauche. Un flou a bord ferme ferait apparaitre un cadre."""
    fy = np.fft.fftfreq(a.shape[0])[:, None]
    fx = np.fft.fftfreq(a.shape[1])[None, :]
    g = np.exp(-2.0 * (np.pi * sigma_px) ** 2 * (fx * fx + fy * fy))
    if a.ndim == 2:
        return np.real(np.fft.ifft2(np.fft.fft2(a) * g))
    return np.stack([lowpass(a[..., c], sigma_px) for c in range(a.shape[2])], axis=-1)


def pack(family, asset, size_cm, files):
    # PASSE-HAUT. Une photo de 2 m porte aussi ses taches a l'echelle de la tuile -- une
    # zone plus seche ici, une ombre la. Repetee tous les 2 m, cette basse frequence se
    # lit en DAMIER des 40 m (premiere capture, vue oblique : le "tile checkerboard" que
    # la direction artistique interdit nommement). On ne garde donc que le detail sous
    # HIGHPASS_CM, relatif a sa moyenne LOCALE ; la variation large reste portee par le
    # bruit macro/meso du materiau, qui ne se repete pas a l'echelle du monde.
    # Les joints et galets de la ruine portent une forme plus large que les
    # grains naturels. Leur passe-haut reste local au site, jamais a la vallee.
    cutoff_cm = 35.0 if family == 'Ruin' else 30.0 if family in ('Path', 'Gravel') else HIGHPASS_CM
    sigma = cutoff_cm / size_cm * 2048.0 / 2.0
    color = srgb_to_lin(load01(files['color'], 3))
    mean = color.reshape(-1, 3).mean(axis=0)
    local = np.maximum(lowpass(color, sigma), 1e-4)
    # Valeur et chromie separees. Diviser chaque canal par sa propre moyenne amplifie le
    # bruit d'un canal presque vide : le bleu de sparse_grass vaut 0.009 en moyenne, et
    # le premier essai ecretait 6 % des pixels de l'herbe sur ce seul canal. La valeur
    # (luminance relative) passe donc entiere, la chromie relative seulement a CHROMA.
    lum = color @ LUMA
    value = lum / np.maximum(local @ LUMA, 1e-4)
    chroma = (color / local) / np.maximum(value, 1e-4)[..., None]
    # Le chemin contient des eclats de calcaire presque blancs. Leur ratio local
    # depassait 1 sur 11 % des pixels apres calibrage : ecretage en jeu et
    # scintillement en vue oblique. Comprimer leur seul contraste haute frequence.
    if family == 'Path':
        value = np.power(value, 0.45)
    detail = value[..., None] * np.power(np.maximum(chroma, 1e-4), CHROMA) * DETAIL_MEAN
    clipped = float((detail > 1.0).any(axis=-1).mean())

    # Hauteur : etiree sur ses percentiles 1-99, pour que le melange par hauteur lise la
    # meme plage d'une famille a l'autre. Sans cela une texture au relief plat (la terre)
    # perdrait toujours contre une texture au relief marque (la roche), quel que soit
    # son poids.
    h = load01(files['disp'], 1)
    lo, hi = np.percentile(h, 1), np.percentile(h, 99)
    h = np.clip((h - lo) / max(hi - lo, 1e-6), 0.0, 1.0)

    ah = np.dstack([to8(lin_to_srgb(detail)), to8(h)])
    # La normale aussi : une grande pente de la photo, repetee, dessine le meme damier en
    # lumiere rasante. On retire sa composante lente et on renormalise.
    nor = load01(files['nor_dx'], 3) * 2.0 - 1.0
    nxy = nor[..., :2] - lowpass(nor[..., :2], sigma)
    nz = np.sqrt(np.clip(1.0 - (nxy ** 2).sum(-1), 0.0, 1.0))
    nor = np.dstack([nxy, nz]) / np.linalg.norm(np.dstack([nxy, nz]), axis=-1, keepdims=True)
    nor = nor * 0.5 + 0.5
    rough = load01(files['rough'], 1)
    rmean = float(rough.mean())
    # Occlusion : meme logique encore, ramenee a une moyenne de 0.5. Brute, elle
    # assombrirait toute la famille de sa moyenne (0.60 pour la litiere) et deplacerait
    # l'albedo cale ; centree, elle ne creuse que les creux.
    # cobblestone_floor_13 ne fournit pas d'AO separee. Son ARM a un canal
    # d'occlusion vide (moyenne ~0.005) : une occlusion neutre est plus honnete.
    ao = load01(files['ao'], 1) if 'ao' in files else np.ones_like(rough)
    ao_mean = float(ao.mean())
    nr = np.dstack([to8(nor[..., 0]), to8(nor[..., 1]),
                    to8(np.clip(0.5 + (rough - lowpass(rough, sigma)), 0.0, 1.0)),
                    to8(np.clip(ao / np.maximum(lowpass(ao, sigma), 1e-3) * 0.5, 0.0, 1.0))])

    paths = {}
    for suffix, arr in (('AH', ah), ('NR', nr)):
        p = os.path.join(PACKED, 'T_Ground_%s_%s.png' % (family, suffix))
        Image.fromarray(arr, 'RGBA').save(p, optimize=False)
        paths[suffix] = p
    info = {
        'asset': asset,
        'albedo_mean_linear': [round(float(v), 4) for v in mean],
        'albedo_clipped_fraction': round(clipped, 5),
        'roughness_mean': round(rmean, 4),
        'ao_mean': round(ao_mean, 4),
        'size': list(ah.shape[:2]),
        'highpass_sigma_px': round(sigma, 2),
        'highpass_cm': cutoff_cm,
    }
    print('GROUND_TEXTURES PACKED %s %s' % (family, json.dumps(info)))
    return paths, info


def main():
    os.makedirs(RAW, exist_ok=True)
    os.makedirs(PACKED, exist_ok=True)
    manifest = {'license': 'CC0 1.0 (Poly Haven)', 'detail_mean': DETAIL_MEAN, 'chroma': CHROMA,
                'highpass_cm': HIGHPASS_CM,
                'families': {}}
    selected = sys.argv[1:] or list(FAMILIES)
    unknown = set(selected) - set(FAMILIES)
    if unknown:
        raise ValueError('familles inconnues: ' + ', '.join(sorted(unknown)))
    for family in selected:
        asset, size_cm, color_name = FAMILIES[family]
        files, sources = {}, {}
        for kind, ext in MAPS:
            if family == 'Ruin' and kind == 'ao':
                sources[kind] = {'derived': 'neutral because source has no AO map'}
                continue
            url = url_for(asset, kind, ext, color_name)
            dst = os.path.join(RAW, os.path.basename(url))
            fetch(url, dst)
            files[kind] = dst
            sources[kind] = {'url': url, 'sha256': sha(dst), 'bytes': os.path.getsize(dst)}
        _, info = pack(family, asset, size_cm, files)
        info['size_cm'] = size_cm
        info['page'] = 'https://polyhaven.com/a/' + asset
        info['sources'] = sources
        manifest['families'][family] = info
    with open(os.path.join(OUT, 'manifest.json'), 'w', encoding='utf-8') as f:
        json.dump(manifest, f, indent=2)
    print('GROUND_TEXTURES COMPLETE out=' + PACKED)


if __name__ == '__main__':
    try:
        main()
    except Exception as exc:  # code de sortie non nul : le lanceur doit voir l'echec
        print('GROUND_TEXTURES FAILED %s' % exc, file=sys.stderr)
        raise
