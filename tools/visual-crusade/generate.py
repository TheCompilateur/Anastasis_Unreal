"""VISUAL_CRUSADE_001: regenerate owned render assets, preserve existing bindings.
Executed as an editor-batch job. Does not save the map or alter simulation data.
"""
import importlib.util
import os
import sys
import unreal

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '../..'))
sys.path.insert(0, os.path.join(ROOT, 'tools/unreal'))
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
les.load_level('/Engine/Maps/Entry')
path = os.path.join(ROOT, 'tools/unreal/create_tree_asset.py')
spec = importlib.util.spec_from_file_location('crusade_trees', path)
trees = importlib.util.module_from_spec(spec)
spec.loader.exec_module(trees)
# Trees only: existing shrubs and rock material belong to other visual strata.
for recipe in trees.FAMILIES + trees.species_specs():
    path = trees.PACKAGE_PATH+'/'+recipe['name']
    original = unreal.load_asset(path)
    sm = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    collision = (sm.get_simple_collision_count(original), str(sm.get_collision_complexity(original)))
    mesh = trees.build_family(recipe)
    far = trees.distant_crown(recipe)
    trees.save_static_mesh(mesh, trees.PACKAGE_PATH+'/'+recipe['name'], far)
    current = unreal.load_asset(path)
    after = (sm.get_simple_collision_count(current), str(sm.get_collision_complexity(current)))
    if after != collision:
        raise RuntimeError('Collision changed: '+path)
    unreal.log('CRUSADE_TREE_SAVED %s collisions=%s triangles=%s' % (recipe['name'], collision, [current.get_num_triangles(i) for i in range(3)]))
unreal.log('CRUSADE_GENERATION_COMPLETE')
unreal.SystemLibrary.quit_editor()
