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
    UV0.xy  distance le long du cours, travers -1 rive gauche .. +1 rive droite.
    UV1.x   courbure signee [-1,1] (positif = virage a gauche).
    UV2.xy  sens du courant x vitesse normalisee [0,1] ; nul sur les lacs et la mer.
    UV3.xy  profondeur au centre en metres, pente de la surface. 0 = inconnu.

RIVER_LOOK_001. Les lacs (UV2 nul) gardent les cinq vagues d'avant. Les rivieres
ajoutent un profil de vitesse (centre plus vite que les rives, exterieur de virage
un peu plus vite), des traines alignees sur le courant, une absorption plus faible
la ou l'eau est peu profonde, et de la mousse seulement la ou le courant, la pente
ou le virage la justifient. Aucune couleur n'est codee par heure : le ciel et la
lumiere restent ceux de la scene.

WATER_VOLUME_001. Le pin Opacity est branche (ecume, 0 ailleurs) : sans cela le moteur saute
tout le volume de Single Layer Water (voir le commentaire OPACITY dans build()). Les coefficients
passent en centimetres (CoefToCm). Parametre VolumeOn 0 = l'eau d'avant, pour les A/B.

Repli : anastasis.Terrain.WaterLook 0 rend l'eau avec M_AnastasisShoreWater, inchangee.

Variables d'environnement :
    ANASTASIS_WATER_REBUILD  "1" vide le graphe du materiau et le regenere (meme asset)
"""
import os
import unreal
import sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import weather_materials


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
# float4 : normale monde xyz, mousse en w. Les lacs (courant nul) sortent tot, sur
# exactement les cinq vagues d'avant : le cout et le reflet des plans d'eau ne bougent pas.
NORMAL_HLSL = r'''
float2 p = P.xy * 0.01;
float spd = saturate(length(Flow));
float ph0 = frac(T * 0.18);
float ph1 = frac(T * 0.18 + 0.5);
float w0 = 1.0 - abs(1.0 - 2.0 * ph0);
float w1 = 1.0 - abs(1.0 - 2.0 * ph1);
if (spd < 0.012)
{
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
    return float4(normalize(float3(-g, 1.0)), 0);
}

float2 dir = Flow / spd;
float2 perp = float2(-dir.y, dir.x);
float across = clamp(Along.y, -1.0, 1.0);
float center = saturate(1.0 - abs(across));
float curv = clamp(Curve.x, -1.0, 1.0);
float outer = saturate(curv * across);
float inner = saturate(-curv * across);
float profile = lerp(0.34, 1.0, pow(center, 0.55));
profile *= lerp(1.0, 1.16, outer);
profile *= lerp(1.0, 0.80, inner);
float depthM = Body.x;
float slope = max(Body.y, 0.0);
float known = smoothstep(0.05, 0.20, depthM);

float2 adv = Flow * (7.0 * profile);
float2 q0 = p - adv * 0.45 * ph0;
float2 q1 = p - adv * 0.45 * ph1 + float2(17.3, 5.1);
float2 f0 = p - adv * 1.35 * ph0;
float2 f1 = p - adv * 1.35 * ph1 + float2(4.2, 9.7);
float ampL = Calm * (1.15 - 0.40 * spd);
float ampS = Calm * (0.15 + 1.15 * spd) * lerp(0.55, 1.0, center);
float2 g = 0;
float2 d; float k; float w;

d = normalize(dir * 0.45 + float2(0.86, -0.51)); k = 1.20; w = 0.35 * sqrt(9.81 * k);
g += d * ampL * 0.85 * (w0 * cos(k * dot(d, q0) - w * T + 4.0) + w1 * cos(k * dot(d, q1) - w * T + 4.0));
d = normalize(dir * 0.30 + float2(-0.45, 0.89)); k = 2.10; w = 0.40 * sqrt(9.81 * k);
g += d * ampL * 0.65 * (w0 * cos(k * dot(d, q0) - w * T + 2.1) + w1 * cos(k * dot(d, q1) - w * T + 2.1));
d = normalize(dir * 0.70 + perp * 0.25); k = 3.40; w = 0.50 * sqrt(9.81 * k);
g += d * ampL * 0.40 * (w0 * cos(k * dot(d, q0) - w * T + 1.1) + w1 * cos(k * dot(d, q1) - w * T + 1.1));

d = normalize(perp * 0.72 + dir * 0.55); k = 4.8;
g += d * ampS * 0.34 * (w0 * cos(k * dot(d, f0) - 1.6 * T) + w1 * cos(k * dot(d, f1) - 1.6 * T));
d = normalize(-perp * 0.55 + dir * 0.70); k = 7.6;
g += d * ampS * 0.18 * (w0 * cos(k * dot(d, f0) - 2.4 * T + 1.7) + w1 * cos(k * dot(d, f1) - 2.4 * T + 1.7));

float riffle = known * saturate((spd * profile - 0.28) / 0.40) * saturate((1.25 - depthM) / 0.85);
float bank = smoothstep(0.78, 0.98, abs(across)) * saturate(spd * profile);
float bend = smoothstep(0.22, 0.70, abs(curv)) * outer * saturate(spd * 1.6);
float steep = known * smoothstep(0.012, 0.040, slope) * saturate(spd * 2.0) * saturate((1.8 - depthM) / 1.1);
float patch = saturate(0.50 + 0.50 * sin(dot(dir, f0) * 0.55) * sin(dot(perp, f0) * 1.15 + 1.3));
float foam = saturate(pow(saturate((riffle * 0.42 + bank * 0.10 + bend * 0.38 + steep * 0.28) * patch), 1.8));
g += perp * foam * Calm * 6.0 * sin(dot(dir, f0) * 14.0);
return float4(normalize(float3(-g, 1.0)), foam);
'''

# float2 : faible profondeur (x), supplement de rugosite hors mousse (y).
SHADE_HLSL = r'''
float spd = saturate(length(Flow));
if (spd < 0.012) return float2(0, 0);
float across = clamp(Along.y, -1.0, 1.0);
float center = saturate(1.0 - abs(across));
float curv = clamp(Curve.x, -1.0, 1.0);
float inner = saturate(-curv * across);
float depthM = Body.x;
float known = smoothstep(0.05, 0.20, depthM);
float shoal = known * saturate((1.15 - depthM) / 1.0);
float edge = smoothstep(0.28, 0.90, abs(across));
float shallowness = saturate(shoal * lerp(0.28, 0.72, edge) + edge * 0.22 + inner * 0.22);
float roughAdd = (1.0 - center) * 0.016 + inner * 0.008;
return float2(shallowness, roughAdd);
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

    def link(tag, src, src_pin, dst, dst_pin):
        wiring[tag] = mel.connect_material_expressions(src, src_pin, dst, dst_pin)
        return dst

    def coord(index, x, y):
        node = mel.create_material_expression(mat, unreal.MaterialExpressionTextureCoordinate, x, y)
        node.set_editor_property('coordinate_index', index)
        return node

    def mask(src, r, g, b, a, x, y):
        node = mel.create_material_expression(mat, unreal.MaterialExpressionComponentMask, x, y)
        node.set_editor_property('r', r)
        node.set_editor_property('g', g)
        node.set_editor_property('b', b)
        node.set_editor_property('a', a)
        link('mask_%d_%d' % (x, y), src, '', node, '')
        return node

    def mul(a, b, x, y):
        node = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, x, y)
        link('mulA_%d_%d' % (x, y), a, '', node, 'A')
        link('mulB_%d_%d' % (x, y), b, '', node, 'B')
        return node

    def add(a, b, x, y):
        node = mel.create_material_expression(mat, unreal.MaterialExpressionAdd, x, y)
        link('addA_%d_%d' % (x, y), a, '', node, 'A')
        link('addB_%d_%d' % (x, y), b, '', node, 'B')
        return node

    def lerp(a, b, alpha, x, y):
        node = mel.create_material_expression(mat, unreal.MaterialExpressionLinearInterpolate, x, y)
        link('lerpA_%d_%d' % (x, y), a, '', node, 'A')
        link('lerpB_%d_%d' % (x, y), b, '', node, 'B')
        link('lerpT_%d_%d' % (x, y), alpha, '', node, 'Alpha')
        return node

    def sat(a, x, y):
        node = mel.create_material_expression(mat, unreal.MaterialExpressionSaturate, x, y)
        link('sat_%d_%d' % (x, y), a, '', node, '')
        return node

    wpos = mel.create_material_expression(mat, unreal.MaterialExpressionWorldPosition, -1600, 0)
    time = mel.create_material_expression(mat, unreal.MaterialExpressionTime, -1600, 80)
    uv0 = coord(0, -1600, 180)
    uv1 = coord(1, -1600, 280)
    uv2 = coord(2, -1600, 380)
    uv3 = coord(3, -1600, 480)
    calm = sparam(mat, 'WaveSlope', 0.05, -1600, 600)
    field_inputs = ('P', 'T', 'Flow', 'Along', 'Curve', 'Body', 'Calm')

    normal = custom(mat, NORMAL_HLSL, 'WaterNormal', unreal.CustomMaterialOutputType.CMOT_FLOAT4,
                    field_inputs, -1100, 80)
    link('n_p', wpos, '', normal, 'P')
    link('n_t', time, '', normal, 'T')
    link('n_flow', uv2, '', normal, 'Flow')
    link('n_along', uv0, '', normal, 'Along')
    link('n_curve', uv1, '', normal, 'Curve')
    link('n_body', uv3, '', normal, 'Body')
    link('n_calm', calm, '', normal, 'Calm')
    normal_rgb = mask(normal, True, True, True, False, -700, 0)
    foam = mask(normal, False, False, False, True, -700, 120)
    wiring['normal'] = mel.connect_material_property(normal_rgb, '', unreal.MaterialProperty.MP_NORMAL)
    weather_materials.water_normal(mat, normal, uv2, wpos, time)

    shade = custom(mat, SHADE_HLSL, 'WaterShade', unreal.CustomMaterialOutputType.CMOT_FLOAT2,
                   ('Flow', 'Along', 'Curve', 'Body'), -1100, 420)
    link('s_flow', uv2, '', shade, 'Flow')
    link('s_along', uv0, '', shade, 'Along')
    link('s_curve', uv1, '', shade, 'Curve')
    link('s_body', uv3, '', shade, 'Body')
    shallowness = mask(shade, True, False, False, False, -700, 400)
    rough_add = mask(shade, False, True, False, False, -700, 480)

    base = vparam(mat, 'SurfaceColor', (0.02, 0.045, 0.05, 1.0), -700, -280)
    shallow = vparam(mat, 'ShallowColor', (0.11, 0.125, 0.075, 1.0), -700, -160)
    foam_tint = vparam(mat, 'FoamTint', (0.58, 0.60, 0.56, 1.0), -700, -40)
    shallow_blend = sparam(mat, 'ShallowBlend', 0.30, -400, -220)
    foam_amount = sparam(mat, 'FoamTintAmount', 0.28, -400, -140)
    base_shallow = lerp(base, shallow, mul(shallowness, shallow_blend, -200, -200), 0, -200)
    base_final = lerp(base_shallow, foam_tint, mul(foam, foam_amount, -200, -80), 200, -140)

    # WATER_VOLUME_001 -- OPACITY. Single Layer Water ne calcule son volume (absorption, diffusion,
    # fond lu, refraction) que si WaterVisibility = 1 - Opacity > 0 (BasePassPixelShader.usf:1141,
    # SingleLayerWaterShading.ush:74), et ne garde de la BaseColor que Opacity x diffuse
    # (BasePassPixelShader.usf:1383). Opacity vaut 1 quand rien n'y est branche : jusqu'ici l'eau
    # n'avait donc AUCUN volume, la couleur etait la teinte peinte ci-dessus et les coefficients
    # d'absorption et de diffusion ne servaient a rien (RIVER_LOOK_001, sonde « surface rouge »).
    # Opacity est la couverture d'une pellicule de surface : ici l'ecume, 0 ailleurs, et la
    # BaseColor de cette pellicule est la teinte d'ecume. VolumeOn 0 redonne l'eau d'avant
    # (Opacity 1, teinte peinte) : c'est l'etat « avant » des A/B de riverbank-capture.py.
    volume_on = sparam(mat, 'VolumeOn', 1.0, -1000, 1100)
    foam_cover = sparam(mat, 'FoamCoverage', 0.9, -700, 1100)
    opaque = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -700, 1180)
    opaque.set_editor_property('r', 1.0)
    opacity = lerp(opaque, mul(foam, foam_cover, -400, 1100), volume_on, -200, 1100)
    wiring['opacity'] = mel.connect_material_property(opacity, '', unreal.MaterialProperty.MP_OPACITY)
    base_color = lerp(base_final, foam_tint, volume_on, 400, -140)
    wiring['base_color'] = mel.connect_material_property(base_color, '', unreal.MaterialProperty.MP_BASE_COLOR)

    rough = sparam(mat, 'Roughness', 0.035, -400, 40)
    foam_rough = sparam(mat, 'FoamRoughness', 0.26, -400, 120)
    rough_final = sat(add(add(rough, mul(foam, foam_rough, 0, 80), 200, 40), rough_add, 400, 80), 560, 60)
    wiring['roughness'] = mel.connect_material_property(rough_final, '', unreal.MaterialProperty.MP_ROUGHNESS)

    # Eau : indice 1,33, F0 = ((1,33 - 1) / (1,33 + 1))^2 = 0,0201, soit Specular = F0 / 0,08 = 0,25.
    # 0,5 (F0 0,04) est l'indice 1,5 du verre ; Single Layer Water en tire aussi son indice de
    # refraction (SingleLayerWaterShading.ush:96). L'etat « avant » de l'A/B repose 0,5.
    spec = sparam(mat, 'Specular', 0.25, -400, 220)
    spec_on_foam = sparam(mat, 'SpecularOnFoam', 0.45, -400, 300)
    spec_final = lerp(spec, mul(spec, spec_on_foam, 0, 260), foam, 220, 240)
    wiring['specular'] = mel.connect_material_property(spec_final, '', unreal.MaterialProperty.MP_SPECULAR)

    # Coefficients par metre. Au large : absorption d'avant (l'eau verdit puis bleuit).
    # En eau peu profonde : absorption plus faible, diffusion un peu plus forte, le fond
    # reste lisible. Rien de tout cela ne depend de l'heure.
    # Le modele Single Layer Water n'accepte qu'un seul noeud de sortie. Une regeneration
    # qui en laisserait un et en ajouterait un second fait tomber le materiau sur le
    # damier par defaut en jeu (vu en SM6 : "can contain only one").
    removed = 0
    for expr in list(mel.get_material_expressions(mat)):
        if expr is not None and expr.get_class().get_name() == 'MaterialExpressionSingleLayerWaterMaterialOutput':
            mel.delete_material_expression(mat, expr)
            removed += 1
    unreal.log('WATER_MATERIAL_SLW_REMOVED %d' % removed)
    out = mel.create_material_expression(mat, unreal.MaterialExpressionSingleLayerWaterMaterialOutput, 700, 420)
    left = 0
    for expr in list(mel.get_material_expressions(mat)):
        if expr is not None and expr.get_class().get_name() == 'MaterialExpressionSingleLayerWaterMaterialOutput':
            left += 1
    unreal.log('WATER_MATERIAL_SLW_COUNT %d' % left)
    if left != 1:
        unreal.log_error('WATER_MATERIAL_SLW_COUNT_BAD %d' % left)
    # WATER_VOLUME_001, palette « trouble » choisie par Alexandre (2026-10-10) : rivière chargée de
    # limon, vert-brun, le bleu absorbé plus que le vert. Les anciennes valeurs (0,55 / 0,16 / 0,11 et
    # 0,015 / 0,040 / 0,048) donnaient du turquoise une fois le volume actif.
    absorption = vparam(mat, 'Absorption', (0.45, 0.22, 0.40, 1.0), -400, 420)
    scattering = vparam(mat, 'Scattering', (0.045, 0.045, 0.025, 1.0), -400, 540)
    phase = sparam(mat, 'PhaseG', 0.1, -400, 660)
    behind = sparam(mat, 'ColorScaleBehindWater', 1.0, -400, 740)
    shallow_absorb = sparam(mat, 'ShallowAbsorb', 0.38, -400, 820)
    shallow_scatter = sparam(mat, 'ShallowScatter', 1.65, -400, 900)
    shallow_behind = sparam(mat, 'ShallowBehind', 1.12, -400, 980)
    # Le moteur multiplie l'extinction par l'epaisseur en CENTIMETRES (SingleLayerWaterShading.ush:221,
    # profondeur de scene) : les coefficients ci-dessus, par metre, passent par CoefToCm = 0,01.
    cm = sparam(mat, 'CoefToCm', 0.01, -400, 1000)
    absorption_out = mul(lerp(absorption, mul(absorption, shallow_absorb, 0, 460), shallowness, 250, 440), cm, 400, 440)
    scattering_out = mul(lerp(scattering, mul(scattering, shallow_scatter, 0, 580), shallowness, 250, 560), cm, 400, 560)
    behind_out = lerp(behind, shallow_behind, shallowness, 250, 700)
    link('absorption', absorption_out, '', out, 'AbsorptionCoefficients')
    link('scattering', scattering_out, '', out, 'ScatteringCoefficients')
    link('phase', phase, '', out, 'PhaseG')
    link('behind', behind_out, '', out, 'ColorScaleBehindWater')

    unreal.log('WATER_MATERIAL_WIRING ' + ' '.join('%s=%s' % (k, wiring[k]) for k in sorted(wiring)))
    if not all(wiring.values()):
        unreal.log_error('WATER_MATERIAL_WIRING_INCOMPLETE ' + repr(wiring))
    # Marqueur AVANT la compilation : un echec de compilation n'est qu'un Warning dans le
    # log ; water-look.ps1 cherche les erreurs qui le suivent et refuse le materiau.
    unreal.log('WATER_MATERIAL_COMPILE ' + PATH)
    mel.recompile_material(mat)
    if not unreal.EditorAssetLibrary.save_asset(PATH):
        raise RuntimeError('Water material save failed: ' + PATH)
    unreal.log('WATER_MATERIAL_SAVED ' + PATH)


# Regeneration EN PLACE : si un package tient deja le materiau (le terrain de
# Lvl_AnastasisSlice, quand l'editeur s'ouvre dessus), delete_asset echoue sans bruit. On vide
# son graphe et on le recable -- meme asset, memes references.
if __name__ == '__main__':
    try:
        if not unreal.EditorAssetLibrary.does_asset_exist(PATH):
            build()
        elif os.environ.get('ANASTASIS_WATER_REBUILD', '0') == '1':
            existing = unreal.EditorAssetLibrary.load_asset(PATH)
            # UE 5.8 DeleteAllMaterialExpressions iterates the live expression array
            # while DeleteMaterialExpression removes entries from that same array.
            # Delete a snapshot so regeneration cannot retain an old WaterNormal.
            for expression in list(mel.get_material_expressions(existing)):
                mel.delete_material_expression(existing, expression)
            remaining = list(mel.get_material_expressions(existing))
            if remaining:
                raise RuntimeError('Water material clear incomplete: %d expressions' % len(remaining))
            unreal.log('WATER_MATERIAL_CLEARED ' + PATH)
            build(existing)
        else:
            unreal.log('WATER_MATERIAL_PRESENT ' + PATH)
    except Exception as e:
        unreal.log_error('WATER_MATERIAL_FAIL %r' % e)

    unreal.log('WATER_MATERIAL_DONE exists=%s' % unreal.EditorAssetLibrary.does_asset_exist(PATH))
    unreal.SystemLibrary.quit_editor()
