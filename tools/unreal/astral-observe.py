"""ASTRAL fixed daylight A/B, live editor + PIE, no asset or map saves.
Outputs go to ANASTASIS_ASTRAL_OUT. Same binary/map/cameras; Ecology is the only A/B setting.
"""
import os, time, json, hashlib, unreal
OUT = os.environ['ANASTASIS_ASTRAL_OUT']
os.makedirs(OUT, exist_ok=True)
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
assert les.load_level('/Game/Anastasis/Maps/Lvl_AnastasisSlice')
world = ues.get_editor_world()
def cmd(c):
    unreal.SystemLibrary.execute_console_command(world, c)
def actors(cls):
    return list(unreal.GameplayStatics.get_all_actors_of_class(world, cls))
cls = unreal.load_class(None, '/Script/Anastasis_UnrealV2.AnastasisWorldEmbodiment')
actor = actors(cls)[0]
cmd('anastasis.Terrain.Surface 2')
cmd('anastasis.Atmosphere 0')
cmd('viewmode lit')
cmd('ShowFlag.Sprites 0')
cmd('ShowFlag.Grid 0')
# Fixed, neutral settings on transient loaded scene only.
for sun in actors(unreal.DirectionalLight):
    sun.get_component_by_class(unreal.DirectionalLightComponent).set_intensity(75000.0)
for fog in actors(unreal.ExponentialHeightFog):
    eas.destroy_actor(fog)
for pp in actors(unreal.PostProcessVolume):
    st=pp.get_editor_property('settings')
    for n,v in [('override_auto_exposure_min_brightness',True),('override_auto_exposure_max_brightness',True),
                ('auto_exposure_min_brightness',14.0),('auto_exposure_max_brightness',14.0),
                ('override_bloom_intensity',True),('bloom_intensity',0.0),
                ('override_vignette_intensity',True),('vignette_intensity',0.0),
                ('override_motion_blur_amount',True),('motion_blur_amount',0.0)]:
        st.set_editor_property(n,v)
    pp.set_editor_property('settings',st)
r=unreal.load_asset('/Game/Anastasis/Presentation/DA_AnastasisPresentation')
for e in r.get_editor_property('entries'):
    unreal.log('ASTRAL_ASSET type=%s variants=%s' % (e.get_editor_property('semantic_type'),
        [(str(v.get_editor_property('mesh')),str(v.get_editor_property('material_override'))) for v in e.get_editor_property('variants')]))
def rebuild(mode):
    cmd('anastasis.Dressing.Ecology '+str(mode))
    assert actor.call_method('EmbodyCanonical',args=(12345,))
def inventory():
    rows=[]; count=0; comps=0
    for c in actor.get_components_by_class(unreal.HierarchicalInstancedStaticMeshComponent):
        if not c.get_name().startswith('Dressing_'): continue
        n=c.get_instance_count(); count+=n; comps+=1
        for i in range(n):
            t=c.get_instance_transform(i,world_space=False)
            rows.append([c.get_name(),t.translation.x,t.translation.y,t.translation.z,t.rotation.x,t.rotation.y,t.rotation.z,t.rotation.w,t.scale3d.x,t.scale3d.y,t.scale3d.z])
    return dict(instances=count, components=comps, actors=len(eas.get_all_level_actors()),
        hash=hashlib.sha256(json.dumps(rows,sort_keys=True).encode()).hexdigest())
rebuild(0)
baseline=inventory()
# Choose one forest patch from baseline spatial bins, then keep the same camera for A/B.
bins={}
for c in actor.get_components_by_class(unreal.HierarchicalInstancedStaticMeshComponent):
    if 'Tree' not in c.get_name(): continue
    for i in range(c.get_instance_count()):
        p=c.get_instance_transform(i,world_space=False).translation
        k=(int(p.x//1200),int(p.y//1200))
        bins.setdefault(k,[]).append(p)
key=max(sorted(bins),key=lambda k:len(bins[k]))
pts=bins[key]
focus=unreal.Vector(sum(p.x for p in pts)/len(pts),sum(p.y for p in pts)/len(pts),sum(p.z for p in pts)/len(pts))
local_loc=focus+unreal.Vector(-1500,-1500,1300)
local_rot=unreal.MathLibrary.find_look_at_rotation(local_loc,focus)
views=[('overview',unreal.Vector(-5400,-5400,10500),unreal.Rotator(0,-32.8,45)),
       ('edge',local_loc,local_rot)]
results={'seed':12345,'surface_mode':2,'sun_lux':75000,'ev100':14,
         'focus':str(focus),'baseline':baseline,'views':[(v,str(l),str(r)) for v,l,r in views]}
phase=0; mark=time.monotonic(); requested=False; handle=None; active_shot=None
jobs=[(0,views[0]),(0,views[1]),(1,views[0]),(1,views[1])]
def finish():
    with open(os.path.join(OUT,'observation.json'),'w') as f: json.dump(results,f,indent=2)
    unreal.unregister_slate_post_tick_callback(handle)
    unreal.log('ASTRAL_OBSERVE_COMPLETE '+json.dumps(results))
    unreal.SystemLibrary.quit_editor()
def tick(dt):
    global phase,mark,requested,active_shot
    elapsed=time.monotonic()-mark
    if phase<len(jobs):
        mode,(name,loc,rot)=jobs[phase]
        if not requested and elapsed>6:
            rebuild(mode)
            ues.set_level_viewport_camera_info(loc,rot)
            active_shot=os.path.join(OUT,('%s_%s.png'%('B' if mode else 'A',name))).replace('\\','/')
            requested=True; mark=time.monotonic()
        elif requested and elapsed>8 and active_shot:
            cmd('HighResShot 1600x900 filename="'+active_shot+'"')
            active_shot=None; mark=time.monotonic()
        elif requested and active_shot is None and elapsed>5:
            path=os.path.join(OUT,('%s_%s.png'%('B' if mode else 'A',name)))
            if not os.path.isfile(path):
                raise RuntimeError('ASTRAL screenshot missing '+path)
            unreal.log('ASTRAL_SHOT '+path)
            phase+=1; requested=False; mark=time.monotonic()
    elif phase==4:
        results['ecology']=inventory()
        rebuild(1)
        results['repeat']=inventory()
        results['deterministic']=results['ecology']['hash']==results['repeat']['hash']
        les.editor_request_begin_play()
        phase=5; mark=time.monotonic()
    elif phase==5 and les.is_in_play_in_editor():
        results['pie_active']=True
        gw=ues.get_game_world()
        found=list(unreal.GameplayStatics.get_all_actors_of_class(gw,cls))
        results['pie_embodiments']=len(found)
        results['pie_dressing']=[a.call_method('GetDressingInstanceCount') for a in found]
        les.editor_request_end_play()
        phase=6; mark=time.monotonic()
    elif phase==6 and not les.is_in_play_in_editor():
        finish()
    elif elapsed>90:
        results['timeout_phase']=phase
        finish()
handle=unreal.register_slate_post_tick_callback(tick)
