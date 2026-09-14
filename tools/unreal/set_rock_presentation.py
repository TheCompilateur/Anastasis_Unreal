"""
ROCK_FORGE_001 -- cable la grammaire rocheuse dans le registre de presentation.

PORTEE : l'entree STONE de /Game/Anastasis/Presentation/DA_AnastasisPresentation,
et rien d'autre. Forest appartient a set_tree_grammar.py, Ruin a
set_presentation_meshes.py. Ce script ne lit ni n'ecrit aucune autre entree :
il les recopie telles quelles.

STONE N'EXISTAIT PAS. Contrairement a Forest et Ruin, il n'y a pas d'entree a
muter : AnastasisPresentation::ResolvePresentation renvoyait false pour chaque
tuile Stone et PlaceDressing n'y posait rien -- 1396 tuiles sur 9216 rendues en
simple teinte de sol. Ce script AJOUTE donc l'entree, et la met a jour si elle
existe deja.

Le data asset fait autorite quand il se charge ; le repli code de
AnastasisPresentationRegistry.cpp ne sert que s'il manque. Les deux doivent dire
la meme chose, et les valeurs ci-dessous sont celles de CreateCodeDefaults.
Si tu changes l'une, change l'autre.

Le piege UE-Python paye par set_ruin_variant.py puis set_tree_grammar.py est
repris tel quel : get_editor_property sur un struct rend une COPIE. Chaque
struct mute doit etre reecrit dans son tableau par index, et le tableau dans son
proprietaire, avant de sauver. Idempotent : un second run dans un process neuf
doit annoncer 'deja conforme'.

    UnrealEditor-Cmd.exe <projet>.uproject -run=pythonscript \
        -script=tools/unreal/set_rock_presentation.py -unattended -nosplash
"""
import unreal

REGISTRY_PATH = "/Game/Anastasis/Presentation/DA_AnastasisPresentation"
MESH_DIR = "/Game/Anastasis/Rock/"
ARCHETYPE = "Rock_Grammar_V1"

# Enveloppe d'echelle. Mesuree contre CE monde : une tuile fait 100 uu, soit un
# metre (c'est la lecture que pose CreateCodeDefaults pour les arbres). Les
# meshes font 150 a 300 uu de haut visible a l'echelle 1, donc 0.30-0.70 donne
# des blocs de 0,45 a 2,1 m -- la meme famille de taille que la ruine (0.6-1.1)
# et nettement sous la canopee (3,4 a 6 m). Un rocher n'est pas une colline.
#
# Premiere passe a 0.85-1.85 : rejetee par les captures. La formation entiere se
# lisait comme un seul bloc blanc en vue aerienne et ecrasait les arbres au MID.
MIN_SCALE = 0.30
MAX_SCALE = 0.70
JITTER = 0.34

# La ruine est a 0 degre parce qu'un pan de mur est une chose fabriquee et se
# tient d'aplomb. Un rocher est exactement l'inverse : il s'est pose comme il est
# tombe. L'inclinaison remonte le pied de (1-cos L) * 50 * Scale, soit 0,4 uu a
# 9 degres -- sans commune mesure avec les 27 a 76 uu de jupe enfouie, donc le
# contact au sol n'en souffre pas.
MAX_LEAN_DEGREES = 9.0

# Meme famille de teinte que HighlandRock (0.518, 0.490, 0.463) du sol, a environ
# un tiers de sa valeur. Reprendre l'albedo du sol a l'identique -- ce que faisait
# la premiere passe -- fait saturer sol ET rochers vers le blanc sous 75000 lux /
# EV100 14 : la pierre cesse alors de se lire comme de la pierre. C'est l'ecart
# d'albedo, pas l'ecart de teinte, qui detache la masse du sol dont elle sort.
TINT = (0.150, 0.142, 0.134)

# Les 19 meshes de la grammaire, produits par tools/unreal/create_rock_assets.py.
# Aucun n'est tague (stature, famille) : ces deux axes decrivent un peuplement
# vegetal, pas un affleurement. AnastasisPresentation::SelectVariantIndex traite
# Any/Any comme "aucune opinion" et echoue ouvert, donc les 19 restent eligibles.
VARIANTS = [
    "SM_Rock_Massive_01", "SM_Rock_Massive_02", "SM_Rock_Massive_03",
    "SM_Rock_Low_01", "SM_Rock_Low_02", "SM_Rock_Low_03",
    "SM_Rock_Vertical_01", "SM_Rock_Vertical_02", "SM_Rock_Vertical_03",
    "SM_Rock_Split_01", "SM_Rock_Split_02", "SM_Rock_Split_03",
    "SM_Rock_Boulder_01", "SM_Rock_Boulder_02", "SM_Rock_Boulder_03",
    "SM_Rock_CliffFragment_01", "SM_Rock_CliffFragment_02", "SM_Rock_CliffFragment_03",
    "SM_Rock_Cluster_01",
]


def log(msg):
    unreal.log("[set_rock_presentation] " + str(msg))


def enum_value(enum_name, name):
    owner = getattr(unreal, enum_name, None)
    value = getattr(owner, name, None) if owner is not None else None
    if value is None:
        raise Exception("unreal.%s.%s inconnu de UE-Python "
                        "(module C++ pas recompile ?)" % (enum_name, name))
    return value


def set_prop(obj, names, value):
    """Le binding Python retire le prefixe b des booleens, et la convention a
    varie selon les versions : on essaie plutot que de parier."""
    for name in names:
        try:
            obj.set_editor_property(name, value)
            return
        except Exception:
            continue
    raise Exception("aucune de ces proprietes n existe: " + ",".join(names))


def build_variants():
    out = []
    for mesh_name in VARIANTS:
        path = MESH_DIR + mesh_name
        mesh = unreal.EditorAssetLibrary.load_asset(path)
        if mesh is None:
            raise Exception("mesh introuvable %s -- lancer d'abord "
                            "tools/unreal/create_rock_assets.py" % path)
        variant = unreal.AnastasisPresentationVariant()
        variant.set_editor_property("mesh", mesh)
        # Pas de material_override : la pierre passe par le BaseShapeMaterial
        # partage teinte par Tint, comme Ruin. GEOMETRY_FIRST -- aucune geometrie
        # n'est cachee derriere une texture dans cette passe.
        variant.set_editor_property("stature", enum_value("AnastasisStatureClass", "ANY"))
        variant.set_editor_property("family", enum_value("AnastasisFoliageFamily", "ANY"))
        variant.set_editor_property("scale_bias", 1.0)
        out.append(variant)
    return out


def make_entry():
    entry = unreal.AnastasisPresentationEntry()
    entry.set_editor_property("semantic_type", enum_value("AnastasisSemanticType", "STONE"))
    entry.set_editor_property("archetype_id", ARCHETYPE)
    set_prop(entry, ("enabled", "b_enabled"), True)
    entry.set_editor_property("tint", unreal.LinearColor(TINT[0], TINT[1], TINT[2], 1.0))
    entry.set_editor_property("min_uniform_scale", MIN_SCALE)
    entry.set_editor_property("max_uniform_scale", MAX_SCALE)
    entry.set_editor_property("jitter_radius_fraction", JITTER)
    set_prop(entry, ("random_yaw", "b_random_yaw"), True)
    entry.set_editor_property("max_lean_degrees", MAX_LEAN_DEGREES)
    entry.set_editor_property("variants", build_variants())
    return entry


def describe(entry):
    """Signature lisible d'une entree, pour comparer avant/apres sans bruit."""
    meshes = []
    for variant in entry.get_editor_property("variants"):
        mesh = variant.get_editor_property("mesh")
        meshes.append(mesh.get_name() if mesh else "NONE")
    try:
        lean = round(float(entry.get_editor_property("max_lean_degrees")), 3)
    except Exception:
        lean = "<absent>"
    tint = entry.get_editor_property("tint")
    return ("archetype=%s scale=(%.2f,%.2f) jitter=%.2f lean=%s "
            "tint=(%.3f,%.3f,%.3f) n=%d | %s" % (
                entry.get_editor_property("archetype_id"),
                entry.get_editor_property("min_uniform_scale"),
                entry.get_editor_property("max_uniform_scale"),
                entry.get_editor_property("jitter_radius_fraction"),
                lean, tint.r, tint.g, tint.b,
                len(meshes), ",".join(meshes)))


def main():
    asset = unreal.EditorAssetLibrary.load_asset(REGISTRY_PATH)
    if asset is None:
        raise Exception("registre introuvable " + REGISTRY_PATH)

    stone = enum_value("AnastasisSemanticType", "STONE")
    entries = list(asset.get_editor_property("entries"))
    log("registre charge : %d entrees" % len(entries))

    fresh = make_entry()
    after = describe(fresh)

    index = -1
    for i in range(len(entries)):
        if entries[i].get_editor_property("semantic_type") == stone:
            index = i
            break

    if index >= 0:
        before = describe(entries[index])
        if before == after:
            log("STONE entry[%d] deja conforme -- rien a faire" % index)
            log("  %s" % after)
            return
        entries[index] = fresh
        log("STONE entry[%d] mise a jour" % index)
        log("  avant : %s" % before)
        log("  apres : %s" % after)
    else:
        entries.append(fresh)
        log("STONE entry AJOUTEE en position %d (elle n'existait pas)" % (len(entries) - 1))
        log("  %s" % after)

    # Le tableau doit etre reecrit dans son proprietaire : get_editor_property a
    # rendu des copies, pas des references.
    asset.set_editor_property("entries", entries)
    unreal.EditorAssetLibrary.save_asset(REGISTRY_PATH)
    log("SAVED %s -- %d entrees" % (REGISTRY_PATH, len(entries)))

    # Relecture : les autres entrees doivent etre intactes.
    reread = unreal.EditorAssetLibrary.load_asset(REGISTRY_PATH)
    for entry in reread.get_editor_property("entries"):
        sem = str(entry.get_editor_property("semantic_type")).split('.')[-1]
        variants = entry.get_editor_property("variants")
        log("  VERIFY %-8s archetype=%-16s variants=%d" % (
            sem, entry.get_editor_property("archetype_id"), len(variants)))


main()
