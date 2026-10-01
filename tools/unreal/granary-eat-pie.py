"""Preuve en scene du grenier (mission granary-eat-001).

Charge Lvl_AnastasisSlice, lance un PIE, et pilote la simulation par ses seules
commandes console, en attendant les etats de la SIMULATION (jamais l'horloge
murale : un premier PIE compile des shaders et le temps simule peut geler) :

    1. Anastasis.Village.FirstGranary 4 6   grenier de 6 portions, 4 affames sans toit
    2. attendre le premier repas confirme -> Status
    3. attendre deux repas de plus (ou 40 s simulees) -> Status
    4. Anastasis.Village.RemoveBuilding building-0 (reservations en cours) -> Status
    5. attendre 10 s simulees -> Status, fin

La preuve est la lecture des lignes ANASTASIS_VILLAGE du log.

    UnrealEditor-Cmd.exe <uproject> -unattended -nosplash -NoLiveCoding -abslog=<log> ^
        -ExecCmds="py tools/unreal/granary-eat-pie.py"
"""
import time

import unreal

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
unreal.log('GRANARY_EAT_MAP_LOAD=' + str(les.load_level('/Game/Anastasis/Maps/Lvl_AnastasisSlice')))

# Lecteurs de debug (UAnastasisSimulationDebugLibrary) : Python n'atteint pas
# directement un sous-systeme de monde.
DBG = unreal.AnastasisSimulationDebugLibrary

t0 = time.monotonic()
phase = 0
step = 0
mark = 0.0
handle = None


def finish(message):
    unreal.log(message)
    unreal.unregister_slate_post_tick_callback(handle)
    unreal.SystemLibrary.quit_editor()


def cmd(world, text):
    unreal.log('GRANARY_EAT_CMD ' + text)
    unreal.SystemLibrary.execute_console_command(world, text)


def tick(dt):
    global phase, step, mark
    now = time.monotonic()
    if now - t0 > 240:
        finish('GRANARY_EAT_TIMEOUT phase=%d step=%d' % (phase, step))
        return
    if phase == 0 and now - t0 > 3:
        phase = 1
        # Temps simule au rythme JS (90 s par jour) : cette preuve attend sur le temps simule ; le jeu, lui, tourne a anastasis.Sim.TimeScale 0.0375 (villager-png-001, point 4).
        unreal.SystemLibrary.execute_console_command(None, 'anastasis.Sim.TimeScale 1')
        les.editor_request_begin_play()
        unreal.log('GRANARY_EAT_PIE_REQUESTED')
        return
    if phase == 1 and les.is_in_play_in_editor():
        phase = 2
        mark = now
        unreal.log('GRANARY_EAT_PIE_ACTIVE')
        return
    if phase == 3 and not les.is_in_play_in_editor():
        finish('GRANARY_EAT_COMPLETE')
        return
    if phase != 2:
        return

    world = ues.get_game_world()
    if not world:
        return
    t = DBG.get_simulation_time(world)
    if t < 0:
        return
    meals = DBG.count_meals_taken(world)

    if step == 0 and now - mark > 1.5:
        # anastasis.Sim.Speed vaut 1 par defaut depuis sky-transitions-001 ; cette preuve a ete
        # etablie a 10 et le reste.
        cmd(world, 'anastasis.Sim.Speed 10')
        cmd(world, 'Anastasis.Village.FirstGranary 4 6')
        cmd(world, 'Anastasis.Village.Status')
        mark = t
        step = 1
    elif step == 1 and meals >= 1:
        unreal.log('GRANARY_EAT_FIRST_MEAL t=%.3f stock=%d' % (t, DBG.get_food_stock(world, 'building-0')))
        cmd(world, 'Anastasis.Village.Status')
        mark = t
        step = 2
    elif step == 2 and (meals >= 3 or t - mark > 40.0):
        unreal.log('GRANARY_EAT_MEALS meals=%d stock=%d t=%.3f' % (meals, DBG.get_food_stock(world, 'building-0'), t))
        cmd(world, 'Anastasis.Village.Status')
        cmd(world, 'Anastasis.Village.RemoveBuilding building-0')
        cmd(world, 'Anastasis.Village.Status')
        mark = t
        step = 3
    elif step == 3 and t - mark > 10.0:
        cmd(world, 'Anastasis.Village.Status')
        phase = 3
        les.editor_request_end_play()
        unreal.log('GRANARY_EAT_PIE_END_REQUESTED')


handle = unreal.register_slate_post_tick_callback(tick)
