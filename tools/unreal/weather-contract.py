"""Editor material contract; numerical evidence, not player or visual proof."""
import math, os, unreal
actor = None
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
original_actors = set(eas.get_all_level_actors())
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
def command(text):
    unreal.SystemLibrary.execute_console_command(world, text)
try:
    cloud = unreal.load_asset('/Engine/EngineSky/VolumetricClouds/m_SimpleVolumetricCloud_Inst')
    base = cloud.get_editor_property('parent')
    while base.get_class().get_name() != 'Material':
        base = base.get_editor_property('parent')
    for n in unreal.MaterialEditingLibrary.get_material_expressions(base):
        if n.get_class().get_name() == 'MaterialExpressionVectorParameter' and str(n.get_editor_property('parameter_name')) == 'Layout_WindControls':
            unreal.log('WEATHER_CLOUD_CHANNELS %s desc=%s' % (n.get_editor_property('channel_names'), n.get_editor_property('desc')))
    collection = unreal.load_asset('/Game/Anastasis/Materials/MPC_AnastasisWeather')
    assert collection, 'missing collection'
    for name in ['M_AnastasisVegetation','M_AnastasisGrass','M_AnastasisWater','M_AnastasisBark','M_AnastasisRock']:
        mat = unreal.load_asset('/Game/Anastasis/Materials/' + name)
        assert mat, name
        refs = [n for n in unreal.MaterialEditingLibrary.get_material_expressions(mat)
                if n.get_class().get_name() == 'MaterialExpressionCollectionParameter'
                and n.get_editor_property('collection') == collection]
        assert refs, 'collection unconsumed: ' + name
    cls = unreal.load_class(None, '/Script/Anastasis_UnrealV2.AnastasisWorldAtmosphere')
    actor = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).spawn_actor_from_class(cls, unreal.Vector(), transient=True)
    command('anastasis.Sky.Clock 1')
    command('anastasis.Sky.Weather 1')
    command('anastasis.Sky.Hour 14')
    command('anastasis.Sky.WindHeading 90')
    for wind, humidity, cover, enabled in [(0,0.45,0.25,1),(1,0.9,0.9,1),(0.3,0.45,0.25,0)]:
        for name,value in [('Sky.Wind',wind),('Sky.Humidity',humidity),('Sky.Cover',cover),('Atmosphere.Coupling',enabled)]:
            command('anastasis.%s %s' % (name,value))
        actor.call_method('Apply')
        w = unreal.MaterialLibrary.get_vector_parameter_value(world,collection,'WeatherWind')
        a = unreal.MaterialLibrary.get_vector_parameter_value(world,collection,'WeatherAir')
        e = unreal.MaterialLibrary.get_scalar_parameter_value(world,collection,'WeatherCoupling')
        assert abs(w.r)<1e-4 and abs(w.g-1)<1e-4 and abs(w.b-wind)<1e-4, str(w)
        assert abs(a.r-humidity)<1e-4 and abs(a.b-cover)<1e-4, str(a)
        assert abs(e-enabled)<1e-4, str(e)
        clouds = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.VolumetricCloud)
        assert clouds, 'cloud absent'
        mid = clouds[0].get_component_by_class(unreal.VolumetricCloudComponent).get_editor_property('material')
        cw = mid.get_vector_parameter_value('Layout_WindControls')
        if enabled:
            assert abs(cw.r)<1e-4 and abs(cw.g-1)<1e-4 and abs(cw.b)<1e-4 and abs(cw.a-wind)<1e-4, str(cw)
        else:
            assert abs(cw.r-1)<1e-4 and abs(cw.g-1)<1e-4 and abs(cw.a-1/3)<1e-4, str(cw)
        unreal.log('WEATHER_CONTRACT cloud_wind=%s' % cw)
        unreal.log('WEATHER_CONTRACT sample wind=%s air=%s enabled=%s' % (w,a,e))
    unreal.log('WEATHER_CONTRACT PASS')
except Exception:
    import traceback
    unreal.log_error('WEATHER_CONTRACT FAIL ' + traceback.format_exc())
finally:
    for c in ['anastasis.Sky.Hour -1','anastasis.Sky.Wind -1','anastasis.Sky.Humidity -1','anastasis.Sky.Cover -1',
              'anastasis.Sky.WindHeading -1','anastasis.Atmosphere.Coupling 1']:
        command(c)
    # Only transient actors created by this synchronous contract, never original level actors.
    for spawned in list(eas.get_all_level_actors()):
        if spawned not in original_actors:
            eas.destroy_actor(spawned)
    if os.environ.get('ANASTASIS_WEATHER_CONTINUE') != '1':
        unreal.SystemLibrary.quit_editor()
