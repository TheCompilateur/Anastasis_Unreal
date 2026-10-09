"""One-editor A/B/A of slope woodland; never saves the level or assets."""
import collections
import hashlib
import json
import os
import time
import unreal

OUT = os.environ['ANASTASIS_PONTIC_SLOPE_OUT']
os.makedirs(OUT, exist_ok=True)
root = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
assert os.path.normcase(os.path.abspath(unreal.Paths.project_dir())) == os.path.normcase(root)
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
assert les.load_level('/Game/Anastasis/Maps/Lvl_AnastasisSlice')
world = ues.get_editor_world()
actors = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.AnastasisWorldEmbodiment)
assert len(actors) == 1
actor = actors[0]

def cmd(value):
    unreal.SystemLibrary.execute_console_command(world, value)

original_hour = unreal.SystemLibrary.get_console_variable_float_value('anastasis.Sky.Hour')
cmd('anastasis.Sky.Hour 11')

def tree_points():
    points = {}
    for comp in actor.get_components_by_class(unreal.HierarchicalInstancedStaticMeshComponent):
        if not comp.get_name().startswith('Dressing_Tree_'):
            continue
        for index in range(comp.get_instance_count()):
            got = comp.get_instance_transform(index, True)
            pose = got[1] if isinstance(got, tuple) else got
            p = pose.translation
            points[(round(p.x), round(p.y))] = (p.x, p.y, p.z)
    return points

def state(value):
    cmd('anastasis.Dressing.PonticSlopeEcology %d' % value)
    actor.call_method('EmbodyCanonical', args=(12345,))
    return tree_points()

def digest(points):
    return hashlib.sha256(json.dumps(sorted(points)).encode()).hexdigest()

reference = state(0)
candidate = state(1)
added = {key: value for key, value in candidate.items() if key not in reference}
if not added:
    raise RuntimeError('Pontic slope switch changed no tree positions')
bins = collections.defaultdict(list)
for point in added.values():
    bins[(int(point[0] // 10000), int(point[1] // 10000))].append(point)
cell, cluster = max(bins.items(), key=lambda item: (len(item[1]), item[0]))
cx = sum(p[0] for p in cluster) / len(cluster)
cy = sum(p[1] for p in cluster) / len(cluster)
cz = sorted(p[2] for p in cluster)[len(cluster) // 2]
surface = actor.get_components_by_class(unreal.ProceduralMeshComponent)[0]
vertices, triangles, normals, uvs, tangents = unreal.ProceduralMeshLibrary.get_section_from_procedural_mesh(surface, 0)

def ground(x, y):
    return min(vertices, key=lambda p: (p.x - x) ** 2 + (p.y - y) ** 2).z

V = unreal.Vector
eye_x, eye_y = cx - 5500, cy - 3000
views = [
    ('slope_aerial', V(cx - 18000, cy - 13000, cz + 9500), V(cx, cy, cz + 2500)),
    ('slope_eye', V(eye_x, eye_y, ground(eye_x, eye_y) + 170), V(cx, cy, cz + 900)),
]
report = {
    'seed': 12345, 'cell_100m': cell, 'added_in_cell': len(cluster),
    'reference_trees': len(reference), 'candidate_trees': len(candidate),
    'added_positions': len(added), 'reference_digest': digest(reference),
    'views': [[name, [eye.x, eye.y, eye.z], [target.x, target.y, target.z]]
              for name, eye, target in views],
    'state': {}, 'scope': 'editor scene only; no player or historical proof'
}
unreal.log('PONTIC_SLOPE_SITE cell=%s added=%d total=%d/%d' % (
    cell, len(cluster), len(reference), len(candidate)))
for flag in ('ShowFlag.Sprites 0', 'ShowFlag.Grid 0', 'viewmode lit'):
    cmd(flag)
states = [('reference', 0), ('candidate', 1), ('reference2', 0)]
look = unreal.MathLibrary.find_look_at_rotation
index, view_index = 0, 0
phase, mark, shot = 'boot', time.monotonic(), None
frames, timings, handle = [], [], None

def save():
    with open(os.path.join(OUT, 'capture.json'), 'w', encoding='utf-8') as stream:
        json.dump(report, stream, indent=1)

def finish(message):
    save()
    unreal.log(message)
    unreal.unregister_slate_post_tick_callback(handle)
    cmd('anastasis.Dressing.PonticSlopeEcology 1')
    cmd('anastasis.Sky.Hour %s' % original_hour)
    unreal.SystemLibrary.quit_editor()

def tick(dt):
    global phase, mark, index, view_index, shot, frames, timings
    try:
        elapsed = time.monotonic() - mark
        try:
            les.editor_invalidate_viewports()
        except Exception:
            pass
        if elapsed > 180:
            raise RuntimeError('state %s view %d phase %s timed out' % (states[index][0], view_index, phase))
        if phase == 'boot':
            if elapsed > 3:
                state(0)
                phase, mark = 'aim', time.monotonic()
            return
        if phase == 'aim':
            label, value = states[index]
            name, eye, target = views[view_index]
            ues.set_level_viewport_camera_info(eye, look(eye, target))
            if elapsed > 2:
                frames.append(dt)
                timing = actor.call_method('GetFrameTimingsMs')
                timings.append((timing.x, timing.y, timing.z))
            if elapsed < (16 if view_index == 0 else 7):
                return
            times = sorted(frames)
            gpu = sorted(sample[2] for sample in timings)
            report['state']['%s_%s' % (label, name)] = {
                'frame_ms_p50': 1000 * times[len(times) // 2] if times else None,
                'gpu_ms_p50': gpu[len(gpu) // 2] if gpu else None}
            shot = os.path.join(OUT, '%s_%s.png' % (name, label)).replace('\\', '/')
            if os.path.exists(shot):
                os.remove(shot)
            cmd('HighResShot 1600x900 filename="%s"' % shot)
            phase, mark = 'shot', time.monotonic()
            return
        if phase == 'shot':
            if not os.path.isfile(shot):
                if elapsed > 50:
                    raise RuntimeError('missing image %s' % shot)
                return
            if elapsed < 1:
                return
            unreal.log('PONTIC_SLOPE_SHOT %s' % os.path.basename(shot))
            view_index += 1
            frames, timings = [], []
            if view_index >= len(views):
                label, value = states[index]
                points = tree_points()
                report['state'][label] = {'trees': len(points), 'digest': digest(points)}
                view_index = 0
                index += 1
                if index >= len(states):
                    if report['state']['reference'] != report['state']['reference2']:
                        raise RuntimeError('reference inventory did not return')
                    finish('PONTIC_SLOPE_CAPTURE PASS trees=%d/%d added=%d views=%d' % (
                        len(reference), len(candidate), len(added), len(views)))
                    return
                state(states[index][1])
            phase, mark = 'aim', time.monotonic()
    except Exception as exc:
        finish('PONTIC_SLOPE_CAPTURE FAIL %s' % exc)

handle = unreal.register_slate_post_tick_callback(tick)
