"""VILLAGER_BODY_3D_001 -- le materiau qui habille le corps 3D des habitants.

SOURCE D'AUTORITE de :
  /Game/Anastasis/Characters/M_AnastasisVillagerBody   tenue peinte sur la pose de liaison

Le corps est le mannequin d'Epic deja present dans le projet (SKM_Manny_Simple / SKM_Quinn_Simple),
anime par BS_Idle_Walk_Run. Le materiau lit la position PRE-SKINNING (pose de liaison) : une bande
reste sur le meme morceau de corps pendant la marche. Il decoupe le corps en hauteur et en ecart a
l'axe : sandales, jambes nues, vetement de l'ourlet au cou, ceinture, manches courtes, avant-bras et
mains nus, visage, cheveux. Les couleurs et l'ourlet viennent du C++ (AnastasisVillagerLooks::BodyLookFor).

Les seuils ne sont pas devines : le script MESURE d'abord les os de Manny et de Quinn (pose de
liaison, fraction de la hauteur du mesh) et les grave comme valeurs par defaut des parametres.

Regenere a chaque run. Lance par tools/unreal/create-villager-body.ps1 (editeur dedie sur
/Engine/Maps/Entry, qui se ferme). Sortie : lignes VILLAGER_BODY::... dans le log.
"""
import os
import unreal

MATERIAL_PATH = '/Game/Anastasis/Characters/M_AnastasisVillagerBody'
MESHES = {
    'male': '/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple',
    'female': '/Game/Characters/Mannequins/Meshes/SKM_Quinn_Simple',
}
LOCOMOTION = '/Game/Characters/Mannequins/Anims/Unarmed/BS_Idle_Walk_Run'
BONES = ['pelvis', 'spine_03', 'neck_01', 'head', 'upperarm_l', 'lowerarm_l', 'hand_l',
         'thigh_l', 'calf_l', 'foot_l', 'ball_l']

FAILURES = []


def log(msg):
    unreal.log('VILLAGER_BODY ' + msg)


def fail(msg):
    FAILURES.append(msg)
    unreal.log_error('VILLAGER_BODY::FAIL ' + msg)


# ------------------------------------------------------------------------------------------ mesure

def measure(kind, path):
    """Os de la pose de liaison, dans l'espace du composant (cm), sur un acteur pose a l'origine."""
    mesh = unreal.EditorAssetLibrary.load_asset(path)
    if mesh is None:
        fail('mesh absent : ' + path)
        return None
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actor = actors.spawn_actor_from_class(unreal.SkeletalMeshActor, unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0))
    comp = actor.get_editor_property('skeletal_mesh_component')
    comp.set_skeletal_mesh_asset(mesh)
    bones = {}
    for bone in BONES:
        p = comp.get_socket_location(bone)
        bones[bone] = (p.x, p.y, p.z)
    bounds = mesh.get_bounds()
    height = bounds.box_extent.z * 2.0
    top = bounds.origin.z + bounds.box_extent.z
    slots = len(mesh.get_editor_property('materials'))
    actors.destroy_actor(actor)
    log('MESH %s %s height=%.1f top=%.1f origin=(%.1f,%.1f,%.1f) extent=(%.1f,%.1f,%.1f) slots=%d' % (
        kind, path, height, top, bounds.origin.x, bounds.origin.y, bounds.origin.z,
        bounds.box_extent.x, bounds.box_extent.y, bounds.box_extent.z, slots))
    for bone in BONES:
        x, y, z = bones[bone]
        log('BONE %s %-10s (%.1f, %.1f, %.1f)  h=%.3f' % (kind, bone, x, y, z, z / height))
    return {'height': height, 'bones': bones}


def describe_locomotion():
    bs = unreal.EditorAssetLibrary.load_asset(LOCOMOTION)
    if bs is None:
        fail('marche absente : ' + LOCOMOTION)
        return
    log('LOCOMOTION %s class=%s' % (LOCOMOTION, bs.get_class().get_name()))
    def field(struct, names):
        for name in names:
            try:
                return struct.get_editor_property(name)
            except Exception:
                continue
        return '?'
    try:
        for i, axis in enumerate(bs.get_editor_property('blend_parameters')):
            log('LOCOMOTION axis[%d] name=%s min=%s max=%s' % (
                i, field(axis, ['display_name']), field(axis, ['min']), field(axis, ['max'])))
    except Exception as e:
        log('LOCOMOTION axes illisibles (%s)' % e)
    try:
        for sample in bs.get_editor_property('sample_data'):
            anim = field(sample, ['animation'])
            log('LOCOMOTION sample %s at %s' % (anim.get_name() if hasattr(anim, 'get_name') else anim,
                                                  field(sample, ['sample_value'])))
    except Exception as e:
        log('LOCOMOTION echantillons illisibles (%s)' % e)


def thresholds(m):
    """Fractions de la hauteur, depuis les os de Manny."""
    h = m['height']
    b = m['bones']
    hand = b['hand_l']
    lateral_is_x = abs(hand[0]) >= abs(hand[1])
    lat = (lambda p: abs(p[0])) if lateral_is_x else (lambda p: abs(p[1]))
    fwd = (lambda p: p[1]) if lateral_is_x else (lambda p: p[0])
    t = {
        'LateralIsX': 1.0 if lateral_is_x else 0.0,
        # Le cou : entre la base du cou et la tete.
        'Neck': (b['neck_01'][2] * 0.6 + b['head'][2] * 0.4) / h,
        # La ceinture, un peu au-dessus du bassin.
        'Waist': (b['pelvis'][2] * 0.55 + b['spine_03'][2] * 0.45) / h,
        # Au-dela de l'epaule : le bras.
        'Shoulder': lat(b['upperarm_l']) / h * 1.05,
        # Manche courte : jusqu'a mi-chemin du coude.
        'Sleeve': (lat(b['upperarm_l']) * 0.45 + lat(b['lowerarm_l']) * 0.55) / h,
        # Sandales : sous la cheville.
        'Ankle': (b['foot_l'][2] * 0.9) / h,
        # Devant / derriere de la tete (le mannequin regarde +Y).
        'HeadFwd': fwd(b['head']) / h,
    }
    for k, v in sorted(t.items()):
        log('THRESHOLD %s=%.4f' % (k, v))
    return t


# ------------------------------------------------------------------------------------------ materiau

HLSL = r'''
float H = max(Height, 1.0);
float h = P.z / H;
float lat = (LateralIsX > 0.5 ? abs(P.x) : abs(P.y)) / H;
float fwd = (LateralIsX > 0.5 ? P.y : P.x) / H;
bool arm = lat > Shoulder && h > Hem + 0.02;
float3 c = Skin.rgb;
bool torso = !arm && h > Hem && h < Neck;
bool sleeve = arm && lat < Sleeve && h > Waist;
if (torso || sleeve) { c = Garment.rgb; }
if (!arm && abs(h - Waist) < 0.012) { c = Trim.rgb; }
if (!arm && h > Hem && h < Hem + 0.012) { c = Trim.rgb; }
if (h < Ankle) { c = Trim.rgb * 0.75; }
bool hair = h > 0.955 || (h > HairLow && fwd < HeadFwd - 0.004 && !arm);
if (hair) { c = Hair.rgb; }
return c;
'''

INPUTS = ['P', 'Skin', 'Hair', 'Garment', 'Trim', 'Hem', 'HairLow', 'Height',
          'LateralIsX', 'Neck', 'Waist', 'Shoulder', 'Sleeve', 'Ankle', 'HeadFwd']


def build_material(t, height):
    mel = unreal.MaterialEditingLibrary
    mat = unreal.EditorAssetLibrary.load_asset(MATERIAL_PATH)
    if mat is None:
        mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            'M_AnastasisVillagerBody', MATERIAL_PATH.rsplit('/', 1)[0], unreal.Material, unreal.MaterialFactoryNew())
    mel.delete_all_material_expressions(mat)

    custom = mel.create_material_expression(mat, unreal.MaterialExpressionCustom, -300, 0)
    custom.set_editor_property('code', HLSL)
    custom.set_editor_property('description', 'VillagerDress')
    custom.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    ins = []
    for name in INPUTS:
        ci = unreal.CustomInput()
        ci.set_editor_property('input_name', name)
        ins.append(ci)
    custom.set_editor_property('inputs', ins)

    # Le noeud « PreSkinnedPosition » : position de la pose de liaison, espace local du mesh.
    # Il n'existe qu'au vertex shader (« not available in the Pixel shader », premier run) : un
    # VertexInterpolator le porte jusqu'au pixel.
    pos = mel.create_material_expression(mat, unreal.MaterialExpressionPreSkinnedPosition, -1000, -300)
    carry = mel.create_material_expression(mat, unreal.MaterialExpressionVertexInterpolator, -800, -300)
    mel.connect_material_expressions(pos, '', carry, '')
    mel.connect_material_expressions(carry, '', custom, 'P')

    colours = {
        'Skin': (0.33, 0.17, 0.09), 'Hair': (0.01, 0.006, 0.005),
        'Garment': (0.58, 0.50, 0.36), 'Trim': (0.055, 0.025, 0.013),
    }
    y = -200
    for name, rgb in colours.items():
        node = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -800, y)
        node.set_editor_property('parameter_name', name)
        node.set_editor_property('default_value', unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0))
        mel.connect_material_expressions(node, '', custom, name)
        y += 90
    scalars = dict(t)
    scalars.update({'Hem': 0.30, 'HairLow': 0.85, 'Height': height})
    for name in INPUTS[5:]:
        node = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -800, y)
        node.set_editor_property('parameter_name', name)
        node.set_editor_property('default_value', float(scalars[name]))
        mel.connect_material_expressions(node, '', custom, name)
        y += 60

    if not mel.connect_material_property(custom, '', unreal.MaterialProperty.MP_BASE_COLOR):
        fail('materiau : BaseColor non branche')
    for value, prop in ((0.82, unreal.MaterialProperty.MP_ROUGHNESS), (0.3, unreal.MaterialProperty.MP_SPECULAR)):
        k = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -300, 300 if prop == unreal.MaterialProperty.MP_ROUGHNESS else 360)
        k.set_editor_property('r', value)
        mel.connect_material_property(k, '', prop)

    mat.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    mat.set_editor_property('blend_mode', unreal.BlendMode.BLEND_OPAQUE)
    # Sans ce drapeau le materiau est refuse sur un mesh squelettique (damier gris par defaut).
    mat.set_editor_property('used_with_skeletal_mesh', True)
    log('MATERIAL_COMPILE_FINAL ' + MATERIAL_PATH)
    mel.recompile_material(mat)
    unreal.EditorAssetLibrary.save_asset(MATERIAL_PATH, only_if_is_dirty=False)
    return mat


def verify():
    mat = unreal.EditorAssetLibrary.load_asset(MATERIAL_PATH)
    if mat is None:
        fail('materiau non relu : ' + MATERIAL_PATH)
        return
    if not mat.get_editor_property('used_with_skeletal_mesh'):
        fail('materiau non marque used_with_skeletal_mesh')
    names = [str(n) for n in unreal.MaterialEditingLibrary.get_vector_parameter_names(mat)]
    for needed in ('Skin', 'Hair', 'Garment', 'Trim'):
        if needed not in names:
            fail('parametre absent : ' + needed)
    log('VERIFY vector_params=%s' % ','.join(sorted(names)))


def main():
    try:
        male = measure('male', MESHES['male'])
        female = measure('female', MESHES['female'])
        describe_locomotion()
        if male is None or female is None:
            fail('mesure impossible')
        else:
            t = thresholds(male)
            build_material(t, male['height'])
            verify()
        if FAILURES:
            unreal.log_error('VILLAGER_BODY::FAIL %d echec(s)' % len(FAILURES))
        else:
            unreal.log('VILLAGER_BODY::PASS ' + MATERIAL_PATH)
    except Exception as e:
        unreal.log_error('VILLAGER_BODY::FAIL exception %s' % e)
    finally:
        if os.environ.get('ANASTASIS_VILLAGER_BODY_QUIT') == '1':
            unreal.SystemLibrary.quit_editor()


main()
