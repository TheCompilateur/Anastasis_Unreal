"""Repeatable Editor smoke test; never quits the user's editor or saves a map."""
from pathlib import Path
import json
import time
import unreal
from . import editor, core


def run(directory=None):
    target=Path(directory) if directory else editor.output_dir()/('Smoke_'+time.strftime('%Y%m%d_%H%M%S'))
    previous_output=editor._OUTPUT
    before=[p.get_path_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()]
    before_actors=sorted(a.get_path_name() for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors())
    try:
        editor._OUTPUT=str(target)
        menu=editor.register()
        a=editor.analyze(); editor.save_snapshot('First')
        b=editor.analyze(); editor.save_snapshot('Second')
        comparison=editor.compare_snapshots('First','Second')
        editor.show('slope'); points=len(editor._DRAW); editor.clear_visualization()
        after=[p.get_path_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()]
        after_actors=sorted(a.get_path_name() for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors())
        checks=dict(menu_registered=menu,repeatable=a['sample_sha256']==b['sample_sha256'],
            dirty_maps_unchanged=before==after,actors_unchanged=before_actors==after_actors,
            visualization_cleared=editor._TICK is None,draw_budget_respected=points<=editor._SETTINGS.max_draw_cells)
        result=dict(checks=checks,status='PASS' if all(checks.values()) else 'FAIL',
            before_dirty=before,after_dirty=after,points=points,statistics=b['statistics'],
            comparison_key=b['comparison_key'],sample_sha256=b['sample_sha256'],
            scope='Editor API and repeatability; visual appearance and native Landscape/NavMesh/WaterBody fixtures not proven by this test')
        target.mkdir(parents=True,exist_ok=True)
        (target/'Smoke.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
        if result['status']!='PASS': raise AssertionError(result)
        unreal.log('MAP_INTELLIGENCE::SMOKE_PASS '+str(target))
        return result
    finally:
        editor.clear_visualization(); editor._OUTPUT=previous_output
