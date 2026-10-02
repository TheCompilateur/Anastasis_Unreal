"""Materiau de sol d'ANASTASIS : M_AnastasisGround + MI_AnastasisGround.

PROPRIETE DES ASSETS. Ce script est la SOURCE D'AUTORITE de deux assets versionnes :
/Game/Anastasis/Materials/M_AnastasisGround (maitre) et .../MI_AnastasisGround
(instance). Ils sont crees ici, puis committes. Une fois crees, ce script ne les
modifie plus : il les charge et rapporte leurs statistiques. Les regenerer se fait
explicitement (ANASTASIS_GROUND_REBUILD=1) et ecrase toute retouche faite a la main.

Meme contrat que tools/unreal/observe-slice.py pour M_AnastasisSlice, qui reste en
place : ce fichier n'y touche pas.

PARTAGE DES ROLES, et c'est le point de tout le dispositif :

  LE MAITRE porte la STRUCTURE -- quelles donnees sont lues, dans quel ordre elles se
  melangent, et a quelles FREQUENCES la variation opere. Les frequences sont des
  proprietes de noeud (Noise.Scale n'est pas parametrable, et VectorNoise n'a pas de
  Scale du tout) : elles se changent ici, par regeneration.

  L'INSTANCE porte le LOOK -- teintes, amplitudes, seuils, rugosites. Changer la
  couleur d'une roche ou l'ampleur d'une variation ne demande donc ni recompilation
  C++, ni ouverture du materiau maitre, ni ce script.

CE QUE LE MATERIAU LIT, ET D'OU CA VIENT (AnastasisTerrainSurface::Build) :

  VertexColor.rgb  teinte semantique de la tuile, deja projetee (type, altitude, rive)
  TexCoord0.x/.y   poids de famille Rock / Litter
  TexCoord1.x      poids de famille Worked           (l'herbe est le reste : 1-R-L-W)
  TexCoord1.y      Wetness [0,1], champ de proximite d'eau du simulateur
  VertexNormalWS   la pente, qui n'a pas besoin d'etre exportee : elle EST la normale
  WorldPosition    l'altitude et la place dans le monde, pour la meme raison

Aucune de ces entrees n'est inventee ici. Le materiau projette, il ne simule pas.

TEXTURES PHOTO (GROUND_TEXTURE_001). Huit textures CC0, deux par famille, importees dans
/Game/Anastasis/Materials/GroundTextures depuis Saved/GroundTextures/packed, que produit
tools/unreal/ground-textures.py (Python systeme, a lancer AVANT). Elles portent ce que la
geometrie a un sommet par metre ne peut pas porter : le detail sous le metre. Elles ne
portent PAS la teinte : l'albedo photo est stocke divise par sa moyenne, et module la
couleur calee au lieu de la remplacer (voir l'en-tete de ground-textures.py). Une fois
importees, ce script ne les reimporte plus, sauf ANASTASIS_GROUND_TEXTURES_REIMPORT=1.

Variables d'environnement :
  ANASTASIS_GROUND_REBUILD             "1" regenere les deux assets (ecrase l'existant)
  ANASTASIS_GROUND_TEXTURES_REIMPORT   "1" reimporte les huit textures depuis packed/
"""
import json
import os
import time
import unreal

PKG = '/Game/Anastasis/Materials'
MASTER = PKG + '/M_AnastasisGround'
INSTANCE = PKG + '/MI_AnastasisGround'

mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()

REBUILD = os.environ.get('ANASTASIS_GROUND_REBUILD', '0') == '1'
TEX_REIMPORT = os.environ.get('ANASTASIS_GROUND_TEXTURES_REIMPORT', '0') == '1'

TEX_PKG = PKG + '/GroundTextures'
# Ordre = ordre des composantes du melange dans le HLSL : herbe, litiere, travaillee, roche.
# Taille = cote reel de la photo en cm (fiche Poly Haven), donc l'echelle par defaut est
# l'echelle physique : un caillou de la photo a la taille d'un caillou.
TEX_FAMILIES = (('Grass', 200.0), ('Litter', 300.0), ('Worked', 130.0), ('Rock', 300.0))
# Inverse de DETAIL_MEAN de ground-textures.py : l'albedo de detail est stocke a une
# moyenne de 0.4, le materiau le ramene a 1. Les deux nombres vont ensemble ;
# import_textures() le verifie contre le manifeste a chaque import.
TEX_DETAIL_MEAN = 0.4


def log(msg):
    unreal.log('GROUND_MATERIAL ' + msg)


def enum_of(enum_type, index, *names):
    """Valeur d'enum robuste au nommage du binding Python.

    Le nom expose par Unreal pour une valeur d'UENUM depend de sa translitteration
    (NOISEFUNCTION_GradientTex3D peut sortir en NOISEFUNCTION_GRADIENT_TEX3_D ou
    NOISEFUNCTION_GRADIENT_TEX3D selon la version). Se tromper de nom ne leve pas :
    getattr rend None, set_editor_property garde la valeur par defaut, et le materiau
    compile avec le MAUVAIS bruit -- une erreur qui ne se voit qu'a l'oeil, trop tard.
    On essaie donc les orthographes, puis l'indice, et on leve si rien ne tient.
    """
    for n in names:
        v = getattr(enum_type, n, None)
        if v is not None:
            return v
    try:
        return enum_type.cast(index)
    except Exception:
        pass
    try:
        return enum_type(index)
    except Exception as exc:
        raise RuntimeError('enum %s: aucune valeur pour %s / index %d (%s)'
                           % (enum_type, names, index, exc))


# --------------------------------------------------------------------------------------
# Constructeurs de noeuds. Chacun LEVE si la connexion echoue.
#
# Un materiau ou une liaison a echoue ne plante pas : il compile, et il rend un sol faux
# -- typiquement un sol plat, exactement le symptome qu'on essaie de corriger. Un retour
# False silencieux serait donc la pire issue possible ici. On refuse de produire l'asset.
# --------------------------------------------------------------------------------------
class Graph:
    def __init__(self, material):
        self.m = material
        self.x = -2600
        self.y = 0

    def node(self, cls, x, y, **props):
        e = mel.create_material_expression(self.m, cls, x, y)
        if e is None:
            raise RuntimeError('create_material_expression failed for %s' % cls)
        for k, v in props.items():
            e.set_editor_property(k, v)
            # Relecture. set_editor_property peut ne rien faire -- valeur refusee par un
            # CanEditChange, remise a zero par un PostEditChangeProperty, enum non
            # convertie -- sans lever. Le noeud garde alors sa valeur par defaut, et la
            # seule trace est un materiau qui ne compile pas, ou pire, qui compile et
            # rend autre chose. On verifie donc ce qui a REELLEMENT ete ecrit.
            got = e.get_editor_property(k)
            same = (got == v)
            # Les proprietes de noeud sont des float32 : 0.34 relu vaut 0.3400000035762787.
            # La tolerance est donc celle du float32, pas celle du double.
            if not same and isinstance(v, float):
                same = abs(float(got) - v) <= 1.e-6 * max(1.0, abs(v))
            if not same and isinstance(v, unreal.LinearColor):
                same = all(abs(getattr(got, c) - getattr(v, c)) <= 1.e-6
                           for c in ('r', 'g', 'b', 'a'))
            if not same:
                raise RuntimeError('%s.%s vaut %r apres ecriture de %r'
                                   % (cls.__name__ if hasattr(cls, '__name__') else cls, k, got, v))
        return e

    def link(self, src, src_out, dst, dst_in):
        # Le nom d'entree expose par Unreal n'est pas celui du champ C++ : sur 5.8 un
        # ComponentMask declare FExpressionInput Input mais n'expose aucun nom, et
        # connect_material_expressions(..., 'Input') echoue. On resout donc le nom
        # contre la liste REELLE du noeud, au lieu de le deviner -- et on leve en
        # citant cette liste, pour que l'erreur suivante soit lisible du premier coup.
        real = list(mel.get_material_expression_input_names(dst))
        wanted = dst_in
        if wanted:
            match = [n for n in real if n.lower() == wanted.lower()]
            if match:
                wanted = match[0]
            elif len(real) == 1 or (len(real) == 0):
                wanted = ''
            else:
                raise RuntimeError('entree "%s" absente de %s ; entrees reelles=%s' % (
                    dst_in, dst.get_class().get_name(), real))
        if not mel.connect_material_expressions(src, src_out, dst, wanted):
            raise RuntimeError('connect %s[%s] -> %s[%s] FAILED (entrees reelles=%s)' % (
                src.get_class().get_name(), src_out, dst.get_class().get_name(), wanted, real))
        return dst

    def prop(self, src, src_out, material_property):
        if not mel.connect_material_property(src, src_out, material_property):
            raise RuntimeError('connect %s[%s] -> %s FAILED' % (
                src.get_class().get_name(), src_out, material_property))

    # --- raccourcis d'arithmetique -----------------------------------------------------
    def scalar(self, name, default, group, x, y):
        return self.node(unreal.MaterialExpressionScalarParameter, x, y,
                         parameter_name=name, default_value=default, group=group)

    def vector(self, name, rgb, group, x, y):
        """Parametre vectoriel DEJA masque en RGB.

        La sortie par defaut d'un VectorParameter est float4. La chaine d'albedo, elle,
        est float3 (VertexColor masquee RGB). Multiplier l'un par l'autre est une erreur
        de compilation -- "Arithmetic between types float3 and float4 are undefined" --
        et un materiau qui ne compile pas est remplace par le Default Material sans que
        rien ne s'affiche a l'ecran. On masque donc a la source, une fois pour toutes.
        """
        p = self.node(unreal.MaterialExpressionVectorParameter, x, y,
                      parameter_name=name,
                      default_value=unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0),
                      group=group)
        return self.mask(p, '', True, True, True, x + 150, y)

    def const(self, v, x, y):
        return self.node(unreal.MaterialExpressionConstant, x, y, r=v)

    def mul(self, a, ao, b, bo, x, y):
        n = self.node(unreal.MaterialExpressionMultiply, x, y)
        self.link(a, ao, n, 'A')
        self.link(b, bo, n, 'B')
        return n

    def mulc(self, a, ao, k, x, y):
        n = self.node(unreal.MaterialExpressionMultiply, x, y, const_b=k)
        self.link(a, ao, n, 'A')
        return n

    def add(self, a, ao, b, bo, x, y):
        n = self.node(unreal.MaterialExpressionAdd, x, y)
        self.link(a, ao, n, 'A')
        self.link(b, bo, n, 'B')
        return n

    def addc(self, a, ao, k, x, y):
        n = self.node(unreal.MaterialExpressionAdd, x, y, const_b=k)
        self.link(a, ao, n, 'A')
        return n

    def sub(self, a, ao, b, bo, x, y):
        n = self.node(unreal.MaterialExpressionSubtract, x, y)
        self.link(a, ao, n, 'A')
        self.link(b, bo, n, 'B')
        return n

    def subc(self, a, ao, k, x, y):
        n = self.node(unreal.MaterialExpressionSubtract, x, y, const_b=k)
        self.link(a, ao, n, 'A')
        return n

    def div(self, a, ao, b, bo, x, y):
        n = self.node(unreal.MaterialExpressionDivide, x, y)
        self.link(a, ao, n, 'A')
        self.link(b, bo, n, 'B')
        return n

    def lerp(self, a, ao, b, bo, alpha, alpha_o, x, y):
        n = self.node(unreal.MaterialExpressionLinearInterpolate, x, y)
        self.link(a, ao, n, 'A')
        self.link(b, bo, n, 'B')
        self.link(alpha, alpha_o, n, 'Alpha')
        return n

    def smoothstep(self, lo, lo_o, hi, hi_o, val, val_o, x, y):
        n = self.node(unreal.MaterialExpressionSmoothStep, x, y)
        self.link(lo, lo_o, n, 'Min')
        self.link(hi, hi_o, n, 'Max')
        self.link(val, val_o, n, 'Value')
        return n

    def sat(self, a, ao, x, y):
        n = self.node(unreal.MaterialExpressionSaturate, x, y)
        self.link(a, ao, n, '')
        return n

    def mask(self, a, ao, r, g, b, x, y):
        n = self.node(unreal.MaterialExpressionComponentMask, x, y, r=r, g=g, b=b, a=False)
        self.link(a, ao, n, '')
        return n

    def one_minus(self, a, ao, x, y):
        n = self.node(unreal.MaterialExpressionOneMinus, x, y)
        self.link(a, ao, n, '')
        return n

    def custom(self, code, desc, out_type, inputs, extra_outputs, x, y):
        """Noeud Custom HLSL, entrees et sorties supplementaires RELUES apres ecriture.

        Les tableaux de structures ne passent pas par node() : l'egalite d'un tableau de
        FCustomInput relu n'est pas celle de la liste Python ecrite. On compare donc les
        NOMS que le noeud expose reellement -- c'est ce que link() verra. Une sortie
        supplementaire qui n'existe pas ferait echouer la liaison plus loin, avec un
        message bien moins lisible que celui-ci.
        """
        n = self.node(unreal.MaterialExpressionCustom, x, y, code=code, description=desc,
                      output_type=out_type)
        ins = []
        for name in inputs:
            ci = unreal.CustomInput()
            ci.set_editor_property('input_name', name)
            ins.append(ci)
        n.set_editor_property('inputs', ins)
        outs = []
        for name, t in extra_outputs:
            co = unreal.CustomOutput()
            co.set_editor_property('output_name', name)
            co.set_editor_property('output_type', t)
            outs.append(co)
        n.set_editor_property('additional_outputs', outs)
        got_in = [str(s) for s in mel.get_material_expression_input_names(n)]
        if got_in != list(inputs):
            raise RuntimeError('Custom %s : entrees %s, attendues %s' % (desc, got_in, list(inputs)))
        got_out = [str(s) for s in mel.get_material_expression_output_names(n)]
        missing = [name for name, _ in extra_outputs if name not in got_out]
        if missing:
            raise RuntimeError('Custom %s : sorties %s absentes de %s' % (desc, missing, got_out))
        return n


# ENoiseFunction : index 2 = NOISEFUNCTION_GradientTex3D ("Fast Gradient - 3D Texture",
# ~16 instructions et 1 lookup par niveau, la seule variante assez bon marche pour deux
# octaves de decor). EVectorNoiseFunction : index 2 = VNF_GradientALU ("Perlin Gradient",
# RGB = gradient, A = scalaire).
NOISE_FAST_GRADIENT_3D = enum_of(unreal.NoiseFunction, 2,
                                 'NOISEFUNCTION_GRADIENT_TEX3_D', 'NOISEFUNCTION_GRADIENT_TEX3D',
                                 'NOISEFUNCTION_GradientTex3D')
VECTOR_NOISE_PERLIN_GRADIENT = enum_of(unreal.VectorNoiseFunction, 2,
                                       'VNF_GRADIENT_ALU', 'VNF_GRADIENTALU', 'VNF_GradientALU')
CMOT_FLOAT1 = enum_of(unreal.CustomMaterialOutputType, 0, 'CMOT_FLOAT1', 'CMOT_Float1')
CMOT_FLOAT3 = enum_of(unreal.CustomMaterialOutputType, 2, 'CMOT_FLOAT3', 'CMOT_Float3')
SAMPLER_COLOR = enum_of(unreal.MaterialSamplerType, 0, 'SAMPLERTYPE_COLOR', 'SAMPLERTYPE_Color')
SAMPLER_LINEAR_COLOR = enum_of(unreal.MaterialSamplerType, 3, 'SAMPLERTYPE_LINEAR_COLOR',
                               'SAMPLERTYPE_LinearColor')


# --------------------------------------------------------------------------------------
# Textures photo
# --------------------------------------------------------------------------------------
def tex_path(family, suffix):
    return '%s/T_Ground_%s_%s' % (TEX_PKG, family, suffix)


def import_textures():
    """Importe les huit textures si elles manquent (ou sur REIMPORT), puis les rend.

    AH : sRGB, la couleur de detail est perceptuelle et 8 bits lineaires la
    postÃƒÂ©riseraient dans les sombres. NR : LINEAIRE -- c'est une normale, une rugosite
    et une occlusion, pas une couleur ; decodee en sRGB, la normale serait tordue.
    Les deux en BC7 : BC1 n'a pas d'alpha, BC3 abime la normale ; BC7 garde les quatre
    canaux propres pour 1 octet par texel.
    """
    packed = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()),
                          'GroundTextures', 'packed')
    manifest_checked = False
    out = {}
    for family, _ in TEX_FAMILIES:
        for suffix, srgb in (('AH', True), ('NR', False)):
            path = tex_path(family, suffix)
            if TEX_REIMPORT or not eal.does_asset_exist(path):
                if not manifest_checked:
                    with open(os.path.join(packed, '..', 'manifest.json'), encoding='utf-8') as f:
                        mean = json.load(f)['detail_mean']
                    if abs(mean - TEX_DETAIL_MEAN) > 1e-6:
                        raise RuntimeError('manifest detail_mean=%s, materiau=%s : les deux scripts '
                                           'ne parlent plus de la meme texture' % (mean, TEX_DETAIL_MEAN))
                    manifest_checked = True
                src = os.path.join(packed, 'T_Ground_%s_%s.png' % (family, suffix))
                if not os.path.isfile(src):
                    raise RuntimeError('%s absent : lancer d abord python tools/unreal/ground-textures.py'
                                       % src)
                task = unreal.AssetImportTask()
                task.set_editor_property('filename', src)
                task.set_editor_property('destination_path', TEX_PKG)
                task.set_editor_property('destination_name', 'T_Ground_%s_%s' % (family, suffix))
                task.set_editor_property('replace_existing', True)
                task.set_editor_property('automated', True)
                task.set_editor_property('save', False)
                tools.import_asset_tasks([task])
                tex = unreal.load_asset(path)
                if tex is None:
                    raise RuntimeError('import de %s echoue' % src)
                tex.set_editor_property('srgb', srgb)
                tex.set_editor_property('compression_settings',
                                        unreal.TextureCompressionSettings.TC_BC7)
                tex.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_WORLD)
                eal.save_asset(path)
                log('TEXTURE_IMPORTED %s srgb=%s' % (path, srgb))
            tex = unreal.load_asset(path)
            if tex is None:
                raise RuntimeError('texture %s introuvable' % path)
            if bool(tex.get_editor_property('srgb')) != srgb:
                raise RuntimeError('%s srgb=%s, attendu %s' % (path, tex.get_editor_property('srgb'), srgb))
            out[(family, suffix)] = tex
    log('TEXTURES_READY count=%d' % len(out))
    return out


# Projection triplanaire d'UNE famille. Les axes dont le poids est negligeable ne sont pas
# echantillonnes : sur un terrain, le plat ne paie que l'axe Z. D'ou [branch] et
# SampleGrad -- un echantillonnage implicite dans une branche divergente lirait des
# derivees indefinies, et les mips sauteraient en damier aux bords de branche.
#
# Normale : "whiteout" de Ben Golus, axe par axe. Chaque projection pose sa normale
# tangente sur la normale de sommet, dans le repere ou u et v suivent DEUX axes du monde
# nommes explicitement ; aucune tangente n'est donc necessaire -- le maillage n'en a pas.
# La normale DirectX d'Unreal a +G vers +v, ce qui fait correspondre tn.xy a (u, v) sans
# inversion. Le signe de l'axe retourne u sur les faces arriere, pour ne pas les mirer.
_AXES = (
    # (poids, signe, uv, gradient, normale tangente posee, swizzle vers le monde)
    ('tw.x', 'sg.x', 'float2(P.z * sg.x, P.y)', 'float2({d}.z * sg.x, {d}.y)', 'float3(xy + Nn.zy, z * Nn.x)', 'zyx'),
    ('tw.y', 'sg.y', 'float2(P.x * sg.y, P.z)', 'float2({d}.x * sg.y, {d}.z)', 'float3(xy + Nn.xz, z * Nn.y)', 'xzy'),
    ('tw.z', 'sg.z', 'float2(P.x * sg.z, P.y)', 'float2({d}.x * sg.z, {d}.y)', 'float3(xy + Nn.xy, z * Nn.z)', 'xyz'),
)


def _family_hlsl(i, family, comp):
    lines = ['[branch] if (W.%s > 0.002) {' % comp,
             '    float t = 1.0 / TexSize%s;' % family,
             '    float ns = NSf.%s;' % comp]
    for w, s, uv, grad, tn, swz in _AXES:
        lines += [
            '    [branch] if (%s > 0.01) {' % w,
            '        float2 uv = %s * t;' % uv,
            '        float2 gx = %s * t;' % grad.format(d='dPx'),
            '        float2 gy = %s * t;' % grad.format(d='dPy'),
            '        float4 a = Texture2DSampleGrad(%sAH, %sAHSampler, uv, gx, gy);' % (family, family),
            '        float4 n = Texture2DSampleGrad(%sNR, %sNRSampler, uv, gx, gy);' % (family, family),
            '        float2 xy0 = n.xy * 2.0 - 1.0;',
            '        float z = sqrt(saturate(1.0 - dot(xy0, xy0)));',
            '        float2 xy = xy0 * ns;',
            '        xy.x *= %s;' % s,
            '        A%d += a * %s; R%d += n * %s; N%d += (%s).%s * %s;' % (i, w, i, w, i, tn, swz, w),
            '    }']
    lines += ['    H.%s = A%d.a;' % (comp, i), '}']
    return '\n'.join(lines)


def ground_texture_hlsl():
    """Corps du noeud Custom : poids de famille, projection, melange par hauteur.

    Retour (float3) : facteur d'albedo, neutre en moyenne -- detail * occlusion.
    TexNormal : normale monde, deja posee sur la normale de sommet.
    TexRough  : ecart de rugosite signe, a ajouter a la rugosite calee.

    Melange par hauteur : chaque famille concourt avec poids + hauteur*HeightBlend, et
    seules celles a moins de BlendDepth du maximum restent. La terre remplit donc d'abord
    les creux entre les cailloux au lieu de se fondre en fondu enchaine -- c'est ce qui
    fait une transition de sol, par opposition a un degrade.
    """
    comps = 'xyzw'
    body = '\n'.join(_family_hlsl(i, f, comps[i]) for i, (f, _) in enumerate(TEX_FAMILIES))
    return '''TexNormal = normalize(N);
TexRough = 0.0;
[branch] if (Fade < 0.001) { return float3(1.0, 1.0, 1.0); }
float3 Nn = TexNormal;
float3 sg = float3(Nn.x >= 0.0 ? 1.0 : -1.0, Nn.y >= 0.0 ? 1.0 : -1.0, Nn.z >= 0.0 ? 1.0 : -1.0);
float3 tw = pow(abs(Nn), 8.0);
tw /= max(tw.x + tw.y + tw.z, 1e-5);
float3 dPx = ddx(P);
float3 dPy = ddy(P);
// Poids des familles : la roche est le masque du materiau (simulation + pente + meso),
// le reste se partage herbe / litiere / travaillee au prorata des canaux UV.
float g = saturate(1.0 - RockW - Litter - Worked);
float s = Litter + Worked + g;
float4 W = s > 1e-3 ? float4(g, Litter, Worked, 0.0) / s : float4(1.0, 0.0, 0.0, 0.0);
W.xyz *= 1.0 - RockMask;
W.w = RockMask;
float4 NSf = NormalStrength * float4(1.0, 1.0, 1.0, RockNormalScale);
float4 H = 0;
float4 A0 = 0, A1 = 0, A2 = 0, A3 = 0;
float4 R0 = 0, R1 = 0, R2 = 0, R3 = 0;
float3 N0 = 0, N1 = 0, N2 = 0, N3 = 0;
%s
float4 hw = W + H * HeightBlend;
float m = max(max(hw.x, hw.y), max(hw.z, hw.w)) - BlendDepth;
float4 b = max(hw - m, 0.0) * saturate((W - 0.002) * 25.0);
b /= max(dot(b, 1.0), 1e-5);
float3 alb = (b.x * A0.rgb + b.y * A1.rgb + b.z * A2.rgb + b.w * A3.rgb) * %.6f;
float4 nr = b.x * R0 + b.y * R1 + b.z * R2 + b.w * R3;
float3 nw = b.x * N0 + b.y * N1 + b.z * N2 + b.w * N3;
nw = dot(nw, nw) > 1e-8 ? normalize(nw) : Nn;
float ao = lerp(1.0, nr.a * 2.0, AOStrength);
float3 detail = lerp(float3(1.0, 1.0, 1.0), alb, AlbedoStrength) * ao;
TexNormal = normalize(lerp(Nn, nw, Fade));
TexRough = (nr.b - 0.5) * 2.0 * RoughnessStrength * Fade;
return lerp(float3(1.0, 1.0, 1.0), detail, Fade);
''' % (body, 1.0 / TEX_DETAIL_MEAN)



def slope_surface_hlsl():
    """Existing rock fabric at metre scale on exposed middle-distance slopes."""
    lines = ["SlopeNormal = normalize(N); float3 Nn = SlopeNormal;",
        "float weight = saturate(Enabled) * smoothstep(0.06,0.25,1.0-Nn.z);",
        "weight *= (1.0-0.80*saturate(Litter))*(1.0-0.30*saturate(Wet));",
        "weight *= smoothstep(1000.0,2200.0,Depth)*(1.0-smoothstep(18000.0,35000.0,Depth));",
        "float3 dPx=ddx(P),dPy=ddy(P);",
        "[branch] if(weight < 0.001) return float3(1,1,1);",
        "float3 sg=float3(Nn.x>=0?1:-1,Nn.y>=0?1:-1,Nn.z>=0?1:-1);",
        "float3 tw=pow(abs(Nn),8.0); tw/=max(dot(tw,1.0),1e-5);",
        "float t=1.0/max(Size,100.0);",
        "float4 A=0; float3 Nm=0;"]
    for w, sign, uv, grad, tn, swizzle in _AXES:
        lines += ['[branch] if (%s > 0.01) {' % w,
            'float2 uv=%s*t,gx=%s*t,gy=%s*t;' % (uv,grad.format(d='dPx'),grad.format(d='dPy')),
            'float4 a=Texture2DSampleGrad(RockAH,RockAHSampler,uv,gx,gy);',
            'float4 n=Texture2DSampleGrad(RockNR,RockNRSampler,uv,gx,gy);',
            'float2 xy=(n.xy*2.0-1.0)*0.16; xy.x *= %s;' % sign,
            'float z=sqrt(saturate(1.0-dot(xy,xy)));',
            'A += a*%s; Nm += (%s).%s * %s;' % (w,tn,swizzle,w), '}']
    lines += ['SlopeNormal=normalize(lerp(Nn,normalize(Nm),weight));',
        'float3 contrast=clamp(A.rgb*%.6f,0.55,1.45);' % (1.0/TEX_DETAIL_MEAN),
        'return lerp(float3(1,1,1),contrast,weight*0.80);']
    return '\n'.join(lines)


def build_master(textures):
    mat = tools.create_asset('M_AnastasisGround', PKG, unreal.Material, unreal.MaterialFactoryNew())
    if mat is None:
        raise RuntimeError('create_asset M_AnastasisGround failed')
    # Si l'ancien asset n'avait pas reellement disparu, Unreal ne refuse pas : il cree
    # M_AnastasisGround1. Le C++ charge le chemin EXACT, il ne verrait donc jamais ce
    # materiau -- et le repli sur la tranche historique ressemblerait a un bug de rendu.
    if mat.get_name() != 'M_AnastasisGround':
        raise RuntimeError('asset renomme en %s : l ancien n a pas ete supprime' % mat.get_name())

    # Normale en ESPACE MONDE, pas tangent. AnastasisTerrainSurface::Build ne produit
    # AUCUNE tangente -- il passe un TArray<FProcMeshTangent> vide a la section. Une
    # normale tangent-space serait donc transformee par une base degeneree : un
    # eclairage faux, et faux d'une maniere qui ressemble a un bug de lumiere. On reste
    # en monde, ou la seule base necessaire est la normale de sommet, qui existe.
    mat.set_editor_property('tangent_space_normal', False)

    g = Graph(mat)

    # ---------------------------------------------------------------- entrees du monde
    vc = g.node(unreal.MaterialExpressionVertexColor, -2600, -200)
    uv0 = g.node(unreal.MaterialExpressionTextureCoordinate, -2600, 60, coordinate_index=0)
    uv1 = g.node(unreal.MaterialExpressionTextureCoordinate, -2600, 180, coordinate_index=1)
    nws = g.node(unreal.MaterialExpressionVertexNormalWS, -2600, 300)
    wp = g.node(unreal.MaterialExpressionWorldPosition, -2600, 420)

    w_rock = g.mask(uv0, '', True, False, False, -2380, 40)
    w_litter = g.mask(uv0, '', False, True, False, -2380, 120)
    w_worked = g.mask(uv1, '', True, False, False, -2380, 200)
    wetness = g.mask(uv1, '', False, True, False, -2380, 280)
    # Pas de lecture de VertexColor.A ici, et pas de branche eau du tout : la nappe d'eau
    # est une AUTRE section de maillage, et elle recoit son propre materiau (cf.
    # AAnastasisWorldEmbodiment::EmbodyCrop). Un materiau de sol qui porterait aussi la
    # logique de l'eau melangerait deux perimetres, dont un qui n'appartient pas a cette
    # mission -- et il paierait trois interpolations par pixel pour un cas que la
    # geometrie separe deja.
    # Pente : 1 - N.z. Plat = 0, vertical = 1. Pas une donnee exportee -- la normale de
    # sommet est deja la pente, la reexporter en UV serait la dupliquer.
    nz = g.mask(nws, '', False, False, True, -2380, 320)
    slope = g.one_minus(nz, '', -2200, 320)

    # ------------------------------------------------------------------------- bruits
    # Trois echelles nettement separees, jamais superposees : une seule frequence donne
    # le "papier peint procedural" que la direction artistique interdit. Les FREQUENCES
    # sont ici (structure), les AMPLITUDES sont des parametres (look).
    #
    # Macro ~60 m : les grandes masses chromatiques du paysage.
    # Meso  ~11 m : les taches, et l'irregularite des masques roche/humidite.
    # Detail ~25 cm : le grain, et la SEULE source de relief micro. L'echelle de 1,1 m
    # (1/70) laissait chaque trou entre les touffes d'une seule teinte. VectorNoise
    # "Perlin Gradient" rend le gradient (RGB) ET le scalaire (A) en une evaluation,
    # ce qui donne une normale analytique sans payer trois bruits.
    #
    # Fast Gradient 3D se repete tous les 16 en position mise a l'echelle : a 1/6000
    # cela fait 96 000 UU, dix fois le monde (9 600 UU). Aucune repetition visible.
    # La position est CONNECTEE, pas laissee par defaut : l'entree Position de Noise est
    # declaree requise, et un noeud requis non branche est une erreur de compilation.
    # Effet de bord bienvenu : la frequence passe alors par un parametre, donc l'echelle
    # de variation devient reglable depuis l'instance comme le reste du look.
    p_macro_tiling = g.scalar('MacroTiling', 1.0 / 6000.0, 'Ground|Macro', -2560, 500)
    p_meso_tiling = g.scalar('MesoTiling', 1.0 / 1100.0, 'Ground|Macro', -2560, 680)
    # Detail ~25 cm : le grain qui se lit ENTRE les touffes, a 60 cm. A 1/70 (~1,1 m) chaque
    # trou du premier plan ne contenait qu'une valeur, et le sol restait une nappe. Le fondu
    # (DetailFade*) l'eteint avant 15-70 m, la ou cette frequence redeviendrait un motif.
    p_detail_tiling = g.scalar('DetailTiling', 1.0 / 16.0, 'Ground|Detail', -2560, 860)
    # FOREST_TERRAIN_P4 -- ECHELLE REGIONALE. La "macro" de 60 m a ete reglee pour un monde de
    # 9 600 uu ; a l'echelle 5 la carte en fait 192 000, et rien ne variait au-dela de quelques
    # dizaines de metres : un versant entier avait la meme teinte. ~600 m : des pans de colline
    # plus secs ou plus verts, ce que donne une carte mediterraneenne vue de loin.
    p_region_tiling = g.scalar('RegionTiling', 1.0 / 60000.0, 'Ground|Region', -2560, 1300)
    n_region = g.node(unreal.MaterialExpressionNoise, -2200, 1300,
                      scale=1.0, levels=2, quality=1, turbulence=False,
                      output_min=0.0, output_max=1.0,
                      noise_function=NOISE_FAST_GRADIENT_3D)
    g.link(g.mul(wp, '', p_region_tiling, '', -2380, 1300), '', n_region, 'World Position')

    n_macro = g.node(unreal.MaterialExpressionNoise, -2200, 520,
                     scale=1.0, levels=2, quality=1, turbulence=False,
                     output_min=0.0, output_max=1.0,
                     noise_function=NOISE_FAST_GRADIENT_3D)
    g.link(g.mul(wp, '', p_macro_tiling, '', -2380, 520), '', n_macro, 'World Position')
    n_meso = g.node(unreal.MaterialExpressionNoise, -2200, 700,
                    scale=1.0, levels=2, quality=1, turbulence=False,
                    output_min=0.0, output_max=1.0,
                    noise_function=NOISE_FAST_GRADIENT_3D)
    g.link(g.mul(wp, '', p_meso_tiling, '', -2380, 700), '', n_meso, 'World Position')

    # VectorNoise n'a pas de propriete Scale du tout : la frequence ne peut passer que
    # par la position.
    wp_detail = g.mul(wp, '', p_detail_tiling, '', -2320, 880)
    v_detail = g.node(unreal.MaterialExpressionVectorNoise, -2140, 880, quality=1, tiling=False,
                      noise_function=VECTOR_NOISE_PERLIN_GRADIENT)
    g.link(wp_detail, '', v_detail, '')  # VectorNoise n'a qu'une entree
    log('DETAIL_NOISE function=%s outputs=%s' % (
        v_detail.get_editor_property('noise_function'),
        list(mel.get_material_expression_output_names(v_detail))))
    # Le grain vient d'un Noise scalaire dedie, et le VectorNoise ne sert qu'au gradient.
    #
    # Ce n'est PAS une contrainte du moteur : VectorNoise "Perlin Gradient" rend bien un
    # float4 (RGB=gradient, A=scalaire) dans les deux traducteurs. Le seul masque alpha
    # qui refusait de compiler ici etait celui pose sur VertexColor, pas celui-ci.
    # On garde neanmoins deux bruits separes : le grain d'albedo et le relief micro ne
    # gagnent rien a etre le MEME champ -- correles, ils font ressortir le motif du bruit
    # au lieu de le cacher, ce qui est precisement le "papier peint procedural" interdit.
    # Le Fast Gradient 3D coute ~16 instructions et 1 lookup par niveau.
    d_grad = g.mask(v_detail, '', True, True, True, -1960, 1120)       # gradient signe
    n_grain = g.node(unreal.MaterialExpressionNoise, -2140, 1000,
                     scale=1.0, levels=2, quality=1, turbulence=False,
                     output_min=-1.0, output_max=1.0,
                     noise_function=NOISE_FAST_GRADIENT_3D)
    g.link(wp_detail, '', n_grain, 'World Position')
    d_grain = n_grain

    # --------------------------------------------------------------------- parametres
    P = 'Ground|'
    p_albedo = g.scalar('GroundAlbedoScale', 1.0, P + 'Albedo', -1900, -420)
    p_rock_col = g.vector('RockColor', (0.152, 0.147, 0.139), P + 'Albedo', -1900, -340)
    p_litter_tint = g.vector('LitterTint', (0.74, 0.68, 0.54), P + 'Albedo', -1900, -240)
    p_worked_tint = g.vector('WorkedTint', (1.14, 0.98, 0.74), P + 'Albedo', -1900, -140)
    p_macro_cool = g.vector('MacroTintCool', (0.82, 0.92, 0.84), P + 'Macro', -1900, -40)
    p_macro_warm = g.vector('MacroTintWarm', (1.20, 1.06, 0.84), P + 'Macro', -1900, 60)
    p_macro_amt = g.scalar('MacroContrast', 0.62, P + 'Macro', -1900, 160)
    p_meso_amt = g.scalar('MesoContrast', 0.30, P + 'Macro', -1900, 220)
    p_detail_amt = g.scalar('DetailContrast', 0.40, P + 'Detail', -1900, 280)
    # CES DEUX SEUILS SONT CALIBRES CONTRE L'EXAGERATION VERTICALE DU TERRAIN.
    #
    # La pente lue par le materiau est celle de la surface RENDUE, pas celle du
    # simulateur. TERRAIN_FORGE multiplie le relief au-dessus de la mer par
    # anastasis.Terrain.Forge.Exaggerate (3.6 par defaut) : une pente de 28 degres dans
    # la simulation en fait 64 une fois forgee. Les valeurs d'origine (0.20 / 0.52),
    # calibrees sur la surface non exageree, saturaient donc le masque presque partout
    # et rendaient tout le sol en roche pale -- le A/B de docs/visual/ground-001 le
    # montre : materiau eteint, la foret est verte ; allume, elle etait beige.
    #
    # 0.62 / 0.82 correspondent a une pente d'origine d'environ 35 a 45 degres, ce qui
    # est le terrain qui expose reellement sa roche. Si quelqu'un change Exaggerate,
    # ces deux nombres bougent avec lui -- c'est un couplage, et il est reglable depuis
    # l'instance sans recompiler.
    p_slope_lo = g.scalar('SlopeRockStart', 0.62, P + 'Rock', -1900, 340)
    p_slope_hi = g.scalar('SlopeRockEnd', 0.82, P + 'Rock', -1900, 400)
    p_damp_lo = g.scalar('DampStart', 0.60, P + 'Wet', -1900, 460)
    p_damp_hi = g.scalar('DampEnd', 0.96, P + 'Wet', -1900, 520)
    # 0.88, et pas 0.52 : l'ASSOMBRISSEMENT humide ne nous appartient plus.
    #
    # hydrology-surface projette Wetness dans la couleur de sommet (teinte WetMud, jusqu'a
    # 55 % vers une vase sombre). Ce materiau lisait la MEME Wetness depuis UV1.y et
    # assombrissait une seconde fois : au bord de l'eau le sol sortait deux fois plus
    # sombre qu'aucune des deux missions ne le voulait.
    #
    # Le partage retenu suit ce que chaque support sait faire. La couleur de sommet porte
    # la CHROMIE de la zone humide -- elle est large, basse frequence, et elle fonctionne
    # meme a anastasis.Terrain.GroundMaterial 0. Le materiau garde le LUSTRE : rugosite et
    # speculaire au trait de cote, que la couleur de sommet ne peut pas exprimer du tout.
    # DampDarken ne creuse donc plus qu'un dernier cran au contact de l'eau.
    p_damp_dark = g.scalar('DampDarken', 0.88, P + 'Wet', -1900, 580)
    p_soil_rough = g.scalar('SoilRoughness', 0.94, P + 'Roughness', -1900, 640)
    p_rock_rough = g.scalar('RockRoughness', 0.70, P + 'Roughness', -1900, 700)
    # 0.22, pas 0.38 : a 1,7 m le trait de cote doit accrocher le ciel. 0.38 restait
    # un sol mat, et le contact eau/terre ne se lisait pas.
    p_damp_rough = g.scalar('DampRoughness', 0.22, P + 'Roughness', -1900, 760)
    p_rough_grain = g.scalar('RoughnessGrain', 0.14, P + 'Roughness', -1900, 820)
    # Ces deux amplitudes ne valent que PRES du sol : DetailFade les eteint au loin.
    # BumpStrength n'est lisible que parce que le gradient est borne plus bas -- sans ce
    # bornage, le meme nombre veut dire n'importe quoi. Elle a encore baisse (0.26 -> 0.16)
    # apres TERRAIN_FORGE : le relief forge est plus raide, donc une meme inclinaison
    # supplementaire bascule beaucoup plus de surface a l'ombre.
    #
    # Pourquoi il a fallu en arriver la. A 0.55 sans attenuation, le sol entier se lisait
    # en vermicelles clairs et sombres depuis la vue aerienne -- le "papier peint
    # procedural" que la direction artistique interdit nommement. Rabaisser l'amplitude a
    # 0.14 reglait la vue aerienne et vidait la vue au sol : a deux metres, le sol
    # redevenait une surface peinte et lisse. Les deux captures sont dans docs/visual.
    #
    # Le probleme n'etait donc pas l'amplitude, c'etait la DISTANCE. Une structure de
    # 1,1 m fait une dizaine de pixels vue de 105 m : ce qu'on appelle "detail" devient un
    # MOTIF a cette distance, et aucune amplitude unique ne sert les deux bouts. Il est
    # donc fort de pres, nul de loin -- ou la macro et la meso portent seules la lecture.
    p_bump = g.scalar('BumpStrength', 0.16, P + 'Relief', -1900, 880)
    # INFERIEUR A 1 : sur les faces raides le relief micro s'ATTENUE, il ne s'amplifie pas.
    #
    # L'intuition de depart etait l'inverse -- la roche parait plus accidentee, donc plus
    # de bosse. Trois captures ont dit le contraire. Une face raide tourne deja le dos au
    # soleil : elle est a quelques degres du terminateur, et toute inclinaison
    # supplementaire la fait basculer entierement a l'ombre. On n'obtient pas du relief,
    # on obtient des taches noires -- la peau de leopard de
    # docs/visual/ground-001/E_regression_leopard_bump.png, revenue apres TERRAIN_FORGE
    # parce que le forgeage rend les pentes bien plus raides qu'avant.
    #
    # La roche tire son caractere de son albedo et de sa rugosite. Le relief micro, lui,
    # se lit sur le plat, ou il ne risque pas de franchir le terminateur.
    p_rock_bump = g.scalar('RockBumpScale', 0.55, P + 'Relief', -1900, 940)
    # Le talus vu a 1,7 m tient dans les quinze premiers metres. Un fondu qui ne
    # commence qu'a 15 m laisse ce plan entier au meme grain. 2,5 m : encore net
    # sous les pieds. 16 m : le grain est parti, il reste la couleur.
    p_fade_near = g.scalar('DetailFadeStart', 250.0, P + 'Detail', -1900, 1060)
    p_fade_far = g.scalar('DetailFadeEnd', 1600.0, P + 'Detail', -1900, 1120)
    p_soil_spec = g.scalar('SoilSpecular', 0.22, P + 'Roughness', -1900, 1000)
    # Le lustre du dernier metre, pas un miroir : le sec reste a 0.22.
    p_damp_spec = g.scalar('DampSpecular', 0.55, P + 'Roughness', -1900, 1040)

    # ------------------------------------------------------------------ fondu de detail
    # PixelDepth plutot qu'une distance a la camera calculee : c'est la profondeur deja
    # disponible, une instruction, et elle suffit -- on veut savoir a quelle distance le
    # pixel est vu, pas ou il se trouve dans le monde.
    depth = g.node(unreal.MaterialExpressionPixelDepth, -1700, 1000)
    detail_fade = g.one_minus(
        g.smoothstep(p_fade_near, '', p_fade_far, '', depth, '', -1540, 1000), '', -1380, 1000)
    grain_amt = g.mul(p_detail_amt, '', detail_fade, '', -1220, 1000)

    # ------------------------------------------------------------------ masque de roche
    # La roche apparait la ou la simulation dit "pierre" ET la ou la PENTE l'expose --
    # c'est la logique d'erosion que demande la direction artistique. Le bruit meso
    # casse la bande de pente : sans lui, la roche dessinerait une courbe de niveau,
    # ce qui se lit immediatement comme une fonction mathematique.
    slope_rock = g.smoothstep(p_slope_lo, '', p_slope_hi, '', slope, '', -1700, 340)
    rock_raw = g.add(w_rock, '', slope_rock, '', -1540, 300)
    meso_rock = g.addc(n_meso, '', 0.55, -1700, 700)
    rock_mask = g.sat(g.mul(rock_raw, '', meso_rock, '', -1380, 320), '', -1240, 320)

    # ------------------------------------------------------------------ masque humide
    # Bande etroite au bord de l'eau, pas la nappe d'humidite entiere : Wetness porte
    # jusqu'a 6,5 tuiles, ce qui ferait une aureole, pas un trait de cote. Le seuil
    # haut isole le dernier metre, celui qui est reellement detrempe.
    damp = g.smoothstep(p_damp_lo, '', p_damp_hi, '', wetness, '', -1700, 460)
    meso_damp = g.addc(g.mulc(n_meso, '', 0.8, -1700, 780), '', 0.6, -1540, 780)
    damp_mask = g.sat(g.mul(damp, '', meso_damp, '', -1380, 470), '', -1240, 470)

    # SOIL_CRUSADE_001: pilot bounds retained; shipped MI covers the world. Wetness is a
    # proximity proxy, NOT a sediment simulation. Slope and wetness gate every patch;
    # existing meso noise only breaks their edges. No new texture samples or geometry.
    soil = g.custom("""
float pilot = 1.0 - smoothstep(Radius * 0.8, max(Radius, 1.0), length(P.xy - Center.xy));
float enabled = saturate(Enabled) * pilot;
float slope = saturate(1.0 - normalize(N).z);
float open = 1.0 - saturate(Litter);
float patch = smoothstep(0.28, 0.72, Meso);
// Rendered slopes: 26 to 53 degrees, not the old pre-erosion 68 to 80 degrees.
float exposure = smoothstep(0.10, 0.40, slope) * (0.45 + 0.55 * patch);
float rock = max(Rock, exposure * (1.0 - 0.55 * Wet) * (0.45 + 0.55 * open));
// Moist low-gradient ground: a plausible fine-deposit signature, not proof of alluvium.
float fines = smoothstep(0.20, 0.68, Wet) * (1.0 - smoothstep(0.025, 0.16, slope));
fines *= (0.35 + 0.65 * patch) * (1.0 - rock) * (0.25 + 0.75 * open);
// Thin dry soil follows slope; no random bare islands on flat meadows.
float thin = smoothstep(0.025, 0.16, slope) * (1.0 - smoothstep(0.20, 0.60, Wet));
thin *= (0.30 + 0.70 * patch) * (1.0 - rock) * open;
// Close ground is a soil matrix under separate blades, not another green canopy.
// At distance the original color represents unresolved vegetation; retain that proxy.
Matrix = enabled * (1.0 - rock) * (1.0 - saturate(Worked)) * (0.55 + 0.15 * patch) * Near;
return float3(lerp(Rock, rock, enabled), saturate(thin * 0.65) * enabled, fines * enabled);
""", 'SoilHistory', CMOT_FLOAT3,
        ('P', 'N', 'Center', 'Radius', 'Enabled', 'Wet', 'Litter', 'Meso', 'Rock', 'Worked', 'Near'),
        (('Matrix', CMOT_FLOAT1),), -1180, 2480)
    for name, src in (('P', wp), ('N', nws), ('Wet', wetness), ('Litter', w_litter),
                      ('Meso', n_meso), ('Rock', rock_mask), ('Worked', w_worked), ('Near', detail_fade)):
        g.link(src, '', soil, name)
    g.link(g.vector('SoilPilotCenter', (96000.0, 110000.0, 0.0), P + 'SoilHistory', -1900, 2500), '', soil, 'Center')
    g.link(g.scalar('SoilPilotRadius', 38000.0, P + 'SoilHistory', -1900, 2560), '', soil, 'Radius')
    g.link(g.scalar('SoilHistory', 1.0, P + 'SoilHistory', -1900, 2620), '', soil, 'Enabled')
    rock_mask = g.mask(soil, '', True, False, False, -960, 2480)
    thin_soil = g.mask(soil, '', False, True, False, -960, 2540)
    fine_soil = g.mask(soil, '', False, False, True, -960, 2600)
    exposed_soil = g.sat(g.add(thin_soil, '', fine_soil, '', -780, 2520), '', -600, 2520)
    exposed_soil = g.sat(g.add(exposed_soil, '', soil, 'Matrix', -500, 2660), '', -340, 2660)
    # Reuse the existing fine mineral/mud photo; do not merely recolor grass into soil.
    texture_worked = g.lerp(w_worked, '', g.one_minus(w_litter, '', -780, 2660), '', exposed_soil, '', -600, 2600)

    # ------------------------------------------------------------------ textures photo
    # Le detail sous le metre, que la geometrie ne porte pas. Un seul noeud Custom : la
    # projection triplanaire branchee, le melange par hauteur et la pose de la normale
    # sont des boucles et des max, que des noeuds rendraient illisibles (et ~300 de plus).
    # Tout le reglage reste dans des parametres, donc dans l'instance.
    T = 'Ground|Texture'
    tex_params = {
        'AlbedoStrength': g.scalar('TexAlbedoStrength', 0.85, T, -1900, 1300),
        'AOStrength': g.scalar('TexAOStrength', 0.85, T, -1900, 1360),
        'RoughnessStrength': g.scalar('TexRoughnessStrength', 0.28, T, -1900, 1420),
        'NormalStrength': g.scalar('TexNormalStrength', 1.15, T, -1900, 1480),
        # Meme constat que RockBumpScale : une face raide est deja pres du terminateur,
        # une normale forte y fait des taches noires, pas de la roche.
        'RockNormalScale': g.scalar('TexRockNormalScale', 0.6, T, -1900, 1540),
        'HeightBlend': g.scalar('TexHeightBlend', 0.6, T, -1900, 1600),
        'BlendDepth': g.scalar('TexBlendDepth', 0.2, T, -1900, 1660),
    }
    for i, (family, size) in enumerate(TEX_FAMILIES):
        tex_params['TexSize' + family] = g.scalar('TexSize' + family, size, T, -1900, 1720 + 60 * i)
    # Pleine a 4 m, absente a 20 m. Le talus du cadrage a 1,7 m perd sa photo
    # avant l'eau ; au-dela le noeud ne lit plus aucune texture.
    p_tex_near = g.scalar('TexFadeStart', 400.0, T, -1900, 1960)
    p_tex_far = g.scalar('TexFadeEnd', 2000.0, T, -1900, 2020)
    tex_fade = g.one_minus(
        g.smoothstep(p_tex_near, '', p_tex_far, '', depth, '', -1540, 1960), '', -1380, 1960)

    tex_inputs = ['P', 'N', 'RockMask', 'RockW', 'Litter', 'Worked', 'Fade'] + list(tex_params.keys())
    tex_objects = {}
    for family, _ in TEX_FAMILIES:
        for suffix, sampler in (('AH', SAMPLER_COLOR), ('NR', SAMPLER_LINEAR_COLOR)):
            name = family + suffix
            tex_inputs.append(name)
            tex_objects[name] = g.node(unreal.MaterialExpressionTextureObjectParameter,
                                       -1700, 2100 + 80 * len(tex_objects),
                                       parameter_name='Tex' + name, group=T,
                                       texture=textures[(family, suffix)], sampler_type=sampler)
    tex = g.custom(ground_texture_hlsl(), 'GroundTexture', CMOT_FLOAT3, tex_inputs,
                   (('TexNormal', CMOT_FLOAT3), ('TexRough', CMOT_FLOAT1)), -1100, 1500)
    for name, src in (('P', wp), ('N', nws), ('RockMask', rock_mask), ('RockW', w_rock),
                      ('Litter', w_litter), ('Worked', texture_worked), ('Fade', tex_fade)):
        g.link(src, '', tex, name)
    for name, src in tex_params.items():
        g.link(src, '', tex, name)
    for name, src in tex_objects.items():
        g.link(src, '', tex, name)

    # SOIL_SLOPE_002: existing rock fabric, own distance band, no new geometry/assets.
    slope_surface = g.custom(slope_surface_hlsl(), 'SlopeSurface', CMOT_FLOAT3,
        ('P','N','Depth','Litter','Wet','Enabled','Size','RockAH','RockNR'),
        (('SlopeNormal', CMOT_FLOAT3),), -500, 3100)
    for name, src in (('P',wp),('N',nws),('Depth',depth),('Litter',w_litter),('Wet',wetness),
                      ('RockAH',tex_objects['RockAH']),('RockNR',tex_objects['RockNR'])):
        g.link(src, '', slope_surface, name)
    g.link(g.scalar('SlopeSurface',1.0,P+'SoilHistory',-1700,3100),'',slope_surface,'Enabled')
    g.link(g.scalar('SlopeSurfaceSize',900.0,P+'SoilHistory',-1700,3160),'',slope_surface,'Size')

    # ------------------------------------------------------------------------- albedo
    base = g.mask(vc, '', True, True, True, -1240, -240)
    base = g.lerp(base, '', g.mul(base, '', p_litter_tint, '', -1080, -200), '', w_litter, '', -920, -240)
    base = g.lerp(base, '', g.mul(base, '', p_worked_tint, '', -760, -200), '', w_worked, '', -600, -240)
    base = g.lerp(base, '', p_rock_col, '', rock_mask, '', -440, -240)

    # Masses macro : une modulation de TEINTE, pas une seconde couleur posee par-dessus.
    macro_tint = g.lerp(p_macro_cool, '', p_macro_warm, '', n_macro, '', -440, 40)
    base = g.lerp(base, '', g.mul(base, '', macro_tint, '', -280, 0), '', p_macro_amt, '', -120, -240)

    # FOREST_TERRAIN_P4 -- pans regionaux, puis terre rouge (terra rossa) a nu sur les pentes
    # seches : le sol mediterraneen se montre entre l'herbe et le maquis, pas sur le plat
    # gras ni au bord de l'eau, jamais sur la roche (elle a deja sa couleur).
    p_region_dry = g.vector('RegionDryTint', (1.16, 1.06, 0.74), P + 'Region', -1900, 1360)
    p_region_green = g.vector('RegionGreenTint', (0.90, 1.00, 0.86), P + 'Region', -1900, 1440)
    p_region_amt = g.scalar('RegionContrast', 0.55, P + 'Region', -1900, 1520)
    region_tint = g.lerp(p_region_green, '', p_region_dry, '', n_region, '', -440, 1360)
    base = g.lerp(base, '', g.mul(base, '', region_tint, '', -280, 1320), '', p_region_amt, '', -120, 1300)
    p_rossa = g.vector('TerraRossa', (0.215, 0.125, 0.075), P + 'Soil', -1900, 1600)
    p_rossa_amt = g.scalar('TerraRossaAmount', 0.55, P + 'Soil', -1900, 1680)
    rossa_patch = g.smoothstep(g.const(0.55, -1700, 1600), '', g.const(0.85, -1700, 1640), '', n_meso, '', -1540, 1600)
    rossa_slope = g.smoothstep(g.const(0.04, -1700, 1700), '', g.const(0.20, -1700, 1740), '', slope, '', -1540, 1700)
    rossa_mask = g.mul(rossa_patch, '', rossa_slope, '', -1380, 1640)
    rossa_mask = g.mul(rossa_mask, '', g.one_minus(damp_mask, '', -1380, 1720), '', -1220, 1640)
    rossa_mask = g.mul(rossa_mask, '', g.one_minus(rock_mask, '', -1220, 1720), '', -1060, 1640)
    rossa_mask = g.mul(rossa_mask, '', g.addc(g.mulc(n_region, '', 0.6, -1060, 1720), '', 0.4, -900, 1720), '', -900, 1640)
    rossa_mask = g.sat(g.mul(rossa_mask, '', p_rossa_amt, '', -740, 1640), '', -580, 1640)
    base = g.lerp(base, '', p_rossa, '', rossa_mask, '', -120, 1500)

    # Mineral earth and fines remain restrained linear albedos. Existing wet-bank
    # chroma is retained (partial blend), as is the simulation Forest litter mask.
    dry_earth = g.vector('ThinSoilColor', (0.135, 0.105, 0.070), P + 'SoilHistory', -1900, 2700)
    fine_earth = g.vector('FineSoilColor', (0.105, 0.092, 0.072), P + 'SoilHistory', -1900, 2760)
    organic_earth = g.vector('OrganicSoilColor', (0.075, 0.060, 0.040), P + 'SoilHistory', -1900, 2820)
    soil_matrix_color = g.lerp(dry_earth, '', organic_earth, '', w_litter, '', -140, 2900)
    base = g.lerp(base, '', soil_matrix_color, '', soil, 'Matrix', 20, 2900)
    base = g.lerp(base, '', dry_earth, '', thin_soil, '', 20, 1500)
    base = g.lerp(base, '', fine_earth, '', g.mulc(fine_soil, '', 0.65, -120, 1580), '', 180, 1500)

    # Meso et grain : des modulations de VALEUR centrees sur 1, donc neutres en moyenne.
    # Moduler ainsi plutot que multiplier par le bruit lui-meme evite d'assombrir
    # globalement le sol a chaque couche ajoutee. Le recentrage differe parce que les
    # deux bruits ne sortent pas dans la meme plage : la meso est en [0,1] et se recentre
    # par -0.5, le grain sort deja en [-1,1] et se module tel quel.
    meso_val = g.addc(g.mul(p_meso_amt, '', g.subc(n_meso, '', 0.5, -440, 740), '', -280, 740), '', 1.0, -120, 740)
    base = g.mul(base, '', meso_val, '', 40, -240)
    grain_val = g.addc(g.mul(grain_amt, '', d_grain, '', -280, 900), '', 1.0, -120, 900)
    base = g.mul(base, '', grain_val, '', 200, -240)
    # Le grain de valeur, meme a 25 cm, module le jaune deja la : il ne le quitte pas.
    # Le creux (d_grain < 0) tire vers une terre, de pres seulement (detail_fade).
    # Valeurs capturees (prairie_low, label ground-soil) : le premier plan reste jaune,
    # moyenne 137 -> 139, 17,5 % des pixels du bas bougent. Un disque au pied de chaque
    # touffe a ete essaye puis retire : sur le materiau feuillage il se lit comme un trou.
    p_gap_soil = g.vector('GapSoil', (0.16, 0.12, 0.07), P + 'Detail', -1900, 1180)
    p_gap_amt = g.scalar('GapSoilAmount', 0.70, P + 'Detail', -1900, 1240)
    gap_dark = g.sat(g.mulc(d_grain, '', -1.0, -440, 980), '', -280, 980)
    gap = g.mul(gap_dark, '', g.mul(p_gap_amt, '', detail_fade, '', -280, 1040), '', -120, 980)
    base = g.lerp(base, '', p_gap_soil, '', gap, '', 240, -160)
    # Detail photo : un facteur neutre en moyenne, pose APRES les teintes calees.
    base = g.mul(base, '', tex, '', 280, -240)

    # Sol detrempe : plus sombre, comme un sol reellement mouille.
    dark = g.mul(base, '', p_damp_dark, '', 360, -120)
    damp_lerp = g.lerp(base, '', dark, '', damp_mask, '', 520, -120)
    base_final = g.mul(damp_lerp, '', p_albedo, '', 680, -160)

    base_final = g.mul(base_final, '', slope_surface, '', 900, 3100)
    g.prop(base_final, '', unreal.MaterialProperty.MP_BASE_COLOR)

    # ---------------------------------------------------------------------- rugosite
    r = g.lerp(p_soil_rough, '', p_rock_rough, '', rock_mask, '', -120, 400)
    r = g.lerp(r, '', g.const(0.86, -120, 2820), '', thin_soil, '', 0, 2800)
    r = g.lerp(r, '', g.const(0.72, -120, 2880), '', fine_soil, '', 160, 2800)
    r = g.lerp(r, '', p_damp_rough, '', damp_mask, '', 40, 400)
    r = g.add(r, '', g.mul(g.mul(p_rough_grain, '', detail_fade, '', 200, 520), '', d_grain, '', 200, 460), '', 360, 400)
    r = g.add(r, '', tex, 'TexRough', 440, 400)
    r = g.sat(r, '', 520, 400)
    g.prop(r, '', unreal.MaterialProperty.MP_ROUGHNESS)
    spec = g.lerp(p_soil_spec, '', p_damp_spec, '', damp_mask, '', 520, 520)
    g.prop(spec, '', unreal.MaterialProperty.MP_SPECULAR)

    # ------------------------------------------------------------------------ normale
    # Relief micro analytique. Le gradient du bruit est une direction dans l'espace
    # monde ; sa composante le long de la normale ne dit rien de la pente locale de la
    # surface, on la retire. Ce qui reste est tangent, et le soustraire a la normale
    # incline le point exactement comme le ferait une bosse de hauteur.
    #
    # C'est la seule micro-relief du sol : la geometrie est a 1 sommet par metre, donc
    # en dessous du metre il n'y a AUCUNE forme. Sans cette normale le sol est
    # parfaitement lisse, ce qui est la premiere raison pour laquelle il se lisait
    # comme une maquette peinte.
    g_dot_n = g.node(unreal.MaterialExpressionDotProduct, -1700, 1120)
    g.link(d_grad, '', g_dot_n, 'A')
    g.link(nws, '', g_dot_n, 'B')
    along = g.mul(nws, '', g_dot_n, '', -1540, 1120)
    tangential = g.sub(d_grad, '', along, '', -1380, 1120)

    # Saturation douce du gradient : G / (1 + |G|), donc une longueur toujours < 1.
    #
    # Sans elle, BumpStrength n'a aucun sens physique. Le gradient d'un bruit de Perlin
    # n'est pas norme -- sa longueur depasse couramment 1 -- si bien qu'a 0.45 la normale
    # basculait de plus de 40 degres par endroits. Sous un soleil rasant (-38 deg), cela
    # ne produit pas du relief : cela produit des taches ENTIEREMENT a l'ombre. Le sol
    # au bord de l'eau se lisait alors comme une peau de leopard, noire et blanche.
    # Voir docs/visual/ground-001.
    #
    # Bornee, la strength redevient lisible : c'est la tangente de l'inclinaison maximale.
    # 0.35 donne 19 degres au pire, et beaucoup moins la ou le bruit est plat.
    grad_len = g.node(unreal.MaterialExpressionLength, -1380, 1240)
    g.link(tangential, '', grad_len, '')
    tangential = g.div(tangential, '', g.addc(grad_len, '', 1.0, -1240, 1240), '', -1120, 1120)
    bump_amt = g.mul(p_bump, '', g.lerp(g.const(1.0, -1380, 1260), '', p_rock_bump, '', rock_mask, '', -1220, 1260), '', -1060, 1200)
    bump_amt = g.mul(bump_amt, '', detail_fade, '', -940, 1200)
    perturb = g.mul(tangential, '', bump_amt, '', -740, 1120)
    out_normal = g.node(unreal.MaterialExpressionNormalize, -580, 1120)
    # Le relief micro procedural s'incline desormais depuis la normale PHOTO, qui est
    # deja posee sur la normale de sommet : les deux echelles se cumulent (1,6 m et
    # sous le metre) au lieu de se remplacer.
    g.link(g.sub(tex, 'TexNormal', perturb, '', -740, 1000), '', out_normal, '')
    slope_delta = g.sub(slope_surface, 'SlopeNormal', nws, '', 400, 3200)
    normal_combined = g.node(unreal.MaterialExpressionNormalize, 720, 3200)
    g.link(g.add(out_normal, '', slope_delta, '', 560, 3200), '', normal_combined, '')
    g.prop(normal_combined, '', unreal.MaterialProperty.MP_NORMAL)

    # LE garde-fou qui manquait au premier jet. recompile_material RETOURNE les erreurs
    # du compilateur ; les ignorer produit un asset qui s'enregistre tres bien et que le
    # moteur remplace silencieusement par le Default Material au rendu. C'est exactement
    # ce qui est arrive : un sol beige uniforme, sans eau ni semantique, qu'on aurait pu
    # prendre pour un mauvais reglage artistique au lieu d'un materiau mort.
    # Marqueur pour ground-material.ps1 : un "Failed to compile Material" APRES lui est le
    # vrai verdict ; ceux d'avant sont les etats intermediaires du graphe en construction.
    log('RECOMPILE_BEGIN')
    errors = list(mel.recompile_material(mat))
    if errors:
        for e in errors:
            unreal.log_error('GROUND_MATERIAL COMPILE_ERROR ' + e)
        raise RuntimeError('M_AnastasisGround ne compile pas (%d erreurs) -- asset non enregistre'
                           % len(errors))
    eal.save_asset(MASTER)
    log('MASTER_SAVED expressions=%d' % mel.get_num_material_expressions(mat))
    return mat


def publish_soil_scope(mi):
    """Publish the validated soil treatment over the full supported world footprint.

    The master retains its 380 m diagnostic pilot. The shipped instance explicitly
    covers the world; slope/wetness/litter and near-detail fade still gate the effect.
    SoilHistory=0 is the reversible switch, radius=38000 restores the original pilot.
    """
    radius = 10000000.0
    # UE5.8 setter returns false unconditionally; validate the actual readback.
    mel.set_material_instance_scalar_parameter_value(mi, 'SoilPilotRadius', radius)
    mel.update_material_instance(mi)
    got = mel.get_material_instance_scalar_parameter_value(mi, 'SoilPilotRadius')
    if abs(got - radius) > 1.0:
        raise RuntimeError('SoilPilotRadius readback mismatch: %s' % got)
    if not eal.save_asset(INSTANCE):
        raise RuntimeError('MI_AnastasisGround save failed')
    log('SOIL_SCOPE_SAVED radius_cm=10000000 switch=SoilHistory')


def build_instance(master):
    mi = tools.create_asset('MI_AnastasisGround', PKG, unreal.MaterialInstanceConstant,
                            unreal.MaterialInstanceConstantFactoryNew())
    if mi is None:
        raise RuntimeError('create_asset MI_AnastasisGround failed')
    if mi.get_name() != 'MI_AnastasisGround':
        raise RuntimeError('asset renomme en %s : l ancien n a pas ete supprime' % mi.get_name())
    mel.set_material_instance_parent(mi, master)
    publish_soil_scope(mi)
    log('INSTANCE_SAVED parent=M_AnastasisGround overrides=1 (SoilPilotRadius)')
    return mi


def release_scene_references():
    """Detache la scene des materiaux avant de les supprimer.

    L'editeur ouvre sa carte de demarrage, Lvl_AnastasisSlice, qui contient un
    AAnastasisWorldEmbodiment. Celui-ci fait exactement son travail : il charge le
    materiau de sol et le pose sur sa surface. Il le REFERENCE donc, par son UPROPERTY
    GroundMaterial et par les OverrideMaterials de sa ProceduralMeshComponent.

    delete_asset echoue alors -- "est en cours d'utilisation" -- et le moteur laisse
    derriere lui un paquet qu'il qualifie lui-meme de "potentially corrupt". On retire
    donc l'acteur d'abord. Rien n'est sauvegarde : le niveau n'est pas ecrit, et de
    toute facon il ne porte aucune verite de monde (tout est regenere depuis la graine).
    """
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    removed = 0
    for actor in eas.get_all_level_actors():
        if actor.get_class().get_name() == 'AnastasisWorldEmbodiment':
            eas.destroy_actor(actor)
            removed += 1
    unreal.SystemLibrary.collect_garbage()
    log('SCENE_RELEASED embodiments_removed=%d' % removed)


def run():
    if REBUILD:
        release_scene_references()
        for path in (INSTANCE, MASTER):
            if eal.does_asset_exist(path):
                eal.delete_asset(path)
                log('DELETED ' + path)
        # Sans ce ramassage, le paquet supprime reste charge et occupe encore son nom :
        # create_asset rend alors None (observe sur MI_AnastasisGround) ou, pire, cree
        # un MI_AnastasisGround1 que le C++ ne chargera jamais.
        unreal.SystemLibrary.collect_garbage()

    # REBUILD force la recreation sans reinterroger does_asset_exist : juste apres un
    # delete_asset le registre d'assets repond encore True, et on repartait alors sur
    # l'ancienne instance -- pointant vers un maitre qui venait d'etre supprime.
    # Les textures d'abord, et meme sans REBUILD quand on les reimporte : le maitre
    # existant les reference par chemin, il voit donc les nouvelles sans regeneration.
    if TEX_REIMPORT and not REBUILD:
        import_textures()

    if REBUILD or not eal.does_asset_exist(MASTER):
        log('CREATE ' + MASTER)
        master_asset = build_master(import_textures())
    else:
        master_asset = unreal.load_asset(MASTER)
        log('LOAD ' + MASTER)

    if REBUILD or not eal.does_asset_exist(INSTANCE):
        log('CREATE ' + INSTANCE)
        build_instance(master_asset)
    else:
        log('LOAD ' + INSTANCE)

    # Remettre le sol en scene. Deux raisons, et la seconde compte autant que la premiere :
    #
    #   1. get_statistics ne lit pas le graphe, il lit la shader map COMPILEE. Tant que
    #      rien ne demande le materiau, aucune map n'est produite et le budget reste
    #      introuvable. Le terrain qui s'en sert est ce qui la demande.
    #   2. recharger la carte scellee rejoue le chemin REEL -- ResolveSliceMaterial,
    #      chargement par chemin exact, pose sur la surface. Une instance mal nommee ou
    #      un maitre mal reference se voit ici, pas trois runs plus tard dans une capture.
    unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level(
        '/Game/Anastasis/Maps/Lvl_AnastasisSlice')
    log('SCENE_RELOADED')


def report_stats():
    """Budget, mesure et non pas estime.

    get_statistics ne lit pas le graphe : il lit la SHADER MAP compilee. Juste apres
    recompile_material celle-ci n'existe pas encore -- le compilateur de shaders est
    asynchrone -- et l'appel rend 0 partout, samplers=-1. Publier ces zeros comme un
    budget serait pire que ne rien publier. On attend donc que la map soit la, et on
    dit franchement si elle ne vient pas.
    """
    done = []
    for label, path in (('master', MASTER), ('instance', INSTANCE)):
        stats = mel.get_statistics(unreal.load_asset(path))
        samplers = stats.get_editor_property('num_samplers')
        pixel = stats.get_editor_property('num_pixel_shader_instructions')
        if samplers < 0 or pixel <= 0:
            return False
        done.append('STATS %s pixel_instructions=%d vertex_instructions=%d samplers=%d '
                    'pixel_texture_samples=%d uv_scalars=%d' % (
                        label, pixel,
                        stats.get_editor_property('num_vertex_shader_instructions'),
                        samplers,
                        stats.get_editor_property('num_pixel_texture_samples'),
                        stats.get_editor_property('num_uv_scalars')))
    for line in done:
        log(line)
    return True


# L'editeur doit rendre la main quoi qu'il arrive : lance en -unattended il n'a personne
# pour fermer sa fenetre, et un script qui leve le laisserait tourner jusqu'au timeout du
# runner -- ce qui rend un echec de cablage indiscernable d'un editeur bloque.
_t0 = time.monotonic()
_handle = None


def _finish(msg, error=False):
    (unreal.log_error if error else unreal.log)('GROUND_MATERIAL ' + msg)
    if _handle is not None:
        unreal.unregister_slate_post_tick_callback(_handle)
    unreal.SystemLibrary.quit_editor()


def _tick(delta):
    # Un tick Slate, pas une boucle d'attente : c'est le tick qui fait avancer le
    # compilateur de shaders. Une boucle bloquante attendrait pour toujours.
    elapsed = time.monotonic() - _t0
    if report_stats():
        _finish('COMPLETE')
    elif elapsed > 240.0:
        # Pas d'erreur : les assets sont bons, c'est la MESURE qui manque. La distinction
        # compte -- un budget non mesure se rapporte comme non mesure, pas comme un echec
        # de generation, et surtout pas comme un budget de zero.
        unreal.log_warning('GROUND_MATERIAL STATS_UNAVAILABLE shader map non compilee apres %.0fs' % elapsed)
        _finish('COMPLETE')


try:
    run()
    _handle = unreal.register_slate_post_tick_callback(_tick)
except Exception:
    import traceback
    unreal.log_error('GROUND_MATERIAL FAILED ' + traceback.format_exc())
    unreal.SystemLibrary.quit_editor()
