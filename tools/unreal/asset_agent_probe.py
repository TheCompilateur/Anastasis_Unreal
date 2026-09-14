"""
ANASTASIS_ASSET_AGENT_001 -- Phase 4 verify.

Adapted from tools/unreal/probe-demo.py for this mission's own isolated
worktree (no dependency on the canonical root's env/paths). Drives PIE on the
default map, sets the canonical 32x32 surface-mode crop (matching the earlier
visualbuild001-surface evidence so the two captures are directly comparable),
then issues Anastasis.World.Capture. Read-only with respect to level content.
"""
import os, time, unreal

LOG_PREFIX = 'ASSET_AGENT_PROBE'
MAP = '/Game/FirstPerson/Lvl_FirstPerson'
MISSION = os.environ.get('ANASTASIS_PROBE_MISSION', 'asset-agent-001')

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
        run_cmd('Anastasis.World.Capture GROUND ' + MISSION)
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
