"""SHORELINE_FORGE_001 -- materiau de la nappe d'eau.

PROPRIETE DE L'ASSET. Ce script est la SOURCE D'AUTORITE de
/Game/Anastasis/Materials/M_AnastasisShoreWater. Il le cree, puis ne le modifie
plus : le regenerer est explicite (ANASTASIS_SHORE_REBUILD=1) et ecrase toute
retouche faite a la main dans l'editeur. Meme convention que observe-slice.py
pour M_AnastasisSlice et ground-material.py pour le sol.

PERIMETRE. Ce materiau ne s'applique QU'A la section 1 du ProceduralMesh, la
nappe d'eau. Le sol (section 0) appartient a GROUND_SURFACE_001 et n'est jamais
touche ici. Le relief n'est pas deplace : la nappe reste plate au niveau de la
mer et le trait de cote reste l'intersection que AnastasisTerrainSurface::Build
produisait deja. Ce qui change est ce que la nappe SAIT d'elle-meme.

ENTREES, toutes ecrites par AnastasisTerrainSurface::Build :
    UV0.x  Depth     profondeur normalisee sur ShoreDepthSpan (120 uu), [0,1]
    UV0.y  Flatness  normale Z du relief au meme sommet, [0,1]
    UV1.x  Flow      FlowAmt de la tuile source, [0,1], non nul sur les chenaux

Variables d'environnement :
    ANASTASIS_SHORE_REBUILD  "1" supprime et regenere le materiau
"""
import os
import unreal

PKG = '/Game/Anastasis/Materials'
NAME = 'M_AnastasisShoreWater'
PATH = PKG + '/' + NAME

mel = unreal.MaterialEditingLibrary

# ---------------------------------------------------------------------------
# SHORELINE_GENOME. Une seule ecriture, partagee par les deux noeuds Custom :
# deux copies divergentes du meme calcul seraient deux rives differentes.
#
# CE QUE LA NAPPE DOIT REPARER, ET CE QU'ELLE NE PEUT PAS. Le relief immerge est
# deja peint en bleu par AnastasisTerrainSurface::TileColor -- une tuile d'eau est
# de l'eau, meme vue par en dessous. La transparence ne revele donc PAS du limon :
# elle revele du bleu. C'est pourquoi la marge porte sa propre matiere, presque
# opaque, et pourquoi la transparence sert ici a une seule chose, mais la bonne :
# effacer la nappe juste avant son arete, pour que le bord d'eau cesse d'etre une
# intersection de polygones. Repeindre le fond immerge serait toucher TileColor,
# qui appartient a GROUND_SURFACE_001 : ce n'est pas fait ici.
#
# Les nombres, et pourquoi ils valent ce qu'ils valent :
#
#   marginEnd   bornes de la bande de limon, en profondeur NORMALISEE
#       (ShoreDepthSpan = 60 uu). Ces deux nombres sont cales sur la DISTRIBUTION
#       reelle des profondeurs du monde canonique, que le marqueur
#       TERRAIN_SHORELINE_DEPTHS sort a chaque execution :
#
#           p10=9  p20=14  p30=18  p40=20  p50=23  p60=27  p70=31  p80=37  p90=48
#
#       Le fond d'un lac est plat, donc presque tous les sommets de nappe ont une
#       platitude proche de 1 et prennent la borne haute. Une borne haute a 0.55
#       (33 uu) aurait donc mis SOIXANTE-DIX POUR CENT de l'eau du monde dans la
#       bande de limon : les lacs seraient bruns. 0.26 (15.6 uu) la ramene autour
#       du p20 -- la bande couvre le vrai bord d'eau et rien de plus.
#       La borne basse, 0.08 (4.8 uu), tient la bande sous le p10 sur une paroi :
#       une berge abrupte garde un trait net.
#       La largeur au SOL n'est reglee nulle part : elle sort de la pente, une
#       berge douce etalant la meme tranche de profondeur sur beaucoup plus de
#       terrain. Ces bornes ne font qu'accentuer ce que la topographie dit deja.
#
#   edge = d / 0.05    trois uu de profondeur : l'epaisseur d'eau des tout
#       derniers centimetres de rive, bien en dessous du p10. C'est la seule zone
#       ou la nappe s'efface, et c'est exactement celle ou son arete se voyait.
#
#   foamEnd     l'ourlet, pose en BANDE et non sur la ligne : son centre est a
#       mi-profondeur d'ourlet, sinon il disparaitrait dans le fondu de bord.
#       0.03 a 0.09 du span, soit 1.8 a 5.4 uu -- un vrai liseret. Une premiere
#       version montait a 0.20 (12 uu) : l'ourlet couvrait alors presque toute la
#       bande de limon et la lavait, si bien que la rive rendait un halo pale au
#       lieu de trois matieres. L'ourlet doit tenir DANS la bande, pas la remplacer.
#
#   0.22 + 0.48 * flow   une rive dormante garde un ourlet discret ; une rive de
#       chenal le porte a plus du triple. C'est le simulateur qui decide ou, via
#       FlowAmt. Ces deux nombres ont ete divises par deux apres la premiere
#       capture : le sol de ce monde est encore un platre quasi blanc sous
#       EV100 = 14 (c'est le sujet de GROUND_SURFACE_001, pas d'ici), et un ourlet
#       clair s'y fondait completement. Le signal lisible sur ce fond n'est pas une
#       ecume claire, c'est une bande SOMBRE de limon mouille.
# ---------------------------------------------------------------------------
GENOME = """
    float d     = saturate(Depth);
    float flatn = saturate(Flatness);
    float flow  = saturate(Flow);

    float marginEnd = lerp(0.10, 0.32, flatn);
    float t         = saturate(d / max(marginEnd, 1e-4));

    float edge = saturate(d / 0.07);

    float foamEnd  = lerp(0.03, 0.09, flatn);
    float foamHalf = max(foamEnd * 0.5, 1e-4);
    float band     = saturate(1.0 - abs(d - foamHalf) / foamHalf);
    float foam     = pow(band, 1.4) * saturate(0.22 + 0.48 * flow);
"""

# Albedos, pas des couleurs d'interface : ils sont lus sous 75 000 lux a
# EV100 = 14. Meme discipline que la palette de sol de GROUND_SURFACE_001.
#   SiltWet      limon gorge d'eau : la matiere de la marge dormante. Sombre et
#                chaud DELIBEREMENT -- sur un sol surexpose presque blanc, le seul
#                signal de rive qui se lit a l'echelle gameplay est une bande
#                sombre, pas un liseret clair.
#   GravelScour  galets laves d'un chenal : plus clair, plus froid, plus mineral
#   ShallowWater / DeepWater  l'eau de la NAPPE, qui n'est pas celle du relief
#                immerge. Premiere version : les teintes de TileColor telles
#                quelles. Resultat mesure a la capture -- presque toute l'eau de ce
#                monde est peu profonde (p50 = 23 uu sur un span de 60), donc
#                l'interpolation restait bloquee pres de ShallowWater et le lac
#                virait au turquoise de piscine, PLUS CLAIR que la nappe opaque
#                d'avant. Une rive ne se prouve pas en eclaircissant l'eau.
#                ShallowWater est donc ramenee pres de la couleur de sommet
#                historique (0.043, 0.176, 0.290) : le corps du lac ne change
#                presque pas, et tout le gain se joue sur la marge.
#   FoamColor    un ourlet retenu, jamais un liseret blanc de carte postale. A
#                0.52 il se confondait avec le sol surexpose ; a 0.315 il reste un
#                eclat d'eau brassee a l'interieur de la bande sombre.
BASE_COLOR_HLSL = GENOME + """
    float3 SiltWet      = float3(0.090, 0.070, 0.050);
    float3 GravelScour  = float3(0.150, 0.152, 0.142);
    float3 ShallowWater = float3(0.072, 0.196, 0.262);
    float3 DeepWater    = float3(0.016, 0.063, 0.204);
    float3 FoamColor    = float3(0.315, 0.335, 0.330);

    float3 margin = lerp(SiltWet, GravelScour, flow);
    float3 water  = lerp(ShallowWater, DeepWater, d);
    return lerp(lerp(margin, water, t), FoamColor, foam);
"""

# (Opacity, Roughness, Specular) en une passe : trois sorties du meme etat de
# rive, jamais reglees l'une sans l'autre.
#   Opacity   la marge porte sa matiere (0.88) et l'eau franche est pleine (0.96),
#             mais TOUT est multiplie par le fondu de bord : c'est lui qui supprime
#             la coupure. 0.88 et non 0.62 : sous 0.62 le bleu du relief immerge
#             traversait le limon et la bande cessait d'etre une matiere. La
#             transparence ne sert PAS a montrer le fond -- le fond est bleu -- elle
#             sert a effacer l'arete, et c'est `edge` qui s'en charge, lui seul.
#   Roughness la marge est mate et diffusante, l'eau franche speculaire. Un
#             chenal reste plus rugueux qu'un lac.
#   Specular  meme gradient : un bord d'eau ne doit pas miroiter comme un lac.
SURFACE_HLSL = GENOME + """
    float opacity   = saturate(lerp(0.88, 0.96, t) * edge + foam * 0.20);
    float roughness = saturate(lerp(0.80, 0.16, t) + flow * 0.14);
    float specular  = saturate(lerp(0.20, 1.00, t));
    return float3(opacity, roughness, specular);
"""


def make_custom(mat, code, name, out_type, x, y):
    node = mel.create_material_expression(mat, unreal.MaterialExpressionCustom, x, y)
    node.set_editor_property('code', code)
    node.set_editor_property('description', name)
    node.set_editor_property('output_type', out_type)
    # FCustomInput porte un FExpressionInput, que Python ne sait pas construire :
    # le constructeur n'accepte donc aucun argument nomme (TypeError: call() takes
    # at most 0 arguments). On instancie vide, puis on nomme.
    inputs = []
    for input_name in ('Depth', 'Flatness', 'Flow'):
        ci = unreal.CustomInput()
        ci.set_editor_property('input_name', input_name)
        inputs.append(ci)
    node.set_editor_property('inputs', inputs)
    return node


def build():
    mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        NAME, PKG, unreal.Material, unreal.MaterialFactoryNew())

    # Translucide : c'est ce qui permet a la nappe de s'effacer avant son arete,
    # donc au bord d'eau de cesser d'etre une intersection de polygones. Eclairage
    # par pixel pour que la pente du terrain immerge se lise encore sous l'eau.
    mat.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
    mat.set_editor_property(
        'translucency_lighting_mode',
        unreal.TranslucencyLightingMode.TLM_SURFACE_PER_PIXEL_LIGHTING)
    # La nappe est vue d'au-dessus et n'est jamais traversee : une seule face.
    mat.set_editor_property('two_sided', False)

    uv0 = mel.create_material_expression(mat, unreal.MaterialExpressionTextureCoordinate, -1100, -200)
    uv0.set_editor_property('coordinate_index', 0)
    uv1 = mel.create_material_expression(mat, unreal.MaterialExpressionTextureCoordinate, -1100, 60)
    uv1.set_editor_property('coordinate_index', 1)

    def mask(src, r, g, x, y):
        m = mel.create_material_expression(mat, unreal.MaterialExpressionComponentMask, x, y)
        m.set_editor_property('r', r)
        m.set_editor_property('g', g)
        m.set_editor_property('b', False)
        m.set_editor_property('a', False)
        mel.connect_material_expressions(src, '', m, '')
        return m

    depth = mask(uv0, True, False, -880, -240)     # UV0.x
    flatness = mask(uv0, False, True, -880, -140)  # UV0.y
    flow = mask(uv1, True, False, -880, 60)        # UV1.x

    wiring = {}

    def feed(node, tag):
        wiring[tag + '_depth'] = mel.connect_material_expressions(depth, '', node, 'Depth')
        wiring[tag + '_flatness'] = mel.connect_material_expressions(flatness, '', node, 'Flatness')
        wiring[tag + '_flow'] = mel.connect_material_expressions(flow, '', node, 'Flow')

    color = make_custom(mat, BASE_COLOR_HLSL, 'ShorelineBaseColor',
                        unreal.CustomMaterialOutputType.CMOT_FLOAT3, -520, -220)
    feed(color, 'color')
    wiring['base_color'] = mel.connect_material_property(color, '', unreal.MaterialProperty.MP_BASE_COLOR)

    surface = make_custom(mat, SURFACE_HLSL, 'ShorelineSurface',
                          unreal.CustomMaterialOutputType.CMOT_FLOAT3, -520, 180)
    feed(surface, 'surface')

    # (Opacity, Roughness, Specular) -> trois masques d'un seul calcul.
    opacity = mask(surface, True, False, -220, 120)
    rough = mask(surface, False, True, -220, 220)
    spec = mel.create_material_expression(mat, unreal.MaterialExpressionComponentMask, -220, 320)
    spec.set_editor_property('r', False)
    spec.set_editor_property('g', False)
    spec.set_editor_property('b', True)
    spec.set_editor_property('a', False)
    mel.connect_material_expressions(surface, '', spec, '')

    wiring['opacity'] = mel.connect_material_property(opacity, '', unreal.MaterialProperty.MP_OPACITY)
    wiring['roughness'] = mel.connect_material_property(rough, '', unreal.MaterialProperty.MP_ROUGHNESS)
    wiring['specular'] = mel.connect_material_property(spec, '', unreal.MaterialProperty.MP_SPECULAR)

    unreal.log('SHORE_MATERIAL_WIRING ' + ' '.join(
        '%s=%s' % (k, wiring[k]) for k in sorted(wiring)))
    if not all(wiring.values()):
        unreal.log_error('SHORE_MATERIAL_WIRING_INCOMPLETE ' + repr(wiring))

    mel.recompile_material(mat)
    unreal.EditorAssetLibrary.save_asset(PATH)
    unreal.log('SHORE_MATERIAL_SAVED ' + PATH)


if os.environ.get('ANASTASIS_SHORE_REBUILD', '0') == '1' and unreal.EditorAssetLibrary.does_asset_exist(PATH):
    unreal.EditorAssetLibrary.delete_asset(PATH)
    unreal.log('SHORE_MATERIAL_DELETED ' + PATH)

if unreal.EditorAssetLibrary.does_asset_exist(PATH):
    unreal.log('SHORE_MATERIAL_PRESENT ' + PATH)
else:
    build()

unreal.log('SHORE_MATERIAL_DONE exists=%s' % unreal.EditorAssetLibrary.does_asset_exist(PATH))
unreal.SystemLibrary.quit_editor()
