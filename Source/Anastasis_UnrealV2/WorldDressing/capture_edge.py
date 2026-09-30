"""Bounded current-main forest-edge A/B. Uses existing exclusions; never saves map/assets.
Launched by launch_preview.ps1 -Edge. Fixed seed, world, geometry and cameras across OFF/ON.
"""
import os, time, json, math, hashlib, traceback
import unreal
from anastasis_toolset.toolsets.inspect import AnastasisInspectTools
OUT=os.environ['ANASTASIS_DRESSING_OUT']
os.makedirs(OUT,exist_ok=True)
les=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ues=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
eas=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
handle=None
results={}

def save(name,obj):
    with open(os.path.join(OUT,name),'w',encoding='utf8') as f: json.dump(obj,f,indent=2)
def vec(v): return [v.x,v.y,v.z]
def digest(obj): return hashlib.sha256(json.dumps(obj,separators=(',',':'),sort_keys=True).encode()).hexdigest()
def finish(error=None):
    if error: results['error']=str(error)
    save('observation.json',results)
    (unreal.log_error if error else unreal.log)('WORLD_DRESSING_EDGE_'+('FAIL' if error else 'COMPLETE')+' '+json.dumps(results))
    if handle is not None: unreal.unregister_slate_post_tick_callback(handle)
    if os.environ.get('ANASTASIS_DRESSING_KEEP_OPEN')!='1': unreal.SystemLibrary.quit_editor()
def inventory(actor):
    rows=[]
    for c in actor.get_components_by_class(unreal.HierarchicalInstancedStaticMeshComponent):
        m=c.get_editor_property('static_mesh')
        for i in range(c.get_instance_count()):
            t=c.get_instance_transform(i,world_space=True)
            rows.append([m.get_path_name() if m else '',*[round(v,6) for v in (*vec(t.translation),t.rotation.x,t.rotation.y,t.rotation.z,t.rotation.w,*vec(t.scale3d))]])
    return sorted(rows)
def geometry(component,section):
    v,i,*_=unreal.ProceduralMeshLibrary.get_section_from_procedural_mesh(component,section)
    t=component.get_world_transform()
    return [vec(unreal.MathLibrary.transform_location(t,p)) for p in v],list(i)
def index_mesh(vertices,indices):
    bins={}
    for n in range(0,len(indices),3):
        a,b,c=[vertices[indices[n+k]] for k in range(3)]
        for y in range(math.floor(min(a[1],b[1],c[1])/2000),math.floor(max(a[1],b[1],c[1])/2000)+1):
            for x in range(math.floor(min(a[0],b[0],c[0])/2000),math.floor(max(a[0],b[0],c[0])/2000)+1):
                bins.setdefault((x,y),[]).append((a,b,c))
    return bins
def sample(bins,x,y):
    best=None
    for a,b,c in bins.get((math.floor(x/2000),math.floor(y/2000)),[]):
        d=(b[1]-c[1])*(a[0]-c[0])+(c[0]-b[0])*(a[1]-c[1])
        if abs(d)<1e-10: continue
        u=((b[1]-c[1])*(x-c[0])+(c[0]-b[0])*(y-c[1]))/d
        v=((c[1]-a[1])*(x-c[0])+(a[0]-c[0])*(y-c[1]))/d
        if min(u,v,1-u-v)<-1e-7: continue
        z=u*a[2]+v*b[2]+(1-u-v)*c[2]
        ab=[b[j]-a[j] for j in range(3)];ac=[c[j]-a[j] for j in range(3)]
        normal=[ab[1]*ac[2]-ab[2]*ac[1],ab[2]*ac[0]-ab[0]*ac[2],ab[0]*ac[1]-ab[1]*ac[0]]
        slope=math.degrees(math.acos(min(1,abs(normal[2])/math.sqrt(sum(t*t for t in normal)))))
        if best is None or z>best[0]: best=(z,slope)
    return best

try:
    session=dict(AnastasisInspectTools.get_session_snapshot())
    expected=os.path.normcase(os.path.abspath(os.environ['ANASTASIS_DRESSING_PROJECT']))
    assert os.path.normcase(os.path.abspath(session['project_dir']))==expected,session
    assert session['pie']=='false',session
    results['session']=session
    assert les.load_level('/Game/Anastasis/Maps/Lvl_AnastasisSlice')
    world=ues.get_editor_world()
    def cmd(s): unreal.SystemLibrary.execute_console_command(world,s)
    for s in ('viewmode lit','ShowFlag.Sprites 0','ShowFlag.Grid 0','t.MaxFPS 30'):
        cmd(s)
    # Freeze material time for the comparison; no asset/material edits.
    cmd('r.Test.OverrideTimeMaterialExpressions 0')
    cls=unreal.load_class(None,'/Script/Anastasis_UnrealV2.AnastasisWorldEmbodiment')
    sources=list(unreal.GameplayStatics.get_all_actors_of_class(world,cls));assert len(sources)==1
    source=sources[0]
    actual_scale=unreal.SystemLibrary.get_console_variable_float_value('anastasis.WorldView.Scale')
    actual_geography=unreal.SystemLibrary.get_console_variable_int_value('anastasis.Terrain.HumanGeography')
    assert actual_scale==5.0 and actual_geography==1,(actual_scale,actual_geography)
    results['world_scale']=actual_scale;results['human_geography']=actual_geography
    surface=next(c for c in source.get_components_by_class(unreal.ProceduralMeshComponent) if c.get_name()=='ExperimentalTerrain')
    ground_geo=geometry(surface,0);water_geo=geometry(surface,1)
    ground=index_mesh(*ground_geo);water=index_mesh(*water_geo)
    source_before=inventory(source);source_hash=digest(source_before)
    geo_hash=digest([ground_geo,water_geo])
    trees=[r for r in source_before if '/Vegetation/SM_Tree_' in r[0]]
    assert trees,'No existing trees to select a forest edge'
    # Select an existing stand in the interior, with supported dry ground, before any added props.
    bins={}
    for r in trees:
        x,y,z=r[1:4]
        if not (10000<x<180000 and 10000<y<180000): continue
        g=sample(ground,x,y);w=sample(water,x,y)
        if not g or g[1]>25 or (w and g[0]<=w[0]+200): continue
        bins.setdefault((int(x//10000),int(y//10000)),[]).append((x,y,z))
    assert bins,'No eligible dry stand'
    pts=bins[max(sorted(bins),key=lambda k:len(bins[k]))]
    cx=sum(p[0] for p in pts)/len(pts);cy=sum(p[1] for p in pts)/len(pts)
    gz=sample(ground,cx,cy);assert gz
    cz=gz[0]
    region=[cx-4500,cy-4500,cx+4500,cy+4500]
    xmin,ymin,xmax,ymax=region
    # Keep a foreground clearing, plus an explicit small path and building footprint as exclusion probes.
    clearing=[xmin,ymin,cx+500,cy-500]
    road=[cx+1200,ymin,cx+1600,ymax]
    building=[cx-1100,cy+1000,cx-300,cy+1800]
    manager=eas.spawn_actor_from_class(unreal.load_class(None,'/Script/Anastasis_UnrealV2.AnastasisWorldDressingManager'),unreal.Vector(0,0,0))
    manager.set_actor_label('World Dressing V0 - bounded forest edge preview')
    cube=unreal.load_asset('/Engine/BasicShapes/Cube')
    def exclusion(name,box):
        x0,y0,x1,y1=box
        a=eas.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector((x0+x1)/2,(y0+y1)/2,cz))
        a.set_actor_label(name)
        c=a.static_mesh_component;c.set_static_mesh(cube)
        a.set_actor_scale3d(unreal.Vector((x1-x0)/100,(y1-y0)/100,1000))
        c.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION);c.set_visibility(False)
        return a
    outer=[[-1000,-1000,xmin,200000],[xmax,-1000,200000,200000],[xmin,-1000,xmax,ymin],[xmin,ymax,xmax,200000]]
    exclusion_actors=[exclusion('WD_Outside_'+str(i),b) for i,b in enumerate(outer)]
    exclusion_actors += [exclusion('WD_ProtectedClearing',clearing),exclusion('WD_BuildingProbe',building)]
    road_actor=exclusion('WD_RoadProbe',road)
    profile=unreal.new_object(unreal.AnastasisWorldDressingProfile)
    rules=[]
    for name,asset,density,lo,hi,cluster in [
        ('EdgeYoung','SM_Tree_Broadleaf_Understory_01',10.0,1.4,2.4,500.0),
        ('EdgeMid','SM_Tree_Broadleaf_Subcanopy_01',1.0,3.5,5.5,350.0)]:
        r=unreal.AnastasisDressingRule()
        mesh=unreal.load_asset('/Game/Anastasis/Vegetation/'+asset);assert mesh
        for k,v in dict(asset_id=name,static_mesh=mesh,density=density,scale_min=lo,scale_max=hi,
            cluster_radius=cluster,clearance_radius=40.0,slope_max=25.0,distance_to_water_min=200.0,
            allowed_terrain_families=[unreal.AnastasisDressingTerrain.GRASS,unreal.AnastasisDressingTerrain.SCRUB,unreal.AnastasisDressingTerrain.FOREST]).items():
            r.set_editor_property(k,v)
        rules.append(r)
    profile.set_editor_property('rules',rules)
    for k,v in dict(world_source=source,profile=profile,seed=12345,exclusion_padding=0.0,
                   building_exclusions=exclusion_actors,road_exclusions=[road_actor]).items():
        manager.set_editor_property(k,v)
    actor_count=len(eas.get_all_level_actors())
    manager.generate_preview()
    first=inventory(manager);first_hash=manager.get_editor_property('placement_hash')
    material_seeds=sorted((c.get_editor_property('static_mesh').get_path_name(),c.get_editor_property('instancing_random_seed')) for c in manager.get_components_by_class(unreal.HierarchicalInstancedStaticMeshComponent))
    assert first,'No placements: '+str(manager.get_editor_property('placement_report'))
    report=str(manager.get_editor_property('placement_report'))
    manager.rebuild_from_seed()
    assert material_seeds==sorted((c.get_editor_property('static_mesh').get_path_name(),c.get_editor_property('instancing_random_seed')) for c in manager.get_components_by_class(unreal.HierarchicalInstancedStaticMeshComponent)), material_seeds
    results['material_seeds']=material_seeds
    assert inventory(manager)==first and manager.get_editor_property('placement_hash')==first_hash
    assert len(eas.get_all_level_actors())==actor_count
    # Independent validation from actual HISM transforms and exported source triangles.
    errors=[];max_anchor_error=0.0;max_slope=0.0
    for c in manager.get_components_by_class(unreal.HierarchicalInstancedStaticMeshComponent):
        mesh=c.get_editor_property('static_mesh');bottom=mesh.get_bounding_box().min.z
        for i in range(c.get_instance_count()):
            t=c.get_instance_transform(i,world_space=True);x,y= t.translation.x,t.translation.y
            root=t.translation.z+bottom*t.scale3d.z
            g=sample(ground,x,y);w=sample(water,x,y)
            if not g: errors.append('no-ground');continue
            max_anchor_error=max(max_anchor_error,abs(root-g[0]));max_slope=max(max_slope,g[1])
            if not (xmin<x<xmax and ymin<y<ymax):errors.append('outside-region')
            for b in (clearing,road,building):
                if b[0]<=x<=b[2] and b[1]<=y<=b[3]: errors.append('exclusion')
            if w and root<=w[0]+1: errors.append('water')
    assert not errors,errors
    assert max_anchor_error<.1 and max_slope<=25.0001,(max_anchor_error,max_slope)
    assert inventory(source)==source_before
    assert digest([geometry(surface,0),geometry(surface,1)])==geo_hash
    save('placements.json',first)
    results.update(region_cm=region,clearing_cm=clearing,road_probe_cm=road,building_probe_cm=building,
        seed=12345,main_base=os.environ.get('ANASTASIS_DRESSING_BASE'),comparison_key='seed12345_scale5_humangeography1_edge_v0',
        source_hism_hash=source_hash,source_geometry_hash=geo_hash,source_unchanged=True,
        count=len(first),placement_hash=first_hash,independent_hism_hash=digest(first),repeat_equal=True,
        actor_count=actor_count,prop_actors_created=0,exclusion_helpers=len(exclusion_actors)+1,
        max_anchor_error_cm=max_anchor_error,max_slope_degrees=max_slope,invalid_count=len(errors),report=report)
    manager.clear_preview()
    assert inventory(manager)==[]
    assert len(manager.get_components_by_class(unreal.HierarchicalInstancedStaticMeshComponent))==0
    results['clear_pass']=True
    focus=unreal.Vector(cx,cy,cz+100)
    oblique=unreal.Vector(cx-6000,cy-8000,cz+5000)
    near=unreal.Vector(cx-1800,cy-3500,sample(ground,cx-1800,cy-3500)[0]+170)
    views=[('oblique',oblique,unreal.MathLibrary.find_look_at_rotation(oblique,focus)),
           ('ground',near,unreal.MathLibrary.find_look_at_rotation(near,unreal.Vector(cx,cy+1800,cz+180)))]
    results['cameras']=[dict(name=n,location=vec(l),rotation=[r.pitch,r.yaw,r.roll]) for n,l,r in views]
    jobs=[('before',v) for v in views]+[('after',v) for v in views]+[('cleared',views[0])]
    phase=0;mark=time.monotonic();requested=False;setup=False;requested_wall=0
    def tick(dt):
        global phase,mark,requested,setup,requested_wall
        try:
            les.editor_invalidate_viewports()
            if phase>=len(jobs):
                assert inventory(source)==source_before
                assert digest([geometry(surface,0),geometry(surface,1)])==geo_hash
                assert inventory(manager)==[]
                manager.generate_preview();assert inventory(manager)==first
                results['final_source_unchanged']=True
                finish();return
            state,(name,loc,rot)=jobs[phase];elapsed=time.monotonic()-mark
            if elapsed>120: raise RuntimeError('Capture timeout '+state+name)
            if not setup:
                if state=='after':manager.generate_preview()
                else:manager.clear_preview()
                ues.set_level_viewport_camera_info(loc,rot);setup=True;mark=time.monotonic();return
            path=os.path.join(OUT,state+'_'+name+'.png').replace('\\','/')
            if not requested and elapsed>8:
                requested_wall=time.time()
                cmd('HighResShot 1600x900 filename="'+path+'"');requested=True;mark=time.monotonic()
            elif requested and elapsed>5 and os.path.isfile(path) and os.path.getmtime(path)>=requested_wall:
                unreal.log('WORLD_DRESSING_EDGE_SHOT '+path)
                phase+=1;mark=time.monotonic();requested=False;setup=False
        except Exception:finish(traceback.format_exc())
    handle=unreal.register_slate_post_tick_callback(tick)
except Exception:finish(traceback.format_exc())
