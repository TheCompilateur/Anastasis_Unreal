"""Create/reload one shelter and capture front, rear and interior with existing props.
Launcher: capture-camp-shelter.ps1 -OutDir <new absolute path>.
Assets saved only in CampShelter009. The studio level is never saved.
"""
import os,sys,time,json,math,traceback,importlib.util
import unreal as u
sys.dont_write_bytecode=True
OUT=os.environ['ANASTASIS_SHELTER_OUT']
spec=importlib.util.spec_from_file_location('refugee_props',os.path.join(u.Paths.project_dir(),'tools/unreal/create-camp-shelter.py'))
recipe=importlib.util.module_from_spec(spec);spec.loader.exec_module(recipe)
eas=u.get_editor_subsystem(u.EditorActorSubsystem)
les=u.get_editor_subsystem(u.LevelEditorSubsystem)
ues=u.get_editor_subsystem(u.UnrealEditorSubsystem)
actors={};stats={};shots=[];handle=None
phase='setup';mark=time.monotonic();index=0
jobs=[('front',0),('rear',1),('interior',2)]

def log(s):u.log('SHELTER009 '+s)
def cmd(s):u.SystemLibrary.execute_console_command(ues.get_editor_world(),s)
def spawn(cls,loc,rot=None):
    return eas.spawn_actor_from_class(cls,u.Vector(*loc),rot or u.Rotator())
def setup():
    global stats,cam
    assert u.EditorLoadingAndSavingUtils.new_blank_map(False)
    meshes,stats=recipe.create()
    # Neutral empty studio, no existing world regeneration or scene dressing.
    floor=spawn(u.StaticMeshActor,(0,0,-6))
    floor.static_mesh_component.set_static_mesh(u.load_asset('/Engine/BasicShapes/Cube'))
    floor.set_actor_scale3d(u.Vector(50,50,.1))
    floor.set_actor_label('Shelter009_StudioFloor')
    mel=u.MaterialEditingLibrary
    floor_mat=u.AssetToolsHelpers.get_asset_tools().create_asset('M_Studio_Unsaved','/Game/Anastasis/Shelter009StudioTemp',u.Material,u.MaterialFactoryNew())
    grey=mel.create_material_expression(floor_mat,u.MaterialExpressionConstant3Vector,0,0)
    grey.set_editor_property('constant',u.LinearColor(.18,.19,.20,1))
    assert mel.connect_material_property(grey,'',u.MaterialProperty.MP_BASE_COLOR)
    assert not list(mel.recompile_material(floor_mat))
    floor.static_mesh_component.set_material(0,floor_mat)
    sun=spawn(u.DirectionalLight,(0,0,1000),u.Rotator(pitch=-40,yaw=45,roll=0))
    light=sun.get_component_by_class(u.DirectionalLightComponent)
    light.set_mobility(u.ComponentMobility.MOVABLE)
    light.set_intensity(75000);light.set_editor_property('atmosphere_sun_light',True)
    fill=spawn(u.DirectionalLight,(0,0,900),u.Rotator(pitch=-28,yaw=220,roll=0)).get_component_by_class(u.DirectionalLightComponent)
    fill.set_mobility(u.ComponentMobility.MOVABLE)
    fill.set_intensity(18000)
    fill.set_editor_property('cast_shadows',False)
    spawn(u.SkyAtmosphere,(0,0,0))
    sky=spawn(u.SkyLight,(0,0,500)).get_component_by_class(u.SkyLightComponent)
    sky.set_mobility(u.ComponentMobility.MOVABLE)
    sky.set_editor_property('real_time_capture',True)
    pp=spawn(u.PostProcessVolume,(0,0,0));pp.set_editor_property('unbound',True)
    settings=pp.get_editor_property('settings')
    for key,value in [('override_auto_exposure_min_brightness',True),('override_auto_exposure_max_brightness',True),
        ('auto_exposure_min_brightness',14.0),('auto_exposure_max_brightness',14.0),
        ('override_bloom_intensity',True),('bloom_intensity',0.0),
        ('override_motion_blur_amount',True),('motion_blur_amount',0.0),
        ('override_vignette_intensity',True),('vignette_intensity',0.0)]:
        settings.set_editor_property(key,value)
    pp.set_editor_property('settings',settings)
    cam=spawn(u.CameraActor,(0,-1000,400))
    cam.camera_component.set_field_of_view(48)
    a=spawn(u.StaticMeshActor,(0,0,0))
    a.static_mesh_component.set_static_mesh(meshes[recipe.NAME])
    a.set_actor_label(recipe.NAME);actors[recipe.NAME]=a
    assert a.static_mesh_component.get_editor_property('static_mesh').get_path_name()==meshes[recipe.NAME].get_path_name()
    context=[
        ('SM_Chest_Travel_01',(100,72,0),-12),
        ('SM_Amphora_Transport_01',(-137,48,0),22),
        ('SM_FishTrap_Wicker_01',(-102,-37,0),-18),
        ('SM_Tripod_Cauldron_01',(-95,-353,0),15)]
    for n,p,yaw in context:
        mesh=u.load_asset('/Game/Anastasis/RefugeeProps008/'+n)
        assert mesh,n
        a=spawn(u.StaticMeshActor,p,u.Rotator(yaw=yaw))
        a.static_mesh_component.set_static_mesh(mesh)
        a.set_actor_label('ExistingContext_'+n)
    for flag in ['Sprites','Grid','BillboardSprites','Selection','SelectionOutline','ModeWidgets','Brushes','Bounds','CameraFrustums','Collision']:
        cmd('ShowFlag.'+flag+' 0')
    cmd('viewmode lit')
    eas.clear_actor_selection_set()
    les.pilot_level_actor(cam)
    les.editor_set_game_view(True)
    log('READY assets=1')
def pose(name,view):
    poses={
        'front':((770,-1080,550),(0,-30,116),48),
        'rear':((-730,900,470),(0,10,120),48),
        'interior':((126,-310,158),(-28,82,124),68)}
    xyz,aim,fov=poses[name]
    loc=u.Vector(*xyz);target=u.Vector(*aim)
    cam.camera_component.set_field_of_view(fov)
    rot=u.MathLibrary.find_look_at_rotation(loc,target)
    cam.set_actor_location(loc,False,False);cam.set_actor_rotation(rot,False)
    les.pilot_level_actor(cam)
    les.editor_set_game_view(True)
    eas.clear_actor_selection_set()
    shots.append(dict(asset=name,view=view,camera=[loc.x,loc.y,loc.z],rotation=[rot.pitch,rot.yaw,rot.roll],fov=fov))
    with open(os.path.join(OUT,'manifest.json'),'w',encoding='utf-8') as f:
        json.dump(dict(recipe=recipe.VERSION,assets=stats,shots=shots,sun_lux=75000,ev100=14,
            reference='docs/visual/reference/pontique-props-materiel-refugies.png',
            supporting_reference='docs/visual/reference/pontique-camp-installation-rive.png',
            fill_lux=18000,saved_level=False,collision_scope='none on shelter; visual prop only',
            production_scope='static shelter; no simulated cloth, collision, LOD chain or gameplay integration'),f,indent=2)
    les.editor_invalidate_viewports()
def finish(error=None):
    if handle is not None:u.unregister_slate_post_tick_callback(handle)
    if error:u.log_error('SHELTER009 FAILED '+error)
    else:log('COMPLETE shots=%d assets=1'%len(jobs))
    u.SystemLibrary.quit_editor()
def tick(dt):
    global phase,mark,index,shot
    try:
        elapsed=time.monotonic()-mark
        les.editor_invalidate_viewports()
        if phase=='setup':
            phase='busy';setup();pose(*jobs[0]);phase='settle';mark=time.monotonic()
        elif phase=='settle' and elapsed>(60 if index==0 else 12):
            name,view=jobs[index]
            shot=os.path.join(OUT,name+'_'+str(view)+'.png').replace('\\','/')
            phase='busy';cmd('HighResShot 1600x900 filename="'+shot+'"');phase='shot';mark=time.monotonic()
        elif phase=='shot':
            if os.path.isfile(shot) and os.path.getsize(shot)>1000 and elapsed>2:
                log('SHOT '+shot);index+=1
                if index==len(jobs):finish();return
                phase='busy';pose(*jobs[index]);phase='settle';mark=time.monotonic()
            elif elapsed>90:raise RuntimeError('Screenshot timeout')
    except Exception:finish(traceback.format_exc())
handle=u.register_slate_post_tick_callback(tick)
