"""Build shared weather collection and existing material graphs only. Run via weather-materials.ps1."""
import os, runpy, sys, unreal
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import weather_materials
try:
    weather_materials.collection()
    tree = runpy.run_path(os.path.join(HERE, 'create_tree_asset.py'), run_name='weather_authority')
    tree['ensure_material']()
    tree['ensure_bark_material']()
    tree['ensure_rock_material']()
    grass = runpy.run_path(os.path.join(HERE, 'create-ground-cover.py'), run_name='weather_authority')
    grass['ensure_material']()
    water = runpy.run_path(os.path.join(HERE, 'water-look.py'), run_name='weather_authority')
    asset = unreal.load_asset(water['PATH'])
    if asset:
        unreal.MaterialEditingLibrary.delete_all_material_expressions(asset)
    water['build'](asset) if asset else water['build']()
    cloud = unreal.load_asset('/Engine/EngineSky/VolumetricClouds/m_SimpleVolumetricCloud_Inst')
    for name in unreal.MaterialEditingLibrary.get_vector_parameter_names(cloud):
        unreal.log('WEATHER_CLOUD_VECTOR %s=%s' % (name, unreal.MaterialEditingLibrary.get_material_instance_vector_parameter_value(cloud,name)))
    for name in unreal.MaterialEditingLibrary.get_scalar_parameter_names(cloud):
        unreal.log('WEATHER_CLOUD_SCALAR %s=%s' % (name, unreal.MaterialEditingLibrary.get_material_instance_scalar_parameter_value(cloud,name)))
    unreal.log('WEATHER_MATERIALS PASS')
except Exception:
    import traceback
    unreal.log_error('WEATHER_MATERIALS FAIL ' + traceback.format_exc())
finally:
    unreal.SystemLibrary.quit_editor()
