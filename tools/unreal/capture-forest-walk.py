"""Forest views in a dedicated live editor; no asset/map saved, quits on completion.

Set ANASTASIS_FOREST_OUT to a NEW directory. Run via -ExecCmds="py <absolute path>".
Optional ANASTASIS_FOREST_CAMERAS points to a frozen camera JSON for comparisons.
The default views are at the existing vieille_foret. Timings are editor callback
wall time, not GPU cost. This is not a PIE or player test.
"""
import hashlib
import json
import os
import time
import unreal

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '../..'))
OUT = os.environ['ANASTASIS_FOREST_OUT']
assert os.path.normcase(os.path.abspath(unreal.Paths.project_dir())) == os.path.normcase(ROOT)
os.makedirs(OUT, exist_ok=True)
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
assert les.load_level('/Game/Anastasis/Maps/Lvl_AnastasisSlice')
world = ues.get_editor_world()
actor = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.AnastasisWorldEmbodiment)[0]
rows, trees, transforms = [], [], []
for component in actor.get_components_by_class(unreal.HierarchicalInstancedStaticMeshComponent):
    if not component.get_name().startswith('Dressing_Tree_'):
        continue
    mesh = component.static_mesh
    bounds = mesh.get_bounding_box()
    assert abs(bounds.min.z+50) < .01 and abs(bounds.max.z-50) < .01, mesh.get_name()
    assert len(mesh.get_editor_property('static_materials')) == 2, mesh.get_name()
    rows.append(dict(name=mesh.get_name(), count=component.get_instance_count(),
                     triangles_by_lod=[mesh.get_num_triangles(i) for i in range(mesh.get_num_lods())]))
    for i in range(component.get_instance_count()):
        p = component.get_instance_transform(i, world_space=True)
        # Unreal struct repr contains process-specific information; hash values only.
        transforms.append([mesh.get_name(), p.translation.x, p.translation.y, p.translation.z,
                           p.rotation.x, p.rotation.y, p.rotation.z, p.rotation.w,
                           p.scale3d.x, p.scale3d.y, p.scale3d.z])
        trees.append([p.translation.x, p.translation.y, p.translation.z, 100*p.scale3d.z])

camera_file = os.environ.get('ANASTASIS_FOREST_CAMERAS')
if camera_file:
    with open(camera_file) as stream:
        cameras = json.load(stream)
else:
    places = [row.split('|') for row in actor.call_method('GetPlaceReport')]
    wood = next(row for row in places if row[0] == 'vieille_foret')
    x, y = float(wood[1]), float(wood[2])
    near = sorted(trees, key=lambda p: (p[0]-x)**2+(p[1]-y)**2)[:80]
    hero = max(near, key=lambda p: p[3])
    base = hero[2]-hero[3]/2
    # Use a tree's anchored base instead of tracing onto its collision canopy.
    cameras = [dict(name='trunk', eye=[hero[0]-900, hero[1]-600, base+170],
                    target=[hero[0], hero[1], base+800]),
               dict(name='crown', eye=[hero[0]-1800, hero[1]-1200, base+170],
                    target=[hero[0], hero[1], base+hero[3]*.72])]
for camera in cameras:
    assert not os.path.exists(os.path.join(OUT, camera['name']+'.png')), 'Use a fresh output directory'
transforms.sort()
report = dict(project=unreal.Paths.project_dir(), meshes=rows, trees=len(trees), cameras=cameras,
              instance_transform_sha256=hashlib.sha256(json.dumps(transforms).encode()).hexdigest(),
              instance_weighted_lod0_triangle_upper_bound=sum(r['count']*r['triangles_by_lod'][0] for r in rows),
              frame_scope='Editor callback wall time; includes host load, not GPU timing', completed=False)

def write_report():
    with open(os.path.join(OUT, 'report.json'), 'w') as stream:
        json.dump(report, stream, indent=2)

def command(value):
    unreal.SystemLibrary.execute_console_command(world, value)

write_report()
phase, mark, shot, samples, handle = -1, time.monotonic(), False, [], None

def next_camera():
    global phase, mark, shot, samples
    phase += 1
    if phase == len(cameras):
        report['completed'] = True
        write_report()
        unreal.unregister_slate_post_tick_callback(handle)
        unreal.SystemLibrary.quit_editor()
        return
    camera = cameras[phase]
    eye, target = unreal.Vector(*camera['eye']), unreal.Vector(*camera['target'])
    ues.set_level_viewport_camera_info(eye, unreal.MathLibrary.find_look_at_rotation(eye, target))
    unreal.get_editor_subsystem(unreal.EditorActorSubsystem).select_nothing()
    command('ShowFlag.Sprites 0')
    command('ShowFlag.Grid 0')
    command('viewmode lit')
    mark, shot, samples = time.monotonic(), False, []

def tick(dt):
    global shot
    les.editor_invalidate_viewports()
    elapsed = time.monotonic()-mark
    if phase < 0:
        if elapsed > 5:
            next_camera()
        return
    if elapsed > 180:
        unreal.log_error('FOREST_WALK: capture timeout')
        unreal.unregister_slate_post_tick_callback(handle)
        unreal.SystemLibrary.quit_editor()
        return
    if 8 < elapsed < 18:
        samples.append(dt*1000)
    camera = cameras[phase]
    path = os.path.join(OUT, camera['name']+'.png').replace('\\', '/')
    if elapsed > 18 and not shot:
        ordered = sorted(samples)
        if ordered:
            camera.update(editor_wall_ms_p50=ordered[len(ordered)//2],
                          editor_wall_ms_p95=ordered[int(len(ordered)*.95)], samples=len(ordered))
        command('HighResShot 1600x900 filename="%s"' % path)
        shot = True
    if elapsed > 23 and shot and os.path.isfile(path):
        next_camera()

handle = unreal.register_slate_post_tick_callback(tick)
