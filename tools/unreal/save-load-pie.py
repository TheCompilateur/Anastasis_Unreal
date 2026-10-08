"""SAVE_STATE_001 -- preuve PIE de la sauvegarde : sauver, vivre, recharger, revivre le meme futur.

Lance par editor-batch.ps1 -Proofs save-load-pie. Le village du lancement, tel quel, plus le monde
exterieur du jeu (Anastasis.Geo.Load). La simulation ne bouge que par Anastasis.Sim.Advance
(anastasis.Sim.TimeScale 0 : rien n'avance entre deux commandes), donc deux passages sur le meme etat
doivent rendre le meme etat :

  1. trois jours vecus (Advance 3d : la maison d'ouverture est fondee), puis Anastasis.Sim.Save -> empreinte S
  2. deux jours de plus (Advance 2d)                                -> empreinte F
  3. Anastasis.Sim.Load                                             -> empreinte S, presentation refaite
     (un acteur par batiment simule, une carte par habitant), et chaque batiment garde sa FORME :
     programme, variante affichee et fondateur, tires de sa biographie (save-history-001, ecart n°46)
  4. les memes deux jours (Advance 2d)                              -> empreinte F : le meme futur
  5. Anastasis.Sim.Load d'un slot absent                            -> refuse, empreinte inchangee

Lecture par AnastasisSimulationDebugLibrary.get_save_status (JSON). Verdict SAVE_LOAD_PIE PASS/FAIL ;
etats dans $ANASTASIS_SAVE_PIE_OUT/save-load.json. Aucun asset sauve ; le slot ecrit est
Saved/SaveGames/save-state-pie.sav.
"""
import json
import os
import time

import unreal

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
dbg = unreal.AnastasisSimulationDebugLibrary
les.load_level('/Game/Anastasis/Maps/Lvl_AnastasisSlice')

OUT = os.environ.get('ANASTASIS_SAVE_PIE_OUT', '')
SLOT = 'save-state-pie'
WALL_SECONDS = float(os.environ.get('ANASTASIS_SAVE_PIE_WALL_SECONDS', '400'))

started = time.monotonic()
phase = 0
wait_since = None
states = {}
handle = None


def console(cmd):
    unreal.SystemLibrary.execute_console_command(ues.get_game_world(), cmd)


def status():
    return json.loads(dbg.get_save_status(ues.get_game_world()) or '{}')


def shapes():
    """Forme de chaque batiment : programme (biographie), variante affichee, forme fixee, fondateur."""
    s = json.loads(dbg.get_settlement_status(ues.get_game_world()) or '{}')
    return {b['id']: {k: b.get(k) for k in ('type', 'program', 'variant', 'fixed', 'founder', 'job', 'household')}
            for b in s.get('buildings', [])}


def finish(ok, reason):
    try:
        console('anastasis.Sim.TimeScale 0.0375')
    except Exception:
        pass
    if OUT:
        try:
            os.makedirs(OUT, exist_ok=True)
            with open(os.path.join(OUT, 'save-load.json'), 'w', encoding='utf-8') as f:
                json.dump({'ok': ok, 'reason': reason, 'states': states}, f, indent=2)
        except Exception as e:
            unreal.log_warning('SAVE_LOAD_PIE could not write evidence: %s' % e)
    unreal.log('SAVE_LOAD_PIE %s %s wall_s=%.0f' % ('PASS' if ok else 'FAIL', reason, time.monotonic() - started))
    unreal.unregister_slate_post_tick_callback(handle)
    unreal.SystemLibrary.quit_editor()


def check(label, cond, detail):
    if not cond:
        finish(False, '%s %s' % (label, json.dumps(detail)))
    return cond


def tick(_dt):
    global phase, wait_since
    try:
        step()
    except Exception as e:  # une exception ne doit pas laisser l'editeur ouvert
        finish(False, 'exception %r' % e)


def step():
    global phase, wait_since
    now = time.monotonic()
    if now - started > WALL_SECONDS:
        finish(False, 'wall_timeout phase=%d states=%s' % (phase, json.dumps(states)))
        return
    if phase == 0:
        if now - started > 3:
            phase = 1
            les.editor_request_begin_play()
        return
    if phase == 1:
        if les.is_in_play_in_editor() and ues.get_game_world():
            phase = 2
            console('anastasis.Sim.TimeScale 0')
            console('anastasis.Sim.Warp 1')
            wait_since = now
        return
    if phase == 2:
        # Le village du lancement se pose quand le terrain est la (TryStartVillage).
        s = status()
        if s and s.get('npcs', 0) > 0 and s.get('building_actors', 0) > 0:
            phase = 3
        elif now - wait_since > 120:
            finish(False, 'no_start_village %s' % json.dumps(s))
        return
    if phase == 3:
        console('Anastasis.Geo.Load')
        console('Anastasis.Sim.Advance 3d')
        lived = status()
        states['lived'] = lived
        if not check('geo_not_loaded', lived.get('geo_loaded'), lived):
            return
        console('Anastasis.Sim.Save %s' % SLOT)
        saved = status()
        states['saved'] = saved
        if not check('save_failed', saved.get('last', {}).get('op') == 'save' and saved['last'].get('ok'), saved):
            return
        if not check('save_digest', saved['last'].get('digest') == saved.get('digest') == lived.get('digest'), saved):
            return
        states['shapes_saved'] = shapes()
        founded = [i for i, b in states['shapes_saved'].items() if b['type'] == 'house' and b['fixed']]
        if not check('no_founded_house', founded, states['shapes_saved']):
            return

        console('Anastasis.Sim.Advance 2d')
        future = status()
        states['future'] = future
        if not check('future_unchanged', future.get('digest') != saved.get('digest'), future):
            return

        console('Anastasis.Sim.Load %s' % SLOT)
        loaded = status()
        states['loaded'] = loaded
        last = loaded.get('last', {})
        if not check('load_failed', last.get('op') == 'load' and last.get('ok'), loaded):
            return
        if not check('load_digest', loaded.get('digest') == saved.get('digest') and last.get('digest_match'), loaded):
            return
        if not check('load_clock', loaded.get('time') == saved.get('time') and loaded.get('day') == saved.get('day'), loaded):
            return
        if not check('load_geo', loaded.get('geo_loaded'), loaded):
            return
        phase = 4
        wait_since = now
        return
    if phase == 4:
        # Une frame pour que la presentation refaite se pose (cartes des habitants).
        if now - wait_since < 1.0:
            return
        shown = status()
        states['shown'] = shown
        if not check('presentation_buildings', shown.get('building_actors') == shown.get('buildings'), shown):
            return
        if not check('presentation_villagers', shown.get('villager_actors') == shown.get('npcs'), shown):
            return
        if not check('presentation_still_saved_state', shown.get('digest') == states['saved'].get('digest'), shown):
            return
        states['shapes_loaded'] = shapes()
        lost = {i: (b, states['shapes_loaded'].get(i)) for i, b in states['shapes_saved'].items() if states['shapes_loaded'].get(i) != b}
        if not check('shapes_changed', not lost, lost):
            return

        console('Anastasis.Sim.Advance 2d')
        replay = status()
        states['replay'] = replay
        if not check('same_future', replay.get('digest') == states['future'].get('digest'), replay):
            return

        before = replay.get('digest')
        console('Anastasis.Sim.Load save-state-pie-absent')
        refused = status()
        states['refused'] = refused
        if not check('absent_slot_accepted', refused.get('last', {}).get('ok') is False, refused):
            return
        if not check('refusal_wrote', refused.get('digest') == before, refused):
            return
        founded = sorted(i for i, b in states['shapes_loaded'].items() if b['type'] == 'house' and b['fixed'])
        finish(True, 'saved=%s future=%s replay=%s npcs=%d buildings=%d bytes=%d day=%d founded=%s shapes_kept=%d' % (
            states['saved']['digest'], states['future']['digest'], replay['digest'], shown['npcs'], shown['buildings'],
            states['saved']['last']['bytes'], states['saved']['day'], ','.join(founded), len(states['shapes_loaded'])))


handle = unreal.register_slate_post_tick_callback(tick)
