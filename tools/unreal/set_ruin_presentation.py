"""
RUIN_GRAMMAR_V1 -- cable la grammaire des vestiges dans le registre de presentation.

PORTEE : l'entree RUIN de /Game/Anastasis/Presentation/DA_AnastasisPresentation, et
rien d'autre. Forest appartient a set_tree_grammar.py, Stone a set_rock_presentation.py.
Les autres entrees sont recopiees telles quelles.

L'ORDRE DES VARIANTES N'EST PAS CONTRACTUEL. AAnastasisWorldEmbodiment::PlaceDressing
associe une piece a ses variantes en comparant le NOM du mesh au nom de la piece
(AnastasisRuinDressing::PieceName), pas son rang dans le tableau. Un tri, un ajout ou un
retrait dans l'editeur ne peut donc pas faire dessiner un foyer la ou un mur va. En
revanche, renommer un mesh casse l'association -- et ca, ca se voit.

Le data asset fait autorite quand il se charge ; le repli code de
AnastasisPresentationRegistry.cpp ne sert que s'il manque. Les deux doivent dire la meme
chose. Idempotent : un second run dans un process neuf doit annoncer 'deja conforme'.

    UnrealEditor-Cmd.exe <projet>.uproject -run=pythonscript \
        -script=tools/unreal/set_ruin_presentation.py -unattended -nosplash
"""
import unreal

REGISTRY_PATH = "/Game/Anastasis/Presentation/DA_AnastasisPresentation"
MESH_DIR = "/Game/Anastasis/Architecture/"
ARCHETYPE = "Ruin_Grammar_V1"

# Les meshes sont deja a l'echelle du bati (empreintes de 2 a 7,7 m), donc l'enveloppe
# reste proche de 1 : elle ne sert plus a fabriquer une taille, seulement a eviter que
# deux sites voisins soient identiques au centimetre pres.
MIN_SCALE = 0.92
MAX_SCALE = 1.12

# Jitter FAIBLE, contrairement a la roche (0.34). Un bloc erratique s'est pose n'importe
# ou sur sa tuile ; un mur appartient a un plan. Deplacer les pieces de 34 uu les unes
# par rapport aux autres suffit a rendre l'alignement du site illisible.
JITTER = 0.10

# Zero degre, et c'est deliberement le contraire du rocher (9 degres). Une assise a ete
# posee de niveau. PlaceDressing ecrase de toute facon la rotation par le yaw du site,
# donc la valeur est surtout une declaration d'intention pour qui relira l'asset.
MAX_LEAN_DEGREES = 0.0

# Meme famille que HighlandRock, un rien plus chaude et plus claire que la teinte des
# rochers (0.150, 0.142, 0.134) : la pierre de taille est plus seche et plus blonde que
# le bloc brut du haut-pays. L'ecart reste petit -- ce sont les memes carrieres.
TINT = (0.168, 0.155, 0.140)

# Les 18 vestiges de tools/unreal/create_ruin_grammar.py.
VARIANTS = [
    "SM_Ruin_Soubassement_01", "SM_Ruin_Soubassement_02", "SM_Ruin_Soubassement_03",
    "SM_Ruin_Angle_01", "SM_Ruin_Angle_02", "SM_Ruin_Angle_03",
    "SM_Ruin_Mur_01", "SM_Ruin_Mur_02", "SM_Ruin_Mur_03",
    "SM_Ruin_Foyer_01", "SM_Ruin_Foyer_02", "SM_Ruin_Foyer_03",
    "SM_Ruin_Enclos_01", "SM_Ruin_Enclos_02", "SM_Ruin_Enclos_03",
    "SM_Ruin_Reemploi_01", "SM_Ruin_Reemploi_02", "SM_Ruin_Reemploi_03",
]


def log(msg):
    unreal.log("[set_ruin_presentation] " + str(msg))


def enum_value(enum_name, name):
    owner = getattr(unreal, enum_name, None)
    value = getattr(owner, name, None) if owner is not None else None
    if value is None:
        raise Exception("unreal.%s.%s inconnu de UE-Python "
                        "(module C++ pas recompile ?)" % (enum_name, name))
    return value


def set_prop(obj, names, value):
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
                            "tools/unreal/create_ruin_grammar.py" % path)
        variant = unreal.AnastasisPresentationVariant()
        variant.set_editor_property("mesh", mesh)
        variant.set_editor_property("stature", enum_value("AnastasisStatureClass", "ANY"))
        variant.set_editor_property("family", enum_value("AnastasisFoliageFamily", "ANY"))
        variant.set_editor_property("scale_bias", 1.0)
        out.append(variant)
    return out


def make_entry():
    entry = unreal.AnastasisPresentationEntry()
    entry.set_editor_property("semantic_type", enum_value("AnastasisSemanticType", "RUIN"))
    entry.set_editor_property("archetype_id", ARCHETYPE)
    set_prop(entry, ("enabled", "b_enabled"), True)
    entry.set_editor_property("tint", unreal.LinearColor(TINT[0], TINT[1], TINT[2], 1.0))
    entry.set_editor_property("min_uniform_scale", MIN_SCALE)
    entry.set_editor_property("max_uniform_scale", MAX_SCALE)
    entry.set_editor_property("jitter_radius_fraction", JITTER)
    set_prop(entry, ("random_yaw", "b_random_yaw"), False)
    entry.set_editor_property("max_lean_degrees", MAX_LEAN_DEGREES)
    entry.set_editor_property("variants", build_variants())
    return entry


def describe(entry):
    meshes = []
    for variant in entry.get_editor_property("variants"):
        mesh = variant.get_editor_property("mesh")
        meshes.append(mesh.get_name() if mesh else "NONE")
    try:
        lean = round(float(entry.get_editor_property("max_lean_degrees")), 3)
    except Exception:
        lean = "<absent>"
    try:
        yaw = entry.get_editor_property("random_yaw")
    except Exception:
        yaw = entry.get_editor_property("b_random_yaw")
    tint = entry.get_editor_property("tint")
    return ("archetype=%s scale=(%.2f,%.2f) jitter=%.2f lean=%s random_yaw=%s "
            "tint=(%.3f,%.3f,%.3f) n=%d | %s" % (
                entry.get_editor_property("archetype_id"),
                entry.get_editor_property("min_uniform_scale"),
                entry.get_editor_property("max_uniform_scale"),
                entry.get_editor_property("jitter_radius_fraction"),
                lean, yaw, tint.r, tint.g, tint.b, len(meshes), ",".join(meshes)))


def main():
    asset = unreal.EditorAssetLibrary.load_asset(REGISTRY_PATH)
    if asset is None:
        raise Exception("registre introuvable " + REGISTRY_PATH)

    ruin = enum_value("AnastasisSemanticType", "RUIN")
    entries = list(asset.get_editor_property("entries"))
    log("registre charge : %d entrees" % len(entries))

    fresh = make_entry()
    after = describe(fresh)

    index = -1
    for i in range(len(entries)):
        if entries[i].get_editor_property("semantic_type") == ruin:
            index = i
            break

    if index >= 0:
        before = describe(entries[index])
        if before == after:
            log("RUIN entry[%d] deja conforme -- rien a faire" % index)
            return
        entries[index] = fresh
        log("RUIN entry[%d] mise a jour" % index)
        log("  avant : %s" % before)
        log("  apres : %s" % after)
    else:
        entries.append(fresh)
        log("RUIN entry AJOUTEE en position %d" % (len(entries) - 1))
        log("  %s" % after)

    asset.set_editor_property("entries", entries)
    unreal.EditorAssetLibrary.save_asset(REGISTRY_PATH)
    log("SAVED %s -- %d entrees" % (REGISTRY_PATH, len(entries)))

    reread = unreal.EditorAssetLibrary.load_asset(REGISTRY_PATH)
    for entry in reread.get_editor_property("entries"):
        sem = str(entry.get_editor_property("semantic_type")).split('.')[-1]
        log("  VERIFY %-10s archetype=%-18s variants=%d" % (
            sem, entry.get_editor_property("archetype_id"),
            len(entry.get_editor_property("variants"))))


main()
