"""One-editor 0/1/0 capture of the far mountain material at human eye height.

Launched by mountain-rock-capture.ps1. No actor or package is saved.
"""
import os
import time
import unreal

out_dir = os.environ['ANASTASIS_MOUNTAIN_ROCK_OUT']
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
les.load_level('/Game/Anastasis/Maps/Lvl_AnastasisSlice')
world = ues.get_editor_world()


def command(value):
    unreal.SystemLibrary.execute_console_command(world, value)


for value in ('ShowFlag.Sprites 0', 'ShowFlag.Grid 0', 'viewmode lit',
              'anastasis.Terrain.Horizon 1', 'anastasis.Terrain.HorizonFarMaterial 1'):
    command(value)

world_class = unreal.load_class(None, '/Script/Anastasis_UnrealV2.AnastasisWorldEmbodiment')
actors = unreal.GameplayStatics.get_all_actors_of_class(world, world_class)
actor = actors[0] if actors else eas.spawn_actor_from_class(
    world_class, unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0))
unreal.log('MOUNTAIN_ROCK_ACTORS count=%d' % len(actors))
actor.call_method('EmbodyCanonical', args=(12345,))

horizon = next((c for c in actor.get_components_by_class(unreal.ProceduralMeshComponent)
                if c.get_name() == 'HorizonTerrain'), None)
if horizon is None or not horizon.is_visible():
    raise RuntimeError('HorizonTerrain absent ou invisible')
parent = horizon.get_material(2)
if parent is None or 'M_AnastasisFarTerrain' not in parent.get_name():
    raise RuntimeError('materiau lointain inattendu: %s' % parent)
mid = horizon.create_dynamic_material_instance(2, parent)
if mid is None:
    raise RuntimeError('MID de montagne impossible')

basin = actor.call_method('GetTerrainForgeBasin')
hit = unreal.SystemLibrary.line_trace_single(
    world, unreal.Vector(basin.x, basin.y, 1000000),
    unreal.Vector(basin.x, basin.y, -100000),
    unreal.TraceTypeQuery.ECC_VISIBILITY, True, [], unreal.DrawDebugTrace.NONE, True)
impact = hit[1] if isinstance(hit, tuple) else hit
ground_z = impact.to_tuple()[4].z
eye = unreal.Vector(basin.x, basin.y, ground_z + 170.0)
rotation = unreal.Rotator(0.0, 3.0, 45.0)
unreal.log('MOUNTAIN_ROCK_VIEW x=%.0f y=%.0f z=%.0f pitch=3 yaw=45' % (
    eye.x, eye.y, eye.z))
for sky in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.SkyAtmosphere):
    component = sky.get_component_by_class(unreal.SkyAtmosphereComponent)
    unreal.log('MOUNTAIN_ROCK_SKY aerial_scale=%.3f' %
               component.get_editor_property('aerial_pespective_view_distance_scale'))

states = [('A', 0.0), ('B', 1.0), ('A2', 0.0)]
index = 0
phase = 'aim'
mark = time.monotonic()
shot = None
gpu = []


def tick(_dt):
    global index, phase, mark, shot, gpu
    if phase == 'done':
        return
    try:
        les.editor_invalidate_viewports()
    except Exception:
        pass
    elapsed = time.monotonic() - mark
    if phase == 'aim':
        if index >= len(states):
            unreal.log('MOUNTAIN_ROCK_COMPLETE')
            phase = 'done'
            unreal.SystemLibrary.quit_editor()
            return
        tag, value = states[index]
        mid.set_scalar_parameter_value('MountainRockDetail', value)
        ues.set_level_viewport_camera_info(eye, rotation)
        if elapsed > 2.5:
            try:
                gpu.append(actor.call_method('GetFrameTimingsMs').z)
            except Exception:
                pass
        if elapsed > (16.0 if index == 0 else 8.0):
            shot = os.path.join(out_dir, 'S045_%s.png' % tag).replace('\\', '/')
            if os.path.exists(shot):
                os.remove(shot)
            command('HighResShot 1920x1080 filename="%s"' % shot)
            phase, mark = 'wait', time.monotonic()
    elif phase == 'wait':
        if not os.path.isfile(shot):
            if elapsed > 45.0:
                unreal.log_error('MOUNTAIN_ROCK_FAIL missing=%s' % shot)
                phase = 'done'
                unreal.SystemLibrary.quit_editor()
            return
        if elapsed > 1.5:
            samples = sorted(gpu)
            unreal.log('MOUNTAIN_ROCK_SHOT %s bytes=%d gpu_ms_p50=%.2f' % (
                os.path.basename(shot), os.path.getsize(shot),
                samples[len(samples) // 2] if samples else -1.0))
            gpu = []
            index += 1
            phase, mark = 'aim', time.monotonic()


unreal.register_slate_post_tick_callback(tick)
