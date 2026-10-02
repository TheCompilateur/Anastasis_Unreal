"""Materiau d'usure des maisons vides (mission abandon-001, ABANDON_001).

Cree /Game/Anastasis/VillageBuildings/M_VillageBuilding_Aged : le MEME grain que
M_VillageBuilding_Surface (couleur de sommet, bois / pierre / tuile), plus un parametre scalaire
`Neglect` (0 = intact, 1 = long abandon, jours de vacance de la simulation) qui ternit la couleur,
grise le bois, fait pâlir la tuile et pose de la mousse sur les faces tournees vers le ciel.
Aucun mesh ni autre materiau n'est touche : l'acteur maison ne l'applique qu'a une maison vide.

    ANASTASIS_AGING_REBUILD=1   supprime et regenere le materiau (recette identique ou nouvelle)

Lance par create-building-aging.ps1 (editeur dedie, se ferme).
"""
import os

PKG = '/Game/Anastasis/VillageBuildings'
NAME = 'M_VillageBuilding_Aged'
VERSION = 'abandon-001-v1'

CODE = """
float grain=sin(P.x*1.6+sin(P.z*0.9))*sin(P.y*1.9-P.z*1.3);
float wood=sin(P.x*.24+3*sin(P.z*2.7+sin(P.x*.047)))*sin(P.z*5.2);
float ashlar=sin(floor(P.x*.04)*1.7+floor(P.z*.06)*2.1);
float tile=sin(P.x*.55+P.y*2.4)*sin(P.z*3.1);
float isWood=step(.80,R)*(1-step(.88,R));
float isStone=step(.70,R)*(1-step(.80,R));
float isTile=step(.55,R)*(1-step(.70,R));
float n=lerp(grain, wood, isWood);
n=lerp(n, ashlar, isStone);
n=lerp(n, tile, isTile);
float3 col=C*(0.84+0.16*n);
float t=saturate(Neglect);
float lum=dot(col,float3(0.299,0.587,0.114));
col=lerp(col,float3(lum,lum,lum)*0.78,t*0.55);
col=lerp(col,float3(0.30,0.28,0.25)*(0.85+0.3*grain),t*isWood*0.7);
col=lerp(col,col*float3(0.88,0.9,0.85),t*isTile);
float patch=saturate(0.5+0.5*sin(P.x*.071+sin(P.y*.083))*sin(P.y*.057+P.z*.021)+0.35*grain);
float up=saturate(N.z*1.4-0.15);
float moss=saturate((t-0.25)/0.75)*up*saturate(patch*1.3);
col=lerp(col,float3(0.09,0.16,0.05)*(0.7+0.5*grain),moss*0.85);
return col;
"""


def create():
    import unreal as u
    mel = u.MaterialEditingLibrary
    eal = u.EditorAssetLibrary
    assets = u.AssetToolsHelpers.get_asset_tools()
    path = PKG + '/' + NAME
    if eal.does_asset_exist(path):
        if os.environ.get('ANASTASIS_AGING_REBUILD', '0') == '1':
            assert eal.get_metadata_tag(eal.load_asset(path), 'Recipe') in (VERSION, 'abandon-001-v1')
            assert eal.delete_asset(path), 'rebuild: delete failed'
        else:
            mat = eal.load_asset(path)
            assert eal.get_metadata_tag(mat, 'Recipe') == VERSION, 'existing asset belongs to another recipe'
            u.log('AGING ASSET exists recipe=' + VERSION)
            u.log('AGING COMPLETE')
            return
    mat = assets.create_asset(NAME, PKG, u.Material, u.MaterialFactoryNew())
    vc = mel.create_material_expression(mat, u.MaterialExpressionVertexColor, -700, 0)
    wp = mel.create_material_expression(mat, u.MaterialExpressionWorldPosition, -700, 200)
    nrm = mel.create_material_expression(mat, u.MaterialExpressionPixelNormalWS, -700, 380)
    neg = mel.create_material_expression(mat, u.MaterialExpressionScalarParameter, -700, 520)
    neg.set_editor_property('parameter_name', 'Neglect')
    neg.set_editor_property('default_value', 0.0)
    custom = mel.create_material_expression(mat, u.MaterialExpressionCustom, -300, 0)
    custom.set_editor_property('output_type', u.CustomMaterialOutputType.CMOT_FLOAT3)
    inputs = []
    for name in ['P', 'C', 'R', 'N', 'Neglect']:
        ci = u.CustomInput()
        ci.set_editor_property('input_name', name)
        inputs.append(ci)
    custom.set_editor_property('inputs', inputs)
    custom.set_editor_property('code', CODE)
    assert mel.connect_material_expressions(wp, '', custom, 'P')
    assert mel.connect_material_expressions(vc, '', custom, 'C')
    assert mel.connect_material_expressions(vc, 'A', custom, 'R')
    assert mel.connect_material_expressions(nrm, '', custom, 'N')
    assert mel.connect_material_expressions(neg, '', custom, 'Neglect')
    assert mel.connect_material_property(custom, '', u.MaterialProperty.MP_BASE_COLOR)
    rough = mel.create_material_expression(mat, u.MaterialExpressionCustom, -300, 300)
    rough.set_editor_property('output_type', u.CustomMaterialOutputType.CMOT_FLOAT1)
    rin = []
    for name in ['R', 'Neglect']:
        ci = u.CustomInput()
        ci.set_editor_property('input_name', name)
        rin.append(ci)
    rough.set_editor_property('inputs', rin)
    rough.set_editor_property('code', 'return lerp(R, 1.0, saturate(Neglect)*0.45);')
    assert mel.connect_material_expressions(vc, 'A', rough, 'R')
    assert mel.connect_material_expressions(neg, '', rough, 'Neglect')
    assert mel.connect_material_property(rough, '', u.MaterialProperty.MP_ROUGHNESS)
    metal = mel.create_material_expression(mat, u.MaterialExpressionCustom, -300, 560)
    metal.set_editor_property('output_type', u.CustomMaterialOutputType.CMOT_FLOAT1)
    ci = u.CustomInput()
    ci.set_editor_property('input_name', 'R')
    metal.set_editor_property('inputs', [ci])
    metal.set_editor_property('code', 'return 1-step(0.5,R);')
    assert mel.connect_material_expressions(vc, 'A', metal, 'R')
    assert mel.connect_material_property(metal, '', u.MaterialProperty.MP_METALLIC)
    errors = list(mel.recompile_material(mat))
    assert not errors, errors
    # Un « Failed to compile » n'est qu'un Warning : l'asset existe mais est casse. Relire avant de sauver.
    eal.set_metadata_tag(mat, 'Recipe', VERSION)
    assert eal.save_asset(path)
    u.log('AGING ASSET ' + path + ' recipe=' + VERSION)
    u.log('AGING COMPLETE')


def main():
    import unreal
    try:
        create()
    except BaseException:
        import traceback
        unreal.log_error(traceback.format_exc())
        raise
    finally:
        unreal.SystemLibrary.quit_editor()


if __name__ == '__main__':
    main()
