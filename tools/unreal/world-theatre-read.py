"""WORLD_THEATRE_001 -- releve du monde reellement execute, en PIE.

Lance PIE sur Lvl_AnastasisSlice (le monde du jeu : incarnation de BeginPlay, village de depart),
attend que l'incarnation et le village soient poses, puis `anastasis.Theatre.Read <dossier>` :
sol et eau rendus (carte + anneau d'horizon) rasterises, objets poses classes par famille.
Rien n'est modifie ni sauve. L'analyse se fait ensuite HORS moteur (world-theatre-analyze.py).

Lancement : `py tools/unreal/world-theatre-read.py` dans un editeur ouvert, ou au lot (`world-theatre-read`).
  ANASTASIS_THEATRE_OUT      dossier de sortie (defaut Saved/WorldTheatreEvidence/read)
  ANASTASIS_THEATRE_SETTLE   secondes reelles d'attente en PIE avant le releve (defaut 25)
Verdict : WORLD_THEATRE_READ_PIE PASS si le releve est ecrit (reading.json present), FAIL sinon.
"""
import os, time, unreal

LEVEL = '/Game/Anastasis/Maps/Lvl_AnastasisSlice'
OUT = os.environ.get('ANASTASIS_THEATRE_OUT') or os.path.join(
    unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), 'WorldTheatreEvidence', 'read')
SETTLE = float(os.environ.get('ANASTASIS_THEATRE_SETTLE', '25'))
LIMIT = SETTLE + 240.0

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
unreal.log('WORLD_THEATRE_READ_MAP_LOAD=' + str(les.load_level(LEVEL)))
t0 = time.monotonic()
state = {'phase': 0, 'started': 0.0, 'ok': False}
handle = None


def finish(msg, error=False):
    (unreal.log_error if error else unreal.log)(msg)
    unreal.unregister_slate_post_tick_callback(handle)
    unreal.SystemLibrary.quit_editor()


def tick(_dt):
    now = time.monotonic()
    phase = state['phase']
    if phase == 0 and now - t0 > 3.0:
        state['phase'] = 1
        les.editor_request_begin_play()
    elif phase == 1 and les.is_in_play_in_editor():
        state['phase'], state['started'] = 2, now
        unreal.log('WORLD_THEATRE_READ_PIE_ACTIVE settle=%g' % SETTLE)
    elif phase == 2 and now - state['started'] >= SETTLE:
        world = ues.get_game_world()
        os.makedirs(OUT, exist_ok=True)
        marker = os.path.join(OUT, 'reading.json')
        if os.path.exists(marker):
            os.remove(marker)
        unreal.SystemLibrary.execute_console_command(world, 'anastasis.Theatre.Read %s' % OUT.replace('\\', '/'))
        state['ok'] = os.path.exists(marker)
        unreal.log('WORLD_THEATRE_READ_PIE %s out=%s' % ('PASS' if state['ok'] else 'FAIL', OUT))
        state['phase'] = 3
        les.editor_request_end_play()
    elif phase == 3 and not les.is_in_play_in_editor():
        finish('WORLD_THEATRE_READ_COMPLETE')
        return
    if now - t0 > LIMIT:
        if les.is_in_play_in_editor():
            les.editor_request_end_play()
        finish('WORLD_THEATRE_READ_PIE FAIL timeout phase=%d' % state['phase'], True)


handle = unreal.register_slate_post_tick_callback(tick)
