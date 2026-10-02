"""Preuve en scene du joueur minimal (mission player-minimal-001).

Charge Lvl_AnastasisSlice, lance un PIE et pilote la simulation par ses seules commandes console, en
attendant le temps SIMULE (jamais l'horloge murale) :

    1. anastasis.Sim.TimeScale 1 ; Anastasis.Player.Arrive      un habitant arrive et est incarne
    2. 3 s simulees : il attend (but idle), immobile ; le pawn est pose sur lui
    3. Anastasis.Player.Move 1 0, 2 s simulees : il marche vers +X, le pawn le suit ; Move 0 0
    3b. Anastasis.Player.Goal build (pas de chantier) : refuse `hors-table`, il attend, l'intention reste
    3c. Anastasis.Player.Goal drink : il marche au puits et boit (<= 60 s simulees) ; Goal none
    4. Anastasis.Sim.Advance 7d : une semaine sautee -> presence ~3 %, 7 jours oisifs, plus personne ne le voit
    5. Anastasis.Sim.Advance 1d : un minuit de plus -> reputation sous 50
    6. Anastasis.Player.Release : observateur, le pawn retrouve sa marche

Etat lu par AnastasisSimulationDebugLibrary.get_player_status (JSON). Lignes PLAYER_PIE et
ANASTASIS_PLAYER du log ; PLAYER_PIE PASS si chaque controle tient. Aucun asset sauve.

    UnrealEditor.exe <uproject> -nosplash -NoLiveCoding -abslog=<log> -ExecCmds="py tools/unreal/player-pie.py"
"""
import json
import os
import time

import unreal

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
unreal.log('PLAYER_PIE_MAP_LOAD=' + str(les.load_level('/Game/Anastasis/Maps/Lvl_AnastasisSlice')))

DBG = unreal.AnastasisSimulationDebugLibrary
OUT = os.environ.get('ANASTASIS_PLAYER_PIE_OUT', '')

t0 = time.monotonic()
phase = 0
step = 0
mark = 0.0
handle = None
checks = []
samples = {}


def check(name, ok, detail=''):
    checks.append((name, bool(ok)))
    unreal.log('PLAYER_PIE CHECK %s %s %s' % ('OK' if ok else 'FAIL', name, detail))


def finish(message):
    unreal.log(message)
    passed = bool(checks) and all(ok for _, ok in checks)
    unreal.log('PLAYER_PIE %s checks=%d failed=%d' % ('PASS' if passed else 'FAIL', len(checks),
                                                        sum(1 for _, ok in checks if not ok)))
    if OUT:
        try:
            with open(os.path.join(OUT, 'player-pie.json'), 'w') as f:
                json.dump({'checks': checks, 'samples': samples}, f, indent=2)
        except Exception as e:  # la preuve est dans le log ; le JSON n'est qu'une copie
            unreal.log_warning('PLAYER_PIE json not written: %s' % e)
    unreal.unregister_slate_post_tick_callback(handle)
    unreal.SystemLibrary.quit_editor()


def cmd(world, text):
    unreal.log('PLAYER_PIE_CMD ' + text)
    unreal.SystemLibrary.execute_console_command(world, text)


def status(world, label):
    raw = DBG.get_player_status(world)
    unreal.log('PLAYER_PIE_STATUS %s %s' % (label, raw))
    s = json.loads(raw) if raw else {}
    samples[label] = s
    return s


def pawn_gap(world, s):
    """Distance au sol entre le pawn et le corps simule, en unites Unreal."""
    pawn = unreal.GameplayStatics.get_player_pawn(world, 0)
    if not pawn or 'ux' not in s:
        return 1e9
    loc = pawn.get_actor_location()
    return ((loc.x - s['ux']) ** 2 + (loc.y - s['uy']) ** 2) ** 0.5


def tick(dt):
    global phase, step, mark
    try:
        _tick()
    except Exception as e:
        check('no python exception', False, repr(e))
        phase = 3
        les.editor_request_end_play()


def _tick():
    global phase, step, mark
    now = time.monotonic()
    if now - t0 > 900:
        finish('PLAYER_PIE_TIMEOUT phase=%d step=%d' % (phase, step))
        return
    if phase == 0 and now - t0 > 3:
        phase = 1
        les.editor_request_begin_play()
        return
    if phase == 1 and les.is_in_play_in_editor():
        phase = 2
        mark = now
        unreal.log('PLAYER_PIE_ACTIVE')
        return
    if phase == 3 and not les.is_in_play_in_editor():
        finish('PLAYER_PIE_COMPLETE')
        return
    if phase != 2:
        return

    world = ues.get_game_world()
    if not world:
        return
    t = DBG.get_simulation_time(world)
    if t < 0:
        return

    if step == 0 and now - mark > 2.0:
        # Temps simule = temps reel : la preuve attend sur le temps simule (AGENTS.md, « Rythme »).
        cmd(world, 'anastasis.Sim.TimeScale 1')
        cmd(world, 'Anastasis.Player.Arrive')
        mark = t
        step = 1
        return

    if step == 1 and t - mark > 0.5:
        s = status(world, 'arrived')
        check('arrived and incarnated', s.get('player', '') != '', s.get('player'))
        samples['x0'] = s.get('x')
        samples['y0'] = s.get('y')
        mark = t
        step = 2
    elif step == 2 and t - mark > 3.0:
        s = status(world, 'idle')
        check('waits: goal idle', s.get('goal') == 'idle', s.get('goal'))
        check('waits: activity attend', s.get('activity') == 'attend', s.get('activity'))
        check('does not move by itself', abs(s.get('x', -1) - samples['x0']) < 1e-6 and abs(s.get('y', -1) - samples['y0']) < 1e-6)
        check('pawn bound to the inhabitant', s.get('pawn') is True)
        gap = pawn_gap(world, s)
        check('pawn stands on the body', gap < 5.0, 'gap=%.1f uu' % gap)
        samples['seenBy_before'] = s.get('seenBy')
        cmd(world, 'Anastasis.Player.Move 1 0')
        mark = t
        step = 3
    elif step == 3 and t - mark > 2.0:
        s = status(world, 'walked')
        moved = s.get('x', 0) - samples['x0']
        check('walks east when driven', moved > 1.0, 'dx=%.3f tiles' % moved)
        check('walking activity', s.get('activity') == 'marche', s.get('activity'))
        gap = pawn_gap(world, s)
        check('pawn follows the body', gap < 5.0, 'gap=%.1f uu' % gap)
        cmd(world, 'Anastasis.Player.Move 0 0')
        # player-goals-001 : la main du joueur. Batir sans chantier ouvert : refuse, et il attend.
        cmd(world, 'Anastasis.Player.Goal build')
        mark = t
        step = 31
    elif step == 31 and t - mark > 2.0:
        s = status(world, 'goal_refused')
        check('build without a site: refused hors-table', s.get('refusal') == 'hors-table', s.get('refusal'))
        check('refused: he waits, Nous does not pick', s.get('goal') == 'idle', s.get('goal'))
        check('refused: the intention is kept', s.get('choice') == 'build' and s.get('yields', 0) >= 1, '%s yields=%s' % (s.get('choice'), s.get('yields')))
        check('the player reads a table of goals', 'drink' in s.get('options', []), ','.join(s.get('options', [])))
        samples['drinks_before'] = s.get('drinks', 0)
        cmd(world, 'Anastasis.Player.Goal drink')
        mark = t
        step = 32
    elif step == 32:
        s = json.loads(DBG.get_player_status(world) or '{}')
        if s.get('drinks', 0) > samples['drinks_before'] or t - mark > 60.0:
            s = status(world, 'goal_drink')
            check('drink chosen: he walks to the well and drinks', s.get('drinks', 0) > samples['drinks_before'],
                  'drinks %s -> %s in %.1f s sim' % (samples['drinks_before'], s.get('drinks'), t - mark))
            check('drink chosen: the intention held', s.get('choice') == 'drink' and s.get('holds', 0) >= 1, 'holds=%s' % s.get('holds'))
            cmd(world, 'Anastasis.Player.Goal none')
            cmd(world, 'Anastasis.Sim.Advance 7d')
            step = 4
    elif step == 4:
        s = status(world, 'week_skipped')
        check('a skipped week: presence below 5 %', s.get('presence', 1) < 0.05, 'presence=%.4f' % s.get('presence', -1))
        check('a skipped week: 7 idle days', abs(s.get('idleDays', 0) - 7.0) < 0.01, 'idleDays=%.4f' % s.get('idleDays', -1))
        check('a skipped week: nobody sees him', s.get('seenBy', -1) == 0,
              'seenBy=%s (before: %s)' % (s.get('seenBy'), samples.get('seenBy_before')))
        check('a skipped week: reputation below 50', s.get('reputation', 50) < 50.0, 'reputation=%.3f' % s.get('reputation', -1))
        cmd(world, 'Anastasis.Sim.Advance 1d')
        step = 5
    elif step == 5:
        s = status(world, 'day_after')
        check('one more midnight: reputation keeps falling', s.get('reputation', 50) < samples['week_skipped'].get('reputation', 0),
              'reputation=%.3f' % s.get('reputation', -1))
        cmd(world, 'Anastasis.Player.Release')
        step = 6
    elif step == 6:
        s = status(world, 'released')
        check('released: observer again', s.get('player', 'x') == '')
        pawn = unreal.GameplayStatics.get_player_pawn(world, 0)
        movement = pawn.get_component_by_class(unreal.CharacterMovementComponent) if pawn else None
        mode = movement.movement_mode if movement else None
        check('released: pawn walks on its own again', mode is not None and mode != unreal.MovementMode.MOVE_NONE, str(mode))
        step = 7
    elif step == 7:
        phase = 3
        les.editor_request_end_play()


handle = unreal.register_slate_post_tick_callback(tick)
