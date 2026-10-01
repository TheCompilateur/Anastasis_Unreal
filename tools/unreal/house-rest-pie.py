"""Preuve en scene de la maison (mission house-rest-001).

Charge Lvl_AnastasisSlice, lance un PIE, puis pilote la simulation UNIQUEMENT par
ses commandes console, en attendant les etats de la SIMULATION (pas l'horloge
murale : un premier PIE compile des shaders et le temps simule peut geler) :

    1. Anastasis.Village.FirstHouse 4      maison possedee + maison libre, 4 habitants
    2. attendre la nuit, puis que la maison du proprietaire soit OCCUPEE -> Status
    3. Anastasis.Village.RemoveNpc npc-2   un habitant retire -> Status
    4. attendre ~6 s simulees de sommeil -> Status
    5. Anastasis.Village.RemoveBuilding building-0   la maison OCCUPEE demolie -> Status
    6. attendre l'aube -> Status, fin

La preuve est la lecture des lignes ANASTASIS_VILLAGE du log.

    UnrealEditor-Cmd.exe <uproject> -unattended -nosplash -NoLiveCoding -abslog=<log> ^
        -ExecCmds="py tools/unreal/house-rest-pie.py"
"""
import time

import unreal

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
unreal.log('HOUSE_REST_MAP_LOAD=' + str(les.load_level('/Game/Anastasis/Maps/Lvl_AnastasisSlice')))

t0 = time.monotonic()
phase = 0
step = 0
mark = 0.0
handle = None


def finish(message):
    unreal.log(message)
    unreal.unregister_slate_post_tick_callback(handle)
    unreal.SystemLibrary.quit_editor()


# Lecteurs de debug (UAnastasisSimulationDebugLibrary) : Python n'atteint pas
# directement un sous-systeme de monde.
DBG = unreal.AnastasisSimulationDebugLibrary


def cmd(world, text):
    unreal.log('HOUSE_REST_CMD ' + text)
    unreal.SystemLibrary.execute_console_command(world, text)


def tick(dt):
    global phase, step, mark
    now = time.monotonic()
    if now - t0 > 240:
        finish('HOUSE_REST_TIMEOUT phase=%d step=%d' % (phase, step))
        return
    if phase == 0 and now - t0 > 3:
        phase = 1
        # Temps simule au rythme JS (90 s par jour) : cette preuve attend sur le temps simule ; le jeu, lui, tourne a anastasis.Sim.TimeScale 0.0375 (villager-png-001, point 4).
        unreal.SystemLibrary.execute_console_command(None, 'anastasis.Sim.TimeScale 1')
        les.editor_request_begin_play()
        unreal.log('HOUSE_REST_PIE_REQUESTED')
        return
    if phase == 1 and les.is_in_play_in_editor():
        phase = 2
        mark = now
        unreal.log('HOUSE_REST_PIE_ACTIVE')
        return
    if phase == 3 and not les.is_in_play_in_editor():
        finish('HOUSE_REST_COMPLETE')
        return
    if phase != 2:
        return

    world = ues.get_game_world()
    if not world:
        return
    t = DBG.get_simulation_time(world)
    if t < 0:
        return
    village_phase = DBG.get_village_phase(world)

    if step == 0 and now - mark > 1.5:
        # anastasis.Sim.Speed vaut 1 par defaut depuis sky-transitions-001 ; cette preuve a ete
        # etablie a 10 et le reste.
        cmd(world, 'anastasis.Sim.Speed 10')
        cmd(world, 'Anastasis.Village.FirstHouse 4')
        cmd(world, 'Anastasis.Village.Status')
        step = 1
    elif step == 1 and village_phase == 'night' and DBG.count_inside(world, 'building-0') > 0:
        unreal.log('HOUSE_REST_NIGHT_OCCUPIED t=%.3f' % t)
        cmd(world, 'Anastasis.Village.Status')
        cmd(world, 'Anastasis.Village.RemoveNpc npc-2')
        cmd(world, 'Anastasis.Village.Status')
        mark = t
        step = 2
    elif step == 2 and t - mark > 6.0:
        cmd(world, 'Anastasis.Village.Status')
        unreal.log('HOUSE_REST_REMOVE_OCCUPIED inside=%d' % DBG.count_inside(world, 'building-0'))
        cmd(world, 'Anastasis.Village.RemoveBuilding building-0')
        cmd(world, 'Anastasis.Village.Status')
        step = 3
    elif step == 3 and village_phase == 'dawn':
        unreal.log('HOUSE_REST_DAWN t=%.3f' % t)
        cmd(world, 'Anastasis.Village.Status')
        phase = 3
        les.editor_request_end_play()
        unreal.log('HOUSE_REST_PIE_END_REQUESTED')


handle = unreal.register_slate_post_tick_callback(tick)
