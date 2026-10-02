"""RAIN_001 -- la pluie qui tombe : materiau et maillage des stries.
PROPRIETE DES ASSETS. Ce script est la SOURCE D'AUTORITE de
/Game/Anastasis/Weather/M_AnastasisRain et /Game/Anastasis/Weather/SM_AnastasisRainStreak.
Il les cree, puis ne les modifie plus : les regenerer est explicite (ANASTASIS_RAIN_REBUILD=1)
et ecrase toute retouche faite a la main. Lance par tools/unreal/rain-material.ps1.

POURQUOI PAS NIAGARA. Un systeme Niagara ne se construit pas proprement par script ; la pluie
restait donc invisible (handoffs/env-realism-001.md). Ici, AUCUNE particule simulee :
quelques milliers d'instances d'une meme strie (UInstancedStaticMeshComponent pose par
AAnastasisWorldAtmosphere), toutes placees par le materiau, en HLSL, a chaque image :
  - chaque instance porte une position de depart dans [0,1)^3 (PerInstanceCustomData 0..2) ;
  - elle tombe a la vitesse Fall, poussee par le vent (Wind.xy, en uu/s) ;
  - sa position est REPLIEE dans une boite (Box) centree sur la camera : la pluie entoure
    toujours l'oeil, reste fixe dans le monde quand la camera bouge, et ne s'epuise jamais ;
  - la strie est un ruban orient le long de sa vitesse, tourne vers la camera.
L'intensite (RainAmount) eteint une part des instances (PerInstanceRandom) : 0 = aucune,
1 = toutes. Le C++ ne fait que poser RainAmount, Wind et Fall sur une instance dynamique.

LUMIERE. Translucide ECLAIRE (volume de lumiere translucide, non directionnel) : le soleil,
le ciel et la nuit eclairent la pluie comme le reste, sous l'exposition fixe (ECL-01) ;
aucune couleur n'est codee par heure.

MAILLAGE. Copie du plan de /Engine/BasicShapes (100 x 100 uu, UV 0..1), avec une extension de
bornes de 4 km : les instances sont posees a l'origine de l'acteur mais dessinees autour de la
camera ; sans cette extension, le moteur les eliminerait hors champ.

Variables d'environnement :
    ANASTASIS_RAIN_REBUILD  "1" vide le graphe du materiau et le regenere (meme asset)
"""
import os
import unreal

PKG = '/Game/Anastasis/Weather'
NAME = 'M_AnastasisRain'
PATH = PKG + '/' + NAME
MESH = PKG + '/SM_AnastasisRainStreak'
mel = unreal.MaterialEditingLibrary
BOUNDS_UU = 400000.0

# Decalage de sommet (WPO), espace monde. UV0 du plan : x = travers, y = long de la strie.
WPO_HLSL = r'''
float3 box = max(Box.xyz, float3(100.0, 100.0, 100.0));
float3 vel = float3(Wind.x, Wind.y, -max(Fall, 1.0));
float3 p = D * box + vel * T;
float3 rel = p - Cam;
rel = rel - box * floor(rel / box + 0.5);
float3 c = Cam + rel;
float3 dir = normalize(vel);
float3 side = cross(dir, Cam - c);
float sl = length(side);
side = sl > 0.001 ? side / sl : float3(1.0, 0.0, 0.0);
float2 l = UV - 0.5;
float3 pos = c + side * (l.x * Width) - dir * (l.y * Len);
return pos - WPos;
'''

# Opacite : instance allumee si son tirage est sous RainAmount ; profil doux en travers et
# en long ; fondu pres de l'oeil (pas de strie geante sur l'objectif) et au bord de la
# boite (une strie repliee n'apparait pas d'un coup). Fondu proche de 1,2 a 3,2 m : a 40 cm,
# une strie de 90 cm traversait la moitie de l'ecran (rain-ab2, 2026-10-02).
OPACITY_HLSL = r'''
float on = Rnd < Amount ? 1.0 : 0.0;
float across = pow(saturate(1.0 - abs(UV.x * 2.0 - 1.0)), 1.5);
float along = sin(saturate(UV.y) * 3.14159265);
float d = length(WPos - Cam);
float nearFade = saturate((d - 120.0) / 200.0);
float farFade = 1.0 - saturate((d - 0.38 * Box.x) / (0.10 * Box.x));
return on * across * along * nearFade * farFade * Base;
'''


def custom(mat, code, name, out_type, inputs, x, y):
    node = mel.create_material_expression(mat, unreal.MaterialExpressionCustom, x, y)
    node.set_editor_property('code', code)
    node.set_editor_property('description', name)
    node.set_editor_property('output_type', out_type)
    ins = []
    for input_name in inputs:
        ci = unreal.CustomInput()
        ci.set_editor_property('input_name', input_name)
        ins.append(ci)
    node.set_editor_property('inputs', ins)
    return node


def vparam(mat, name, value, x, y):
    node = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, x, y)
    node.set_editor_property('parameter_name', name)
    node.set_editor_property('default_value', unreal.LinearColor(*value))
    return node


def sparam(mat, name, value, x, y):
    node = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, x, y)
    node.set_editor_property('parameter_name', name)
    node.set_editor_property('default_value', value)
    return node


def build_mesh():
    if unreal.EditorAssetLibrary.does_asset_exist(MESH):
        if os.environ.get('ANASTASIS_RAIN_REBUILD', '0') != '1':
            unreal.log('RAIN_MESH_PRESENT ' + MESH)
            return
        mesh = unreal.EditorAssetLibrary.load_asset(MESH)
    else:
        mesh = unreal.EditorAssetLibrary.duplicate_asset('/Engine/BasicShapes/Plane', MESH)
        if mesh is None:
            raise RuntimeError('duplication du plan moteur impossible')
    ext = unreal.Vector(BOUNDS_UU, BOUNDS_UU, BOUNDS_UU)
    mesh.set_editor_property('positive_bounds_extension', ext)
    mesh.set_editor_property('negative_bounds_extension', ext)
    unreal.EditorAssetLibrary.save_asset(MESH)
    unreal.log('RAIN_MESH_SAVED %s bounds_extension=%.0f' % (MESH, BOUNDS_UU))


def build(mat=None):
    if mat is None:
        mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            NAME, PKG, unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
    mat.set_editor_property('two_sided', True)
    mat.set_editor_property('translucency_lighting_mode',
                            unreal.TranslucencyLightingMode.TLM_VOLUMETRIC_NON_DIRECTIONAL)
    mat.set_editor_property('used_with_instanced_static_meshes', True)
    wiring = {}

    def link(tag, src, src_pin, dst, dst_pin):
        wiring[tag] = mel.connect_material_expressions(src, src_pin, dst, dst_pin)

    uv = mel.create_material_expression(mat, unreal.MaterialExpressionTextureCoordinate, -1400, -200)
    wpos = mel.create_material_expression(mat, unreal.MaterialExpressionWorldPosition, -1400, -120)
    cam = mel.create_material_expression(mat, unreal.MaterialExpressionCameraPositionWS, -1400, -40)
    time = mel.create_material_expression(mat, unreal.MaterialExpressionTime, -1400, 40)
    data = []
    for i in range(3):
        node = mel.create_material_expression(mat, unreal.MaterialExpressionPerInstanceCustomData, -1400, 120 + 70 * i)
        node.set_editor_property('data_index', i)
        data.append(node)
    xy = mel.create_material_expression(mat, unreal.MaterialExpressionAppendVector, -1150, 140)
    link('d_xy_a', data[0], '', xy, 'A')
    link('d_xy_b', data[1], '', xy, 'B')
    xyz = mel.create_material_expression(mat, unreal.MaterialExpressionAppendVector, -1000, 160)
    link('d_xyz_a', xy, '', xyz, 'A')
    link('d_xyz_b', data[2], '', xyz, 'B')

    box = vparam(mat, 'Box', (2400.0, 2400.0, 1400.0, 0.0), -1400, 360)
    wind = vparam(mat, 'Wind', (0.0, 0.0, 0.0, 0.0), -1400, 460)
    fall = sparam(mat, 'Fall', 900.0, -1400, 560)
    length = sparam(mat, 'Len', 90.0, -1400, 620)
    width = sparam(mat, 'Width', 2.5, -1400, 680)
    amount = sparam(mat, 'RainAmount', 0.0, -1400, 760)
    base = sparam(mat, 'Opacity', 0.6, -1400, 820)
    rnd = mel.create_material_expression(mat, unreal.MaterialExpressionPerInstanceRandom, -1400, 880)

    wpo = custom(mat, WPO_HLSL, 'RainPlacement', unreal.CustomMaterialOutputType.CMOT_FLOAT3,
                 ('UV', 'WPos', 'Cam', 'T', 'D', 'Box', 'Wind', 'Fall', 'Len', 'Width'), -700, 0)
    for pin, src in (('UV', uv), ('WPos', wpos), ('Cam', cam), ('T', time), ('D', xyz), ('Box', box),
                     ('Wind', wind), ('Fall', fall), ('Len', length), ('Width', width)):
        link('wpo_' + pin, src, '', wpo, pin)
    wiring['wpo_out'] = mel.connect_material_property(wpo, '', unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)

    opacity = custom(mat, OPACITY_HLSL, 'RainOpacity', unreal.CustomMaterialOutputType.CMOT_FLOAT1,
                     ('UV', 'WPos', 'Cam', 'Box', 'Rnd', 'Amount', 'Base'), -700, 500)
    for pin, src in (('UV', uv), ('WPos', wpos), ('Cam', cam), ('Box', box), ('Rnd', rnd),
                     ('Amount', amount), ('Base', base)):
        link('op_' + pin, src, '', opacity, pin)
    wiring['opacity_out'] = mel.connect_material_property(opacity, '', unreal.MaterialProperty.MP_OPACITY)

    # Eau claire : albedo moyen, peu rugueuse. La lumiere de la scene fait le reste.
    color = vparam(mat, 'RainColor', (0.55, 0.60, 0.66, 1.0), -400, 700)
    wiring['base_color'] = mel.connect_material_property(color, '', unreal.MaterialProperty.MP_BASE_COLOR)
    rough = sparam(mat, 'Roughness', 0.25, -400, 800)
    wiring['roughness'] = mel.connect_material_property(rough, '', unreal.MaterialProperty.MP_ROUGHNESS)

    unreal.log('RAIN_MATERIAL_WIRING ' + ' '.join('%s=%s' % (k, wiring[k]) for k in sorted(wiring)))
    if not all(wiring.values()):
        unreal.log_error('RAIN_MATERIAL_WIRING_INCOMPLETE ' + repr(wiring))
    # Marqueur AVANT la compilation : un echec de compilation n'est qu'un Warning dans le log ;
    # rain-material.ps1 cherche les erreurs qui le suivent et refuse le materiau.
    unreal.log('RAIN_MATERIAL_COMPILE ' + PATH)
    mel.recompile_material(mat)
    unreal.EditorAssetLibrary.save_asset(PATH)
    unreal.log('RAIN_MATERIAL_SAVED ' + PATH)


try:
    build_mesh()
    if not unreal.EditorAssetLibrary.does_asset_exist(PATH):
        build()
    elif os.environ.get('ANASTASIS_RAIN_REBUILD', '0') == '1':
        existing = unreal.EditorAssetLibrary.load_asset(PATH)
        mel.delete_all_material_expressions(existing)
        unreal.log('RAIN_MATERIAL_CLEARED ' + PATH)
        build(existing)
    else:
        unreal.log('RAIN_MATERIAL_PRESENT ' + PATH)
except Exception as e:
    unreal.log_error('RAIN_MATERIAL_FAIL %r' % e)

unreal.log('RAIN_MATERIAL_DONE material=%s mesh=%s' % (unreal.EditorAssetLibrary.does_asset_exist(PATH),
                                                        unreal.EditorAssetLibrary.does_asset_exist(MESH)))
unreal.SystemLibrary.quit_editor()
