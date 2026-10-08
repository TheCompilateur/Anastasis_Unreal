"""NOX_003: read back an emissive-only HDR sky cube at fixed sky states.

The UE API performs a separate capture; this is not the live real-time Sky Light
cubemap, and its linear values are not measured lux. capture-sky.py supplies the
same level, seed and ridge eye for all states and writes sky.json + SDR witnesses.
"""
import json
import os
import unreal

root = unreal.Paths.project_dir()
out = os.path.join(unreal.Paths.project_saved_dir(), 'NoxEvidence', 'cubemap-probe')
os.environ['ANASTASIS_SKY_OUT'] = out
os.environ['ANASTASIS_SKY_VIEWS'] = 'ridge_long'
os.environ['ANASTASIS_SKY_STATES'] = '|'.join((
    'dark=anastasis.Sky.Day 1;anastasis.Sky.Hour 0;anastasis.Sky.Cover 0;anastasis.Sky.Rain 0;anastasis.Nox.Profile 1;anastasis.Nox.MoonFraction 0',
    'full=anastasis.Nox.MoonFraction 1',
    'day=anastasis.Sky.Hour 12;anastasis.Nox.Profile 0',
    'dark_repeat=anastasis.Sky.Hour 0;anastasis.Nox.Profile 1;anastasis.Nox.MoonFraction 0',
))
script = os.path.join(root, 'tools', 'unreal', 'capture-sky.py')
scope = {'__name__': '__main__', '__file__': script}
with open(script, encoding='utf-8') as source:
    exec(compile(source.read(), script, 'exec'), scope)

world = scope['world']
original_cmd = scope['cmd']
scope['report']['emissive_sky_cube'] = {}


def diagnostic_cmd(command):
    if command.startswith('HighResShot '):
        state = scope['STATES'][scope['state_i']][0]
        skies = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.SkyLight)
        if len(skies) != 1:
            raise RuntimeError('NOX_CUBEMAP_FAIL expected one Sky Light, found %d' % len(skies))
        component = skies[0].get_component_by_class(unreal.SkyLightComponent)
        result = json.loads(unreal.AnastasisSkyRadianceProbe.capture_emissive_sky_radiance(component))
        if result.get('error'):
            raise RuntimeError('NOX_CUBEMAP_FAIL %s: %s' % (state, result['error']))
        scope['report']['emissive_sky_cube'][state] = result
        unreal.log('NOX_CUBEMAP state=%s mean_y=%.9g p95_y=%.9g nonzero=%d/%d' %
                   (state, result['mean_y'], result['p95_y'],
                    result['nonzero_count'], result['pixel_count']))
    original_cmd(command)


scope['cmd'] = diagnostic_cmd
