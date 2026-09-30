"""Preuve PIE de l'horloge du ciel (DAY_NIGHT_WEATHER_001).

Lance PIE sur Lvl_AnastasisSlice et le laisse tourner plus d'un jour de simulation
(DAY_LENGTH = 90 s) : AAnastasisWorldAtmosphere logge une ligne ANASTASIS_SKY a chaque
changement de phase du village. La preuve se lit dans ces lignes : l'heure du ciel suit
le temps de simulation (GetSimulationTime), et la phase du ciel est celle du village
(GetVillagePhase), echantillonnees ensemble toutes les ~5 s (lignes SKY_PIE_SAMPLE).

Lancement : dans un editeur ouvert, `py tools/unreal/sky-clock-pie.py`, ou par un editeur
dedie (Start-AnastasisEditor, -ExecCmds="py <chemin>"). Rien n'est sauve.
ANASTASIS_SKY_PIE_SECONDS  duree du PIE en secondes reelles (defaut 110).
"""
import os, time, unreal

LEVEL = '/Game/Anastasis/Maps/Lvl_AnastasisSlice'
SECONDS = float(os.environ.get('ANASTASIS_SKY_PIE_SECONDS', '110'))

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
unreal.log('SKY_PIE_MAP_LOAD=' + str(les.load_level(LEVEL)))

t0 = time.monotonic()
phase, handle, started, last_sample = 0, None, 0.0, 0.0
lib = unreal.AnastasisSimulationDebugLibrary


def finish(msg, error=False):
    (unreal.log_error if error else unreal.log)(msg)
    unreal.unregister_slate_post_tick_callback(handle)
    unreal.SystemLibrary.quit_editor()


def tick(_dt):
    global phase, started, last_sample
    now = time.monotonic()
    if phase == 0 and now - t0 > 3.0:
        phase = 1
        les.editor_request_begin_play()
    elif phase == 1 and les.is_in_play_in_editor():
        phase, started = 2, now
        unreal.log('SKY_PIE_ACTIVE')
    elif phase == 2:
        world = ues.get_game_world()
        if world and now - last_sample > 5.0:
            last_sample = now
            unreal.log('SKY_PIE_SAMPLE real=%.1f sim_time=%.3f village_phase=%s' % (
                now - started, lib.get_simulation_time(world), lib.get_village_phase(world)))
        if now - started > SECONDS:
            phase = 3
            les.editor_request_end_play()
    elif phase == 3 and not les.is_in_play_in_editor():
        finish('SKY_PIE_COMPLETE')
    if now - t0 > SECONDS + 120:
        finish('SKY_PIE_TIMEOUT phase=%d' % phase, True)


handle = unreal.register_slate_post_tick_callback(tick)
