"""
ANASTASIS_UNREAL_TREE_FORM_001 -- .5f : ce que la vegetation coute vraiment.

LECTURE SEULE. Ne cree, ne modifie et ne sauve aucun asset ni aucun niveau.

Pourquoi ce fichier existe : trois passes de vegetation ont ete livrees en
affirmant "ca reste compatible avec beaucoup d'arbres" sans jamais produire un
seul chiffre. Ni triangles, ni sommets, ni niveaux de detail, ni nombre de
composants. Une affirmation de cout qu'on ne mesure pas est une opinion.

Ce qu'il mesure, sur l'incarnation REELLE du monde canonique :
  - par mesh : triangles et sommets du LOD0, nombre de LOD, slots de materiau ;
  - par composant HISM : le mesh pose, le nombre d'instances ;
  - le total : triangles reellement soumis, et une borne haute du nombre de
    draw calls (un par composant et par slot, ce que fait un HISM sans LOD).

Le chiffre qui compte n'est pas le triangle du mesh isole : c'est
triangles x instances, parce que c'est lui qui explose quand la foret se
densifie.

    tools\\unreal\\measure-tree-cost.ps1
"""
import unreal

LEVEL = '/Game/Anastasis/Maps/Lvl_AnastasisSlice'
SEED = 12345

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)


def log(msg):
    unreal.log('COST ' + str(msg))


def mesh_stats(mesh):
    """Triangles / sommets / LOD, en essayant les points d'entree connus de cette
    version plutot qu'en pariant sur un seul."""
    tris = verts = lods = None
    sm_sub = getattr(unreal, 'StaticMeshEditorSubsystem', None)
    sub = unreal.get_editor_subsystem(sm_sub) if sm_sub else None
    for owner in (sub, unreal.EditorStaticMeshLibrary):
        if owner is None:
            continue
        try:
            if tris is None:
                tris = owner.get_number_triangles(mesh, 0)
            if verts is None:
                verts = owner.get_number_verts(mesh, 0)
            if lods is None:
                lods = owner.get_lod_count(mesh)
        except Exception:
            continue
    if tris is None:
        try:
            tris = mesh.get_num_triangles(0)
            verts = mesh.get_num_vertices(0)
            lods = mesh.get_num_lods()
        except Exception as exc:
            log('WARN stats illisibles sur %s : %s' % (mesh.get_name(), exc))
    try:
        slots = len(mesh.get_editor_property('static_materials'))
    except Exception:
        slots = None
    return tris, verts, lods, slots


les.load_level(LEVEL)
world = ues.get_editor_world()
unreal.SystemLibrary.execute_console_command(world, 'anastasis.Terrain.Surface 2')
cls = unreal.load_class(None, '/Script/Anastasis_UnrealV2.AnastasisWorldEmbodiment')
actor = unreal.GameplayStatics.get_all_actors_of_class(world, cls)[0]
log('embody=%s' % actor.call_method('EmbodyCanonical', args=(SEED,)))

total_tris = 0
total_instances = 0
draw_calls = 0
rows = []
seen = {}

for comp in actor.get_components_by_class(unreal.HierarchicalInstancedStaticMeshComponent):
    name = comp.get_name()
    if name.startswith('Tiles_'):
        continue  # le sol DEBUG, pas de la vegetation
    mesh = comp.static_mesh
    count = comp.get_instance_count()
    if mesh is None or count == 0:
        continue
    key = mesh.get_name()
    if key not in seen:
        seen[key] = mesh_stats(mesh)
    tris, verts, lods, slots = seen[key]
    submitted = (tris or 0) * count
    total_tris += submitted
    total_instances += count
    draw_calls += (slots or 1)
    rows.append((key, count, tris, verts, lods, slots, submitted))

rows.sort(key=lambda r: -r[6])
log('%-34s %7s %7s %7s %5s %6s %12s' % ('mesh', 'inst', 'tris', 'verts', 'lod', 'slots', 'tris_poses'))
for key, count, tris, verts, lods, slots, submitted in rows:
    log('%-34s %7d %7s %7s %5s %6s %12d' % (key, count, tris, verts, lods, slots, submitted))

log('TOTAL composants=%d instances=%d triangles_poses=%d draw_calls_max=%d'
    % (len(rows), total_instances, total_tris, draw_calls))

no_lod = [r[0] for r in rows if (r[4] or 1) <= 1]
if no_lod:
    log('SANS_LOD %d mesh(es) : %s' % (len(no_lod), ', '.join(sorted(no_lod))))
    log('SANS_LOD consequence : chaque instance soumet sa geometrie pleine a toute distance.')

# A quelle densite le budget devient-il un sujet ? On projette a partir du cout
# mesure par instance, sans rien supposer d'autre.
if total_instances:
    per_instance = total_tris / float(total_instances)
    log('PAR_INSTANCE %.0f triangles en moyenne' % per_instance)
    for target in (5000, 20000, 100000):
        log('PROJECTION %6d instances -> %10.1f M triangles poses'
            % (target, target * per_instance / 1e6))

log('COMPLETE')
unreal.SystemLibrary.quit_editor()
