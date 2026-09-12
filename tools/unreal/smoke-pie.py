import unreal, time
s = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
unreal.log('CANONICAL_EDITOR_BOOT')
unreal.log('CANONICAL_MAP_LOAD=' + str(s.load_level('/Game/FirstPerson/Lvl_FirstPerson')))
t0 = time.monotonic()
phase = 0
handle = None
def tick(dt):
    global phase, handle
    elapsed = time.monotonic() - t0
    if phase == 0 and elapsed > 3:
        phase = 1
        s.editor_request_begin_play()
        unreal.log('CANONICAL_PIE_REQUESTED')
    elif phase == 1 and s.is_in_play_in_editor():
        phase = 2
        unreal.log('CANONICAL_PIE_ACTIVE')
    elif phase == 2 and elapsed > 15:
        phase = 3
        s.editor_request_end_play()
        unreal.log('CANONICAL_PIE_END_REQUESTED')
    elif phase == 3 and not s.is_in_play_in_editor():
        unreal.log('CANONICAL_COMPLETE')
        unreal.unregister_slate_post_tick_callback(handle)
        unreal.SystemLibrary.quit_editor()
    elif elapsed > 60:
        unreal.log_error('CANONICAL_TIMEOUT phase=' + str(phase))
        unreal.unregister_slate_post_tick_callback(handle)
        unreal.SystemLibrary.quit_editor()
handle = unreal.register_slate_post_tick_callback(tick)
