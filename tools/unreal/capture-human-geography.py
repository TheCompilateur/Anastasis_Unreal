"""Read-only live A/B capture; never saves the level or source assets."""
import unreal, json, os, time, traceback
OUT = os.environ.get('ANASTASIS_HUMAN_EVIDENCE', r'C:/Users/alex_/.codex/visualizations/2026/09/29/01a0eead-8695-79e0-a61e-9d4359418d75')
ues=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
eas=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
les=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
world=ues.get_editor_world()
actor=next(a for a in eas.get_all_level_actors() if a.get_class().get_name()=='AnastasisWorldEmbodiment')
def cmd(s): unreal.SystemLibrary.execute_console_command(world,s)
def dump(name,data):
    with open(os.path.join(OUT,name),'w',encoding='utf-8') as f: json.dump(data,f)
def vec(v): return [v.x,v.y,v.z]
cmd('anastasis.WorldView.Scale 20')
cmd('ShowFlag.Sprites 0');cmd('ShowFlag.Grid 0')
captures=[]; shots=[]; mode=-1; phase=0; mark=time.monotonic(); requested=False; handle=None
def prepare(layer):
    global shots, phase, mark, requested
    cmd('anastasis.Terrain.HumanGeography '+str(layer))
    actor.call_method('EmbodyCanonical',args=(12345,))
    c=actor.get_components_by_class(unreal.ProceduralMeshComponent)[0]
    tr=c.get_world_transform(); terrain=None
    for section in range(c.get_num_sections()):
        vertices,triangles,normals,uvs,tangents=unreal.ProceduralMeshLibrary.get_section_from_procedural_mesh(c,section)
        v=[vec(unreal.MathLibrary.transform_location(tr,p)) for p in vertices]
        dump('human_v2_'+str(layer)+'_mesh_'+str(section)+'.json',{'vertices':v,'triangles':list(triangles)})
        if section==0: terrain=v
    tag='v2' if layer else 'original_scaled'
    shots=[(tag+'_overview',[-35000,-55000,178000],[96000,96000,10000])]
    for name,x,y,dx,dy in [('A_village',53,53,0,1),('A_fields',61,47,-1,1),('B_pasture',33,24,0,1),('AB_pass',42,33,1,1),('C_mountains',60,86,1,0)]:
        p=min(terrain,key=lambda v:(v[0]-x*2000)**2+(v[1]-y*2000)**2)
        loc=[p[0],p[1],p[2]+170]
        shots.append((tag+'_'+name,loc,[loc[0]+dx*10000,loc[1]+dy*10000,loc[2]]))
    captures.extend(shots); dump('human_v2_capture_positions.json',captures)
    dump('human_v2_runtime.json',{'engine':unreal.SystemLibrary.get_engine_version(),'project':unreal.Paths.project_dir(),'world':world.get_path_name(),'seed':12345,'spatial_scale':20,'current_layer':layer,'landscape_count':sum(isinstance(a,unreal.LandscapeProxy) for a in eas.get_all_level_actors()),'eye_height_cm':170,'comparison':'same XY and direction, eye height relative to each actual surface'})
    phase=0;mark=time.monotonic();requested=False
def tick(dt):
    global mode,phase,mark,requested
    try:
        les.editor_invalidate_viewports()
        if mode<0:
            mode=0;prepare(mode);return
        if phase>=len(shots):
            if mode==0:
                mode=1;prepare(mode);return
            unreal.unregister_slate_post_tick_callback(handle)
            unreal.log('HUMAN_GEOGRAPHY_AB_CAPTURE_COMPLETE')
            unreal.SystemLibrary.quit_editor();return
        name,loc,target=shots[phase]
        elapsed=time.monotonic()-mark
        if not requested:
            ues.set_level_viewport_camera_info(unreal.Vector(*loc),unreal.MathLibrary.find_look_at_rotation(unreal.Vector(*loc),unreal.Vector(*target)))
            if elapsed>7:
                cmd('HighResShot 1600x900 filename="'+OUT+'/'+name+'.png"')
                requested=True;mark=time.monotonic()
        elif elapsed>4:
            if not os.path.isfile(OUT+'/'+name+'.png'):raise RuntimeError('missing '+name)
            phase+=1;requested=False;mark=time.monotonic()
    except Exception:
        unreal.log_error(traceback.format_exc());unreal.unregister_slate_post_tick_callback(handle);unreal.SystemLibrary.quit_editor()
handle=unreal.register_slate_post_tick_callback(tick)
