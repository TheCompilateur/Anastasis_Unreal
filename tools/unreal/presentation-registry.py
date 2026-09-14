"""Cree et peuple le Data Asset de presentation ANASTASIS.

PROPRIETE DE L'ASSET. Ce script est la SOURCE D'AUTORITE de
/Game/Anastasis/Presentation/DA_AnastasisPresentation, meme convention que
tools/unreal/observe-slice.py pour M_AnastasisSlice : il le cree s'il manque,
puis il ne fait plus que le verifier. Le regenerer explicitement
(ANASTASIS_PRESENTATION_REBUILD=1) ecrase toute retouche faite a la main.

Un agent assets n'a PAS besoin de ce script : il edite l'asset dans l'editeur.
Voir docs/unreal/PRESENTATION_ASSET_BINDING.md.

Les valeurs semees ici sont exactement celles que VISUAL_BUILD_001 codait en dur,
pour que la migration ne change pas le rendu.
"""
import os, unreal

ASSET_DIR = '/Game/Anastasis/Presentation'
ASSET_NAME = 'DA_AnastasisPresentation'
ASSET_PATH = ASSET_DIR + '/' + ASSET_NAME

# (semantic, archetype_id, mesh, tint rgb, min scale, max scale, jitter)
# La graine ne pose qu'UNE variante par type : c'est ce qu'un registre vide doit
# avoir pour rendre quelque chose, pas la donnee finale. Forest recoit donc la
# silhouette de canopee et son enveloppe de stature, puis
# tools/unreal/set_tree_grammar.py y ecrit les six variantes de la grammaire --
# ce que ce format de ligne ne sait pas exprimer. Sans ce changement, un registre
# recree de zero repartait sur un cone moteur, la silhouette meme que
# TREE_FORM_001 retire.
SEED_ENTRIES = [
    (unreal.AnastasisSemanticType.FOREST, 'Tree_Generic',
     '/Game/Anastasis/Vegetation/SM_Tree_Conifer_Canopy_01',
     (0.102, 0.243, 0.114), 3.6, 5.0, 0.30),
    (unreal.AnastasisSemanticType.RUIN, 'Ruin_Generic', '/Engine/BasicShapes/Cylinder.Cylinder',
     (0.353, 0.302, 0.318), 0.6, 1.1, 0.20),
]


def log(msg):
    unreal.log('PRESENTATION_REGISTRY ' + msg)


def set_prop(obj, names, value):
    """Ecrit la premiere propriete qui existe parmi `names`.

    Le binding Python d'Unreal retire le prefixe `b` des booleens (bEnabled ->
    enabled), mais la convention a varie selon les versions : on essaie les deux
    plutot que de parier.
    """
    for name in names:
        try:
            obj.set_editor_property(name, value)
            return name
        except Exception:
            continue
    raise Exception('aucune de ces proprietes n existe: ' + ','.join(names))


def make_variant(mesh_path):
    variant = unreal.AnastasisPresentationVariant()
    mesh = unreal.load_asset(mesh_path)
    if mesh is None:
        unreal.log_error('PRESENTATION_REGISTRY MESH_MISSING ' + mesh_path)
        return None
    variant.set_editor_property('mesh', mesh)
    return variant


def make_entry(semantic, archetype_id, mesh_path, tint, min_scale, max_scale, jitter):
    entry = unreal.AnastasisPresentationEntry()
    entry.set_editor_property('semantic_type', semantic)
    entry.set_editor_property('archetype_id', archetype_id)
    set_prop(entry, ('enabled', 'b_enabled'), True)
    entry.set_editor_property('tint', unreal.LinearColor(tint[0], tint[1], tint[2], 1.0))
    entry.set_editor_property('min_uniform_scale', min_scale)
    entry.set_editor_property('max_uniform_scale', max_scale)
    entry.set_editor_property('jitter_radius_fraction', jitter)
    set_prop(entry, ('random_yaw', 'b_random_yaw'), True)
    variant = make_variant(mesh_path)
    entry.set_editor_property('variants', [variant] if variant else [])
    return entry


if os.environ.get('ANASTASIS_PRESENTATION_REBUILD', '0') == '1' and unreal.EditorAssetLibrary.does_asset_exist(ASSET_PATH):
    unreal.EditorAssetLibrary.delete_asset(ASSET_PATH)
    log('DELETED ' + ASSET_PATH)

if not unreal.EditorAssetLibrary.does_asset_exist(ASSET_PATH):
    log('CREATE ' + ASSET_PATH)
    factory = unreal.DataAssetFactory()
    factory.set_editor_property('data_asset_class', unreal.AnastasisPresentationRegistry)
    asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        ASSET_NAME, ASSET_DIR, unreal.AnastasisPresentationRegistry, factory)
    entries = [make_entry(*row) for row in SEED_ENTRIES]
    asset.set_editor_property('entries', entries)
    unreal.EditorAssetLibrary.save_asset(ASSET_PATH)
    log('SAVED entries=%d' % len(entries))

# Verification en lecture seule : l'asset committe fait foi.
asset = unreal.load_asset(ASSET_PATH)
if asset is None:
    unreal.log_error('PRESENTATION_REGISTRY LOAD_FAILED ' + ASSET_PATH)
else:
    entries = asset.get_editor_property('entries')
    log('VERIFY entries=%d' % len(entries))
    for entry in entries:
        variants = entry.get_editor_property('variants')
        meshes = []
        for variant in variants:
            mesh = variant.get_editor_property('mesh')
            meshes.append(mesh.get_path_name() if mesh else 'NONE')
        try:
            enabled = entry.get_editor_property('enabled')
        except Exception:
            enabled = entry.get_editor_property('b_enabled')
        log('  entry semantic=%s archetype=%s enabled=%s variants=%d meshes=%s scale=(%.2f,%.2f) jitter=%.2f' % (
            entry.get_editor_property('semantic_type'),
            entry.get_editor_property('archetype_id'),
            enabled,
            len(variants), ','.join(meshes),
            entry.get_editor_property('min_uniform_scale'),
            entry.get_editor_property('max_uniform_scale'),
            entry.get_editor_property('jitter_radius_fraction')))

log('COMPLETE')

# Le script se termine lui-meme, comme observe-slice.py : passer ";Quit" dans
# -ExecCmds collerait le token a l'argument du "py" et Python evaluerait le chemin.
if os.environ.get('ANASTASIS_PRESENTATION_KEEP_EDITOR', '0') != '1':
    unreal.SystemLibrary.quit_editor()
