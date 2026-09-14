"""Cree et peuple le Data Asset d'atmosphere ANASTASIS.

PROPRIETE DE L'ASSET. Ce script est la SOURCE D'AUTORITE de
/Game/Anastasis/Presentation/DA_AnastasisAtmosphere, meme convention que
tools/unreal/presentation-registry.py pour DA_AnastasisPresentation : il le cree
s'il manque, puis il ne fait plus que le verifier. Le regenerer explicitement
(ANASTASIS_ATMOSPHERE_REBUILD=1) ecrase toute retouche faite a la main.

Un agent visuel n'a PAS besoin de ce script : il edite l'asset dans l'editeur.

Les valeurs semees ici sont exactement celles du rig de tools/unreal/observe-slice.py
(soleil pitch -38 / yaw -55, 75000 lux, exposition figee EV100 14), plus le seul
element nouveau de la mission ATMOSPHERE_001 : le brouillard de hauteur. Creer
l'asset ne doit donc rien changer a l'image par rapport aux valeurs par defaut du
code -- Anastasis.Atmosphere.RigParity verrouille cette egalite du cote C++.
"""
import os, unreal

ASSET_DIR = '/Game/Anastasis/Presentation'
ASSET_NAME = 'DA_AnastasisAtmosphere'
ASSET_PATH = ASSET_DIR + '/' + ASSET_NAME


def log(msg):
    unreal.log('ATMOSPHERE_PROFILE ' + msg)


def set_prop(obj, names, value):
    """Ecrit la premiere propriete qui existe parmi `names`.

    Le binding Python d'Unreal retire le prefixe `b` des booleens (bEnabled ->
    enabled), mais la convention a varie selon les versions : on essaie les deux
    plutot que de parier.
    """
    for name in names:
        try:
            obj.set_editor_property(name, value)
            return name
        except Exception:
            continue
    raise Exception('aucune de ces proprietes n existe: ' + ','.join(names))


def seed(asset):
    set_prop(asset, ('enabled', 'b_enabled'), True)

    # Soleil : les angles du rig d'observation, tels quels.
    asset.set_editor_property('sun_pitch_degrees', -38.0)
    asset.set_editor_property('sun_yaw_degrees', -55.0)
    asset.set_editor_property('sun_intensity_lux', 75000.0)
    asset.set_editor_property('sun_color', unreal.LinearColor(1.0, 1.0, 1.0, 1.0))
    set_prop(asset, ('sun_is_atmosphere_light', 'b_sun_is_atmosphere_light'), True)
    set_prop(asset, ('sun_casts_shadows', 'b_sun_casts_shadows'), True)

    # Soleil derive de l'heure : implemente et teste, mais desactive tant que rien
    # dans ANASTASIS ne pilote une heure du monde. L'activer deplacerait le soleil
    # loin de l'angle sous lequel toutes les captures existantes ont ete prises.
    set_prop(asset, ('derive_sun_from_time_of_day', 'b_derive_sun_from_time_of_day'), False)
    asset.set_editor_property('time_of_day_hours', 12.0)
    asset.set_editor_property('latitude_degrees', 41.0)
    asset.set_editor_property('sun_declination_degrees', 0.0)

    # Ciel.
    set_prop(asset, ('sky_atmosphere_enabled', 'b_sky_atmosphere_enabled'), True)
    set_prop(asset, ('sky_light_enabled', 'b_sky_light_enabled'), True)
    set_prop(asset, ('sky_light_real_time_capture', 'b_sky_light_real_time_capture'), True)
    asset.set_editor_property('sky_light_intensity', 1.0)

    # Brouillard : l'element nouveau. Retenu, pas un blanchiment.
    set_prop(asset, ('fog_enabled', 'b_fog_enabled'), True)
    asset.set_editor_property('fog_density', 0.012)
    asset.set_editor_property('fog_height_falloff', 0.2)
    asset.set_editor_property('fog_start_distance', 1500.0)
    asset.set_editor_property('fog_max_opacity', 0.85)
    asset.set_editor_property('fog_inscattering_color', unreal.LinearColor(0.42, 0.50, 0.56, 1.0))
    asset.set_editor_property('fog_height_z', 0.0)

    # Exposition figee : deux captures doivent rester comparables.
    set_prop(asset, ('fixed_exposure', 'b_fixed_exposure'), True)
    asset.set_editor_property('exposure_ev100', 14.0)


if os.environ.get('ANASTASIS_ATMOSPHERE_REBUILD', '0') == '1' and unreal.EditorAssetLibrary.does_asset_exist(ASSET_PATH):
    unreal.EditorAssetLibrary.delete_asset(ASSET_PATH)
    log('DELETED ' + ASSET_PATH)

if not unreal.EditorAssetLibrary.does_asset_exist(ASSET_PATH):
    log('CREATE ' + ASSET_PATH)
    factory = unreal.DataAssetFactory()
    factory.set_editor_property('data_asset_class', unreal.AnastasisAtmosphereProfile)
    asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        ASSET_NAME, ASSET_DIR, unreal.AnastasisAtmosphereProfile, factory)
    seed(asset)
    unreal.EditorAssetLibrary.save_asset(ASSET_PATH)
    log('SAVED')

# Verification en lecture seule : l'asset committe fait foi.
asset = unreal.load_asset(ASSET_PATH)
if asset is None:
    unreal.log_error('ATMOSPHERE_PROFILE LOAD_FAILED ' + ASSET_PATH)
else:
    def get(names):
        for name in names:
            try:
                return asset.get_editor_property(name)
            except Exception:
                continue
        return 'MISSING'

    log('VERIFY enabled=%s sun_pitch=%.3f sun_yaw=%.3f lux=%.1f atmo_sun=%s' % (
        get(('enabled', 'b_enabled')),
        asset.get_editor_property('sun_pitch_degrees'),
        asset.get_editor_property('sun_yaw_degrees'),
        asset.get_editor_property('sun_intensity_lux'),
        get(('sun_is_atmosphere_light', 'b_sun_is_atmosphere_light'))))
    log('VERIFY derive_sun=%s time=%.2f lat=%.2f decl=%.2f' % (
        get(('derive_sun_from_time_of_day', 'b_derive_sun_from_time_of_day')),
        asset.get_editor_property('time_of_day_hours'),
        asset.get_editor_property('latitude_degrees'),
        asset.get_editor_property('sun_declination_degrees')))
    log('VERIFY fog=%s density=%.4f falloff=%.3f start=%.1f max_opacity=%.2f' % (
        get(('fog_enabled', 'b_fog_enabled')),
        asset.get_editor_property('fog_density'),
        asset.get_editor_property('fog_height_falloff'),
        asset.get_editor_property('fog_start_distance'),
        asset.get_editor_property('fog_max_opacity')))
    log('VERIFY fixed_exposure=%s ev100=%.2f sky_rtc=%s' % (
        get(('fixed_exposure', 'b_fixed_exposure')),
        asset.get_editor_property('exposure_ev100'),
        get(('sky_light_real_time_capture', 'b_sky_light_real_time_capture'))))

log('COMPLETE')

# Le script se termine lui-meme, comme presentation-registry.py : passer ";Quit"
# dans -ExecCmds collerait le token a l'argument du "py" et Python evaluerait le
# chemin.
if os.environ.get('ANASTASIS_ATMOSPHERE_KEEP_EDITOR', '0') != '1':
    unreal.SystemLibrary.quit_editor()
