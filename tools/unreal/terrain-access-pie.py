"""Batch-only terrain accessibility diagnostic. INSTRUMENT_PASS is not path validity."""
import hashlib
import json
import math
import time
from pathlib import Path
import unreal
import anastasis_terrain_access as audit
from anastasis_map_intelligence.editor import _mesh

ROOT=Path(unreal.Paths.project_dir()).resolve()
OUT=ROOT/'Saved'/'TerrainAccessEvidence'/str(time.time_ns())
LES=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
UES=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
LIB=unreal.AnastasisTerrainAccessProbe
LEVEL='/Game/Anastasis/Maps/Lvl_AnastasisSlice'
CVARS={'anastasis.Village.StartVillagers':0,'anastasis.Sim.TimeScale':0,
       'anastasis.Sim.Speed':1,'anastasis.Sim.Warp':1}
old={k:unreal.SystemLibrary.get_console_variable_float_value(k) for k in CVARS}
state={'phase':0,'finished':False,'project':str(ROOT),'samples':[],'segments':[],
       'policy':{'step_m':.5,'slope_limit_deg':18,'freeboard_m':.1,'max_gap_sim_s':.1}}
started=time.monotonic()
handle=None
mesh=None
surfaces=None
previous=None


def cmd(k,v):
    unreal.SystemLibrary.execute_console_command(None,f'{k} {v:g}')


def fingerprint(data):
    return hashlib.sha256(json.dumps(data,separators=(',',':')).encode()).hexdigest()


def write_overlay():
    routes=state.get('routes',[])
    points=[s for r in routes for s in r.get('samples',[])]
    if not points: return
    x0=min(p['x'] for p in points); y0=min(p['y'] for p in points)
    width=max(1,max(p['x'] for p in points)-x0); height=max(1,max(p['y'] for p in points)-y0)
    scale=min(850/width,550/height)
    def xy(p): return f'{40+(p["x"]-x0)*scale:.2f},{70+(p["y"]-y0)*scale:.2f}'
    svg=['<svg xmlns="http://www.w3.org/2000/svg" width="940" height="710" viewBox="0 0 940 710">',
         '<rect width="940" height="710" fill="#101820"/>',
         '<text x="30" y="28" fill="white">Terrain access: planned XY routes / sampled terrain anomalies</text>']
    for i,r in enumerate(routes):
        color=['#6fcfff','#eed070','#ad93ff'][i%3]
        pts=r.get('samples',[])
        svg.append(f'<polyline points="{" ".join(xy(p) for p in pts)}" fill="none" stroke="{color}" stroke-width="2"/>')
        for p in pts:
            if p['flags']:
                x,y=xy(p).split(','); svg.append(f'<circle cx="{x}" cy="{y}" r="2" fill="#ff6464"/>')
        svg.append(f'<text x="30" y="{650+i*18}" fill="{color}">{r["name"]}: {r["status"]}</text>')
    svg.append('</svg>')
    (OUT/'routes.svg').write_text('\n'.join(svg),encoding='utf-8')


def finish(ok,reason):
    if state['finished']: return
    state['finished']=True
    try:
        if mesh is not None and 'geometry_sha256' in state:
            fresh=[_mesh(mesh,i) for i in (0,1,2)]
            state['geometry_unchanged']=fingerprint(fresh)==state['geometry_sha256']
            ok=ok and state['geometry_unchanged']
        state.update(instrument='PASS' if ok else 'FAIL',reason=reason,
                     scope='Three planned routes and sampled autonomous NPC movement in a seeded PIE fixture. No collision or player proof.')
        OUT.mkdir(parents=True,exist_ok=True)
        (OUT/'audit.json').write_text(json.dumps(state,indent=2),encoding='utf-8')
        write_overlay()
    except Exception as exc:
        ok=False
        unreal.log_error('TERRAIN_ACCESS_OUTPUT_ERROR '+repr(exc))
    finally:
        for k,v in old.items(): cmd(k,v)
        unreal.unregister_slate_post_tick_callback(handle)
        unreal.log('TERRAIN_ACCESS '+('INSTRUMENT_PASS' if ok else 'FAIL')+' '+reason+' output='+str(OUT))
        unreal.SystemLibrary.quit_editor()


def tick(_dt):
    global mesh,surfaces,previous
    try:
        run()
    except Exception as exc:
        finish(False,repr(exc))


def run():
    global mesh,surfaces,previous
    if time.monotonic()-started>480:
        finish(False,'timeout'); return
    if state['phase']==0:
        if LES.is_in_play_in_editor(): finish(False,'PIE_already_running'); return
        if UES.get_editor_world().get_path_name().split('.')[0]!=LEVEL and not LES.load_level(LEVEL):
            finish(False,'map_load_failed'); return
        for k,v in CVARS.items(): cmd(k,v)
        LES.editor_request_begin_play(); state['phase']=1; return
    if not LES.is_in_play_in_editor(): return
    world=UES.get_game_world()
    if not world: return
    if state['phase']==1:
        context=json.loads(LIB.begin(world))
        if context.get('error') in ('terrain_not_ready','not_running_PIE'): return
        if 'error' in context: raise ValueError(context['error'])
        if context['world']!=world.get_path_name(): raise ValueError('wrong PIE world')
        state['context']=context
        cmd('anastasis.Village.StartVillagers',old['anastasis.Village.StartVillagers'])
        meshes=[m for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.Actor)
                for m in a.get_components_by_class(unreal.ProceduralMeshComponent)
                if m.get_name()=='ExperimentalTerrain' and m.is_visible()]
        if len(meshes)!=1: raise ValueError('ambiguous terrain')
        mesh=meshes[0]
        if not all(mesh.is_mesh_section_visible(i) for i in (0,1,2)):
            raise ValueError('missing visible ground/water sections')
        data=[_mesh(mesh,i) for i in (0,1,2)]
        if not data[0][1]: raise ValueError('empty ground')
        surfaces=[audit.Surface(*d) for d in data]
        state['geometry_sha256']=fingerprint(data)
        state['geometry_component']=mesh.get_path_name()
        OUT.mkdir(parents=True,exist_ok=True)
        (OUT/'geometry.json').write_text(json.dumps({'units':'metres','sections':[0,1,2],'data':data}),encoding='utf-8')
        state['routes']=[]
        for r in context['routes']:
            if not r['found']:
                state['routes'].append(dict(name=r['name'],status='UNKNOWN_NO_SEMANTIC_PATH')); continue
            points=[[p['x']*context['tile_m'],p['y']*context['tile_m']] for p in r['points']]
            result=audit.inspect_path(points,surfaces[0],surfaces[1:])
            result['name']=r['name']; state['routes'].append(result)
        state['phase']=2
        cmd('anastasis.Sim.TimeScale',.5)
        return
    raw=json.loads(LIB.read(world))
    if 'error' in raw: raise ValueError(raw['error'])
    if raw['world']!=state['context']['world']: raise ValueError('PIE identity changed')
    if previous is not None and raw['time']==previous['time']: return
    actors=[a for a in raw['actors'] if a['id']==state['context']['npc']]
    if len(actors)!=1: raise ValueError('tracked farmer missing')
    row={'time':raw['time'],'actor':actors[0]}
    if not state['samples']: state['start_sim']=row['time']
    state['samples'].append(row)
    if previous is not None:
        segment=audit.observed_segment(previous,row,state['context']['tile_m'],surfaces[0],surfaces[1:])
        segment.update(start_time=previous['time'],end_time=row['time'])
        state['segments'].append(segment)
    previous=row
    if row['time']-state['start_sim']>=180:
        known=[s for s in state['segments'] if s['status']!='UNKNOWN']
        distance=sum(s.get('length_m',0) for s in known)
        visible=[s for s in state['samples'] if 'visual_xy_cm' in s['actor'] and not s['actor']['visual_hidden']]
        visual_distance=0.
        if visible:
            p=visible[0]['actor']['visual_xy_cm']
            visual_distance=max(math.hypot(s['actor']['visual_xy_cm']['x']-p['x'],s['actor']['visual_xy_cm']['y']-p['y'])/100 for s in visible)
        state['movement_summary']={'known_chord_length_m':distance,'visual_displacement_m':visual_distance,
            'unknown_segments':len(state['segments'])-len(known),'anomaly_segments':sum(s['status']=='ANOMALY' for s in known),
            'route_execution':'UNKNOWN: planned routes are not claimed as fully traversed',
            'last_gathered':actors[0]['gathered'],'last_delivered':actors[0]['delivered']}
        ok=len(state['routes'])==3 and distance>=10 and visual_distance>=10 and actors[0]['gathered']>0
        finish(ok,'diagnostic_complete' if ok else 'insufficient_observed_movement')

handle=unreal.register_slate_post_tick_callback(tick)
