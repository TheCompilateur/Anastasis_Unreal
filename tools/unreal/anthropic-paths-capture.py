"""Actual opening NPC traffic, frozen-memory off/on/off control at human height. No asset save."""
import json, os, re, time, statistics
from pathlib import Path
import unreal
ROOT=Path(unreal.Paths.project_dir()).resolve()
ROUTINE=os.environ.get('ANASTASIS_PATHS_SCENARIO')=='farmer'
OUT=ROOT/'Saved'/'AnthropicPathsEvidence'
if ROUTINE: OUT=OUT/'farmer'
OUT.mkdir(parents=True,exist_ok=True)
les=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ues=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
DBG=unreal.AnastasisSimulationDebugLibrary
level='/Game/Anastasis/Maps/Lvl_AnastasisSlice'
if ues.get_editor_world().get_path_name().split('.')[0]!=level: les.load_level(level)
cam=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).spawn_actor_from_class(unreal.CameraActor,unreal.Vector(),unreal.Rotator())
cam.set_actor_label('AnthropicPathsProofCamera')
log=Path(os.environ['ANASTASIS_EDITOR_BATCH_JOBS']).parent/'editor-batch.log'
offset=log.stat().st_size
expected={'Anastasis.Anthropic.'+x for x in ('ObservationBoundary','RepetitionRecoveryAndBounds','DistanceNotFrames','ObservedContinuity')}
start=time.monotonic()
s={'phase':'tests_start','at':start,'checks':[],'tests':{},'samples':[],'captures':{},'finished':False}
handle=None

def cmd(t): unreal.SystemLibrary.execute_console_command(ues.get_game_world(),t)
def report():
    return {k:float(v) for k,v in (part.split('=',1) for part in unreal.AnastasisAnthropicDebugLibrary.get_status(ues.get_game_world()).split() if '=' in part)}
def check(name,ok):
    s['checks'].append([name,bool(ok)])
    unreal.log('ANTHROPIC_PATH_CHECK %s %s'%('PASS' if ok else 'FAIL',name))
def finish(reason):
    if s['finished']: return
    s['finished']=True
    ok=bool(s['checks']) and all(v for _,v in s['checks']) and len(s['captures'])==6
    data={k:v for k,v in s.items() if k not in ('camera','actor','files','timings')}
    data.update(pass_result=ok,reason=reason,project=str(ROOT))
    (OUT/'paths.json').write_text(json.dumps(data,indent=2),encoding='utf-8')
    unreal.log('ANTHROPIC_PATHS %s %s'%('PASS' if ok else 'FAIL',reason))
    cmd('anastasis.Anthropic.Memory 0'); cmd('anastasis.Anthropic.Draw 1'); cmd('anastasis.Sim.Warp 1')
    unreal.unregister_slate_post_tick_callback(handle)
    les.editor_request_end_play(); unreal.SystemLibrary.quit_editor()
def files(): return set((ROOT/'Saved'/'Screenshots').rglob('*.png'))
def next_pose():
    n=len(s['captures']); view=n//3; mode=['off','on','off2'][n%3]
    s['name']=['eye','oblique'][view]+'_'+mode
    cmd('anastasis.Anthropic.Draw '+('1' if mode=='on' else '0'))
    focus=s['focus']; tile=s['site']['tile_m']*100; x,y=focus['focus_x']/tile,focus['focus_y']/tile
    w=ues.get_game_world()
    target=DBG.get_settlement_ground_point(w,x,y)
    if view==0:
        pos=DBG.get_settlement_ground_point(w,x-0.20,y-0.20); pos.z+=170
        target.z+=15
    else: pos=unreal.Vector(target.x-650,target.y-650,target.z+700)
    s['camera'].set_actor_location(pos,False,True)
    s['camera'].set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(pos,target),True)
    s['at']=time.monotonic(); s['phase']='settle'; s['timings']=[]
def tick(dt):
    try: run(dt)
    except Exception as e: check('no_exception',False); finish(repr(e))
def run(dt):
    now=time.monotonic(); phase=s['phase']
    if now-start>(540 if ROUTINE else 360): check('timeout',False); finish('timeout'); return
    if phase=='tests_start' and now-start>3:
        cmd('Automation RunTests Anastasis.Anthropic'); s['phase']='tests'
    elif phase=='tests':
        with log.open('rb') as f: f.seek(offset); tail=f.read().decode('utf-8',errors='replace')
        for result,path in re.findall(r'Test Completed. Result=\{(\w+)\}.*?Path=\{([^}]+)\}',tail):
            if path in expected: s['tests'][path]=result
        if set(s['tests'])!=expected: return
        check('four_targeted_tests',all(v=='Success' for v in s['tests'].values()))
        if not s['checks'][-1][1]: finish('tests failed'); return
        for c in ['anastasis.Sim.Speed 1','anastasis.Sim.Warp 1','anastasis.Sim.TimeScale 0.15','anastasis.Village.StartVillagers 12','anastasis.Anthropic.Memory '+('0' if ROUTINE else '1'),'anastasis.Anthropic.Draw 1','anastasis.Sky.Hour 11','anastasis.Village.Debug 0']: cmd(c)
        les.editor_request_begin_play(); s['phase']='opening'
    elif phase=='opening' and les.is_in_play_in_editor():
        w=ues.get_game_world()
        data=json.loads(DBG.get_settlement_site_status(w))
        if data.get('status')=='pending': return
        check('geographic_village',data.get('status')=='selected')
        if not s['checks'][-1][1]: finish('village unavailable'); return
        s['site']=data; s['at']=now; s['phase']='observe'; s['sample_at']=0
        if ROUTINE:
            check('routine_starts_without_opening_traces',report().get('cells')==0)
            if not s['checks'][-1][1]: finish('opening traffic contaminated routine'); return
            cmd('Anastasis.Village.FirstFarmer 1')
            cmd('anastasis.Anthropic.Memory 1')
            cmd('anastasis.Sim.TimeScale 0.3')
            s['routine_samples']=[]
        s['sim_start']=DBG.get_simulation_time(w)
        s['camera']=next(c for c in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.CameraActor) if c.get_actor_label()=='AnthropicPathsProofCamera')
        s['actor']=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.AnastasisWorldEmbodiment)[0]
        unreal.GameplayStatics.get_player_controller(w,0).set_view_target_with_blend(s['camera'],0)
        target=DBG.get_settlement_ground_point(w,data['selected']['x']+.5,data['selected']['y']+.5)
        pos=target+unreal.Vector(-800,-800,600)
        s['camera'].set_actor_location(pos,False,True)
        s['camera'].set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(pos,target),True)
    elif phase=='observe':
        if now-s['sample_at']>5:
            r=report(); s['samples'].append(r); s['sample_at']=now
            if ROUTINE:
                g=json.loads(DBG.get_gather_status(ues.get_game_world()))
                s['routine_samples'].append(g)
                total=g['field']+g['bag']+g['stock']+g['meals']
                if 'food_initial' not in s: s['food_initial']=total
                if total!=s['food_initial']: check('food_conservation',False); finish('food conservation failed'); return
                unreal.log('ANTHROPIC_ROUTINE_SAMPLE '+json.dumps(g))
            unreal.log('ANTHROPIC_PATH_SAMPLE '+json.dumps(r))
        if now-s['at']<(260 if ROUTINE else 110): return
        s['sim_elapsed']=DBG.get_simulation_time(ues.get_game_world())-s['sim_start']
        if ROUTINE:
            g=json.loads(DBG.get_gather_status(ues.get_game_world())); s['routine_final']=g
            check('three_actual_deliveries',g.get('deliveries',0)>=3)
            if not s['checks'][-1][1]: finish('insufficient repeated routine; do not infer mature paths'); return
        r=report(); s['focus']=r
        check('real_traffic_visible',r.get('grass',0)>0 and r.get('focus_strength',0)>.15)
        check('no_capacity_loss',r.get('dropped')==0 and r.get('grass_cap')==0)
        if not all(v for _,v in s['checks']): finish('insufficient trustworthy traffic'); return
        cmd('anastasis.Sim.Warp 0'); s['phase']='freeze'; s['at']=now
    elif phase=='freeze' and now-s['at']>2:
        s['frozen_cells']=report()['cells']; next_pose()
    elif phase=='settle':
        if now-s['at']>2:
            t=s['actor'].call_method('GetFrameTimingsMs'); s['timings'].append([t.x,t.y,t.z])
        if now-s['at']<8: return
        r=report(); check(s['name']+'_same_memory',r['cells']==s['frozen_cells'])
        if s['name'].endswith('on'): check(s['name']+'_drawn',r['grass']>0)
        else: check(s['name']+'_restored',r['grass']==0 and r['restore_errors']==0 and r['restored']>0)
        s['capture_report']=r; s['files']=files(); cmd('Shot'); s['phase']='shot'; s['at']=now
    elif phase=='shot':
        fresh=sorted(files()-s['files'],key=lambda p:p.stat().st_mtime)
        if not fresh:
            if now-s['at']>25: check('screenshot',False); finish('missing screenshot')
            return
        p=OUT/(s['name']+'.png'); fresh[-1].replace(p)
        s['captures'][s['name']]={'path':str(p),'report':s['capture_report'],'timing_samples':len(s['timings']),'timing_ms_p50':[statistics.median(t[i] for t in s['timings']) for i in range(3)]}
        if len(s['captures'])<6: next_pose()
        else: finish('real sampled traffic; frozen-memory off/on/off images require artistic review')
handle=unreal.register_slate_post_tick_callback(tick)
