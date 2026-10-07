"""NOX_001 diagnostic: does any radiance remain when the moon is zero?

Reuses capture-sky.py without saving assets. Its fixed EV is temporarily overridden
after each Apply, only for the named diagnostic states. This is not a player profile.
Run through editor-batch.ps1 after adding a local proof entry, or from an editor.
"""
import os
import unreal

root = unreal.Paths.project_dir()
os.environ['ANASTASIS_SKY_OUT'] = os.path.join(unreal.Paths.project_saved_dir(), 'NoxEvidence', 'exposure-diagnostic')
os.environ['ANASTASIS_SKY_VIEWS'] = 'valley_long'
os.environ['ANASTASIS_SKY_STATES'] = '|'.join((
    'dark_default=anastasis.Sky.Day 1;anastasis.Sky.Hour 0;anastasis.Sky.Cover 0;anastasis.Nox.Profile 1;anastasis.Nox.MoonFraction 0',
    'dark_ev_m6=anastasis.Nox.MoonFraction 0',
    'dark_ev_m10=anastasis.Nox.MoonFraction 0',
    'full_restore=anastasis.Nox.Profile 0;anastasis.Nox.MoonFraction 1',
))
script = os.path.join(root, 'tools', 'unreal', 'capture-sky.py')
namespace = {'__name__': '__main__', '__file__': script}
with open(script, encoding='utf-8') as source:
    exec(compile(source.read(), script, 'exec'), namespace)

original_apply = namespace['apply_state']


def diagnostic_apply(state):
    original_apply(state)
    ev = {'dark_ev_m6': -6.0, 'dark_ev_m10': -10.0}.get(state[0])
    if ev is None:
        return
    volumes = unreal.GameplayStatics.get_all_actors_of_class(namespace['world'], unreal.PostProcessVolume)
    if not volumes:
        raise RuntimeError('NOX_DIAGNOSTIC missing exposure volume')
    for volume in volumes:
        settings = volume.get_editor_property('settings')
        settings.set_editor_property('override_auto_exposure_min_brightness', True)
        settings.set_editor_property('override_auto_exposure_max_brightness', True)
        settings.set_editor_property('auto_exposure_min_brightness', ev)
        settings.set_editor_property('auto_exposure_max_brightness', ev)
        volume.set_editor_property('settings', settings)
    unreal.SystemLibrary.execute_console_command(namespace['world'], 'r.EyeAdaptation.CachedLightingPreExposure %g' % ev)
    unreal.log('NOX_DIAGNOSTIC state=%s manual_ev=%g volumes=%d' % (state[0], ev, len(volumes)))


namespace['apply_state'] = diagnostic_apply
