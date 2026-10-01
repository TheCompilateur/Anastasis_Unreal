"""VILLAGER_PNG_001 -- import des portraits d'habitants, materiau de carte, population du registre.

SOURCE D'AUTORITE de :
  /Game/Anastasis/Characters/PNG/<Categorie>/CHR_*      une texture par PNG de SourceArt/Characters/PNG
  /Game/Anastasis/Characters/M_AnastasisVillager        carte masquee, `Portrait` (texture) et `Mirror` (0/1)
  DA_AnastasisPresentation.Villagers / .VillagerMaterial

Chaque run reimporte les PNG presents (la source fait foi), reecrit le materiau, reecrit la liste
du registre a partir du manifeste, puis RELIT tout et verifie. Les PNG absents sont simplement
absents de la population : un depot partiel est un etat valide.

Lance par tools/unreal/import-villagers.ps1 (editeur dedie sur /Engine/Maps/Entry, qui se ferme).
Sortie : lignes VILLAGERS_IMPORT::... dans le log.
"""
import json
import os
import unreal

ROOT = os.path.abspath(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
MANIFEST = os.path.join(ROOT, 'SourceArt', 'Characters', 'villager-population.json')
# La population : ecrite par `villager-png.py sheets` (ids, categories, statures, pose).
EXTRACT = os.path.join(ROOT, 'SourceArt', 'Characters', 'villager-extract.json')
SOURCE = os.path.join(ROOT, 'SourceArt', 'Characters', 'PNG')
DEST = '/Game/Anastasis/Characters/PNG'
MATERIAL_PATH = '/Game/Anastasis/Characters/M_AnastasisVillager'
REGISTRY_PATH = '/Game/Anastasis/Presentation/DA_AnastasisPresentation'

CATEGORY_ENUM = {
    'Adult_Male': 'ADULT_MALE', 'Adult_Female': 'ADULT_FEMALE',
    'Elder_Male': 'ELDER_MALE', 'Elder_Female': 'ELDER_FEMALE',
    'Child_Male': 'CHILD_MALE', 'Child_Female': 'CHILD_FEMALE',
}

FAILURES = []
# bInGame : le binding Python d'UE 5.8 l'expose sous `game` (il retire `bIn`, pas seulement `b`),
# vu au premier import de la population. On essaie les deux plutot que de parier.
IN_GAME_NAMES = ['in_game', 'game']


def log(msg):
    unreal.log('VILLAGERS_IMPORT ' + msg)


def fail(msg):
    FAILURES.append(msg)
    unreal.log_error('VILLAGERS_IMPORT::FAIL ' + msg)


def get_first(obj, names):
    for name in names:
        try:
            return obj.get_editor_property(name)
        except Exception:
            continue
    raise Exception('aucune propriete parmi ' + ','.join(names))


def set_first(obj, names, value):
    for name in names:
        try:
            obj.set_editor_property(name, value)
            return name
        except Exception:
            continue
    raise Exception('aucune propriete parmi ' + ','.join(names))


# ------------------------------------------------------------------------------------------ textures

def configure_texture(tex):
    """Portrait detoure, vu de pres comme de loin.

    BC7 : la compression RGBA de qualite (BC3/DXT5 bave sur les contours d'un visage de 60 px).
    sRGB : c'est une couleur peinte. Groupe Character : le streaming et le filtrage des personnages.
    Bords en Clamp : l'alpha nul du canevas ne doit pas boucler sur l'autre bord.
    Couverture alpha conservee dans les mips : sinon la silhouette maigrit avec la distance
    (seuil 0,5 = le seuil du masque du materiau).
    """
    tex.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_BC7)
    tex.set_editor_property('srgb', True)
    tex.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_CHARACTER)
    tex.set_editor_property('mip_gen_settings', unreal.TextureMipGenSettings.TMGS_FROM_TEXTURE_GROUP)
    tex.set_editor_property('filter', unreal.TextureFilter.TF_DEFAULT)
    tex.set_editor_property('address_x', unreal.TextureAddress.TA_CLAMP)
    tex.set_editor_property('address_y', unreal.TextureAddress.TA_CLAMP)
    tex.set_editor_property('compression_no_alpha', False)
    # Hors streaming : 32 portraits 512x1024 BC7 font ~22 Mo ; streames, la premiere apparition d'un
    # habitant sert un mip flou (vu au premier run PIE, texte du portrait illisible a 4 m).
    tex.set_editor_property('never_stream', True)
    try:
        set_first(tex, ['do_scale_mips_for_alpha_coverage', 'b_do_scale_mips_for_alpha_coverage'], True)
        tex.set_editor_property('alpha_coverage_thresholds', unreal.Vector4(0.0, 0.0, 0.0, 0.5))
    except Exception as exc:  # noqa: BLE001 -- rapporte, la verification le relira
        log('WARN couverture alpha non reglable: %s' % exc)


def import_portraits(people):
    tasks = []
    for p in people:
        src = os.path.join(SOURCE, p['category'], p['id'] + '.png')
        if not os.path.exists(src):
            continue
        task = unreal.AssetImportTask()
        task.set_editor_property('filename', src)
        task.set_editor_property('destination_path', DEST + '/' + p['category'])
        task.set_editor_property('destination_name', p['id'])
        task.set_editor_property('replace_existing', True)
        task.set_editor_property('automated', True)
        task.set_editor_property('save', False)
        tasks.append((p, task))
    if tasks:
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([t for _, t in tasks])
    imported = []
    for p, task in tasks:
        path = '%s/%s/%s' % (DEST, p['category'], p['id'])
        tex = unreal.EditorAssetLibrary.load_asset(path)
        if not isinstance(tex, unreal.Texture2D):
            fail('%s : import sans Texture2D' % p['id'])
            continue
        configure_texture(tex)
        unreal.EditorAssetLibrary.save_asset(path, only_if_is_dirty=False)
        imported.append((p, tex))
    return imported


# ------------------------------------------------------------------------------------------ material

def ensure_material(default_texture):
    mel = unreal.MaterialEditingLibrary
    mat = unreal.EditorAssetLibrary.load_asset(MATERIAL_PATH)
    if mat is None:
        mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            'M_AnastasisVillager', MATERIAL_PATH.rsplit('/', 1)[0], unreal.Material, unreal.MaterialFactoryNew())
    mel.delete_all_material_expressions(mat)

    # UV = (lerp(u, 1-u, Mirror), v) : le portrait regarde a gauche, Mirror le retourne.
    uv = mel.create_material_expression(mat, unreal.MaterialExpressionTextureCoordinate, -1200, 0)
    # Les quatre canaux explicitement : un masque qui garde un canal de trop rend un float2 la
    # ou on attend un scalaire, et l'echantillon refuse ses UV (float4 -> float2, vu au 1er run).
    u = mel.create_material_expression(mat, unreal.MaterialExpressionComponentMask, -1000, -60)
    v = mel.create_material_expression(mat, unreal.MaterialExpressionComponentMask, -1000, 80)
    for node, keep in ((u, 'r'), (v, 'g')):
        for ch in ('r', 'g', 'b', 'a'):
            node.set_editor_property(ch, ch == keep)
    mel.connect_material_expressions(uv, '', u, '')
    mel.connect_material_expressions(uv, '', v, '')
    flip = mel.create_material_expression(mat, unreal.MaterialExpressionOneMinus, -820, -20)
    mel.connect_material_expressions(u, '', flip, '')
    mirror = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -820, 40)
    mirror.set_editor_property('parameter_name', 'Mirror')
    mirror.set_editor_property('default_value', 0.0)
    pick = mel.create_material_expression(mat, unreal.MaterialExpressionLinearInterpolate, -640, -40)
    mel.connect_material_expressions(u, '', pick, 'A')
    mel.connect_material_expressions(flip, '', pick, 'B')
    mel.connect_material_expressions(mirror, '', pick, 'Alpha')
    uv2 = mel.create_material_expression(mat, unreal.MaterialExpressionAppendVector, -460, 20)
    mel.connect_material_expressions(pick, '', uv2, 'A')
    mel.connect_material_expressions(v, '', uv2, 'B')

    sample = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSampleParameter2D, -260, 0)
    sample.set_editor_property('parameter_name', 'Portrait')
    sample.set_editor_property('texture', default_texture)
    sample.set_editor_property('sampler_type', unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    mel.connect_material_expressions(uv2, '', sample, 'UVs')
    if not mel.connect_material_property(sample, 'RGB', unreal.MaterialProperty.MP_BASE_COLOR):
        fail('materiau : BaseColor non branche')
    if not mel.connect_material_property(sample, 'A', unreal.MaterialProperty.MP_OPACITY_MASK):
        fail('materiau : OpacityMask non branche')
    for value, prop, y in ((0.85, unreal.MaterialProperty.MP_ROUGHNESS, 260), (0.2, unreal.MaterialProperty.MP_SPECULAR, 320)):
        k = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -260, y)
        k.set_editor_property('r', value)
        mel.connect_material_property(k, '', prop)

    # Eclairage : la carte est plate et tourne vers la camera. Eclairee par SA normale, elle passe
    # au noir des que le soleil est derriere elle (vu au premier banc). Le portrait porte deja sa
    # lumiere peinte : on l'eclaire avec une normale monde penchee vers le haut, comme le sol sur
    # lequel l'habitant se tient -- le jour et la nuit du monde la touchent, l'angle de vue non.
    vn = mel.create_material_expression(mat, unreal.MaterialExpressionVertexNormalWS, -900, 420)
    side = mel.create_material_expression(mat, unreal.MaterialExpressionTwoSidedSign, -900, 480)
    facing = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -720, 440)
    mel.connect_material_expressions(vn, '', facing, 'A')
    mel.connect_material_expressions(side, '', facing, 'B')
    weight = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -720, 520)
    weight.set_editor_property('r', 0.35)
    tilt = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -560, 460)
    mel.connect_material_expressions(facing, '', tilt, 'A')
    mel.connect_material_expressions(weight, '', tilt, 'B')
    up = mel.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector, -560, 540)
    up.set_editor_property('constant', unreal.LinearColor(0.0, 0.0, 1.0, 0.0))
    blend = mel.create_material_expression(mat, unreal.MaterialExpressionAdd, -400, 480)
    mel.connect_material_expressions(tilt, '', blend, 'A')
    mel.connect_material_expressions(up, '', blend, 'B')
    unit = mel.create_material_expression(mat, unreal.MaterialExpressionNormalize, -260, 480)
    mel.connect_material_expressions(blend, '', unit, '')
    if not mel.connect_material_property(unit, '', unreal.MaterialProperty.MP_NORMAL):
        fail('materiau : Normal non branche')
    mat.set_editor_property('tangent_space_normal', False)

    mat.set_editor_property('blend_mode', unreal.BlendMode.BLEND_MASKED)
    mat.set_editor_property('opacity_mask_clip_value', 0.5)
    mat.set_editor_property('two_sided', True)
    mat.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    # La compilation echoue en Warning dans le log, pas en exception, et de facon synchrone :
    # import-villagers.ps1 refuse le PASS si un echec de ce materiau suit ce marqueur. Les echecs
    # d'avant sont ceux des etats intermediaires du graphe, pendant les branchements.
    log('MATERIAL_COMPILE_FINAL ' + MATERIAL_PATH)
    mel.recompile_material(mat)
    unreal.EditorAssetLibrary.save_asset(MATERIAL_PATH, only_if_is_dirty=False)
    return mat


# ------------------------------------------------------------------------------------------ registry

def write_registry(imported, material):
    reg = unreal.EditorAssetLibrary.load_asset(REGISTRY_PATH)
    if reg is None:
        fail('registre introuvable : ' + REGISTRY_PATH + ' (tools/unreal/presentation-registry.py le cree)')
        return
    looks = []
    for p, tex in imported:
        look = unreal.AnastasisVillagerLook()
        look.set_editor_property('look_id', p['id'])
        look.set_editor_property('category', getattr(unreal.AnastasisVillagerCategory, CATEGORY_ENUM[p['category']]))
        look.set_editor_property('portrait', tex)
        # Assis : sur la planche, jamais sur un habitant qui marche.
        set_first(look, IN_GAME_NAMES, bool(p.get('in_game', True)))
        looks.append(look)
    reg.set_editor_property('villagers', looks)
    reg.set_editor_property('villager_material', material)
    unreal.EditorAssetLibrary.save_asset(REGISTRY_PATH, only_if_is_dirty=False)


# ------------------------------------------------------------------------------------------ verify

def verify(people, canvas):
    reg = unreal.EditorAssetLibrary.load_asset(REGISTRY_PATH)
    looks = list(reg.get_editor_property('villagers')) if reg else []
    by_id = {str(l.get_editor_property('look_id')): l for l in looks}
    counts = {}
    for p in people:
        src = os.path.join(SOURCE, p['category'], p['id'] + '.png')
        if not os.path.exists(src):
            if p['id'] in by_id:
                fail('%s au registre sans PNG source' % p['id'])
            continue
        look = by_id.get(p['id'])
        if look is None:
            fail('%s : PNG present, absent du registre' % p['id'])
            continue
        tex = look.get_editor_property('portrait')
        if not isinstance(tex, unreal.Texture2D):
            fail('%s : portrait non charge' % p['id'])
            continue
        size = (tex.blueprint_get_size_x(), tex.blueprint_get_size_y())
        checks = {
            'taille': size == (canvas['width'], canvas['height']),
            'bc7': tex.get_editor_property('compression_settings') == unreal.TextureCompressionSettings.TC_BC7,
            'srgb': tex.get_editor_property('srgb'),
            'alpha': not tex.get_editor_property('compression_no_alpha'),
            'groupe': tex.get_editor_property('lod_group') == unreal.TextureGroup.TEXTUREGROUP_CHARACTER,
            'clamp': tex.get_editor_property('address_x') == unreal.TextureAddress.TA_CLAMP,
            'hors_streaming': tex.get_editor_property('never_stream'),
            'categorie': look.get_editor_property('category') == getattr(unreal.AnastasisVillagerCategory, CATEGORY_ENUM[p['category']]),
            'en_jeu': get_first(look, IN_GAME_NAMES) == bool(p.get('in_game', True)),
        }
        try:
            checks['couverture_alpha'] = bool(tex.get_editor_property('do_scale_mips_for_alpha_coverage'))
        except Exception:
            checks['couverture_alpha'] = False
        bad = [k for k, ok in checks.items() if not ok]
        if bad:
            fail('%s : %s (taille %dx%d)' % (p['id'], ','.join(bad), size[0], size[1]))
        else:
            log('VERIFY %s %dx%d bc7 srgb alpha character clamp hors_streaming couverture_alpha' % (p['id'], size[0], size[1]))
        counts[p['category']] = counts.get(p['category'], 0) + 1
    mat = reg.get_editor_property('villager_material') if reg else None
    if mat is None:
        fail('registre sans VillagerMaterial')
    else:
        log('VERIFY materiau %s masked=%s two_sided=%s' % (
            mat.get_path_name(), mat.get_editor_property('blend_mode'), mat.get_editor_property('two_sided')))
    extra = sorted(set(by_id) - {p['id'] for p in people})
    if extra:
        fail('entrees hors manifeste au registre : ' + ','.join(extra))
    return counts, len(looks)


def main():
    with open(MANIFEST, encoding='utf-8') as f:
        manifest = json.load(f)
    if not os.path.exists(EXTRACT):
        fail('population absente : lancer `python tools/unreal/villager-png.py sheets` puis `prep`')
        people = []
    else:
        with open(EXTRACT, encoding='utf-8') as f:
            people = json.load(f)['people']
    imported = import_portraits(people)
    if not imported:
        fail('aucun PNG traite dans ' + SOURCE + ' (lancer villager-png.py prep)')
    else:
        material = ensure_material(imported[0][1])
        write_registry(imported, material)
    counts, n = verify(people, manifest['canvas'])
    log('POPULATION ' + ' '.join('%s=%d' % (c, counts.get(c, 0)) for c in CATEGORY_ENUM) + ' total=%d en_jeu=%d' % (n, sum(1 for p in people if p.get('in_game', True) and not p['category'].startswith('Child') and os.path.exists(os.path.join(SOURCE, p['category'], p['id'] + '.png')))))
    print('VILLAGERS_IMPORT::%s imported=%d registry=%d failures=%d' % ('PASS' if not FAILURES else 'FAIL', len(imported), n, len(FAILURES)))
    unreal.log('VILLAGERS_IMPORT::%s imported=%d registry=%d failures=%d' % ('PASS' if not FAILURES else 'FAIL', len(imported), n, len(FAILURES)))


main()
if os.environ.get('ANASTASIS_VILLAGERS_QUIT') == '1':
    unreal.SystemLibrary.quit_editor()
