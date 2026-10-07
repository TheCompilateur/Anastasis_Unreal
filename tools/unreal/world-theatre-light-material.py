"""WORLD_THEATRE v2.1 -- source d'autorite de deux materiaux de post-traitement, /Game/WorldTheatre/.

M_WorldTheatreDistance  avant le tonemapper (HDR lineaire) : au-dela de Start km, le sol et ce qui s'y tient
                        perdent valeur (Darken), saturation (Desaturate) et chaleur (Tint) jusqu'a Full km. Le
                        ciel (profondeur > SkyCut) n'est touche qu'a hauteur de SkyAmount. Le lointain cesse
                        d'etre la chose la plus claire de l'image : c'est l'intention (« ce qui est loin inquiete »).
                        Le ciel ne suit que dans sa bande basse (HorizonHigh degres), a hauteur de SkyAmount ; le zenith
                        reste intact.
M_WorldTheatreDepthProbe apres le tonemapper : ecrit la profondeur de scene en gris, log2 des metres sur
                        [0,5 m ; 131 km] -> [0 ; 1]. Instrument de mesure (luminance par plan de distance), jamais
                        visible en jeu.

Les deux sont pilotes par l'acteur du theatre (UAnastasisWorldTheatreSubsystem) via des instances dynamiques :
les valeurs vivent dans les CVars anastasis.Theatre.Light.*, pas dans l'asset.
Recree a chaque lancement (l'asset est supprime puis refait : rien ne le reference en dur). Lancer par world-theatre-light-material.ps1.
"""
import os
import unreal

DIR = '/Game/WorldTheatre'
mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary

DISTANCE_CODE = '''
float km = Depth / 100000.0;
// Ciel : seule la bande basse (0 a HorizonHigh degres au-dessus de l'horizon) suit le lointain, sinon la montagne
// assombrie se decoupe sur une brume restee claire (run 19 h : chaine en papier decoupe dans la brume du soir).
float3 dir = normalize(WP - CamPos);
float elev = degrees(asin(clamp(dir.z, -1.0, 1.0)));
float band = 1.0 - smoothstep(0.0, HorizonHigh, elev);
float sky = Depth > SkyCut ? SkyAmount * band : 1.0;
float t = smoothstep(Start, Full, km) * sky;
float3 c = C.rgb;
float l = dot(c, float3(0.2126, 0.7152, 0.0722));
float3 d = lerp(c, float3(l, l, l), Desaturate * t);
d *= lerp(float3(1.0, 1.0, 1.0), Tint, t);
d *= 1.0 - Darken * t;
return d;
'''

PROBE_CODE = '''
float m = max(Depth / 100.0, 0.5);
float g = saturate((log2(m) + 1.0) / 18.0);
return float3(g, g, g);
'''


def log(msg):
    unreal.log('[world-theatre-light-material] ' + str(msg))


def enum_value(enum, *names):
    for n in names:
        if hasattr(enum, n):
            return getattr(enum, n), n
    raise RuntimeError('aucun de %s dans %s' % (names, enum))


def fresh(name):
    path = DIR + '/' + name
    # delete_all_material_expressions laisse des expressions sur ce graphe (run du 2026-10-07) : les deux assets ne sont
    # references que par chemin (UAnastasisWorldTheatreSubsystem), on les recree donc entiers.
    if eal.does_asset_exist(path) and not eal.delete_asset(path):
        raise RuntimeError('suppression impossible : ' + path)
    mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, DIR, unreal.Material, unreal.MaterialFactoryNew())
    if mat is None:
        raise RuntimeError('creation impossible : ' + path)
    mat.set_editor_property('material_domain', unreal.MaterialDomain.MD_POST_PROCESS)
    return mat, path


def scene_texture(mat, sid, x, y):
    node = mel.create_material_expression(mat, unreal.MaterialExpressionSceneTexture, x, y)
    node.set_editor_property('scene_texture_id', sid)
    return node


def custom(mat, code, inputs, x, y, label):
    node = mel.create_material_expression(mat, unreal.MaterialExpressionCustom, x, y)
    node.set_editor_property('description', label)
    node.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    node.set_editor_property('code', code)
    entries = []
    for name in inputs:
        e = unreal.CustomInput()
        e.set_editor_property('input_name', name)
        entries.append(e)
    node.set_editor_property('inputs', entries)
    return node


def link(src, out, dst, inp):
    if not mel.connect_material_expressions(src, out, dst, inp):
        raise RuntimeError('lien %s.%s -> %s non pose' % (out, inp, inp))


def compile_and_save(mat, path, name):
    unreal.log('WORLD_THEATRE_MATERIAL_COMPILE_BEGIN ' + name)
    errors = list(mel.recompile_material(mat) or [])
    if errors:
        for e in errors:
            unreal.log_error('WORLD_THEATRE_MATERIAL_ERROR %s %s' % (name, e))
        raise RuntimeError('%s ne compile pas' % name)
    eal.save_asset(path)
    log('MATERIAL ' + path)


def build_distance():
    mat, path = fresh('M_WorldTheatreDistance')
    loc, used = enum_value(unreal.BlendableLocation, 'BL_SCENE_COLOR_BEFORE_DOF', 'BL_BEFORE_TRANSLUCENCY')
    mat.set_editor_property('blendable_location', loc)
    log('blendable_location=' + used)
    color = scene_texture(mat, unreal.SceneTextureId.PPI_POST_PROCESS_INPUT0, -1100, -200)
    depth = scene_texture(mat, unreal.SceneTextureId.PPI_SCENE_DEPTH, -1100, 0)
    node = custom(mat, DISTANCE_CODE, ['C', 'Depth', 'Start', 'Full', 'Darken', 'Desaturate', 'Tint', 'SkyCut', 'SkyAmount',
                                       'HorizonHigh', 'WP', 'CamPos'], -400, 0, 'WorldTheatreDistance')
    wp = mel.create_material_expression(mat, unreal.MaterialExpressionWorldPosition, -800, 700)
    link(wp, '', node, 'WP')
    cam = mel.create_material_expression(mat, unreal.MaterialExpressionCameraPositionWS, -800, 780)
    link(cam, '', node, 'CamPos')
    link(color, 'Color', node, 'C')
    # Profondeur : canal R du SceneTexture, masque explicite sur les quatre canaux (piege du ComponentMask).
    mask = mel.create_material_expression(mat, unreal.MaterialExpressionComponentMask, -800, 0)
    for ch, on in (('r', True), ('g', False), ('b', False), ('a', False)):
        mask.set_editor_property(ch, on)
    link(depth, 'Color', mask, '')
    link(mask, '', node, 'Depth')
    defaults = (('Start', 1.5), ('Full', 9.0), ('Darken', 0.45), ('Desaturate', 0.45), ('SkyCut', 2.0e7), ('SkyAmount', 0.7),
                ('HorizonHigh', 12.0))
    for k, (pname, value) in enumerate(defaults):
        p = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -800, 150 + 70 * k)
        p.set_editor_property('parameter_name', pname)
        p.set_editor_property('default_value', value)
        link(p, '', node, pname)
    tint = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -800, 600)
    tint.set_editor_property('parameter_name', 'Tint')
    tint.set_editor_property('default_value', unreal.LinearColor(0.86, 0.94, 1.06, 1.0))
    link(tint, '', node, 'Tint')
    if not mel.connect_material_property(node, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR):
        raise RuntimeError('emissive non cable')
    compile_and_save(mat, path, 'M_WorldTheatreDistance')


def build_probe():
    mat, path = fresh('M_WorldTheatreDepthProbe')
    loc, used = enum_value(unreal.BlendableLocation, 'BL_SCENE_COLOR_AFTER_TONEMAPPING', 'BL_AFTER_TONEMAPPING')
    mat.set_editor_property('blendable_location', loc)
    log('probe blendable_location=' + used)
    depth = scene_texture(mat, unreal.SceneTextureId.PPI_SCENE_DEPTH, -900, 0)
    mask = mel.create_material_expression(mat, unreal.MaterialExpressionComponentMask, -600, 0)
    for ch, on in (('r', True), ('g', False), ('b', False), ('a', False)):
        mask.set_editor_property(ch, on)
    link(depth, 'Color', mask, '')
    node = custom(mat, PROBE_CODE, ['Depth'], -300, 0, 'WorldTheatreDepthProbe')
    link(mask, '', node, 'Depth')
    if not mel.connect_material_property(node, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR):
        raise RuntimeError('emissive non cable')
    compile_and_save(mat, path, 'M_WorldTheatreDepthProbe')


def main():
    if unreal.get_editor_subsystem(unreal.AssetEditorSubsystem) is None:
        raise RuntimeError('editeur vivant requis ; aucun asset modifie')
    build_distance()
    build_probe()
    log('WORLD_THEATRE_LIGHT_MATERIAL::PASS')


if __name__ == '__main__':
    try:
        main()
    except Exception as exc:  # noqa: BLE001
        import traceback
        unreal.log_error('WORLD_THEATRE_LIGHT_MATERIAL::FAIL %s\n%s' % (exc, traceback.format_exc()))
    finally:
        if os.environ.get('ANASTASIS_THEATRE_MATERIAL_QUIT') == '1':
            unreal.SystemLibrary.quit_editor()
