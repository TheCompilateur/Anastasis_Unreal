"""Preuve PIE des arrivants et du conseil du soir (arrivants-001, ecart n°49).

Lance PIE sur Lvl_AnastasisSlice : Valmire est fondee, et avec elle le monde exterieur se charge
(`anastasis.Geo.AutoLoad 1`). Saute soixante jours (`Anastasis.Sim.Advance 1d`, jour par jour, sans joueur :
le temps accelere ne doit incarner personne, AGENTS.md), puis ecrit la chronique dans Saved/Chronicle/ et
l'etat des arrivants (`get_arrivals_status`) dans Saved/ArrivalsEvidence/pie/arrivals.json.

Verdict ARRIVANTS_PIE PASS si (le « fini quand » d'Alexandre, ARRIVANTS_001.md) :
  - le monde exterieur est charge, et au moins deux groupes arrivent par la route ;
  - chaque groupe passe au conseil, chaque voix a sa raison ;
  - au moins un groupe accueilli ET au moins un groupe refuse ;
  - une maison achevee revient a un groupe accueilli ;
  - au moins une legende dans la chronique ;
  - le fichier de chronique est ecrit.
Ce n'est pas un verdict sur l'interet du recit : celui-la, Alexandre le donne en le lisant.

Lancement : `py tools/unreal/arrivants-pie.py` dans un editeur ouvert, ou au lot (editor-batch, `arrivants-pie`). Rien n'est sauve.
  ANASTASIS_ARRIVALS_DAYS     jours a sauter (defaut 60)
  ANASTASIS_ARRIVALS_SECONDS  plafond en secondes REELLES (defaut 420)
"""
import json, os, time, unreal

LEVEL = '/Game/Anastasis/Maps/Lvl_AnastasisSlice'
DAYS = int(os.environ.get('ANASTASIS_ARRIVALS_DAYS', '60'))
SECONDS = float(os.environ.get('ANASTASIS_ARRIVALS_SECONDS', '420'))
OUT = os.path.join(unreal.Paths.project_saved_dir(), 'ArrivalsEvidence', 'pie')

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
lib = unreal.AnastasisSimulationDebugLibrary
unreal.log('ARRIVANTS_PIE_MAP_LOAD=' + str(les.load_level(LEVEL)))

t0 = time.monotonic()
phase, handle, advanced = 0, None, 0


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
    arrivals = status(lib.get_arrivals_status(world))
    chronicle_path = lib.write_chronicle(world, 'arrivants-pie-%d-jours.txt' % DAYS)
    kinds = chronicle.get('kinds', {})
    groups = arrivals.get('groups', [])
    councils = arrivals.get('councils', [])
    checks = {
        'geo_loaded': bool(arrivals.get('geo_loaded')),
        'groups': len(groups) >= 2,
        'councils': len(councils) == len(groups) and all(v.get('reason') for c in councils for v in c.get('votes', [])),
        'welcomed': any(c.get('accepted') for c in councils),
        'refused': any(not c.get('accepted') for c in councils),
        'arrival_house': any(g.get('house_done') for g in groups),
        'legend': kinds.get('Legend', 0) >= 1,
        'file': bool(chronicle_path) and os.path.isfile(chronicle_path),
    }
    ok = all(checks.values())
    os.makedirs(OUT, exist_ok=True)
    with open(os.path.join(OUT, 'arrivals.json'), 'w', encoding='utf-8') as f:
        json.dump({'arrivals': arrivals, 'chronicle': chronicle, 'checks': checks}, f, ensure_ascii=False, indent=1)
    for c in councils:
        unreal.log('ARRIVANTS_PIE_CONSEIL jour=%s %s %s : %s' % (c.get('day'), c.get('family'), 'ACCUEILLI' if c.get('accepted') else 'REFUSE',
            ' | '.join('%s %s/%s [%s]' % (v.get('voter'), 'oui' if v.get('yes') else 'non', v.get('reason'), v.get('terms')) for v in c.get('votes', []))))
    unreal.log('ARRIVANTS_PIE %s groups=%d councils=%d welcomed=%d refused=%d houses=%d legends=%s rumors=%s people=%s checks=%s' % (
        'PASS' if ok else 'FAIL', len(groups), len(councils), sum(1 for c in councils if c.get('accepted')),
        sum(1 for c in councils if not c.get('accepted')), sum(1 for g in groups if g.get('house_done')),
        kinds.get('Legend'), kinds.get('Rumor'), arrivals.get('people'), ','.join('%s=%d' % (k, int(v)) for k, v in checks.items())))


def tick(_dt):
    global phase, advanced
    now = time.monotonic()
    if phase == 0 and now - t0 > 3.0:
        phase = 1
        les.editor_request_begin_play()
    elif phase == 1 and les.is_in_play_in_editor():
        phase = 2
        unreal.log('ARRIVANTS_PIE_ACTIVE days=%d' % DAYS)
    elif phase == 2:
        world = ues.get_game_world()
        if world and status(lib.get_chronicle_status(world)).get('started'):
            phase = 3
    elif phase == 3:
        # Un jour par frame : la chronique lit le village entre deux sauts, comme en jeu.
        world = ues.get_game_world()
        unreal.SystemLibrary.execute_console_command(world, 'Anastasis.Sim.Advance 1d')
        advanced += 1
        if advanced >= DAYS:
            phase = 4
    elif phase == 4:
        verdict(ues.get_game_world())
        phase = 5
        les.editor_request_end_play()
    elif phase == 5 and not les.is_in_play_in_editor():
        finish('ARRIVANTS_PIE_COMPLETE')
        return
    if phase in (1, 2, 3) and now - t0 > SECONDS:
        unreal.log('ARRIVANTS_PIE FAIL timeout phase=%d advanced=%d' % (phase, advanced))
        phase = 5
        les.editor_request_end_play()
    elif now - t0 > SECONDS + 120:
        finish('ARRIVANTS_PIE_TIMEOUT phase=%d' % phase, True)


handle = unreal.register_slate_post_tick_callback(tick)
