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

Variables d'environnement :
  ANASTASIS_GROUND_REBUILD  "1" regenere les deux assets (ecrase l'existant)
"""
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


# ENoiseFunction : index 2 = NOISEFUNCTION_GradientTex3D ("Fast Gradient - 3D Texture",
# ~16 instructions et 1 lookup par niveau, la seule variante assez bon marche pour deux
# octaves de decor). EVectorNoiseFunction : index 2 = VNF_GradientALU ("Perlin Gradient",
# RGB = gradient, A = scalaire).
NOISE_FAST_GRADIENT_3D = enum_of(unreal.NoiseFunction, 2,
                                 'NOISEFUNCTION_GRADIENT_TEX3_D', 'NOISEFUNCTION_GRADIENT_TEX3D',
                                 'NOISEFUNCTION_GradientTex3D')
VECTOR_NOISE_PERLIN_GRADIENT = enum_of(unreal.VectorNoiseFunction, 2,
                                       'VNF_GRADIENT_ALU', 'VNF_GRADIENTALU', 'VNF_GradientALU')


def build_master():
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
    # Detail ~1.6 m : le grain, et la SEULE source de relief micro -- VectorNoise
    #   "Perlin Gradient" rend le gradient (RGB) ET le scalaire (A) en une evaluation,
    #   ce qui donne une normale analytique sans payer trois bruits.
    #
    # Fast Gradient 3D se repete tous les 16 en position mise a l'echelle : a 1/6000
    # cela fait 96 000 UU, dix fois le monde (9 600 UU). Aucune repetition visible.
    # La position est CONNECTEE, pas laissee par defaut : l'entree Position de Noise est
    # declaree requise, et un noeud requis non branche est une erreur de compilation.
    # Effet de bord bienvenu : la frequence passe alors par un parametre, donc l'echelle
    # de variation devient reglable depuis l'instance comme le reste du look.
    p_macro_tiling = g.scalar('MacroTiling', 1.0 / 6000.0, 'Ground|Macro', -2560, 500)
    p_meso_tiling = g.scalar('MesoTiling', 1.0 / 1100.0, 'Ground|Macro', -2560, 680)
    p_detail_tiling = g.scalar('DetailTiling', 1.0 / 70.0, 'Ground|Detail', -2560, 860)

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
    p_detail_amt = g.scalar('DetailContrast', 0.26, P + 'Detail', -1900, 280)
    p_slope_lo = g.scalar('SlopeRockStart', 0.20, P + 'Rock', -1900, 340)
    p_slope_hi = g.scalar('SlopeRockEnd', 0.52, P + 'Rock', -1900, 400)
    p_damp_lo = g.scalar('DampStart', 0.60, P + 'Wet', -1900, 460)
    p_damp_hi = g.scalar('DampEnd', 0.96, P + 'Wet', -1900, 520)
    p_damp_dark = g.scalar('DampDarken', 0.52, P + 'Wet', -1900, 580)
    p_soil_rough = g.scalar('SoilRoughness', 0.94, P + 'Roughness', -1900, 640)
    p_rock_rough = g.scalar('RockRoughness', 0.70, P + 'Roughness', -1900, 700)
    p_damp_rough = g.scalar('DampRoughness', 0.38, P + 'Roughness', -1900, 760)
    p_rough_grain = g.scalar('RoughnessGrain', 0.14, P + 'Roughness', -1900, 820)
    # Ces deux amplitudes ne valent que PRES du sol : DetailFade les eteint au loin.
    # BumpStrength n'est lisible que parce que le gradient est borne plus bas -- sans ce
    # bornage, le meme nombre veut dire n'importe quoi.
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
    p_bump = g.scalar('BumpStrength', 0.26, P + 'Relief', -1900, 880)
    # Quasi neutre, et c'est voulu. A 1.6 les faces rocheuses raides basculaient au-dela
    # du terminateur solaire et devenaient des taches noires franches : sur une pente qui
    # tourne deja le dos au soleil, amplifier l'inclinaison ne donne pas du relief, cela
    # donne de l'ombre pleine. La roche tire son caractere de son albedo et de sa
    # rugosite, pas d'une bosse plus creusee.
    p_rock_bump = g.scalar('RockBumpBoost', 1.15, P + 'Relief', -1900, 940)
    # 15 m : encore du relief sous les pieds. 70 m : plus rien, bien avant que la
    # structure n'atteigne la taille d'un motif a l'ecran.
    p_fade_near = g.scalar('DetailFadeStart', 1500.0, P + 'Detail', -1900, 1060)
    p_fade_far = g.scalar('DetailFadeEnd', 7000.0, P + 'Detail', -1900, 1120)
    p_soil_spec = g.scalar('SoilSpecular', 0.22, P + 'Roughness', -1900, 1000)

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

    # ------------------------------------------------------------------------- albedo
    base = g.mask(vc, '', True, True, True, -1240, -240)
    base = g.lerp(base, '', g.mul(base, '', p_litter_tint, '', -1080, -200), '', w_litter, '', -920, -240)
    base = g.lerp(base, '', g.mul(base, '', p_worked_tint, '', -760, -200), '', w_worked, '', -600, -240)
    base = g.lerp(base, '', p_rock_col, '', rock_mask, '', -440, -240)

    # Masses macro : une modulation de TEINTE, pas une seconde couleur posee par-dessus.
    macro_tint = g.lerp(p_macro_cool, '', p_macro_warm, '', n_macro, '', -440, 40)
    base = g.lerp(base, '', g.mul(base, '', macro_tint, '', -280, 0), '', p_macro_amt, '', -120, -240)

    # Meso et grain : des modulations de VALEUR centrees sur 1, donc neutres en moyenne.
    # Moduler ainsi plutot que multiplier par le bruit lui-meme evite d'assombrir
    # globalement le sol a chaque couche ajoutee. Le recentrage differe parce que les
    # deux bruits ne sortent pas dans la meme plage : la meso est en [0,1] et se recentre
    # par -0.5, le grain sort deja en [-1,1] et se module tel quel.
    meso_val = g.addc(g.mul(p_meso_amt, '', g.subc(n_meso, '', 0.5, -440, 740), '', -280, 740), '', 1.0, -120, 740)
    base = g.mul(base, '', meso_val, '', 40, -240)
    grain_val = g.addc(g.mul(grain_amt, '', d_grain, '', -280, 900), '', 1.0, -120, 900)
    base = g.mul(base, '', grain_val, '', 200, -240)

    # Sol detrempe : plus sombre, comme un sol reellement mouille.
    dark = g.mul(base, '', p_damp_dark, '', 360, -120)
    damp_lerp = g.lerp(base, '', dark, '', damp_mask, '', 520, -120)
    base_final = g.mul(damp_lerp, '', p_albedo, '', 680, -160)

    g.prop(base_final, '', unreal.MaterialProperty.MP_BASE_COLOR)

    # ---------------------------------------------------------------------- rugosite
    r = g.lerp(p_soil_rough, '', p_rock_rough, '', rock_mask, '', -120, 400)
    r = g.lerp(r, '', p_damp_rough, '', damp_mask, '', 40, 400)
    r = g.add(r, '', g.mul(g.mul(p_rough_grain, '', detail_fade, '', 200, 520), '', d_grain, '', 200, 460), '', 360, 400)
    r = g.sat(r, '', 520, 400)
    g.prop(r, '', unreal.MaterialProperty.MP_ROUGHNESS)
    g.prop(p_soil_spec, '', unreal.MaterialProperty.MP_SPECULAR)

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
    g.link(g.sub(nws, '', perturb, '', -740, 1000), '', out_normal, '')
    g.prop(out_normal, '', unreal.MaterialProperty.MP_NORMAL)

    # LE garde-fou qui manquait au premier jet. recompile_material RETOURNE les erreurs
    # du compilateur ; les ignorer produit un asset qui s'enregistre tres bien et que le
    # moteur remplace silencieusement par le Default Material au rendu. C'est exactement
    # ce qui est arrive : un sol beige uniforme, sans eau ni semantique, qu'on aurait pu
    # prendre pour un mauvais reglage artistique au lieu d'un materiau mort.
    errors = list(mel.recompile_material(mat))
    if errors:
        for e in errors:
            unreal.log_error('GROUND_MATERIAL COMPILE_ERROR ' + e)
        raise RuntimeError('M_AnastasisGround ne compile pas (%d erreurs) -- asset non enregistre'
                           % len(errors))
    eal.save_asset(MASTER)
    log('MASTER_SAVED expressions=%d' % mel.get_num_material_expressions(mat))
    return mat


def build_instance(master):
    mi = tools.create_asset('MI_AnastasisGround', PKG, unreal.MaterialInstanceConstant,
                            unreal.MaterialInstanceConstantFactoryNew())
    if mi is None:
        raise RuntimeError('create_asset MI_AnastasisGround failed')
    if mi.get_name() != 'MI_AnastasisGround':
        raise RuntimeError('asset renomme en %s : l ancien n a pas ete supprime' % mi.get_name())
    mel.set_material_instance_parent(mi, master)
    # AUCUN override pose ici, volontairement. L'instance est une surface de retouche
    # vide : tout ce qu'elle montre vient du maitre, donc il n'existe qu'UNE valeur par
    # defaut, et elle est visible au meme endroit que la structure qui la consomme. Un
    # override pose d'avance serait une seconde verite, et la plus dure a trouver.
    eal.save_asset(INSTANCE)
    log('INSTANCE_SAVED parent=M_AnastasisGround overrides=0')
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
    if REBUILD or not eal.does_asset_exist(MASTER):
        log('CREATE ' + MASTER)
        master_asset = build_master()
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
