"""SKY_CONTINUITY_002: continuous dawn/dusk A/B/A in PIE, not Apply-per-hour stills.

Run with editor-batch.ps1 -Proofs sky-passage-pie. Optional ANASTASIS_PASSAGE_SHOTS=0
disables Shot images. PASS covers sampled component/exposure contracts ONLY. Images
still require visual review. Shot cadence cannot exclude a one-frame flash between images.
No assets saved. One fixed 1.7 m camera, seed and weather held between all three states.
"""
import json
import os
import time
import traceback
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.project_dir())
OUT = ROOT / 'Saved' / 'SkyPassageEvidence' / time.strftime('%Y%m%d-%H%M%S')
OUT.mkdir(parents=True, exist_ok=False)
SHOTS = os.environ.get('ANASTASIS_PASSAGE_SHOTS', '1') != '0'
SHOT_ROOT = ROOT / 'Saved' / 'Screenshots'
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
V = unreal.Vector
LABEL = 'SkyPassageProofCamera'
DURATION = 12.0
PLAN = [(label, enabled, start, end) for start, end in [(4., 8.), (16., 20.)]
        for label, enabled in [('reference', 0), ('passage', 1), ('reference2', 0)]]
CVARS = {'anastasis.Sim.TimeScale': 0, 'anastasis.Sky.Day': 1,
         'anastasis.Sky.Hour': 4, 'anastasis.Sky.Humidity': .45,
         'anastasis.Sky.Wind': .3, 'anastasis.Sky.Cover': .25,
         'anastasis.Sky.Clock': 1, 'anastasis.Sky.Passage': 1,
         'r.MotionBlurQuality': 0, 't.MaxFPS': 30}
before = {k: unreal.SystemLibrary.get_console_variable_float_value(k) for k in CVARS}
report = {'scope': 'sampled component contracts; visual verdict separate',
          'project': str(ROOT), 'sequences': {}, 'errors': [], 'camera': None}
s = {'phase': 'boot', 'mark': time.monotonic(), 'index': 0, 'pending': None, 'last_shot': 0.}
started = time.monotonic()
handle = None


def cmd(name, value):
    unreal.SystemLibrary.execute_console_command(None, '%s %g' % (name, value))


def finish(ok, reason):
    if s.get('done'):
        return
    s['done'] = True
    for k, v in before.items():
        cmd(k, v)
    report.update(instrument_pass=ok, reason=reason)
    (OUT / 'passage.json').write_text(json.dumps(report, indent=1), encoding='utf-8')
    unreal.unregister_slate_post_tick_callback(handle)
    (unreal.log if ok else unreal.log_error)('SKY_PASSAGE %s %s out=%s' % ('PASS' if ok else 'FAIL', reason, OUT))
    unreal.SystemLibrary.quit_editor()


def tick(dt):
    now = time.monotonic()
    if now - started > 480:
        finish(False, 'timeout ' + s['phase'])
        return
    if s['phase'] == 'boot':
        if now - s['mark'] > 2:
            for k, v in CVARS.items():
                cmd(k, v)
            les.editor_request_begin_play()
            s.update(phase='pie', mark=now)
        return
    world = ues.get_game_world()
    if not world or not les.is_in_play_in_editor():
        return
    if s['phase'] == 'pie':
        if now - s['mark'] < 8:
            return
        atmos = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.AnastasisWorldAtmosphere)
        cameras = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.CameraActor)
        if len(atmos) != 1:
            raise RuntimeError('expected one atmosphere, got %d' % len(atmos))
        s['atmos'] = atmos[0]
        camera = next(c for c in cameras if c.get_actor_label() == LABEL)
        # Same ground-traced human-height pose for A/B/A; no inferred height.
        def ground(x, y):
            hit = unreal.SystemLibrary.line_trace_single(world, V(x, y, 200000), V(x, y, -200000),
                unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, True, [], unreal.DrawDebugTrace.NONE, True)
            return hit.to_tuple()[4].z if hit else None
        width = 0
        while width < 1e6 and ground(width + 1000, 1000) is not None:
            width += 1000
        if width <= 0:
            raise RuntimeError('terrain width unavailable')
        x, y = width * 44 / 96, width * 52 / 96
        z = ground(x, y)
        if z is None:
            raise RuntimeError('no ground at camera')
        eye = V(x, y, z + 170)
        target = V(x, y - 10000, eye.z + 800)  # west: evening sun, dawn landscape
        camera.set_actor_location(eye, False, True)
        camera.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(eye, target), True)
        pc = unreal.GameplayStatics.get_player_controller(world, 0)
        if not pc:
            raise RuntimeError('no player controller')
        pc.set_view_target_with_blend(camera, 0.)
        report['camera'] = {'eye': [eye.x, eye.y, eye.z], 'target': [target.x, target.y, target.z]}
        lights = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.DirectionalLight)
        s['sun'] = next(a for a in lights if not a.actor_has_tag('AnastasisMoon'))
        s['moon'] = next(a for a in lights if a.actor_has_tag('AnastasisMoon'))
        s['phase'] = 'prepare'
    if s['phase'] == 'prepare':
        if s['index'] == len(PLAN):
            candidate = [v for v in report['sequences'].values() if v['enabled']]
            # A run that never traversed the horizon is not a transition proof.
            covered = all(len(v['samples']) >= 20 and min(r['sun_elev'] for r in v['samples']) < -5
                          and max(r['sun_elev'] for r in v['samples']) > 5 for v in candidate)
            first_dawn = next(iter(report['sequences'].values()))
            negative = max(r['target_ev'] - r['applied_ev'] for r in first_dawn['samples']) > .5
            if not negative:
                report['errors'].append('negative control did not exercise the exposure-lag defect')
            ok = len(candidate) == 2 and covered and not report['errors']
            finish(ok, 'sampled contracts only; review continuous images separately')
            return
        label, enabled, start, end = PLAN[s['index']]
        key = '%02d_%s_%02d-%02d' % (s['index'], label, start, end)
        cmd('anastasis.Sky.Passage', enabled)
        cmd('anastasis.Sky.Hour', start)
        s['atmos'].apply()  # initial condition ONLY; never during sweep
        s.update(key=key, enabled=enabled, start=start, end=end, phase='settle', mark=now)
        report['sequences'][key] = {'enabled': enabled, 'samples': [], 'shots': []}
        return
    if s['phase'] == 'settle':
        if now - s['mark'] > 6:
            s.update(phase='sweep', mark=now, last_shot=0.)
        return
    seq = report['sequences'][s['key']]
    if s['pending']:
        old, fired = s['pending']
        fresh = sorted(set(SHOT_ROOT.rglob('*.png')) - old)
        if fresh:
            dest = OUT / ('%s_%04d.png' % (s['key'], len(seq['shots'])))
            fresh[-1].replace(dest)
            seq['shots'].append({'path': dest.name, 'request_seconds': fired - s['mark']})
            s['pending'] = None
        elif now - fired > 15:
            raise RuntimeError('Shot produced no image')
    if s['phase'] == 'drain':
        if s['pending'] is None:
            if SHOTS and len(seq['shots']) < 10:
                report['errors'].append('insufficient images ' + s['key'])
            s['index'] += 1
            s['phase'] = 'prepare'
        return
    elapsed = now - s['mark']
    ev, target = s['atmos'].get_applied_exposure_ev(), s['atmos'].get_target_exposure_ev()
    # ExposureVolume est un UPROPERTY() sans specificateur : invisible en Python. Le volume est le
    # PostProcessVolume non borne que l'atmosphere adopte ou cree (AdoptOrSpawn), retrouve dans le monde.
    if s.get('exposure') is None:
        volumes = [v for v in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.PostProcessVolume)
                   if v.get_editor_property('unbound')]
        owned = [v for v in volumes if v.get_owner() == s['atmos']]
        s['exposure'] = (owned or volumes or [None])[0]
        if s['exposure'] is None:
            raise RuntimeError('exposure volume not found')
    settings = s['exposure'].get_editor_property('settings')
    volume_ev = settings.get_editor_property('auto_exposure_min_brightness')
    sun, moon = s['sun'].get_component_by_class(unreal.DirectionalLightComponent), s['moon'].get_component_by_class(unreal.DirectionalLightComponent)
    elev = -s['sun'].get_actor_rotation().pitch
    row = {'seconds': elapsed, 'sun_elev': elev, 'target_ev': target, 'applied_ev': ev, 'volume_ev': volume_ev,
           'sun_diffuse': sun.get_editor_property('diffuse_scale'),
           'moon_diffuse': moon.get_editor_property('diffuse_scale'),
           'sun_specular': sun.get_editor_property('specular_scale'),
           'moon_specular': moon.get_editor_property('specular_scale')}
    seq['samples'].append(row)
    if s['enabled'] and (ev < target - .006 or volume_ev < target - .006
                         or abs(volume_ev - ev) > .006
                         or (elev <= 0 and max(row['sun_diffuse'], row['sun_specular']) > .0001)
                         or (elev >= 0 and max(row['moon_diffuse'], row['moon_specular']) > .0001)):
        report['errors'].append('%s sample=%d' % (s['key'], len(seq['samples'])))
    if elapsed >= DURATION:
        s['phase'] = 'drain'
        return
    if SHOTS and s['pending'] is None and now - s['last_shot'] >= .25:
        s['pending'] = (set(SHOT_ROOT.rglob('*.png')), now)
        unreal.SystemLibrary.execute_console_command(world, 'Shot')
        s['last_shot'] = now
    # Drives the NEXT rendered tick. Sample above describes the previous actual tick.
    cmd('anastasis.Sky.Hour', s['start'] + (s['end'] - s['start']) * min(1., elapsed / DURATION))


def guarded(dt):
    try:
        tick(dt)
    except Exception:
        unreal.log_error(traceback.format_exc())
        finish(False, 'exception')


les.load_level('/Game/Anastasis/Maps/Lvl_AnastasisSlice')
unreal.get_editor_subsystem(unreal.EditorActorSubsystem).spawn_actor_from_class(
    unreal.CameraActor, V(0, 0, 1000), unreal.Rotator()).set_actor_label(LABEL)
handle = unreal.register_slate_post_tick_callback(guarded)
