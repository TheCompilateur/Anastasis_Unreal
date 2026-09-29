"""Read-only map inspection and fixed-camera macro-forest comparison. Never saves assets.

Set ANASTASIS_FOREST_OUT to an evidence directory, then run via editor -ExecCmds="py <path>".
The default map load is measured before changing any CVar, so a forced rebuild cannot
hide a missing forest on reopening the map.
"""
import hashlib
import json
import os
import time
import unreal

OUT = os.environ['ANASTASIS_FOREST_OUT']
os.makedirs(OUT, exist_ok=True)
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
if not les.load_level('/Game/Anastasis/Maps/Lvl_AnastasisSlice'):
    unreal.SystemLibrary.quit_editor()
    raise RuntimeError('Could not load the existing map; check Git LFS checkout')
world = ues.get_editor_world()
actors = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.AnastasisWorldEmbodiment)
if len(actors) != 1:
    unreal.SystemLibrary.quit_editor()
    raise RuntimeError('Expected exactly one existing world embodiment')
actor = actors[0]


def cmd(value):
    unreal.SystemLibrary.execute_console_command(world, value)


def inventory():
    rows = []
    heights = []
    components = []
    for comp in actor.get_components_by_class(unreal.HierarchicalInstancedStaticMeshComponent):
        if not comp.get_name().startswith('Dressing_Tree_'):
            continue
        mesh = comp.get_editor_property('static_mesh')
        count = comp.get_instance_count()
        components.append({'name': comp.get_name(), 'count': count, 'mesh': mesh.get_path_name(), 'visible': comp.is_visible()})
        for index in range(count):
            pose = comp.get_instance_transform(index, world_space=False)
            loc = pose.translation
            scale = pose.scale3d
            rot = pose.rotation
            rows.append((comp.get_name(), loc.x, loc.y, loc.z, scale.x, scale.y, scale.z, rot.x, rot.y, rot.z, rot.w))
            heights.append(mesh.get_bounds().box_extent.z * 2 * scale.z)
    heights.sort()
    rows.sort()
    return {'count': len(rows), 'sha256': hashlib.sha256(json.dumps(rows, sort_keys=True).encode()).hexdigest(),
            'height_uu': [heights[0], heights[len(heights)//2], heights[-1]] if heights else [], 'components': components}


report = {'default_load': inventory(), 'project': os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))), 'seed': 12345,
          'scope': 'editor scene; no player or frame-time claim'}
basin = actor.call_method('GetTerrainForgeBasin')
report['basin'] = [basin.x, basin.y, basin.z]
overview = unreal.Vector(-3200,-3200,6400)
overview_target = unreal.Vector(4800,4800,600)
valley = unreal.Vector(basin.x,basin.y,basin.z+170)
valley_target = unreal.Vector(5900,2400,1050)
jobs = [(0,'A_overview',overview,overview_target), (1,'B_overview',overview,overview_target),
        (0,'A_valley',valley,valley_target), (1,'B_valley',valley,valley_target)]
phase = -1
requested = False
mark = time.monotonic()
handle = None


def finish():
    actor.call_method('EmbodyCanonical', args=(12345,))
    report['repeat'] = inventory()
    report['repeat_matches'] = report['repeat']['sha256'] == report['macro']['sha256']
    report['default_matches_macro'] = report['default_load']['sha256'] == report['macro']['sha256']
    with open(os.path.join(OUT,'forest-scene.json'),'w') as f:
        json.dump(report,f,indent=2)
    unreal.log('MACRO_FOREST_SCENE ' + json.dumps(report))
    unreal.unregister_slate_post_tick_callback(handle)
    unreal.SystemLibrary.quit_editor()


def advance():
    global phase, mark, requested
    phase += 1
    if phase == len(jobs):
        finish()
        return
    mode, name, loc, target = jobs[phase]
    cmd('anastasis.Dressing.MacroForest %d' % mode)
    assert actor.call_method('EmbodyCanonical',args=(12345,))
    report['macro' if mode else 'original'] = inventory()
    report[name] = {'location': [loc.x,loc.y,loc.z], 'target': [target.x,target.y,target.z]}
    ues.set_level_viewport_camera_info(loc,unreal.MathLibrary.find_look_at_rotation(loc,target))
    unreal.get_editor_subsystem(unreal.EditorActorSubsystem).select_nothing()
    cmd('ShowFlag.Sprites 0')
    cmd('ShowFlag.Grid 0')
    cmd('viewmode lit')
    mark = time.monotonic()
    requested = False


def tick(dt):
    global requested
    les.editor_invalidate_viewports()
    elapsed = time.monotonic()-mark
    if phase == -1:
        if elapsed > 5:
            advance()
        return
    if elapsed > 90:
        unreal.log_error('MACRO_FOREST_CAPTURE timeout phase=%d' % phase)
        unreal.unregister_slate_post_tick_callback(handle)
        unreal.SystemLibrary.quit_editor()
        return
    path = os.path.join(OUT,jobs[phase][1]+'.png').replace('\\','/')
    if not requested and elapsed > 12:
        cmd('HighResShot 1600x900 filename="%s"' % path)
        requested = True
    elif requested and elapsed > 20 and os.path.isfile(path):
        advance()


handle = unreal.register_slate_post_tick_callback(tick)
