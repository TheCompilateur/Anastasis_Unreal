"""
VILLAGER_PNG_001 -- planche de population dans Unreal.

Repond a la question de la mission : « plusieurs habitants differents d'un meme village, ou
32 variantes du meme humain ? ». Les cartes sont les VRAIS acteurs de jeu
(AnastasisVillagerVisual, le materiau et les textures du registre), sous la lumiere neutre du
banc d'observation (Lvl_AnastasisSlice), pas une image composee hors moteur.

Les sujets sont sur un ARC centre sur la camera (meme raison que capture-tree-lineup.py) : a
distance egale, une difference de taille a l'ecran est une difference de stature, pas de
perspective. Plusieurs prises dans la meme session : la population entiere, puis par groupe
de huit, assez pres pour juger les visages. Le temoin a gauche est une barre de 180 cm.

LECTURE SEULE SUR LES ASSETS : acteurs poses hors de l'emprise du monde, demontes avant de
quitter, niveau jamais sauve.

    tools\\unreal\\villager-lineup.ps1 -Label v1
"""
import math
import os
import shutil
import time
import unreal

OUT_DIR = os.environ.get('ANASTASIS_VILLAGER_LINEUP_OUT', '')
LEVEL = '/Game/Anastasis/Maps/Lvl_AnastasisSlice'
REGISTRY = '/Game/Anastasis/Presentation/DA_AnastasisPresentation'
# En l'air, a 300 m : hors de l'emprise du monde, l'anneau d'horizon (anastasis.Terrain.Horizon) mettait
# la scene au sol dans son ombre -- sol, temoin et cartes noirs au premier banc. Meme soleil, rien au-dessus.
STAGE = unreal.Vector(-14000.0, 0.0, 30000.0)
SPACING = 75.0        # corde entre deux cartes : une epaule et demie
CAM_HEIGHT = 115.0    # a hauteur de poitrine d'adulte : ni plongee ni contre-plongee

ORDER = ['ADULT_MALE', 'ADULT_FEMALE', 'ELDER_MALE', 'ELDER_FEMALE', 'CHILD_MALE', 'CHILD_FEMALE']

eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)


def log(msg):
    unreal.log('VILLAGER_LINEUP ' + str(msg))


def category_key(look):
    """Comparaison aux membres de l'enum : le format de str() d'un enum UE n'est pas un contrat."""
    value = look.get_editor_property('category')
    for name in ORDER:
        if value == getattr(unreal.AnastasisVillagerCategory, name):
            return name
    return '?'


les.load_level(LEVEL)
world = ues.get_editor_world()
registry = unreal.EditorAssetLibrary.load_asset(REGISTRY)
looks = list(registry.get_editor_property('villagers')) if registry else []
material = registry.get_editor_property('villager_material') if registry else None
looks.sort(key=lambda l: (ORDER.index(category_key(l)) if category_key(l) in ORDER else 99, str(l.get_editor_property('look_id'))))
log('REGISTRY looks=%d material=%s' % (len(looks), material.get_path_name() if material else None))


def group(keys):
    return [l for l in looks if category_key(l) in keys]


# (nom, sujets, resolution, rayon de l'arc)
SHOTS = [('A_population', looks, '3840x2160', max(600.0, SPACING * len(looks) / math.radians(70.0)))]
for name, keys in (('B_hommes_adultes', ['ADULT_MALE']), ('C_femmes_adultes', ['ADULT_FEMALE']),
                   ('D_aines', ['ELDER_MALE', 'ELDER_FEMALE']), ('E_enfants', ['CHILD_MALE', 'CHILD_FEMALE'])):
    members = group(keys)
    if members:
        SHOTS.append((name, members, '2560x1440', max(380.0, SPACING * len(members) / math.radians(60.0))))

SPAWNED = []      # tout ce qui doit etre demonte avant de quitter
CURRENT = []      # les cartes de la prise en cours


def spawn(cls, location, label):
    actor = eas.spawn_actor_from_class(cls, location, unreal.Rotator(0, 0, 0))
    actor.set_actor_label(label)
    SPAWNED.append(actor)
    return actor


ground = spawn(unreal.StaticMeshActor, STAGE, 'VillagerLineup_Ground')
ground.static_mesh_component.set_static_mesh(unreal.EditorAssetLibrary.load_asset('/Engine/BasicShapes/Plane.Plane'))
ground.set_actor_scale3d(unreal.Vector(120.0, 120.0, 1.0))
try:
    mid = unreal.MaterialLibrary.create_dynamic_material_instance(world, unreal.EditorAssetLibrary.load_asset('/Engine/BasicShapes/BasicShapeMaterial'))
    mid.set_vector_parameter_value('Color', unreal.LinearColor(0.30, 0.26, 0.20, 1.0))
    ground.static_mesh_component.set_material(0, mid)
except Exception as exc:  # noqa: BLE001
    log('WARN sol neutre non applique: %s' % exc)


def stage(shot):
    name, members, _res, radius = shot
    for actor in CURRENT:
        try:
            eas.destroy_actor(actor)
            SPAWNED.remove(actor)
        except Exception:
            pass
    del CURRENT[:]
    cam = unreal.Vector(STAGE.x, STAGE.y + radius, STAGE.z + CAM_HEIGHT)
    n = len(members) + 1  # + le temoin
    step = 2.0 * math.degrees(math.asin(min(1.0, SPACING * 0.5 / radius)))
    first = -step * (n - 1) * 0.5
    for i in range(n):
        theta = math.radians(first + step * i)
        feet = unreal.Vector(cam.x + radius * math.sin(theta), cam.y - radius * math.cos(theta), STAGE.z)
        if i == 0:
            bar = spawn(unreal.StaticMeshActor, unreal.Vector(feet.x, feet.y, STAGE.z + 90.0), 'VillagerLineup_180cm')
            bar.static_mesh_component.set_static_mesh(unreal.EditorAssetLibrary.load_asset('/Engine/BasicShapes/Cylinder.Cylinder'))
            bar.set_actor_scale3d(unreal.Vector(0.08, 0.08, 1.8))
            CURRENT.append(bar)
            continue
        look = members[i - 1]
        card = spawn(unreal.AnastasisVillagerVisual, feet, 'VillagerLineup_' + str(look.get_editor_property('look_id')))
        ok = card.set_look(look.get_editor_property('look_id'), look.get_editor_property('portrait'), material)
        card.face_towards(cam)
        CURRENT.append(card)
        if not ok:
            unreal.log_error('VILLAGER_LINEUP set_look refuse ' + str(look.get_editor_property('look_id')))
    log('STAGE %s n=%d radius=%.0f step=%.2f deg' % (name, len(members), radius, step))
    return cam


SHOT_DIR = os.path.join(unreal.Paths.project_saved_dir(), 'Screenshots')


def newest_png(after):
    best, best_t = None, after
    for root, _dirs, files in os.walk(SHOT_DIR):
        for f in files:
            if f.lower().endswith('.png'):
                full = os.path.join(root, f)
                t = os.path.getmtime(full)
                if t > best_t:
                    best, best_t = full, t
    return best


def aim(cam):
    unreal.SystemLibrary.execute_console_command(world, 'viewmode lit')
    unreal.SystemLibrary.execute_console_command(world, 'ShowFlag.Sprites 0')
    unreal.SystemLibrary.execute_console_command(world, 'ShowFlag.Grid 0')
    unreal.SystemLibrary.execute_console_command(world, 'ShowFlag.Fog 0')
    # Le soleil fixe du banc d'observation, pas l'horloge de la simulation (heure arbitraire en editeur).
    unreal.SystemLibrary.execute_console_command(world, 'anastasis.Sky.Clock 0')
    ues.set_level_viewport_camera_info(cam, unreal.Rotator(0.0, 0.0, -90.0))
    try:
        les.editor_invalidate_viewports()
    except Exception:
        pass


state = {'i': 0, 'phase': 'stage', 't': time.monotonic(), 'mark': 0.0, 'req': 0.0, 'cam': None, 'ok': 0}
t_start = time.monotonic()
handle = None


def finish(msg, error=False):
    (unreal.log_error if error else unreal.log)(msg)
    unreal.unregister_slate_post_tick_callback(handle)
    for actor in SPAWNED:
        try:
            eas.destroy_actor(actor)
        except Exception:
            pass
    unreal.SystemLibrary.quit_editor()


def tick(dt):
    now = time.monotonic()
    if now - t_start > 120.0 + 60.0 * len(SHOTS):
        finish('VILLAGER_LINEUP::FAIL timeout shot=%d' % state['i'], True)
        return
    if state['i'] >= len(SHOTS):
        finish('VILLAGER_LINEUP::%s shots=%d/%d' % ('PASS' if state['ok'] == len(SHOTS) else 'FAIL', state['ok'], len(SHOTS)), state['ok'] != len(SHOTS))
        return
    shot = SHOTS[state['i']]
    elapsed = now - state['t']
    if state['phase'] == 'stage' and elapsed > 3.0:
        state['cam'] = stage(shot)
        aim(state['cam'])
        state['phase'], state['t'] = 'settle', now
    elif state['phase'] == 'settle' and elapsed > 4.0:
        aim(state['cam'])
        state['mark'] = time.time()
        unreal.SystemLibrary.execute_console_command(world, 'HighResShot ' + shot[2])
        state['phase'], state['t'], state['req'] = 'wait', now, now
        log('SHOT_REQUESTED ' + shot[0])
    elif state['phase'] == 'wait':
        try:
            les.editor_invalidate_viewports()
        except Exception:
            pass
        found = newest_png(state['mark'])
        if found:
            dst = os.path.join(OUT_DIR, shot[0] + '.png')
            shutil.copyfile(found, dst)
            log('SHOT_OK %s bytes=%d' % (dst, os.path.getsize(dst)))
            state['ok'] += 1
            state['i'] += 1
            state['phase'], state['t'] = 'stage', now
        elif now - state['req'] > 12.0 and elapsed < 50.0:
            aim(state['cam'])
            unreal.SystemLibrary.execute_console_command(world, 'HighResShot ' + shot[2])
            state['req'] = now
            log('SHOT_RETRY ' + shot[0])
        elif elapsed >= 50.0:
            log('SHOT_MISSING ' + shot[0])
            state['i'] += 1
            state['phase'], state['t'] = 'stage', now


if not looks or material is None:
    unreal.log_error('VILLAGER_LINEUP::FAIL registre sans population ou sans materiau (lancer import-villagers.ps1)')
    unreal.SystemLibrary.quit_editor()
else:
    handle = unreal.register_slate_post_tick_callback(tick)
