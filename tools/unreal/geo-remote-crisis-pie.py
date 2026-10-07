"""Preuve PIE du monde exterieur (geopolitical-world-001) : l'experience CRISE LOINTAINE.

Lance PIE sur Lvl_AnastasisSlice, charge le scenario geo-pontos-1204 (Anastasis.Geo.Load), puis
injecte une crise a Konya, un noeud lointain relie au village par plusieurs relais :
    Anastasis.Geo.Inject konya TradeDisruption=1 Migration=0.8 info=1 id=remote-crisis-proof
et fait avancer la simulation jour par jour (Anastasis.Sim.Advance 1d : la preuve n'attend pas le
temps simule, elle l'avance). Chaque jour, elle lit l'etat par
AnastasisSimulationDebugLibrary.get_geo_status (verite, exposition, savoir, groupes, arrivees au
village avec leur cause) et le nombre d'habitants simules.

Verdict GEO_PIE PASS si, pour CETTE cause (le choc historique de 1204 du scenario tourne aussi,
il est filtre par sa cause) :
  T0  le jour de l'injection, rien au village : ni arrivee, ni savoir ;
  T1  une nouvelle (rumor) arrive d'abord ;
  T2  puis la penurie (TradeDisruption) arrive au village, plus tard, et l'exposition monte ce jour-la ;
  T3  puis un groupe de migrants arrive et devient des habitants (le nombre d'habitants simules
      augmente exactement des identifiants admis ce jour-la) ;
  la trace de la penurie au village remonte a remote-crisis-proof ;
  Anastasis.Geo.Save puis Anastasis.Geo.Restore pendant que des paquets sont en route rendent
  un etat identique, et la suite arrive quand meme.
Les jours et valeurs observes sont ecrits dans ANASTASIS_GEO_PIE_OUT/geo-remote-crisis.json.

Lancement : au lot (editor-batch, `geo-remote-crisis-pie`), ou `py tools/unreal/geo-remote-crisis-pie.py`
dans un editeur ouvert. Rien n'est sauve dans Content/.
  ANASTASIS_GEO_PIE_OUT       dossier de sortie (defaut Saved/GeoEvidence/pie)
  ANASTASIS_GEO_PIE_MAX_DAYS  jours simules au plus (defaut 70)
  ANASTASIS_GEO_PIE_SECONDS   plafond en secondes REELLES (defaut 480)
"""
import json, os, time, unreal

LEVEL = '/Game/Anastasis/Maps/Lvl_AnastasisSlice'
CAUSE = 'remote-crisis-proof'
MAX_DAYS = int(os.environ.get('ANASTASIS_GEO_PIE_MAX_DAYS', '70'))
SECONDS = float(os.environ.get('ANASTASIS_GEO_PIE_SECONDS', '480'))
OUT = os.environ.get('ANASTASIS_GEO_PIE_OUT') or os.path.join(unreal.Paths.project_saved_dir(), 'GeoEvidence', 'pie')

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
lib = unreal.AnastasisSimulationDebugLibrary
unreal.log('GEO_PIE_MAP_LOAD=' + str(les.load_level(LEVEL)))

t0 = time.monotonic()
state = {'phase': 0, 'started': 0.0, 'wait': 0.0, 'days': 0}
timeline = []
record = {'cause': CAUSE, 'checks': {}, 'timeline': timeline}
handle = None


def world():
    return ues.get_game_world()


def cmd(text):
    unreal.SystemLibrary.execute_console_command(world(), text)


def status():
    raw = lib.get_geo_status(world()) if world() else '{}'
    try:
        return json.loads(raw)
    except Exception as exc:  # un etat illisible est un echec, pas un silence
        unreal.log_error('GEO_PIE status illisible: %s %s' % (exc, raw[:200]))
        return {}


def mine(items, key='cause'):
    return [x for x in items if x.get(key) == CAUSE]


def snapshot(st):
    geo = st.get('geo', {})
    return {
        'simDay': st.get('simDay'),
        'geoDay': geo.get('day'),
        'actors': st.get('actors'),
        'exposure': geo.get('exposure', {}),
        'knowledge': mine(geo.get('knowledge', [])),
        'arrivals': mine(geo.get('villageArrivals', [])),
        'batches': mine(geo.get('batches', [])),
        'allBatches': geo.get('batches', []),
    }


def finish(msg, error=False):
    (unreal.log_error if error else unreal.log)(msg)
    unreal.unregister_slate_post_tick_callback(handle)
    unreal.SystemLibrary.quit_editor()


def judge():
    c = record['checks']
    t = timeline
    inject = record.get('injectSnapshot', {})
    c['T0_nothing_on_inject_day'] = not inject.get('arrivals') and not inject.get('knowledge')
    rumor = next((k for s in t for k in s['knowledge'] if k.get('sourceType') == 'rumor'), None)
    trade = next((a for s in t for a in s['arrivals'] if a.get('pressure') == 'TradeDisruption'), None)
    batch = next((b for s in t for b in s['batches'] if b.get('status') == 'admitted'), None)
    record['observed'] = {
        'injectDay': record.get('injectDay'),
        'rumorDay': rumor and rumor.get('learnedDay'), 'rumorReliability': rumor and rumor.get('reliability'),
        'rumorReported': rumor and rumor.get('reported'),
        'tradeDay': trade and trade.get('day'), 'tradeMagnitude': trade and trade.get('magnitude'), 'tradeFrom': trade and trade.get('from'),
        'migrationDay': batch and batch.get('day'), 'migrationPersons': batch and batch.get('persons'),
        'admittedIds': batch and batch.get('admitted'),
    }
    o = record['observed']
    c['T1_rumor_arrives'] = rumor is not None
    c['T2_trade_after_rumor'] = bool(rumor and trade and trade['day'] > rumor['learnedDay'])
    # L'exposition du village monte le jour de l'arrivee, par rapport a la veille (lu dans la chronologie).
    rises = False
    for prev, cur in zip(t, t[1:]):
        if trade and cur['geoDay'] == trade['day']:
            before_v = prev['exposure'].get('TradeDisruption', 0.0)
            after_v = cur['exposure'].get('TradeDisruption', 0.0)
            o['tradeExposureBefore'], o['tradeExposureAfter'] = before_v, after_v
            rises = after_v > before_v
    c['T2_exposure_rises'] = rises
    c['T3_migration_after_trade'] = bool(trade and batch and batch['day'] > trade['day'])
    # Habitants : la difference au jour de l'admission egale la somme des identifiants admis ce jour (toutes causes).
    grew = False
    for prev, cur in zip(t, t[1:]):
        if batch and cur['geoDay'] == batch['day']:
            admitted_today = sum(len(b.get('admitted', [])) for b in cur['allBatches'] if b.get('day') == cur['geoDay'] and b.get('status') == 'admitted')
            o['actorsBefore'], o['actorsAfter'], o['admittedToday'] = prev['actors'], cur['actors'], admitted_today
            grew = admitted_today > 0 and cur['actors'] - prev['actors'] == admitted_today
    c['T3_village_population_grows'] = grew
    c['trace_reaches_cause'] = CAUSE in record.get('trace', '')
    c['save_restore_identical'] = record.get('saveRestoreIdentical', False)
    ok = all(c.values())
    record['verdict'] = 'PASS' if ok else 'FAIL'
    os.makedirs(OUT, exist_ok=True)
    with open(os.path.join(OUT, 'geo-remote-crisis.json'), 'w', encoding='utf-8') as f:
        json.dump(record, f, ensure_ascii=False, indent=2)
    unreal.log('GEO_PIE_TIMELINE inject=%s rumor=%s trade=%s(m=%s via %s) migration=%s persons=%s actors=%s->%s' % (
        o['injectDay'], o['rumorDay'], o['tradeDay'], o['tradeMagnitude'], o['tradeFrom'], o['migrationDay'],
        o['migrationPersons'], o.get('actorsBefore'), o.get('actorsAfter')))
    unreal.log('GEO_PIE %s %s' % (record['verdict'], ' '.join('%s=%s' % (k, v) for k, v in c.items())))


def tick(_dt):
    now = time.monotonic()
    p = state['phase']
    if p == 0 and now - t0 > 3.0:
        state['phase'] = 1
        les.editor_request_begin_play()
    elif p == 1 and les.is_in_play_in_editor() and world() and lib.get_simulation_time(world()) >= 0:
        state['phase'], state['started'], state['wait'] = 2, now, now
    elif p == 2 and now - state['wait'] > 6.0:
        # Le village du lancement a eu le temps de se poser ; le monde exterieur arrive.
        cmd('Anastasis.Geo.Load')
        st = status()
        if not st.get('geo', {}).get('loaded'):
            record['error'] = 'scenario non charge'
            judge()
            state['phase'] = 9
            les.editor_request_end_play()
            return
        cmd('Anastasis.Geo.Inject konya TradeDisruption=1 Migration=0.8 info=1 id=%s label=crise_lointaine_preuve tags=crise,konya' % CAUSE)
        snap = snapshot(status())
        record['injectDay'] = snap['geoDay']
        record['injectSnapshot'] = snap
        timeline.append(snap)
        unreal.log('GEO_PIE_INJECT day=%s actors=%s' % (snap['geoDay'], snap['actors']))
        state['phase'] = 3
    elif p == 3:
        last = timeline[-1]
        rumor_known = any(k.get('sourceType') == 'rumor' for k in last['knowledge'])
        trade_arrived = any(a.get('pressure') == 'TradeDisruption' for a in last['arrivals'])
        if rumor_known and not trade_arrived and 'saveRestoreIdentical' not in record:
            # Persistance a chaud : des paquets de la cause sont encore en route.
            before = lib.get_geo_status(world())
            cmd('Anastasis.Geo.Save geo-pie-proof')
            cmd('Anastasis.Geo.Restore geo-pie-proof')
            after = lib.get_geo_status(world())
            record['saveRestoreIdentical'] = before == after
            record['saveRestoreDay'] = last['geoDay']
            unreal.log('GEO_PIE_SAVE_RESTORE day=%s identical=%s' % (last['geoDay'], before == after))
        done = any(b.get('status') == 'admitted' for b in last['batches'])
        if done or state['days'] >= MAX_DAYS:
            record['trace'] = lib.get_geo_trace(world(), 'village', 'TradeDisruption')
            for line in record['trace'].splitlines():
                unreal.log('GEO_PIE_TRACE ' + line)
            judge()
            state['phase'] = 9
            les.editor_request_end_play()
            return
        cmd('Anastasis.Sim.Advance 1d')
        state['days'] += 1
        snap = snapshot(status())
        timeline.append(snap)
        if snap['knowledge'] or snap['arrivals'] or snap['batches']:
            unreal.log('GEO_PIE_DAY day=%s actors=%s trade=%.3f migration=%.3f knowledge=%d arrivals=%d batches=%d' % (
                snap['geoDay'], snap['actors'], snap['exposure'].get('TradeDisruption', 0), snap['exposure'].get('Migration', 0),
                len(snap['knowledge']), len(snap['arrivals']), len(snap['batches'])))
    elif p == 9 and not les.is_in_play_in_editor():
        finish('GEO_PIE_COMPLETE')
        return
    if p in (2, 3) and now - state['started'] > SECONDS:
        record['error'] = 'timeout'
        judge()
        unreal.log('GEO_PIE FAIL timeout days=%d' % state['days'])
        state['phase'] = 9
        les.editor_request_end_play()
    elif now - t0 > SECONDS + 180:
        finish('GEO_PIE_TIMEOUT phase=%d' % p, True)


handle = unreal.register_slate_post_tick_callback(tick)
