"""Preuve en scene du premier batiment (mission first-building-001).

Charge Lvl_AnastasisSlice, lance un PIE, puis pilote la simulation UNIQUEMENT par
ses commandes console — exactement ce qu'un humain taperait :

    Anastasis.Village.FirstWell 4        puits + 4 habitants, soifs echelonnees
    Anastasis.Village.Status             (plusieurs fois) etat, usagers, pourquoi
    Anastasis.Village.RemoveNpc npc-1    un habitant retire pendant la vie du village
    Anastasis.Village.RemoveBuilding building-0
    Anastasis.Village.Status             plus de batiment, plus de reference

Le script ne juge rien : il laisse des marqueurs FIRST_BUILDING_* dans le log.
C'est la lecture du log (lignes ANASTASIS_VILLAGE) qui fait la preuve.

    UnrealEditor-Cmd.exe <uproject> -unattended -nosplash -NoLiveCoding -abslog=<log> ^
        -ExecCmds="py tools/unreal/first-building-pie.py"
"""
import time

import unreal

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
unreal.log('FIRST_BUILDING_MAP_LOAD=' + str(les.load_level('/Game/Anastasis/Maps/Lvl_AnastasisSlice')))

# (secondes apres PIE actif, commande)
SCRIPT = [
    # anastasis.Sim.Speed vaut 1 par defaut depuis sky-transitions-001 ; preuve etablie a 10.
    (0.0, 'anastasis.Sim.Speed 10'),
    (2.0, 'Anastasis.Village.FirstWell 4'),
    (3.0, 'Anastasis.Village.Status'),
    (6.0, 'Anastasis.Village.Status'),
    (6.5, 'Anastasis.Village.RemoveNpc npc-1'),
    (10.0, 'Anastasis.Village.Status'),
    (10.5, 'Anastasis.Village.RemoveBuilding building-0'),
    (11.0, 'Anastasis.Village.Status'),
    (14.0, 'Anastasis.Village.Status'),
]

t0 = time.monotonic()
phase = 0
active_at = None
step = 0
handle = None


def finish(message):
    unreal.log(message)
    unreal.unregister_slate_post_tick_callback(handle)
    unreal.SystemLibrary.quit_editor()


def tick(dt):
    global phase, active_at, step
    now = time.monotonic()
    if phase == 0 and now - t0 > 3:
        phase = 1
        # Temps simule au rythme JS (90 s par jour) : cette preuve attend sur le temps simule ; le jeu, lui, tourne a anastasis.Sim.TimeScale 0.0375 (villager-png-001, point 4).
        unreal.SystemLibrary.execute_console_command(None, 'anastasis.Sim.TimeScale 1')
        les.editor_request_begin_play()
        unreal.log('FIRST_BUILDING_PIE_REQUESTED')
    elif phase == 1 and les.is_in_play_in_editor():
        phase = 2
        active_at = now
        unreal.log('FIRST_BUILDING_PIE_ACTIVE')
    elif phase == 2:
        world = ues.get_game_world()
        while step < len(SCRIPT) and now - active_at >= SCRIPT[step][0]:
            unreal.log('FIRST_BUILDING_CMD ' + SCRIPT[step][1])
            unreal.SystemLibrary.execute_console_command(world, SCRIPT[step][1])
            step += 1
        if step >= len(SCRIPT):
            phase = 3
            les.editor_request_end_play()
            unreal.log('FIRST_BUILDING_PIE_END_REQUESTED')
    elif phase == 3 and not les.is_in_play_in_editor():
        finish('FIRST_BUILDING_COMPLETE')
    if now - t0 > 120:
        finish('FIRST_BUILDING_TIMEOUT phase=' + str(phase))


handle = unreal.register_slate_post_tick_callback(tick)
