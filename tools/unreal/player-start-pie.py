"""Preuve PIE du debut de partie (mission player-start-001, PLAYER_START_001).

Appuyer sur Play = debut du jeu : l'habitant-joueur est incarne au village du lancement, sans aucune
commande de simulation. La preuve ne tape AUCUNE commande Anastasis.* pendant le PIE : elle ne fait que
poser la CVar avant d'appuyer sur Play (un editeur pilote par script demarre en observateur,
`anastasis.Player.AutoArrive 0`, voir editor-launch.ps1 / editor-batch.py), puis lire l'etat.

    A. anastasis.Player.AutoArrive 1 (le defaut du jeu), Play :
       - le village du lancement se pose, puis le joueur arrive (get_player_status : player, arrivedAtStart)
       - le pawn est lie au corps, pose dessus, a moins de PLAYER_START_MAX_M du puits, regard vers le puits
       - sa carte portrait est cachee ; le village vit : les autres habitants bougent, le temps simule avance
    B. temoin anastasis.Player.AutoArrive 0, Play : village du lancement pose, personne d'incarne (comme avant)
    C. anastasis.Player.AutoArrive 0 + anastasis.Visual.Mode 2 (PLAYER), Play : le mode PLAYER force l'arrivee

Verdict PLAYER_START_PIE PASS/FAIL ; JSON dans ANASTASIS_PLAYER_START_OUT si posee. Aucun asset sauve.

    UnrealEditor.exe <uproject> -nosplash -NoLiveCoding -abslog=<log> -ExecCmds="py tools/unreal/player-start-pie.py"
    (au lot : tools\\unreal\\editor-batch.ps1 -Proofs player-start-pie)
"""
import json
import os
import time

import unreal

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
unreal.log('PLAYER_START_PIE_MAP_LOAD=' + str(les.load_level('/Game/Anastasis/Maps/Lvl_AnastasisSlice')))

DBG = unreal.AnastasisSimulationDebugLibrary
OUT = os.environ.get('ANASTASIS_PLAYER_START_OUT', '')
MAX_M = float(os.environ.get('PLAYER_START_MAX_M', '100'))
SIM_WATCH = float(os.environ.get('PLAYER_START_SIM_WATCH', '1.0'))

# (nom, AutoArrive, Visual.Mode, joueur attendu)
RUNS = [('auto', 1, 1, True), ('witness_off', 0, 1, False), ('mode_player', 0, 2, True)]

t0 = time.monotonic()
state = {'run': 0, 'phase': 'setup', 'mark': 0.0, 'sim0': None, 'cards0': None, 'first': None}
checks = []
samples = {}
handle = None


def check(name, ok, detail=''):
    checks.append((name, bool(ok), str(detail)))
    unreal.log('PLAYER_START_PIE CHECK %s %s %s' % ('OK' if ok else 'FAIL', name, detail))


def finish(message):
    unreal.log(message)
    failed = sum(1 for _, ok, _ in checks if not ok)
    passed = bool(checks) and failed == 0 and state['run'] >= len(RUNS)
    for name in ('anastasis.Player.AutoArrive 0', 'anastasis.Visual.Mode 1'):
        unreal.SystemLibrary.execute_console_command(None, name)
    unreal.log('PLAYER_START_PIE %s checks=%d failed=%d' % ('PASS' if passed else 'FAIL', len(checks), failed))
    if OUT:
        try:
            os.makedirs(OUT, exist_ok=True)
            with open(os.path.join(OUT, 'player-start.json'), 'w') as f:
                json.dump({'checks': checks, 'samples': samples}, f, indent=2)
        except Exception as e:  # la preuve est dans le log ; le JSON n'en est qu'une copie
            unreal.log_warning('PLAYER_START_PIE json not written: %s' % e)
    unreal.unregister_slate_post_tick_callback(handle)
    unreal.SystemLibrary.quit_editor()


def status(world):
    raw = DBG.get_player_status(world)
    return json.loads(raw) if raw else {}


def cards(world):
    raw = DBG.get_villager_cards(world)
    return json.loads(raw) if raw else {}


def tick(dt):
    try:
        _tick()
    except Exception as e:
        check('no python exception', False, repr(e))
        if les.is_in_play_in_editor():
            les.editor_request_end_play()
        finish('PLAYER_START_PIE_ERROR')


def _tick():
    now = time.monotonic()
    if now - t0 > 1100:
        if les.is_in_play_in_editor():
            les.editor_request_end_play()
        finish('PLAYER_START_PIE_TIMEOUT run=%d phase=%s' % (state['run'], state['phase']))
        return
    if state['run'] >= len(RUNS):
        if not les.is_in_play_in_editor():
            finish('PLAYER_START_PIE_COMPLETE')
        return
    name, auto, mode, expect = RUNS[state['run']]
    phase = state['phase']

    if phase == 'setup':
        if les.is_in_play_in_editor() or now - state['mark'] < 3.0:
            return
        # Avant Play, dans le monde de l'editeur : c'est tout ce que la preuve pose. Rien pendant le PIE.
        unreal.SystemLibrary.execute_console_command(None, 'anastasis.Player.AutoArrive %d' % auto)
        unreal.SystemLibrary.execute_console_command(None, 'anastasis.Visual.Mode %d' % mode)
        unreal.log('PLAYER_START_PIE_RUN %s AutoArrive=%d Visual.Mode=%d' % (name, auto, mode))
        les.editor_request_begin_play()
        state.update(phase='starting', mark=now, sim0=None, cards0=None, first=None)
        return

    if phase == 'starting':
        if not les.is_in_play_in_editor():
            return
        world = ues.get_game_world()
        if not world:
            return
        s = status(world)
        # Le village du lancement attend que le terrain soit pret (au plus ~10 s), puis le joueur arrive.
        ready = s.get('npcs', 0) > 0 and (s.get('player', '') != '' or not expect)
        if ready and (expect or now - state['mark'] > 20.0):
            state['first'] = s
            state['sim0'] = DBG.get_simulation_time(world)
            state['cards0'] = cards(world)
            samples[name + '_first'] = s
            unreal.log('PLAYER_START_PIE_STATUS %s first %s' % (name, json.dumps(s)))
            state.update(phase='watching', mark=now)
        elif now - state['mark'] > 120.0:
            check('%s: start village seeded' % name, False, 'npcs=%s player=%r after 120 s' % (s.get('npcs'), s.get('player')))
            les.editor_request_end_play()
            state.update(phase='ending', mark=now)
        return

    if phase == 'watching':
        world = ues.get_game_world()
        if not world:
            return
        # Au rythme du jeu (TimeScale 0,0375 : 1 s simulee ~ 27 s reelles), sans rien accelerer : on laisse
        # passer SIM_WATCH s simulees (les habitants pensent, se levent, marchent), au plus 240 s reelles.
        if expect and DBG.get_simulation_time(world) - (state['sim0'] or 0) < SIM_WATCH and now - state['mark'] < 240.0:
            return
        if not expect and now - state['mark'] < 15.0:
            return
        s = status(world)
        first = state['first']
        samples[name + '_later'] = s
        unreal.log('PLAYER_START_PIE_STATUS %s later %s' % (name, json.dumps(s)))
        if expect:
            check('%s: player incarnated without any command' % name, first.get('player', '') != '', first.get('player'))
            check('%s: arrived at the start of play' % name, first.get('arrivedAtStart') is True, first.get('arrivedAtStart'))
            check('%s: start village around him' % name, first.get('npcs', 0) >= 5 and first.get('well') is True,
                  'npcs=%s well=%s' % (first.get('npcs'), first.get('well')))
            check('%s: pawn bound to the inhabitant' % name, s.get('pawn') is True, s.get('pawn'))
            gap = ((s.get('px', 0) - s.get('ux', 1e9)) ** 2 + (s.get('py', 0) - s.get('uy', 1e9)) ** 2) ** 0.5
            check('%s: pawn stands on the body' % name, gap < 5.0, 'gap=%.1f uu' % gap)
            check('%s: pawn in the village' % name, 0.0 <= s.get('pawnToWellM', -1) < MAX_M,
                  'pawnToWell=%.1f m (max %.0f)' % (s.get('pawnToWellM', -1), MAX_M))
            check('%s: looks toward the well' % name, s.get('facingErrDeg', 180) < 20.0, 'facingErr=%.1f deg' % s.get('facingErrDeg', 180))
            check('%s: he waits (no command)' % name, s.get('goal') == 'idle', s.get('goal'))
            # La carte portrait du joueur est cachee (on ne se voit pas) ; les autres vivent.
            c1 = cards(world)
            me = s.get('player')
            hidden = [v.get('hidden') for v in c1.get('villagers', []) if v.get('npc') == me]
            check('%s: own portrait hidden' % name, hidden == [True], hidden)
            before = {v['npc']: v for v in (state['cards0'] or {}).get('villagers', [])}
            # On compte les habitants deplaces de plus de 20 cm ou dont le corps marche (vitesse d'animation).
            moved = 0
            walking = 0
            for v in c1.get('villagers', []):
                if v['npc'] == me:
                    continue
                b = before.get(v['npc'])
                if b and ((v['x'] - b['x']) ** 2 + (v['y'] - b['y']) ** 2) ** 0.5 > 20.0:
                    moved += 1
                if v.get('speed', 0) > 5.0:
                    walking += 1
            # Les autres decident : un but autre qu'attendre (Nous pense pour eux, pas pour le joueur).
            busy = 0
            goals = []
            for v in c1.get('villagers', []):
                if v['npc'] == me:
                    continue
                goal = (DBG.get_npc_state(world, v['npc']) or '').split('|')[0]
                goals.append(goal)
                if goal and goal not in ('idle', 'observer'):
                    busy += 1
            sim1 = DBG.get_simulation_time(world)
            check('%s: the village lives (others act, time runs)' % name,
                  (moved >= 1 or walking >= 1 or busy >= 3) and sim1 > (state['sim0'] or 0),
                  'moved=%d walking=%d busy=%d goals=%s sim %.2f -> %.2f s'
                  % (moved, walking, busy, ','.join(sorted(set(goals))), state['sim0'] or -1, sim1))
        else:
            check('%s: start village seeded' % name, first.get('npcs', 0) >= 5, 'npcs=%s' % first.get('npcs'))
            check('%s: nobody incarnated (observer start)' % name, s.get('player', 'x') == '' and s.get('arrivedAtStart') is False,
                  'player=%r arrivedAtStart=%s' % (s.get('player'), s.get('arrivedAtStart')))
            check('%s: pawn walks on its own' % name, s.get('pawn') is not True, s.get('pawn'))
        les.editor_request_end_play()
        state.update(phase='ending', mark=now)
        return

    if phase == 'ending':
        if les.is_in_play_in_editor():
            return
        state['run'] += 1
        state.update(phase='setup', mark=now)


handle = unreal.register_slate_post_tick_callback(tick)
