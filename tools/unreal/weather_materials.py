"""Shared visual weather contract. Imported by material authorities; no simulation writes."""
import unreal
MPC = '/Game/Anastasis/Materials/MPC_AnastasisWeather'
mel = unreal.MaterialEditingLibrary

def collection():
    asset = unreal.load_asset(MPC)
    if asset:
        return asset
    asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        'MPC_AnastasisWeather', '/Game/Anastasis/Materials',
        unreal.MaterialParameterCollection, unreal.MaterialParameterCollectionFactoryNew())
    vectors = []
    for name, value in [('WeatherWind', (0.894427, 0.447214, 0.3, 0)),
                        ('WeatherAir', (0.45, 0.0, 0.25, 0))]:
        p = unreal.CollectionVectorParameter()
        p.set_editor_property('parameter_name', name)
        p.set_editor_property('default_value', unreal.LinearColor(*value))
        vectors.append(p)
    enabled = unreal.CollectionScalarParameter()
    enabled.set_editor_property('parameter_name', 'WeatherCoupling')
    enabled.set_editor_property('default_value', 1.0)
    asset.set_editor_property('vector_parameters', vectors)
    asset.set_editor_property('scalar_parameters', [enabled])
    unreal.EditorAssetLibrary.save_loaded_asset(asset)
    return asset

def parameter(mat, name):
    n = mel.create_material_expression(mat, unreal.MaterialExpressionCollectionParameter)
    n.set_editor_property('collection', collection())
    n.set_editor_property('parameter_name', name)
    return n

def custom(mat, code, sources, output=unreal.CustomMaterialOutputType.CMOT_FLOAT3):
    n = mel.create_material_expression(mat, unreal.MaterialExpressionCustom)
    n.set_editor_property('description', 'Atmosphere weather coupling')
    n.set_editor_property('code', code)
    n.set_editor_property('output_type', output)
    inputs = []
    for name in sources:
        i = unreal.CustomInput()
        i.set_editor_property('input_name', name)
        inputs.append(i)
    n.set_editor_property('inputs', inputs)
    for name, (src, out) in sources.items():
        if not mel.connect_material_expressions(src, out, n, name):
            raise RuntimeError('Weather input disconnected: ' + name)
    return n

def node(mat, cls):
    return mel.create_material_expression(mat, cls)

def tree_wind(mat, old, mask):
    # World direction survives random instance yaw. Leaf mask keeps woody trunks rigid.
    n = custom(mat, """
float2 d = normalize(Wind.xy + float2(0.00001,0));
float front = dot(P.xy, d) * 0.0012 - T * 0.65;
float gust = 0.65 + 0.25*sin(front) + 0.10*sin(front*1.71 + 1.2);
float sway = 0.65 + 0.25*sin(T*0.8 + Random*6.283) + 0.10*sin(T*2.4 + Random*8);
float3 coherent = float3(d,0) * (4.0 * saturate(Wind.z) * gust * sway * Mask);
return lerp(Old, coherent, Enabled);
""", {'Old': (old, ''), 'Mask': (mask, 'A'),
      'P': (node(mat, unreal.MaterialExpressionWorldPosition), ''),
      'T': (node(mat, unreal.MaterialExpressionTime), ''),
      'Random': (node(mat, unreal.MaterialExpressionPerInstanceRandom), ''),
      'Wind': (parameter(mat, 'WeatherWind'), ''),
      'Enabled': (parameter(mat, 'WeatherCoupling'), '')})
    if not mel.connect_material_property(n, '', unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET):
        raise RuntimeError('Weather tree WPO disconnected')

def water_normal(mat, normal, flow, position, time):
    n = custom(mat, """
float2 d = normalize(Wind.xy + float2(0.00001,0));
float spd = saturate(length(Flow));
float wind = saturate(Wind.z);
// River flow retains its own gravity-driven disturbance, even in still air.
float strength = lerp(0.12 + 0.88*wind, 1.0, smoothstep(0.012,0.15,spd));
// Crosswind phase modulation breaks endless parallel rails without changing river flow.
float2 across = float2(-d.y,d.x);
float phase = dot(P.xy,d)*0.024 - T*2.1 + 0.35*sin(dot(P.xy,across)*0.006);
float2 gradient = -N.xy / max(N.z,0.1) * strength;
gradient += d * (0.035*wind) * (sin(phase) + 0.35*sin(phase*1.73 + 0.8));
return normalize(lerp(N.xyz, normalize(float3(-gradient,1)), Enabled));
""", {'N': (normal, ''), 'Flow': (flow, ''), 'P': (position, ''), 'T': (time, ''),
      'Wind': (parameter(mat, 'WeatherWind'), ''),
      'Enabled': (parameter(mat, 'WeatherCoupling'), '')})
    if not mel.connect_material_property(n, '', unreal.MaterialProperty.MP_NORMAL):
        raise RuntimeError('Weather water normal disconnected')

def wet_surface(mat):
    # Bounded dampening. Air humidity alone does not make a polished, soaked surface.
    for prop, code, output in [
        (unreal.MaterialProperty.MP_BASE_COLOR,
         'return Dry * (1.0 - 0.12*saturate(Air.y)*Enabled);', unreal.CustomMaterialOutputType.CMOT_FLOAT3),
        (unreal.MaterialProperty.MP_ROUGHNESS,
         'return lerp(Dry, max(0.48, Dry*0.86), saturate(Air.y)*Enabled);', unreal.CustomMaterialOutputType.CMOT_FLOAT1)]:
        src = mel.get_material_property_input_node(mat, prop)
        if not src:
            raise RuntimeError('Missing authored dry surface input')
        out_name = mel.get_material_property_input_node_output_name(mat, prop)
        n = custom(mat, code, {'Dry': (src,out_name), 'Air': (parameter(mat,'WeatherAir'),''),
                              'Enabled': (parameter(mat,'WeatherCoupling'),'')}, output)
        if not mel.connect_material_property(n,'',prop):
            raise RuntimeError('Wet surface disconnected')
