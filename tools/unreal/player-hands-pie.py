"""Preuve en scene des mains du joueur (mission mains-joueur-001, ecart n°57).

Charge Lvl_AnastasisSlice, lance un PIE, gele la simulation (`anastasis.Sim.TimeScale 0` : ni besoins ni Nous ne
bougent, seuls les gestes changent l'etat), puis, pour chaque cible, fait arriver un habitant-joueur a cote
(`Anastasis.Player.Arrive X Y`), tourne la camera vers elle et lit ce que la main vise :

    1. le puits        : offre « boire », a portee ; `Anastasis.Player.Use` -> une gorgee de plus
    2. un champ        : offre « recolter » ; clic maintenu (`Anastasis.Player.Hold 1`) -> une portion dans le sac
    3. le sac          : `Anastasis.Player.EatFromBag` -> la portion mangee
    4. le grenier      : offre « deposer » ou « prendre » ; `Anastasis.Player.Use` -> le sac et le grenier changent
                         d'autant, rien ne se cree
    5. une tuile de bois : clic maintenu -> une buche dans le sac
    6. la barre de 8 cases est a l'ecran

Etat lu par AnastasisSimulationDebugLibrary.get_player_hands_status / get_player_hand_targets (JSON). Lignes
PLAYER_HANDS_PIE et ANASTASIS_HANDS du log ; PLAYER_HANDS_PIE PASS si chaque controle tient. Aucun asset sauve.
Sortie : ANASTASIS_PLAYER_HANDS_OUT (defaut Saved/PlayerHandsEvidence/pie/).

    UnrealEditor.exe <uproject> -nosplash -NoLiveCoding -abslog=<log> -ExecCmds="py tools/unreal/player-hands-pie.py"
"""
import json
import os
import time

import unreal

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
unreal.log('PLAYER_HANDS_PIE_MAP_LOAD=' + str(les.load_level('/Game/Anastasis/Maps/Lvl_AnastasisSlice')))

DBG = unreal.AnastasisSimulationDebugLibrary
OUT = os.environ.get('ANASTASIS_PLAYER_HANDS_OUT') or os.path.join(unreal.Paths.project_saved_dir(), 'PlayerHandsEvidence', 'pie')

t0 = time.monotonic()
phase = 0
step = 0
mark = 0.0
handle = None
checks = []
samples = {}
target = {}


def check(name, ok, detail=''):
    checks.append((name, bool(ok)))
    unreal.log('PLAYER_HANDS_PIE CHECK %s %s %s' % ('OK' if ok else 'FAIL', name, detail))


def finish(message):
    unreal.log(message)
    passed = bool(checks) and all(ok for _, ok in checks)
    unreal.log('PLAYER_HANDS_PIE %s checks=%d failed=%d' % ('PASS' if passed else 'FAIL', len(checks),
                                                              sum(1 for _, ok in checks if not ok)))
    try:
        os.makedirs(OUT, exist_ok=True)
        with open(os.path.join(OUT, 'player-hands.json'), 'w') as f:
            json.dump({'checks': checks, 'samples': samples}, f, indent=2)
    except Exception as e:  # la preuve est dans le log ; le JSON n'est qu'une copie
        unreal.log_warning('PLAYER_HANDS_PIE json not written: %s' % e)
    unreal.unregister_slate_post_tick_callback(handle)
    unreal.SystemLibrary.quit_editor()


def cmd(world, text):
    unreal.log('PLAYER_HANDS_PIE_CMD ' + text)
    unreal.SystemLibrary.execute_console_command(world, text)


def hands(world, label=None):
    raw = DBG.get_player_hands_status(world)
    s = json.loads(raw) if raw else {}
    if label:
        unreal.log('PLAYER_HANDS_PIE_STATUS %s %s' % (label, raw))
        samples[label] = s
    return s


def targets(world):
    raw = DBG.get_player_hand_targets(world)
    return json.loads(raw) if raw else {}


def aim(world, point):
    """Tourne la camera du joueur vers un point Unreal (centre de la cible, un peu au-dessus du sol)."""
    controller = unreal.GameplayStatics.get_player_controller(world, 0)
    camera = unreal.GameplayStatics.get_player_camera_manager(world, 0)
    if not controller or not camera:
        return False
    eye = camera.get_camera_location()
    goal = unreal.Vector(point['ux'], point['uy'], point['uz'] + point.get('lift', 0.0))
    controller.set_control_rotation(unreal.MathLibrary.find_look_at_rotation(eye, goal))
    return True


def building(world, kind):
    for b in targets(world).get('buildings', []):
        if b.get('type') == kind and b.get('done'):
            return b
    return None


def tick(dt):
    global phase
    try:
        _tick()
    except Exception as e:
        check('no python exception', False, repr(e))
        phase = 3
        les.editor_request_end_play()


def _tick():
    global phase, step, mark, target
    now = time.monotonic()
    if now - t0 > 900:
        finish('PLAYER_HANDS_PIE_TIMEOUT phase=%d step=%d' % (phase, step))
        return
    if phase == 0 and now - t0 > 3:
        phase = 1
        les.editor_request_begin_play()
        return
    if phase == 1 and les.is_in_play_in_editor():
        phase = 2
        mark = now
        unreal.log('PLAYER_HANDS_PIE_ACTIVE')
        return
    if phase == 3 and not les.is_in_play_in_editor():
        finish('PLAYER_HANDS_PIE_COMPLETE')
        return
    if phase != 2:
        return
    world = ues.get_game_world()
    if not world or DBG.get_simulation_time(world) < 0:
        return

    # Chaque etape : arriver, viser, attendre que l'ecran ait vu la cible (quelques images), agir, relire.
    if step == 0 and now - mark > 3.0:
        cmd(world, 'anastasis.Sim.TimeScale 0')
        cmd(world, 'Anastasis.Player.Arrive')
        step, mark = 1, now
    elif step == 1 and now - mark > 1.0:
        well = building(world, 'well')
        check('a finished well to drink at', well is not None, json.dumps(well))
        if not well:
            step = 90
            return
        target = dict(well, lift=120.0)
        cmd(world, 'Anastasis.Player.Arrive %d %d' % (well['x'] + 1, well['y']))
        step, mark = 2, now
    elif step == 2 and now - mark > 1.0:
        aim(world, target)
        step, mark = 3, now
    elif step == 3 and now - mark > 0.6:
        s = hands(world, 'well_focus')
        check('looking at the well: it offers to drink', s.get('offer') == 'boire', s.get('offer'))
        check('... within reach', s.get('in_reach') is True)
        check('the 8-slot bar is on screen', s.get('panel') is True)
        samples['drinks0'] = s.get('drinks', 0)
        cmd(world, 'Anastasis.Player.Use')
        step, mark = 4, now
    elif step == 4 and now - mark > 0.4:
        s = hands(world, 'drank')
        check('use: one more drink', s.get('drinks', 0) == samples['drinks0'] + 1, '%s -> %s' % (samples['drinks0'], s.get('drinks')))
        food = targets(world).get('tiles', {}).get('food')
        check('a food tile near the village', food is not None, json.dumps(food))
        if not food:
            step = 90
            return
        target = dict(food, lift=20.0)
        cmd(world, 'Anastasis.Player.Arrive %d %d' % (food['x'] + 1, food['y']))
        step, mark = 5, now
    elif step == 5 and now - mark > 1.0:
        aim(world, target)
        step, mark = 6, now
    elif step == 6 and now - mark > 0.6:
        s = hands(world, 'field_focus')
        check('looking at the field: it offers to gather', s.get('offer') == 'recolter', '%s tile=%s' % (s.get('offer'), s.get('focus_tile')))
        check('... within reach', s.get('in_reach') is True)
        samples['food0'] = s.get('food', 0)
        cmd(world, 'Anastasis.Player.Hold 1')
        step, mark = 7, now
    elif step == 7 and now - mark > 2.0:
        s = hands(world, 'gathered')
        cmd(world, 'Anastasis.Player.Hold 0')
        check('holding the click: a portion in the bag', s.get('food', 0) > samples['food0'], '%s -> %s' % (samples['food0'], s.get('food')))
        samples['meals0'] = s.get('meals', 0)
        cmd(world, 'Anastasis.Player.EatFromBag')
        step, mark = 8, now
    elif step == 8 and now - mark > 0.4:
        s = hands(world, 'ate')
        check('eat from the bag: one meal, one portion less', s.get('meals', 0) == samples['meals0'] + 1
              and s.get('food', 0) == samples['gathered'].get('food', 0) - 1, 'meals=%s food=%s' % (s.get('meals'), s.get('food')))
        granary = building(world, 'granary')
        check('a finished granary', granary is not None, json.dumps(granary))
        if not granary:
            step = 90
            return
        target = dict(granary, lift=150.0)
        samples['granary0'] = granary.get('food', 0)
        cmd(world, 'Anastasis.Player.Arrive %d %d' % (granary['x'] + 1, granary['y']))
        step, mark = 9, now
    elif step == 9 and now - mark > 1.0:
        aim(world, target)
        step, mark = 10, now
    elif step == 10 and now - mark > 0.6:
        s = hands(world, 'granary_focus')
        check('looking at the granary: deposit or take', s.get('offer') in ('deposer la nourriture', 'prendre une portion'), s.get('offer'))
        check('... within reach', s.get('in_reach') is True)
        samples['bag_before_granary'] = s.get('food', 0)
        cmd(world, 'Anastasis.Player.Use')
        step, mark = 11, now
    elif step == 11 and now - mark > 0.4:
        s = hands(world, 'granary_done')
        g = building(world, 'granary') or {}
        before = samples['granary0'] + samples['bag_before_granary']
        after = g.get('food', 0) + s.get('food', 0)
        check('granary: bag and granary change by the same amount (nothing created)', before == after and s.get('food') != samples['bag_before_granary'],
              'granary %s->%s bag %s->%s' % (samples['granary0'], g.get('food'), samples['bag_before_granary'], s.get('food')))
        wood = targets(world).get('tiles', {}).get('wood')
        check('a wood tile near the village', wood is not None, json.dumps(wood))
        if not wood:
            step = 90
            return
        target = dict(wood, lift=60.0)
        cmd(world, 'Anastasis.Player.Arrive %d %d' % (wood['x'] + 1, wood['y']))
        step, mark = 12, now
    elif step == 12 and now - mark > 1.0:
        aim(world, target)
        step, mark = 13, now
    elif step == 13 and now - mark > 0.6:
        s = hands(world, 'wood_focus')
        check('looking at the wood: it offers to chop', s.get('offer') == 'couper du bois', '%s tile=%s' % (s.get('offer'), s.get('focus_tile')))
        samples['wood0'] = s.get('wood', 0)
        cmd(world, 'Anastasis.Player.Hold 1')
        step, mark = 14, now
    elif step == 14 and now - mark > 3.0:
        s = hands(world, 'chopped')
        cmd(world, 'Anastasis.Player.Hold 0')
        check('holding the click on wood: a log in the bag', s.get('wood', 0) > samples['wood0'], '%s -> %s' % (samples['wood0'], s.get('wood')))
        step = 90
    elif step == 90:
        cmd(world, 'Anastasis.Player.Release')
        cmd(world, 'anastasis.Sim.TimeScale 0.0375')
        phase = 3
        les.editor_request_end_play()


handle = unreal.register_slate_post_tick_callback(tick)
