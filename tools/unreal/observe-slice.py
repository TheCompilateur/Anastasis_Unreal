"""Observation de la tranche canonique 32x32 dans une scene dediee.

Cree si besoin /Game/Anastasis/Maps/Lvl_AnastasisSlice (jour simple + camera
d'observation + acteur d'incarnation), puis capture la tranche.

Variables d'environnement :
  ANASTASIS_SLICE_SHOT  chemin PNG de sortie (requis)
  ANASTASIS_SLICE_MODE  "0" terrain DEBUG legacy, "1" surface continue (defaut 1)
"""
import os, shutil, time, unreal

SHOT = os.environ.get('ANASTASIS_SLICE_SHOT', '')
MODE = os.environ.get('ANASTASIS_SLICE_MODE', '1')
SEED = 12345
LEVEL = '/Game/Anastasis/Maps/Lvl_AnastasisSlice'

# La tranche occupe [0,3200]x[0,3200] UU ; niveau de la mer a 275 UU.
SLICE_SPAN = 3200.0
CENTER = unreal.Vector(SLICE_SPAN * 0.5, SLICE_SPAN * 0.5, 400.0)
CAM_LOC = unreal.Vector(-1800.0, -1800.0, 3500.0)
CAM_ROT = unreal.Rotator(0.0, -32.8, 45.0)

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

unreal.log('SLICE_BOOT mode=%s shot=%s' % (MODE, SHOT))


def spawn(class_path, loc, rot):
    return eas.spawn_actor_from_class(unreal.load_class(None, class_path), loc, rot)


MATERIAL = '/Game/Anastasis/Materials/M_AnastasisSlice'

if os.environ.get('ANASTASIS_SLICE_REBUILD_MATERIAL', '0') == '1' and unreal.EditorAssetLibrary.does_asset_exist(MATERIAL):
    unreal.EditorAssetLibrary.delete_asset(MATERIAL)
    unreal.log('SLICE_MATERIAL_DELETED')

if not unreal.EditorAssetLibrary.does_asset_exist(MATERIAL):
    unreal.log('SLICE_MATERIAL_CREATE ' + MATERIAL)
    mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        'M_AnastasisSlice', '/Game/Anastasis/Materials', unreal.Material, unreal.MaterialFactoryNew())
    mel = unreal.MaterialEditingLibrary
    vc = mel.create_material_expression(mat, unreal.MaterialExpressionVertexColor, -600, 0)
    # La couleur de sommet EST la semantique : aucune texture decorative.
    r1 = False
    for out_name in ('', 'RGB', 'Color'):
        if mel.connect_material_property(vc, out_name, unreal.MaterialProperty.MP_BASE_COLOR):
            r1 = 'output=' + repr(out_name)
            break
    # Alpha = 1 sur l'eau : elle devient lisse et speculaire, la terre reste mate.
    rough = mel.create_material_expression(mat, unreal.MaterialExpressionLinearInterpolate, -350, 260)
    rough.set_editor_property('const_a', 0.93)
    rough.set_editor_property('const_b', 0.30)
    r2 = mel.connect_material_expressions(vc, 'A', rough, 'Alpha')
    r3 = mel.connect_material_property(rough, '', unreal.MaterialProperty.MP_ROUGHNESS)
    spec = mel.create_material_expression(mat, unreal.MaterialExpressionLinearInterpolate, -350, 460)
    spec.set_editor_property('const_a', 0.20)
    spec.set_editor_property('const_b', 1.00)
    r4 = mel.connect_material_expressions(vc, 'A', spec, 'Alpha')
    r5 = mel.connect_material_property(spec, '', unreal.MaterialProperty.MP_SPECULAR)
    unreal.log('SLICE_MATERIAL_WIRING base_color=%s vc_to_rough=%s roughness=%s vc_to_spec=%s specular=%s'
               % (r1, r2, r3, r4, r5))
    mel.recompile_material(mat)
    unreal.EditorAssetLibrary.save_asset(MATERIAL)
    unreal.log('SLICE_MATERIAL_SAVED')

if not unreal.EditorAssetLibrary.does_asset_exist(LEVEL):
    unreal.log('SLICE_LEVEL_CREATE ' + LEVEL)
    les.new_level(LEVEL)
    # Jour simple : soleil rasant pour que le relief se lise, ciel pour l'ambiante.
    sun = spawn('/Script/Engine.DirectionalLight', unreal.Vector(0, 0, 4000), unreal.Rotator(0.0, -38.0, -55.0))
    sun.set_actor_label('Sun_SliceObservation')
    spawn('/Script/Engine.SkyAtmosphere', unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0)).set_actor_label('SkyAtmosphere_Slice')
    spawn('/Script/Engine.SkyLight', unreal.Vector(0, 0, 2000), unreal.Rotator(0, 0, 0)).set_actor_label('SkyLight_Slice')
    cam = spawn('/Script/Engine.CameraActor', CAM_LOC, CAM_ROT)
    cam.set_actor_label('Cam_SliceObservation')
    spawn('/Script/Anastasis_UnrealV2.AnastasisWorldEmbodiment', unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0)).set_actor_label('AnastasisWorldEmbodiment')
    les.save_current_level()
    unreal.log('SLICE_LEVEL_SAVED')

unreal.log('SLICE_MAP_LOAD=' + str(les.load_level(LEVEL)))
world = ues.get_editor_world()

def ensure_daylight():
    """Jour physique + exposition figee : les captures doivent etre comparables."""
    changed = False
    for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.load_class(None, '/Script/Engine.DirectionalLight')):
        comp = a.get_component_by_class(unreal.DirectionalLightComponent)
        if comp and abs(comp.get_editor_property('intensity') - 75000.0) > 1.0:
            comp.set_intensity(75000.0)   # plein soleil, en lux
            changed = True
        # Sans ce drapeau, SkyAtmosphere n'est pas eclairee : ciel noir, et l'eau
        # lisse reflete ce noir en plaques.
        if comp and not comp.get_editor_property('atmosphere_sun_light'):
            comp.set_editor_property('atmosphere_sun_light', True)
            changed = True
    for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.load_class(None, '/Script/Engine.SkyLight')):
        comp = a.get_component_by_class(unreal.SkyLightComponent)
        if comp and not comp.get_editor_property('real_time_capture'):
            comp.set_editor_property('real_time_capture', True)
            changed = True
    # EV100 fige : le projet a ExtendDefaultLuminanceRange=True. Une valeur trop
    # haute expose bien le sol mais ecrase le ciel, que l'eau lisse reflete ensuite.
    EV = float(os.environ.get('ANASTASIS_SLICE_EV', '12.0'))
    ppvs = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.load_class(None, '/Script/Engine.PostProcessVolume'))
    if not ppvs:
        ppv = spawn('/Script/Engine.PostProcessVolume', unreal.Vector(1600, 1600, 1000), unreal.Rotator(0, 0, 0))
        ppv.set_actor_label('PP_SliceExposure')
        ppv.set_editor_property('unbound', True)
        ppvs = [ppv]
        changed = True
    for ppv in ppvs:
        st = ppv.get_editor_property('settings')
        if abs(st.get_editor_property('auto_exposure_min_brightness') - EV) > 0.01:
            st.set_editor_property('override_auto_exposure_method', True)
            st.set_editor_property('auto_exposure_method', unreal.AutoExposureMethod.AEM_HISTOGRAM)
            st.set_editor_property('override_auto_exposure_min_brightness', True)
            st.set_editor_property('auto_exposure_min_brightness', EV)
            st.set_editor_property('override_auto_exposure_max_brightness', True)
            st.set_editor_property('auto_exposure_max_brightness', EV)
            ppv.set_editor_property('settings', st)
            changed = True
    if not unreal.GameplayStatics.get_all_actors_of_class(world, unreal.load_class(None, '/Script/Engine.SkyAtmosphere')):
        spawn('/Script/Engine.SkyAtmosphere', unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0)).set_actor_label('SkyAtmosphere_Slice')
        changed = True
    if changed:
        les.save_current_level()
    inventory = {}
    for path in ('/Script/Engine.DirectionalLight', '/Script/Engine.SkyLight', '/Script/Engine.SkyAtmosphere',
                 '/Script/Engine.PostProcessVolume', '/Script/Engine.CameraActor'):
        inventory[path.split('.')[-1]] = len(unreal.GameplayStatics.get_all_actors_of_class(world, unreal.load_class(None, path)))
    unreal.log('SLICE_DAYLIGHT changed=%s inventory=%s' % (changed, inventory))


ensure_daylight()
unreal.SystemLibrary.execute_console_command(world, 'ShowFlag.Sprites 0')
unreal.SystemLibrary.execute_console_command(world, 'ShowFlag.Grid 0')
unreal.SystemLibrary.execute_console_command(world, 'anastasis.Terrain.Surface ' + MODE)

cls = unreal.load_class(None, '/Script/Anastasis_UnrealV2.AnastasisWorldEmbodiment')
found = unreal.GameplayStatics.get_all_actors_of_class(world, cls)
actor = found[0] if len(found) > 0 else spawn('/Script/Anastasis_UnrealV2.AnastasisWorldEmbodiment', unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0))

ok = actor.call_method('EmbodyCanonical', args=(SEED,))
unreal.log('SLICE_EMBODY=%s instances=%s' % (ok, actor.call_method('GetInstanceCount')))

SHOT_DIR = os.path.join(unreal.Paths.project_saved_dir(), 'Screenshots')


def newest_png(after):
    best, best_t = None, after
    for root, _dirs, files in os.walk(SHOT_DIR):
        for f in files:
            if f.lower().endswith('.png'):
                full = os.path.join(root, f)
                t = os.path.getmtime(full)
                if t > best_t:
                    best, best_t = full, t
    return best


def aim():
    ues.set_level_viewport_camera_info(CAM_LOC, CAM_ROT)
    loc, rot = ues.get_level_viewport_camera_info()
    unreal.log('SLICE_CAMERA_APPLIED loc=(%.0f,%.0f,%.0f) pitch=%.1f yaw=%.1f'
               % (loc.x, loc.y, loc.z, rot.pitch, rot.yaw))


t0 = time.monotonic()
mark = time.time()
phase = 0
last_request = 0.0
handle = None


def finish(msg, error=False):
    (unreal.log_error if error else unreal.log)(msg)
    unreal.unregister_slate_post_tick_callback(handle)
    unreal.SystemLibrary.quit_editor()


def tick(dt):
    global phase, last_request
    elapsed = time.monotonic() - t0
    if phase == 0 and elapsed > 3.0:
        phase = 1
        aim()
    elif phase == 1 and elapsed > 8.0:
        # Un materiau fraichement cree compile ses shaders : la premiere demande
        # de capture peut etre perdue, donc on relance tant qu'aucun PNG n'arrive.
        phase = 2
        aim()
        unreal.SystemLibrary.execute_console_command(world, 'HighResShot 1920x1080')
        last_request = elapsed
        unreal.log('SLICE_SHOT_REQUESTED')
    elif phase == 2:
        found_png = newest_png(mark)
        if found_png:
            phase = 3
            shutil.copyfile(found_png, SHOT)
            unreal.log('SLICE_SHOT_OK bytes=%d' % os.path.getsize(SHOT))
            finish('SLICE_COMPLETE')
        elif elapsed - last_request > 12.0 and elapsed < 100.0:
            aim()
            unreal.SystemLibrary.execute_console_command(world, 'HighResShot 1920x1080')
            last_request = elapsed
            unreal.log('SLICE_SHOT_RETRY t=%.0f' % elapsed)
        elif elapsed >= 100.0:
            finish('SLICE_SHOT_MISSING dir=' + SHOT_DIR, True)
    elif elapsed > 130.0:
        finish('SLICE_TIMEOUT phase=%d' % phase, True)


handle = unreal.register_slate_post_tick_callback(tick)
