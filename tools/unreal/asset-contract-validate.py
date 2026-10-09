"""Read-only mechanical validation of one asset family in a fresh Unreal editor.

ANASTASIS_ASSET_CONTRACT is the absolute path to a V1 JSON contract in this
worktree. The script reads saved StaticMesh assets and source references; it
never creates, edits or saves assets. Run through editor-batch.ps1 so the proof
is replayed by the designated integrator.
"""

import json
import math
import os

import unreal


ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
PREFIX = 'ASSET_CONTRACT'


def fail(message):
    raise ValueError(message)


def local_file(relative):
    if not isinstance(relative, str) or not relative or os.path.isabs(relative):
        fail('invalid relative file path %r' % relative)
    full = os.path.abspath(os.path.join(ROOT, relative))
    if os.path.commonpath((ROOT, full)) != ROOT or not os.path.isfile(full):
        fail('missing or external source file %r' % relative)
    return full


def required_text(data, key):
    value = data.get(key)
    if not isinstance(value, str) or not value.strip():
        fail('missing text field %s' % key)
    return value.strip()


def positive_numbers(values, count, field):
    if not isinstance(values, list) or len(values) != count:
        fail('%s must contain %d values' % (field, count))
    for value in values:
        if isinstance(value, bool) or not isinstance(value, (int, float)) or not math.isfinite(value) or value <= 0:
            fail('%s has invalid positive value %r' % (field, value))
    return values


def asset_path(path):
    if not path.startswith('/Game/') or '.' in path or path.endswith('/'):
        fail('invalid Unreal asset path %r' % path)
    return path


def validate_asset(spec):
    path = asset_path(required_text(spec, 'path'))
    materials = spec.get('materials')
    if not isinstance(materials, list) or not materials:
        fail('%s has no materials contract' % path)
    expected_materials = [asset_path(value) for value in materials]
    expected_triangles = spec.get('triangles_by_lod')
    if not isinstance(expected_triangles, list) or not expected_triangles:
        fail('%s has no LOD contract' % path)
    if any(isinstance(n, bool) or not isinstance(n, int) or n <= 0 for n in expected_triangles):
        fail('%s has invalid triangle counts' % path)
    if any(a < b for a, b in zip(expected_triangles, expected_triangles[1:])):
        fail('%s triangle counts increase with LOD' % path)
    max_size = positive_numbers(spec.get('max_size_cm'), 3, 'max_size_cm')
    min_z = spec.get('min_z_cm')
    if not isinstance(min_z, list) or len(min_z) != 2 or any(
            isinstance(v, bool) or not isinstance(v, (int, float)) or not math.isfinite(v) for v in min_z):
        fail('%s has invalid min_z_cm interval' % path)
    if min_z[0] > min_z[1]:
        fail('%s has reversed min_z_cm interval' % path)

    consumer_file = local_file(required_text(spec, 'consumer_file'))
    token = required_text(spec, 'consumer_token')
    if token != path:
        fail('%s consumer token differs from asset path' % path)
    with open(consumer_file, encoding='utf-8-sig') as source:
        if token not in source.read():
            fail('%s absent from declared consumer %s' % (path, spec['consumer_file']))

    mesh = unreal.EditorAssetLibrary.load_asset(path)
    if mesh is None or not isinstance(mesh, unreal.StaticMesh):
        fail('%s is not a loadable StaticMesh' % path)
    if mesh.get_path_name() != path + '.' + path.rsplit('/', 1)[-1]:
        fail('%s loaded from unexpected object path %s' % (path, mesh.get_path_name()))
    lod_count = mesh.get_num_lods()
    if lod_count != len(expected_triangles):
        fail('%s LOD count %d != %d' % (path, lod_count, len(expected_triangles)))
    triangles = [mesh.get_num_triangles(i) for i in range(lod_count)]
    if triangles != expected_triangles:
        fail('%s triangles %r != %r' % (path, triangles, expected_triangles))

    actual_materials = []
    for slot in mesh.get_editor_property('static_materials'):
        material = slot.get_editor_property('material_interface')
        actual_materials.append(material.get_path_name().split('.')[0] if material else None)
    if actual_materials != expected_materials:
        fail('%s materials %r != %r' % (path, actual_materials, expected_materials))

    bounds = mesh.get_bounding_box()
    minimum = (bounds.min.x, bounds.min.y, bounds.min.z)
    maximum = (bounds.max.x, bounds.max.y, bounds.max.z)
    if not all(math.isfinite(v) for v in minimum + maximum):
        fail('%s bounds are not finite' % path)
    size = [maximum[i] - minimum[i] for i in range(3)]
    if any(size[i] <= 0 or size[i] > max_size[i] for i in range(3)):
        fail('%s size %r cm exceeds %r cm or is empty' % (path, size, max_size))
    if not min_z[0] <= minimum[2] <= min_z[1]:
        fail('%s minimum Z %.2f cm outside %r' % (path, minimum[2], min_z))
    unreal.log('%s ASSET path=%s lods=%d triangles=%s size_cm=%s min_z_cm=%.2f materials=%s consumer_ref=present' % (
        PREFIX, path, lod_count, triangles, [round(v, 2) for v in size], minimum[2], actual_materials))


def main():
    project = os.path.abspath(unreal.Paths.project_dir())
    if not os.path.samefile(ROOT, project):
        fail('editor project %s differs from script root %s' % (project, ROOT))
    contract_path = os.environ.get('ANASTASIS_ASSET_CONTRACT', '')
    if not contract_path or not os.path.isfile(contract_path):
        fail('ANASTASIS_ASSET_CONTRACT is missing or unreadable')
    if os.path.commonpath((ROOT, os.path.abspath(contract_path))) != ROOT:
        fail('contract is outside this worktree')
    with open(contract_path, encoding='utf-8-sig') as stream:
        contract = json.load(stream)
    if contract.get('schema_version') != 1:
        fail('unsupported schema_version')
    name = required_text(contract, 'id')
    required_text(contract, 'family')
    required_text(contract, 'intent')
    required_text(contract, 'reference_provenance')
    required_text(contract, 'collision_policy')
    required_text(contract, 'keep')
    required_text(contract, 'reject')
    for field in ('references', 'generator_files'):
        paths = contract.get(field)
        if not isinstance(paths, list) or not paths:
            fail('%s is empty' % field)
        for value in paths:
            local_file(value)
    proof = required_text(contract, 'capture_proof')
    with open(local_file('tools/unreal/proofs.txt'), encoding='utf-8-sig') as stream:
        registered = any(line.startswith(proof + ' | ') for line in stream)
    if not registered:
        fail('capture proof %s is absent from proofs.txt' % proof)
    specs = contract.get('assets')
    if not isinstance(specs, list) or not specs:
        fail('assets list is empty')
    paths = [required_text(spec, 'path') for spec in specs]
    if len(paths) != len(set(paths)):
        fail('duplicate asset paths')
    for spec in specs:
        validate_asset(spec)
    unreal.log('%s PASS id=%s assets=%d capture_proof=%s scope=mechanical_saved_assets_and_source_references' % (
        PREFIX, name, len(specs), proof))


try:
    main()
except Exception as error:
    unreal.log_error('%s FAIL %s: %s' % (PREFIX, type(error).__name__, error))
finally:
    unreal.SystemLibrary.quit_editor()
