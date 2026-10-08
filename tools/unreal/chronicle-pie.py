"""Preuve PIE de la chronique du village (CHRONIQUE_VILLAGE_001).

Lance PIE sur Lvl_AnastasisSlice, attend le village du lancement (anastasis.Village.StartVillagers),
saute trente jours de simulation (`Anastasis.Sim.Advance 30d`, regle « une preuve avance le temps »)
puis ecrit la chronique du village en francais dans Saved/Chronicle/ (`write_chronicle`).

Verdict CHRONICLE_PIE PASS si :
  - la chronique s'est ouverte sur le village du lancement (fondation racontee) ;
  - trente jours au moins sont clos, chacun avec son bilan ;
  - elle raconte autre chose que la fondation ;
  - le fichier ecrit contient le jour 30.
Ce n'est pas un verdict sur l'interet du recit : celui-la, Alexandre le donne en le lisant.

Lancement : dans un editeur ouvert, `py tools/unreal/chronicle-pie.py`, ou au lot (editor-batch, `chronicle-pie`).
Rien n'est sauve.
  ANASTASIS_CHRONICLE_DAYS     jours a sauter (defaut 30)
  ANASTASIS_CHRONICLE_SECONDS  plafond en secondes REELLES (defaut 240)
"""
import json, os, time, unreal

LEVEL = '/Game/Anastasis/Maps/Lvl_AnastasisSlice'
DAYS = int(os.environ.get('ANASTASIS_CHRONICLE_DAYS', '30'))
SECONDS = float(os.environ.get('ANASTASIS_CHRONICLE_SECONDS', '240'))
FILE_NAME = 'chronique-pie-%d-jours.txt' % DAYS

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
lib = unreal.AnastasisSimulationDebugLibrary
unreal.log('CHRONICLE_PIE_MAP_LOAD=' + str(les.load_level(LEVEL)))

t0 = time.monotonic()
phase, handle, started = 0, None, 0.0


def status(world):
    raw = lib.get_chronicle_status(world) if world else ''
    try:
        return json.loads(raw) if raw else {}
    except ValueError:
        return {}


def finish(msg, error=False):
    (unreal.log_error if error else unreal.log)(msg)
    unreal.unregister_slate_post_tick_callback(handle)
    unreal.SystemLibrary.quit_editor()


def verdict(world):
    st = status(world)
    path = lib.write_chronicle(world, FILE_NAME) if world else ''
    text = ''
    if path and os.path.isfile(path):
        with open(path, encoding='utf-8') as f:
            text = f.read()
    kinds = st.get('kinds', {})
    founding = kinds.get('Founding', 0)
    told = st.get('entries', 0) - founding
    checks = {
        'started': bool(st.get('started')) and founding > 0,
        'days_closed': st.get('days_closed', 0) >= DAYS,
        'more_than_founding': told > 0,
        # Le jour 30 seul, ou au bout d'un bloc de jours calmes (« JOURS 17 À 30 »).
        'file_has_last_day': ('\nJOUR %d\n' % DAYS) in text or (' À %d\n' % DAYS) in text,
    }
    # familles-feu-001 : avec les fondateurs, la chronique s'ouvre sur les familles et le premier soir au feu.
    if unreal.SystemLibrary.get_console_variable_int_value('anastasis.Village.Founders') == 1:
        checks['families'] = 'LES FAMILLES' in text and 'La maison du Scribe' in text
        checks['fire_scene'] = kinds.get('Scene', 0) >= 20
    ok = all(checks.values())
    unreal.log('CHRONICLE_PIE_STATUS ' + json.dumps(st, ensure_ascii=False))
    unreal.log('CHRONICLE_PIE %s path=%s days_closed=%s entries=%s told=%d people=%s alive=%s checks=%s' % (
        'PASS' if ok else 'FAIL', path or '-', st.get('days_closed'), st.get('entries'), told,
        st.get('people'), st.get('alive'), ','.join('%s=%s' % (k, int(v)) for k, v in checks.items())))
    # Les premieres lignes, pour lire le recit dans le log du lot sans ouvrir le fichier.
    for line in text.splitlines()[:40]:
        unreal.log('CHRONICLE_PIE_TEXT ' + line)


def tick(_dt):
    global phase, started
    now = time.monotonic()
    if phase == 0 and now - t0 > 3.0:
        phase = 1
        les.editor_request_begin_play()
    elif phase == 1 and les.is_in_play_in_editor():
        phase, started = 2, now
        unreal.log('CHRONICLE_PIE_ACTIVE days=%d' % DAYS)
    elif phase == 2:
        world = ues.get_game_world()
        if world and status(world).get('started'):
            unreal.log('CHRONICLE_PIE_VILLAGE ' + json.dumps(status(world), ensure_ascii=False))
            unreal.SystemLibrary.execute_console_command(world, 'Anastasis.Sim.Advance %dd' % DAYS)
            phase = 3
    elif phase == 3:
        world = ues.get_game_world()
        verdict(world)
        phase = 4
        les.editor_request_end_play()
    elif phase == 4 and not les.is_in_play_in_editor():
        finish('CHRONICLE_PIE_COMPLETE')
        return
    if phase in (1, 2) and now - t0 > SECONDS:
        world = ues.get_game_world()
        unreal.log('CHRONICLE_PIE FAIL timeout phase=%d status=%s' % (phase, json.dumps(status(world))))
        phase = 4
        les.editor_request_end_play()
    elif now - t0 > SECONDS + 120:
        finish('CHRONICLE_PIE_TIMEOUT phase=%d' % phase, True)


handle = unreal.register_slate_post_tick_callback(tick)
