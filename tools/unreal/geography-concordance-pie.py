"""Batch-only opening water acquisition proof. INSTRUMENT_PASS never means world agreement.
Exports raw tile-centre partition, provenance and a diagnostic SVG; no assets saved.
NPC journeys, water quality, discharge and basin closure remain UNKNOWN.
"""
import json
import time
from pathlib import Path
import unreal
from anastasis_map_intelligence import core, editor, concordance

ROOT=Path(unreal.Paths.project_dir()).resolve()
OUT=ROOT/'Saved'/'GeographyConcordanceEvidence'
LES=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
UES=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
LEVEL='/Game/Anastasis/Maps/Lvl_AnastasisSlice'
CVARS={'anastasis.Village.SiteSelection':1,'anastasis.Village.StartVillagers':12}
previous={k:unreal.SystemLibrary.get_console_variable_int_value(k) for k in CVARS}
started=time.monotonic()
state={'phase':0,'finished':False}
handle=None


def finish(ok,reason):
    if state['finished']: return
    state['finished']=True
    for name,value in previous.items():
        unreal.SystemLibrary.execute_console_command(None,f'{name} {value}')
    unreal.unregister_slate_post_tick_callback(handle)
    unreal.log('GEOGRAPHY_CONCORDANCE '+('INSTRUMENT_PASS' if ok else 'FAIL')+' '+reason)
    unreal.SystemLibrary.quit_editor()


def tick(_dt):
    try:
        if time.monotonic()-started>240:
            finish(False,'timeout'); return
        if state['phase']==0:
            # Batch runner must have ended the preceding job before this proof.
            if LES.is_in_play_in_editor():
                finish(False,'unexpected existing PIE'); return
            if UES.get_editor_world().get_path_name().split('.')[0]!=LEVEL:
                if not LES.load_level(LEVEL):
                    finish(False,'map load failed'); return
            for name,value in CVARS.items():
                unreal.SystemLibrary.execute_console_command(None,f'{name} {value}')
            LES.editor_request_begin_play()
            state['phase']=1
            return
        if not LES.is_in_play_in_editor(): return
        world=UES.get_game_world()
        if not world: return
        report=json.loads(unreal.AnastasisSimulationDebugLibrary.get_settlement_site_status(world))
        if report.get('status') in ('pending','not_started'): return
        summary=concordance.validate(report)
        if summary['compared']==0: raise ValueError('No paired samples acquired')
        c=report['water_concordance']
        if c['world']!=world.get_path_name() or not c['terrain'].startswith(world.get_path_name()):
            raise ValueError('Survey belongs to another world')
        actors=list(unreal.GameplayStatics.get_all_actors_of_class(world,unreal.Actor))
        # Bounded acquisition only; avoid expensive neighbourhood analysis here.
        settings=core.Settings(step_m=8,max_cells=100000)
        with unreal.ScopedSlowTask(1,'Read geographic water coverage') as progress:
            grid,targets,source,sampling,water=editor._get_grid(actors,settings,progress)
        sections={v['section']:v for v in water}
        if not all(sections.get(i,{}).get('status')=='SAMPLED' for i in (1,2)):
            raise ValueError('Expected visible lake and river meshes were not acquired')
        report['instrument']={'project':str(ROOT),'world':world.get_path_name(),
            'water_sources':water,'grid':grid.identity(),'source':source,'sampling':sampling,
            'scope':'acquisition only; water mismatch allowed and reported; no NPC journey proof',
            'summary':summary,'seconds':time.monotonic()-started}
        OUT.mkdir(parents=True,exist_ok=True)
        (OUT/'opening-water.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
        (OUT/'opening-water.svg').write_text(concordance.svg(report),encoding='utf-8')
        finish(True,json.dumps(summary,sort_keys=True))
    except Exception as exc:
        finish(False,repr(exc))


handle=unreal.register_slate_post_tick_callback(tick)
