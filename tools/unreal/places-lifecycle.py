"""Prove Places HISM reuse across four embodiments of the current full map."""
import json
import os
import time

import unreal


OUT = os.environ.get('ANASTASIS_PLACES_LIFECYCLE_OUT')
LEVEL = '/Game/Anastasis/Maps/Lvl_AnastasisSlice'
STATES = (('on', 1), ('repeat', 1), ('off', 0), ('return', 1))
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
handle = None


def finish(message, error=False):
    (unreal.log_error if error else unreal.log)(message)
    if handle is not None:
        unreal.unregister_slate_post_tick_callback(handle)
    unreal.SystemLibrary.quit_editor()


try:
    if not OUT:
        raise RuntimeError('ANASTASIS_PLACES_LIFECYCLE_OUT missing')
    os.makedirs(OUT, exist_ok=True)
    if ues.get_editor_world().get_path_name().split('.')[0] != LEVEL:
        les.load_level(LEVEL)
    world = ues.get_editor_world()
    cls = unreal.load_class(None, '/Script/Anastasis_UnrealV2.AnastasisWorldEmbodiment')
    found = unreal.GameplayStatics.get_all_actors_of_class(world, cls)
    actor = found[0] if found else eas.spawn_actor_from_class(cls, unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0))
except Exception as exc:
    finish('PLACES_LIFECYCLE FAIL setup %s' % exc, True)
    raise


rows = []
state_index = 0
mark = time.monotonic()


def sample(label, enabled):
    unreal.SystemLibrary.execute_console_command(world, 'anastasis.Dressing.Places %d' % enabled)
    actor.call_method('EmbodyCanonical', args=(12345,))
    components = [c for c in actor.get_components_by_class(unreal.HierarchicalInstancedStaticMeshComponent)
                  if c.get_name().startswith('Place_')]
    ids = sorted(c.get_path_name() for c in components)
    instances = sum(c.get_instance_count() for c in components)
    active = sum(c.get_instance_count() > 0 for c in components)
    row = dict(state=label, components=len(components), active=active, instances=instances, ids=ids)
    rows.append(row)
    unreal.log('PLACES_LIFECYCLE state=%s components=%d active=%d instances=%d' %
               (label, len(components), active, instances))
    if label == 'on' and (not components or not instances):
        raise RuntimeError('no Places instances in reference state')
    if label != 'on':
        first = rows[0]
        if row['ids'] != first['ids']:
            raise RuntimeError('Places component identities changed in state %s' % label)
        if label == 'off' and instances != 0:
            raise RuntimeError('disabled Places retained %d instances' % instances)
        if label != 'off' and instances != first['instances']:
            raise RuntimeError('Places instance count changed in state %s' % label)


def tick(dt):
    global state_index, mark
    try:
        if time.monotonic() - mark < (2.0 if state_index == 0 else 7.0):
            return
        label, enabled = STATES[state_index]
        sample(label, enabled)
        state_index += 1
        mark = time.monotonic()
        if state_index == len(STATES):
            with open(os.path.join(OUT, 'lifecycle.json'), 'w', encoding='utf-8') as f:
                json.dump(rows, f, indent=2)
            finish('PLACES_LIFECYCLE COMPLETE components=%d instances=%d' %
                   (rows[0]['components'], rows[0]['instances']))
    except Exception as exc:
        with open(os.path.join(OUT, 'lifecycle.json'), 'w', encoding='utf-8') as f:
            json.dump(rows, f, indent=2)
        finish('PLACES_LIFECYCLE FAIL %s' % exc, True)


handle = unreal.register_slate_post_tick_callback(tick)
