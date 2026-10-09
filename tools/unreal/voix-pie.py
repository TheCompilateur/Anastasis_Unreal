"""Preuve PIE d'une voix au conseil (voix-conseil-001, ecart n°54).

Lance PIE sur Lvl_AnastasisSlice : Valmire fondee, le monde exterieur charge avec elle. Le joueur arrive
(`Anastasis.Player.Arrive`) et vit soixante jours par pas de six heures (`Anastasis.Sim.Advance 6h`) en joueur
difficile : il dort, boit ou mange quand son corps le demande, sinon il va parler ; il dit non au premier
conseil, oui aux suivants (`Anastasis.Player.Vote`) ; il refuse la premiere demande d'aide, laisse la deuxieme
sans reponse, accepte les autres (`Anastasis.Player.Help`) ; au jour 20 il decide de batir (`Anastasis.Player.Build`)
et va demander de l'aide a deux habitants par jour pendant quatre jours (`Anastasis.Player.Ask`), avant de lever sa
cabane (ma-cabane-001 : seul, elle monte en quelques heures). Puis la chronique et le carnet sont
ecrits dans Saved/Chronicle/, et l'etat des arrivants et du joueur dans Saved/VoiceEvidence/pie/voice.json.

Verdict VOIX_PIE PASS si (le « fini quand » d'Alexandre, VOIX_CONSEIL_001.md) :
  - la chronique raconte une voix du joueur au conseil ;
  - le joueur a refuse (ou laisse sans reponse) une demande d'aide ;
  - le joueur s'est vu refuser une aide a cause de ce qu'il a fait ou de ce qu'on dit de lui
    (`refus_rendu`, `on_dit`, `porte_fermee`) ;
  - son carnet a entendu au moins une histoire sur lui, dans son dos ;
  - le joueur est vivant au bout des soixante jours, et les fichiers sont ecrits.
Ce n'est pas un verdict sur l'interet du recit : celui-la, Alexandre le donne en le lisant.

Lancement : `py tools/unreal/voix-pie.py` dans un editeur ouvert, ou au lot (editor-batch, `voix-pie`). Rien n'est sauve.
  ANASTASIS_VOICE_DAYS     jours (defaut 60)
  ANASTASIS_VOICE_SECONDS  plafond en secondes REELLES (defaut 540)
"""
import json, os, time, unreal

LEVEL = '/Game/Anastasis/Maps/Lvl_AnastasisSlice'
DAYS = int(os.environ.get('ANASTASIS_VOICE_DAYS', '60'))
SECONDS = float(os.environ.get('ANASTASIS_VOICE_SECONDS', '540'))
OUT = os.path.join(unreal.Paths.project_saved_dir(), 'VoiceEvidence', 'pie')
BUILD_DAY = 20
ASK_DAYS = 4

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
lib = unreal.AnastasisSimulationDebugLibrary
unreal.log('VOIX_PIE_MAP_LOAD=' + str(les.load_level(LEVEL)))

t0 = time.monotonic()
phase, handle, step = 0, None, 0
voted, answered, asked = set(), [0], [0]


def status(raw):
    try:
        return json.loads(raw) if raw else {}
    except ValueError:
        return {}


def cmd(world, text):
    unreal.SystemLibrary.execute_console_command(world, text)


def finish(msg, error=False):
    (unreal.log_error if error else unreal.log)(msg)
    unreal.unregister_slate_post_tick_callback(handle)
    unreal.SystemLibrary.quit_editor()


def act(world):
    """Six heures de la vie d'un joueur difficile."""
    day = 1 + step // 4
    player = status(lib.get_player_status(world))
    arrivals = status(lib.get_arrivals_status(world))
    # Le corps d'abord : c'est au joueur de choisir le remede (ecart n°21).
    if player.get('body'):
        goal = player['body']
    elif player.get('energy', 100) <= 25:
        goal = 'rest'
    elif player.get('thirst', 0) >= 40:
        goal = 'drink'
    elif player.get('hunger', 0) >= 40:
        goal = 'eat'
    elif 'build' in player.get('options', []) and step % 2 == 0 and not (BUILD_DAY <= day <= BUILD_DAY + ASK_DAYS):
        # Il a dit oui a quelqu'un (ou il batit chez lui) : il va au chantier, une fois sur deux. Sa cabane se leve
        # seul en quelques heures (ma-cabane-001) : il va d'abord demander, puis il batit.
        goal = 'build'
    else:
        goal = 'socialize'
    cmd(world, 'Anastasis.Player.Goal ' + goal)
    # Au conseil : non la premiere fois, oui ensuite.
    for group in arrivals.get('groups', []):
        if group.get('guest') and group.get('family') not in voted:
            cmd(world, 'Anastasis.Player.Vote ' + ('non' if not voted else 'oui'))
            voted.add(group.get('family'))
            break
    # Les demandes d'aide : non a la premiere, le silence a la deuxieme (le soir suivant, c'est un refus), oui ensuite.
    oldest = arrivals.get('player', {}).get('oldest_pending', 0)
    if oldest == 1:
        cmd(world, 'Anastasis.Player.Help non')
        answered[0] += 1
    elif oldest >= 3:
        cmd(world, 'Anastasis.Player.Help oui')
        answered[0] += 1
    # Sa maison : il decide au jour 20, puis demande a deux habitants chaque matin.
    if day == BUILD_DAY and step % 4 == 0:
        cmd(world, 'Anastasis.Player.Build')
    if BUILD_DAY < day <= BUILD_DAY + ASK_DAYS and step % 4 == 1:
        for _ in range(2):
            cmd(world, 'Anastasis.Player.Ask npc-%d' % (asked[0] % 14))
            asked[0] += 1


def verdict(world):
    chronicle = status(lib.get_chronicle_status(world))
    notebook = status(lib.get_notebook_status(world))
    arrivals = status(lib.get_arrivals_status(world))
    player = status(lib.get_player_status(world))
    chronicle_path = lib.write_chronicle(world, 'voix-pie-%d-jours.txt' % DAYS)
    notebook_path = lib.write_notebook(world, 'carnet-voix-%d-jours.txt' % DAYS)
    text = ''
    if chronicle_path and os.path.isfile(chronicle_path):
        with open(chronicle_path, encoding='utf-8') as f:
            text = f.read()
    me = arrivals.get('player', {})
    help_log = me.get('help', [])
    pid = me.get('id')
    checks = {
        'vote': 'le nouveau : ' in text,
        'refused_by_player': any(h.get('to') == pid and not h.get('accepted') for h in help_log),
        'silence': any(h.get('to') == pid and h.get('reason') == 'silence' for h in help_log),
        'refused_for_deeds': any(h.get('from') == pid and not h.get('accepted') and h.get('reason') in ('refus_rendu', 'on_dit', 'porte_fermee') for h in help_log),
        'heard_behind_back': notebook.get('about_me', 0) >= 1,
        'player_alive': bool(player.get('player')),
        'files': bool(chronicle_path) and os.path.isfile(chronicle_path) and bool(notebook_path) and os.path.isfile(notebook_path),
    }
    ok = all(checks.values())
    os.makedirs(OUT, exist_ok=True)
    with open(os.path.join(OUT, 'voice.json'), 'w', encoding='utf-8') as f:
        json.dump({'arrivals': arrivals, 'notebook': notebook, 'player': player, 'checks': checks}, f, ensure_ascii=False, indent=1)
    reasons = {}
    for h in help_log:
        if h.get('from') == pid:
            key = '%s:%s' % ('oui' if h.get('accepted') else 'non', h.get('reason'))
            reasons[key] = reasons.get(key, 0) + 1
    unreal.log('VOIX_PIE %s votes=%d asks_to_me=%d my_asks=%s about_me=%s deeds=%s notes=%s checks=%s' % (
        'PASS' if ok else 'FAIL', len(voted), sum(1 for h in help_log if h.get('to') == pid), json.dumps(reasons),
        notebook.get('about_me'), notebook.get('deeds'), notebook.get('notes'),
        ','.join('%s=%d' % (k, int(v)) for k, v in checks.items())))


def tick(_dt):
    global phase, step
    now = time.monotonic()
    if phase == 0 and now - t0 > 3.0:
        phase = 1
        les.editor_request_begin_play()
    elif phase == 1 and les.is_in_play_in_editor():
        phase = 2
        unreal.log('VOIX_PIE_ACTIVE days=%d' % DAYS)
    elif phase == 2:
        world = ues.get_game_world()
        if world and status(lib.get_chronicle_status(world)).get('started'):
            cmd(world, 'Anastasis.Player.Arrive')
            phase = 3
    elif phase == 3:
        # Six heures par frame : la chronique et le carnet lisent le village entre deux sauts.
        world = ues.get_game_world()
        act(world)
        cmd(world, 'Anastasis.Sim.Advance 6h')
        step += 1
        if step >= DAYS * 4:
            phase = 4
    elif phase == 4:
        verdict(ues.get_game_world())
        phase = 5
        les.editor_request_end_play()
    elif phase == 5 and not les.is_in_play_in_editor():
        finish('VOIX_PIE_COMPLETE')
        return
    if phase in (1, 2, 3) and now - t0 > SECONDS:
        unreal.log('VOIX_PIE FAIL timeout phase=%d step=%d' % (phase, step))
        phase = 5
        les.editor_request_end_play()
    elif now - t0 > SECONDS + 120:
        finish('VOIX_PIE_TIMEOUT phase=%d' % phase, True)


handle = unreal.register_slate_post_tick_callback(tick)
