"""CREDIBLE_BANKS_001: three sediment pockets on the inspected rendered reed bend.
Preview = unsaved A/B; Save = own tagged actors + own materials + current map;
Verify = reopen and compare manifest. Launch with credible-banks.ps1.
No terrain, water mesh, simulation, existing actors or shared material edits.
"""
import os, math, random, json, hashlib, time, traceback
import unreal
OUT=os.environ['BANKS_OUT'].replace('\\','/')
MODE=os.environ.get('BANKS_MODE','Preview').lower()
TAG='CredibleBanks001'
PKG='/Game/Anastasis/CredibleBanks001'
LEVEL='/Game/Anastasis/Maps/Lvl_AnastasisSlice'
ues=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
eas=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
les=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
world=ues.get_editor_world()
if MODE not in ('preview','save','verify'):raise RuntimeError('Unknown mode')
if not world.get_path_name().startswith(LEVEL+'.'):raise RuntimeError('Wrong level')
if MODE!='verify' and os.path.normcase(os.path.abspath(unreal.Paths.project_dir()))==os.path.normcase(r'C:\dev\ANASTASIS_UNREAL'):raise RuntimeError('Use isolated worktree')
os.makedirs(OUT,exist_ok=True)
def dump(name,value):
    with open(OUT+'/'+name,'w',encoding='utf-8') as f:json.dump(value,f,indent=2)
def cmd(s):unreal.SystemLibrary.execute_console_command(world,s)
def xyz(v):return [v.x,v.y,v.z]
def own(a):return TAG in [str(t) for t in a.tags]
owner=next(a for a in eas.get_all_level_actors() if a.get_class().get_name()=='AnastasisWorldEmbodiment')
comp=owner.get_components_by_class(unreal.ProceduralMeshComponent)[0]
tr=comp.get_world_transform()
def geometry():
    return [[xyz(unreal.MathLibrary.transform_location(tr,v)) for v in unreal.ProceduralMeshLibrary.get_section_from_procedural_mesh(comp,s)[0]] for s in (0,1)]
ground,water=geometry();N=round(math.sqrt(len(ground)));STEP=ground[1][0]-ground[0][0]
if N!=381 or abs(STEP-500)>.01:raise RuntimeError('Requires inspected 381x381 rendered grid')
def fingerprint():return hashlib.sha256(json.dumps(geometry()).encode()).hexdigest()
terrain_hash=fingerprint()
untouched={a.get_path_name():[xyz(a.get_actor_location()),xyz(a.get_actor_scale3d())] for a in eas.get_all_level_actors() if not own(a)}
def sample(x,y,field=ground):
    u=(x-ground[0][0])/STEP;v=(y-ground[0][1])/STEP
    ix=math.floor(u);iy=math.floor(v);fx=u-ix;fy=v-iy
    if not(0<=ix<N-1 and 0<=iy<N-1):raise RuntimeError('Outside rendered grid')
    a=iy*N+ix;b=a+1;c=a+N;d=c+1
    return field[a][2]*(1-fx-fy)+field[b][2]*fx+field[c][2]*fy if fx+fy<=1 else field[d][2]*(fx+fy-1)+field[b][2]*(1-fy)+field[c][2]*(1-fx)
def freeboard(x,y):return sample(x,y)-sample(x,y,water)
def bank_y(x,h):
    # Local SOUTH shore only, not a global scatter or a guess from old water tiles.
    ys=range(110500,114501,10)
    y=min(ys,key=lambda y:abs(freeboard(x,y)-h))
    if abs(freeboard(x,y)-h)>3:raise RuntimeError('Inspected shore no longer found')
    return float(y)
# Stop on a different geographic layout before replacing owned actors.
for x,y in [(98500,bank_y(98500,30)),(100300,bank_y(100300,30)),(101650,bank_y(101650,30))]:
    if not 112000<y<114200:raise RuntimeError('Reference bend moved')
mel=unreal.MaterialEditingLibrary
materials=[];manifest=[];rejected=0

def material_master():
    path=PKG+'/M_BankDeposit'
    mat=unreal.load_asset(path)
    if mat:return mat
    mat=unreal.AssetToolsHelpers.get_asset_tools().create_asset('M_BankDeposit',PKG,unreal.Material,unreal.MaterialFactoryNew())
    mel.delete_all_material_expressions(mat)
    mat.set_editor_properties({'material_domain':unreal.MaterialDomain.MD_DEFERRED_DECAL,'blend_mode':unreal.BlendMode.BLEND_TRANSLUCENT})
    def node(cls,x,y,**props):
        n=mel.create_material_expression(mat,cls,x,y)
        for k,v in props.items():n.set_editor_property(k,v)
        return n
    uv=node(unreal.MaterialExpressionTextureCoordinate,-700,0)
    mask=node(unreal.MaterialExpressionCustom,-420,100,code='''
        float2 p=(UV-0.5)*2.0;
        float2 q=UV*float2(7.0,4.0), i=floor(q), f=frac(q);
        f=f*f*(3.0-2.0*f);
        float a=frac(sin(dot(i,float2(127.1,311.7)))*43758.5453);
        float b=frac(sin(dot(i+float2(1,0),float2(127.1,311.7)))*43758.5453);
        float c=frac(sin(dot(i+float2(0,1),float2(127.1,311.7)))*43758.5453);
        float d=frac(sin(dot(i+float2(1,1),float2(127.1,311.7)))*43758.5453);
        float n=lerp(lerp(a,b,f.x),lerp(c,d,f.x),f.y);
        float e=1.0-smoothstep(0.50,0.99,length(p)+(n-0.5)*0.18);
        return saturate(e*(0.88+0.12*n));
    ''',output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT1)
    inp=unreal.CustomInput();inp.set_editor_property('input_name','UV');mask.set_editor_property('inputs',[inp])
    assert mel.connect_material_expressions(uv,'',mask,'UV')
    strength=node(unreal.MaterialExpressionScalarParameter,-410,280,parameter_name='Opacity',default_value=.75)
    opacity=node(unreal.MaterialExpressionMultiply,-100,140)
    assert mel.connect_material_expressions(mask,'',opacity,'A')
    assert mel.connect_material_expressions(strength,'',opacity,'B')
    # Coverage is wired through Substrate ConvertToDecal below.
    color=node(unreal.MaterialExpressionVectorParameter,-380,-220,parameter_name='Color',default_value=unreal.LinearColor(.15,.12,.08,1))
    # Explicit Substrate graph; legacy inputs alone do not drive a new Substrate material.
    rough=node(unreal.MaterialExpressionScalarParameter,-100,350,parameter_name='Roughness',default_value=.8)
    slab=node(unreal.MaterialExpressionSubstrateShadingModels,180,0)
    convert=node(unreal.MaterialExpressionSubstrateConvertToDecal,420,0)
    assert mel.connect_material_expressions(color,'RGB',slab,'BaseColor')
    assert mel.connect_material_expressions(rough,'',slab,'Roughness')
    assert mel.connect_material_expressions(slab,'',convert,'DecalMaterial')
    assert mel.connect_material_expressions(opacity,'',convert,'Coverage')
    assert mel.connect_material_property(convert,'',unreal.MaterialProperty.MP_FRONT_MATERIAL)
    mat.set_editor_properties({'material_domain':unreal.MaterialDomain.MD_DEFERRED_DECAL,'blend_mode':unreal.BlendMode.BLEND_TRANSLUCENT})
    errors=mel.recompile_material(mat)
    if errors:raise RuntimeError('Decal compile errors: '+str(errors))
    unreal.log('BANKS_MATERIAL '+str(mat.get_editor_property('material_domain'))+' '+str(mat.get_editor_property('blend_mode')))
    return mat

def instance(name,parent,color,rough=None,opacity=None):
    m=unreal.load_asset(PKG+'/'+name)
    if not m:m=unreal.AssetToolsHelpers.get_asset_tools().create_asset(name,PKG,unreal.MaterialInstanceConstant,unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(m,parent)
    mel.set_material_instance_vector_parameter_value(m,'Color',unreal.LinearColor(*color,1))
    if rough is not None:mel.set_material_instance_scalar_parameter_value(m,'Roughness',rough)
    if opacity is not None:mel.set_material_instance_scalar_parameter_value(m,'Opacity',opacity)
    materials.append(m);return m

def tag(a,kind):
    a.tags=[TAG];a.set_actor_label('BANK01_'+kind+'_'+str(len(manifest)).zfill(3));a.set_folder_path('CREDIBLE_BANKS_001')

def decal(kind,x,y,rx,ry,mat,yaw):
    z=sample(x,y)
    a=eas.spawn_actor_from_class(unreal.DecalActor,unreal.Vector(x,y,z+90),unreal.Rotator(pitch=-90,yaw=yaw,roll=0))
    tag(a,kind)
    c=a.get_component_by_class(unreal.DecalComponent)
    c.set_decal_material(mat);c.set_editor_property('decal_size',unreal.Vector(180,ry,rx))
    c.set_editor_property('fade_screen_size',.0002)
    c.set_editor_property('sort_order',1 if kind=='silt' else 0)
    manifest.append({'label':a.get_actor_label(),'kind':kind,'location':xyz(a.get_actor_location()),'scale':xyz(a.get_actor_scale3d()),'material':mat.get_path_name()})

cache={}
def place(kind,name,x,y,length,yaw,sink,material=None,stretch=1.0):
    global rejected
    z=sample(x,y);w=sample(x,y,water)
    if z-w<4 or z-w>155:rejected+=1;return
    path='/Game/Anastasis/'+('Rock/' if name.startswith('SM_Rock') else 'Ecotone/')+name
    if path not in cache:cache[path]=unreal.load_asset(path)
    mesh=cache[path]
    if not mesh:raise RuntimeError('Missing '+path)
    b=mesh.get_bounding_box();span=max(b.max.x-b.min.x,b.max.y-b.min.y)
    s=length/span;sz=s if kind not in ('tuft','grass') else length/(b.max.z-b.min.z)
    if kind in ('tuft','grass'):s=sz*1.7
    # Bottom of the complete rotated bounds is supported, with partial burial.
    rad=math.radians(yaw);points=[]
    for bx in (b.min.x,b.max.x):
        for by in (b.min.y,b.max.y):
            X=x+(bx*math.cos(rad)-by*math.sin(rad))*s
            Y=y+(bx*math.sin(rad)+by*math.cos(rad))*s
            points.append(sample(X,Y))
    base=min(points)-b.min.z*sz-sink
    if max(points)-min(points)>(max(25,(b.max.z-b.min.z)*sz*.65)):rejected+=1;return
    a=eas.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(x,y,base),unreal.Rotator(pitch=0,yaw=yaw,roll=0))
    tag(a,kind);a.static_mesh_component.set_static_mesh(mesh);a.set_actor_scale3d(unreal.Vector(s,s,sz*stretch));a.set_actor_enable_collision(False)
    if material:a.static_mesh_component.set_material(0,material)
    manifest.append({'label':a.get_actor_label(),'kind':kind,'mesh':path,'location':xyz(a.get_actor_location()),'scale':xyz(a.get_actor_scale3d()),'freeboard_cm':z-w,'sink_cm':sink})

if MODE!='verify':
    master=material_master();materials.append(master)
    silt=instance('MI_WetSilt',master,(.105,.079,.049),.57,.86)
    gravel=instance('MI_GravelBed',master,(.18,.157,.112),.92,.88)
    stoneparent=unreal.load_asset('/Game/LevelPrototyping/Materials/M_FlatCol')
    stones=[instance('MI_BankStone_'+str(i),stoneparent,c) for i,c in enumerate([(.12,.115,.098),(.19,.176,.142),(.085,.08,.069)])]
    for a in eas.get_all_level_actors():
        if own(a):eas.destroy_actor(a)
    rng=random.Random(300930)
    # Three pockets, asymmetric lengths and dry gaps; ~45 metres of shore only.
    for index,(cx,rx) in enumerate([(98600,620),(100250,920),(101850,550)]):
        sy=bank_y(cx,20);gy=bank_y(cx,57)
        decal('silt',cx,sy,rx,330,silt,10+index*4)
        decal('silt',cx+rx*.38,bank_y(cx+rx*.38,13),rx*.46,190,silt,7)
        decal('gravel',cx-90,gy,rx*.90,580,gravel,12)
        # Coarse fragments on the upper bar; finer pebbles toward the wet edge.
        for i in range(62 if index==1 else 42):
            dx=rng.gauss(0,rx*.45);dx=max(-rx,min(rx,dx));x=cx+dx
            h=rng.uniform(15,85);y=bank_y(x,h)+rng.uniform(-60,60)
            size=rng.uniform(9,27) if h<40 else rng.uniform(18,48)
            place('gravel','SM_Rock_Low_'+str(1+i%3).zfill(2),x,y,size,rng.uniform(0,360),rng.uniform(.8,2.6),stones[i%3])
        for i in range(5):
            x=cx+rng.uniform(-rx*.6,rx*.6);y=bank_y(x,rng.uniform(35,75))
            place('anchor','SM_Rock_Low_'+str(1+i%3).zfill(2),x,y,rng.uniform(40,75),rng.uniform(0,360),rng.uniform(3,5),stones[i%3])
        for i in range(22 if index==1 else 13):
            # Patches above sediment, never an evenly spaced hedge.
            x=cx+rng.choice([-.65,.40])*rx+rng.gauss(0,100);y=bank_y(x,rng.uniform(85,135))+rng.gauss(0,55)
            place('tuft','SM_Ecotone_ShoreTuft_01',x,y,rng.uniform(35,65),rng.uniform(0,360),2)
        x=cx+rx*.45;y=bank_y(x,65)
        place('driftwood','SM_Ecotone_Driftwood_01',x,y,165 if index!=1 else 220,12+rng.uniform(-12,12),6)
    dump('placements.json',manifest)
else:
    with open(OUT+'/placements.json',encoding='utf-8') as f:manifest=json.load(f)
owned=[a for a in eas.get_all_level_actors() if own(a)]
bylabel={a.get_actor_label():a for a in owned}
if len(owned)!=len(manifest):raise RuntimeError('Owned actor count mismatch')
for p in manifest:
    a=bylabel[p['label']]
    if max(abs(v-w) for v,w in zip(xyz(a.get_actor_location()),p['location']))>.03:raise RuntimeError('Reload position mismatch')
    if max(abs(v-w) for v,w in zip(xyz(a.get_actor_scale3d()),p['scale']))>.0001:raise RuntimeError('Reload scale mismatch')
    if 'mesh' in p and a.static_mesh_component.static_mesh.get_path_name().split('.')[0]!=p['mesh']:raise RuntimeError('Reload mesh mismatch')
if terrain_hash!=fingerprint():raise RuntimeError('Terrain changed')
for a in eas.get_all_level_actors():
    if not own(a) and a.get_path_name() in untouched:
        if [xyz(a.get_actor_location()),xyz(a.get_actor_scale3d())]!=untouched[a.get_path_name()]:raise RuntimeError('Unrelated actor moved')
report={'mode':MODE,'project':unreal.Paths.project_dir(),'terrain_sha256':terrain_hash,'actors':len(owned),'kinds':{k:sum(p['kind']==k for p in manifest) for k in sorted(set(p['kind'] for p in manifest))},'rejected':rejected,'freeboard_min':min(p['freeboard_cm'] for p in manifest if 'freeboard_cm' in p),'terrain_unchanged':True,'unrelated_transforms_unchanged':True,'new_collision':False}
if MODE=='verify':
    saved=json.load(open(OUT+'/save_report.json'))
    if saved['terrain_sha256']!=terrain_hash:raise RuntimeError('Terrain differs from save')
if MODE=='save':
    for m in materials:
        if not unreal.EditorAssetLibrary.save_loaded_asset(m):raise RuntimeError('Material save failed')
    if not les.save_current_level():raise RuntimeError('Level save failed')
dump(MODE+'_report.json',report)
shots=[('ground',[98000,112000,sample(98000,112000)+170],[100000,113200,sample(100000,113200)+35]),('detail',[100250,112350,sample(100250,112350)+170],[100250,113300,sample(100250,113300)+12]),('oblique',[97800,109900,3900],[100000,113000,500])]
dump('cameras.json',shots)
cmd('viewmode lit');cmd('ShowFlag.Sprites 0');cmd('ShowFlag.Grid 0');cmd('ShowFlag.CompositeEditorPrimitives 0')
phases=['off','on'] if MODE=='preview' else ['on']
queue=[(phase,shot) for phase in phases for shot in shots]
idx=0;mark=time.monotonic();requested=False;handle=None

def tick(dt):
    global idx,mark,requested
    try:
        if idx>=len(queue):
            unreal.unregister_slate_post_tick_callback(handle);unreal.log('BANKS_COMPLETE '+json.dumps(report));unreal.SystemLibrary.quit_editor();return
        phase,(name,loc,target)=queue[idx]
        les.editor_invalidate_viewports()
        if not requested:
            for a in owned:a.set_is_temporarily_hidden_in_editor(phase=='off')
            ues.set_level_viewport_camera_info(unreal.Vector(*loc),unreal.MathLibrary.find_look_at_rotation(unreal.Vector(*loc),unreal.Vector(*target)))
            if time.monotonic()-mark>8:
                cmd('HighResShot 1280x720 filename="'+OUT+'/'+MODE+'_'+phase+'_'+name+'.png"');requested=True;mark=time.monotonic()
        elif os.path.isfile(OUT+'/'+MODE+'_'+phase+'_'+name+'.png') and time.monotonic()-mark>3:
            idx+=1;requested=False;mark=time.monotonic()
        elif time.monotonic()-mark>60:raise RuntimeError('Missing screenshot '+name)
    except Exception:
        unreal.log_error('BANKS_FAIL '+traceback.format_exc());unreal.unregister_slate_post_tick_callback(handle);unreal.SystemLibrary.quit_editor()
handle=unreal.register_slate_post_tick_callback(tick)
