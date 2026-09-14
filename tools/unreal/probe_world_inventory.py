"""
Inventaire de ce que le monde POSE reellement : un mesh, un compte.

Sert a trancher une question qu'une capture ne tranche pas -- « cet objet gris, c'est un
rocher ou une ruine ? ». Lit les composants HISM de AAnastasisWorldEmbodiment apres
incarnation et rapporte, par mesh, le nombre d'instances et leur enveloppe de taille
reelle en monde (bounds du mesh x echelle de l'instance).

Lecture seule : charge le niveau, ne le sauvegarde pas, ne cree ni ne modifie aucun asset.

Env :
  ANASTASIS_INV_MODE   anastasis.Terrain.Surface, defaut "2" (monde 96x96)
"""
import os

import unreal

SEED = 12345
LEVEL = '/Game/Anastasis/Maps/Lvl_AnastasisSlice'
MODE = os.environ.get('ANASTASIS_INV_MODE', '2')

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)


def log(msg):
    unreal.log('WORLD_INV ' + str(msg))


def main():
    log('MAP_LOAD=' + str(les.load_level(LEVEL)))
    world = ues.get_editor_world()
    unreal.SystemLibrary.execute_console_command(world, 'anastasis.Terrain.Surface ' + MODE)

    cls = unreal.load_class(None, '/Script/Anastasis_UnrealV2.AnastasisWorldEmbodiment')
    found = unreal.GameplayStatics.get_all_actors_of_class(world, cls)
    if not found:
        raise RuntimeError('no embodiment actor')
    actor = found[0]
    log('EMBODY ok=%s dressing=%s' % (
        actor.call_method('EmbodyCanonical', args=(SEED,)),
        actor.call_method('GetDressingInstanceCount')))

    rows = []
    for comp in actor.get_components_by_class(
            unreal.HierarchicalInstancedStaticMeshComponent):
        mesh = comp.get_editor_property('static_mesh')
        count = comp.get_instance_count()
        if mesh is None or count == 0:
            continue
        box = mesh.get_bounding_box()
        h = box.max.z - box.min.z
        w = max(box.max.x - box.min.x, box.max.y - box.min.y)
        lo, hi = None, None
        for i in range(count):
            xf = comp.get_instance_transform(i, world_space=True)
            if xf is None:
                continue
            s = xf.scale3d.x
            lo = s if lo is None else min(lo, s)
            hi = s if hi is None else max(hi, s)
        lo = lo or 1.0
        hi = hi or 1.0
        rows.append((count, mesh.get_name(), w, h, lo, hi))

    rows.sort(reverse=True)
    log('--- INVENTAIRE (mode=%s) ---' % MODE)
    log('%-6s %-34s %-18s %s' % ('count', 'mesh', 'mesh w x h (uu)', 'taille monde h (uu)'))
    total = 0
    for count, name, w, h, lo, hi in rows:
        total += count
        log('%-6d %-34s %7.0f x %-7.0f   %6.0f .. %-6.0f' % (
            count, name, w, h, h * lo, h * hi))
    log('TOTAL instances=%d sur %d meshes' % (total, len(rows)))


main()
unreal.SystemLibrary.quit_editor()
