"""Micro-implantation humaine dans le bassin deja reserve par la geographie.

Isolee : les acteurs portent le tag HumanOccupation001 et sont enregistres dans
Lvl_HumanOccupation. Lvl_AnastasisSlice n'est pas sauvegardee. Aucun systeme
PNJ, simulateur, meteo, ciel ou foret n'est modifie ; l'heure du ciel n'est
epinglee que le temps des captures, dans cet editeur, apres la sauvegarde.
"""
import os, json, math, random, time, traceback
import unreal

OUT = os.environ['HO01_OUT']
TAG = 'HumanOccupation001'
LEVEL_DST = '/Game/Anastasis/Maps/Lvl_HumanOccupation'
MAT_PATH = '/Game/Anastasis/HumanOccupation/M_HO_Tread'
os.makedirs(OUT, exist_ok=True)

def dump(name, value):
    with open(os.path.join(OUT, name), 'w', encoding='utf-8') as handle:
        json.dump(value, handle, indent=2)

def fail(message):
    unreal.log_error(message)
    dump('error.json', {'error': message})
    unreal.SystemLibrary.quit_editor()
    raise SystemExit(message)

root = os.path.normcase(os.path.normpath(unreal.Paths.project_dir()))
if root == os.path.normcase(r'C:\dev\ANASTASIS_UNREAL'):
    fail('Prototype belongs in an isolated worktree, never the integration root.')

ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
world = ues.get_editor_world()
if not world.get_path_name().startswith('/Game/Anastasis/Maps/Lvl_AnastasisSlice.'):
    fail('Open Lvl_AnastasisSlice first, got ' + world.get_path_name())

def vec(v):
    return (v.x, v.y, v.z)

def embodiment():
    return next(a for a in eas.get_all_level_actors() if a.get_class().get_name() == 'AnastasisWorldEmbodiment')

def read_fields(actor):
    """Sol et nappe du terrain forge, pas l'anneau d'horizon : grille 381, pas 500 cm."""
    target = 381 * 381
    for comp in actor.get_components_by_class(unreal.ProceduralMeshComponent):
        ground_raw = unreal.ProceduralMeshLibrary.get_section_from_procedural_mesh(comp, 0)
        water_raw = unreal.ProceduralMeshLibrary.get_section_from_procedural_mesh(comp, 1)
        if not ground_raw or not water_raw:
            continue
        if len(ground_raw[0]) != target or len(water_raw[0]) != target:
            continue
        tr = comp.get_world_transform()
        ground = [vec(unreal.MathLibrary.transform_location(tr, p)) for p in ground_raw[0]]
        wet = [vec(unreal.MathLibrary.transform_location(tr, p)) for p in water_raw[0]]
        step = ground[1][0] - ground[0][0]
        return ground, wet, 381, step
    return None

emb = embodiment()
fields = read_fields(emb)
if fields is None or fields[2] != 381 or abs(fields[3] - 500) > 0.01:
    unreal.log('HO01 embody canonical before placement')
    if not emb.call_method('EmbodyCanonical', args=(12345,)):
        fail('EmbodyCanonical failed')
    fields = read_fields(emb)
if fields is None or fields[2] != 381 or abs(fields[3] - 500) > 0.01:
    fail('Requires the full Human Geography surface (381 grid, 500 cm).')
ground, water, N, STEP = fields

def sample(x, y, field):
    u = (x - ground[0][0]) / STEP
    v = (y - ground[0][1]) / STEP
    ix = math.floor(u)
    iy = math.floor(v)
    fx = u - ix
    fy = v - iy
    if not (0 <= ix < N - 1 and 0 <= iy < N - 1):
        return None
    a = iy * N + ix
    b = a + 1
    c = a + N
    d = c + 1
    if fx + fy <= 1:
        return field[a][2] * (1 - fx - fy) + field[b][2] * fx + field[c][2] * fy
    return field[d][2] * (fx + fy - 1) + field[b][2] * (1 - fy) + field[c][2] * (1 - fx)

def height(x, y):
    return sample(x, y, ground)

def freeboard(x, y):
    z = sample(x, y, ground)
    w = sample(x, y, water)
    if z is None or w is None:
        return None, None
    return z, z - w

basin = emb.get_terrain_forge_basin()
if abs(basin.x) < 1 and abs(basin.y) < 1:
    basin = unreal.Vector(53.0 * 2000.0, 53.0 * 2000.0, 0)
    unreal.log('HO01 basin fallback tile 53')
bx, by = basin.x, basin.y

places = []
for line in emb.get_place_report():
    parts = str(line).split('|')
    if len(parts) >= 5:
        places.append((parts[0], float(parts[1]), float(parts[2]), float(parts[4])))

def blocked_by_place(x, y):
    for _name, px, py, radius in places:
        if math.hypot(x - px, y - py) < radius + 1800:
            return True
    return False

trees = []
for actor in eas.get_all_level_actors():
    for comp in actor.get_components_by_class(unreal.HierarchicalInstancedStaticMeshComponent):
        mesh = comp.get_editor_property('static_mesh')
        if mesh is None or 'Tree' not in mesh.get_name():
            continue
        count = comp.get_instance_count()
        for index in range(count):
            got = comp.get_instance_transform(index, True)
            xf = got[1] if isinstance(got, tuple) else got
            tx, ty = xf.translation.x, xf.translation.y
            if math.hypot(tx - bx, ty - by) <= 60000:
                trees.append((tx, ty))
unreal.log('HO01 trees_near_basin %d places %d' % (len(trees), len(places)))

def relief(x, y, radius):
    zs = []
    for k in range(8):
        a = k * math.tau / 8
        z = height(x + math.cos(a) * radius, y + math.sin(a) * radius)
        if z is None:
            return None
        zs.append(z)
    return max(zs) - min(zs)

def walk_to_water(x, y):
    """Cherche une berge : on descend, on ne grimpe pas, on s'arrete au sec."""
    best = None
    for k in range(24):
        a = k * math.tau / 24
        prev = height(x, y)
        landed = None
        for dist in range(600, 11001, 600):
            px = x + math.cos(a) * dist
            py = y + math.sin(a) * dist
            z, fb = freeboard(px, py)
            if z is None or prev is None:
                break
            if z > prev + 90:
                break
            prev = z
            if fb < 45:
                back = dist - 600
                if back < 1600:
                    break
                lx = x + math.cos(a) * back
                ly = y + math.sin(a) * back
                lz, lfb = freeboard(lx, ly)
                if lz is None or lfb < 70:
                    break
                landed = (lx, ly, lz, back, a)
                break
        if landed and (best is None or landed[3] < best[3]):
            best = landed
    return best

def search(fb_min, fb_max, relief_max, radius, honor_places):
    found = []
    for ix in range(-10, 11):
        for iy in range(-10, 11):
            x = bx + ix * 1000
            y = by + iy * 1000
            z, fb = freeboard(x, y)
            if z is None or not (fb_min <= fb <= fb_max):
                continue
            span = relief(x, y, radius)
            if span is None or span > relief_max:
                continue
            if honor_places and blocked_by_place(x, y):
                continue
            bank = walk_to_water(x, y)
            if bank is None:
                continue
            wx = math.cos(bank[4])
            wy = math.sin(bank[4])
            near = 0
            edge = 0
            for tx, ty in trees:
                d = math.hypot(tx - x, ty - y)
                if d < 900:
                    near += 1
                elif d < 9000 and (tx - x) * wx + (ty - y) * wy < 0:
                    edge += 1
            if near > 2:
                continue
            score = -span * 4.0 - abs(bank[3] - 3400) * 0.03 + min(edge, 6) * 8
            found.append((score, x, y, z, fb, span, bank, edge))
    return found

candidates = []
search_pass = None
for search_pass, args in (
    ('strict', (160, 700, 55, 900, True)),
    ('loose', (90, 1400, 110, 700, True)),
    ('loose_ignoring_places', (90, 1400, 110, 700, False)),
):
    candidates = search(*args)
    if candidates:
        break
if not candidates:
    fail('No terrace above the water, inside the reserved basin, flat enough for houses.')
unreal.log('HO01 site_pass %s candidates %d' % (search_pass, len(candidates)))
candidates.sort(key=lambda item: item[0], reverse=True)
_score, sx, sy, sz, sfb, sspan, bank, sedge = candidates[0]
lx, ly, lz, water_dist, water_angle = bank
water_x, water_y = math.cos(water_angle), math.sin(water_angle)
right_x, right_y = -water_y, water_x

def forest_dir():
    best_d = None
    acc = [0.0, 0.0]
    for tx, ty in trees:
        dx, dy = tx - sx, ty - sy
        if dx * water_x + dy * water_y >= 0:
            continue
        d = math.hypot(dx, dy)
        if 8000 <= d <= 50000:
            acc[0] += dx / d
            acc[1] += dy / d
            if best_d is None or d < best_d[0]:
                best_d = (d, tx, ty)
    length = math.hypot(acc[0], acc[1])
    if length < 0.01:
        return -water_x, -water_y, best_d
    return acc[0] / length, acc[1] / length, best_d

fx, fy, nearest_tree = forest_dir()

def shift(dist_water, dist_right):
    return (sx + water_x * dist_water + right_x * dist_right,
            sy + water_y * dist_water + right_y * dist_right)

def settle(x, y):
    """Decale le long de la terrasse si l'empreinte penche ou trempe."""
    for dist in (0, 250, -250, 500, -500, 800, -800):
        px = x + right_x * dist
        py = y + right_y * dist
        z, fb = freeboard(px, py)
        span = relief(px, py, 220)
        if z is None or fb is None or span is None:
            continue
        if fb >= 120 and span <= 40:
            return px, py, z
    z, fb = freeboard(x, y)
    return x, y, z

# Trois maisons dos a la pente, grenier vers l'eau mais hors du sentier,
# puits au centre, abri de travail vers le bois. Le cote +droite reste vide :
# c'est la croissance.
spots = {
    'house_a': shift(-780, -620),
    'house_b': shift(-920, 540),
    'house_c': shift(-360, 1080),
    'granary': shift(220, -860),
    'well': shift(-80, 160),
    'shelter': (sx + fx * 1350 + right_x * -420, sy + fy * 1350 + right_y * -420),
}
placed_xy = {}
for name, (x, y) in spots.items():
    px, py, pz = settle(x, y)
    placed_xy[name] = (px, py, pz)

wood_xy = (placed_xy['shelter'][0] + fx * 480, placed_xy['shelter'][1] + fy * 480)
wood_z = height(*wood_xy)

MESHES = {
    'house': '/Game/Anastasis/VillageBuildings/SM_House_Refuge_01',
    'granary': '/Game/Anastasis/VillageBuildings/SM_Granary_Raised_01',
    'well': '/Game/Anastasis/VillageBuildings/SM_Well_Stone_01',
    'shelter': '/Game/Anastasis/CampShelter009/SM_Shelter_PatchedCanvas_01',
    'chest': '/Game/Anastasis/RefugeeProps008/SM_Chest_Travel_01',
    'amphora': '/Game/Anastasis/RefugeeProps008/SM_Amphora_Transport_01',
    'cauldron': '/Game/Anastasis/RefugeeProps008/SM_Tripod_Cauldron_01',
    'trap': '/Game/Anastasis/RefugeeProps008/SM_FishTrap_Wicker_01',
    'stump': '/Game/Anastasis/Ecotone/SM_Ecotone_Stump_01',
    'pile': '/Game/Anastasis/Ecotone/SM_Ecotone_BranchPile_01',
    'log': '/Game/Anastasis/Ecotone/SM_Ecotone_FallenLog_01',
}
loaded = {}
for key, path in MESHES.items():
    asset = unreal.load_asset(path)
    if not asset:
        fail('Missing existing asset ' + path)
    loaded[key] = asset

def mesh_size(mesh):
    b = mesh.get_bounding_box()
    return (b.max.x - b.min.x, b.max.y - b.min.y, b.max.z - b.min.z, b.min.z)

def scale_for(key):
    w, d, h, _minz = mesh_size(loaded[key])
    if key in ('house', 'granary', 'well', 'shelter'):
        return 1.0
    if key == 'log':
        longest = max(w, d, h)
        return 340.0 / longest if longest > 1 else 1.0
    target = {'stump': 58, 'pile': 72, 'chest': 70, 'amphora': 85, 'cauldron': 95, 'trap': 55}.get(key, 80)
    if 25 <= h <= 160 and key in ('chest', 'amphora', 'cauldron', 'trap'):
        return 1.0
    return target / h if h > 1 else 1.0

for actor in list(eas.get_all_level_actors()):
    if TAG in [str(t) for t in actor.tags]:
        eas.destroy_actor(actor)

def yaw_toward(x, y, tx, ty):
    return math.degrees(math.atan2(ty - y, tx - x))

records = []

def drop(key, x, y, yaw, label, scale=None):
    z = height(x, y)
    if z is None:
        fail('Unsampled ground for ' + label)
    mesh = loaded[key]
    s = scale_for(key) if scale is None else scale
    _w, _d, _h, min_z = mesh_size(mesh)
    actor = eas.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(x, y, z - min_z * s), unreal.Rotator(pitch=0, yaw=yaw, roll=0))
    actor.set_actor_label(label)
    actor.set_folder_path('HUMAN_OCCUPATION')
    actor.tags = [TAG]
    comp = actor.static_mesh_component
    comp.set_mobility(unreal.ComponentMobility.MOVABLE)
    comp.set_static_mesh(mesh)
    actor.set_actor_scale3d(unreal.Vector(s, s, s))
    comp.set_editor_property('can_ever_affect_navigation', False)
    rot = actor.get_actor_rotation()
    if abs(rot.pitch) > 1.5 or abs(rot.roll) > 1.5:
        fail('Tilted actor %s pitch=%.1f roll=%.1f' % (label, rot.pitch, rot.roll))
    records.append({'label': label, 'mesh': mesh.get_name(), 'xy': [round(x, 1), round(y, 1)], 'z': round(z, 1), 'yaw': round(yaw, 1), 'scale': round(s, 3), 'size_cm': [round(v, 1) for v in mesh_size(mesh)[:3]]})
    return actor

yard = placed_xy['well']
for name, key in (('house_a', 'house'), ('house_b', 'house'), ('house_c', 'house'), ('granary', 'granary'), ('well', 'well'), ('shelter', 'shelter')):
    x, y, _z = placed_xy[name]
    drop(key, x, y, yaw_toward(x, y, yard[0], yard[1]), 'HO01_' + name)

# Devant chaque batiment, le depart du sentier.
def door(name, reach):
    x, y, _z = placed_xy[name]
    facing = yaw_toward(x, y, yard[0], yard[1])
    rad = math.radians(facing)
    return (x + math.cos(rad) * reach, y + math.sin(rad) * reach)

gx, gy, _gz = placed_xy['granary']
drop('chest', gx + right_x * 220, gy + right_y * 220, yaw_toward(gx, gy, yard[0], yard[1]) + 20, 'HO01_chest')
drop('amphora', gx + right_x * 380 + water_x * 80, gy + right_y * 380 + water_y * 80, 15, 'HO01_amphora')
wx, wy, _wz = placed_xy['well']
drop('cauldron', wx + right_x * -200, wy + right_y * -200, 40, 'HO01_cauldron')
drop('trap', lx + right_x * 220, ly + right_y * 220, math.degrees(water_angle) + 90, 'HO01_fish_trap')
drop('pile', wood_xy[0], wood_xy[1], math.degrees(math.atan2(fy, fx)), 'HO01_wood_pile')
drop('pile', wood_xy[0] + right_x * 180, wood_xy[1] + right_y * 180, 25, 'HO01_wood_pile_b', scale=scale_for('pile') * 0.85)
drop('log', wood_xy[0] - right_x * 240, wood_xy[1] - right_y * 240, math.degrees(math.atan2(right_y, right_x)), 'HO01_felled_log')
rng = random.Random(1001)
for i, (ox, oy) in enumerate(((160, 80), (-120, 210), (40, -180))):
    drop('stump', wood_xy[0] + fx * ox + right_x * oy, wood_xy[1] + fy * ox + right_y * oy, rng.uniform(0, 360), 'HO01_stump_%d' % i)

def ensure_decal():
    # Recree a chaque passe : la premiere projection etait une carte blanche verticale.
    path = MAT_PATH
    if unreal.EditorAssetLibrary.does_asset_exist(path) and not unreal.EditorAssetLibrary.delete_asset(path):
        path = '/Game/Anastasis/HumanOccupation/M_HO_Tread_B'
        if unreal.EditorAssetLibrary.does_asset_exist(path):
            unreal.EditorAssetLibrary.delete_asset(path)
    name = path.rsplit('/', 1)[-1]
    folder = path.rsplit('/', 1)[0]
    mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        name, folder, unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property('material_domain', unreal.MaterialDomain.MD_DEFERRED_DECAL)
    mat.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
    mel = unreal.MaterialEditingLibrary
    color = mel.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector, -350, 0)
    color.set_editor_property('constant', unreal.LinearColor(0.28, 0.20, 0.11, 1))
    opac = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -350, 180)
    opac.set_editor_property('r', 0.72)
    rough = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -350, 300)
    rough.set_editor_property('r', 0.92)
    if not mel.connect_material_property(color, '', unreal.MaterialProperty.MP_BASE_COLOR):
        fail('Decal base color link failed')
    if not mel.connect_material_property(opac, '', unreal.MaterialProperty.MP_OPACITY):
        fail('Decal opacity link failed')
    if not mel.connect_material_property(rough, '', unreal.MaterialProperty.MP_ROUGHNESS):
        fail('Decal roughness link failed')
    errors = list(mel.recompile_material(mat))
    if errors:
        fail('Decal compile failed ' + ';'.join(str(e) for e in errors))
    if not unreal.EditorAssetLibrary.save_asset(path, False):
        fail('Decal material save failed')
    return mat

decal_mat = ensure_decal()

def organic(a, b, bow):
    dx, dy = b[0] - a[0], b[1] - a[1]
    length = math.hypot(dx, dy) or 1.0
    px, py = -dy / length, dx / length
    cx = (a[0] + b[0]) * 0.5 + px * bow
    cy = (a[1] + b[1]) * 0.5 + py * bow
    steps = max(2, int(length / 280))
    pts = []
    for i in range(steps + 1):
        t = i / steps
        u = 1 - t
        pts.append((u * u * a[0] + 2 * u * t * cx + t * t * b[0], u * u * a[1] + 2 * u * t * cy + t * t * b[1]))
    return pts

path_pts = []
well_xy = (placed_xy['well'][0], placed_xy['well'][1])
landing_xy = (lx, ly)
for name, reach, bow in (('house_a', 220, 140), ('house_b', 220, -160), ('house_c', 220, 120), ('granary', 200, -100), ('shelter', 260, 180)):
    path_pts.extend(organic(door(name, reach), well_xy, bow)[1:])
path_pts.extend(organic(well_xy, landing_xy, 280)[1:])
path_pts.extend(organic(door('shelter', 260), wood_xy, -90)[1:])

decals = 0
previous = None
for x, y in path_pts:
    z, fb = freeboard(x, y)
    if z is None or fb < 55:
        continue
    if previous is None:
        heading = rng.uniform(0, 360)
    else:
        heading = math.degrees(math.atan2(y - previous[1], x - previous[0]))
    previous = (x, y)
    yaw = heading + rng.uniform(-10, 10)
    actor = eas.spawn_actor_from_class(unreal.DecalActor, unreal.Vector(x, y, z + 30), unreal.Rotator(pitch=-90, yaw=yaw, roll=0))
    actor.set_actor_label('HO01_Path_%03d' % decals)
    actor.set_folder_path('HUMAN_OCCUPATION/Paths')
    actor.tags = [TAG]
    for kind_name in ('ArrowComponent', 'BillboardComponent'):
        kind = getattr(unreal, kind_name, None)
        if kind is None:
            continue
        try:
            extra = actor.get_component_by_class(kind)
        except Exception:
            extra = None
        if not extra:
            continue
        try:
            extra.set_visibility(False, True)
        except Exception:
            pass
    comp = actor.get_component_by_class(unreal.DecalComponent)
    comp.set_decal_material(decal_mat)
    # X = profondeur de projection. Y/Z = demi-largeur d'une trace, pas une route.
    comp.set_editor_property('decal_size', unreal.Vector(180, 170, 80))
    decals += 1

def look(a, b):
    return unreal.MathLibrary.find_look_at_rotation(unreal.Vector(*a), unreal.Vector(*b))

def eye(x, y, lift):
    z = height(x, y)
    if z is None:
        z = sz
    return (x, y, z + lift)

# A sol : en retrait des maisons, pour voir la cour, le puits et le sentier vers l'eau.
stand = shift(-1700, 900)
shots = [
    ('ground', eye(*stand, 170), eye(lx, ly, 50)),
    ('distance', eye(sx - water_x * 2800 + right_x * 1800, sy - water_y * 2800 + right_y * 1800, 2400), eye(sx, sy, 150)),
    ('from_river', eye(lx + water_x * 280, ly + water_y * 280, 165), eye(sx, sy, 160)),
    ('to_forest', eye(*shift(1100, 400), 170), eye(sx + fx * 8000, sy + fy * 8000, 280)),
]
if nearest_tree:
    _d, tx, ty = nearest_tree
    dx, dy = sx - tx, sy - ty
    d = math.hypot(dx, dy) or 1
    ex = tx + dx / d * 900 - dy / d * 450
    ey = ty + dy / d * 900 + dx / d * 450
    shots.append(('from_forest', eye(ex, ey, 170), eye(sx, sy, 180)))
else:
    shots.append(('from_forest', eye(sx + fx * 4500, sy + fy * 4500, 170), eye(sx, sy, 160)))

owned = [a for a in eas.get_all_level_actors() if TAG in [str(t) for t in a.tags]]
report = {
    'basin_cm': [round(bx, 1), round(by, 1), round(basin.z, 1)],
    'site_cm': [round(sx, 1), round(sy, 1), round(sz, 1)],
    'freeboard_cm': round(sfb, 1),
    'relief_cm': round(sspan, 1),
    'water_distance_cm': round(water_dist, 1),
    'landing_cm': [round(lx, 1), round(ly, 1), round(lz, 1)],
    'forest_edge_trees': sedge,
    'nearest_tree_cm': None if not nearest_tree else [round(nearest_tree[1], 1), round(nearest_tree[2], 1), round(nearest_tree[0], 1)],
    'growth_right_cm': [round(right_x, 3), round(right_y, 3)],
    'places_avoided': [p[0] for p in places],
    'actors': len(owned),
    'static_mesh_actors': sum(1 for a in owned if a.get_class().get_name() == 'StaticMeshActor'),
    'decals': decals,
    'meshes': sorted(set(r['mesh'] for r in records)),
    'collision': 'mesh default; can_ever_affect_navigation false',
    'slice_saved': False,
    'candidates': len(candidates),
    'search_pass': search_pass,
}
dump('placements.json', records)
dump('placement_report.json', report)

saved_level = False
try:
    if unreal.EditorAssetLibrary.does_asset_exist(LEVEL_DST):
        unreal.EditorAssetLibrary.delete_asset(LEVEL_DST)
    saved_level = bool(unreal.EditorLoadingAndSavingUtils.save_map(world, LEVEL_DST))
except Exception:
    unreal.log_error(traceback.format_exc())
    saved_level = False
report['level_saved'] = saved_level
report['level'] = LEVEL_DST if saved_level else None
dump('placement_report.json', report)
if not saved_level:
    unreal.log_error('HO01 level was not saved; captures still run; slice must stay untouched')

atm = None
try:
    atm_cls = unreal.load_class(None, '/Script/Anastasis_UnrealV2.AnastasisWorldAtmosphere')
    atm = eas.spawn_actor_from_class(atm_cls, unreal.Vector(0, 0, 0), unreal.Rotator(pitch=0, yaw=0, roll=0), transient=True)
except Exception:
    unreal.log_error(traceback.format_exc())

# Heures epinglees apres la sauvegarde, uniquement pour les captures.
queue = [(name, loc, tgt, None) for name, loc, tgt in shots]
queue.append(('ground_hour8',) + shots[0][1:] + ('anastasis.Sky.Hour 8',))
queue.append(('ground_hour17',) + shots[0][1:] + ('anastasis.Sky.Hour 17.5',))

def cmd(text):
    unreal.SystemLibrary.execute_console_command(world, text)

cmd('viewmode lit')
cmd('ShowFlag.Sprites 0')
cmd('ShowFlag.Grid 0')

idx = 0
mark = time.monotonic()
requested = False
attempts = 0
pending_hour = None
handle = None

def finish(ok):
    global handle
    if handle is not None:
        unreal.unregister_slate_post_tick_callback(handle)
    try:
        world.get_package().set_dirty_flag(False)
    except Exception:
        pass
    try:
        report['frame_timings_ms'] = [round(v, 2) for v in vec(emb.get_frame_timings_ms())]
    except Exception:
        report['frame_timings_ms'] = None
    dump('placement_report.json', report)
    unreal.log('HUMAN_OCCUPATION_001_COMPLETE ' + json.dumps(report) if ok else 'HUMAN_OCCUPATION_001_FAILED')
    unreal.SystemLibrary.quit_editor()

def tick(_dt):
    global idx, mark, requested, attempts, pending_hour
    try:
        if idx >= len(queue):
            finish(True)
            return
        name, loc, tgt, hour = queue[idx]
        les.editor_invalidate_viewports()
        if hour != pending_hour:
            if hour and atm is not None:
                cmd(hour)
                atm.call_method('Apply')
                atm.call_method('ApplyMist')
            pending_hour = hour
            requested = False
            mark = time.monotonic()
            return
        if not requested:
            ues.set_level_viewport_camera_info(unreal.Vector(*loc), look(loc, tgt))
            wait = 12 if hour else 7
            if time.monotonic() - mark > wait:
                shot_path = os.path.join(OUT, name + '.png')
                if os.path.isfile(shot_path):
                    os.remove(shot_path)
                cmd('HighResShot 1600x900 filename="%s/%s.png"' % (OUT, name))
                requested = True
                mark = time.monotonic()
        elif time.monotonic() - mark > 4:
            if not os.path.isfile(os.path.join(OUT, name + '.png')):
                attempts += 1
                if attempts >= 5:
                    fail('Screenshot missing ' + name)
                    return
                requested = False
                mark = time.monotonic()
                return
            if name == 'ground':
                try:
                    report['frame_timings_ms'] = [round(v, 2) for v in vec(emb.get_frame_timings_ms())]
                except Exception:
                    pass
            idx += 1
            requested = False
            attempts = 0
            mark = time.monotonic()
    except Exception:
        unreal.log_error(traceback.format_exc())
        finish(False)

handle = unreal.register_slate_post_tick_callback(tick)
unreal.log('HO01 placement ready actors=%d decals=%d site=%s' % (len(owned), decals, report['site_cm']))
