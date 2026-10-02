"""Preuve en scene : le fermier s'abrite de l'orage (mission village-weather-001).

Charge Lvl_AnastasisSlice, lance un PIE (vitesse et echelle de temps a 1), et pilote la simulation
par ses seules commandes console, en attendant les etats de la SIMULATION (jamais l'horloge murale) :

    1. Anastasis.Village.FirstFarmer 1          un grenier vide pres d'un champ, un fermier embauche
    2. 8 s simulees : il travaille -> Status
    3. Anastasis.Village.ForceWeather 0.9       l'orage (le `sim.forceWeather` de la reference)
    4. attendre son but `shelterRain`, puis qu'il soit DEDANS pour `shelterRain` -> Status
    5. attendre qu'il en ressorte (abris pris +1) -> Status ; Anastasis.Village.ForceWeather off
    6. 20 s simulees -> Status, fin

L'etat de l'habitant est lu par GetNpcState ("but|activite|batiment|but interieur|abris") : etre
DEDANS ne suffit pas — premiere version de ce script, le fermier etait au grenier pour MANGER au
moment de l'orage, et la preuve l'avait compte comme un abri. Ici seul `shelterRain` compte.

Lignes VILLAGE_WEATHER_ et ANASTASIS_VILLAGE du log. Le CIEL n'est pas force : la commande n'impose
que ce que les habitants lisent.

Temps accelere (TIME_WARP_001, pie-advance-001) : `anastasis.Sim.Warp 10` -- le pas de la reference a x10
(celui de `Sim.Speed 10`, deja pris par les autres preuves du village), plusieurs pas par frame ; les
delais restent en temps SIMULE. Verdict : VILLAGE_WEATHER PASS si le fermier est entre a l'abri pour
`shelterRain` PUIS en est ressorti, FAIL sinon (avec l'etape atteinte).

    UnrealEditor.exe <uproject> -nosplash -NoLiveCoding -abslog=<log> -ExecCmds="py tools/unreal/village-weather-pie.py"
"""
import time

import unreal

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
unreal.log('VILLAGE_WEATHER_MAP_LOAD=' + str(les.load_level('/Game/Anastasis/Maps/Lvl_AnastasisSlice')))

DBG = unreal.AnastasisSimulationDebugLibrary
FARMER = 'npc-0'
GOAL_SHELTER = 'shelterRain'

t0 = time.monotonic()
phase = 0
step = 0
mark = 0.0
storm_at = 0.0
handle = None
shelters_before = 0
# Etape la plus loin atteinte : 5 = entre a l'abri pour shelterRain puis ressorti.
reached = 0


def verdict():
    unreal.log('VILLAGE_WEATHER %s reached_step=%d' % ('PASS' if reached >= 5 else 'FAIL', reached))


def finish(message):
    unreal.log(message)
    unreal.unregister_slate_post_tick_callback(handle)
    unreal.SystemLibrary.quit_editor()


def cmd(world, text):
    unreal.log('VILLAGE_WEATHER_CMD ' + text)
    unreal.SystemLibrary.execute_console_command(world, text)


def state(world):
    raw = DBG.get_npc_state(world, FARMER)
    parts = raw.split('|') if raw else []
    if len(parts) != 5:
        return None
    goal, activity, building, inside_goal, shelters = parts
    return {'goal': goal, 'activity': activity, 'building': building, 'inside_goal': inside_goal,
            'shelters': int(shelters), 'raw': raw}


def tick(dt):
    global phase, step, mark, storm_at, shelters_before, reached
    now = time.monotonic()
    if now - t0 > 900:
        verdict()
        unreal.SystemLibrary.execute_console_command(None, 'anastasis.Sim.Warp 1')
        finish('VILLAGE_WEATHER_TIMEOUT phase=%d step=%d' % (phase, step))
        return
    if phase == 0 and now - t0 > 3:
        phase = 1
        les.editor_request_begin_play()
        return
    if phase == 1 and les.is_in_play_in_editor():
        phase = 2
        mark = now
        unreal.log('VILLAGE_WEATHER_PIE_ACTIVE')
        return
    if phase == 3 and not les.is_in_play_in_editor():
        finish('VILLAGE_WEATHER_COMPLETE')
        return
    if phase != 2:
        return

    world = ues.get_game_world()
    if not world:
        return
    t = DBG.get_simulation_time(world)
    if t < 0:
        return

    if step == 0 and now - mark > 1.5:
        cmd(world, 'anastasis.Sim.Speed 1')
        # Temps simule = temps reel : l'echelle de jour (anastasis.Sim.TimeScale, branche
        # villager-png-001) ralentit la journee a ~40 min ; cette preuve attend sur le temps simule.
        cmd(world, 'anastasis.Sim.TimeScale 1')
        # TIME_WARP_001 : x10, le pas de `Sim.Speed 10` ; les delais ci-dessous restent en temps simule.
        cmd(world, 'anastasis.Sim.Warp 10')
        cmd(world, 'Anastasis.Village.FirstFarmer 1')
        cmd(world, 'Anastasis.Village.Status')
        mark = t
        step = 1
        return

    s = state(world)
    if s is None:
        return

    if step == 1 and t - mark > 8.0:
        unreal.log('VILLAGE_WEATHER_BEFORE t=%.3f state=%s' % (t, s['raw']))
        cmd(world, 'Anastasis.Village.ForceWeather 0.9')
        shelters_before = s['shelters']
        storm_at = t
        mark = t
        step = 2
    elif step == 2:
        if s['goal'] == GOAL_SHELTER:
            unreal.log('VILLAGE_WEATHER_GOAL_SHELTER t=%.3f after_storm=%.3f state=%s' % (t, t - storm_at, s['raw']))
            step = 3
        elif t - mark > 150.0:
            unreal.log_error('VILLAGE_WEATHER_NO_SHELTER_GOAL after=%.3f state=%s' % (t - storm_at, s['raw']))
            cmd(world, 'Anastasis.Village.Status')
            step = 6
    elif step == 3:
        if s['building'] != '-' and s['inside_goal'] == GOAL_SHELTER:
            unreal.log('VILLAGE_WEATHER_SHELTERED t=%.3f after_storm=%.3f state=%s' % (t, t - storm_at, s['raw']))
            cmd(world, 'Anastasis.Village.Status')
            mark = t
            step = 4
        elif t - storm_at > 150.0:
            unreal.log_error('VILLAGE_WEATHER_NEVER_INSIDE state=%s' % s['raw'])
            cmd(world, 'Anastasis.Village.Status')
            step = 6
    elif step == 4:
        if s['shelters'] > shelters_before:
            unreal.log('VILLAGE_WEATHER_LEFT_SHELTER t=%.3f stayed=%.3f state=%s' % (t, t - mark, s['raw']))
            cmd(world, 'Anastasis.Village.Status')
            cmd(world, 'Anastasis.Village.ForceWeather off')
            mark = t
            step = 5
            reached = 5
        elif t - mark > 90.0:
            unreal.log_error('VILLAGE_WEATHER_NEVER_LEFT stayed=%.3f state=%s' % (t - mark, s['raw']))
            step = 6
    elif step == 5 and t - mark > 20.0:
        unreal.log('VILLAGE_WEATHER_AFTER t=%.3f state=%s' % (t, s['raw']))
        cmd(world, 'Anastasis.Village.Status')
        step = 6
    elif step == 6:
        verdict()
        cmd(world, 'anastasis.Sim.Warp 1')
        phase = 3
        les.editor_request_end_play()


handle = unreal.register_slate_post_tick_callback(tick)
