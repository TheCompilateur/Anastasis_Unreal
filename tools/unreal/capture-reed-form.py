"""Reed form/material comparison. A existing; B no WPO; C curved mesh.
Same roots/cameras/light. New review assets only; no map or original asset save.
"""
import bisect
import hashlib
import json
import math
import os
import random
import time
import traceback
import unreal

OUT = os.environ['ANASTASIS_REED_FORM_OUT']
os.makedirs(OUT, exist_ok=True)
SEED = 12345
BASE = '82a337b' # delivered 4 m terrain; no terrain edits by this study
MESH_PATH = '/Game/Anastasis/Ecotone/SM_Ecotone_Reed_01'
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
manifest = dict(base=BASE, seed=SEED, source_size=[96,96], tile_world_size_uu=400, saved_assets=False,
                mode='editor; PLAYER NOT_IMPLEMENTED', mesh=MESH_PATH,
                mesh_source='fb683983c22c381bf00390d312634b419f585e24',
                comparison='same roots/yaw/scale; A original; B original mesh without WPO; C curved mesh and B material',
                reference='docs/visual/reference/pontique-etat-zero-5-lisieres-transitions.png',
                material_wpo='A existing wind; B/C zero WPO; leaf/stem/panicle geometry and colors change in C',
                sun_lux=75000, exposure_ev100=14, views=[], placements=[])
spawned = []
handle = None


def log(s):
    unreal.log('REED_REVIEW ' + str(s))


def cmd(s):
    unreal.SystemLibrary.execute_console_command(ues.get_editor_world(), s)


def write_manifest():
    with open(os.path.join(OUT, 'manifest.json'), 'w', encoding='utf-8') as f:
        json.dump(manifest, f, indent=2)


def height(x,y):
    if not (xs[0] <= x <= xs[-1] and ys[0] <= y <= ys[-1]):
        raise ValueError('outside terrain')
    i = max(0,min(len(xs)-2,bisect.bisect_right(xs,x)-1))
    j = max(0,min(len(ys)-2,bisect.bisect_right(ys,y)-1))
    x0,x1=xs[i:i+2]; y0,y1=ys[j:j+2]
    u=(x-x0)/(x1-x0); v=(y-y0)/(y1-y0)
    a,b,c,d=[heights[p] for p in ((x0,y0),(x1,y0),(x0,y1),(x1,y1))]
    return a*(1-u-v)+b*u+c*v if u+v<=1 else d*(u+v-1)+c*(1-u)+b*(1-v)


def wet(x,y):
    # Membership in rendered water triangles, plus ground below water surface.
    for tri in water_bins.get((int(x//100),int(y//100)),[]):
        a,b,c=tri
        den=(b[1]-c[1])*(a[0]-c[0])+(c[0]-b[0])*(a[1]-c[1])
        if abs(den)<1e-6: continue
        u=((b[1]-c[1])*(x-c[0])+(c[0]-b[0])*(y-c[1]))/den
        v=((c[1]-a[1])*(x-c[0])+(a[0]-c[0])*(y-c[1]))/den
        if u>=-1e-5 and v>=-1e-5 and u+v<=1.00001:
            return height(x,y)<water_z-1
    return False


def setup():
    global xs,ys,heights,water_bins,water_z,mesh,curved_mesh,static_mat,original_mat
    assert les.load_level('/Game/Anastasis/Maps/Lvl_AnastasisSlice')
    for s in ('viewmode lit','ShowFlag.Sprites 0','ShowFlag.Grid 0',
              'anastasis.Atmosphere 0','anastasis.Terrain.Surface 2',
              'anastasis.Terrain.Forge 1','anastasis.Terrain.Forge.Terraces 0',
              'anastasis.Terrain.Forge.Escarpments 0','anastasis.Terrain.Forge.Bicubic 1',
              'anastasis.Terrain.Forge.Sharpen 0','anastasis.Terrain.Forge.TalusDeg 40',
              'anastasis.Terrain.Forge.ErosionIterations 300'):
        cmd(s)
    world=ues.get_editor_world()
    cls=unreal.load_class(None,'/Script/Anastasis_UnrealV2.AnastasisWorldEmbodiment')
    actor=list(unreal.GameplayStatics.get_all_actors_of_class(world,cls))[0]
    assert actor.call_method('EmbodyCanonical',args=(SEED,))
    assert actor.get_actor_location().length()<0.01
    # Existing dressing remains visible; record roots to avoid close occluders.
    roots=[]
    for c in actor.get_components_by_class(unreal.HierarchicalInstancedStaticMeshComponent):
        if c.get_instance_count() and c.is_visible():
            for i in range(c.get_instance_count()):
                p=c.get_instance_transform(i,world_space=True).translation
                roots.append((p.x,p.y,p.z))
    comps=actor.get_components_by_class(unreal.ProceduralMeshComponent)
    assert len(comps)==1
    vertices=unreal.ProceduralMeshLibrary.get_section_from_procedural_mesh(comps[0],0)[0]
    heights={(p.x,p.y):p.z for p in vertices}
    xs=sorted({p.x for p in vertices}); ys=sorted({p.y for p in vertices})
    assert len(heights)==len(xs)*len(ys)
    assert abs(xs[-1]-xs[0]-38000)<0.01, 'This study requires the delivered 4 m terrain'
    log('TERRAIN_READ')
    water=unreal.ProceduralMeshLibrary.get_section_from_procedural_mesh(comps[0],1)
    wv,wi=water[0],water[1]
    water_z=sum(p.z for p in wv)/len(wv)
    assert max(abs(p.z-water_z) for p in wv)<0.01
    water_bins={}
    for n in range(0,len(wi),3):
        tri=[(wv[wi[n+k]].x,wv[wi[n+k]].y) for k in range(3)]
        for bx in range(int(min(p[0] for p in tri)//100),int(max(p[0] for p in tri)//100)+1):
            for by in range(int(min(p[1] for p in tri)//100),int(max(p[1] for p in tri)//100)+1):
                water_bins.setdefault((bx,by),[]).append(tri)
    # Candidate roots: shallow water or low dry margin, with < 8 cm support spread.
    candidates=[]
    for x in range(int(xs[0]+1600),int(xs[-1]-1600),80):
        for y in range(int(ys[0]+1600),int(ys[-1]-1600),80):
            z=height(x,y)
            if not water_z-18 <= z <= water_z+45: continue
            near=sum(wet(x+70*math.cos(a*math.tau/8),y+70*math.sin(a*math.tau/8)) for a in range(8))
            if not 1<=near<=7: continue
            support=[height(x+16*math.cos(a*math.tau/8),y+16*math.sin(a*math.tau/8)) for a in range(8)]+[z]
            if max(support)-min(support)>8: continue
            if any((x-r[0])**2+(y-r[1])**2<180**2 for r in roots): continue
            candidates.append((x,y,z,min(support),max(support)))
    assert len(candidates)>=20, 'insufficient supported shoreline roots'
    # Densest local cluster, stable tie-break. Composition remains a bounded study.
    bins={}
    for p in candidates:
        bins.setdefault((int(p[0]//600),int(p[1]//600)),[]).append(p)
    key=max(sorted(bins),key=lambda k:len(bins[k]))
    cluster=bins[key]
    cx=sum(p[0] for p in cluster)/len(cluster); cy=sum(p[1] for p in cluster)/len(cluster)
    nearby=[p for p in candidates if (p[0]-cx)**2+(p[1]-cy)**2<430**2]
    # Deterministic islands with gaps, never a continuous wall.
    rng=random.Random(SEED)
    selected=[]
    for p in nearby:
        density=0.5+0.3*math.sin(p[0]/83)*math.cos(p[1]/109)
        if rng.random()>density: continue
        selected.append(p)
    assert len(selected)>=12
    selected=selected[:110]
    mesh=unreal.EditorAssetLibrary.load_asset(MESH_PATH)
    assert mesh is not None
    import importlib.util
    import sys
    sys.dont_write_bytecode=True
    recipe_path=os.path.join(unreal.Paths.project_dir(),'tools','unreal','create-reed-form.py')
    spec=importlib.util.spec_from_file_location('reed_form_recipe',recipe_path)
    recipe=importlib.util.module_from_spec(spec)
    spec.loader.exec_module(recipe)
    if unreal.EditorAssetLibrary.does_asset_exist(recipe.MESH):
        curved_mesh=unreal.EditorAssetLibrary.load_asset(recipe.MESH)
        static_mat=unreal.EditorAssetLibrary.load_asset(recipe.MAT)
        assert curved_mesh and static_mat
        bounds_c=curved_mesh.get_bounding_box()
        assert 170<bounds_c.max.z-bounds_c.min.z<190
        assert curved_mesh.get_material(0).get_path_name()==static_mat.get_path_name()
        log('CANDIDATE_RELOAD_PASS height=%.3f material=%s'%(bounds_c.max.z-bounds_c.min.z,static_mat.get_path_name()))
    else:
        curved_mesh,static_mat=recipe.create()
    wpo=unreal.MaterialEditingLibrary.get_material_property_input_node(static_mat,unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
    assert isinstance(wpo,unreal.MaterialExpressionConstant) and abs(wpo.get_editor_property('r'))<1e-8
    log('MATERIAL_RELOAD_PASS wpo=constant_zero')
    original_mat=unreal.EditorAssetLibrary.load_asset(recipe.SOURCE_MAT)
    manifest['candidate_mesh']=curved_mesh.get_path_name()
    manifest['candidate_material']=static_mat.get_path_name()
    with open(recipe_path,'rb') as recipe_file:
        manifest['recipe_sha256']=hashlib.sha256(recipe_file.read()).hexdigest()
    manifest['saved_assets']='new candidate mesh/material only; original assets and map unchanged'
    bounds=mesh.get_bounding_box()
    manifest.update(terrain_grid=[len(xs),len(ys)],water_z=water_z,
                    candidate_roots=len(candidates),selected_roots=len(selected),
                    focus=[cx,cy,height(cx,cy)],support_radius_uu=16,
                    support_spread_limit_uu=8,dressing_roots=len(roots))
    for i,p in enumerate(selected):
        x,y,z,low,high=p
        yaw=rng.uniform(0,360); rng.uniform(0.62,1)  # preserve the previous experiment's RNG sequence
        # Put entire base ring at/below local ground. Max burial remains <= 8 cm.
        a=eas.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(x,y,low-bounds.min.z),unreal.Rotator(pitch=0,yaw=yaw,roll=0))
        a.set_actor_label('ReedReview_%03d'%i)
        c=a.static_mesh_component
        c.set_mobility(unreal.ComponentMobility.MOVABLE)
        assert c.set_static_mesh(mesh)
        actual=a.get_actor_location()
        assert max(abs(actual.x-x),abs(actual.y-y),abs(actual.z-(low-bounds.min.z)))<0.01
        assert abs(a.get_actor_rotation().yaw-yaw)%360<0.01 or abs(a.get_actor_rotation().yaw-yaw+360)<0.01
        spawned.append(a)
        manifest['placements'].append(dict(location=[x,y,low-bounds.min.z],yaw=yaw,
            scale=[1,1,1],ground=z,support_min=low,support_max=high))
    # Fixed directional light and exposure for all shots.
    for sun in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.DirectionalLight):
        sun.get_component_by_class(unreal.DirectionalLightComponent).set_intensity(75000)
        sun.set_actor_rotation(unreal.Rotator(pitch=-38,yaw=-35,roll=0),False)
    for fog in list(unreal.GameplayStatics.get_all_actors_of_class(world,unreal.ExponentialHeightFog)):
        eas.destroy_actor(fog)
    for pp in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.PostProcessVolume):
        s=pp.get_editor_property('settings')
        for n,v in [('override_auto_exposure_min_brightness',True),('override_auto_exposure_max_brightness',True),
                    ('auto_exposure_min_brightness',14.0),('auto_exposure_max_brightness',14.0),
                    ('override_bloom_intensity',True),('bloom_intensity',0.0),
                    ('override_vignette_intensity',True),('vignette_intensity',0.0),
                    ('override_motion_blur_amount',True),('motion_blur_amount',0.0)]: s.set_editor_property(n,v)
        pp.set_editor_property('settings',s)
    target=unreal.Vector(cx,cy,water_z+65)
    options=[]
    for i in range(32):
        theta=math.tau*i/32
        x=cx+700*math.cos(theta); y=cy+700*math.sin(theta)
        g=height(x,y)
        if wet(x,y): continue
        loc=unreal.Vector(x,y,g+165)
        clearance=min(loc.z+(target.z-loc.z)*t/32-height(x+(cx-x)*t/32,y+(cy-y)*t/32) for t in range(1,32))
        if clearance<25: continue
        blocked=sum(1 for r in roots if any((x+(cx-x)*t/16-r[0])**2+(y+(cy-y)*t/16-r[1])**2<130**2 for t in range(17)))
        options.append((blocked,abs(g-water_z),i,loc,clearance))
    assert options,'no supported dry-bank camera'
    _,_,_,eye,clearance=min(options,key=lambda p:p[:3])
    views=[('eye',eye,target),('context',unreal.Vector(eye.x,eye.y,eye.z+430),target)]
    for name,loc,tar in views:
        rot=unreal.MathLibrary.find_look_at_rotation(loc,tar)
        manifest['views'].append(dict(name=name,location=[loc.x,loc.y,loc.z],rotation=[rot.pitch,rot.yaw,rot.roll],
                                     ground=height(loc.x,loc.y),target=[tar.x,tar.y,tar.z]))
    manifest['eye_terrain_clearance']=clearance
    write_manifest()
    log('READY roots=%d focus=(%.1f,%.1f) candidates=%d'%(len(spawned),cx,cy,len(candidates)))


def pose(mode,view):
    for a,p in zip(spawned,manifest['placements']):
        a.set_actor_hidden_in_game(False)
        a.static_mesh_component.set_visibility(mode!='bare')
        wanted_mesh=curved_mesh if mode=='C' else mesh
        a.static_mesh_component.set_static_mesh(wanted_mesh)
        assert a.static_mesh_component.get_editor_property('static_mesh').get_path_name()==wanted_mesh.get_path_name()
        a.static_mesh_component.set_material(0,original_mat if mode=='A' else static_mat)
        a.set_actor_scale3d(unreal.Vector(1,1,1))
    eas.clear_actor_selection_set()
    for flag in ('BillboardSprites','Selection','SelectionOutline','ModeWidgets','Brushes'):
        cmd('ShowFlag.%s 0'%flag)
    v=manifest['views'][view]
    ues.set_level_viewport_camera_info(unreal.Vector(*v['location']),unreal.Rotator(pitch=v['rotation'][0],yaw=v['rotation'][1],roll=v['rotation'][2]))
    actual_loc,actual_rot=ues.get_level_viewport_camera_info()
    assert abs(actual_rot.pitch-v['rotation'][0])<0.01 and abs(actual_rot.yaw-v['rotation'][1])<0.01
    log('CAMERA_VERIFIED mode=%s view=%s pitch=%.3f yaw=%.3f'%(mode,v['name'],actual_rot.pitch,actual_rot.yaw))
    les.editor_invalidate_viewports()


def finish(message,error=False):
    (unreal.log_error if error else unreal.log)('REED_REVIEW '+message)
    if handle is not None: unreal.unregister_slate_post_tick_callback(handle)
    unreal.SystemLibrary.quit_editor()


jobs=[(m,v) for v in range(2) for m in ('bare','A','B','C')]
phase='setup'; mark=time.monotonic(); index=0; shot=None


def tick(dt):
    global phase,mark,index,shot
    try:
        les.editor_invalidate_viewports()
        elapsed=time.monotonic()-mark
        if phase=='setup':
            phase='busy'  # map loading pumps Slate; prevent callback reentry
            setup(); pose(*jobs[0]); phase='settle'; mark=time.monotonic()
        elif phase=='settle' and elapsed>10:
            mode,vi=jobs[index]
            shot=os.path.join(OUT,mode+'_'+manifest['views'][vi]['name']+'.png').replace('\\','/')
            cmd('HighResShot 1600x900 filename="'+shot+'"')
            phase='shot'; mark=time.monotonic()
        elif phase=='shot':
            if os.path.isfile(shot) and os.path.getsize(shot)>1000 and elapsed>2:
                log('SHOT '+shot)
                index+=1
                if index==len(jobs): finish('COMPLETE shots=%d'%index); return
                pose(*jobs[index]); phase='settle'; mark=time.monotonic()
            elif elapsed>60: raise RuntimeError('capture timeout '+shot)
    except Exception:
        finish(traceback.format_exc(),True)


handle=unreal.register_slate_post_tick_callback(tick)
