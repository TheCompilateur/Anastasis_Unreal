"""SHORELINE_FORGE_001 -- preuve visuelle A/B du bord d'eau.

POURQUOI UN SCRIPT DE PLUS. Deux chemins de capture existent deja et aucun ne
convient ici :

  probe-demo.py    capture en PIE. docs/unreal/ATMOSPHERE_002.md documente deux
                   defauts non corriges de ce chemin (la photo peut prendre
                   l'editeur au lieu du jeu ; elle peut partir en plein fondu de
                   camera). Une mission dont le verdict EST une image ne peut pas
                   s'y adosser.
  observe-slice.py capture le viewport de l'editeur -- fiable, c'est le chemin
                   des preuves scellees -- mais ne connait que deux cameras,
                   celle de la tranche et celle du monde. Aucune ne regarde une
                   rive.

Ce script reprend EXACTEMENT la scene d'observe-slice.py : le niveau committe
Lvl_AnastasisSlice, son soleil a 75 000 lux, son exposition figee a EV100 = 14.
Il n'en change QUE la camera et la CVar mesuree. Il ne cree ni ne modifie aucun
asset : si le niveau ou le materiau de tranche manquent, il refuse au lieu de les
fabriquer -- leur fabrication appartient a observe-slice.py.

Variables d'environnement :
  ANASTASIS_SHORE_SHOT   chemin PNG de sortie (requis)
  ANASTASIS_SHORE_VIEW   nom de vue dans VIEWS (defaut MID)
  ANASTASIS_SHORE_MODE   valeur de anastasis.Terrain.Shoreline : "0" nappe
                         opaque historique, "1" rive graduee (defaut 1)
"""
import os, shutil, time, unreal

SHOT = os.environ.get('ANASTASIS_SHORE_SHOT', '')
VIEW = os.environ.get('ANASTASIS_SHORE_VIEW', 'MID')
MODE = os.environ.get('ANASTASIS_SHORE_MODE', '1')
SEED = 12345
# AnastasisWorld::SeaLevel (0.275) x AnastasisWorldView::AltitudeScale (1000).
WATER_PLANE_Z = 275.0
LEVEL = '/Game/Anastasis/Maps/Lvl_AnastasisSlice'
MATERIAL = '/Game/Anastasis/Materials/M_AnastasisSlice'

# ---------------------------------------------------------------------------
# VUES DE VALIDATION.
#
# Les points vises ne sont pas choisis a l'oeil : ils sortent du marqueur
# TERRAIN_SHORELINE_SITES du test Anastasis.Terrain.Shoreline, qui parcourt le
# monde canonique 96x96 (graine 12345) et designe, PAR MESURE, une rive plate,
# une rive abrupte et une rive de chenal. Les trois familles de GATE 7 sont donc
# celles que le simulateur produit, pas celles qu'un artiste aurait aime voir.
#
# Chaque entree : (x, y, z, pitch, yaw, fov). Le repere est celui de
# AnastasisWorldView::TileToUnreal -- tuile (X,Y) au centre ((X+0.5)*100,
# (Y+0.5)*100), altitude x 1000, niveau de la mer a 275 uu.
# ---------------------------------------------------------------------------
VIEWS = {}

# Sites mesures par Anastasis.Terrain.Shoreline sur le monde canonique, graine
# 12345. RECOPIES DU MARQUEUR TERRAIN_SHORELINE_SITE, pas releves a la souris.
# Si la graine ou le generateur changent, ces points ne valent plus rien : relancer
# le test et recopier.
# Releve de TERRAIN_SHORELINE_FORGED_SITE, graine 12345, sur le maillage
# REELLEMENT RENDU (TERRAIN_FORGE, 381x381) :
#
#   TYPE_A  (6200,6350)  depth=0.538  flatness=1.000  flow=0.000   677 sommets
#   TYPE_B  (8200,1200)  depth=0.312  flatness=0.738  flow=0.945   992 sommets
#   TYPE_C  (7350,1625)  depth=0.642  flatness=0.083  flow=0.000  1751 sommets
#
# Les sites releves sur la surface TUILEE ne valent plus : la forge exagere le
# relief, et deux des trois etaient passes au-dessus du niveau de la mer -- les
# cadrer donnait une capture de coteau, pas de rive. Un site de rive se mesure sur
# le maillage qu'on photographie, pas sur celui qui le precede.
#
# La cote Z n'est pas utilisee pour la camera (cf. register_family) : seul le XY
# compte, la hauteur se prend sur le niveau de la mer.
SITES = {
    'TYPE_A': (6200.0, 6350.0, 243.0),   # rive douce, fond plat
    'TYPE_B': (8200.0, 1200.0, 256.0),   # rive de chenal, FlowAmt = 0.945
    'TYPE_C': (7350.0, 1625.0, 236.0),   # berge quasi verticale, normale Z = 0.083
}

SITES_CROP = {
    'CROP_A': (1450.0, 650.0, 274.0),
    'CROP_B': (1950.0, 2050.0, 232.0),
    'CROP_C': (50.0, 450.0, 268.0),
}


import math


def register(name, loc, pitch, yaw=45.0, fov=75.0):
    VIEWS[name] = (loc[0], loc[1], loc[2], pitch, yaw, fov)


# --- Poses.
#
# UNE SEULE pose est choisie par site : la vue MID, posee a `back` uu au sud-ouest
# du point mesure et `up` uu au-dessus. Le recul est sur la diagonale (yaw 45),
# l'axe de toutes les captures scellees du projet, donc ces images restent
# comparables a celles des autres missions.
#
# CLOSE et GAMEPLAY ne sont PAS des cadrages independants : ce sont la meme vue
# avancee le long de son propre axe de visee. Un premier essai les placait chacune
# par un recul et une hauteur propres, et la vue rapprochee tombait derriere un
# relief -- elle photographiait un talus, pas une rive. Avancer sur le rayon garde
# le meme contenu et ne change que la distance : c'est ce qu'on veut mesurer, et
# c'est aussi ce qui rend l'A/B honnete.
#
#       MID       la bande de rive dans son contexte de terrain
#       CLOSE     la meme rive, 620 uu plus pres : la coupure se voit ou pas
#       GAMEPLAY  la meme rive, ramenee a hauteur d'oeil au-dessus de l'eau
def register_family(name, site, back=900.0, up=760.0, pitch=-27.0):
    d = back * 0.70710678
    # La hauteur est reference au NIVEAU DE LA MER, pas a l'altitude de la tuile.
    # TERRAIN_FORGE exagere le relief (jusqu'a z=1667 sur ce monde) : une camera
    # posee sur l'altitude tuilee du site se retrouvait ENTERREE sous le relief
    # forge, et photographiait l'envers de la geometrie. Le plan d'eau, lui, reste
    # a SeaLevel quoi qu'il arrive -- c'est la seule cote stable a laquelle une
    # camera de rive puisse s'accrocher.
    loc = (site[0] - d, site[1] - d, WATER_PLANE_Z + up)
    register(name + '_MID', loc, pitch)

    # Avance sur le rayon de visee de MID. Aucun cadrage nouveau.
    p = math.radians(pitch)
    fwd = (math.cos(p) * 0.70710678, math.cos(p) * 0.70710678, math.sin(p))

    def advance(metres):
        return (loc[0] + fwd[0] * metres, loc[1] + fwd[1] * metres, loc[2] + fwd[2] * metres)

    register(name + '_CLOSE', advance(700.0), pitch)
    # A hauteur d'oeil : 170 uu au-dessus du NIVEAU DE LA MER, pas au-dessus du
    # fond -- c'est la hauteur qu'aurait un personnage debout au bord de l'eau.
    g = advance(380.0)
    register(name + '_GAMEPLAY', (g[0], g[1], WATER_PLANE_Z + 170.0), -9.0, fov=85.0)


for _name, _site in SITES.items():
    register_family(_name, _site)
for _name, _site in SITES_CROP.items():
    register_family(_name, _site)

# Cadrage de repli, recule et plus haut. Le recul de 900 uu a 460 uu de hauteur
# convient au site TYPE_A, qui borde un lac ouvert ; sur TYPE_B et TYPE_C, qui
# sont des tuiles de CHENAL encaissees pres du bord est du monde, la meme regle
# met la camera derriere une crete et photographie un talus. Ce n'est pas un
# defaut de la rive, c'est un defaut de la regle de cadrage : une seule regle ne
# peut pas cadrer trois topographies. Le repli monte assez haut pour qu'aucun
# relief intermediaire ne puisse masquer le site.
for _name, _site in SITES.items():
    register_family(_name + '_W', _site, back=1700.0, up=1700.0, pitch=-40.0)

VIEWS['AERIAL'] = (-5400.0, -5400.0, 10500.0, -32.8, 45.0, 90.0)


# Le viewport de l'editeur GARDE sa camera d'une session a l'autre. Une vue
# inconnue ne rendait donc pas une erreur visible : elle rendait une image, cadree
# par la session precedente, et l'A/B semblait tenir. Huit captures ont ete prises
# ainsi. Une vue inconnue doit faire echouer la capture, pas la deplacer en
# silence -- une preuve qui se trompe de camera est pire qu'une preuve absente.
def aim_pose():
    pose = VIEWS.get(VIEW)
    if pose is None:
        unreal.log_error('SHORE_VIEW_UNKNOWN %s known=%s' % (VIEW, sorted(VIEWS)))
    return pose


les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)

unreal.log('SHORE_BOOT view=%s mode=%s shot=%s' % (VIEW, MODE, SHOT))

# Lecture seule sur le contenu : ce script ne fabrique rien.
missing = [p for p in (LEVEL, MATERIAL) if not unreal.EditorAssetLibrary.does_asset_exist(p)]
if missing:
    unreal.log_error('SHORE_ASSETS_MISSING ' + repr(missing) + ' -- lancer capture-slice.ps1 d\'abord')
    unreal.SystemLibrary.quit_editor()

unreal.log('SHORE_MAP_LOAD=' + str(les.load_level(LEVEL)))
world = ues.get_editor_world()

unreal.SystemLibrary.execute_console_command(world, 'ShowFlag.Sprites 0')
unreal.SystemLibrary.execute_console_command(world, 'ShowFlag.Grid 0')
# Emprise : mode 2 (monde entier) pour les sites WORLD, mode 1 (tranche scellee)
# pour les sites CROP. Le nom de la vue porte donc l'emprise -- une capture ne peut
# pas se tromper d'emprise sans se tromper aussi de nom.
SURFACE_MODE = '1' if VIEW.startswith('CROP_') else '2'
unreal.SystemLibrary.execute_console_command(world, 'anastasis.Terrain.Surface ' + SURFACE_MODE)
unreal.log('SHORE_SURFACE_MODE=' + SURFACE_MODE)
unreal.SystemLibrary.execute_console_command(world, 'anastasis.Terrain.Shoreline ' + MODE)

cls = unreal.load_class(None, '/Script/Anastasis_UnrealV2.AnastasisWorldEmbodiment')
found = unreal.GameplayStatics.get_all_actors_of_class(world, cls)
if not found:
    unreal.log_error('SHORE_NO_EMBODIMENT dans ' + LEVEL)
    unreal.SystemLibrary.quit_editor()
actor = found[0]
unreal.log('SHORE_EMBODY=%s' % actor.call_method('EmbodyCanonical', args=(SEED,)))

# RESOLUTION DE CAPTURE = celle du viewport, deliberement.
#
# Demander 1920x1080 a un viewport lance en 1280x720 force le rendu EN TUILES.
# Sur la surface tuilee d'avant (18 050 triangles) cela passait ; sur le maillage
# de TERRAIN_FORGE (288 800 triangles) la demande ne produit AUCUN fichier et
# aucune erreur -- huit relances de suite, puis SHORE_SHOT_MISSING. C'est trait
# pour trait le KNOWN_DEBT n.1 de docs/unreal/TERRAIN_SURFACE_EXTENT.md
# (<< HighResShot ne rend rien en mode 2 >>), reste sans cause depuis. En demandant
# exactement la taille du viewport, il n'y a plus de tuilage et la capture passe.
#
# Une preuve en 1280x720 qui existe vaut mieux qu'une preuve en 1920x1080 qui
# n'est jamais ecrite.
SHOT_W, SHOT_H = 1280, 720

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
    # Le mode d'affichage du viewport persiste entre sessions : un precedent
    # "lighting only" rendrait tout en gris et la comparaison serait truquee.
    unreal.SystemLibrary.execute_console_command(world, 'viewmode lit')
    unreal.SystemLibrary.execute_console_command(world, 'ShowFlag.Sprites 0')
    unreal.SystemLibrary.execute_console_command(world, 'ShowFlag.Grid 0')
    # Le flou de mouvement n'a rien a faire dans une capture de verdict --
    # defaut 2 de ATMOSPHERE_002.
    unreal.SystemLibrary.execute_console_command(world, 'ShowFlag.MotionBlur 0')
    pose = aim_pose()
    if pose is None:
        finish('SHORE_VIEW_UNKNOWN ' + VIEW, True)
        return
    x, y, z, pitch, yaw, _fov = pose
    ues.set_level_viewport_camera_info(unreal.Vector(x, y, z), unreal.Rotator(0.0, pitch, yaw))
    loc, rot = ues.get_level_viewport_camera_info()
    unreal.log('SHORE_CAMERA_APPLIED view=%s loc=(%.0f,%.0f,%.0f) pitch=%.1f yaw=%.1f'
               % (VIEW, loc.x, loc.y, loc.z, rot.pitch, rot.yaw))


t0 = time.monotonic()
mark = time.time()
phase = 0
last_request = 0.0
handle = None


def finish(msg, error=False):
    (unreal.log_error if error else unreal.log)(msg)
    unreal.unregister_slate_post_tick_callback(handle)
    unreal.SystemLibrary.quit_editor()


def tick(dt):
    global phase, last_request
    elapsed = time.monotonic() - t0
    if phase == 0 and elapsed > 3.0:
        phase = 1
        aim()
    elif phase == 1 and elapsed > 18.0:
        # 18 s, pas 9. Le SkyLight de Lvl_AnastasisSlice est en capture temps reel :
        # tant qu'il n'a pas capture, les instances HISM rendent en gris neutre. Une
        # capture prise trop tot donne donc une image correctement cadree mais
        # SOUS-ECLAIREE -- et deux images d'un A/B qui ne partagent pas le meme
        # eclairage ne comparent plus rien. C'est arrive une fois, sur la vue
        # TYPE_B : arbres gris d'un cote, verts de l'autre. La difference mesuree
        # aurait alors ete celle du ciel, pas celle de la rive.
        # Le materiau de rive est translucide par-dessus : ses shaders compilent a
        # la premiere image et la premiere demande de capture peut se perdre.
        phase = 2
        aim()
        unreal.SystemLibrary.execute_console_command(world, 'HighResShot %dx%d' % (SHOT_W, SHOT_H))
        last_request = elapsed
        unreal.log('SHORE_SHOT_REQUESTED')
    elif phase == 2:
        found_png = newest_png(mark)
        if found_png:
            phase = 3
            shutil.copyfile(found_png, SHOT)
            unreal.log('SHORE_SHOT_OK bytes=%d' % os.path.getsize(SHOT))
            finish('SHORE_COMPLETE')
        elif elapsed - last_request > 12.0 and elapsed < 120.0:
            aim()
            unreal.SystemLibrary.execute_console_command(world, 'HighResShot %dx%d' % (SHOT_W, SHOT_H))
            last_request = elapsed
            unreal.log('SHORE_SHOT_RETRY t=%.0f' % elapsed)
        elif elapsed >= 120.0:
            finish('SHORE_SHOT_MISSING dir=' + SHOT_DIR, True)
    elif elapsed > 150.0:
        finish('SHORE_TIMEOUT phase=%d' % phase, True)


handle = unreal.register_slate_post_tick_callback(tick)
