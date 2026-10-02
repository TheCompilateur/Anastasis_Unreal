"""Opening-site comparison + actual PIE opening. No asset save, one editor.
Run through editor-batch -Proofs settlement-site-pie. Geometry/nav selection is not player proof.
"""
import json, os, re, time
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
OUT = ROOT / 'Saved' / 'SettlementSiteEvidence'
OUT.mkdir(parents=True, exist_ok=True)
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
DBG = unreal.AnastasisSimulationDebugLibrary
LEVEL = '/Game/Anastasis/Maps/Lvl_AnastasisSlice'
if ues.get_editor_world().get_path_name().split('.')[0] != LEVEL:
    les.load_level(LEVEL)
cam_source = eas.spawn_actor_from_class(unreal.CameraActor, unreal.Vector(0,0,1000), unreal.Rotator())
cam_source.set_actor_label('SettlementSiteProofCamera')
log_path = Path(os.environ['ANASTASIS_EDITOR_BATCH_JOBS']).parent / 'editor-batch.log'
log_offset = log_path.stat().st_size if log_path.exists() else 0
EXPECTED = {'Anastasis.SettlementSite.' + x for x in ('GeographicChoice','BarriersAndMissingEvidence','ResourcesDriveChoice')}
start = time.monotonic()
handle = None
state = {'phase':0, 'at':start, 'tests':{}, 'checks':[], 'shots':[], 'finished':False}


def cmd(world, text):
    unreal.SystemLibrary.execute_console_command(world, text)


def check(name, value):
    state['checks'].append([name,bool(value)])
    unreal.log('SETTLEMENT_SITE_CHECK %s %s' % ('PASS' if value else 'FAIL', name))


def finish(reason):
    if state['finished']: return
    state['finished'] = True
    ok = bool(state['checks']) and all(c[1] for c in state['checks']) and len(state['shots']) == 3
    output = {k:v for k,v in state.items() if k not in ('camera','shot_before','initial_rows')}
    output.update({'pass':ok,'reason':reason,'project':str(ROOT)})
    (OUT / 'comparison.json').write_text(json.dumps(output,indent=2),encoding='utf-8')
    unreal.log('SETTLEMENT_SITE_PIE %s %s' % ('PASS' if ok else 'FAIL',reason))
    cmd(None,'anastasis.Sim.Warp 1')
    unreal.unregister_slate_post_tick_callback(handle)
    les.editor_request_end_play()
    unreal.SystemLibrary.quit_editor()


def shots():
    folder = ROOT / 'Saved' / 'Screenshots'
    return set(folder.rglob('*.png')) if folder.exists() else set()


def begin_view(world, number):
    selected = state['comparison']['selected']
    legacy = state['comparison']['legacy']
    tile = selected if number < 2 else legacy
    x,y = tile['x'] + 0.5,tile['y'] + 0.5
    target = DBG.get_settlement_ground_point(world,x,y)
    if number == 0:
        distance = 8.0 / state['comparison']['tile_m']
        pos = DBG.get_settlement_ground_point(world,x-distance,y-distance)
        pos.z += 170.0
        target.z += 170.0
    else:
        pos = unreal.Vector(target.x-4500,target.y-4500,target.z+3500)
    camera = state['camera']
    camera.set_actor_location(pos,False,True)
    camera.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(pos,target),True)
    state['view'] = number
    state['view_name'] = ['selected_eye','selected_oblique','legacy_terrain_oblique'][number]
    state['shot_before'] = shots()
    state['at'] = time.monotonic()
    state['phase'] = 4


def tick(_dt):
    try:
        run()
    except Exception as exc:
        check('no_exception',False)
        finish(repr(exc))


def run():
    now = time.monotonic()
    if now-start > 300:
        check('timeout',False); finish('timeout'); return
    phase = state['phase']
    if phase == 0 and now-start > 3:
        cmd(None,'Automation RunTests Anastasis.SettlementSite')
        state['phase'] = 1
    elif phase == 1:
        with log_path.open('rb') as f:
            f.seek(log_offset)
            tail = f.read().decode('utf-8',errors='replace')
        for result,path in re.findall(r'Test Completed\. Result=\{(\w+)\}.*?Path=\{([^}]+)\}',tail):
            if path in EXPECTED: state['tests'][path]=result
        if set(state['tests']) != EXPECTED: return
        check('three_targeted_tests',all(v=='Success' for v in state['tests'].values()))
        if not state['checks'][-1][1]: finish('targeted test failure'); return
        cmd(None,'anastasis.Village.SiteSelection 1')
        cmd(None,'anastasis.Village.StartVillagers 12')
        cmd(None,'anastasis.Sim.TimeScale 1')
        cmd(None,'anastasis.Sim.Speed 1')
        cmd(None,'anastasis.Sim.Warp 1')
        les.editor_request_begin_play()
        state['phase']=2
    elif phase == 2 and les.is_in_play_in_editor():
        world=ues.get_game_world()
        if not world: return
        data=json.loads(DBG.get_settlement_site_status(world))
        if data.get('status')=='pending': return
        state['comparison']=data
        check('geographic_selection',data.get('status')=='selected')
        if data.get('status')!='selected': finish('no geographic site'); return
        best=data['selected']; old=data['legacy']
        check('better_or_legacy_ineligible',not old['eligible'] or best['score']>=old['score'])
        check('site_slope',best['slope_deg']<=8.0)
        check('connected_area',best['area_m2']>=3600)
        check('water_access',0<=best['water_m']<=300)
        check('food_access',0<=best['food_m']<=600)
        check('wood_access',0<=best['wood_m']<=600)
        buildings=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.AnastasisVillageBuilding)
        expected_x=(best['x']+0.5)*data['tile_m']*100
        expected_y=(best['y']+0.5)*data['tile_m']*100
        check('well_really_at_selected_site',any(abs(a.get_actor_location().x-expected_x)<1 and abs(a.get_actor_location().y-expected_y)<1 for a in buildings))
        cards=json.loads(DBG.get_villager_cards(world))
        check('twelve_npcs',cards.get('npcs')==12)
        state['initial_rows']={r['npc']:(r['x'],r['y']) for r in cards.get('villagers',[])}
        state['sim_start']=DBG.get_simulation_time(world)
        state['phase']=3
        cameras=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.CameraActor)
        state['camera']=next(c for c in cameras if c.get_actor_label()=='SettlementSiteProofCamera')
        unreal.GameplayStatics.get_player_controller(world,0).set_view_target_with_blend(state['camera'],0.0)
    elif phase == 3:
        world=ues.get_game_world()
        if DBG.get_simulation_time(world)-state['sim_start'] < 10: return
        cards=json.loads(DBG.get_villager_cards(world))
        moved=any(r['npc'] in state['initial_rows'] and
                  abs(r['x']-state['initial_rows'][r['npc']][0])+abs(r['y']-state['initial_rows'][r['npc']][1])>20
                  for r in cards.get('villagers',[]))
        check('simulation_advanced_and_npcs_moved',moved)
        cmd(world,'anastasis.Sim.Warp 0')
        begin_view(world,0)
    elif phase == 4 and now-state['at']>1.5:
        cmd(ues.get_game_world(),'Shot')
        state['phase']=5; state['at']=now
    elif phase == 5:
        fresh=sorted(shots()-state['shot_before'],key=lambda p:p.stat().st_mtime)
        if not fresh:
            if now-state['at']>25: check('screenshot_written',False); finish('missing screenshot')
            return
        destination=OUT/(state['view_name']+'.png')
        fresh[-1].replace(destination)
        state['shots'].append(str(destination))
        if state['view']<2: begin_view(ues.get_game_world(),state['view']+1)
        else:
            check('three_actual_screenshots',True)
            finish('geographic opening observed; images require visual review, no global navigation claim')


handle=unreal.register_slate_post_tick_callback(tick)
