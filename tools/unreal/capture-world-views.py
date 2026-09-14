"""
Vues du MONDE ENTIER (96x96), pas de la tranche 32x32.

tools/unreal/capture-rock-views.py cadre le voisinage rocheux le plus dense : c'est ce
qu'il faut pour juger une grammaire de roche, et c'est inutilisable pour juger le monde.
Ce script-ci recule et montre la carte : meme niveau, meme rig lumineux fige (75000 lux,
EV100 14), meme seed canonique, mais l'emprise complete et quatre distances.

Lecture seule vis-a-vis de tout asset committe : il charge Lvl_AnastasisSlice, ne le
sauvegarde jamais, et ne cree aucun asset. Il ne touche pas non plus au registre de
presentation -- ce qui est rendu ici est exactement ce que le data asset livre.

Env :
  ANASTASIS_WORLD_SHOTDIR   dossier de sortie (defaut Saved/WorldEvidence)
  ANASTASIS_WORLD_MODE      anastasis.Terrain.Surface, defaut "2" (monde 96x96)

Lance par tools/unreal/capture-world-views.ps1.
"""
import json
import math
import os
import shutil
import time

import unreal

SEED = 12345
LEVEL = '/Game/Anastasis/Maps/Lvl_AnastasisSlice'
MODE = os.environ.get('ANASTASIS_WORLD_MODE', '2')
OUT_DIR = os.environ.get(
    'ANASTASIS_WORLD_SHOTDIR',
    os.path.join(unreal.Paths.project_saved_dir(), 'WorldEvidence'))

# 96 tuiles de 100 UU : le monde occupe [0, 9600] sur X et Y. AnastasisWorldView place
# le niveau de la mer a 275 UU ; on vise legerement au-dessus pour que l'eau ne mange
# pas le centre de l'image.
#
# Le MODE change l'emprise reellement rendue, donc le cadrage doit suivre : en mode 1
# seule la tranche scellee 32x32 ([0, 3200]) porte une surface, et viser le centre du
# monde 96x96 cadrerait alors du vide. Le mode 1 est aussi le repli quand la machine ne
# peut pas incarner 96x96 -- trois fois moins de terrain et beaucoup moins d'instances
# posees, donc une capture qui aboutit au lieu de mourir en "editor hung".
WORLD_SPAN = 3200.0 if MODE == '1' else 9600.0
CENTER = (WORLD_SPAN * 0.5, WORLD_SPAN * 0.5, 450.0)
SCALE = WORLD_SPAN / 9600.0

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)


def log(msg):
    unreal.log('WORLD_VIEW ' + str(msg))


def err(msg):
    unreal.log_error('WORLD_VIEW ' + str(msg))


log('BOOT mode=%s out=%s' % (MODE, OUT_DIR))
if not os.path.isdir(OUT_DIR):
    os.makedirs(OUT_DIR)

log('MAP_LOAD=' + str(les.load_level(LEVEL)))
world = ues.get_editor_world()

for cmd in ('ShowFlag.Sprites 0', 'ShowFlag.Grid 0', 'viewmode lit',
            'anastasis.Terrain.Surface ' + MODE):
    unreal.SystemLibrary.execute_console_command(world, cmd)

emb_cls = unreal.load_class(None, '/Script/Anastasis_UnrealV2.AnastasisWorldEmbodiment')
found = unreal.GameplayStatics.get_all_actors_of_class(world, emb_cls)
if not found:
    raise RuntimeError('no AAnastasisWorldEmbodiment in ' + LEVEL)
embodiment = found[0]

try:
    ok = embodiment.call_method('EmbodyCanonical', args=(SEED,))
    log('EMBODY ok=%s dressing=%s' % (ok, embodiment.call_method('GetDressingInstanceCount')))
except Exception as exc:  # noqa: BLE001
    err('EMBODY_FAILED %s' % exc)
    unreal.SystemLibrary.quit_editor()
    raise


def rig(dist, height, pitch, yaw_deg=45.0):
    """Distances exprimees pour le monde 96x96, remises a l'echelle de l'emprise rendue.

    Les quatre vues gardent ainsi le meme cadrage relatif quel que soit le mode : la
    vue carte cadre la carte, la vue ras du sol reste au ras du sol.
    """
    yaw = math.radians(yaw_deg)
    d = dist * SCALE
    h = height * SCALE
    loc = unreal.Vector(CENTER[0] - math.cos(yaw) * d,
                        CENTER[1] - math.sin(yaw) * d,
                        CENTER[2] + h)
    return loc, unreal.Rotator(0.0, pitch, yaw_deg)


# Quatre distances sur la MEME diagonale (yaw 45) : la carte entiere, puis on descend.
# Garder un seul azimut rend les quatre images comparables entre elles.
VIEWS = [
    ('01_MAP', rig(5200.0, 10500.0, -62.0)),     # la carte entiere, presque a plomb
    ('02_OVERVIEW', rig(7600.0, 5200.0, -33.0)),  # trois quarts : le relief se lit
    ('03_REGION', rig(4200.0, 1900.0, -22.0)),    # une region, echelle des masses
    ('04_VISTA', rig(2600.0, 520.0, -7.0)),       # au ras du sol : ce que verrait un joueur
]

SHOT_DIR = os.path.join(unreal.Paths.project_saved_dir(), 'Screenshots')
RESULTS = []


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


def aim_at(loc, rot):
    unreal.SystemLibrary.execute_console_command(world, 'viewmode lit')
    unreal.SystemLibrary.execute_console_command(world, 'ShowFlag.Sprites 0')
    unreal.SystemLibrary.execute_console_command(world, 'ShowFlag.Grid 0')
    ues.set_level_viewport_camera_info(loc, rot)


job = -1
state = 'next'
mark = 0.0
t_state = time.monotonic()
current = None
handle = None


def finish(msg, error=False):
    (err if error else log)(msg)
    with open(os.path.join(OUT_DIR, 'world_views.json'), 'w') as fh:
        json.dump({'center': CENTER, 'mode': MODE, 'results': RESULTS}, fh, indent=2)
    unreal.unregister_slate_post_tick_callback(handle)
    unreal.SystemLibrary.quit_editor()


def tick(dt):
    global job, state, mark, t_state, current
    now = time.monotonic()
    elapsed = now - t_state

    if state == 'next':
        job += 1
        if job >= len(VIEWS):
            finish('COMPLETE shots=%d' % len(RESULTS))
            return
        current = VIEWS[job]
        name, (loc, rot) = current
        aim_at(loc, rot)
        log('AIMING %s loc=(%.0f,%.0f,%.0f) pitch=%.0f' % (name, loc.x, loc.y, loc.z, rot.pitch))
        state, t_state = 'settle', now
        return

    if state == 'settle' and elapsed > 3.0:
        name, (loc, rot) = current
        aim_at(loc, rot)
        mark = time.time()
        unreal.SystemLibrary.execute_console_command(world, 'HighResShot 1920x1080')
        log('SHOT_REQUESTED ' + name)
        state, t_state = 'wait', now
        return

    if state == 'wait':
        png = newest_png(mark)
        if png:
            name, _ = current
            dest = os.path.join(OUT_DIR, '%s.png' % name)
            shutil.copyfile(png, dest)
            RESULTS.append({'view': name, 'file': os.path.basename(dest),
                            'bytes': os.path.getsize(dest)})
            log('SHOT_OK %s bytes=%d' % (name, os.path.getsize(dest)))
            state, t_state = 'next', now
        elif elapsed > 15.0:
            name, (loc, rot) = current
            aim_at(loc, rot)
            mark = time.time()
            unreal.SystemLibrary.execute_console_command(world, 'HighResShot 1920x1080')
            log('SHOT_RETRY ' + name)
            t_state = now
        return


handle = unreal.register_slate_post_tick_callback(tick)
