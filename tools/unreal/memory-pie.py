"""Preuve PIE de la memoire et des decisions (memoire-decisions-001).

Lance PIE sur Lvl_AnastasisSlice, attend les fondateurs de Valmire (anastasis.Village.Founders), fait arriver
le joueur (`Anastasis.Player.Arrive` : c'est ce que la preuve mesure, le carnet est le sien), saute trente
jours par pas de six heures (`Anastasis.Sim.Advance 6h`, le joueur boit, mange ou va parler entre deux), puis ecrit la chronique et le carnet dans Saved/Chronicle/.

Verdict MEMORY_PIE PASS si :
  - les souvenirs circulent (des histoires racontees de bouche en bouche dans la chronique) ;
  - une famille sans maison a decide de batir, et l'on a demande de l'aide (oui ou non, avec la raison) ;
  - le carnet du joueur a note au moins une chose entendue, et le joueur est vivant au bout des trente jours ;
  - les deux fichiers sont ecrits.
Ce n'est pas un verdict sur l'interet du recit : celui-la, Alexandre le donne en le lisant.

Lancement : `py tools/unreal/memory-pie.py` dans un editeur ouvert, ou au lot (editor-batch, `memory-pie`). Rien n'est sauve.
  ANASTASIS_MEMORY_DAYS     jours a sauter (defaut 30)
  ANASTASIS_MEMORY_SECONDS  plafond en secondes REELLES (defaut 300)
"""
import json, os, time, unreal

LEVEL = '/Game/Anastasis/Maps/Lvl_AnastasisSlice'
DAYS = int(os.environ.get('ANASTASIS_MEMORY_DAYS', '30'))
SECONDS = float(os.environ.get('ANASTASIS_MEMORY_SECONDS', '300'))

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
lib = unreal.AnastasisSimulationDebugLibrary
unreal.log('MEMORY_PIE_MAP_LOAD=' + str(les.load_level(LEVEL)))

t0 = time.monotonic()
phase, handle = 0, None


def status(raw):
    try:
        return json.loads(raw) if raw else {}
    except ValueError:
        return {}


def finish(msg, error=False):
    (unreal.log_error if error else unreal.log)(msg)
    unreal.unregister_slate_post_tick_callback(handle)
    unreal.SystemLibrary.quit_editor()


def verdict(world):
    chronicle = status(lib.get_chronicle_status(world))
    notebook = status(lib.get_notebook_status(world))
    chronicle_path = lib.write_chronicle(world, 'memoire-pie-%d-jours.txt' % DAYS)
    notebook_path = lib.write_notebook(world, 'carnet-pie-%d-jours.txt' % DAYS)
    kinds = chronicle.get('kinds', {})
    checks = {
        'rumors': kinds.get('Rumor', 0) >= 1,
        'house_decided': kinds.get('HouseDecided', 0) >= 1,
        'help_asked': kinds.get('HelpGiven', 0) + kinds.get('HelpRefused', 0) >= 1,
        'notebook': notebook.get('notes', 0) >= 1,
        # Le carnet est celui d'un vivant : le joueur passe les trente jours (il dort, boit et mange quand il le faut).
        'player_alive': bool(status(lib.get_player_status(world)).get('player')),
        'files': bool(chronicle_path) and os.path.isfile(chronicle_path) and bool(notebook_path) and os.path.isfile(notebook_path),
    }
    ok = all(checks.values())
    unreal.log('MEMORY_PIE_CHRONICLE ' + json.dumps(chronicle, ensure_ascii=False))
    unreal.log('MEMORY_PIE_NOTEBOOK ' + json.dumps(notebook, ensure_ascii=False))
    unreal.log('MEMORY_PIE %s rumors=%s legends=%s houses=%s help=%s/%s notes=%s checks=%s' % (
        'PASS' if ok else 'FAIL', kinds.get('Rumor'), kinds.get('Legend'), kinds.get('HouseDecided'),
        kinds.get('HelpGiven'), kinds.get('HelpRefused'), notebook.get('notes'),
        ','.join('%s=%s' % (k, int(v)) for k, v in checks.items())))
    if notebook_path and os.path.isfile(notebook_path):
        with open(notebook_path, encoding='utf-8') as f:
            for line in f.read().splitlines()[:30]:
                unreal.log('MEMORY_PIE_CARNET ' + line)


def live(world):
    """Le joueur vit en humain attentif : son corps ne choisit pas a sa place (ecart n°21), alors toutes les
    six heures la preuve choisit pour lui -- dormir s'il est epuise, boire s'il a soif, manger s'il a faim,
    sinon aller parler aux autres, la ou se racontent les histoires que son carnet retient. Sans dormir, epuise,
    il refusait tout autre but (« le corps passe devant ») et mourait de faim au jour 8 (player-goal-stall-001)."""
    for _ in range(DAYS * 4):
        player = status(lib.get_player_status(world))
        if player.get('body'):
            goal = player['body']
        elif player.get('energy', 100) <= 25:
            goal = 'rest'
        elif player.get('thirst', 0) >= 40:
            goal = 'drink'
        elif player.get('hunger', 0) >= 40:
            goal = 'eat'
        else:
            goal = 'socialize'
        unreal.SystemLibrary.execute_console_command(world, 'Anastasis.Player.Goal ' + goal)
        unreal.SystemLibrary.execute_console_command(world, 'Anastasis.Sim.Advance 6h')
    player = status(lib.get_player_status(world))
    unreal.log('MEMORY_PIE_PLAYER alive=%s thirst=%s hunger=%s drinks=%s meals=%s' % (
        bool(player.get('player')), player.get('thirst'), player.get('hunger'), player.get('drinks'), player.get('meals')))


def tick(_dt):
    global phase
    now = time.monotonic()
    if phase == 0 and now - t0 > 3.0:
        phase = 1
        les.editor_request_begin_play()
    elif phase == 1 and les.is_in_play_in_editor():
        phase = 2
        unreal.log('MEMORY_PIE_ACTIVE days=%d' % DAYS)
    elif phase == 2:
        world = ues.get_game_world()
        if world and status(lib.get_chronicle_status(world)).get('started'):
            unreal.SystemLibrary.execute_console_command(world, 'Anastasis.Player.Arrive')
            live(world)
            phase = 3
    elif phase == 3:
        verdict(ues.get_game_world())
        phase = 4
        les.editor_request_end_play()
    elif phase == 4 and not les.is_in_play_in_editor():
        finish('MEMORY_PIE_COMPLETE')
        return
    if phase in (1, 2) and now - t0 > SECONDS:
        unreal.log('MEMORY_PIE FAIL timeout phase=%d' % phase)
        phase = 4
        les.editor_request_end_play()
    elif now - t0 > SECONDS + 120:
        finish('MEMORY_PIE_TIMEOUT phase=%d' % phase, True)


handle = unreal.register_slate_post_tick_callback(tick)
