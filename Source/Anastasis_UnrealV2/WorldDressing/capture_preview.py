"""Run inside a dedicated Unreal editor. Does not save assets or the map.
ANASTASIS_DRESSING_OUT: absolute evidence folder. ANASTASIS_DRESSING_KEEP_OPEN=1 retains preview.
"""
import os, time, json, hashlib, traceback
import unreal

OUT = os.environ['ANASTASIS_DRESSING_OUT']
os.makedirs(OUT, exist_ok=True)
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
handle = None
results = {}

def finish(error=None):
    if error:
        results['error'] = str(error)
        unreal.log_error('WORLD_DRESSING_CAPTURE_FAIL ' + str(error))
    else:
        unreal.log('WORLD_DRESSING_CAPTURE_COMPLETE ' + json.dumps(results))
    with open(os.path.join(OUT, 'observation.json'), 'w') as f:
        json.dump(results, f, indent=2)
    if handle is not None:
        unreal.unregister_slate_post_tick_callback(handle)
    if os.environ.get('ANASTASIS_DRESSING_KEEP_OPEN') != '1':
        unreal.SystemLibrary.quit_editor()

def inventory(actor):
    rows = []
    for c in actor.get_components_by_class(unreal.HierarchicalInstancedStaticMeshComponent):
        mesh = c.get_editor_property('static_mesh')
        for i in range(c.get_instance_count()):
            t = c.get_instance_transform(i, world_space=True)
            rows.append([mesh.get_path_name() if mesh else '', *[round(v, 6) for v in
                (t.translation.x,t.translation.y,t.translation.z,t.rotation.x,t.rotation.y,t.rotation.z,t.rotation.w,
                 t.scale3d.x,t.scale3d.y,t.scale3d.z)]])
    rows.sort()
    return {'count': len(rows), 'hash': hashlib.sha256(json.dumps(rows).encode()).hexdigest()}

try:
    assert les.load_level('/Game/Anastasis/Maps/Lvl_AnastasisSlice')
    world = ues.get_editor_world()
    def cmd(value):
        unreal.SystemLibrary.execute_console_command(world, value)
    for c in ('viewmode lit', 'ShowFlag.Sprites 0', 'ShowFlag.Grid 0', 't.MaxFPS 30'):
        cmd(c)
    source_cls = unreal.load_class(None, '/Script/Anastasis_UnrealV2.AnastasisWorldEmbodiment')
    sources = list(unreal.GameplayStatics.get_all_actors_of_class(world, source_cls))
    assert len(sources) == 1, 'Expected one already embodied map'
    source = sources[0]
    source_before = inventory(source)
    manager_cls = unreal.load_class(None, '/Script/Anastasis_UnrealV2.AnastasisWorldDressingManager')
    manager = eas.spawn_actor_from_class(manager_cls, unreal.Vector(0,0,0))
    manager.set_actor_label('World Dressing V0 - TRANSIENT PREVIEW')
    profile = unreal.new_object(unreal.AnastasisWorldDressingProfile)
    def rule(asset_id, path, density, low, high, families):
        r = unreal.AnastasisDressingRule()
        for name, value in dict(asset_id=asset_id, static_mesh=unreal.load_asset(path), density=density,
                scale_min=low, scale_max=high, slope_max=32.0, clearance_radius=20.0,
                allowed_terrain_families=families).items():
            r.set_editor_property(name,value)
        return r
    land = [unreal.AnastasisDressingTerrain.GRASS,unreal.AnastasisDressingTerrain.SCRUB]
    rules = [
        rule('BroadleafUnderstory', '/Game/Anastasis/Vegetation/SM_Tree_Broadleaf_Understory_01', .25, 1.0, 1.8, land),
        rule('ConiferCanopy', '/Game/Anastasis/Vegetation/SM_Tree_Conifer_Canopy_01', .075, 2.5, 4.0, land),
        rule('RuinDetail', '/Game/Anastasis/Architecture/SM_Ruin_Generic_01', .06, .35, .65,
             [unreal.AnastasisDressingTerrain.STONE,unreal.AnastasisDressingTerrain.RUIN])]
    profile.set_editor_property('rules',rules)
    manager.set_editor_property('world_source',source)
    manager.set_editor_property('profile',profile)
    manager.set_editor_property('seed',12345)
    actor_count = len(eas.get_all_level_actors())
    manager.generate_preview()
    first = inventory(manager)
    first_hash = manager.get_editor_property('placement_hash')
    assert first['count'] > 0, str(manager.get_editor_property('placement_report'))
    assert first_hash
    manager.rebuild_from_seed()
    assert inventory(manager) == first
    assert manager.get_editor_property('placement_hash') == first_hash
    assert len(eas.get_all_level_actors()) == actor_count, 'One actor per prop violation'
    assert inventory(source) == source_before, 'Existing dressing was changed'
    # Find a populated patch solely to choose a camera; camera then stays fixed across A/B.
    bins = {}
    for c in manager.get_components_by_class(unreal.HierarchicalInstancedStaticMeshComponent):
        for i in range(c.get_instance_count()):
            p=c.get_instance_transform(i,world_space=True).translation
            bins.setdefault((int(p.x//1200),int(p.y//1200)),[]).append(p)
    pts=bins[max(sorted(bins),key=lambda k:len(bins[k]))]
    focus=unreal.Vector(sum(p.x for p in pts)/len(pts),sum(p.y for p in pts)/len(pts),sum(p.z for p in pts)/len(pts))
    near=focus+unreal.Vector(-1200,-1500,1100)
    views=[('overview',unreal.Vector(-5400,-5400,10500),unreal.Rotator(0,-32.8,45)),
           ('detail',near,unreal.MathLibrary.find_look_at_rotation(near,focus))]
    results.update(seed=12345, hash=first_hash, independent_hism_inventory=first,
        actor_count=actor_count, repeat_equal=True, source_unchanged=True,
        report=str(manager.get_editor_property('placement_report')),
        views=[(name,str(loc),str(rot)) for name,loc,rot in views])
    manager.clear_preview()
    assert inventory(manager)['count'] == 0
    assert len(manager.get_components_by_class(unreal.HierarchicalInstancedStaticMeshComponent)) == 0
    assert inventory(source) == source_before
    results['clear_pass'] = True
    jobs=[('before',v) for v in views]+[('after',v) for v in views]
    phase=0
    mark=time.monotonic()
    requested=False
    setup=False
    def tick(dt):
        global phase,mark,requested,setup
        try:
            if phase>=len(jobs):
                assert inventory(manager)==first
                assert inventory(source)==source_before
                eas.set_selected_level_actors([manager])
                finish()
                return
            state,(name,loc,rot)=jobs[phase]
            elapsed=time.monotonic()-mark
            les.editor_invalidate_viewports()
            if elapsed>90:
                raise RuntimeError('Screenshot timeout phase='+str(phase))
            if not setup:
                if state=='after': manager.generate_preview()
                else: manager.clear_preview()
                ues.set_level_viewport_camera_info(loc,rot)
                setup=True
                mark=time.monotonic()
                return
            path=os.path.join(OUT,state+'_'+name+'.png').replace('\\','/')
            if not requested and elapsed>10:
                cmd('HighResShot 1600x900 filename="'+path+'"')
                requested=True
                mark=time.monotonic()
            elif requested and elapsed>8 and os.path.isfile(path):
                unreal.log('WORLD_DRESSING_SHOT '+path)
                phase+=1;mark=time.monotonic();requested=False;setup=False
        except Exception:
            finish(traceback.format_exc())
    handle=unreal.register_slate_post_tick_callback(tick)
except Exception:
    finish(traceback.format_exc())
