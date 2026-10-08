"""Authority for the first ANASTASIS cosmic sky material and lavender moon texture.

The star river, dust, occasional meteor and rare veil are code-native in one sky
material. The moon albedo is the original image in ArtSource/Celestial. Nothing
here touches a map: AAnastasisWorldAtmosphere places the dome transiently.
"""
import os
import traceback
import unreal

PKG = '/Game/Anastasis/Celestial'
VERSION = int(os.environ.get('ANASTASIS_COSMIC_VERSION', '0'))
if VERSION not in (0, 1):
    raise RuntimeError('unsupported cosmic art version: ' + str(VERSION))
TEX = PKG + '/T_MoonLavender'
SKY_NAME = 'T_CosmicRiverV1' if VERSION == 1 else 'T_CosmicRiver'
SKY_TEX = PKG + '/' + SKY_NAME
MAT_NAME = 'M_AnastasisCosmicSkyV1' if VERSION == 1 else 'M_AnastasisCosmicSky'
MAT = PKG + '/' + MAT_NAME
SOURCE = os.path.join(unreal.Paths.project_dir(), 'ArtSource', 'Celestial', 'moon_lavender.png')
SKY_SOURCE = os.path.join(unreal.Paths.project_dir(), 'ArtSource', 'Celestial',
                          'cosmic_river_v1.png' if VERSION == 1 else 'cosmic_river.png')
REBUILD = os.environ.get('ANASTASIS_COSMIC_REBUILD') == '1'
mel = unreal.MaterialEditingLibrary

HLSL = r'''
float3 d = normalize(-CamVec);
float c = cos(SkyRotation), s = sin(SkyRotation);
float3 r = float3(c*d.x-s*d.y, s*d.x+c*d.y, d.z);
float plane = abs(dot(r, normalize(float3(0.47, -0.68, 0.56))));
float core = exp(-pow(plane / 0.075, 2.0));
float halo = exp(-pow(plane / 0.22, 2.0));
float az = atan2(r.y, r.x);
float grains = 0.54 + 0.18*sin(az*37.0+r.z*71.0) + 0.13*sin(az*83.0-r.z*141.0);
float lanes = smoothstep(0.52, 0.83,
    0.5 + 0.25*sin(az*13.0+r.z*31.0)*sin(az*29.0-r.z*53.0));
float river = (0.23*halo + 0.82*core) * saturate(grains) * (1.0-0.78*lanes);
float3 galaxy = river * float3(0.0011, 0.0008, 0.0021);

float2 suv = float2(atan2(r.y,r.x)/6.2831853+0.5, asin(clamp(r.z,-1.0,1.0))/3.14159265+0.5);
float3 panorama = Texture2DSample(SkyTex, SkyTexSampler, suv).rgb;
float seam = smoothstep(0.0,0.065,suv.x) * (1.0-smoothstep(0.935,1.0,suv.x));
float2 grid = suv * float2(500.0, 250.0);
float2 cell = floor(grid), f = frac(grid);
cell.x = fmod(cell.x, 500.0);
float h = frac(sin(dot(cell,float2(127.1,311.7)))*43758.5453);
float2 p = float2(frac(sin(dot(cell,float2(269.5,183.3)))*43758.5453),
                  frac(sin(dot(cell,float2(419.2,371.9)))*43758.5453));
float star = step(0.981, h) * (1.0-smoothstep(0.012,0.18,length(f-p)));
float2 bigGrid = suv * float2(130.0,65.0);
float2 bigCell = floor(bigGrid), bf = frac(bigGrid);
bigCell.x = fmod(bigCell.x, 130.0);
float bh = frac(sin(dot(bigCell,float2(91.7,257.3)))*43758.5453);
float2 bp = float2(frac(sin(dot(bigCell,float2(345.2,159.4)))*43758.5453),
                   frac(sin(dot(bigCell,float2(217.6,341.8)))*43758.5453));
float bright = step(0.976,bh) * (1.0-smoothstep(0.01,0.065,length(bf-bp)));
float3 stars = star * float3(0.009,0.012,0.024) + bright * float3(0.040,0.036,0.080);
stars *= 0.7 + 0.65*halo;

float3 moon = 0.0;
float3 md = normalize(MoonDir.xyz);
if (MoonStrength > 0.001 && dot(d,md) > 0.984)
{
    float3 axis = abs(md.z) < 0.98 ? float3(0,0,1) : float3(1,0,0);
    float3 right = normalize(cross(axis,md));
    float3 up = cross(md,right);
    float2 uv = 0.5 + float2(dot(d,right),dot(d,up)) / 0.17498;
    if (all(uv >= 0.0) && all(uv <= 1.0))
    {
        float4 tex = Texture2DSample(MoonTex, MoonTexSampler, uv);
        float rad = length(uv-0.5);
        moon = tex.rgb * tex.a * (1.0-smoothstep(0.47,0.50,rad)) * MoonStrength * 0.065;
    }
}

float3 meteor = 0.0;
if (MeteorStrength > 0.001)
{
    float3 a = normalize(MeteorStart.xyz), b = normalize(MeteorEnd.xyz);
    float3 head = normalize(lerp(a,b,saturate(MeteorPhase)));
    float3 along = normalize(b-a);
    float x = dot(d-head,along);
    float y = length((d-head)-x*along);
    float tail = exp(-pow(y/0.0018,2.0)) * smoothstep(-0.105,-0.012,x)
        * (1.0-smoothstep(-0.002,0.003,x));
    float fire = exp(-pow(length(d-head)/0.005,2.0));
    meteor = (tail*float3(0.12,0.10,0.22)+fire*float3(0.38,0.32,0.55))*MeteorStrength;
}

float wave = abs(dot(r,normalize(float3(-0.26,0.41,0.87)))
    - 0.14*sin(az*3.0 + SkyRotation*0.27));
float veil = exp(-pow(wave/0.035,2.0)) * (0.55+0.45*sin(az*19.0+r.z*45.0));
float3 strange = max(veil,0.0) * VeilStrength * float3(0.0018,0.0008,0.0045);

return (panorama*0.15*seam + galaxy + stars)*NightStrength + moon + meteor + strange;
'''
if VERSION == 1:
    shader_path = os.path.join(unreal.Paths.project_dir(), 'ArtSource', 'Celestial', 'cosmic_sky_v1.hlsl')
    with open(shader_path, encoding='utf-8') as shader_file:
        HLSL = shader_file.read()


def custom(mat, code, inputs):
    node = mel.create_material_expression(mat, unreal.MaterialExpressionCustom, -450, 0)
    node.set_editor_property('code', code)
    node.set_editor_property('description', 'Cosmic river, cratered moon and rare nights')
    node.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    names = []
    for name in inputs:
        ci = unreal.CustomInput()
        ci.set_editor_property('input_name', name)
        names.append(ci)
    node.set_editor_property('inputs', names)
    return node


def scalar(mat, name, default, y):
    node = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -1000, y)
    node.set_editor_property('parameter_name', name)
    node.set_editor_property('default_value', default)
    return node


def vector(mat, name, rgba, y):
    node = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -1000, y)
    node.set_editor_property('parameter_name', name)
    node.set_editor_property('default_value', unreal.LinearColor(*rgba))
    return node


def create_texture(path, asset_name):
    dest = PKG + '/' + asset_name
    if not os.path.isfile(path):
        raise RuntimeError('missing art source: ' + path)
    if unreal.EditorAssetLibrary.does_asset_exist(dest) and (not REBUILD or (VERSION == 1 and asset_name == 'T_MoonLavender')):
        unreal.log('COSMIC_TEXTURE_PRESENT ' + dest)
        return unreal.load_asset(dest)
    task = unreal.AssetImportTask()
    task.set_editor_property('filename', path)
    task.set_editor_property('destination_path', PKG)
    task.set_editor_property('destination_name', asset_name)
    task.set_editor_property('replace_existing', True)
    task.set_editor_property('automated', True)
    task.set_editor_property('save', False)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    tex = unreal.load_asset(dest)
    if tex is None:
        raise RuntimeError('moon import failed')
    tex.set_editor_property('srgb', True)
    tex.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_BC7)
    tex.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_SKYBOX)
    unreal.EditorAssetLibrary.save_asset(dest)
    unreal.log('COSMIC_TEXTURE_SAVED ' + dest)
    return tex


def create_material(moon, panorama):
    if unreal.EditorAssetLibrary.does_asset_exist(MAT):
        if not REBUILD:
            unreal.log('COSMIC_MATERIAL_PRESENT ' + MAT)
            return
        mat = unreal.load_asset(MAT)
        mel.delete_all_material_expressions(mat)
    else:
        mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            MAT_NAME, PKG, unreal.Material, unreal.MaterialFactoryNew())
    if mat is None:
        raise RuntimeError('sky material creation failed')
    mat.set_editor_property('blend_mode', unreal.BlendMode.BLEND_OPAQUE)
    mat.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
    mat.set_editor_property('two_sided', True)
    mat.set_editor_property('is_sky', True)
    atmosphere = mel.create_material_expression(mat, unreal.MaterialExpressionSkyAtmosphereViewLuminance, -650, -240)
    sun_disk = mel.create_material_expression(mat, unreal.MaterialExpressionSkyAtmosphereLightDiskLuminance, -650, -140)
    sun_disk.set_editor_property('light_index', 0)
    cam = mel.create_material_expression(mat, unreal.MaterialExpressionCameraVectorWS, -1000, -110)
    moon_tex = mel.create_material_expression(mat, unreal.MaterialExpressionTextureObjectParameter, -1000, 0)
    moon_tex.set_editor_property('parameter_name', 'MoonTex')
    moon_tex.set_editor_property('texture', moon)
    sky_tex = mel.create_material_expression(mat, unreal.MaterialExpressionTextureObjectParameter, -1000, -10)
    sky_tex.set_editor_property('parameter_name', 'SkyTex')
    sky_tex.set_editor_property('texture', panorama)
    nodes = {
        'CamVec': cam,
        'MoonTex': moon_tex,
        'SkyTex': sky_tex,
        'MoonDir': vector(mat, 'MoonDir', (0,0,1,0), 100),
        'NightStrength': scalar(mat, 'NightStrength', 0, 200),
        'MoonStrength': scalar(mat, 'MoonStrength', 0, 290),
        'SkyRotation': scalar(mat, 'SkyRotation', 0, 380),
        'MeteorStrength': scalar(mat, 'MeteorStrength', 0, 470),
        'MeteorPhase': scalar(mat, 'MeteorPhase', 0, 560),
        'MeteorStart': vector(mat, 'MeteorStart', (0,0,1,0), 650),
        'MeteorEnd': vector(mat, 'MeteorEnd', (0,0,1,0), 740),
        'VeilStrength': scalar(mat, 'VeilStrength', 0, 830),
    }
    sky = custom(mat, HLSL, tuple(nodes.keys()))
    wiring = {}
    for name, node in nodes.items():
        wiring[name] = mel.connect_material_expressions(node, '', sky, name)
    day_sky = mel.create_material_expression(mat, unreal.MaterialExpressionAdd, -300, -170)
    wiring['atmosphere'] = mel.connect_material_expressions(atmosphere, '', day_sky, 'A')
    wiring['sun_disk'] = mel.connect_material_expressions(sun_disk, '', day_sky, 'B')
    add = mel.create_material_expression(mat, unreal.MaterialExpressionAdd, -100, 0)
    wiring['day_sky'] = mel.connect_material_expressions(day_sky, '', add, 'A')
    wiring['celestial'] = mel.connect_material_expressions(sky, '', add, 'B')
    wiring['emissive'] = mel.connect_material_property(add, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    if not all(wiring.values()):
        raise RuntimeError('material graph incomplete: ' + repr(wiring))
    unreal.log('COSMIC_MATERIAL_COMPILE ' + MAT)
    errors = list(mel.recompile_material(mat) or [])
    if errors:
        for error in errors:
            unreal.log_error('COSMIC_MATERIAL_COMPILE_ERROR ' + str(error))
        raise RuntimeError('sky material shader failed with %d error(s)' % len(errors))
    unreal.EditorAssetLibrary.save_asset(MAT)
    unreal.log('COSMIC_MATERIAL_SAVED ' + MAT)


try:
    create_material(create_texture(SOURCE, 'T_MoonLavender'),
                    create_texture(SKY_SOURCE, SKY_NAME))
    unreal.log('COSMIC_MATERIAL_DONE')
except Exception:
    unreal.log_error('COSMIC_MATERIAL_FAIL ' + traceback.format_exc())
