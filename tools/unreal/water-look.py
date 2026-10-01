"""WATER_LOOK_001 -- materiau d'eau Single Layer Water.
PROPRIETE DE L'ASSET. Ce script est la SOURCE D'AUTORITE de
/Game/Anastasis/Materials/M_AnastasisWater. Il le cree, puis ne le modifie plus :
le regenerer est explicite (ANASTASIS_WATER_REBUILD=1) et ecrase toute retouche faite
a la main dans l'editeur. Lance par tools/unreal/water-look.ps1.

POURQUOI. M_AnastasisShoreWater (SHORELINE_FORGE_001) est translucide a 0.88-0.96,
d'une seule couleur, sans reflet ni mouvement : vue d'en haut, l'eau se lit comme de la
peinture posee sur l'herbe. Ce materiau-ci utilise le modele d'ombrage Single Layer
Water d'Unreal, sur n'importe quel maillage, sans le plugin Water :
  - absorption et diffusion selon la profondeur REELLEMENT vue (le fond rendu) :
    clair au bord, sombre au large -- la profondeur se lit sans canal de sommet ;
  - reflets du ciel (Fresnel), rugosite faible ;
  - normales animees, calculees en HLSL (aucune texture) : vagues directionnelles,
    advectees par le courant en flowmap a deux phases.

ENTREES (ecrites par AnastasisDrainage::BuildRiverRibbons) :
    UV2.xy  sens du courant x vitesse normalisee [0,1] ; nul sur les lacs et la mer.

Repli : anastasis.Terrain.WaterLook 0 rend l'eau avec M_AnastasisShoreWater, inchangee.

Variables d'environnement :
    ANASTASIS_WATER_REBUILD  "1" vide le graphe du materiau et le regenere (meme asset)
"""
import os
import unreal

PKG = '/Game/Anastasis/Materials'
NAME = 'M_AnastasisWater'
PATH = PKG + '/' + NAME
mel = unreal.MaterialEditingLibrary

# Normale MONDE (tangent_space_normal = False : les maillages proceduraux n'ont pas de
# tangentes). Hauteur = somme de vagues directionnelles ; on n'en garde que le gradient.
# Longueurs d'onde 0.8 a 5.5 m, pulsation de la dispersion en eau profonde (ralentie de
# 40 % : une riviere de jeu, pas un ocean). Flowmap a deux phases : deux echantillons
# advectes par le courant, decales d'une demi-periode et fondus en triangle -- le motif
# coule sans s'etirer a l'infini.
NORMAL_HLSL = r'''
float2 p = P.xy * 0.01;
float spd = saturate(length(Flow));
float ph0 = frac(T * 0.18);
float ph1 = frac(T * 0.18 + 0.5);
float w0 = 1.0 - abs(1.0 - 2.0 * ph0);
float w1 = 1.0 - abs(1.0 - 2.0 * ph1);
float2 adv = Flow * 7.0;
float2 q0 = p - adv * ph0;
float2 q1 = p - adv * ph1 + float2(17.3, 5.1);
float amp = Calm * (1.0 + 2.2 * spd);
float2 g = 0;
float2 d; float k; float w;
d = float2(0.97, 0.24);  k = 7.85; w = 0.6 * sqrt(9.81 * k);
g += d * amp * 0.35 * (w0 * cos(k * dot(d, q0) - w * T) + w1 * cos(k * dot(d, q1) - w * T));
d = float2(0.62, 0.78);  k = 4.83; w = 0.6 * sqrt(9.81 * k);
g += d * amp * 0.55 * (w0 * cos(k * dot(d, q0) - w * T + 1.3) + w1 * cos(k * dot(d, q1) - w * T + 1.3));
d = float2(-0.45, 0.89); k = 2.99; w = 0.6 * sqrt(9.81 * k);
g += d * amp * 0.70 * (w0 * cos(k * dot(d, q0) - w * T + 2.1) + w1 * cos(k * dot(d, q1) - w * T + 2.1));
d = float2(0.86, -0.51); k = 1.85; w = 0.6 * sqrt(9.81 * k);
g += d * amp * 0.80 * (w0 * cos(k * dot(d, q0) - w * T + 4.0) + w1 * cos(k * dot(d, q1) - w * T + 4.0));
d = float2(-0.99, -0.12); k = 1.14; w = 0.6 * sqrt(9.81 * k);
g += d * amp * 0.60 * (w0 * cos(k * dot(d, q0) - w * T + 5.2) + w1 * cos(k * dot(d, q1) - w * T + 5.2));
return normalize(float3(-g, 1.0));
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


def build(mat=None):
    if mat is None:
        mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            NAME, PKG, unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property('blend_mode', unreal.BlendMode.BLEND_OPAQUE)
    mat.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_SINGLE_LAYER_WATER)
    mat.set_editor_property('tangent_space_normal', False)
    mat.set_editor_property('two_sided', False)

    wiring = {}
    wpos = mel.create_material_expression(mat, unreal.MaterialExpressionWorldPosition, -1200, 0)
    time = mel.create_material_expression(mat, unreal.MaterialExpressionTime, -1200, 120)
    uv2 = mel.create_material_expression(mat, unreal.MaterialExpressionTextureCoordinate, -1200, 240)
    uv2.set_editor_property('coordinate_index', 2)
    calm = sparam(mat, 'WaveSlope', 0.05, -1200, 360)

    normal = custom(mat, NORMAL_HLSL, 'WaterNormal', unreal.CustomMaterialOutputType.CMOT_FLOAT3,
                    ('P', 'T', 'Flow', 'Calm'), -800, 120)
    wiring['n_p'] = mel.connect_material_expressions(wpos, '', normal, 'P')
    wiring['n_t'] = mel.connect_material_expressions(time, '', normal, 'T')
    wiring['n_flow'] = mel.connect_material_expressions(uv2, '', normal, 'Flow')
    wiring['n_calm'] = mel.connect_material_expressions(calm, '', normal, 'Calm')
    wiring['normal'] = mel.connect_material_property(normal, '', unreal.MaterialProperty.MP_NORMAL)

    base = vparam(mat, 'SurfaceColor', (0.02, 0.045, 0.05, 1.0), -500, -300)
    rough = sparam(mat, 'Roughness', 0.035, -500, -180)
    spec = sparam(mat, 'Specular', 0.5, -500, -100)
    wiring['base_color'] = mel.connect_material_property(base, '', unreal.MaterialProperty.MP_BASE_COLOR)
    wiring['roughness'] = mel.connect_material_property(rough, '', unreal.MaterialProperty.MP_ROUGHNESS)
    wiring['specular'] = mel.connect_material_property(spec, '', unreal.MaterialProperty.MP_SPECULAR)

    # Coefficients par metre. Absorption forte dans le rouge : l'eau verdit puis bleuit
    # avec la profondeur ; diffusion faible et bleu-vert : une eau douce un peu chargee.
    out = mel.create_material_expression(mat, unreal.MaterialExpressionSingleLayerWaterMaterialOutput, -100, 300)
    absorption = vparam(mat, 'Absorption', (0.55, 0.16, 0.11, 1.0), -500, 260)
    scattering = vparam(mat, 'Scattering', (0.015, 0.040, 0.048, 1.0), -500, 380)
    phase = sparam(mat, 'PhaseG', 0.1, -500, 500)
    behind = sparam(mat, 'ColorScaleBehindWater', 1.0, -500, 580)
    wiring['absorption'] = mel.connect_material_expressions(absorption, '', out, 'AbsorptionCoefficients')
    wiring['scattering'] = mel.connect_material_expressions(scattering, '', out, 'ScatteringCoefficients')
    wiring['phase'] = mel.connect_material_expressions(phase, '', out, 'PhaseG')
    wiring['behind'] = mel.connect_material_expressions(behind, '', out, 'ColorScaleBehindWater')

    unreal.log('WATER_MATERIAL_WIRING ' + ' '.join('%s=%s' % (k, wiring[k]) for k in sorted(wiring)))
    if not all(wiring.values()):
        unreal.log_error('WATER_MATERIAL_WIRING_INCOMPLETE ' + repr(wiring))
    # Marqueur AVANT la compilation : un echec de compilation n'est qu'un Warning dans le
    # log ; water-look.ps1 cherche les erreurs qui le suivent et refuse le materiau.
    unreal.log('WATER_MATERIAL_COMPILE ' + PATH)
    mel.recompile_material(mat)
    unreal.EditorAssetLibrary.save_asset(PATH)
    unreal.log('WATER_MATERIAL_SAVED ' + PATH)


# Regeneration EN PLACE : si un package tient deja le materiau (le terrain de
# Lvl_AnastasisSlice, quand l'editeur s'ouvre dessus), delete_asset echoue sans bruit. On vide
# son graphe et on le recable -- meme asset, memes references.
try:
    if not unreal.EditorAssetLibrary.does_asset_exist(PATH):
        build()
    elif os.environ.get('ANASTASIS_WATER_REBUILD', '0') == '1':
        existing = unreal.EditorAssetLibrary.load_asset(PATH)
        mel.delete_all_material_expressions(existing)
        unreal.log('WATER_MATERIAL_CLEARED ' + PATH)
        build(existing)
    else:
        unreal.log('WATER_MATERIAL_PRESENT ' + PATH)
except Exception as e:
    unreal.log_error('WATER_MATERIAL_FAIL %r' % e)

unreal.log('WATER_MATERIAL_DONE exists=%s' % unreal.EditorAssetLibrary.does_asset_exist(PATH))
unreal.SystemLibrary.quit_editor()
