"""SETTLEMENT_SENSITIVITY (geo-measure-001) : un reglage du RELIEF RENDU deplace-t-il le village de depart ?

IRON_CRUSADE_001, menace F3 : `TryStartVillage` choisit le site d'ouverture de la SIMULATION en lisant
le maillage rendu (`AnastasisSettlementSurvey::Read`). Ce maillage depend de CVars de rendu appliquees
a l'incarnation (`BeginPlay` -> `EmbodyFromConsoleVariables`). Ici, quatre PIE successifs dans un seul
editeur, memes graine et carte, une seule variable a la fois :
    ref         reglages par defaut
    drainage0   anastasis.Terrain.Drainage 0        (reseau de drainage rendu)
    humangeo0   anastasis.Terrain.HumanGeography 0  (relief dessine a la main)
    ref2        reglages par defaut : temoin, doit retrouver le site de ref
Lecture : `AnastasisSimulationDebugLibrary.get_settlement_site_status` (JSON du sondage), par etat.
Sortie : Saved/SettlementSensitivityEvidence/<horodatage>/sensitivity.json.
INSTRUMENT_PASS = quatre sondages lus et temoin reproductible ; le deplacement (MOVED / STABLE) est le
resultat mesure, pas le critere de reussite. Aucun asset sauve. Lancement : editor-batch.ps1 -Proofs
settlement-sensitivity-pie (registre proofs.txt).
"""
import json
import time
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
OUT = ROOT / 'Saved' / 'SettlementSensitivityEvidence' / str(time.time_ns())
LES = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
UES = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
LEVEL = '/Game/Anastasis/Maps/Lvl_AnastasisSlice'
FIXED = {'anastasis.Village.SiteSelection': 1, 'anastasis.Village.StartVillagers': 12}
VARIED = ['anastasis.Terrain.Drainage', 'anastasis.Terrain.HumanGeography']
DEFAULTS = {k: unreal.SystemLibrary.get_console_variable_int_value(k) for k in VARIED}
STATES = [
    ('ref', {}),
    ('drainage0', {'anastasis.Terrain.Drainage': 0}),
    ('humangeo0', {'anastasis.Terrain.HumanGeography': 0}),
    ('ref2', {}),
]
previous = {k: unreal.SystemLibrary.get_console_variable_int_value(k) for k in list(FIXED) + VARIED}
STATE_TIMEOUT = 240.0
started = time.monotonic()
state = {'index': 0, 'phase': 'start', 'since': time.monotonic(), 'finished': False, 'results': []}
handle = None


def cmd(name, value):
    unreal.SystemLibrary.execute_console_command(None, f'{name} {value}')


def finish(ok, reason):
    if state['finished']:
        return
    state['finished'] = True
    try:
        if LES.is_in_play_in_editor():
            LES.editor_request_end_play()
    except Exception:
        pass
    for name, value in previous.items():
        cmd(name, value)
    OUT.mkdir(parents=True, exist_ok=True)
    payload = {'project': str(ROOT), 'level': LEVEL, 'defaults': DEFAULTS, 'fixed': FIXED,
               'states': state['results'], 'verdict': 'INSTRUMENT_PASS' if ok else 'FAIL', 'reason': reason,
               'wall_seconds': time.monotonic() - started,
               'scope': 'site d ouverture choisi par le sondage du relief rendu ; un PIE par etat, une variable a la fois'}
    (OUT / 'sensitivity.json').write_text(json.dumps(payload, indent=2), encoding='utf-8')
    unreal.unregister_slate_post_tick_callback(handle)
    unreal.log('SETTLEMENT_SENSITIVITY ' + ('INSTRUMENT_PASS' if ok else 'FAIL') + ' ' + reason)
    unreal.SystemLibrary.quit_editor()


def summarize(label, overrides, report):
    sel = report.get('selected') or {}
    conc = report.get('water_concordance') or {}
    count = lambda key: len(conc.get(key) or [])
    return {
        'label': label, 'overrides': overrides, 'status': report.get('status'), 'error': report.get('error'),
        'eligible_count': report.get('eligible_count'), 'surveyed': report.get('surveyed'),
        'site': [sel.get('x'), sel.get('y')] if sel else None, 'score': sel.get('score'),
        'ground_z': sel.get('ground_z'), 'slope_deg': sel.get('slope_deg'),
        'water_m': sel.get('water_m'), 'food_m': sel.get('food_m'),
        'top': [[t.get('x'), t.get('y'), t.get('score')] for t in (report.get('top') or [])],
        'concordance': {'both_dry': count('both_dry'), 'both_water': count('both_water'),
                        'simulation_only': count('simulation_only'), 'render_only': count('render_only'),
                        'unknown': count('unknown'), 'mismatch_cells': conc.get('mismatch_cells')},
    }


def tick(_dt):
    try:
        if time.monotonic() - started > STATE_TIMEOUT * len(STATES) + 120:
            finish(False, 'timeout global'); return
        if time.monotonic() - state['since'] > STATE_TIMEOUT:
            finish(False, f"timeout etat {STATES[state['index']][0]} phase {state['phase']}"); return
        label, overrides = STATES[state['index']]
        if state['phase'] == 'start':
            if LES.is_in_play_in_editor():
                return
            if UES.get_editor_world().get_path_name().split('.')[0] != LEVEL:
                if not LES.load_level(LEVEL):
                    finish(False, 'map load failed'); return
            for name, value in FIXED.items():
                cmd(name, value)
            for name in VARIED:
                cmd(name, overrides.get(name, DEFAULTS[name]))
            LES.editor_request_begin_play()
            state.update(phase='wait', since=time.monotonic())
            return
        if state['phase'] == 'wait':
            if not LES.is_in_play_in_editor():
                return
            world = UES.get_game_world()
            if not world:
                return
            report = json.loads(unreal.AnastasisSimulationDebugLibrary.get_settlement_site_status(world))
            if report.get('status') in ('pending', 'not_started', 'reset'):
                return
            row = summarize(label, overrides, report)
            state['results'].append(row)
            unreal.log('SETTLEMENT_SENSITIVITY_STATE ' + json.dumps(row, sort_keys=True))
            LES.editor_request_end_play()
            state.update(phase='stop', since=time.monotonic())
            return
        if state['phase'] == 'stop':
            if LES.is_in_play_in_editor():
                return
            state['index'] += 1
            if state['index'] < len(STATES):
                state.update(phase='start', since=time.monotonic())
                return
            by = {r['label']: r for r in state['results']}
            ref, ref2 = by['ref'], by['ref2']
            if ref['site'] is None or ref2['site'] is None:
                finish(False, f"temoin sans site : ref={ref['status']} ref2={ref2['status']}"); return
            if ref['site'] != ref2['site']:
                finish(False, f"temoin non reproductible : ref={ref['site']} ref2={ref2['site']}"); return
            moved = {k: ('MOVED' if by[k]['site'] != ref['site'] else 'STABLE') for k in ('drainage0', 'humangeo0')}
            finish(True, json.dumps({'ref': ref['site'], **{k: [by[k]['site'], moved[k]] for k in moved}}, sort_keys=True))
    except Exception as exc:
        finish(False, repr(exc))


handle = unreal.register_slate_post_tick_callback(tick)
