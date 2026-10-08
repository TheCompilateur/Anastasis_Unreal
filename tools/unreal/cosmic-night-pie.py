"""PIE component proof for COSMIC_NIGHT_001; visual judgment remains separate.

Run through editor-batch.ps1 -Proofs cosmic-night-pie. No Content or map saved.
"""
import json
import os
import time
import traceback
import unreal

LEVEL = '/Game/Anastasis/Maps/Lvl_AnastasisSlice'
OUT = os.path.join(unreal.Paths.project_saved_dir(), 'CosmicSkyEvidence')
os.makedirs(OUT, exist_ok=True)
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
unreal.log('COSMIC_NIGHT_PIE_MAP_LOAD=' + str(les.load_level(LEVEL)))
CVARS = {
    'anastasis.Sim.TimeScale': 0,
    'anastasis.Sky.Clock': 1,
    'anastasis.Sky.Cosmic': 1,
    'anastasis.Sky.CosmicArtVersion': 1,
    'anastasis.Sky.CosmicEvent': 2,
    'anastasis.Sky.Day': 1,
    'anastasis.Sky.Hour': 23,
    'anastasis.Sky.Cover': 0,
    'anastasis.Sky.Rain': 0,
}
before = {name: unreal.SystemLibrary.get_console_variable_float_value(name) for name in CVARS}
samples = {}
phase = 'boot'
mark = start = time.monotonic()
handle = None
atm = None


def cmd(name, value):
    unreal.SystemLibrary.execute_console_command(None, '%s %g' % (name, value))


def sample(atm):
    dome = next((component for component in atm.get_components_by_class(unreal.StaticMeshComponent)
                 if component.get_name() == 'CosmicDome'), None)
    material = dome.get_material(0) if dome else None
    if not material or not isinstance(material, unreal.MaterialInstanceDynamic):
        raise RuntimeError('CosmicDome dynamic material absent')
    parent = material.get_editor_property('parent')
    return {
        'visible': bool(dome.is_visible()),
        'material': material.get_path_name(),
        'parent': parent.get_path_name() if parent else '',
        'night': float(material.get_scalar_parameter_value('NightStrength')),
        'moon': float(material.get_scalar_parameter_value('MoonStrength')),
        'veil': float(material.get_scalar_parameter_value('VeilStrength')),
        'meteor': float(material.get_scalar_parameter_value('MeteorStrength')),
        'art_cvar': float(unreal.SystemLibrary.get_console_variable_float_value('anastasis.Sky.CosmicArtVersion')),
    }


def finish(ok, message):
    for name, value in before.items():
        cmd(name, value)
    with open(os.path.join(OUT, 'cosmic-night-pie.json'), 'w', encoding='utf-8') as output:
        json.dump({'verdict': 'PASS' if ok else 'FAIL', 'message': message, 'samples': samples},
                  output, indent=2)
    (unreal.log if ok else unreal.log_error)('COSMIC_NIGHT_PIE %s %s' % ('PASS' if ok else 'FAIL', message))
    unreal.unregister_slate_post_tick_callback(handle)
    unreal.SystemLibrary.quit_editor()


def tick(_dt):
    global phase, mark, atm
    now = time.monotonic()
    if now - start > 150:
        finish(False, 'timeout phase=' + phase)
        return
    try:
        if phase == 'boot' and now - mark > 3:
            for name, value in CVARS.items():
                cmd(name, value)
            les.editor_request_begin_play()
            phase, mark = 'pie', now
        elif phase == 'pie' and les.is_in_play_in_editor() and now - mark > 7:
            world = ues.get_game_world()
            actors = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.AnastasisWorldAtmosphere)
            if len(actors) != 1:
                raise RuntimeError('expected one atmosphere, got %d' % len(actors))
            atm = actors[0]
            samples['veil'] = sample(atm)
            if not (samples['veil']['visible'] and samples['veil']['night'] > .5
                    and samples['veil']['veil'] > .05 and samples['veil']['moon'] > 0):
                raise RuntimeError('clear 23h night did not show sky, veil and moon')
            if 'M_AnastasisCosmicSkyV1' not in samples['veil']['parent']:
                raise RuntimeError('V1 night material was not bound')
            cmd('anastasis.Sky.CosmicArtVersion', 0)
            phase, mark = 'v0', now
        elif phase == 'v0' and now - mark > 2:
            probe = sample(atm)
            if 'M_AnastasisCosmicSky.' in probe['parent']:
                samples['v0'] = probe
                cmd('anastasis.Sky.CosmicArtVersion', 1)
                phase, mark = 'v1', now
            elif now - mark > 18:
                samples['v0_timeout'] = probe
                raise RuntimeError('art A/B did not restore V0 material after 18s')
        elif phase == 'v1' and now - mark > 2:
            probe = sample(atm)
            if 'M_AnastasisCosmicSkyV1' in probe['parent']:
                samples['v1'] = probe
                cmd('anastasis.Sky.Cosmic', 0)
                phase, mark = 'off', now
            elif now - mark > 18:
                samples['v1_timeout'] = probe
                raise RuntimeError('art A/B did not restore V1 material after 18s')
        elif phase == 'off' and now - mark > 2:
            atm = unreal.GameplayStatics.get_all_actors_of_class(
                ues.get_game_world(), unreal.AnastasisWorldAtmosphere)[0]
            samples['off'] = sample(atm)
            if samples['off']['visible']:
                raise RuntimeError('off switch did not hide dome')
            cmd('anastasis.Sky.Cosmic', 1)
            cmd('anastasis.Sky.Hour', 12)
            phase, mark = 'day', now
        elif phase == 'day' and now - mark > 2:
            samples['day'] = sample(atm)
            if samples['day']['night'] > .01 or samples['day']['veil'] > .01:
                raise RuntimeError('night art leaked into day')
            cmd('anastasis.Sky.Hour', 23)
            cmd('anastasis.Sky.Cover', 1)
            phase, mark = 'cloud', now
        elif phase == 'cloud' and now - mark > 2:
            samples['cloud'] = sample(atm)
            if samples['cloud']['night'] > .01 or samples['cloud']['veil'] > .01:
                raise RuntimeError('complete cover did not conceal night art')
            cmd('anastasis.Sky.Cover', 0)
            cmd('anastasis.Sky.CosmicEvent', 1)
            hour = float(atm.call_method('GetFirstCosmicMeteorHour', args=(1,)))
            if hour < 0:
                raise RuntimeError('meteor calendar has no sample')
            samples['meteor_hour'] = hour
            cmd('anastasis.Sky.Day', 1 if hour >= 21 else 2)
            cmd('anastasis.Sky.Hour', hour)
            phase, mark = 'meteor', now
        elif phase == 'meteor' and now - mark > 2:
            samples['meteor'] = sample(atm)
            if samples['meteor']['meteor'] <= .1:
                raise RuntimeError('scheduled meteor did not reach the material')
            finish(True, 'dome, lunar and rare event parameters respond in PIE; image quality separate')
    except Exception:
        finish(False, traceback.format_exc())


handle = unreal.register_slate_post_tick_callback(tick)
