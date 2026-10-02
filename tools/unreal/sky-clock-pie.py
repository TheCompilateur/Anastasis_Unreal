"""Preuve PIE de l'horloge du ciel (DAY_NIGHT_WEATHER_001).

Lance PIE sur Lvl_AnastasisSlice et le laisse tourner plus d'un jour de SIMULATION
(DAY_LENGTH = 90 s) : AAnastasisWorldAtmosphere logge une ligne ANASTASIS_SKY a chaque
changement de phase du village. La preuve se lit dans ces lignes : l'heure du ciel suit
le temps de simulation (GetSimulationTime), et la phase du ciel est celle du village
(GetVillagePhase), echantillonnees ensemble toutes les 5 s simulees (lignes SKY_PIE_SAMPLE).

Temps accelere (TIME_WARP_001, pie-advance-001) : la preuve attend sur le temps SIMULE, pousse
par anastasis.Sim.Warp -- un jour et quart en ~30 s reelles au lieu de 110. Verdict :
SKY_PIE PASS si un jour et quart a passe ET que les six phases du village ont ete vues, FAIL sinon.

Lancement : dans un editeur ouvert, `py tools/unreal/sky-clock-pie.py`, ou par un editeur
dedie (Start-AnastasisEditor, -ExecCmds="py <chemin>"), ou au lot (editor-batch, `sky-clock-pie`).
Rien n'est sauve.
  ANASTASIS_SKY_PIE_SIM_SECONDS  temps simule a parcourir (defaut 112 : un jour et quart)
  ANASTASIS_SKY_PIE_WARP         acceleration (defaut 4)
  ANASTASIS_SKY_PIE_SECONDS      plafond en secondes REELLES avant SKY_PIE_TIMEOUT (defaut 110)
"""
import os, time, unreal

LEVEL = '/Game/Anastasis/Maps/Lvl_AnastasisSlice'
SIM_SECONDS = float(os.environ.get('ANASTASIS_SKY_PIE_SIM_SECONDS', '112'))
WARP = float(os.environ.get('ANASTASIS_SKY_PIE_WARP', '4'))
SECONDS = float(os.environ.get('ANASTASIS_SKY_PIE_SECONDS', '110'))
# Phases du village (AnastasisRhythm::PhaseId) qu'un jour complet traverse.
EXPECTED_PHASES = {'dawn', 'morning', 'midday', 'afternoon', 'evening', 'night'}

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
unreal.log('SKY_PIE_MAP_LOAD=' + str(les.load_level(LEVEL)))

t0 = time.monotonic()
phase, handle, started = 0, None, 0.0
sim_start, last_sample, sim_now = None, None, -1.0
seen = set()
lib = unreal.AnastasisSimulationDebugLibrary


def finish(msg, error=False):
    (unreal.log_error if error else unreal.log)(msg)
    unreal.unregister_slate_post_tick_callback(handle)
    unreal.SystemLibrary.quit_editor()


def verdict(sim_elapsed):
    missing = sorted(EXPECTED_PHASES - seen)
    ok = sim_elapsed >= SIM_SECONDS and not missing
    unreal.log('SKY_PIE %s sim_elapsed=%.1f phases=%s%s' % (
        'PASS' if ok else 'FAIL', sim_elapsed, ','.join(sorted(seen)),
        (' missing=' + ','.join(missing)) if missing else ''))


def tick(_dt):
    global phase, started, sim_start, last_sample, sim_now
    now = time.monotonic()
    if phase == 0 and now - t0 > 3.0:
        phase = 1
        # Temps simule au rythme JS (90 s par jour), puis accelere : la preuve attend sur le temps simule.
        unreal.SystemLibrary.execute_console_command(None, 'anastasis.Sim.TimeScale 1')
        unreal.SystemLibrary.execute_console_command(None, 'anastasis.Sim.Warp %g' % WARP)
        les.editor_request_begin_play()
    elif phase == 1 and les.is_in_play_in_editor():
        phase, started = 2, now
        unreal.log('SKY_PIE_ACTIVE warp=%g sim_seconds=%g' % (WARP, SIM_SECONDS))
    elif phase == 2:
        world = ues.get_game_world()
        sim_now = lib.get_simulation_time(world) if world else -1.0
        if sim_now >= 0:
            if sim_start is None:
                sim_start = sim_now
            village_phase = lib.get_village_phase(world)
            if village_phase:
                seen.add(village_phase)
            if last_sample is None or sim_now - last_sample >= 5.0:
                last_sample = sim_now
                unreal.log('SKY_PIE_SAMPLE real=%.1f sim_time=%.3f village_phase=%s' % (now - started, sim_now, village_phase))
            if sim_now - sim_start >= SIM_SECONDS:
                verdict(sim_now - sim_start)
                unreal.SystemLibrary.execute_console_command(None, 'anastasis.Sim.Warp 1')
                phase = 3
                les.editor_request_end_play()
    elif phase == 3 and not les.is_in_play_in_editor():
        finish('SKY_PIE_COMPLETE')
        return
    if phase == 2 and now - started > SECONDS:
        elapsed = (sim_now - sim_start) if sim_start is not None else -1.0
        unreal.log('SKY_PIE FAIL timeout sim_elapsed=%.1f phases=%s' % (elapsed, ','.join(sorted(seen))))
        unreal.SystemLibrary.execute_console_command(None, 'anastasis.Sim.Warp 1')
        phase = 3
        les.editor_request_end_play()
    elif now - t0 > SECONDS + 120:
        unreal.SystemLibrary.execute_console_command(None, 'anastasis.Sim.Warp 1')
        finish('SKY_PIE_TIMEOUT phase=%d' % phase, True)


handle = unreal.register_slate_post_tick_callback(tick)
