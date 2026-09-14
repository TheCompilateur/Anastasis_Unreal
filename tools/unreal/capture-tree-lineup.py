"""
ANASTASIS_UNREAL_TREE_FORM_001 -- planche de stature.

Aligne un exemplaire de chaque silhouette de la grammaire, a la hauteur que le
monde lui donne reellement, sous la lumiere neutre du banc d'observation
(Lvl_AnastasisSlice : soleil 75000 lux, EV100 fige). C'est le TEST ANTI-ARNAQUE
de la mission : une silhouette qui ne tient pas sous un eclairage banal ne tient
pas du tout. Aucun brouillard, aucun coucher de soleil, aucun etalonnage.

Les deux temoins a droite ne sont pas decoratifs. Ils repondent a la seule
question qui compte pour une stature : PAR RAPPORT A QUOI. Le fut de ruine est
le seul autre objet construit que ce monde contienne aujourd'hui, et la barre de
180 uu est la taille d'un adulte -- echelle joueur non encore choisie par le
projet, posee ici comme reperage, pas comme decision.

LECTURE SEULE SUR LES ASSETS. Le script charge Lvl_AnastasisSlice, y pose des
acteurs TRANSIENTS hors de l'emprise du terrain, capture, et ne sauve JAMAIS le
niveau. Rien de ce qu'il fabrique n'atteint le disque a part le PNG.

    tools\\unreal\\capture-tree-lineup.ps1 -Out B_stature_board.png
"""
import math
import os
import shutil
import time
import unreal

SHOT = os.environ.get('ANASTASIS_LINEUP_SHOT', '')
LEVEL = '/Game/Anastasis/Maps/Lvl_AnastasisSlice'
MESH_DIR = '/Game/Anastasis/Vegetation/'
MATERIAL = '/Game/Anastasis/Materials/M_AnastasisVegetation'

# Scene montee loin de l'emprise du monde ([0,9600]^2) : le terrain reste charge
# mais hors champ, et on n'a pas a le modifier pour l'ecarter.
STAGE = unreal.Vector(-14000.0, 0.0, 0.0)

# LES SUJETS SONT SUR UN ARC, PAS SUR UNE LIGNE.
#
# Une planche de stature ne vaut que si les hauteurs se comparent. Alignes en
# rang, les sujets des extremites sont plus LOIN de la camera que celui du
# milieu, et la perspective les rapetisse : a 1250 uu de recul pour un rang de
# 2100, le sujet du bord perd 23% de sa taille apparente. La planche mentirait
# alors exactement sur ce qu'elle pretend montrer.
#
# Sur un arc centre sur la camera, tous sont a la MEME distance. La seule
# difference de taille a l'ecran est la difference de taille reelle.
ARC_RADIUS = 2000.0
ARC_SPACING = 340.0   # corde entre deux sujets : plus large que la plus large couronne
CAM_HEIGHT = 320.0    # a mi-hauteur de l'emergent, pour ne privilegier ni le haut ni le bas

# (mesh, hauteur en uu, etiquette). Les hauteurs sont la MEDIANE que chaque
# stature recoit dans le monde : enveloppe d'entree 3.6-5.0 au milieu (4.3),
# multipliee par le facteur de strate ecologique, puis par le biais de variante.
# Ce ne sont donc pas des tailles choisies pour la photo.
SUBJECTS = [
    ('SM_Tree_Conifer_Understory_01', 150.0, 'understory 1.5 m'),
    ('SM_Tree_Broadleaf_Subcanopy_01', 253.0, 'sous-canopee feuillue 2.5 m'),
    ('SM_Tree_Conifer_Subcanopy_01', 275.0, 'sous-canopee conifere 2.8 m'),
    ('SM_Tree_Broadleaf_Canopy_01', 393.0, 'canopee feuillue 3.9 m'),
    ('SM_Tree_Conifer_Canopy_01', 462.0, 'canopee conifere 4.6 m'),
    ('SM_Tree_Conifer_Emergent_01', 624.0, 'emergent 6.2 m'),
]

# Temoins d'echelle : la ruine telle qu'elle est reellement posee (mediane
# mesuree 91 uu) et un adulte de 1.80 m.
WITNESSES = [
    ('/Engine/BasicShapes/Cylinder.Cylinder', 91.0, 0.6, 'ruine 0.9 m'),
    ('/Engine/BasicShapes/Cylinder.Cylinder', 180.0, 0.22, 'adulte 1.8 m'),
]

eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)


def log(msg):
    unreal.log('LINEUP ' + str(msg))


SPAWNED = []


def spawn_mesh(mesh, location, scale, label):
    actor = eas.spawn_actor_from_class(unreal.StaticMeshActor, location, unreal.Rotator(0, 0, 0))
    SPAWNED.append(actor)
    actor.set_actor_label(label)
    # Pas de RF_TRANSIENT : UE-Python n'expose pas set_flags sur un acteur. La
    # garantie que rien n'atteint le disque tient a ce que ce script n'appelle
    # JAMAIS save_current_level -- c'est la seule chose qui figerait ces acteurs.
    comp = actor.static_mesh_component
    comp.set_static_mesh(mesh)
    comp.set_mobility(unreal.ComponentMobility.MOVABLE)
    actor.set_actor_scale3d(scale)
    return actor


les.load_level(LEVEL)
world = ues.get_editor_world()
material = unreal.EditorAssetLibrary.load_asset(MATERIAL)

# Camera d'abord : c'est elle qui definit l'arc, pas l'inverse. Elle regarde -Y,
# donc son vecteur droite est +X et l'angle croissant va vers la droite de l'image.
CAM_LOC = unreal.Vector(STAGE.x, STAGE.y + ARC_RADIUS, CAM_HEIGHT)
CAM_ROT = unreal.Rotator(0.0, 0.0, -90.0)

ROW = [(MESH_DIR + n, h, None, l) for n, h, l in SUBJECTS]
ROW = [(p, h, r, l) for p, h, r, l in WITNESSES] + ROW  # temoins a gauche, puis du plus petit au plus grand

step = 2.0 * math.degrees(math.asin(min(1.0, ARC_SPACING * 0.5 / ARC_RADIUS)))
first = -step * (len(ROW) - 1) * 0.5
log('ARC radius=%.0f step=%.2f deg spread=%.1f deg' % (ARC_RADIUS, step, step * (len(ROW) - 1)))

# Sol neutre : une dalle, pas le terrain du monde. On juge la silhouette, pas le
# biome qu'elle traverse.
ground = spawn_mesh(
    unreal.EditorAssetLibrary.load_asset('/Engine/BasicShapes/Plane.Plane'),
    unreal.Vector(STAGE.x, STAGE.y, 0.0), unreal.Vector(60.0, 60.0, 1.0), 'Lineup_Ground')
try:
    grey = unreal.EditorAssetLibrary.load_asset('/Engine/BasicShapes/BasicShapeMaterial')
    mid = unreal.MaterialLibrary.create_dynamic_material_instance(world, grey)
    mid.set_vector_parameter_value('Color', unreal.LinearColor(0.20, 0.20, 0.19, 1.0))
    ground.static_mesh_component.set_material(0, mid)
except Exception as exc:  # noqa: BLE001 -- la dalle par defaut reste lisible
    log('WARN sol neutre non applique: %s' % exc)

for index, (path, height_uu, radius_scale, label) in enumerate(ROW):
    mesh = unreal.EditorAssetLibrary.load_asset(path)
    if mesh is None:
        unreal.log_error('LINEUP mesh introuvable ' + path)
        continue
    theta = math.radians(first + step * index)
    location = unreal.Vector(
        CAM_LOC.x + ARC_RADIUS * math.sin(theta),
        CAM_LOC.y - ARC_RADIUS * math.cos(theta),
        height_uu * 0.5)
    if radius_scale is None:
        # Le mesh couvre Z = [-50, +50] : sa base se pose a height/2 au-dessus du
        # sol, exactement le lift que le monde applique.
        scale = unreal.Vector(height_uu / 100.0, height_uu / 100.0, height_uu / 100.0)
    else:
        scale = unreal.Vector(radius_scale, radius_scale, height_uu / 100.0)
    actor = spawn_mesh(mesh, location, scale, label)
    if radius_scale is None and material is not None:
        actor.static_mesh_component.set_material(0, material)
    box = mesh.get_bounding_box()
    log('%-30s height=%.0f uu bounds_z=[%.1f,%.1f] theta=%+.1f deg'
        % (label, (box.max.z - box.min.z) * scale.z, box.min.z, box.max.z, math.degrees(theta)))

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


def aim():
    unreal.SystemLibrary.execute_console_command(world, 'viewmode lit')
    unreal.SystemLibrary.execute_console_command(world, 'ShowFlag.Sprites 0')
    unreal.SystemLibrary.execute_console_command(world, 'ShowFlag.Grid 0')
    ues.set_level_viewport_camera_info(CAM_LOC, CAM_ROT)
    redraw()


def redraw():
    """HighResShot est servi par le viewport, au prochain rendu.

    observe-slice.py n'a pas besoin de ca : son embodiment reconstruit la scene
    et la salit en continu, donc le viewport redessine tout seul. Ici la scene
    est statique des qu'elle est posee, le viewport n'a aucune raison de
    redessiner, et la capture demandee reste en attente pour toujours -- huit
    HighResShot acceptes, zero PNG. On invalide donc explicitement.
    """
    try:
        les.editor_invalidate_viewports()
    except Exception:
        pass


t0 = time.monotonic()
mark = time.time()
phase = 0
last_request = 0.0
handle = None


def finish(msg, error=False):
    (unreal.log_error if error else unreal.log)(msg)
    unreal.unregister_slate_post_tick_callback(handle)
    # Demonter la scene avant de quitter, et pas seulement s'abstenir de sauver.
    #
    # Quitter en laissant le niveau sale ecrit Saved/Autosaves/PackageRestoreData.json,
    # et l'editeur suivant ouvre alors une fenetre modale de recuperation de paquets
    # AVANT d'executer -ExecCmds. Le lancement d'apres ne fait donc rien du tout et
    # expire au bout du timeout. Observe une fois : c'est cette ligne qui l'empeche.
    for actor in SPAWNED:
        try:
            eas.destroy_actor(actor)
        except Exception:
            pass
    unreal.SystemLibrary.quit_editor()


def tick(dt):
    global phase, last_request
    elapsed = time.monotonic() - t0
    if phase == 0 and elapsed > 3.0:
        phase = 1
        aim()
    elif phase == 1 and elapsed > 7.0:
        phase = 2
        aim()
        unreal.SystemLibrary.execute_console_command(world, 'HighResShot 1920x1080')
        last_request = elapsed
        log('SHOT_REQUESTED')
    elif phase == 2:
        redraw()
        found = newest_png(mark)
        if found:
            phase = 3
            shutil.copyfile(found, SHOT)
            log('SHOT_OK bytes=%d' % os.path.getsize(SHOT))
            finish('LINEUP_COMPLETE')
        elif elapsed - last_request > 12.0 and elapsed < 100.0:
            aim()
            unreal.SystemLibrary.execute_console_command(world, 'HighResShot 1920x1080')
            last_request = elapsed
            log('SHOT_RETRY t=%.0f' % elapsed)
        elif elapsed >= 100.0:
            finish('LINEUP_SHOT_MISSING dir=' + SHOT_DIR, True)
    elif elapsed > 130.0:
        finish('LINEUP_TIMEOUT phase=%d' % phase, True)


handle = unreal.register_slate_post_tick_callback(tick)
