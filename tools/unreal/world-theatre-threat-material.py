"""WORLD_THEATRE v2.2 -- source d'autorite de deux materiaux, /Game/WorldTheatre/.

M_WorldTheatreSmoke  colonne de fumee : translucide eclaire (la fumee est noire la nuit, grise au soleil), double face.
                     Densite = alpha de sommet (le maillage la porte : monte au pied, s'efface au sommet) x bord doux
                     (une colonne se lit dense au centre, transparente sur ses bords) x volutes. Les volutes sont des
                     sinus PERIODIQUES autour de la colonne (U) qui montent avec le temps : aucune couture, aucune texture.
                     Parametres : Opacity, Tint, Seed.
M_WorldTheatreFire   feu de signaux : non eclaire, additif, double face, coeur en goutte qui vacille. L'exposition du
                     projet est fixe (EV100 14) : sans compensation, un foyer a 10 km reste noir la nuit. Le materiau
                     multiplie donc par EyeAdaptationInverse (luminance a l'ecran stable) s'il existe, sinon par Gain.
                     Parametres : Intensity, Gain, Seed.

Les deux sont pilotes par UAnastasisWorldTheatreSubsystem (instances dynamiques) ; les valeurs vivent dans le code et
les CVars anastasis.Theatre.Threat.*, pas dans l'asset. Recrees a chaque lancement (rien ne les reference en dur).
Lancer par world-theatre-threat-material.ps1.
"""
import os
import unreal

DIR = '/Game/WorldTheatre'
mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary

SMOKE_OPACITY = '''
float t = T * 0.035 + Seed;
float a = UV.x * 6.2831853;
float n = 0.5 + 0.28 * sin(UV.y * 9.0 - t * 6.0 + sin(a * 3.0 + Seed) * 1.6)
              + 0.22 * sin(UV.y * 23.0 - t * 11.0 + cos(a * 5.0 + Seed * 1.7) * 2.1);
// Bord doux (run 1 : bords droits, colonne trop pleine) : la densite tombe vers la silhouette et se troue de volutes.
float facing = saturate(abs(dot(normalize(N), normalize(V))));
float edge = facing * facing * (3.0 - 2.0 * facing);
float holes = smoothstep(0.25, 0.75, n);
return saturate(Opacity * VC * edge * (0.25 + 0.75 * holes));
'''

SMOKE_GLOW = '''
// La nuit, un incendie se voit a la lueur rouge sous sa fumee : emissif concentre au pied de la colonne.
// Run 2 : lueur en puissance 3, gain 6 -> colonnes blanches de haut en bas, torches fantomes. Puissance 6 : seul le
// premier sixieme de la colonne rougeoie ; la couleur est bornee pour rester orange sous l'exposition de nuit.
float base = pow(saturate(1.0 - UV.y), 6.0);
float t = T * 0.035 + Seed;
float flick = 0.8 + 0.2 * sin(t * 40.0 + UV.x * 12.566);
return min(float3(1.0, 0.33, 0.08) * base * flick * Glow * VC, float3(0.9, 0.3, 0.07));
'''

SMOKE_COLOR = '''
float t = T * 0.035 + Seed;
float shade = 0.82 + 0.18 * sin(UV.y * 13.0 - t * 7.0 + UV.x * 18.85);
return Tint * shade;
'''

FIRE_EMISSIVE = '''
// Vu de loin, un feu est une LUEUR, pas une forme (run 1 : foyer de 8 m a 10 km, sous le pixel, invisible) :
// coeur chaud tres petit + halo doux qui remplit le quad (le quad est dimensionne a la distance par le theatre).
float r = length((UV - float2(0.5, 0.5)) * 2.0);
float core = exp(-r * r * 60.0);
float halo = exp(-r * r * 6.0) * 0.35;
float flick = 0.75 + 0.25 * sin(T * 13.0 + Seed) * sin(T * 7.3 + Seed * 2.1);
float3 c = float3(1.0, 0.55, 0.18) * halo + float3(1.0, 0.85, 0.6) * core;
return c * flick * Intensity * Gain;
'''


def log(msg):
    unreal.log('[world-theatre-threat-material] ' + str(msg))


def fresh(name):
    path = DIR + '/' + name
    if eal.does_asset_exist(path) and not eal.delete_asset(path):
        raise RuntimeError('suppression impossible : ' + path)
    mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, DIR, unreal.Material, unreal.MaterialFactoryNew())
    if mat is None:
        raise RuntimeError('creation impossible : ' + path)
    return mat, path


def custom(mat, code, inputs, x, y, label, out_type=unreal.CustomMaterialOutputType.CMOT_FLOAT1):
    node = mel.create_material_expression(mat, unreal.MaterialExpressionCustom, x, y)
    node.set_editor_property('description', label)
    node.set_editor_property('output_type', out_type)
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
        raise RuntimeError('lien %s -> %s non pose' % (out, inp))


def scalar(mat, name, value, x, y):
    p = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, x, y)
    p.set_editor_property('parameter_name', name)
    p.set_editor_property('default_value', value)
    return p


def compile_and_save(mat, path, name):
    unreal.log('WORLD_THEATRE_MATERIAL_COMPILE_BEGIN ' + name)
    errors = list(mel.recompile_material(mat) or [])
    if errors:
        for e in errors:
            unreal.log_error('WORLD_THEATRE_MATERIAL_ERROR %s %s' % (name, e))
        raise RuntimeError('%s ne compile pas' % name)
    eal.save_asset(path)
    log('MATERIAL ' + path)


def build_smoke():
    mat, path = fresh('M_WorldTheatreSmoke')
    mat.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
    mat.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    mat.set_editor_property('two_sided', True)
    vc = mel.create_material_expression(mat, unreal.MaterialExpressionVertexColor, -1200, 0)
    uv = mel.create_material_expression(mat, unreal.MaterialExpressionTextureCoordinate, -1200, 120)
    tm = mel.create_material_expression(mat, unreal.MaterialExpressionTime, -1200, 220)
    nrm = mel.create_material_expression(mat, unreal.MaterialExpressionVertexNormalWS, -1200, 320)
    cam = mel.create_material_expression(mat, unreal.MaterialExpressionCameraVectorWS, -1200, 420)
    opac = scalar(mat, 'Opacity', 0.8, -1200, 520)
    seed = scalar(mat, 'Seed', 0.0, -1200, 600)
    tint = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -1200, 700)
    tint.set_editor_property('parameter_name', 'Tint')
    tint.set_editor_property('default_value', unreal.LinearColor(0.1, 0.09, 0.08, 1.0))
    op = custom(mat, SMOKE_OPACITY, ['UV', 'T', 'Seed', 'N', 'V', 'Opacity', 'VC'], -600, 200, 'WorldTheatreSmokeOpacity')
    for src, inp in ((uv, 'UV'), (tm, 'T'), (seed, 'Seed'), (nrm, 'N'), (cam, 'V'), (opac, 'Opacity')):
        link(src, '', op, inp)
    # La sortie par defaut d'une couleur de sommet est RGB (float3) : l'alpha (densite) sort par A.
    link(vc, 'A', op, 'VC')
    col = custom(mat, SMOKE_COLOR, ['UV', 'T', 'Seed', 'Tint'], -600, 500, 'WorldTheatreSmokeColor', unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    for src, inp in ((uv, 'UV'), (tm, 'T'), (seed, 'Seed'), (tint, 'Tint')):
        link(src, '', col, inp)
    glow_p = scalar(mat, 'Glow', 0.0, -1200, 800)
    glow = custom(mat, SMOKE_GLOW, ['UV', 'T', 'Seed', 'Glow', 'VC'], -600, 800, 'WorldTheatreSmokeGlow', unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    for src, inp in ((uv, 'UV'), (tm, 'T'), (seed, 'Seed'), (glow_p, 'Glow')):
        link(src, '', glow, inp)
    link(vc, 'A', glow, 'VC')
    if not mel.connect_material_property(glow, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR):
        raise RuntimeError('lueur non cablee')
    if not mel.connect_material_property(op, '', unreal.MaterialProperty.MP_OPACITY):
        raise RuntimeError('opacite non cablee')
    if not mel.connect_material_property(col, '', unreal.MaterialProperty.MP_BASE_COLOR):
        raise RuntimeError('couleur non cablee')
    k = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -300, 700)
    k.set_editor_property('r', 1.0)
    mel.connect_material_property(k, '', unreal.MaterialProperty.MP_ROUGHNESS)
    compile_and_save(mat, path, 'M_WorldTheatreSmoke')


def build_fire():
    mat, path = fresh('M_WorldTheatreFire')
    mat.set_editor_property('blend_mode', unreal.BlendMode.BLEND_ADDITIVE)
    mat.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
    mat.set_editor_property('two_sided', True)
    uv = mel.create_material_expression(mat, unreal.MaterialExpressionTextureCoordinate, -1100, 0)
    tm = mel.create_material_expression(mat, unreal.MaterialExpressionTime, -1100, 120)
    inten = scalar(mat, 'Intensity', 1.0, -1100, 220)
    gain = scalar(mat, 'Gain', 40.0, -1100, 300)
    seed = scalar(mat, 'Seed', 0.0, -1100, 380)
    em = custom(mat, FIRE_EMISSIVE, ['UV', 'T', 'Intensity', 'Gain', 'Seed'], -700, 100, 'WorldTheatreFire', unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    for src, inp in ((uv, 'UV'), (tm, 'T'), (inten, 'Intensity'), (gain, 'Gain'), (seed, 'Seed')):
        link(src, '', em, inp)
    out = em
    cls = getattr(unreal, 'MaterialExpressionEyeAdaptationInverse', None)
    if cls is not None:
        inv = mel.create_material_expression(mat, cls, -350, 100)
        try:
            link(em, '', inv, 'LightValueInput')
            out = inv
            log('fire: EyeAdaptationInverse')
        except RuntimeError:
            log('fire: EyeAdaptationInverse sans entree LightValueInput, gain seul')
    else:
        log('fire: EyeAdaptationInverse absent, gain seul')
    if not mel.connect_material_property(out, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR):
        raise RuntimeError('emissif non cable')
    compile_and_save(mat, path, 'M_WorldTheatreFire')


def main():
    if unreal.get_editor_subsystem(unreal.AssetEditorSubsystem) is None:
        raise RuntimeError('editeur vivant requis ; aucun asset modifie')
    build_smoke()
    build_fire()
    log('WORLD_THEATRE_THREAT_MATERIAL::PASS')


if __name__ == '__main__':
    try:
        main()
    except Exception as exc:  # noqa: BLE001
        import traceback
        unreal.log_error('WORLD_THEATRE_THREAT_MATERIAL::FAIL %s\n%s' % (exc, traceback.format_exc()))
    finally:
        if os.environ.get('ANASTASIS_THEATRE_MATERIAL_QUIT') == '1':
            unreal.SystemLibrary.quit_editor()
