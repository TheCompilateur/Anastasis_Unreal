"""Publish the validated instance scope, then check two views outside the pilot."""
import os, traceback, unreal
from pathlib import Path
root = Path(__file__).resolve().parents[2]
try:
    source = root / 'tools/unreal/ground-material.py'
    code = source.read_text(encoding='utf-8')
    ns = {'__file__': str(source), '__name__': 'soil_material_authority'}
    exec(compile(code[:code.rindex('\ntry:\n    run()')], str(source), 'exec'), ns)
    mi = unreal.load_asset(ns['INSTANCE'])
    if mi is None:
        raise RuntimeError('Ground instance missing')
    parent = mi.get_editor_property('parent')
    names = {str(n) for n in unreal.MaterialEditingLibrary.get_scalar_parameter_names(parent)}
    if parent.get_path_name() != ns['MASTER'] + '.M_AnastasisGround':
        raise RuntimeError('Unexpected ground parent: ' + parent.get_path_name())
    if not {'SoilHistory', 'SoilPilotRadius'}.issubset(names):
        raise RuntimeError('Reloaded master lacks soil parameters: ' + str(sorted(names)))
    unreal.log('SOIL_RELOADED parent=%s history=%s radius=%s' % (
        parent.get_path_name(),
        unreal.MaterialEditingLibrary.get_material_instance_scalar_parameter_value(mi, 'SoilHistory'),
        unreal.MaterialEditingLibrary.get_material_instance_scalar_parameter_value(mi, 'SoilPilotRadius')))
    ns['publish_soil_scope'](mi)
    os.environ['ANASTASIS_SOIL_VIEWS'] = 'vallee_b_eye,hors_vallee_eye'
    capture = root / 'tools/soil-crusade/capture.py'
    exec(compile(capture.read_text(encoding='utf-8'), str(capture), 'exec'),
         {'__file__': str(capture), '__name__': '__main__'})
except Exception:
    unreal.log_error('SOIL_DEPLOY_FAILED ' + traceback.format_exc())
    unreal.SystemLibrary.quit_editor()
