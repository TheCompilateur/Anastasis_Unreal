"""Preuve instrumentale de la premiere demande d'aide du joueur en PIE.

Le joueur incarne un chef de famille dans Valmire. La sonde choisit une autre personne,
se deplace si necessaire jusqu'a portee de voix et lui demande de l'aide. Elle exige une reponse avec motif,
aucune piece ni teleportation au moment du oui/non, et un panneau joueur cree. Elle ne prouve pas
que le texte est lisible, que les touches fonctionnent, ni que la scene est plaisante a hauteur humaine.
"""
import json
import math
import time

import unreal

LEVEL = '/Game/Anastasis/Maps/Lvl_AnastasisSlice'
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
lib = unreal.AnastasisSimulationDebugLibrary
unreal.log('PLAYER_HELP_SCENE_MAP=' + str(les.load_level(LEVEL)))

started = time.monotonic()
phase = 0
handle = None
candidate_id = ''
before = {}
checks = []


def read(world):
    raw = lib.get_help_scene_status(world)
    return json.loads(raw) if raw else {}


def check(label, condition, detail=''):
    checks.append((label, bool(condition)))
    unreal.log('PLAYER_HELP_SCENE_CHECK %s %s %s' % ('OK' if condition else 'FAIL', label, detail))


def stop(message):
    unreal.log(message)
    ok = checks and all(value for _, value in checks)
    unreal.log('PLAYER_HELP_SCENE_PIE %s checks=%d failed=%d' % (
        'PASS' if ok else 'FAIL', len(checks), sum(not value for _, value in checks)))
    unreal.unregister_slate_post_tick_callback(handle)
    unreal.SystemLibrary.quit_editor()


def site_of_player(state):
    return next((s for s in state.get('sites', []) if s.get('playerAllowed') and s.get('progress', 1) < 1), None)


def target(state, site):
    if not site:
        return None
    people = [p for p in state.get('people', []) if p.get('family') != site.get('family')]
    return min(people, key=lambda p: p.get('distance', 1e9)) if people else None


def tick(_dt):
    global phase, candidate_id, before
    try:
        now = time.monotonic()
        if now - started > 300:
            check('time limit', False, 'phase=%s' % phase)
            stop('PLAYER_HELP_SCENE_TIMEOUT')
            return
        if phase == 0 and now - started > 3:
            phase = 1
            les.editor_request_begin_play()
            return
        if phase == 1 and les.is_in_play_in_editor():
            phase = 2
            return
        if phase == 5 and not les.is_in_play_in_editor():
            stop('PLAYER_HELP_SCENE_COMPLETE')
            return
        if phase not in (2, 3, 4):
            return
        world = ues.get_game_world()
        if not world:
            return
        state = read(world)
        if phase == 2:
            if not state.get('ready'):
                return
            unreal.SystemLibrary.execute_console_command(world, 'anastasis.Sim.TimeScale 1')
            lib.start_help_scene(world)
            state = read(world)
            site = site_of_player(state)
            check('player incarnated as a family builder', state.get('active') and bool(state.get('player')) and bool(site),
                  'player=%s site=%s' % (state.get('player'), site.get('id') if site else 'none'))
            check('player panel constructed', state.get('panel') is True)
            candidate = target(state, site)
            check('another family can be asked', candidate is not None)
            if candidate is None or site is None:
                phase = 5
                les.editor_request_end_play()
                return
            candidate_id = candidate['id']
            phase = 3
            return
        if phase == 3:
            site = site_of_player(state)
            candidate = next((p for p in state.get('people', []) if p.get('id') == candidate_id), None)
            if not candidate or not site:
                check('candidate and site remain', False)
                phase = 5
                les.editor_request_end_play()
                return
            if candidate['distance'] > 5.0:
                dx = candidate['x'] - state['playerX']
                dy = candidate['y'] - state['playerY']
                length = max(1e-9, math.hypot(dx, dy))
                unreal.SystemLibrary.execute_console_command(world, 'Anastasis.Player.Move %.4f %.4f' % (dx / length, dy / length))
                return
            unreal.SystemLibrary.execute_console_command(world, 'Anastasis.Player.Move 0 0')
            before = {'pieces': site['pieces'], 'x': candidate['x'], 'y': candidate['y'],
                      'answers': len(state.get('answers', [])), 'site': site['id']}
            lib.ask_help_to_id(world, candidate_id)
            phase = 4
            return
        if phase == 4:
            site = next((s for s in state.get('sites', []) if s['id'] == before['site']), None)
            candidate = next((p for p in state.get('people', []) if p.get('id') == candidate_id), None)
            answers = state.get('answers', [])
            answer = answers[-1] if len(answers) == before['answers'] + 1 else None
            check('one answer recorded immediately', answer is not None)
            check('answer names the person and a causal reason', answer is not None and
                  answer.get('to') == candidate_id and bool(answer.get('reason')),
                  str(answer))
            check('reply does not build', site is not None and site.get('pieces') == before['pieces'])
            check('reply does not teleport', candidate is not None and
                  math.hypot(candidate['x'] - before['x'], candidate['y'] - before['y']) < 0.5)
            unreal.log('PLAYER_HELP_SCENE_STATE ' + json.dumps(state, ensure_ascii=False))
            unreal.SystemLibrary.execute_console_command(world, 'Shot')
            phase = 5
            les.editor_request_end_play()
    except Exception as exc:
        check('python exception', False, repr(exc))
        phase = 5
        les.editor_request_end_play()


handle = unreal.register_slate_post_tick_callback(tick)
