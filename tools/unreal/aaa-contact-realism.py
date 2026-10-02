"""AAA_CONTACT_REALISM_001 -- les materiaux de decalque de la peau de contact.
PROPRIETE DES ASSETS. Ce script est la SOURCE D'AUTORITE de /Game/Anastasis/AAAContactRealism/ :
M_ACR_Decal (le maitre) et MI_ACR_<famille> (onze instances, une par famille de decalque de
AnastasisContactRealism). Rien d'autre n'est cree, aucun asset d'un autre proprietaire n'est lu
en ecriture.

LE MAITRE. Un decalque DBuffer (couleur + normale + rugosite ; en 5.8 le mode se deduit des sorties
branchees, pas d'une propriete : DecalBlendMode est obsolete) : il se compose AVANT l'eclairage
differe, donc la lumiere, les ombres et Lumen le traitent comme du sol, pas comme une etiquette.
Il ne depend d'aucune texture : tout est calcule en HLSL dans le repere du decalque (UV 0..1).
  Pattern   masque (bruit de valeur a quatre octaves casse le disque radial : jamais un anneau,
            jamais un cercle), bruit lui-meme, son gradient (pour la normale), fondu en profondeur ;
  Speckle   semis de taches orientees (feuilles, tiges, graviers) ; densite 0 = aucun ;
  Surface   couleur et opacite ; rugosite ; normale tangente.
La graine de chaque decalque vient de la position du composant (ObjectPositionWS) : deux
decalques de la meme famille ne se ressemblent pas.

PIEGE (capture first, 2026-10-02). Un nœud VectorParameter sort son RGB (float3) : lire `.w` d'un
Custom qui le recoit ne compile pas, et le moteur remplace alors le materiau par le materiau par
defaut (rectangle noir opaque). `-nullrhi` ne compile aucun shader : ce script ne voit pas l'erreur,
seul un editeur avec rendu la voit (« Failed to compile Material ... Default Material will be used »).
contact-realism-capture.ps1 la cherche donc dans son log et refuse la capture.

LES INSTANCES. Chaque famille ne change que des valeurs (table FAMILIES) : elles sont reecrites a
CHAQUE lancement, le maitre seulement avec ANASTASIS_ACR_REBUILD=1 (il vide son graphe).
Albedos : le sol rendu est clair (~0.25-0.30 au soleil bas). Une terre mouillee vaut ~0.55 de sa
valeur seche. Aucun albedo de decalque n'approche 0.5 : a EV100 14 il sortirait blanc (SOL-01).

Variables d'environnement :
    ANASTASIS_ACR_REBUILD  "1" vide le graphe du maitre et le regenere (meme asset)
    ANASTASIS_ACR_QUIT     "0" : ne ferme pas l'editeur (aaa-contact-realism.ps1 -Tests)
"""
import os
import unreal

PKG = '/Game/Anastasis/AAAContactRealism'
MASTER_NAME = 'M_ACR_Decal'
MASTER = PKG + '/' + MASTER_NAME
mel = unreal.MaterialEditingLibrary

# Demi-profondeur de la boite de projection (uu) : la meme pour toutes les familles, lue par le
# materiau (Surface.w) et par le planificateur (FSettings::DecalDepthUU) -- les deux concordent.
DEPTH = 220.0

PATTERN_HLSL = r'''
float2 sd = frac(Seed.xy * 0.0137 + 0.21) * 37.0 + Seed.z * 0.0031;
float2 u = UV * 2.0 - 1.0;
float e = 0.04;
float2 off[3] = { float2(0.0, 0.0), float2(e, 0.0), float2(0.0, e) };
float nv[3];
for (int k = 0; k < 3; k++)
{
    float2 q = (u + off[k]) * PA.x + sd;
    float amp = 0.5;
    float acc = 0.0;
    float nrm = 0.0;
    for (int i = 0; i < 4; i++)
    {
        float2 ip = floor(q);
        float2 fp = frac(q);
        fp = fp * fp * (3.0 - 2.0 * fp);
        float a = frac(sin(dot(ip, float2(127.1, 311.7))) * 43758.5453);
        float b = frac(sin(dot(ip + float2(1.0, 0.0), float2(127.1, 311.7))) * 43758.5453);
        float c = frac(sin(dot(ip + float2(0.0, 1.0), float2(127.1, 311.7))) * 43758.5453);
        float d = frac(sin(dot(ip + float2(1.0, 1.0), float2(127.1, 311.7))) * 43758.5453);
        acc += amp * lerp(lerp(a, b, fp.x), lerp(c, d, fp.x), fp.y);
        nrm += amp;
        amp *= 0.5;
        q = q * 2.03 + 17.3;
    }
    nv[k] = acc / nrm;
}
float r = length(u);
float shape = saturate(1.0 - r) + (nv[0] - 0.5) * PA.y;
float mask = smoothstep(0.0, max(PA.z, 0.05), shape) * smoothstep(1.0, 0.8, r);
// Fondu en profondeur : la boite de projection a des faces planes, le sol non. Sans ce fondu, le
// decalque s'arrete net sur un talus (bords droits). SF.w = demi-profondeur de la boite (uu).
float dz = WPos.z - Seed.z;
float depth = abs(dz) / max(SF.w, 1.0);
mask *= 1.0 - smoothstep(0.4, 1.0, depth);
// Porte d'eau (GT.x = 1 pour les familles de rive) : le decalque est CENTRE SUR LE NIVEAU DE L'EAU, et rien
// de ce qui est dessous n'est touche. Sans elle, le fond immerge, vu par transparence, devenait des ovales
// cyan cernes de blanc (capture second, 2026-10-02).
mask *= lerp(1.0, smoothstep(-6.0, 4.0, dz), saturate(GT.x));
return float4(mask, nv[0], (nv[1] - nv[0]) / e, (nv[2] - nv[0]) / e);
'''

SPECKLE_HLSL = r'''
// Cellules en CENTIMETRES MONDE (PB.x = cote d'une cellule) : une feuille mesure ~10 cm quelle que soit la
// taille du decalque ; en UV, un decalque de 7 m aurait eu des feuilles de 50 cm (capture second).
float2 g = WPos.xy / max(PB.x, 1.0);
float2 id = floor(g);
float2 f = frac(g) - 0.5;
float h1 = frac(sin(dot(id, float2(127.1, 311.7))) * 43758.5453);
float h2 = frac(sin(dot(id + 7.7, float2(269.5, 183.3))) * 43758.5453);
float h3 = frac(sin(dot(id + 3.1, float2(419.2, 371.9))) * 43758.5453);
float ca = cos(h2 * 6.2832);
float sa = sin(h2 * 6.2832);
float2 rf = float2(ca * f.x - sa * f.y, sa * f.x + ca * f.y);
rf.x = rf.x / max(PB.y, 0.2);
rf -= (float2(h3, h1) - 0.5) * 0.35;
float present = step(h1, PA.w);
float dm = present * smoothstep(0.34, 0.12, length(rf));
return float4(dm, h3, h2, 0.0);
'''

SURFACE_HLSL = r'''
float3 baseCol = lerp(CA.rgb, CB.rgb, saturate(Pat.y * 1.5 - 0.25));
float3 specCol = lerp(CC.rgb, CB.rgb * 1.5, Spk.y);
float dm = saturate(Spk.x * Pat.x);
float3 col = lerp(baseCol, specCol, dm);
float op = saturate(Pat.x * SF.x + dm * PB.w);
return float4(col, op);
'''

ROUGH_HLSL = r'''
float dm = Spk.x * Pat.x;
return lerp(SF.y, SF.z, saturate(dm + 0.7 * abs(Pat.y - 0.5)));
'''

NORMAL_HLSL = r'''
float3 n = normalize(float3(-Pat.z * PB.z, -Pat.w * PB.z, 1.0));
return lerp(float3(0.0, 0.0, 1.0), n, Pat.x);
'''

# Une ligne par famille de decalque (ordre de AnastasisContactRealism::EDecal).
#   PatternA = (echelle du bruit, cassure du bord, douceur du bord, densite des taches)
#   PatternB = (cote d'une cellule de taches en cm monde, etirement, force de normale, opacite des taches)
#   Gate     = (porte d'eau : 1 = rien sous le niveau de l'eau et centre du decalque = niveau de l'eau, -, -, -)
#   Surface  = (opacite, rugosite du fond, rugosite des taches, demi-profondeur de la boite en uu)
FAMILIES = {
    # Sol mouille : brun sombre, luisant. Bande ETROITE, gradient doux (Edge 0.9) : le grain du sol
    # (fissures, cailloux) reste lisible dessous, le decalque ne le remplace pas par un aplat.
    'WetBand': dict(ColorA=(0.060, 0.046, 0.030), ColorB=(0.082, 0.064, 0.042), ColorC=(0.12, 0.09, 0.06),
                    PatternA=(2.4, 0.70, 0.75, 0.0), PatternB=(10.0, 1.0, 0.04, 0.0), Surface=(0.70, 0.30, 0.60, DEPTH), Gate=(1, 0, 0, 0)),
    # Vase : plus sombre, tres luisante.
    'Mud': dict(ColorA=(0.045, 0.036, 0.026), ColorB=(0.068, 0.054, 0.038), ColorC=(0.09, 0.07, 0.05),
                PatternA=(3.2, 0.80, 0.80, 0.0), PatternB=(10.0, 1.0, 0.05, 0.0), Surface=(0.80, 0.20, 0.55, DEPTH), Gate=(1, 0, 0, 0)),
    # Pierre mouillee : gris mineral, graviers clairs de 4-5 cm en taches.
    'StoneWet': dict(ColorA=(0.070, 0.068, 0.062), ColorB=(0.100, 0.096, 0.086), ColorC=(0.190, 0.180, 0.160),
                     PatternA=(2.8, 0.75, 0.70, 0.45), PatternB=(4.5, 1.3, 0.06, 0.70), Surface=(0.66, 0.30, 0.60, DEPTH), Gate=(1, 0, 0, 0)),
    # Sol sous les roseaux : organique, tiges paille couchees (taches de 7 cm etirees x3 : ~20 cm).
    'ReedBed': dict(ColorA=(0.050, 0.042, 0.024), ColorB=(0.080, 0.066, 0.036), ColorC=(0.210, 0.160, 0.070),
                    PatternA=(2.6, 0.75, 0.80, 0.35), PatternB=(7.0, 3.2, 0.05, 0.70), Surface=(0.55, 0.40, 0.80, DEPTH), Gate=(1, 0, 0, 0)),
    # Litiere : feuilles mortes brunes et ocre de ~11 cm, mate, bord qui s'effiloche.
    'Litter': dict(ColorA=(0.075, 0.054, 0.030), ColorB=(0.115, 0.080, 0.040), ColorC=(0.165, 0.105, 0.055),
                   PatternA=(2.2, 0.85, 0.70, 0.45), PatternB=(11.0, 1.7, 0.04, 0.80), Surface=(0.40, 0.80, 0.90, DEPTH), Gate=(0, 0, 0, 0)),
    # Terre sombre de la zone racinaire : sans taches, la couleur s'enfonce seulement.
    'ContactDark': dict(ColorA=(0.070, 0.052, 0.032), ColorB=(0.100, 0.075, 0.046), ColorC=(0.12, 0.09, 0.06),
                        PatternA=(2.6, 0.80, 0.80, 0.0), PatternB=(10.0, 1.0, 0.04, 0.0), Surface=(0.55, 0.75, 0.85, DEPTH), Gate=(0, 0, 0, 0)),
    # Collerette de sediment au pied d'un rocher : limon brun-gris, fins graviers de 4 cm.
    'RockDirt': dict(ColorA=(0.100, 0.085, 0.062), ColorB=(0.140, 0.120, 0.088), ColorC=(0.200, 0.180, 0.150),
                     PatternA=(3.0, 0.80, 0.70, 0.35), PatternB=(4.0, 1.2, 0.05, 0.60), Surface=(0.55, 0.70, 0.85, DEPTH), Gate=(0, 0, 0, 0)),
    # Lit de depot : limon pale, mat, bord tres irregulier.
    'Deposit': dict(ColorA=(0.260, 0.225, 0.160), ColorB=(0.310, 0.270, 0.190), ColorC=(0.33, 0.29, 0.20),
                    PatternA=(2.8, 0.90, 0.80, 0.0), PatternB=(10.0, 1.0, 0.03, 0.0), Surface=(0.35, 0.85, 0.90, DEPTH), Gate=(0, 0, 0, 0)),
    # Creux de prairie : humidite diffuse, tres discrete.
    'Depression': dict(ColorA=(0.085, 0.070, 0.045), ColorB=(0.110, 0.092, 0.060), ColorC=(0.13, 0.10, 0.07),
                       PatternA=(2.0, 0.90, 0.90, 0.0), PatternB=(10.0, 1.0, 0.0, 0.0), Surface=(0.30, 0.50, 0.80, DEPTH), Gate=(0, 0, 0, 0)),
    # Rigole d'erosion : trainee minerale pale le long de la pente.
    'Streak': dict(ColorA=(0.240, 0.210, 0.150), ColorB=(0.290, 0.250, 0.180), ColorC=(0.30, 0.26, 0.19),
                   PatternA=(3.4, 0.90, 0.90, 0.0), PatternB=(10.0, 1.0, 0.0, 0.0), Surface=(0.30, 0.85, 0.90, DEPTH), Gate=(0, 0, 0, 0)),
    # Halo humide d'une rive : large, discret, centre sur l'eau (porte d'eau), s'efface vers l'interieur.
    'Halo': dict(ColorA=(0.085, 0.070, 0.045), ColorB=(0.110, 0.092, 0.060), ColorC=(0.13, 0.10, 0.07),
                 PatternA=(2.0, 0.90, 1.00, 0.0), PatternB=(10.0, 1.0, 0.0, 0.0), Surface=(0.40, 0.45, 0.80, DEPTH), Gate=(1, 0, 0, 0)),
}


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


def v4param(mat, name, value, x, y):
    """Parametre vectoriel a QUATRE canaux utiles. La sortie par defaut d'un nœud VectorParameter est
    son RGB (float3) : lire `.w` echoue a la compilation HLSL. On recompose RGB + A par un Append."""
    node = vparam(mat, name, value, x, y)
    pack = mel.create_material_expression(mat, unreal.MaterialExpressionAppendVector, x + 220, y)
    ok_rgb = mel.connect_material_expressions(node, '', pack, 'A')
    ok_a = mel.connect_material_expressions(node, 'A', pack, 'B')
    if not (ok_rgb and ok_a):
        unreal.log_error('ACR_MATERIAL_WIRING_INCOMPLETE pack %s rgb=%s a=%s' % (name, ok_rgb, ok_a))
    return pack


def enum_value(enum_cls, *needles):
    # Les noms Python d'un enum peuvent differer du C++ : on cherche par motifs, et on liste les
    # membres si rien ne va.
    names = [n for n in dir(enum_cls) if not n.startswith('_')]
    for name in names:
        if all(x in name.upper() for x in needles):
            return getattr(enum_cls, name)
    unreal.log_error('ACR_ENUM_MEMBERS %s %s' % (enum_cls.__name__, names))
    raise RuntimeError('enum %s : aucun membre ne contient %s' % (enum_cls.__name__, '+'.join(needles)))


def build(mat):
    mat.set_editor_property('material_domain', enum_value(unreal.MaterialDomain, 'DEFERRED', 'DECAL'))
    mat.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
    wiring = {}

    def link(tag, src, dst, dst_pin, src_pin=''):
        wiring[tag] = mel.connect_material_expressions(src, src_pin, dst, dst_pin)

    uv = mel.create_material_expression(mat, unreal.MaterialExpressionTextureCoordinate, -1500, -200)
    seed = mel.create_material_expression(mat, unreal.MaterialExpressionObjectPositionWS, -1500, -100)
    wpos = mel.create_material_expression(mat, unreal.MaterialExpressionWorldPosition, -1500, -300)
    pa = v4param(mat, 'PatternA', (2.4, 0.7, 0.55, 0.0), -1500, 40)
    pb = v4param(mat, 'PatternB', (10.0, 1.0, 0.1, 0.0), -1500, 140)
    sf = v4param(mat, 'Surface', (0.5, 0.5, 0.8, DEPTH), -1500, 240)
    gt = v4param(mat, 'Gate', (0.0, 0.0, 0.0, 0.0), -1500, 640)
    ca = vparam(mat, 'ColorA', (0.03, 0.022, 0.014, 1.0), -1500, 340)
    cb = vparam(mat, 'ColorB', (0.05, 0.038, 0.024, 1.0), -1500, 440)
    cc = vparam(mat, 'ColorC', (0.07, 0.05, 0.03, 1.0), -1500, 540)

    F1, F3, F4 = (unreal.CustomMaterialOutputType.CMOT_FLOAT1, unreal.CustomMaterialOutputType.CMOT_FLOAT3,
                  unreal.CustomMaterialOutputType.CMOT_FLOAT4)
    pattern = custom(mat, PATTERN_HLSL, 'ACR_Pattern', F4, ('UV', 'Seed', 'PA', 'WPos', 'SF', 'GT'), -900, -100)
    for pin, src in (('UV', uv), ('Seed', seed), ('PA', pa), ('WPos', wpos), ('SF', sf), ('GT', gt)):
        link('pattern_' + pin, src, pattern, pin)
    speckle = custom(mat, SPECKLE_HLSL, 'ACR_Speckle', F4, ('UV', 'Seed', 'PA', 'PB', 'WPos'), -900, 250)
    for pin, src in (('UV', uv), ('Seed', seed), ('PA', pa), ('PB', pb), ('WPos', wpos)):
        link('speckle_' + pin, src, speckle, pin)
    surface = custom(mat, SURFACE_HLSL, 'ACR_Surface', F4, ('Pat', 'Spk', 'CA', 'CB', 'CC', 'SF', 'PB'), -300, -100)
    for pin, src in (('Pat', pattern), ('Spk', speckle), ('CA', ca), ('CB', cb), ('CC', cc), ('SF', sf), ('PB', pb)):
        link('surface_' + pin, src, surface, pin)
    rough = custom(mat, ROUGH_HLSL, 'ACR_Rough', F1, ('Pat', 'Spk', 'SF'), -300, 300)
    for pin, src in (('Pat', pattern), ('Spk', speckle), ('SF', sf)):
        link('rough_' + pin, src, rough, pin)
    normal = custom(mat, NORMAL_HLSL, 'ACR_Normal', F3, ('Pat', 'PB'), -300, 500)
    for pin, src in (('Pat', pattern), ('PB', pb)):
        link('normal_' + pin, src, normal, pin)

    rgb = mel.create_material_expression(mat, unreal.MaterialExpressionComponentMask, 100, -150)
    rgb.set_editor_property('r', True)
    rgb.set_editor_property('g', True)
    rgb.set_editor_property('b', True)
    rgb.set_editor_property('a', False)
    link('rgb_in', surface, rgb, '')
    alpha = mel.create_material_expression(mat, unreal.MaterialExpressionComponentMask, 100, 0)
    alpha.set_editor_property('r', False)
    alpha.set_editor_property('g', False)
    alpha.set_editor_property('b', False)
    alpha.set_editor_property('a', True)
    link('alpha_in', surface, alpha, '')

    wiring['base_color'] = mel.connect_material_property(rgb, '', unreal.MaterialProperty.MP_BASE_COLOR)
    wiring['opacity'] = mel.connect_material_property(alpha, '', unreal.MaterialProperty.MP_OPACITY)
    wiring['roughness'] = mel.connect_material_property(rough, '', unreal.MaterialProperty.MP_ROUGHNESS)
    wiring['normal'] = mel.connect_material_property(normal, '', unreal.MaterialProperty.MP_NORMAL)

    unreal.log('ACR_MATERIAL_WIRING ' + ' '.join('%s=%s' % (k, wiring[k]) for k in sorted(wiring)))
    if not all(wiring.values()):
        unreal.log_error('ACR_MATERIAL_WIRING_INCOMPLETE ' + repr(wiring))
    # Marqueur AVANT la compilation : un echec de compilation n'est qu'un Warning dans le log ;
    # aaa-contact-realism.ps1 cherche les erreurs qui le suivent et refuse le materiau.
    unreal.log('ACR_MATERIAL_COMPILE ' + MASTER)
    mel.recompile_material(mat)
    unreal.EditorAssetLibrary.save_asset(MASTER)
    unreal.log('ACR_MATERIAL_SAVED ' + MASTER)


def write_instances():
    parent = unreal.EditorAssetLibrary.load_asset(MASTER)
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    for family, values in FAMILIES.items():
        name = 'MI_ACR_' + family
        path = PKG + '/' + name
        if unreal.EditorAssetLibrary.does_asset_exist(path):
            mi = unreal.EditorAssetLibrary.load_asset(path)
        else:
            mi = tools.create_asset(name, PKG, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        if mi is None:
            raise RuntimeError('creation impossible : ' + path)
        mel.set_material_instance_parent(mi, parent)
        for key in ('ColorA', 'ColorB', 'ColorC'):
            r, g, b = values[key]
            mel.set_material_instance_vector_parameter_value(mi, key, unreal.LinearColor(r, g, b, 1.0))
        for key in ('PatternA', 'PatternB', 'Surface', 'Gate'):
            r, g, b, a = values[key]
            mel.set_material_instance_vector_parameter_value(mi, key, unreal.LinearColor(r, g, b, a))
        unreal.EditorAssetLibrary.save_asset(path)
        unreal.log('ACR_INSTANCE_SAVED ' + path)


try:
    rebuild = os.environ.get('ANASTASIS_ACR_REBUILD', '0') == '1'
    if not unreal.EditorAssetLibrary.does_asset_exist(MASTER):
        master = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            MASTER_NAME, PKG, unreal.Material, unreal.MaterialFactoryNew())
        build(master)
    elif rebuild:
        master = unreal.EditorAssetLibrary.load_asset(MASTER)
        mel.delete_all_material_expressions(master)
        unreal.log('ACR_MATERIAL_CLEARED ' + MASTER)
        build(master)
    else:
        unreal.log('ACR_MATERIAL_PRESENT ' + MASTER)
    write_instances()
except Exception as e:
    unreal.log_error('ACR_MATERIAL_FAIL %r' % e)
# ANASTASIS_ACR_QUIT=0 (aaa-contact-realism.ps1 -Tests) : le meme editeur enchaine ensuite les tests
# Anastasis.ContactRealism puis se ferme lui-meme ; une file d'editeur de moins.
if os.environ.get('ANASTASIS_ACR_QUIT', '1') != '0':
    unreal.SystemLibrary.quit_editor()
