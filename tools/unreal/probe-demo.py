"""Phase G proof for the World Observatory: LAUNCH -> WORLD SNAPSHOT -> FIXED CAMERA CAPTURE.

Drives PIE on the project's default map (World Settings there already override the
GameMode to AAnastasis_UnrealV2GameMode; see ANASTASIS_VISUAL_MODE markers), then
exercises the Anastasis.World.* console commands against the live game world.
Read-only with respect to level content: spawns nothing persistent, saves nothing.
"""
import os, time, unreal

LOG_PREFIX = 'PROBE_DEMO'
MAP = '/Game/FirstPerson/Lvl_FirstPerson'
MISSION = os.environ.get('ANASTASIS_PROBE_MISSION', 'phaseg')

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)

unreal.log(LOG_PREFIX + '_BOOT')
unreal.log(LOG_PREFIX + '_MAP_LOAD=' + str(les.load_level(MAP)))


def current_world():
    return ues.get_game_world() if les.is_in_play_in_editor() else ues.get_editor_world()


def run_cmd(cmd):
    unreal.SystemLibrary.execute_console_command(current_world(), cmd)
    unreal.log(LOG_PREFIX + '_CMD ' + cmd)


t0 = time.monotonic()
phase = 0
handle = None


def finish(msg, error=False):
    (unreal.log_error if error else unreal.log)(msg)
    unreal.unregister_slate_post_tick_callback(handle)
    unreal.SystemLibrary.quit_editor()


def tick(dt):
    global phase
    elapsed = time.monotonic() - t0
    if phase == 0 and elapsed > 2.0:
        phase = 1
        les.editor_request_begin_play()
        unreal.log(LOG_PREFIX + '_PIE_REQUESTED')
    elif phase == 1 and les.is_in_play_in_editor():
        phase = 2
        unreal.log(LOG_PREFIX + '_PIE_ACTIVE')
    elif phase == 2 and elapsed > 6.0:
        phase = 3
        run_cmd('Anastasis.World.Status')
        run_cmd('Anastasis.World.Snapshot')
        run_cmd('Anastasis.World.NavStatus')
        run_cmd('Anastasis.World.Capture OVERVIEW ' + MISSION)
        unreal.log(LOG_PREFIX + '_COMMANDS_ISSUED')
    elif phase == 3 and elapsed > 14.0:
        phase = 4
        les.editor_request_end_play()
        unreal.log(LOG_PREFIX + '_PIE_END_REQUESTED')
    elif phase == 4 and not les.is_in_play_in_editor():
        finish(LOG_PREFIX + '_COMPLETE')
    elif elapsed > 60.0:
        finish(LOG_PREFIX + '_TIMEOUT phase=' + str(phase), True)


handle = unreal.register_slate_post_tick_callback(tick)
