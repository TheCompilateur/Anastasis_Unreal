"""Preuve PIE de l'architecture a l'echelle humaine (ARCHITECTURE_SCALE_001, architecture-crusade-001).

Lance par architecture-pie.ps1 (editeur discret, rendu hors focus) ou par editor-batch (registre :
architecture-pie). Rien n'est sauve. Sortie : ANASTASIS_ARCH_OUT (defaut Saved/ArchitectureEvidence/pie).

PIE sur Lvl_AnastasisSlice. Le ciel est epingle (anastasis.Sky.Hour), la simulation gelee pendant les
prises (anastasis.Sim.TimeScale 0) : ce qui se juge ici, c'est le bati.

  1. AVANT : anastasis.Village.Architecture 0, `Anastasis.Village.Hamlet` -> les anciens meshes (tuile de
     4 m) aux memes cameras que l'apres : 00-avant-village, 00-avant-pnj-devant.
  2. APRES : anastasis.Village.Architecture 1, le meme hameau reecrit (memes ids, memes cases).
     Mesures : chaque batiment porte un mesh de /Game/Anastasis/VillageArchitecture et son assise ;
     decalage de terrasse |pad| < 480 cm (hauteur de l'assise) ; au moins deux typologies de maison.
  3. Mannequins de reference (le corps des habitants, Manny a l'echelle 0,93 = 167 cm, en idle) poses
     devant la maison, dans sa porte, dans la salle du foyer.
     Prises : 01-pnj-devant, 02-pnj-porte, 03-pnj-interieur, 04-maison-isolee, 05-groupe, 06-village,
     07..10 une par typologie presente (pauvre, ferme, grenier, puits), 11-village-haut.
Une verification qui echoue echoue ; jamais de PASS raconte.
"""
import json
import math
import os
import time
from pathlib import Path

import unreal

ROOT = Path(unreal.Paths.project_dir())
OUT = Path(os.environ.get('ANASTASIS_ARCH_OUT', str(ROOT / 'Saved' / 'ArchitectureEvidence' / 'pie')))
OUT.mkdir(parents=True, exist_ok=True)
SHOTS_DIR = ROOT / 'Saved' / 'Screenshots'
HOUR = os.environ.get('ANASTASIS_ARCH_HOUR', '9.5')
KIT = json.loads((ROOT / 'docs' / 'unreal' / 'architecture' / 'architecture-kit-001.json').read_text(encoding='utf-8'))
ARCH = {b['name']: b for b in KIT['buildings']}
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
DBG = unreal.AnastasisSimulationDebugLibrary
les.load_level('/Game/Anastasis/Maps/Lvl_AnastasisSlice')
cam0 = eas.spawn_actor_from_class(unreal.CameraActor, unreal.Vector(0, 0, 1000), unreal.Rotator())
cam0.set_actor_label('ArchitectureProofCamera')
cam0.camera_component.set_editor_property('field_of_view', 70.0)
cam0.camera_component.set_editor_property('constrain_aspect_ratio', False)


def find_asset(name_part, cls_name, root='/Game'):
    reg = unreal.AssetRegistryHelpers.get_asset_registry()
    flt = unreal.ARFilter(package_paths=[root], recursive_paths=True)
    for a in reg.get_assets(flt):
        if str(a.asset_class_path.asset_name) == cls_name and name_part.lower() in str(a.asset_name).lower():
            return a.get_asset()
    return None


IDLE = find_asset('MM_Idle', 'AnimSequence', '/Game/Characters') or find_asset('Idle', 'AnimSequence', '/Game/Characters')
MANNY = unreal.load_asset('/Game/Characters/Mannequins/Meshes/SKM_Manny') or find_asset('SKM_Manny', 'SkeletalMesh')
mannequins = []
for k in range(3):
    m = eas.spawn_actor_from_class(unreal.SkeletalMeshActor, unreal.Vector(0, 0, -100000 - k * 500), unreal.Rotator())
    m.set_actor_label('ArchitectureScaleMannequin_%d' % k)
    comp = m.skeletal_mesh_component
    if MANNY:
        comp.set_skeletal_mesh_asset(MANNY)
    m.set_actor_scale3d(unreal.Vector(.93, .93, .93))
    mannequins.append(m.get_actor_label())

t0 = time.monotonic()
state = {'phase': 0, 'step': 0, 'pending': None, 'shots': [], 'checks': [], 'failures': [], 'finished': False,
         'camera': None, 'men': [], 'wait': None, 'plan': None}
handle = None


def log(msg):
    unreal.log('ARCH_PIE ' + msg)


def fail(msg):
    state['failures'].append(msg)
    unreal.log_warning('ARCH_PIE FAIL ' + msg)


def finish(reason):
    if state['finished']:
        return
    state['finished'] = True
    expected = len(state['plan'] or []) + 2   # + les deux prises AVANT
    ok = not state['failures'] and expected > 0 and len(state['shots']) == expected and all(s['written'] for s in state['shots'])
    report = {'pass': ok, 'reason': reason, 'failures': state['failures'], 'shots': state['shots'], 'checks': state['checks']}
    (OUT / 'architecture-pie.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    log(('PASS ' if ok else 'FAIL ') + reason + ('' if ok else ' ' + json.dumps(state['failures'])))
    world = ues.get_game_world()
    if world:
        for cmd in ('anastasis.Sim.TimeScale 0.0375', 'anastasis.Village.Architecture 1'):
            unreal.SystemLibrary.execute_console_command(world, cmd)
    unreal.unregister_slate_post_tick_callback(handle)
    unreal.SystemLibrary.quit_editor()


def cmd(world, c):
    unreal.SystemLibrary.execute_console_command(world, c)


def buildings(world):
    out = []
    for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.AnastasisVillageBuilding):
        meshes = a.get_components_by_class(unreal.StaticMeshComponent)
        body = next((c for c in meshes if c.get_name().startswith('Body')), None)
        foot = next((c for c in meshes if c.get_name().startswith('Footing')), None)
        sm = body.static_mesh if body else None
        fm = foot.static_mesh if foot else None
        out.append({'actor': a, 'label': a.get_actor_label(), 'mesh': sm.get_path_name() if sm else '',
                    'name': sm.get_name() if sm else '', 'footing': fm.get_name() if fm else '',
                    'pad': body.get_editor_property('relative_location').z if body else 0.0})
    return out


def to_world(b, local, z_extra=0.0):
    a = b['actor']
    p = a.get_actor_transform().transform_location(unreal.Vector(local[0], local[1], local[2]))
    return unreal.Vector(p.x, p.y, p.z + b['pad'] + z_extra)


def look(pos, target):
    cam = state['camera']
    cam.set_actor_location(pos, False, True)
    cam.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(pos, target), True)
    world = ues.get_game_world()
    pc = unreal.GameplayStatics.get_player_controller(world, 0) if world else None
    if pc and pc.get_view_target() != cam:
        pc.set_view_target_with_blend(cam, 0.0)


def place_man(i, pos, yaw):
    if i < len(state['men']) and state['men'][i]:
        m = state['men'][i]
        m.set_actor_location(pos, False, True)
        m.set_actor_rotation(unreal.Rotator(0, 0, yaw - 90), True)


def park_men():
    for k, m in enumerate(state['men']):
        if m:
            m.set_actor_location(unreal.Vector(0, 0, -100000 - k * 500), False, True)


def existing_shots():
    return set(SHOTS_DIR.rglob('*.png')) if SHOTS_DIR.exists() else set()


def shoot(name):
    path = OUT / (name + '.png')
    if path.exists():
        path.unlink()
    state['pending'] = {'path': path, 'name': name, 'at': time.monotonic(), 'fired': False, 'before': existing_shots()}


def pump_shot(world):
    p = state['pending']
    if not p['fired']:
        if time.monotonic() - p['at'] < 1.2:
            return False
        cmd(world, 'Shot')
        p['fired'], p['at'] = True, time.monotonic()
        return False
    fresh = sorted(existing_shots() - p['before'], key=lambda f: f.stat().st_mtime)
    if fresh and time.monotonic() - p['at'] > 0.4:
        try:
            fresh[-1].replace(p['path'])
        except OSError:
            return False
    if not p['path'].is_file() and time.monotonic() - p['at'] > 25 and not p.get('retried'):
        # Premiere prise apres le chargement des assets : la frame peut sauter le Shot. Une seconde chance.
        log('SHOT %s retry' % p['name'])
        p['retried'], p['fired'], p['at'], p['before'] = True, False, time.monotonic(), existing_shots()
        return False
    if p['path'].is_file() or time.monotonic() - p['at'] > 25:
        state['shots'].append({'file': p['path'].name, 'written': p['path'].is_file()})
        log('SHOT %s written=%s' % (p['name'], p['path'].is_file()))
        state['pending'] = None
        return True
    return False


def pick(bs, part):
    c = [b for b in bs if part in b['name'] and b['name'] in ARCH]
    return c[0] if c else None


def hamlet_center(bs):
    xs = [b['actor'].get_actor_location() for b in bs]
    return unreal.Vector(sum(p.x for p in xs) / len(xs), sum(p.y for p in xs) / len(xs), sum(p.z for p in xs) / len(xs))


def build_plan(bs):
    """Cameras en coordonnees locales des archetypes (docs/unreal/architecture/architecture-kit-001.json)."""
    plan = []
    center = hamlet_center(bs)
    ref = pick(bs, 'House_Medium') or pick(bs, 'House_Farm') or pick(bs, 'House_Poor')
    if not ref:
        fail('aucune maison dans le hameau')
        return plan
    spec = ARCH[ref['name']]
    door = next(d for d in spec['doors'] if d['name'] != 'gate')
    entry = spec['entry']
    hearth = spec['hearths'][0] if spec['hearths'] else None
    lo, hi = spec['bounds']

    def devant():
        place_man(0, to_world(ref, (entry[0], entry[1], 0)), ref['actor'].get_actor_rotation().yaw + 90 + 180)
        look(to_world(ref, (entry[0] + 260, entry[1] + 900, 170)), to_world(ref, (entry[0] - 150, entry[1] - 400, 260)))
    def porte():
        place_man(1, to_world(ref, (door['at'][0], door['at'][1] + 10, door['at'][2])), ref['actor'].get_actor_rotation().yaw + 90)
        look(to_world(ref, (door['at'][0] + 120, door['at'][1] + 420, door['at'][2] + 165)), to_world(ref, (door['at'][0], door['at'][1], door['at'][2] + 110)))
    def interieur():
        # Exposition fixe du projet (EV 14) : un interieur de jour sort noir. L'oeil, lui, s'adapte en
        # entrant -- +3 EV pour CETTE prise seulement, remis a 0 a la suivante (dit dans la legende).
        cmd(ues.get_game_world(), 'r.ExposureOffset 3')
        if hearth:
            place_man(2, to_world(ref, (hearth[0] + 140, hearth[1] + 170, hearth[2] - 40)), ref['actor'].get_actor_rotation().yaw + 200)
            look(to_world(ref, (hearth[0] + 380, hearth[1] + 250, hearth[2] + 125)), to_world(ref, (hearth[0], hearth[1], hearth[2] + 40)))
    def isolee():
        cmd(ues.get_game_world(), 'r.ExposureOffset 0')
        span = max(hi[0] - lo[0], hi[1] - lo[1])
        look(to_world(ref, (span * .55, hi[1] + span * .85, 170)), to_world(ref, ((lo[0] + hi[0]) / 2, (lo[1] + hi[1]) / 2, hi[2] * .4)))
    def groupe():
        others = sorted([b for b in bs if b is not ref], key=lambda b: (b['actor'].get_actor_location() - ref['actor'].get_actor_location()).length())
        o = others[0] if others else ref
        a, b = ref['actor'].get_actor_location(), o['actor'].get_actor_location()
        mid = unreal.Vector((a.x + b.x) / 2, (a.y + b.y) / 2, max(a.z, b.z) + ref['pad'] + 170)
        d = unreal.Vector(b.x - a.x, b.y - a.y, 0)
        n = unreal.Vector(-d.y, d.x, 0)
        L = max(1.0, math.hypot(n.x, n.y))
        pos = unreal.Vector(mid.x + n.x / L * 1600, mid.y + n.y / L * 1600, mid.z)
        look(pos, unreal.Vector(mid.x, mid.y, mid.z + 120))
    def village():
        c = center
        look(unreal.Vector(c.x + 5200, c.y + 3800, c.z + 2200), unreal.Vector(c.x, c.y, c.z + 200))
    def village_haut():
        c = center
        look(unreal.Vector(c.x - 2500, c.y - 6500, c.z + 6500), unreal.Vector(c.x, c.y, c.z))

    plan += [('01-pnj-devant', devant), ('02-pnj-porte', porte)]
    if hearth:
        plan.append(('03-pnj-interieur', interieur))
    plan += [('04-maison-isolee', isolee), ('05-groupe', groupe), ('06-village', village)]
    for tag, part in (('07-pauvre', 'House_Poor'), ('08-ferme', 'House_Farm'), ('09-grenier', 'Storehouse'), ('10-puits', 'Well')):
        b = pick(bs, part)
        if not b:
            continue
        s = ARCH[b['name']]
        blo, bhi = s['bounds']
        sp = max(bhi[0] - blo[0], bhi[1] - blo[1])

        def view(b=b, blo=blo, bhi=bhi, sp=sp):
            park_men()
            e = ARCH[b['name']]['entry']
            place_man(0, to_world(b, (e[0], e[1], 0)), b['actor'].get_actor_rotation().yaw + 270)
            look(to_world(b, (sp * .5, bhi[1] + sp * .8, 170)), to_world(b, ((blo[0] + bhi[0]) / 2, (blo[1] + bhi[1]) / 2, bhi[2] * .35)))
        plan.append((tag, view))
    plan.append(('11-village-haut', village_haut))
    return plan


def tick(dt):
    if time.monotonic() - t0 > 900:
        fail('wall timeout step=%d phase=%d' % (state['step'], state['phase']))
        finish('wall timeout')
        return
    if state['phase'] == 0 and time.monotonic() - t0 > 3:
        state['phase'] = 1
        les.editor_request_begin_play()
        return
    world = ues.get_game_world()
    if state['phase'] == 1:
        if not les.is_in_play_in_editor() or not world or DBG.get_simulation_time(world) < 0:
            return
        for c in ('anastasis.Village.Debug 0', 'anastasis.Village.Bodies 1', 'r.MotionBlurQuality 0',
                  'anastasis.Sky.Clock 0', 'anastasis.Weather.Rain 0', 'anastasis.Sky.Weather 0', 'anastasis.Sky.Hour ' + HOUR, 'anastasis.Village.Architecture 0'):
            cmd(world, c)
        cams = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.CameraActor)
        state['camera'] = next((c for c in cams if c.get_actor_label() == 'ArchitectureProofCamera'), None)
        sk = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.SkeletalMeshActor)
        state['men'] = [next((m for m in sk if m.get_actor_label() == lab), None) for lab in mannequins]
        if not state['camera']:
            fail('camera de preuve absente du monde PIE')
            finish('camera')
            return
        for m in state['men']:
            if m and IDLE:
                m.skeletal_mesh_component.play_animation(IDLE, True)
        state['wait'] = time.monotonic()
        state['phase'] = 2
        return
    if not world:
        return
    if state['pending'] is not None:
        if pump_shot(world):
            state['step'] += 1
        return
    if state['phase'] == 2:
        # Le village du lancement doit etre pose avant qu'on le remplace (terrain pret).
        if time.monotonic() - state['wait'] < 8:
            return
        cmd(world, 'Anastasis.Village.Hamlet 6 10')
        state['phase'] = 3
        state['wait'] = time.monotonic()
        return
    if state['phase'] == 3:
        if time.monotonic() - state['wait'] < 3:
            return
        cmd(world, 'anastasis.Sim.TimeScale 0')
        bs = buildings(world)
        if not bs:
            fail('hameau absent (avant)')
            finish('hameau')
            return
        state['before'] = bs
        log('AVANT ' + json.dumps([{'label': b['label'], 'mesh': b['name']} for b in bs]))
        center = hamlet_center(bs)
        ref = next((b for b in bs if 'house' in b['label']), bs[0])
        state['before_views'] = [
            ('00-avant-village', (unreal.Vector(center.x + 5200, center.y + 3800, center.z + 2200), unreal.Vector(center.x, center.y, center.z + 200))),
            ('00-avant-pnj-devant', None),
        ]
        state['ref_before'] = ref
        state['phase'] = 4
        state['step'] = 0
        return
    if state['phase'] == 4:
        views = state['before_views']
        if state['step'] >= len(views):
            cmd(world, 'anastasis.Village.Architecture 1')
            cmd(world, 'anastasis.Sim.TimeScale 0.0375')
            cmd(world, 'Anastasis.Village.Hamlet 6 10')
            state['phase'] = 5
            state['wait'] = time.monotonic()
            return
        name, v = views[state['step']]
        if v is None:
            ref = state['ref_before']
            a = ref['actor']
            p = a.get_actor_location()
            yaw = math.radians(a.get_actor_rotation().yaw + 90)  # +Y local = acces
            fwd = (math.cos(yaw), math.sin(yaw))
            stand = unreal.Vector(p.x + fwd[0] * 340, p.y + fwd[1] * 340, p.z)
            place_man(0, stand, a.get_actor_rotation().yaw + 270)
            look(unreal.Vector(p.x + fwd[0] * 1250 + fwd[1] * 300, p.y + fwd[1] * 1250 - fwd[0] * 300, p.z + 170),
                 unreal.Vector(p.x, p.y, p.z + 150))
        else:
            look(*v)
        shoot(name)
        return
    if state['phase'] == 5:
        if time.monotonic() - state['wait'] < 4:
            return
        cmd(world, 'anastasis.Sim.TimeScale 0')
        cmd(world, 'Anastasis.Village.ArchitectureReport')
        bs = [b for b in buildings(world) if b['name'] in ARCH or 'VillageArchitecture' not in b['mesh']]
        rec = {'at': 'apres', 'buildings': [{'label': b['label'], 'mesh': b['name'], 'footing': b['footing'],
                                             'pad_cm': round(b['pad'], 1)} for b in bs]}
        state['checks'].append(rec)
        log('APRES ' + json.dumps(rec))
        if len(bs) < 3:
            fail('hameau : %d batiments' % len(bs))
        for b in bs:
            if not b['mesh'].startswith('/Game/Anastasis/VillageArchitecture/'):
                fail('%s porte encore %s' % (b['label'], b['mesh']))
            if not b['footing'].endswith('_Footing'):
                fail('%s sans assise' % b['label'])
            if abs(b['pad']) >= 480:
                fail('%s : terrasse a %.0f cm, au-dela de l assise' % (b['label'], b['pad']))
        kinds = {b['name'] for b in bs if 'House' in b['name']}
        if len(kinds) < 2:
            fail('une seule typologie de maison : %s' % sorted(kinds))
        # Les mannequins prennent le corps et la tenue d'un vrai habitant (materiau aux teintes mesurees).
        vis = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.AnastasisVillagerVisual)
        src = None
        for v in vis:
            sc = v.get_component_by_class(unreal.SkeletalMeshComponent)
            if sc and sc.get_skeletal_mesh_asset():
                src = sc
                break
        for m in state['men']:
            if not m:
                continue
            comp = m.skeletal_mesh_component
            if src:
                comp.set_skeletal_mesh_asset(src.get_skeletal_mesh_asset())
                for i in range(src.get_num_materials()):
                    comp.set_material(i, src.get_material(i))
            if IDLE:
                comp.play_animation(IDLE, True)
        log('MANNEQUINS corps=%s idle=%s' % (src.get_skeletal_mesh_asset().get_name() if src else MANNY, IDLE.get_name() if IDLE else None))
        state['plan'] = build_plan(bs)
        state['phase'] = 6
        state['step'] = 0
        return
    if state['phase'] == 6:
        if state['step'] >= len(state['plan']):
            finish('hameau a l echelle humaine, assises posees, %d prises' % len(state['plan']))
            return
        name, setup = state['plan'][state['step']]
        setup()
        shoot(name)


def guarded(dt):
    try:
        tick(dt)
    except Exception as exc:
        import traceback
        unreal.log_error(traceback.format_exc())
        fail(str(exc))
        finish('exception')


handle = unreal.register_slate_post_tick_callback(guarded)
