"""GPT_FLORA_001 -- les treize sprites GPT en StaticMesh : atlas, materiau, maillages.

PROPRIETE DES ASSETS. Ce script est la SOURCE D'AUTORITE de
  /Game/Anastasis/Vegetation/Gpt/T_GptFlora_Atlas           l'atlas de gerbes (SourceArt/Vegetation/gpt/gpt-flora-atlas.png)
  /Game/Anastasis/Materials/M_AnastasisGptFoliage           le materiau des cartes (duplicata de M_AnastasisVegetation)
  /Game/Anastasis/Vegetation/Gpt/SM_Gpt_<Espece>            treize maillages, trois LOD
Il les recree a chaque run. Il ne touche NI `SM_Tree_*`, NI `SM_Tree_HolmOak_Card_*`, NI les
materiaux existants. Retoucher un de ces assets a la main ne survit pas au prochain run.

D'OU VIENT LA FORME. `SourceArt/Vegetation/gpt/gpt-flora.json` (ecrit par `gpt-flora-fit.py`, hors editeur)
porte, par espece, la silhouette du sprite en tranches, l'axe et la largeur du fut, les teintes et les cases
de l'atlas. `gpt_flora_geometry.py` en tire les troncons de bois et les cartes de feuillage : le meme code
que le controle logiciel (`gpt-flora-preview.py`), la meme graine. Ici on ne fait que les ecrire.

MEME GRAMMAIRE QUE LES ARBRES DU PROJET : normalisation Z = [-50, +50], deux slots (0 feuillage, 1 bois),
trois LOD (cartes / 75 % des cartes plus grandes / masses fermees), normales spheriques peintes sur les
cartes, normales fractionnees sur le bois, bois et cartes de `create-tree-cards.py` (importes tels quels).

Lancer par tools/unreal/create-gpt-flora.ps1 (editeur dedie, discret, qui se ferme). Avec
ANASTASIS_GPT_FLORA_GRAMMAR=1 (le defaut du .ps1), il cable aussi les arbres dans DA_AnastasisPresentation
(set_tree_grammar.py, GPT_SPECIES).
"""
import importlib.util
import json
import math
import os
import random
import sys

import unreal

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import create_tree_asset as cta  # noqa: E402
import gpt_flora_geometry as geo  # noqa: E402

ROOT = os.path.abspath(os.path.join(HERE, '..', '..'))
SPEC_PATH = os.path.join(ROOT, 'SourceArt', 'Vegetation', 'gpt', 'gpt-flora.json')
ATLAS_PATH = os.path.join(ROOT, 'SourceArt', 'Vegetation', 'gpt', 'gpt-flora-atlas.png')
PKG = '/Game/Anastasis/Vegetation/Gpt'
MATERIAL_NAME = 'M_AnastasisGptFoliage'
ALBEDO_GAIN = 0.75
LOD_SCREEN = (1.0, 0.22, 0.055)

# create-tree-cards.py (nom a tiret : chargement par chemin). Ses fonctions lisent des constantes de module
# a l'appel : on pose les notres avant de les appeler, l'atlas et le materiau du chene vert restent intacts.
_spec = importlib.util.spec_from_file_location('create_tree_cards_mod', os.path.join(HERE, 'create-tree-cards.py'))
ctc = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(ctc)
ctc.ATLAS_SRC = ATLAS_PATH
ctc.CARDS_PKG = PKG
ctc.TEX_NAME = 'T_GptFlora_Atlas'
ctc.TEX_PATH = PKG + '/T_GptFlora_Atlas'
ctc.CARD_MATERIAL_NAME = MATERIAL_NAME
ctc.CARD_MATERIAL_PATH = cta.MATERIAL_DIR + '/' + MATERIAL_NAME

eal = unreal.EditorAssetLibrary


def log(msg):
    unreal.log('[create-gpt-flora] ' + str(msg))


def mesh_name(spec):
    return 'SM_Gpt_' + ''.join(p.capitalize() for p in spec['name'].split('_'))


def crown_frame(spec):
    bands = spec['bands']
    if spec['mode'] in ('fronds', 'tufts'):
        hw = bands[0][4]
        return (0.0, 0.0, -62.0), (max(hw, 1.0), max(hw, 1.0), 40.0)
    lo_z = min(b[0] - b[1] * .5 for b in bands)
    hi_z = max(b[0] + b[1] * .5 for b in bands)
    cx = sum(b[2] for b in bands) / len(bands)
    return (cx, 0.0, .5 * (lo_z + hi_z)), (max(max(b[4] for b in bands), 1.0), max(max(b[5] for b in bands), 1.0),
                                          max(.5 * (hi_z - lo_z), 1.0))


def add_card(buf, card, spec, info_by_cell, centre, radii, lod, atlas):
    cols, rows = atlas['cols'], atlas['rows']
    cell = card['cell']
    ci, cj = cell % cols, cell // cols
    ref = info_by_cell[cell]['luma_ref']
    height_lift = max(0.0, min(1.0, 0.5 + 0.5 * (card['anchor'][2] - centre[2]) / radii[2]))
    tone = card['tone'] * (0.50 + 0.52 * card['depth']) * (0.90 + 0.18 * height_lift)
    k = min(1.0, ref * spec.get('gain', ALBEDO_GAIN) * tone)
    rgba = (k, k, k, 1.0)
    ids = []
    for p, (u, v) in geo.card_vertices(card, lod):
        n = ctc.spherical_normal(p, centre, radii)
        ids.append(buf.vertex(p, n, rgba, ((ci + u) / cols, (cj + v) / rows)))
    for row in range(geo.CARD_ROWS - 1):
        a, b, c, d = ids[2 * row], ids[2 * row + 1], ids[2 * row + 2], ids[2 * row + 3]
        buf.tri(a, c, b)
        buf.tri(b, c, d)


def card_mesh(spec, cards, info_by_cell, centre, radii, lod, atlas):
    buf = ctc.Buffers()
    keep = (1.0, 0.75, 0.0)[lod]
    for card in cards:
        if card['keep'] < keep:
            add_card(buf, card, spec, info_by_cell, centre, radii, lod, atlas)
    return buf.mesh(cta.SLOT_FOLIAGE)


def lump_mesh(spec, atlas, rng):
    """LOD2 : masses fermees par tranche, UV sur la case pleine de l'atlas, teinte moyenne de l'espece."""
    buf = ctc.Buffers()
    cols, rows = atlas['cols'], atlas['rows']
    solid = atlas['solid_cell']
    suv = ((solid % cols + .5) / cols, (solid // cols + .5) / rows)
    fol = spec['foliage']
    bands = spec['bands']
    for band in bands[::2]:
        zc, h, cx, cy, hw, hd = band[:6]
        fill = band[6] if len(band) > 6 else 1.0
        if fill < .3:
            continue
        pieces = 2 if hw > 6 else 1
        for k in range(pieces):
            ang = k * math.tau / pieces + rng.uniform(-.3, .3)
            ox = cx + (hw * .45 * math.cos(ang) if pieces > 1 else 0.0)
            oy = cy + (hd * .45 * math.sin(ang) if pieces > 1 else 0.0)
            rx = hw * (.62 if pieces > 1 else 1.0)
            ry = hd * (.62 if pieces > 1 else 1.0)
            rz = max(h * 1.4, .8)           # une tranche sur deux : chaque masse double de hauteur
            tone = rng.uniform(.66, .90)
            rgba = (min(1.0, fol[0] * tone), min(1.0, fol[1] * tone), min(1.0, fol[2] * tone), 1.0)
            r_, c_ = 4, 8
            grid = []
            for i in range(r_ + 1):
                phi = math.pi * i / r_
                for j in range(c_):
                    th = math.tau * j / c_
                    n = (math.sin(phi) * math.cos(th), math.sin(phi) * math.sin(th), math.cos(phi))
                    p = (ox + rx * n[0], oy + ry * n[1], zc + rz * n[2])
                    nn = ctc.norm((n[0] / max(rx, .1), n[1] / max(ry, .1), n[2] / max(rz, .1)))
                    grid.append(buf.vertex(p, nn, rgba, suv))
            for i in range(r_):
                for j in range(c_):
                    a = grid[i * c_ + j]
                    b = grid[i * c_ + (j + 1) % c_]
                    c = grid[(i + 1) * c_ + j]
                    d = grid[(i + 1) * c_ + (j + 1) % c_]
                    buf.tri(a, c, b)
                    buf.tri(b, c, d)
    return buf.mesh(cta.SLOT_FOLIAGE)


def wood_mesh(spec, rng, limit=None):
    segs = geo.build_wood(spec, rng)
    bark = unreal.LinearColor(spec['bark'][0], spec['bark'][1], spec['bark'][2], 0.0)   # alpha 0 : du bois
    wood = unreal.DynamicMesh()
    if spec['mode'] in ('fronds', 'tufts') or not segs:
        # une souche invisible : le maillage garde ses deux slots, comme tous les arbres du projet
        segs = [((0.0, 0.0, -50.0), (0.0, 0.0, -48.5), .25, .2, 'stub')]
    for i, (s, e, r0, r1, role) in enumerate(segs):
        if limit is not None and i >= limit:
            break
        if math.dist(s, e) < 1e-3:       # troncon nul : branch_between divise par sa longueur
            continue
        part = cta.branch_between(unreal.DynamicMesh(), s, e, max(r0, .1), max(r1, .05), steps=10 if role == 'trunk' else 6)
        wood = cta.merge(wood, cta.coloured(part, bark))
    return wood


def build_lods(spec, atlas):
    rng = random.Random(spec['name'] + ':wood')
    wood = cta.shade(wood_mesh(spec, rng))
    cards = geo.build_cards(spec, random.Random(spec['name'] + ':cards'))
    centre, radii = crown_frame(spec)
    info_by_cell = {c['cell']: c for c in spec['cell_info']}
    lods = []
    for lod in (0, 1):
        mesh = cta.merge(unreal.DynamicMesh(), wood)
        mesh = cta.merge(mesh, card_mesh(spec, cards, info_by_cell, centre, radii, lod, atlas))
        lods.append(mesh)
    far = cta.merge(unreal.DynamicMesh(), cta.shade(wood_mesh(spec, random.Random(spec['name'] + ':wood'), limit=6)))
    far = cta.merge(far, lump_mesh(spec, atlas, random.Random(spec['name'] + ':lumps')))
    lods.append(far)
    box0 = unreal.GeometryScript_MeshQueries.get_mesh_bounding_box(lods[0])
    factor = cta.NORMALISED_HEIGHT / (box0.max.z - box0.min.z)
    out = []
    for mesh in lods:
        mesh = unreal.GeometryScript_MeshTransforms.scale_mesh(
            mesh, unreal.Vector(factor, factor, factor), unreal.Vector(0.0, 0.0, box0.min.z))
        mesh = unreal.GeometryScript_MeshTransforms.translate_mesh(
            mesh, unreal.Vector(0.0, 0.0, cta.NORMALISED_BASE_Z - box0.min.z))
        out.append(mesh)
    after = unreal.GeometryScript_MeshQueries.get_mesh_bounding_box(out[0])
    if abs(after.min.z - cta.NORMALISED_BASE_Z) > cta.NORMALISE_TOLERANCE:
        raise RuntimeError('normalisation ratee : bas=%.4f' % after.min.z)
    log('%s cartes=%d bandes=%d bois=%d cadre=%s' % (spec['name'], len(cards), len(spec['bands']),
                                                    len(geo.build_wood(spec, random.Random(spec['name'] + ':wood'))),
                                                    [round(c, 1) for c in centre]))
    return out


def main():
    log('start')
    if unreal.get_editor_subsystem(unreal.AssetEditorSubsystem) is None:
        raise RuntimeError('editeur vivant requis ; aucun asset modifie')
    with open(SPEC_PATH, encoding='utf-8') as fh:
        data = json.load(fh)
    atlas = data['atlas']
    if not os.path.isfile(ATLAS_PATH):
        raise RuntimeError('atlas absent : lancer python tools/unreal/gpt-flora-fit.py (%s)' % ATLAS_PATH)
    # les maillages d'abord, le materiau ensuite (cf. create-tree-cards.py : un materiau supprime pendant
    # qu'un mesh le reference laisse un paquet que ForceDeleteObjects ne decharge plus)
    for spec in data['species']:
        path = PKG + '/' + mesh_name(spec)
        if eal.does_asset_exist(path):
            eal.delete_asset(path)
    tex = ctc.import_atlas()
    material = ctc.ensure_card_material(tex)
    report = []
    for spec in data['species']:
        path = PKG + '/' + mesh_name(spec)
        triangles = ctc.save_card_mesh(build_lods(spec, atlas), path, material)
        report.append('%s=%s' % (mesh_name(spec), triangles))
    log('GPT_FLORA_ASSETS::PASS %s' % ' '.join(report))
    if os.environ.get('ANASTASIS_GPT_FLORA_GRAMMAR') == '1':
        # Cable les sept maillages arbres dans l'entree FOREST du registre de presentation : le corps de
        # set_tree_grammar.py s'execute a l'import (il se verifie lui-meme, RESULT::PASS / FAIL au log).
        import set_tree_grammar  # noqa: F401


if __name__ == '__main__':
    try:
        main()
    except Exception as exc:  # noqa: BLE001
        import traceback
        unreal.log_error('GPT_FLORA_ASSETS::FAIL %s\n%s' % (exc, traceback.format_exc()))
    finally:
        if os.environ.get('ANASTASIS_GPT_FLORA_QUIT') == '1':
            unreal.SystemLibrary.quit_editor()
