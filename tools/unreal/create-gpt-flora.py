"""GPT_FLORA_002 -- les treize sujets de la planche GPT en StaticMesh, modelises en vraie geometrie.

PROPRIETE DES ASSETS. Ce script est la SOURCE D'AUTORITE de
  /Game/Anastasis/Vegetation/Gpt/SM_Gpt_<Espece>      treize maillages, trois LOD, deux slots
Il les recree a chaque run. Il ne touche NI `SM_Tree_*`, NI les materiaux : slot 0 = M_AnastasisVegetation (feuillage
deux faces, la lumiere le traverse), slot 1 = M_AnastasisBark, comme tous les arbres du projet. Aucune texture.

CE QUE C'EST. `gpt_flora_model.py` construit chaque sujet : fut mesure sur le sprite, branches ramifiees (colonisation
d'espace) dans le volume que dessine la silhouette, feuilles / aiguilles / fleurs / baies en maillages, fougere
fronde par fronde, prairie brin par brin. Les couleurs viennent de `SourceArt/Vegetation/gpt/gpt-flora.json` (ecrit par
`gpt-flora-fit.py` : mesures sur les sprites). Le meme code rend les treize en logiciel hors editeur
(`gpt-flora-preview.py`) : ce qui est juge la-bas est ce qui est ecrit ici.

Memes conventions que les arbres du projet : normalisation Z = [-50, +50], alpha de sommet 0 = bois / 1 = feuillage,
normales fractionnees (45 deg), trois LOD (screen size 1,0 / 0,22 / 0,055).

Lancer par tools/unreal/create-gpt-flora.ps1 (editeur dedie, discret, qui se ferme). Avec
ANASTASIS_GPT_FLORA_GRAMMAR=1 (le defaut du .ps1), il cable aussi les arbres dans DA_AnastasisPresentation
(set_tree_grammar.py, GPT_SPECIES).
"""
import importlib.util
import json
import os
import sys

import unreal

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import create_tree_asset as cta  # noqa: E402
import gpt_flora_model as model  # noqa: E402

ROOT = os.path.abspath(os.path.join(HERE, '..', '..'))
SPEC_PATH = os.path.join(ROOT, 'SourceArt', 'Vegetation', 'gpt', 'gpt-flora.json')
PKG = '/Game/Anastasis/Vegetation/Gpt'

# create-tree-cards.py (nom a tiret : chargement par chemin) : on n'y reprend que l'ecriture d'un StaticMesh a trois LOD.
_spec = importlib.util.spec_from_file_location('create_tree_cards_mod', os.path.join(HERE, 'create-tree-cards.py'))
ctc = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(ctc)

eal = unreal.EditorAssetLibrary
# Restes de l'ancienne methode (cartes de feuillage sur atlas) : supprimes s'ils existent.
STALE = (PKG + '/T_GptFlora_Atlas', cta.MATERIAL_DIR + '/M_AnastasisGptFoliage')


def log(msg):
    unreal.log('[create-gpt-flora] ' + str(msg))


def mesh_name(spec):
    return 'SM_Gpt_' + ''.join(p.capitalize() for p in spec['name'].split('_'))


def dynamic(batch, slot):
    """Un lot de triangles en DynamicMesh, dans le slot de materiau donne ; alpha de sommet = 0 bois / 1 feuillage."""
    if not batch.t:
        return unreal.DynamicMesh()
    vertices = [unreal.Vector(p[0], p[1], p[2]) for p in batch.v]
    colors = [unreal.LinearColor(c[0], c[1], c[2], batch.alpha) for c in batch.c]
    triangles = [unreal.IntVector(a, b, c) for (a, b, c) in batch.t]
    uv = [unreal.Vector2D(0.0, 0.0)] * len(vertices)
    buffers = unreal.GeometryScriptSimpleMeshBuffers(vertices=vertices, triangles=triangles, vertex_colors=colors, uv0=uv)
    mesh, unused = unreal.GeometryScript_MeshEdits.append_buffers_to_mesh(unreal.DynamicMesh(), buffers, material_id=slot)
    return mesh


def stub():
    """Une souche cachee sous le sol : le maillage garde ses deux slots meme sans bois."""
    b = model.Batch(0.0)
    model.tube(b, [(0.0, 0.0, -50.0), (0.0, 0.0, -48.6)], [.25, .2], 3, (0.05, 0.04, 0.03))
    return b


def build_lods(spec):
    lods_raw = model.build_all(spec)
    meshes = []
    for lod, (wood, leaf) in enumerate(lods_raw):
        if not wood.t:
            wood = stub()
        merged = cta.merge(unreal.DynamicMesh(), dynamic(wood, cta.SLOT_WOOD))
        merged = cta.merge(merged, dynamic(leaf, cta.SLOT_FOLIAGE))
        # Normales fractionnees apres la fusion (comme build_family) : l'arete qui compte est celle bois / feuille.
        meshes.append(cta.shade(merged))
    box0 = unreal.GeometryScript_MeshQueries.get_mesh_bounding_box(meshes[0])
    factor = cta.NORMALISED_HEIGHT / (box0.max.z - box0.min.z)
    out = []
    for mesh in meshes:
        mesh = unreal.GeometryScript_MeshTransforms.scale_mesh(
            mesh, unreal.Vector(factor, factor, factor), unreal.Vector(0.0, 0.0, box0.min.z))
        mesh = unreal.GeometryScript_MeshTransforms.translate_mesh(
            mesh, unreal.Vector(0.0, 0.0, cta.NORMALISED_BASE_Z - box0.min.z))
        out.append(mesh)
    after = unreal.GeometryScript_MeshQueries.get_mesh_bounding_box(out[0])
    if abs(after.min.z - cta.NORMALISED_BASE_Z) > cta.NORMALISE_TOLERANCE:
        raise RuntimeError('normalisation ratee : bas=%.4f' % after.min.z)
    log('%s triangles(modele)=%s' % (spec['name'], [w.count() + l.count() for w, l in lods_raw]))
    return out


def main():
    log('start')
    if unreal.get_editor_subsystem(unreal.AssetEditorSubsystem) is None:
        raise RuntimeError('editeur vivant requis ; aucun asset modifie')
    with open(SPEC_PATH, encoding='utf-8') as fh:
        data = json.load(fh)
    material = eal.load_asset(cta.MATERIAL_PATH)
    if material is None:
        raise RuntimeError('%s absent : lancer create_tree_asset.py' % cta.MATERIAL_PATH)
    # Les maillages d'abord, les restes ensuite (un materiau supprime pendant qu'un maillage le reference laisse un paquet
    # que ForceDeleteObjects ne decharge plus : cf. create-tree-cards.py).
    for spec in data['species']:
        path = PKG + '/' + mesh_name(spec)
        if eal.does_asset_exist(path):
            eal.delete_asset(path)
    for path in STALE:
        if eal.does_asset_exist(path):
            eal.delete_asset(path)
            log('STALE supprime %s' % path)
    report = []
    for spec in data['species']:
        path = PKG + '/' + mesh_name(spec)
        triangles = ctc.save_card_mesh(build_lods(spec), path, material)
        report.append('%s=%s' % (mesh_name(spec), triangles))
    log('GPT_FLORA_ASSETS::PASS %s' % ' '.join(report))
    if os.environ.get('ANASTASIS_GPT_FLORA_GRAMMAR') == '1':
        # Cable les arbres dans l'entree FOREST du registre de presentation : le corps de set_tree_grammar.py s'execute a
        # l'import (il se verifie lui-meme, RESULT::PASS / FAIL au log).
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
