"""Preuve en scene de la collision reelle du joueur a la main (mission player-collision-reelle-001, ecart n°60).

Charge Lvl_AnastasisSlice, lance un PIE et mesure jusqu'ou le joueur peut marcher a la main (`Anastasis.Player.Move dx dy`,
redirige a chaque image vers sa cible) jusqu'a ce qu'il n'avance plus.

LE PUITS. Il part a 3 tuiles (60 m) du puits, au nord, au sud, a l'est puis a l'ouest, et marche vers son centre :

    anastasis.Player.Collision 0   la regle de la reference : la tuile du puits (20 m) est un mur, il s'arrete a plus de
                                   9 m du centre du puits
    anastasis.Player.Collision 1   la collision reelle : la capsule du pawn est balayee contre le puits rendu, il s'en
                                   approche a moins de 6 m, et d'au moins 3 m de plus qu'avec la regle de la reference

LES ARBRES (mesure, sans verdict). Avec la collision reelle, il part a 30 m de quatre troncs rendus (instances SM_Tree_* du
monde, a 25-120 m du puits, distants de 30 m au moins) et marche droit vers le pied de chacun. Les lignes MEASURE et WHAT_*
consignent a quelle distance du pied il s'arrete et quel composant le bloque ou le recouvre. Ce n'est pas un controle : la
collision simple des arbres (NDOP10_X de create_tree_asset.py) est un volume de la taille de l'arbre entier, pas un tronc ;
le verdict attend une collision de tronc (docs/unreal/PLAYER_COLLISION_001.md).

Controles de chaque marche : le pawn reste sur le corps simule (moins de 5 uu d'ecart) ; pour la collision reelle, le corps
n'est jamais pris dans la geometrie rendue (recouvrement d'une capsule levee d'un pas de marche, quand l'API le permet).
Le temps est avance par le rythme (`anastasis.Sim.TimeScale`), jamais attendu au mur. Aucun asset sauve.

    UnrealEditor.exe <uproject> -nosplash -NoLiveCoding -abslog=<log> -ExecCmds="py tools/unreal/player-collision-pie.py"

Lignes PLAYER_COLLISION_PIE du log ; PLAYER_COLLISION_PIE PASS si chaque controle tient.
"""
import json
import math
import os
import time

import unreal

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
unreal.log('PLAYER_COLLISION_PIE_MAP_LOAD=' + str(les.load_level('/Game/Anastasis/Maps/Lvl_AnastasisSlice')))

DBG = unreal.AnastasisSimulationDebugLibrary
OUT = os.environ.get('ANASTASIS_PLAYER_COLLISION_OUT', '')

# Puits : depart a 3 tuiles, par les quatre cotes (decalage de tuile).
SIDES = [('nord', 3, 0), ('sud', -3, 0), ('est', 0, 3), ('ouest', 0, -3)]
WELL_PLAN = [(0, i) for i in range(len(SIDES))] + [(1, i) for i in range(len(SIDES))]
TREE_COUNT = 4            # troncs essayes
TREE_MIN_M, TREE_MAX_M = 25.0, 120.0   # distance au puits des troncs choisis
TREE_SPREAD_M = 30.0      # distance mini entre deux troncs choisis
TREE_START_M = 30.0       # depart a cette distance du tronc, vers le puits
TREE_SCAN_CM = 15000.0    # rayon de recensement des troncs autour du puits
TREE_NEAR_CM = 4000.0     # troncs retenus autour de la cible pour mesurer le plus proche
STALL_TICKS = 40          # images sans progression : il est arrete
MAX_TICKS = 900           # plafond d'une marche
SETTLE_TICKS = 6          # images apres l'arrivee, avant de partir

t0 = time.monotonic()
phase = 0
step = 0
mark = 0.0
handle = None
checks = []
samples = {}
runs = []
run = {}
cell = 0.0
well_tile = (0, 0)
well_uu = (0.0, 0.0)
tick_no = 0
tree_targets = []
tree_all = []


def check(name, ok, detail=''):
    checks.append((name, bool(ok)))
    unreal.log('PLAYER_COLLISION_PIE CHECK %s %s %s' % ('OK' if ok else 'FAIL', name, detail))


def finish(message):
    unreal.log(message)
    passed = bool(checks) and all(ok for _, ok in checks)
    unreal.log('PLAYER_COLLISION_PIE %s checks=%d failed=%d' % ('PASS' if passed else 'FAIL', len(checks),
                                                                  sum(1 for _, ok in checks if not ok)))
    if OUT:
        try:
            os.makedirs(OUT, exist_ok=True)
            with open(os.path.join(OUT, 'player-collision.json'), 'w') as f:
                json.dump({'checks': checks, 'runs': runs, 'samples': samples}, f, indent=2)
        except Exception as e:  # la preuve est dans le log ; le JSON n'est qu'une copie
            unreal.log_warning('PLAYER_COLLISION_PIE json not written: %s' % e)
    unreal.unregister_slate_post_tick_callback(handle)
    unreal.SystemLibrary.quit_editor()


def cmd(world, text):
    unreal.log('PLAYER_COLLISION_PIE_CMD ' + text)
    unreal.SystemLibrary.execute_console_command(world, text)


def status(world):
    raw = DBG.get_player_status(world)
    return json.loads(raw) if raw else {}


def pawn_gap(world, s):
    """Distance au sol entre le pawn et le corps simule, en unites Unreal."""
    pawn = unreal.GameplayStatics.get_player_pawn(world, 0)
    if not pawn or 'ux' not in s:
        return 1e9
    loc = pawn.get_actor_location()
    return ((loc.x - s['ux']) ** 2 + (loc.y - s['uy']) ** 2) ** 0.5


def embedded_in_geometry(world):
    """Vrai/faux si une capsule levee d'un pas de marche, a la place du pawn, recouvre un acteur statique du monde ;
    None si l'API n'est pas disponible dans cette version (le controle est alors seulement signale)."""
    pawn = unreal.GameplayStatics.get_player_pawn(world, 0)
    if not pawn:
        return None
    try:
        capsule = pawn.get_component_by_class(unreal.CapsuleComponent)
        radius = capsule.get_scaled_capsule_radius()
        half = capsule.get_scaled_capsule_half_height()
        loc = pawn.get_actor_location()
        step_up = 45.0
        shape_half = max(radius, half - step_up * 0.5)
        centre = unreal.Vector(loc.x, loc.y, loc.z - half + step_up + shape_half)
        # Decor statique seul (ObjectTypeQuery1 = WorldStatic) : les villageois rendus ne comptent pas.
        hit = unreal.SystemLibrary.capsule_trace_single_for_objects(
            world, centre, unreal.Vector(centre.x, centre.y, centre.z + 0.1), radius, shape_half,
            [unreal.ObjectTypeQuery.OBJECT_TYPE_QUERY1], False, [pawn], unreal.DrawDebugTrace.NONE, True)
        return bool(hit[0]) if isinstance(hit, tuple) else bool(hit)
    except Exception as e:
        unreal.log('PLAYER_COLLISION_PIE overlap probe unavailable: %r' % (e,))
        return None


OBJECT_TYPES = [('WorldStatic', unreal.ObjectTypeQuery.OBJECT_TYPE_QUERY1),
                ('WorldDynamic', unreal.ObjectTypeQuery.OBJECT_TYPE_QUERY2),
                ('Pawn', unreal.ObjectTypeQuery.OBJECT_TYPE_QUERY3),
                ('PhysicsBody', unreal.ObjectTypeQuery.OBJECT_TYPE_QUERY4)]


def what_is_there(world, direction, length_cm):
    """Diagnostic : qu'est-ce que la capsule du pawn (levee d'un pas de marche, comme celle du crochet) touche, sur place
    (length 0) ou en avancant de `length_cm` vers `direction` (xy) ? Un texte par type d'objet touche."""
    pawn = unreal.GameplayStatics.get_player_pawn(world, 0)
    if not pawn:
        return ['no pawn']
    out = []
    try:
        capsule = pawn.get_component_by_class(unreal.CapsuleComponent)
        radius = capsule.get_scaled_capsule_radius()
        half = capsule.get_scaled_capsule_half_height()
        loc = pawn.get_actor_location()
        step_up = 45.0
        shape_half = max(radius, half - step_up * 0.5)
        z = loc.z - half + step_up + shape_half
        start = unreal.Vector(loc.x, loc.y, z)
        end = unreal.Vector(loc.x + direction[0] * length_cm, loc.y + direction[1] * length_cm, z + (0.1 if length_cm == 0 else 0.0))
        for name, query in OBJECT_TYPES:
            hit = unreal.SystemLibrary.capsule_trace_single_for_objects(
                world, start, end, radius, shape_half, [query], False, [pawn], unreal.DrawDebugTrace.NONE, True)
            if not (hit[0] if isinstance(hit, tuple) else hit):
                continue
            # Selon la version l'API rend (bool, HitResult) ou le HitResult seul.
            hr = hit[-1] if isinstance(hit, tuple) else hit
            try:
                t = hr.to_tuple()
                comp = t[10]
                mesh = ''
                try:
                    mesh = comp.static_mesh.get_name()
                except Exception:
                    pass
                text = 'initialOverlap=%s time=%.4f at=(%.0f,%.0f,%.0f) normal=(%.2f,%.2f,%.2f) actor=%s comp=%s class=%s mesh=%s' % (
                    t[1], t[2], t[4].x, t[4].y, t[4].z, t[7].x, t[7].y, t[7].z,
                    t[9].get_name() if t[9] else None, comp.get_name() if comp else None,
                    comp.get_class().get_name() if comp else None, mesh)
            except Exception as e:
                text = 'unreadable hit %r: %s' % (e, repr(hr)[:300])
            out.append('%s: %s' % (name, text[:700]))
    except Exception as e:
        out.append('probe unavailable: %r' % (e,))
    return out


def collect_trees(world, centre):
    """Positions monde (cm) des troncs rendus (instances SM_Tree_*) a moins de TREE_SCAN_CM de `centre`."""
    out = []
    kinds = {}
    for actor in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor):
        if 'AnastasisWorldEmbodiment' not in actor.get_class().get_name():
            continue
        for comp in actor.get_components_by_class(unreal.HierarchicalInstancedStaticMeshComponent):
            mesh = comp.static_mesh
            if not mesh or not mesh.get_name().startswith('SM_Tree_'):
                continue
            ids = comp.get_instances_overlapping_sphere(unreal.Vector(centre[0], centre[1], 0.0), TREE_SCAN_CM, True)
            if not ids:
                continue
            kinds[mesh.get_name()] = kinds.get(mesh.get_name(), 0) + len(ids)
            for i in ids:
                r = comp.get_instance_transform(i, True)
                tr = r[-1] if isinstance(r, tuple) else r
                out.append((tr.translation.x, tr.translation.y))
    unreal.log('PLAYER_COLLISION_PIE trees within %.0f m of the well: %d %s' % (TREE_SCAN_CM / 100.0, len(out), json.dumps(kinds)))
    return out


def choose_trees(trees, centre):
    """Quatre troncs a 25-120 m du puits, distants de 30 m au moins les uns des autres, du plus proche au plus lointain."""
    def dist(p, q):
        return math.hypot(p[0] - q[0], p[1] - q[1]) / 100.0
    pool = sorted((t for t in trees if TREE_MIN_M <= dist(t, centre) <= TREE_MAX_M), key=lambda t: dist(t, centre))
    chosen = []
    for t in pool:
        if all(dist(t, c) >= TREE_SPREAD_M for c in chosen):
            chosen.append(t)
        if len(chosen) >= TREE_COUNT:
            break
    return chosen


def tick(dt):
    global phase, step
    try:
        _tick()
    except Exception as e:
        check('no python exception', False, repr(e))
        phase = 3
        les.editor_request_end_play()


def begin_run(world, collision, label, target, end_m, near):
    """Une marche : on lache l'ancien corps, on arrive a l'endroit voulu, avec le reglage voulu. `target` en cm monde."""
    global run, tick_no, step
    cmd(world, 'Anastasis.Player.Move 0 0')
    cmd(world, 'Anastasis.Player.Release')
    cmd(world, 'anastasis.Player.Collision %d' % collision)
    run = {'collision': collision, 'label': label, 'target': [target[0], target[1]], 'endM': end_m, 'near': near,
           'minD': 1e9, 'bestTick': 0, 'ticks': 0, 'stalled': False, 'nearMin': 1e9}
    tick_no = 0
    step = 3


def _tick():
    global phase, step, mark, cell, well_tile, well_uu, run, tick_no, tree_targets, tree_all
    now = time.monotonic()
    if now - t0 > 570:
        finish('PLAYER_COLLISION_PIE_TIMEOUT phase=%d step=%d run=%d' % (phase, step, len(runs)))
        return
    if phase == 0 and now - t0 > 3:
        phase = 1
        les.editor_request_begin_play()
        return
    if phase == 1 and les.is_in_play_in_editor():
        phase = 2
        mark = now
        unreal.log('PLAYER_COLLISION_PIE_ACTIVE')
        return
    if phase == 3 and not les.is_in_play_in_editor():
        finish('PLAYER_COLLISION_PIE_COMPLETE')
        return
    if phase != 2:
        return

    world = ues.get_game_world()
    if not world:
        return
    if DBG.get_simulation_time(world) < 0:
        return

    if step == 0 and now - mark > 2.0:
        # Un pas de 20 m a 20 m/s : la marche dure quelques secondes ; le rythme est pose, le temps n'est pas attendu.
        cmd(world, 'anastasis.Sim.TimeScale 0.25')
        cmd(world, 'Anastasis.Player.Arrive')
        mark = now
        step = 1
        return

    if step == 1 and now - mark > 0.5:
        s = status(world)
        check('arrived and incarnated', s.get('player', '') != '', s.get('player'))
        check('a well exists', s.get('well') is True)
        samples['arrived'] = s
        if not s.get('player') or s.get('x', 0) <= 1.0:
            check('can derive the tile size', False, 'x=%s' % s.get('x'))
            phase = 3
            les.editor_request_end_play()
            return
        cell = s['ux'] / s['x']
        well_uu = (s['wx'], s['wy'])
        well_tile = (int(math.floor(s['wx'] / cell)), int(math.floor(s['wy'] / cell)))
        samples['cell_uu'] = cell
        samples['well_tile'] = well_tile
        unreal.log('PLAYER_COLLISION_PIE well tile=%s cell=%.1f uu' % (well_tile, cell))
        step = 2
        return

    if step == 2:
        # Les marches vers le puits, puis vers les troncs.
        if len(runs) < len(WELL_PLAN):
            collision, side = WELL_PLAN[len(runs)]
            name, dx, dy = SIDES[side]
            begin_run(world, collision, 'puits/%s' % name, well_uu, 0.8, [])
            cmd(world, 'Anastasis.Player.Arrive %d %d' % (well_tile[0] + dx, well_tile[1] + dy))
            return
        k = len(runs) - len(WELL_PLAN)
        if k < len(tree_targets):
            tx, ty = tree_targets[k]
            wx, wy = well_uu
            norm = max(1.0, math.hypot(wx - tx, wy - ty))
            sx = tx + (wx - tx) / norm * TREE_START_M * 100.0
            sy = ty + (wy - ty) / norm * TREE_START_M * 100.0
            near = [t for t in tree_all if math.hypot(t[0] - tx, t[1] - ty) <= TREE_NEAR_CM]
            # Lentement (4,8 m/s) : a 20 m/s une image saute plus de 0,3 m et peut enjamber un tronc sans le mesurer.
            cmd(world, 'anastasis.Sim.TimeScale 0.06')
            begin_run(world, 1, 'tronc/%d' % k, (tx, ty), 0.15, near)
            cmd(world, 'Anastasis.Player.Arrive %d %d' % (int(math.floor(sx / cell)), int(math.floor(sy / cell))))
            return
        step = 9
        return

    if step == 3:
        tick_no += 1
        if tick_no >= SETTLE_TICKS:
            s = status(world)
            run['startD'] = math.hypot(s.get('ux', 0) - run['target'][0], s.get('uy', 0) - run['target'][1]) / 100.0
            run['start'] = [s.get('x'), s.get('y')]
            check('run %s/%d: arrived and pawn bound' % (run['label'], run['collision']),
                  s.get('player', '') != '' and s.get('pawn') is True
                  and run['startD'] > (10.0 if run['label'].startswith('puits') else 3.0),
                  'startD=%.1f m' % run['startD'])
            tick_no = 0
            step = 4
        return

    if step == 4:
        tick_no += 1
        s = status(world)
        if s.get('player', '') == '':
            check('run %s/%d: still incarnated' % (run['label'], run['collision']), False)
            step = 5
            return
        tx, ty = run['target']
        d = math.hypot(s['ux'] - tx, s['uy'] - ty) / 100.0
        if d < run['minD'] - 0.02:
            run['minD'] = d
            run['bestTick'] = tick_no
            run['at'] = [s['x'], s['y']]
        if run['near']:
            run['nearMin'] = min(run['nearMin'], min(math.hypot(s['ux'] - t[0], s['uy'] - t[1]) for t in run['near']) / 100.0)
        run['ticks'] = tick_no
        stalled = tick_no - run['bestTick'] > STALL_TICKS
        if stalled or d < run['endM'] or tick_no > MAX_TICKS:
            run['stalled'] = stalled
            run['gap'] = pawn_gap(world, s)
            if run['collision'] == 1:
                run['embedded'] = embedded_in_geometry(world)
                # Diagnostic (ni seuil ni verdict) : ce qui recouvre le corps, et ce qui est devant lui vers la cible.
                ex, ey = tx - s['ux'], ty - s['uy']
                norm = max(1.0, math.hypot(ex, ey))
                for line in what_is_there(world, (0.0, 0.0), 0):
                    unreal.log('PLAYER_COLLISION_PIE WHAT_OVERLAPS %s %s' % (run['label'], line))
                for line in what_is_there(world, (ex / norm, ey / norm), 300.0):
                    unreal.log('PLAYER_COLLISION_PIE WHAT_AHEAD %s %s' % (run['label'], line))
            cmd(world, 'Anastasis.Player.Move 0 0')
            step = 5
            return
        ex, ey = tx - s['ux'], ty - s['uy']
        norm = math.hypot(ex, ey)
        unreal.SystemLibrary.execute_console_command(world, 'Anastasis.Player.Move %.4f %.4f' % (ex / norm, ey / norm))
        return

    if step == 5:
        runs.append(run)
        out = dict(run)
        out.pop('near', None)
        unreal.log('PLAYER_COLLISION_PIE RUN %s' % json.dumps(out))
        check('run %s/%d: pawn stays on the body' % (run['label'], run['collision']), run.get('gap', 1e9) < 5.0,
              'gap=%.1f uu' % run.get('gap', -1))
        if run['label'].startswith('puits'):
            if run['collision'] == 1 and run.get('embedded') is not None:
                check('run %s/1: body never embedded in the rendered geometry' % run['label'], run['embedded'] is False)
        else:
            # MESURE, pas verdict : un tronc rendu arrete-t-il le joueur au pied de l'arbre ? La collision simple des
            # arbres (NDOP10_X, create_tree_asset.py) est un volume de la taille de l'arbre entier, pas un tronc : les
            # oliviers arretent a ~3 m, les platanes recouvrent la capsule. Le verdict attend une collision de tronc.
            unreal.log('PLAYER_COLLISION_PIE MEASURE %s: stopped %.2f m from the trunk base, nearest trunk %.2f m, stalled=%s, '
                       'overlapping geometry=%s' % (run['label'], run['minD'], run['nearMin'], run['stalled'], run.get('embedded')))
        if len(runs) == len(WELL_PLAN):
            well_checks()
            samples_trees(world)
        step = 2
        return

    if step == 9:
        trunk = [r for r in runs if r['label'].startswith('tronc')]
        samples['trunk_stops_m'] = [round(r['minD'], 2) for r in trunk]
        samples['trunk_near_min_m'] = [round(r['nearMin'], 2) for r in trunk]
        unreal.log('PLAYER_COLLISION_PIE trunks: stoppedAt=%s nearMin=%s' % (samples['trunk_stops_m'], samples['trunk_near_min_m']))
        check('trunks: enough trunks tried', len(trunk) >= 2, 'tried=%d' % len(trunk))
        cmd(world, 'anastasis.Player.Collision 1')
        cmd(world, 'Anastasis.Player.Release')
        step = 10
    elif step == 10:
        phase = 3
        les.editor_request_end_play()


def well_checks():
    base = [r['minD'] for r in runs if r['label'].startswith('puits') and r['collision'] == 0]
    real = [r['minD'] for r in runs if r['label'].startswith('puits') and r['collision'] == 1]
    samples['minD_reference_rule'] = base
    samples['minD_real_collision'] = real
    unreal.log('PLAYER_COLLISION_PIE minD reference=%s real=%s' % (['%.2f' % v for v in base], ['%.2f' % v for v in real]))
    check('reference rule: the well tile is a wall (every side stops beyond 9 m)', bool(base) and min(base) > 9.0,
          'min=%.2f m' % (min(base) if base else -1))
    check('real collision: he reaches the well (a side within 6 m of its centre)', bool(real) and min(real) < 6.0,
          'min=%.2f m' % (min(real) if real else -1))
    check('real collision gets closer than the reference rule by at least 3 m',
          bool(base) and bool(real) and min(real) < min(base) - 3.0,
          'real=%.2f ref=%.2f' % (min(real) if real else -1, min(base) if base else -1))
    check('real collision: the well is not a ghost (never within 0.5 m of its centre)', bool(real) and min(real) > 0.5,
          'min=%.2f m' % (min(real) if real else -1))


def samples_trees(world):
    """Recense les troncs rendus autour du puits et choisit les cibles ; si l'API manque, la preuve le dit et echoue."""
    global tree_all, tree_targets
    try:
        tree_all = collect_trees(world, well_uu)
        tree_targets = choose_trees(tree_all, well_uu)
    except Exception as e:
        check('trunks: the rendered trunks can be listed', False, repr(e))
        tree_all, tree_targets = [], []
        return
    check('trunks: rendered trunks found around the well', len(tree_all) > 0, 'n=%d' % len(tree_all))
    unreal.log('PLAYER_COLLISION_PIE trunk targets (cm) %s' % json.dumps([[round(t[0]), round(t[1])] for t in tree_targets]))


handle = unreal.register_slate_post_tick_callback(tick)
