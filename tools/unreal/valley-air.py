"""Transient layer diagnosis at 14h; no asset saves. Uses capture-sky camera/GPU harness."""
import os
os.environ['ANASTASIS_SKY_VIEWS'] = 'riviere_eye,ridge_long'
common = 'anastasis.Sky.Day 1;anastasis.Sky.Hour 14;anastasis.Sky.Humidity .45;anastasis.Sky.Wind .3;anastasis.Sky.Cover .25;anastasis.Sky.Rain 0;anastasis.Atmosphere.Mist 1'
candidate_mode = os.environ.get('ANASTASIS_VALLEY_MODE') == 'candidate'
names = ('warmup','reference','candidate','reference2','candidate2') if candidate_mode else ('warmup','reference','no_global','no_aerial','no_local','reference2')
os.environ['ANASTASIS_SKY_STATES'] = '|'.join(n+'='+common for n in names)
p = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'capture-sky.py')
exec(compile(open(p, encoding='utf-8').read(), p, 'exec'), globals())
views.append(('riviere_eye', V(98484.375,109825.0,1017.5780487060547), V(100473.95833333333,116191.66666666666,432.9681396484375)))
base_apply_state = apply_state

def apply_state(state):
    base_apply_state(state)
    name = state[0]
    fogs = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.ExponentialHeightFog)
    skies = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.SkyAtmosphere)
    assert len(fogs) == 1 and len(skies) == 1, 'ambiguous atmospheric actors'
    fog = fogs[0].get_component_by_class(unreal.ExponentialHeightFogComponent)
    sky = skies[0].get_component_by_class(unreal.SkyAtmosphereComponent)
    fog.set_visibility(name != 'no_global')
    if candidate_mode:
        # Explicit A/B values keep this diagnostic reproducible after a kept default change.
        fog.set_fog_max_opacity(0.30 if name in ('candidate','candidate2') else 0.48)
    if name == 'no_aerial':
        sky.set_aerial_pespective_view_distance_scale(0.0)
    if name == 'no_local':
        cmd('anastasis.Atmosphere.Mist 0')
        atm.call_method('ApplyMist')
    report['states'][name]['observed'] = {
        'fog_visible': fog.is_visible(),
        'fog_density': fog.get_editor_property('fog_density'),
        'fog_max_opacity': fog.get_editor_property('fog_max_opacity'),
        'aerial_scale': sky.get_editor_property('aerial_pespective_view_distance_scale'),
        'local_count': len(unreal.GameplayStatics.get_all_actors_of_class(world, unreal.LocalFogVolume))}
    unreal.log('VALLEY_AIR_STATE '+name+' '+str(report['states'][name]['observed']))

# Fail promptly if an editor API or layer readback fails, instead of leaving the callback idle.
unreal.unregister_slate_post_tick_callback(handle)
base_tick = tick
def guarded_tick(dt):
    try:
        base_tick(dt)
    except Exception:
        import traceback
        finish('VALLEY_AIR_FAIL '+traceback.format_exc(), True)
handle = unreal.register_slate_post_tick_callback(guarded_tick)
