"""Editor-only adapter and Tools menu. Never saves or changes level objects.

Visuals use short-lived debug points from a bounded tick callback. Clearing removes
only this tool's callback, without flushing other agents' debug drawings.
"""
from dataclasses import asdict
from pathlib import Path
import json
import math
import os
import time
import unreal
from . import core

_SETTINGS=core.Settings()
_LAST=None
_WORLD=None
_TICK=None
_DRAW=[]
_MENU='LevelEditor.MainMenu.Tools.AnastasisMapIntelligence'
_OUTPUT=None


def output_dir():
    return Path(_OUTPUT) if _OUTPUT else Path(unreal.Paths.project_saved_dir())/'Anastasis'/'MapAudit'


def configure(**kwargs):
    global _SETTINGS
    values=asdict(_SETTINGS); values.update(kwargs)
    _SETTINGS=core.Settings(**values).validate()
    return asdict(_SETTINGS)


def edit_settings():
    p=output_dir()/'Settings.json'; p.parent.mkdir(parents=True,exist_ok=True)
    if not p.exists(): p.write_text(json.dumps(asdict(_SETTINGS),indent=2),encoding='utf-8')
    os.startfile(str(p.resolve()))


def load_settings():
    p=output_dir()/'Settings.json'
    if p.exists(): configure(**json.loads(p.read_text(encoding='utf-8-sig')))


def _vec(v): return [v.x/100.,v.y/100.,v.z/100.]

def _mesh(component,section):
    vertices,indices,*_=unreal.ProceduralMeshLibrary.get_section_from_procedural_mesh(component,section)
    transform=component.get_world_transform()
    return [_vec(unreal.MathLibrary.transform_location(transform,v)) for v in vertices],list(indices)


def _transform(actor):
    rotation=actor.get_actor_rotation(); scale=actor.get_actor_scale3d()
    return dict(location_m=_vec(actor.get_actor_location()),
        rotation_deg=[rotation.pitch,rotation.yaw,rotation.roll],scale=[scale.x,scale.y,scale.z])


def _fingerprint(actors):
    # Loaded-object provenance; sampled geometry SHA256 is separately recorded.
    return [dict(path=a.get_path_name(),kind=a.get_class().get_name(),
        transform=_transform(a)) for a in sorted(actors,key=lambda a:a.get_path_name())]


def _get_grid(actors,s,progress):
    landscapes=[a for a in actors if isinstance(a,unreal.LandscapeProxy)]
    if landscapes:
        bounds=[]; colliders=[]
        for a in landscapes:
            for c in a.get_components_by_class(unreal.LandscapeHeightfieldCollisionComponent):
                origin,extent,_=unreal.SystemLibrary.get_component_bounds(c)
                b=[(origin.x-extent.x)/100,(origin.y-extent.y)/100,
                   (origin.x+extent.x)/100,(origin.y+extent.y)/100,
                   (origin.z-extent.z)/100,(origin.z+extent.z)/100]
                bounds.append(b); colliders.append((c,b))
        if not bounds: raise RuntimeError('Loaded Landscape has no collision components; no measurements fabricated')
        g=core.Grid.covering([min(b[0] for b in bounds),min(b[1] for b in bounds),max(b[2] for b in bounds),max(b[3] for b in bounds)],s)
        # Bin collision components to grid cells, avoiding O(cells * components).
        bins=[[] for _ in g.z]
        for c,b in colliders:
            for y in range(max(0,math.floor((b[1]-g.y0)/g.dy)),min(g.ny,math.ceil((b[3]-g.y0)/g.dy))):
                for x in range(max(0,math.floor((b[0]-g.x0)/g.dx)),min(g.nx,math.ceil((b[2]-g.x0)/g.dx))): bins[y*g.nx+x].append((c,b))
        for i in range(len(g.z)):
            if i%512==0 and progress.should_cancel(): raise RuntimeError('Analysis cancelled')
            x,y=g.xy(i)
            for c,b in bins[i]:
                hit=c.line_trace_component(unreal.Vector(x*100,y*100,(b[5]+10)*100),
                    unreal.Vector(x*100,y*100,(b[4]-10)*100),True,False,False)
                if hit is None: continue
                location,normal,_,_=hit
                if g.z[i] is None or location.z/100>g.z[i]:
                    g.z[i]=location.z/100
                    if abs(normal.z)>1e-8: g.gradients[i]=(-normal.x/normal.z,-normal.y/normal.z)
        return g,landscapes,'Landscape collision heightfield','loaded Landscape collision; four-neighbour grid',[]
    meshes=[c for a in actors for c in a.get_components_by_class(unreal.ProceduralMeshComponent) if c.get_name()==s.terrain_component]
    if not meshes: raise RuntimeError(f'No loaded Landscape or named procedural component {s.terrain_component}')
    if len(meshes)>1: raise RuntimeError('Multiple matching terrain components; select an unambiguous source name')
    c=meshes[0]; vertices,indices=_mesh(c,s.terrain_section)
    if not indices: raise RuntimeError('Terrain section empty; build/generate terrain before auditing')
    used=[vertices[i] for i in set(indices)]
    g=core.Grid.covering([min(v[0] for v in used),min(v[1] for v in used),max(v[0] for v in used),max(v[1] for v in used)],s)
    core.rasterize(g,vertices,indices)
    water=[]
    if s.water_section!=s.terrain_section and c.get_num_sections()>s.water_section:
        wv,wi=_mesh(c,s.water_section)
        if wi:
            core.rasterize(g,wv,wi,water=True)
            water=[dict(source=c.get_path_name(),section=s.water_section,triangles=len(wi)//3)]
    return g,[c.get_owner()],'ProceduralMesh:'+c.get_name()+':'+str(s.terrain_section),'world-space triangle centre sampling',water


def _water_bodies(g,actors,progress):
    cls=getattr(unreal,'WaterBody',None)
    bodies=[a for a in actors if cls and isinstance(a,cls)]
    evidence=[]
    for a in bodies:
        c=a.get_water_body_component()
        colliders=[]
        for component in c.get_collision_components(True):
            origin,extent,_=unreal.SystemLibrary.get_component_bounds(component)
            colliders.append((component,origin,extent))
        if not colliders:
            evidence.append(dict(source=a.get_path_name(),status='UNKNOWN',reason='No enabled water collision footprint; nearest surface alone is not a wet mask'))
            continue
        failures=0; covered=0
        for i,z in enumerate(g.z):
            if z is None: continue
            if i%512==0 and progress.should_cancel(): raise RuntimeError('Analysis cancelled')
            x,y=g.xy(i); x*=100; y*=100
            inside=False
            for collider,o,e in colliders:
                if abs(x-o.x)>e.x or abs(y-o.y)>e.y: continue
                hit=collider.line_trace_component(unreal.Vector(x,y,o.z+e.z+100),
                    unreal.Vector(x,y,o.z-e.z-100),False,False,False)
                if hit is not None: inside=True; break
            if not inside: continue
            covered+=1
            hit=c.get_water_surface_info_at_location(unreal.Vector(x,y,z*100),True)
            if hit is None: failures+=1; continue
            level=hit[0].z/100
            g.water_z[i]=level if g.water_z[i] is None else max(g.water_z[i],level)
        evidence.append(dict(source=a.get_path_name(),status='SAMPLED_APPROXIMATE' if covered and not failures else 'UNKNOWN',
            failures=failures,footprint_samples=covered,
            caveat='Enabled WaterBody collision footprint plus surface query; collision approximation may differ from rendered shoreline'))
    return evidence


def _world_partition(world):
    try: return world.get_world_settings().get_editor_property('world_partition') is not None
    except Exception: return 'UNKNOWN (loaded actors only)'

def _navigation(world,g,actors,s,progress):
    navdata=[a for a in actors if isinstance(a,unreal.NavigationData)]
    if not navdata: return dict(status='UNKNOWN',reason='No loaded NavigationData',agent='default')
    nav=unreal.NavigationSystemV1
    if nav.is_navigation_being_built_or_locked(world): return dict(status='UNKNOWN',reason='Navigation building or locked',agent='default')
    projected={}; queries=0
    for i,z in enumerate(g.z):
        if z is None: continue
        queries+=1
        if queries>s.max_nav_queries: raise RuntimeError('Nav query budget exceeded; increase step or max_nav_queries')
        if i%512==0 and progress.should_cancel(): raise RuntimeError('Analysis cancelled')
        x,y=g.xy(i)
        p=nav.project_point_to_navigation(world,unreal.Vector(x*100,y*100,z*100),
            nav_data=None,filter_class=None,query_extent=unreal.Vector(s.nav_extent_xy_m*100,s.nav_extent_xy_m*100,s.nav_extent_z_m*100))
        g.nav[i]=p is not None
        if p is not None: projected[i]=p
    # An empty/stale mesh is not evidence that the world is 0% reachable.
    if not projected:
        g.nav=[None]*len(g.z)
        return dict(status='UNKNOWN',reason='No successful NavMesh projection; empty, stale or out of coverage',queries=queries,agent='default')
    for i,p in projected.items():
        for j in g.neighbours(i):
            if j<=i or j not in projected: continue
            queries+=1
            if queries>s.max_nav_queries: raise RuntimeError('Nav query budget exceeded; increase step or max_nav_queries')
            # UE 5.8.2 live probe: Vector when obstructed, None when clear.
            result=nav.navigation_raycast(world,p,projected[j])
            blocked=result is not None
            if not blocked: g.nav_edges.add((i,j))
    return dict(status='SAMPLED',queries=queries,projected=len(projected),edges=len(g.nav_edges),agent='default',
        caveat='Projected samples and clear adjacent nav raycasts only; excludes unsampled detours and off-mesh links')


def analyze():
    global _LAST,_WORLD
    clear_visualization(); _LAST=None; _WORLD=None; load_settings(); s=_SETTINGS.validate(); started=time.perf_counter()
    world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    if world is None: raise RuntimeError('Editor world required')
    actors=list(unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors())
    with unreal.ScopedSlowTask(4,'ANASTASIS Map Intelligence: read-only analysis') as progress:
        progress.make_dialog(True)
        progress.enter_progress_frame(1,'Sample loaded terrain')
        g,targets,source,sampling,water=_get_grid(actors,s,progress)
        before=_fingerprint(targets)
        progress.enter_progress_frame(1,'Read water and navigation')
        water+=_water_bodies(g,actors,progress)
        navigation=_navigation(world,g,actors,s,progress)
        if progress.should_cancel(): raise RuntimeError('Analysis cancelled')
        progress.enter_progress_frame(1,'Regions, suitability and corridors')
        provenance=dict(map=world.get_path_name(),project_dir=unreal.Paths.project_dir(),
            engine=unreal.SystemLibrary.get_engine_version(),source=source,sampling=sampling,
            coverage='loaded_only',loaded_actor_count=len(actors),targets=before,
            water=water or [dict(status='ABSENT_IN_LOADED_SOURCES')],navigation=navigation,
            pcg_class_available=hasattr(unreal,'PCGComponent'),water_class_available=hasattr(unreal,'WaterBody'),
            world_partition=_world_partition(world),centimetres_per_metre=100,
            coordinate_axes='Unreal world XY; no assumed geographic north')
        report=core.analyze(g,s,provenance)
        if any(w.get('status')=='UNKNOWN' for w in water):
            report['limitations'].append('Some loaded WaterBodies could not be sampled; dry suitability and water distances are conditional on incomplete water evidence.')
        if before!=_fingerprint(targets): raise RuntimeError('Terrain transform changed during audit; discard report')
        progress.enter_progress_frame(1,'Export read-only measurements')
        report['total_seconds']=time.perf_counter()-started
        core.export(report,output_dir(),'Latest',overwrite=True)
        _LAST=report; _WORLD=world
    unreal.log('MAP_INTELLIGENCE::COMPLETE '+json.dumps(report['statistics'])+f" total_seconds={report['total_seconds']:.3f}")
    return report


def export_report():
    if _LAST is None: raise RuntimeError('Analyze first')
    return core.export(_LAST,output_dir(),'Report_'+time.strftime('%Y%m%d_%H%M%S'))


def save_snapshot(name):
    if _LAST is None: raise RuntimeError('Analyze first')
    return core.export(_LAST,output_dir(),name)


def compare_snapshots(before='MapAudit_Baseline',after='MapAudit_HumanGeographyV2'):
    # Validate names through a whitelist, never accept arbitrary path fragments.
    import re
    if not all(re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9_.-]{0,100}',n) for n in (before,after)): raise ValueError('Invalid snapshot name')
    load=lambda n:json.loads((output_dir()/(n+'.json')).read_text(encoding='utf-8'))
    report=core.compare(load(before),load(after))
    paths=core.export(report,output_dir(),'Comparison_'+time.strftime('%Y%m%d_%H%M%S'))
    unreal.log('MAP_INTELLIGENCE::COMPARISON '+str(paths))
    return report


def clear_visualization():
    global _TICK,_DRAW
    if _TICK is not None:
        unreal.unregister_slate_post_tick_callback(_TICK); _TICK=None
    _DRAW=[]
    # Draw calls below use duration=0.25 s; remaining points expire without global FlushPersistentDebugLines.
    unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_invalidate_viewports()


def show(mode):
    global _TICK,_DRAW
    if _LAST is None: raise RuntimeError('Analyze first')
    clear_visualization()
    g=_LAST['grid']; c=_LAST['cells']; n=len(c['elevation_m'])
    def point(i): return unreal.Vector((g['x0']+(i%g['nx']+.5)*g['dx'])*100,
        (g['y0']+(i//g['nx']+.5)*g['dy'])*100, c['elevation_m'][i]*100+20)
    palette={'PRIME_SETTLEMENT':(.1,1,.2),'AGRICULTURAL_CANDIDATE':(.6,.9,.1),
        'PASTURE_SECONDARY':(.9,.8,.1),'GENERAL_TRAVERSABLE':(1,.5,.1),'DIFFICULT':(1,.2,.1),
        'SEVERE':(.6,.1,.5),'WATER_OR_MARGIN':(.1,.5,1),'UNKNOWN':(.4,.4,.4)}
    budget=_SETTINGS.max_draw_cells
    if mode=='candidates':
        entries=[(x['representative_cell'],(.1,1,.2),18.) for x in _LAST['candidates']]
    elif mode=='corridors':
        indices=list(dict.fromkeys(i for p in _LAST['corridors'] for i in p['path_cells']))
        entries=[(i,(1,.15,.8),7.) for i in indices[::max(1,math.ceil(len(indices)/budget))]]
    else:
        entries=[]
        for i in range(0,n,max(1,math.ceil(n/budget))):
            if c['elevation_m'][i] is None: continue
            if mode=='habitability': color=palette[c['category'][i]]
            elif mode=='agriculture': color=(.4,1,.1) if c['agriculture'][i] else (.3,.3,.3)
            elif mode=='navigation': color=(.1,1,.3) if c['navigable'][i] is True else ((1,.2,.1) if c['navigable'][i] is False else (.4,.4,.4))
            elif mode=='connectivity':
                label=c['terrain_component'][i]
                color=((label*.618)%1,(label*.371+.3)%1,(label*.173+.5)%1) if label>=0 else (.3,.3,.3)
            elif mode in ('slope','roughness'):
                value=c['slope_deg' if mode=='slope' else 'roughness_m'][i]
                t=min(1.,value/(45 if mode=='slope' else max(_SETTINGS.agriculture_roughness_m,.01))) if value is not None else .5
                color=(t,1-t,.1)
            else: raise ValueError('Unknown visualization mode')
            entries.append((i,color,6.))
    _DRAW=[(point(i),unreal.LinearColor(*rgb,1.),size) for i,rgb,size in entries[:budget] if c['elevation_m'][i] is not None]
    last_draw=[0.]
    def tick(_dt):
        current=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
        if current!=_WORLD: clear_visualization(); return
        now=time.monotonic()
        if now-last_draw[0]<.2: return
        last_draw[0]=now
        for p,color,size in _DRAW:
            unreal.SystemLibrary.draw_debug_point(_WORLD,p,size,color,.25,unreal.DrawDebugSceneDepthPriorityGroup.FOREGROUND)
        unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_invalidate_viewports()
    _TICK=unreal.register_slate_post_tick_callback(tick)
    unreal.log(f'MAP_INTELLIGENCE::SHOW {mode} points={len(_DRAW)} (bounded display; all measurements retained)')


def register():
    menus=unreal.ToolMenus.get()
    if menus.find_menu('LevelEditor.MainMenu.Tools') is None: return False
    menus.remove_menu(_MENU)
    menu=menus.extend_menu('LevelEditor.MainMenu.Tools').add_sub_menu('AnastasisMapIntelligence','Tools',
        'AnastasisMapIntelligence','ANASTASIS Map Intelligence','Read-only terrain intelligence')
    commands=[('Analyze loaded terrain','analyze()'),('Edit settings JSON','edit_settings()'),
        ('Show slope',"show('slope')"),('Show roughness',"show('roughness')"),
        ('Show habitability',"show('habitability')"),('Show agriculture',"show('agriculture')"),
        ('Show navigation',"show('navigation')"),('Show terrain connectivity',"show('connectivity')"),
        ('Show settlement candidates',"show('candidates')"),('Show corridors',"show('corridors')"),
        ('Export report','export_report()'),('Save Baseline',"save_snapshot('MapAudit_Baseline')"),
        ('Save HumanGeographyV2',"save_snapshot('MapAudit_HumanGeographyV2')"),
        ('Compare Baseline / V2','compare_snapshots()'),('Clear visualization','clear_visualization()')]
    for i,(label,command) in enumerate(commands):
        entry=unreal.ToolMenuEntry(name=f'MapIntelligence_{i}',type=unreal.MultiBlockType.MENU_ENTRY)
        entry.set_label(label)
        entry.set_string_command(unreal.ToolMenuStringCommandType.PYTHON,'',
            'from anastasis_map_intelligence import editor as ami; ami.'+command)
        menu.add_menu_entry('Analysis',entry)
    menus.refresh_all_widgets()
    return True
