"""NOX_002: isolate the captured Sky Light contribution on a moonless night.

Run via editor-batch.ps1 -Proofs nox-skylight-probe. No asset is saved. The
1000x state is an instrument, never an authored lighting profile. The HDR
SceneColor samples are linear render-target values, not measured lux and not a
direct readback of the Sky Light's internal cubemap.
"""
import os
import unreal


root = unreal.Paths.project_dir()
out = os.path.join(unreal.Paths.project_saved_dir(), 'NoxEvidence', 'skylight-probe')
os.makedirs(out, exist_ok=True)
os.environ['ANASTASIS_SKY_OUT'] = out
os.environ['ANASTASIS_SKY_VIEWS'] = 'valley_long,ridge_long'
os.environ['ANASTASIS_SKY_STATES'] = '|'.join((
    'dark_native=anastasis.Sky.Day 1;anastasis.Sky.Hour 0;anastasis.Sky.Cover 0;anastasis.Sky.Rain 0;anastasis.Nox.Profile 1;anastasis.Nox.MoonFraction 0',
    'dark_sky_off=anastasis.Nox.MoonFraction 0',
    'dark_sky_x1000=anastasis.Nox.MoonFraction 0',
    'dark_native_repeat=anastasis.Nox.MoonFraction 0',
    'full_moon=anastasis.Nox.MoonFraction 1',
))

script = os.path.join(root, 'tools', 'unreal', 'capture-sky.py')
scope = {'__name__': '__main__', '__file__': script}
with open(script, encoding='utf-8') as source:
    exec(compile(source.read(), script, 'exec'), scope)

world = scope['world']
ues = scope['ues']
original_apply = scope['apply_state']
original_cmd = scope['cmd']
scope['report']['hdr_probe'] = {}


def set_exposure(ev):
    volumes = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.PostProcessVolume)
    if len(volumes) != 1:
        raise RuntimeError('NOX_SKYLIGHT expected one exposure volume, found %d' % len(volumes))
    volume = volumes[0]
    settings = volume.get_editor_property('settings')
    settings.set_editor_property('override_auto_exposure_min_brightness', True)
    settings.set_editor_property('override_auto_exposure_max_brightness', True)
    settings.set_editor_property('auto_exposure_min_brightness', ev)
    settings.set_editor_property('auto_exposure_max_brightness', ev)
    volume.set_editor_property('settings', settings)
    original_cmd('r.EyeAdaptation.CachedLightingPreExposure %g' % ev)


def diagnostic_apply(state):
    original_apply(state)
    label = state[0]
    skies = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.SkyLight)
    if len(skies) != 1:
        raise RuntimeError('NOX_SKYLIGHT expected one Sky Light, found %d' % len(skies))
    component = skies[0].get_component_by_class(unreal.SkyLightComponent)
    if component is None:
        raise RuntimeError('NOX_SKYLIGHT Sky Light has no component')
    native = float(component.get_editor_property('intensity'))
    factor = 0.0 if label == 'dark_sky_off' else (1000.0 if label == 'dark_sky_x1000' else 1.0)
    component.set_intensity(native * factor)
    if label != 'full_moon':
        set_exposure(-10.0)
    scope['report']['states'][label].update({
        'sky_intensity_native': native, 'sky_intensity_factor': factor,
        'ev_for_moonless': -10.0 if label != 'full_moon' else None,
    })
    unreal.log('NOX_SKYLIGHT state=%s intensity=%.6g factor=%.1f' % (label, native * factor, factor))


scope['apply_state'] = diagnostic_apply


def hdr_samples(name):
    fmt = getattr(unreal.TextureRenderTargetFormat, 'RTF_RGBA16F', None)
    if fmt is None:
        fmt = getattr(unreal.TextureRenderTargetFormat, 'RTF_RGBA16f')
    rt = unreal.RenderingLibrary.create_render_target2d(
        world, 256, 144, fmt, unreal.LinearColor(0, 0, 0, 1))
    actor = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).spawn_actor_from_class(
        unreal.SceneCapture2D, unreal.Vector(0, 0, 0), unreal.Rotator())
    if actor is None:
        raise RuntimeError('NOX_SKYLIGHT cannot spawn HDR scene capture')
    try:
        location, rotation = ues.get_level_viewport_camera_info()
        actor.set_actor_location(location, False, False)
        actor.set_actor_rotation(rotation, False)
        comp = actor.capture_component2d
        comp.set_editor_property('capture_source', unreal.SceneCaptureSource.SCS_SCENE_COLOR_HDR)
        comp.set_editor_property('fov_angle', 90.0)
        comp.set_editor_property('texture_target', rt)
        comp.set_editor_property('capture_every_frame', False)
        comp.set_editor_property('capture_on_movement', False)
        comp.capture_scene()
        samples = {}
        for y in (12, 36, 72, 108, 132):
            for x in (32, 128, 224):
                c = unreal.RenderingLibrary.read_render_target_raw_pixel(world, rt, x, y, False)
                rgb = [float(c.r), float(c.g), float(c.b)]
                samples['%d,%d' % (x, y)] = {
                    'rgb': rgb,
                    'linear_y': 0.2126 * rgb[0] + 0.7152 * rgb[1] + 0.0722 * rgb[2],
                }
        unreal.RenderingLibrary.export_render_target(world, rt, out, name)
        scope['report']['hdr_probe'][name] = {
            'capture_source': 'SCS_SceneColorHDR', 'render_target': 'RTF_RGBA16f',
            'size': [256, 144], 'fov': 90.0, 'raw_normalize': False,
            'samples': samples,
        }
        unreal.log('NOX_HDR_PROBE %s mid=%.9g' % (name, samples['128,72']['linear_y']))
    finally:
        actor.destroy_actor()


def diagnostic_cmd(command):
    if command.startswith('HighResShot '):
        state = scope['STATES'][scope['state_i']][0]
        view = scope['queue'][0][1][0]
        hdr_samples('%s_%s' % (view, state))
    original_cmd(command)


scope['cmd'] = diagnostic_cmd
