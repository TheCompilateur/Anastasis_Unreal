"""River-use experiment: autonomous thirsty NPC, no well, actual rendered river.
Batch runner only. PASS means this local sampled journey; never global hydrological validity.
"""
import hashlib
import json
import time
from pathlib import Path
import unreal
import anastasis_river_use as proof
from anastasis_map_intelligence.editor import _mesh

ROOT=Path(unreal.Paths.project_dir()).resolve()
OUT=ROOT/'Saved'/'RiverUseEvidence'/str(time.time_ns())
LES=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
UES=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
LIB=unreal.AnastasisRiverUseProbe
LEVEL='/Game/Anastasis/Maps/Lvl_AnastasisSlice'
CVARS={'anastasis.Village.StartVillagers':0,'anastasis.Sim.TimeScale':0,
       'anastasis.Sim.Speed':1,'anastasis.Sim.Warp':1}
old={name:unreal.SystemLibrary.get_console_variable_float_value(name) for name in CVARS}
state={'phase':0,'samples':[],'finished':False,'project':str(ROOT)}
started=time.monotonic(); handle=None


def finish(status,reason):
    if state['finished']: return
    state['finished']=True
    state.update(status=status,reason=reason,wall_seconds=time.monotonic()-started,
        scope='One autonomous NPC, one river bank, sampled trajectory. No player proof or global hydrology claim.')
    try:
        OUT.mkdir(parents=True,exist_ok=True)
        (OUT/'journey.json').write_text(json.dumps(state,indent=2),encoding='utf-8')
    finally:
        for name,value in old.items(): unreal.SystemLibrary.execute_console_command(None,f'{name} {value:g}')
        unreal.unregister_slate_post_tick_callback(handle)
        unreal.log('RIVER_USE '+('PASS' if status=='PASS' else 'FAIL')+' status='+status+' reason='+reason+' output='+str(OUT))
        unreal.SystemLibrary.quit_editor()


def read_sample(raw):
    if 'error' in raw: raise ValueError(raw['error'])
    if raw['world']!=state['context']['world']: raise ValueError('Wrong PIE world')
    scale=state['context']['tile_m']; x,y=raw['x']*scale,raw['y']*scale
    raw.update(tile_m=scale,position_m=[x,y],physical=proof.physical(ground,(lake,river),x,y),
               river_distance_m=proof.distance((x,y),state['selection']['water_m']))
    return raw


def route_segment(a,b):
    # Bounded gap checked by verdict; sample the chord at <=0.5 m for terrain violations.
    import math
    p,q=a['position_m'],b['position_m']; n=max(1,math.ceil(proof.distance(p,q)/.5))
    if n>100: return 'movement_jump'
    for j in range(1,n+1):
        x=p[0]+(q[0]-p[0])*j/n; y=p[1]+(q[1]-p[1])*j/n
        ph=proof.physical(ground,(lake,river),x,y)
        if ph['dry'] is None: return 'route_ground_unknown'
        if ph['dry'] is False: return 'route_crosses_rendered_water'
        if ph['slope']>18: return 'route_crosses_steep_ground'
    return None


def tick(_dt):
    global ground,lake,river,mesh
    try:
        if time.monotonic()-started>360:
            finish('UNKNOWN','wall_timeout'); return
        if state['phase']==0:
            if LES.is_in_play_in_editor(): finish('UNKNOWN','PIE_already_running'); return
            if UES.get_editor_world().get_path_name().split('.')[0]!=LEVEL and not LES.load_level(LEVEL):
                finish('UNKNOWN','map_load_failed'); return
            for name,value in CVARS.items(): unreal.SystemLibrary.execute_console_command(None,f'{name} {value:g}')
            LES.editor_request_begin_play(); state['phase']=1; return
        if not LES.is_in_play_in_editor(): return
        world=UES.get_game_world()
        if not world: return
        if not state.get('startup_restored'):
            # OnWorldBeginPlay already consumed the zero; no need to keep it across timeout.
            value=old['anastasis.Village.StartVillagers']
            unreal.SystemLibrary.execute_console_command(None,f'anastasis.Village.StartVillagers {value:g}')
            state['startup_restored']=True
        if state['phase']==1:
            context=json.loads(LIB.describe(world))
            if context.get('error')=='terrain_not_ready': return
            if 'error' in context: raise ValueError(context['error'])
            if context['world']!=world.get_path_name(): raise ValueError('Wrong survey world')
            state['context']=context
            actors=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.Actor)
            meshes=[m for a in actors for m in a.get_components_by_class(unreal.ProceduralMeshComponent)
                    if m.get_name()=='ExperimentalTerrain' and m.is_visible()]
            if len(meshes)!=1: raise ValueError('Ambiguous rendered terrain')
            mesh=meshes[0]
            if not all(mesh.is_mesh_section_visible(i) for i in (0,1,2)): raise ValueError('Required mesh section hidden')
            data=[_mesh(mesh,i) for i in (0,1,2)]
            if not data[0][1] or not data[2][1]: raise ValueError('Ground or river empty')
            state['geometry']={'component':mesh.get_path_name(),'units':'metres',
                'sha256':hashlib.sha256(json.dumps(data,separators=(',',':')).encode()).hexdigest(),
                'triangles':[len(d[1])//3 for d in data]}
            ground,lake,river=[proof.Mesh(*d) for d in data]
            state['selection']=proof.select(context,ground,lake,river)
            OUT.mkdir(parents=True,exist_ok=True)
            (OUT/'geometry.json').write_text(json.dumps(data,separators=(',',':')),encoding='utf-8')
            sample=read_sample(json.loads(LIB.begin(world,*state['selection']['spawn_sim'])))
            state['samples'].append(sample); state['npc']=sample['npc']
            state['phase']=2
            unreal.SystemLibrary.execute_console_command(None,'anastasis.Sim.TimeScale 0.25')
            return
        sample=read_sample(json.loads(LIB.read(world,state['npc'])))
        previous=state['samples'][-1]
        if sample['time']==previous['time']: return
        state['samples'].append(sample)
        failure=route_segment(previous,sample)
        if failure: finish('FAIL',failure); return
        status,reason=proof.verdict(state['samples'],state['selection'],final=sample['time']-state['samples'][0]['time']>=60)
        if status!='PENDING':
            # Verify that the sampled geometry was not changed during the scenario.
            current=[_mesh(mesh,i) for i in (0,1,2)]
            state['geometry']['end_sha256']=hashlib.sha256(json.dumps(current,separators=(',',':')).encode()).hexdigest()
            if state['geometry']['end_sha256']!=state['geometry']['sha256']:
                status,reason='UNKNOWN','geometry_changed'
            finish(status,reason)
    except Exception as exc:
        finish('UNKNOWN',repr(exc))


handle=unreal.register_slate_post_tick_callback(tick)
