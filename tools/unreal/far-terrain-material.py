"""CONTINENTAL_001 -- M_AnastasisFarTerrain : le materiau des montagnes lointaines.

PROPRIETE DE L'ASSET. Ce script est la SOURCE D'AUTORITE de /Game/Anastasis/Materials/M_AnastasisFarTerrain.
Il le cree, ou reecrit son graphe sans supprimer l'asset (rien ne le reference par chemin dur).

POURQUOI UN MATERIAU A PART. M_AnastasisGround applique ses propres couleurs de FAMILLE (herbe, litiere,
travaille, roche) et n'utilise la couleur de sommet que comme teinte de la prairie : les montagnes de
l'anneau d'horizon sortaient beiges, sans foret sombre, sans roche grise, sans neige, quelles que soient
les teintes calculees par AnastasisTectonics::SurfaceAt (diagnostic : capture sans brouillard,
Saved/HorizonEvidence/continental-nofog). A 8 km et plus, ni le grain du sol (fondu a 40 m) ni les
textures photo ne servent : la couleur de sommet doit etre l'albedo, rien d'autre.

GRAPHE. BaseColor = couleur de sommet x une variation de valeur macro (deux sinus croises a 5 km et
1 km de longueur d'onde, +-10 %). Un parametre MountainRockDetail 0/1 commande un traitement
experimental des roches par strates et fissures en coordonnees monde ; 0 reproduit exactement le
graphe precedent. Rugosite 0,88, speculaire 0,25. Default Lit, opaque, une face, aucun vent.
Le traitement doit encore etre regenere dans l'asset et juge en A/B/A avant toute revendication SCN.

Lancer par tools/unreal/far-terrain-material.ps1 (editeur dedie, discret, qui se ferme).
"""
import os
import unreal

MATERIAL_DIR = '/Game/Anastasis/Materials'
NAME = 'M_AnastasisFarTerrain'
PATH = MATERIAL_DIR + '/' + NAME
mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary


def log(msg):
    unreal.log('[far-terrain-material] ' + str(msg))


CODE = '''
// Detail=0 reproduces the original far material.
float n = sin(Pos.x * 0.00019 + sin(Pos.y * 0.00015) * 1.7) * sin(Pos.y * 0.00021 + sin(Pos.x * 0.00012) * 1.3);
float m = sin(Pos.x * 0.00095 + Pos.y * 0.00061) * sin(Pos.y * 0.00112 - Pos.x * 0.00047);
float oldValue = 1.0 + 0.10 * n + 0.06 * m;

// UV0.x is the rock weight already authored by the terrain generator.
// World-space wavelengths stay legible at 5-15 km without a UV seam.
float rock = saturate(TerrainUV.x);
float3 q = Pos * float3(0.00010, 0.00010, 0.00012);
float warp = sin(q.x * 0.83 + sin(q.y * 0.62) * 1.6) * sin(q.y * 1.13 - q.z * 0.47);
float bedding = sin(q.z * 2.7 + warp * 2.1);
float cleft = pow(saturate(1.0 - abs(sin(q.x * 1.7 + q.y * 0.94 + warp * 1.4))), 8.0);
float rockValue = 0.84 + 0.16 * warp + 0.065 * bedding - 0.12 * cleft;
float forestValue = 0.96 + 0.10 * warp;
float value = lerp(1.0, lerp(forestValue, rockValue, rock), saturate(Detail));
return saturate(Colour * oldValue * value);
'''


def main():
    if unreal.get_editor_subsystem(unreal.AssetEditorSubsystem) is None:
        raise RuntimeError('editeur vivant requis ; aucun asset modifie')
    # UE 5.8.2 laisse deux expressions apres delete_all_material_expressions.
    # Ce materiau est charge par chemin au runtime : regenerer l'asset entier.
    if eal.does_asset_exist(PATH) and not eal.delete_asset(PATH):
        raise RuntimeError('suppression impossible : ' + PATH)
    mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        NAME, MATERIAL_DIR, unreal.Material, unreal.MaterialFactoryNew())
    if mat is None:
        raise RuntimeError('creation impossible : ' + PATH)
    vc = mel.create_material_expression(mat, unreal.MaterialExpressionVertexColor, -900, 0)
    pos = mel.create_material_expression(mat, unreal.MaterialExpressionWorldPosition, -900, 200)
    uv = mel.create_material_expression(mat, unreal.MaterialExpressionTextureCoordinate, -900, 360)
    detail = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -900, 520)
    detail.set_editor_property('parameter_name', 'MountainRockDetail')
    detail.set_editor_property('default_value', 0.0)  # detail rocheux rejete sur S045
    node = mel.create_material_expression(mat, unreal.MaterialExpressionCustom, -500, 60)
    node.set_editor_property('description', 'FarTerrainValue')
    node.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    node.set_editor_property('code', CODE)
    inputs = []
    for name in ('Colour', 'Pos', 'TerrainUV', 'Detail'):
        entry = unreal.CustomInput()
        entry.set_editor_property('input_name', name)
        inputs.append(entry)
    node.set_editor_property('inputs', inputs)
    if not mel.connect_material_expressions(vc, '', node, 'Colour'):
        raise RuntimeError('Colour non connecte')
    if not mel.connect_material_expressions(pos, '', node, 'Pos'):
        raise RuntimeError('Pos non connecte')
    if not mel.connect_material_expressions(uv, '', node, 'TerrainUV'):
        raise RuntimeError('TerrainUV non connecte')
    if not mel.connect_material_expressions(detail, '', node, 'Detail'):
        raise RuntimeError('Detail non connecte')
    if not mel.connect_material_property(node, '', unreal.MaterialProperty.MP_BASE_COLOR):
        raise RuntimeError('BaseColor non cable')
    for value, prop, y in ((0.88, unreal.MaterialProperty.MP_ROUGHNESS, 300), (0.25, unreal.MaterialProperty.MP_SPECULAR, 380)):
        k = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -300, y)
        k.set_editor_property('r', value)
        if not mel.connect_material_property(k, '', prop):
            raise RuntimeError('%s non cable' % prop)
    mat.set_editor_property('two_sided', False)
    errors = list(mel.recompile_material(mat) or [])
    if errors:
        for e in errors:
            unreal.log_error('FAR_TERRAIN_COMPILE ' + str(e))
        raise RuntimeError('%s ne compile pas' % NAME)
    eal.save_asset(PATH)
    log('MATERIAL %s' % PATH)
    log('FAR_TERRAIN_MATERIAL::PASS')


if __name__ == '__main__':
    try:
        main()
    except Exception as exc:  # noqa: BLE001
        import traceback
        unreal.log_error('FAR_TERRAIN_MATERIAL::FAIL %s\n%s' % (exc, traceback.format_exc()))
    finally:
        if os.environ.get('ANASTASIS_FAR_TERRAIN_QUIT') == '1':
            unreal.SystemLibrary.quit_editor()
