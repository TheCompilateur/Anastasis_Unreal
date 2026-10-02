"""PIE gate: real sampled NPC traffic alters grass, then off clears/restores the layer.
No assets saved. Run via editor-batch -Proofs anthropic-memory-pie. Does NOT prove art quality.
"""
import time
import unreal

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
level = '/Game/Anastasis/Maps/Lvl_AnastasisSlice'
if ues.get_editor_world().get_path_name().split('.')[0] != level:
    les.load_level(level)
start = time.monotonic()
phase = 0
handle = None
mark = start
last_log = 0
changed = False


def cmd(world, text):
    unreal.SystemLibrary.execute_console_command(world, text)


def finish(ok, reason):
    cmd(None, 'anastasis.Anthropic.Memory 0')
    unreal.log('ANTHROPIC_PIE %s %s' % ('PASS' if ok else 'FAIL', reason))
    unreal.unregister_slate_post_tick_callback(handle)
    les.editor_request_end_play()
    unreal.SystemLibrary.quit_editor()


def tick(_dt):
    global phase, mark, last_log, changed
    now = time.monotonic()
    try:
        if now - start > 240:
            finish(False, 'timeout or no observed grass response')
            return
        if phase == 0 and now - start > 3:
            cmd(None, 'anastasis.Sim.Speed 1')
            cmd(None, 'anastasis.Sim.Warp 1')
            cmd(None, 'anastasis.Sim.TimeScale 0.0375')
            cmd(None, 'anastasis.Village.StartVillagers 12')
            cmd(None, 'anastasis.Anthropic.Memory 1')
            les.editor_request_begin_play()
            phase = 1
        elif phase == 1 and les.is_in_play_in_editor():
            phase, mark = 2, now
        elif phase >= 2:
            world = ues.get_game_world()
            if not world:
                return
            report = unreal.AnastasisAnthropicDebugLibrary.get_status(world)
            data = dict(piece.split('=', 1) for piece in report.split() if '=' in piece)
            if now - last_log > 5:
                unreal.log('ANTHROPIC_PIE_SAMPLE ' + report)
                last_log = now
            if phase == 2 and int(data.get('grass', 0)) > 0:
                changed = True
                cmd(world, 'anastasis.Anthropic.Memory 0')
                phase, mark = 3, now
            elif phase == 3 and now - mark > 2:
                ok = changed and data.get('active') == '0' and data.get('grass') == '0' and data.get('cells') == '0' and int(data.get('restored', 0)) > 0 and data.get('restore_errors') == '0'
                finish(ok, 'observed grass response then off; original transforms restored and reread')
    except Exception as exc:
        finish(False, repr(exc))


handle = unreal.register_slate_post_tick_callback(tick)
