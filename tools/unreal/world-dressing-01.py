"""WORLD DRESSING 01: authored places on inspected Human Geography V2, seed 12345.
Run in an isolated UE editor with -ExecCmds="py <this file>".
WD01_OUT is required. WD01_SAVE=1 bakes only after visual review. Default is preview.
No terrain, simulation, forest grammar, mesh source, or presentation registry edits.
"""
import os, json, math, random, time, traceback, hashlib
import unreal
OUT=os.environ['WD01_OUT']
SAVE=os.environ.get('WD01_SAVE','0')=='1'
VERIFY=os.environ.get('WD01_VERIFY','0')=='1'
TAG='WorldDressing01'
# Local openings measured against the delivered macro forest; values are source tiles (20m).
SITE_OFFSETS={'01_Les_Trois_Veilleurs':(.25,-1.0),'03_La_Memoire_de_Pierre':(1.5,-2.875)}
ues=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
les=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
eas=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
world=ues.get_editor_world()
root=os.path.normcase(os.path.abspath(unreal.Paths.project_dir()))
if root==os.path.normcase(r'C:\dev\ANASTASIS_UNREAL') and not VERIFY:
    raise RuntimeError('Composition belongs in an isolated worktree, never the integration root.')
if not world.get_path_name().startswith('/Game/Anastasis/Maps/Lvl_AnastasisSlice.'):
    raise RuntimeError('Open Lvl_AnastasisSlice first.')
os.makedirs(OUT,exist_ok=True)
def dump(n,v):
    with open(os.path.join(OUT,n),'w',encoding='utf-8') as f:json.dump(v,f,indent=2)
def vec(v):return [v.x,v.y,v.z]
def cmd(s):unreal.SystemLibrary.execute_console_command(world,s)
actor=next(a for a in eas.get_all_level_actors() if a.get_class().get_name()=='AnastasisWorldEmbodiment')
comp=actor.get_components_by_class(unreal.ProceduralMeshComponent)[0]
sections=[]
for section in (0,1):
    v,t,n,u,ta=unreal.ProceduralMeshLibrary.get_section_from_procedural_mesh(comp,section)
    tr=comp.get_world_transform()
    sections.append([vec(unreal.MathLibrary.transform_location(tr,p)) for p in v])
ground,water=sections;N=round(math.sqrt(len(ground)));STEP=ground[1][0]-ground[0][0]
if N!=381 or abs(STEP-500)>0.01:raise RuntimeError('Requires inspected full V2 at Scale=5 (381 grid, 500cm spacing).')
terrain_hash=hashlib.sha256(json.dumps(sections).encode()).hexdigest()
def sample(x,y,field=ground):
    u=(x-ground[0][0])/STEP;v=(y-ground[0][1])/STEP
    ix=math.floor(u);iy=math.floor(v);fx=u-ix;fy=v-iy
    if not (0<=ix<N-1 and 0<=iy<N-1):raise RuntimeError('Outside rendered surface')
    a=iy*N+ix;b=a+1;c=a+N;d=c+1
    if fx+fy<=1:return field[a][2]*(1-fx-fy)+field[b][2]*fx+field[c][2]*fy
    return field[d][2]*(fx+fy-1)+field[b][2]*(1-fy)+field[c][2]*(1-fx)
# Fingerprint of the inspected V2 control points, fail instead of placing on a new seed/relief.
for x,y,z in [(42,33,1600),(49,56,620.04),(40,34,1984.0)]:
    if abs(sample(x*2000,y*2000)-z)>3:raise RuntimeError('Terrain differs from inspected V2 at '+str((x,y)))
assets={}
def mesh(name):
    if name not in assets:
        folder='Architecture' if name=='SM_Ruin_Generic_01' else ('Ecotone' if 'Reed' in name else 'Vegetation')
        assets[name]=unreal.load_asset('/Game/Anastasis/'+folder+'/'+name)
        if not assets[name]:raise RuntimeError('Missing existing asset '+name)
    return assets[name]
placements=[];rejected=[];rng=random.Random(290901)
def place(site,name,x,y,height,yaw=None,wide=1.0,sink=0.0,wet=False):
    dx,dy=SITE_OFFSETS.get(site,(0,0));x+=dx;y+=dy
    X=x*2000;Y=y*2000;Z=sample(X,Y);W=sample(X,Y,water)
    # Wet reeds may stand in <=8cm water; woody roots and masonry remain dry.
    if Z<W+(-8 if wet else 20):rejected.append([site,x,y,'water']);return
    m=mesh(name);b=m.get_bounding_box();s=height/(b.max.z-b.min.z)
    support=22 if wet else (height*.12*wide if 'Tree' in name else max(b.max.x-b.min.x,b.max.y-b.min.y)*s*wide*.5)
    ring=[sample(X+support*math.cos(a*math.pi/4),Y+support*math.sin(a*math.pi/4)) for a in range(8)]
    if max(ring)-min(ring)>(30 if wet else (150 if 'Ruin' in name else 180)):rejected.append([site,x,y,'support']);return
    sink=max(sink,Z-min(ring)+3)
    placements.append(dict(site=site,mesh=name,location=[X,Y,Z-b.min.z*s-sink],scale=[s*wide,s*wide,s],yaw=rng.uniform(0,360) if yaw is None else yaw,root=[X,Y,Z],water=W,sink=sink))
def tree(site,family,stature,x,y,metres,yaw=None,wide=.72):
    place(site,'SM_Tree_'+family+'_'+stature+'_01',x,y,metres*100,yaw,wide)
def reeds(site,cx,cy,count,rx,ry):
    for i in range(count):
        a=rng.random()*math.tau;r=math.sqrt(rng.random())
        place(site,'SM_Ecotone_Reed_01',cx+math.cos(a)*r*rx,cy+math.sin(a)*r*ry,rng.uniform(125,205),wide=rng.uniform(.8,1.25),wet=True)
if not VERIFY:
    # Three asymmetrical skyline markers on ONE shoulder; the passage stays 40m wide.
    for x,y,h,yaw in [(40.1,34.3,26,11),(41.15,35.30,22,137),(40.2,35.65,29,244)]:
        tree('01_Les_Trois_Veilleurs','Conifer','Emergent',x,y,h,yaw)
    for x,y,h in [(39.8,34.9,11),(39.6,35.4,8),(40.7,35.8,13),(40.9,35.5,5),(39.8,34.0,4)]:
        tree('01_Les_Trois_Veilleurs','Conifer','Subcanopy' if h>7 else 'Understory',x,y,h)
    # A broad crown marks the inside of the bend; reed islands follow the water, with gaps.
    tree('02_Le_Coude_des_Roseaux','Broadleaf','Emergent',49.65,55.85,20,78,.74)
    for x,y,h in [(50.05,56.05,11),(49.2,55.8,7),(49.8,55.35,4),(50.1,55.6,3),(51.0,56.4,8)]:
        tree('02_Le_Coude_des_Roseaux','Broadleaf','Canopy' if h>5 else 'Understory',x,y,h)
    for x,y,c,rx,ry in [(49.95,56.7,26,.12,.055),(50.3,56.85,32,.17,.065),(50.85,57.05,27,.14,.055),(51.6,57.32,23,.10,.06),(49.35,56.55,18,.11,.05)]:
        reeds('02_Le_Coude_des_Roseaux',x,y,c,rx,ry)
    # A broken 7m boundary, not an enclosure/building. Vegetation takes over its inland edge.
    for x,y,h,yaw,wide in [(33.55,66.18,180,24,1.5),(33.64,66.22,145,30,1.4),(33.72,66.28,90,17,1.5),(33.82,66.33,48,52,1.8),(33.64,66.12,35,112,1.3),(33.76,66.18,32,198,1.3)]:
        place('03_La_Memoire_de_Pierre','SM_Ruin_Generic_01',x,y,h,yaw,wide,15)
    tree('03_La_Memoire_de_Pierre','Broadleaf','Emergent',33.30,66.22,18,215,.78)
    for x,y,h in [(33.15,65.9,7),(33.38,65.85,4),(33.65,65.85,2.8),(33.85,66.0,3.5),(33.1,66.4,10)]:
        tree('03_La_Memoire_de_Pierre','Broadleaf','Canopy' if h>5 else 'Understory',x,y,h)
    # Resolve every placement before touching the level. Only own tagged actors can be replaced.
    for a in eas.get_all_level_actors():
        if TAG in [str(t) for t in a.tags]:eas.destroy_actor(a)
    # Reuse the existing flat-colour material through a local instance, no source asset edits.
    matpath='/Game/Anastasis/WorldDressing01/MI_WeatheredStone'
    mat=unreal.load_asset(matpath)
    if not mat:
        mat=unreal.AssetToolsHelpers.get_asset_tools().create_asset('MI_WeatheredStone','/Game/Anastasis/WorldDressing01',unreal.MaterialInstanceConstant,unreal.MaterialInstanceConstantFactoryNew())
    parent=unreal.load_asset('/Game/LevelPrototyping/Materials/M_FlatCol')
    unreal.MaterialEditingLibrary.set_material_instance_parent(mat,parent)
    params=unreal.MaterialEditingLibrary.get_vector_parameter_names(parent)
    if not params:raise RuntimeError('Existing M_FlatCol has no colour parameter')
    for param in params:unreal.MaterialEditingLibrary.set_material_instance_vector_parameter_value(mat,param,unreal.LinearColor(.20,.185,.145,1))
    dump('stone_parameters.json',[str(p) for p in params])
    for i,p in enumerate(placements):
        a=eas.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(*p['location']),unreal.Rotator(pitch=0,yaw=p['yaw'],roll=0))
        a.set_actor_label('WD01_'+p['site']+'_'+str(i).zfill(3));a.set_folder_path('WORLD_DRESSING_01/'+p['site']);a.tags=[TAG]
        c=a.static_mesh_component;c.set_mobility(unreal.ComponentMobility.MOVABLE);c.set_static_mesh(mesh(p['mesh']))
        a.set_actor_scale3d(unreal.Vector(*p['scale']))
        # This pass is presentation only; simulation paths and future occupation remain untouched.
        c.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
        c.set_editor_property('can_ever_affect_navigation',False)
        if p['mesh']=='SM_Ruin_Generic_01':c.set_material(0,mat)
    dump('placements.json',placements);dump('rejected.json',rejected)
else:
    placements=json.load(open(os.path.join(OUT,'placements.json'),encoding='utf-8'))
owned=[a for a in eas.get_all_level_actors() if TAG in [str(t) for t in a.tags]]
if len(owned)!=len(placements):raise RuntimeError('Serialized placement count mismatch')
by_label={a.get_actor_label():a for a in owned}
for i,p in enumerate(placements):
    a=by_label['WD01_'+p['site']+'_'+str(i).zfill(3)]
    if max(abs(x-y) for x,y in zip(vec(a.get_actor_location()),p['location']))>.02:
        raise RuntimeError('Placement location mismatch '+a.get_actor_label())
    if max(abs(x-y) for x,y in zip(vec(a.get_actor_scale3d()),p['scale']))>.0001:
        raise RuntimeError('Placement scale mismatch '+a.get_actor_label())
    if abs(a.get_actor_rotation().pitch)>.001 or abs(a.get_actor_rotation().roll)>.001:
        raise RuntimeError('Unexpected tilted object '+a.get_actor_label())
    if a.static_mesh_component.static_mesh.get_name()!=p['mesh']:
        raise RuntimeError('Placement mesh mismatch '+a.get_actor_label())
if VERIFY:
    previous=json.load(open(os.path.join(OUT,'placement_report.json'),encoding='utf-8'))
    if previous['terrain_sha256']!=terrain_hash:raise RuntimeError('Terrain changed on reopening')

report=dict(project=unreal.Paths.project_dir(),engine=unreal.SystemLibrary.get_engine_version(),count=len(owned),terrain_sha256=terrain_hash,saved=SAVE,reloaded=VERIFY,seed=12345,scale=5,extent_m=1900,site_counts={s:sum(p['site']==s for p in placements) for s in sorted(set(p['site'] for p in placements))},meshes=sorted(set(p['mesh'] for p in placements)),collision='disabled',navigation='unchanged',root_water_violations=sum(p['root'][2]<p['water']-8 for p in placements))
dump('reload_report.json' if VERIFY else 'placement_report.json',report)
shots=[]
def shot(name,xy,target,z=170,tz=170):
    site='01_Les_Trois_Veilleurs' if name.startswith('veilleurs') else ('03_La_Memoire_de_Pierre' if name.startswith('memoire') else '')
    dx,dy=SITE_OFFSETS.get(site,(0,0))
    x,y=(xy[0]+dx)*2000,(xy[1]+dy)*2000;tx,ty=(target[0]+dx)*2000,(target[1]+dy)*2000
    shots.append((name,[x,y,sample(x,y)+z],[tx,ty,sample(tx,ty)+tz]))
shot('veilleurs_ground',(42,33.2),(40.35,34.95),170,850)
shot('veilleurs_air',(43,31),(40.25,35),6500,900)
shot('coude_ground',(49,56),(50,56.3),170,450)
shot('coude_air',(47.5,53),(50,56.5),6000,400)
shot('memoire_ground',(34.05,66.1),(33.55,66.2),170,110)
shot('memoire_air',(35,64.5),(33.55,66.2),3600,400)
shots.append(('overview',[-35000,-55000,178000],[96000,96000,10000]))
dump('cameras.json',shots)
if not VERIFY:
    for a in eas.get_all_level_actors():
        if 'WorldDressing01View' in [str(t) for t in a.tags]:eas.destroy_actor(a)
    for name,loc,target in shots:
        if not name.endswith('_ground'):continue
        rotation=unreal.MathLibrary.find_look_at_rotation(unreal.Vector(*loc),unreal.Vector(*target))
        cam=eas.spawn_actor_from_class(unreal.CameraActor,unreal.Vector(*loc),rotation)
        cam.set_actor_label('WD01_VIEW_'+name);cam.set_folder_path('WORLD_DRESSING_01/Views')
        cam.tags=['WorldDressing01View'];cam.set_editor_property('is_editor_only_actor',True)
    if SAVE:
        loc,target=shots[1][1:]
        ues.set_level_viewport_camera_info(unreal.Vector(*loc),unreal.MathLibrary.find_look_at_rotation(unreal.Vector(*loc),unreal.Vector(*target)))
        unreal.EditorAssetLibrary.save_loaded_asset(mat)
        if not les.save_current_level():raise RuntimeError('Map save failed')

cmd('viewmode lit');cmd('ShowFlag.Sprites 0');cmd('ShowFlag.Grid 0')
idx=0;mark=time.monotonic();requested=False;handle=None;attempts=0
prefix='reload_' if VERIFY else ('baked_' if SAVE else 'preview_')
def tick(dt):
    global idx,mark,requested,attempts
    try:
        if idx>=len(shots):
            unreal.unregister_slate_post_tick_callback(handle)
            unreal.log('WORLD_DRESSING_01_COMPLETE '+json.dumps(report))
            if VERIFY and os.environ.get('WD01_TESTS','0')=='1':
                cmd('Automation RunTests Anastasis.Terrain;Quit')
            else:unreal.SystemLibrary.quit_editor()
            return
        name,loc,target=shots[idx];les.editor_invalidate_viewports()
        if not requested:
            ues.set_level_viewport_camera_info(unreal.Vector(*loc),unreal.MathLibrary.find_look_at_rotation(unreal.Vector(*loc),unreal.Vector(*target)))
            if time.monotonic()-mark>7:
                cmd('HighResShot 1600x900 filename="'+OUT+'/'+prefix+name+'.png"');requested=True;mark=time.monotonic()
        elif time.monotonic()-mark>5:
            if not os.path.isfile(OUT+'/'+prefix+name+'.png'):
                attempts+=1
                if attempts>=5:raise RuntimeError('Screenshot missing after retries '+name)
                requested=False;mark=time.monotonic()
                return
            idx+=1;requested=False;mark=time.monotonic();attempts=0
    except Exception:
        unreal.log_error(traceback.format_exc());unreal.unregister_slate_post_tick_callback(handle);unreal.SystemLibrary.quit_editor()
handle=unreal.register_slate_post_tick_callback(tick)
