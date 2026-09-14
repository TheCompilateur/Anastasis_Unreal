"""
Planche de presentation de RUIN_GRAMMAR_V1 : les 18 vestiges alignes, meme lumiere.

Sert a juger les FORMES sans que le terrain, la distribution ou l'atmosphere ne s'en
melent -- l'equivalent pour les ruines de ce que capture-tree-lineup.py fait pour les
arbres. Le jugement en situation, lui, se fait dans le monde, pas ici.

Scene jetable montee en memoire dans un niveau vide : rien n'est sauvegarde, aucun asset
n'est cree ni modifie.

Env :
  ANASTASIS_LINEUP_SHOTDIR   dossier de sortie (defaut Saved/RuinEvidence)
  ANASTASIS_LINEUP_DIR       dossier des meshes (defaut /Game/Anastasis/Architecture)
  ANASTASIS_LINEUP_PREFIX    prefixe des meshes a aligner (defaut SM_Ruin_)
"""
import json
import math
import os
import shutil
import time

import unreal

OUT_DIR = os.environ.get(
    'ANASTASIS_LINEUP_SHOTDIR',
    os.path.join(unreal.Paths.project_saved_dir(), 'RuinEvidence'))
MESH_DIR = os.environ.get('ANASTASIS_LINEUP_DIR', '/Game/Anastasis/Architecture')
PREFIX = os.environ.get('ANASTASIS_LINEUP_PREFIX', 'SM_Ruin_')

# Le rig lumineux fige du projet, pour que la planche soit comparable aux captures monde.
SUN_LUX = 75000.0
EV100 = 14.0
SPACING = 900.0   # UU entre deux pieces : la plus large fait ~780
COLS = 6

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def log(msg):
    unreal.log('RUIN_LINEUP ' + str(msg))


def spawn(class_path, loc, rot):
    return eas.spawn_actor_from_class(unreal.load_class(None, class_path), loc, rot)


def main():
    names = []
    for path in sorted(unreal.EditorAssetLibrary.list_assets(MESH_DIR, recursive=False)):
        asset = unreal.load_asset(path)
        if isinstance(asset, unreal.StaticMesh) and asset.get_name().startswith(PREFIX):
            names.append(asset)
    if not names:
        raise RuntimeError('aucun mesh %s* dans %s' % (PREFIX, MESH_DIR))
    log('MESHES n=%d' % len(names))

    les.new_level('/Temp/RuinLineup')
    world = ues.get_editor_world()

    # Soleil RASANT, pas le -38 du rig monde. Ces pieces font 11 a 148 uu de haut : a
    # -38 degres elles ne projettent presque rien et la planche ne montre que du gris.
    # A -17 chaque assise porte son ombre, et c'est l'ombre qui rend l'assise lisible.
    sun = spawn('/Script/Engine.DirectionalLight', unreal.Vector(0, 0, 4000),
                unreal.Rotator(0.0, -17.0, -50.0))
    comp = sun.get_component_by_class(unreal.DirectionalLightComponent)
    comp.set_intensity(SUN_LUX)
    comp.set_editor_property('atmosphere_sun_light', True)
    spawn('/Script/Engine.SkyAtmosphere', unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0))
    sky = spawn('/Script/Engine.SkyLight', unreal.Vector(0, 0, 2000), unreal.Rotator(0, 0, 0))
    sky.get_component_by_class(unreal.SkyLightComponent).set_editor_property(
        'real_time_capture', True)

    ppv = spawn('/Script/Engine.PostProcessVolume', unreal.Vector(0, 0, 500),
                unreal.Rotator(0, 0, 0))
    ppv.set_editor_property('unbound', True)
    st = ppv.get_editor_property('settings')
    st.set_editor_property('override_auto_exposure_method', True)
    st.set_editor_property('auto_exposure_method', unreal.AutoExposureMethod.AEM_HISTOGRAM)
    st.set_editor_property('override_auto_exposure_min_brightness', True)
    st.set_editor_property('auto_exposure_min_brightness', EV100)
    st.set_editor_property('override_auto_exposure_max_brightness', True)
    st.set_editor_property('auto_exposure_max_brightness', EV100)
    ppv.set_editor_property('settings', st)

    # Un sol neutre : les pieces sont parkees avec leur plan de contact a Z = -50, donc
    # le sol se pose a -50 pour que la partie enfouie le soit vraiment.
    rows = int(math.ceil(len(names) / float(COLS)))
    floor = spawn('/Script/Engine.StaticMeshActor', unreal.Vector(0, 0, -50.0),
                  unreal.Rotator(0, 0, 0))
    fmesh = unreal.load_asset('/Engine/BasicShapes/Cube')
    fc = floor.static_mesh_component
    fc.set_static_mesh(fmesh)
    fc.set_world_scale3d(unreal.Vector(COLS * SPACING / 100.0 + 4.0,
                                       rows * SPACING / 100.0 + 4.0, 0.02))

    placed = []
    for i, mesh in enumerate(names):
        col, row = i % COLS, i // COLS
        x = (col - (COLS - 1) * 0.5) * SPACING
        y = (row - (rows - 1) * 0.5) * SPACING
        # +50 : meme lift constant que AnastasisPresentation::ResolveInstanceTransform,
        # donc la piece repose ici exactement comme elle reposera dans le monde.
        actor = spawn('/Script/Engine.StaticMeshActor', unreal.Vector(x, y, 0.0),
                      unreal.Rotator(0, 0, 0))
        actor.static_mesh_component.set_static_mesh(mesh)
        actor.set_actor_label(mesh.get_name())
        placed.append({'mesh': mesh.get_name(), 'x': x, 'y': y})
        log('  %-30s col=%d row=%d' % (mesh.get_name(), col, row))

    # Cadrage calcule sur l'emprise REELLE de la grille, pas sur un "span" arbitraire :
    # la premiere planche cadrait 5400 uu depuis 5100 de haut et les pieces faisaient
    # trois pixels.
    grid_w = COLS * SPACING
    grid_d = rows * SPACING
    # Champ horizontal de 90 degres : la moitie de la largeur doit tenir sous la
    # diagonale, d'ou la hauteur ~ demi-largeur, avec un peu de marge.
    top_h = max(grid_w, grid_d) * 0.62
    VIEWS = [
        ('RUIN_LINEUP_TOP',
         unreal.Vector(-grid_w * 0.34, -grid_d * 0.34, top_h),
         unreal.Rotator(0.0, -50.0, 45.0)),
        # Deux demi-planches vues de pres : c'est la qu'on juge les assises.
        ('RUIN_LINEUP_ROW_A',
         unreal.Vector(-grid_w * 0.30, -grid_d * 0.75, SPACING * 0.62),
         unreal.Rotator(0.0, -19.0, 52.0)),
        ('RUIN_LINEUP_ROW_B',
         unreal.Vector(grid_w * 0.22, -grid_d * 0.72, SPACING * 0.58),
         unreal.Rotator(0.0, -17.0, 108.0)),
    ]

    if not os.path.isdir(OUT_DIR):
        os.makedirs(OUT_DIR)
    shot_dir = os.path.join(unreal.Paths.project_saved_dir(), 'Screenshots')
    results = []
    state = {'job': -1, 'phase': 'next', 'mark': 0.0, 't': time.monotonic(), 'h': None}

    def newest(after):
        best, bt = None, after
        for root, _d, files in os.walk(shot_dir):
            for f in files:
                if f.lower().endswith('.png'):
                    full = os.path.join(root, f)
                    t = os.path.getmtime(full)
                    if t > bt:
                        best, bt = full, t
        return best

    def aim(loc, rot):
        unreal.SystemLibrary.execute_console_command(world, 'viewmode lit')
        unreal.SystemLibrary.execute_console_command(world, 'ShowFlag.Sprites 0')
        unreal.SystemLibrary.execute_console_command(world, 'ShowFlag.Grid 0')
        ues.set_level_viewport_camera_info(loc, rot)

    def tick(dt):
        now = time.monotonic()
        el = now - state['t']
        if state['phase'] == 'next':
            state['job'] += 1
            if state['job'] >= len(VIEWS):
                with open(os.path.join(OUT_DIR, 'ruin_lineup.json'), 'w') as fh:
                    json.dump({'placed': placed, 'shots': results}, fh, indent=2)
                log('COMPLETE shots=%d' % len(results))
                unreal.unregister_slate_post_tick_callback(state['h'])
                unreal.SystemLibrary.quit_editor()
                return
            name, loc, rot = VIEWS[state['job']]
            aim(loc, rot)
            log('AIMING ' + name)
            state['phase'], state['t'] = 'settle', now
        elif state['phase'] == 'settle' and el > 3.0:
            name, loc, rot = VIEWS[state['job']]
            aim(loc, rot)
            state['mark'] = time.time()
            unreal.SystemLibrary.execute_console_command(world, 'HighResShot 1920x1080')
            state['phase'], state['t'] = 'wait', now
        elif state['phase'] == 'wait':
            png = newest(state['mark'])
            if png:
                name, _l, _r = VIEWS[state['job']]
                dest = os.path.join(OUT_DIR, name + '.png')
                shutil.copyfile(png, dest)
                results.append(name)
                log('SHOT_OK %s bytes=%d' % (name, os.path.getsize(dest)))
                state['phase'], state['t'] = 'next', now
            elif el > 15.0:
                name, loc, rot = VIEWS[state['job']]
                aim(loc, rot)
                state['mark'] = time.time()
                unreal.SystemLibrary.execute_console_command(world, 'HighResShot 1920x1080')
                state['t'] = now

    state['h'] = unreal.register_slate_post_tick_callback(tick)


main()
