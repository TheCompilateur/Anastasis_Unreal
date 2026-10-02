"""Regenerate the authoritative ground assets, then capture in the SAME editor."""
import os, time, traceback, unreal
from pathlib import Path
root = Path(__file__).resolve().parents[2]
os.environ['ANASTASIS_GROUND_REBUILD'] = '1'
source = root / 'tools/unreal/ground-material.py'
code = source.read_text(encoding='utf-8')
# Load the authority without its standalone quit callback. Keep the actual run/stats code.
cut = code.rindex('\ntry:\n    run()')
ns = {'__file__': str(source), '__name__': 'soil_material_authority'}
handle = None
started = time.monotonic()
def step(dt):
    global handle
    try:
        if ns['report_stats']():
            unreal.unregister_slate_post_tick_callback(handle)
            handle = None
            unreal.log('SOIL_REBUILD_COMPLETE')
            capture = root / 'tools/soil-crusade/capture.py'
            exec(compile(capture.read_text(encoding='utf-8'), str(capture), 'exec'),
                 {'__file__': str(capture), '__name__': '__main__'})
        elif time.monotonic() - started > 240:
            raise RuntimeError('material stats timeout; no capture of uncompiled material')
    except Exception:
        if handle is not None:
            unreal.unregister_slate_post_tick_callback(handle)
            handle = None
        unreal.log_error('SOIL_REBUILD_FAILED ' + traceback.format_exc())
        unreal.SystemLibrary.quit_editor()
try:
    exec(compile(code[:cut], str(source), 'exec'), ns)
    ns['run']()
    handle = unreal.register_slate_post_tick_callback(step)
except Exception:
    unreal.log_error('SOIL_REBUILD_FAILED ' + traceback.format_exc())
    unreal.SystemLibrary.quit_editor()
