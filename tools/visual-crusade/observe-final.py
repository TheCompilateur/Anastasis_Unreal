"""Final assembled slice: fixed baseline poses then a short human-height camera translation.
Read-only scene observation. No asset/map save, no simulation or visual quality overrides.
Run with ANASTASIS_GROUND_OUT, ANASTASIS_GROUND_CAMERAS and GROUND_STATES=on.
"""
import json
import math
import os
import time
import unreal

root = os.path.abspath(os.path.join(os.path.dirname(__file__), '../..'))
path = os.path.join(root, 'tools/unreal/ground-cover-capture.py')
ns = {'__file__': path, '__name__': '__main__'}
with open(path, encoding='utf-8-sig') as f:
    exec(compile(f.read(), path, 'exec'), ns)
original_finish = ns['finish']
travel = {'handle': None, 'start': None, 'shot': 0, 'poses': []}


def moving_tick(dt):
    try:
        elapsed = time.monotonic() - travel['start']
        fraction = max(0., min(1., (elapsed - 3.) / 10.))
        eye0, target = travel['eye'], travel['target']
        x, y = eye0.x + 1200. * fraction, eye0.y + 400. * fraction
        eye = ns['at'](x / ns['T'], y / ns['T'], 170.)
        if eye is None:
            raise RuntimeError('camera path has no terrain')
        ns['ues'].set_level_viewport_camera_info(eye, ns['look'](eye, target))
        ns['les'].editor_invalidate_viewports()
        thresholds = (3., 8., 13.)
        if travel['shot'] < 3 and elapsed >= thresholds[travel['shot']]:
            image_path = os.path.join(ns['OUT'], 'travel_%02d.png' % travel['shot']).replace('\\', '/')
            ns['cmd']('HighResShot 1600x900 filename="%s"' % image_path)
            travel['poses'].append({'path': image_path, 'elapsed_s': elapsed,
                                    'eye_cm': [eye.x, eye.y, eye.z], 'fraction': fraction})
            travel['shot'] += 1
        if elapsed > 17.:
            if not all(os.path.isfile(p['path']) for p in travel['poses']):
                raise RuntimeError('missing travel screenshot')
            with open(os.path.join(ns['OUT'], 'travel.json'), 'w') as f:
                json.dump(travel['poses'], f, indent=2)
            unreal.log('CRUSADE_TRAVEL_COMPLETE samples=3 distance_m=12.65 eye_height_cm=170')
            unreal.unregister_slate_post_tick_callback(travel['handle'])
            unreal.SystemLibrary.quit_editor()
    except Exception as exc:
        unreal.log_error('CRUSADE_TRAVEL_FAIL ' + str(exc))
        if travel['handle'] is not None:
            unreal.unregister_slate_post_tick_callback(travel['handle'])
        unreal.SystemLibrary.quit_editor()


def fixed_finished(message, error=False):
    if error or 'GROUND_CAPTURE_COMPLETE' not in message:
        original_finish(message, error)
        return
    unreal.log(message)
    unreal.unregister_slate_post_tick_callback(ns['handle'])
    unused, eye, target = next(v for v in ns['views'] if v[0] == 'prairie_eye')
    travel.update(start=time.monotonic(), eye=eye, target=target)
    travel['handle'] = unreal.register_slate_post_tick_callback(moving_tick)

ns['finish'] = fixed_finished