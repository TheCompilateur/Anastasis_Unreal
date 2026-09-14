"""
ANASTASIS_UNREAL_TREE_FORM_001 -- cable la grammaire d'arbres dans le registre.

PORTEE : l'entree FOREST de /Game/Anastasis/Presentation/DA_AnastasisPresentation,
et rien d'autre. Ruin appartient a set_presentation_meshes.py, qui reste
l'outil generique de cablage d'un mesh par archetype. Cette grammaire-ci a
besoin d'ecrire une LISTE de variantes portant chacune sa stature, son biais
d'echelle et son materiau -- ce qu'une cible (type, mesh) ne sait pas exprimer.

Le data asset fait autorite quand il se charge ; le repli code de
AnastasisPresentationRegistry.cpp ne sert que s'il manque. Les deux doivent
donc dire la meme chose, et les valeurs ci-dessous sont celles de
CreateCodeDefaults. Si tu changes l'une, change l'autre.

Le piege UE-Python paye par set_ruin_variant.py est repris tel quel :
get_editor_property sur un struct rend une COPIE. Chaque struct mute doit etre
reecrit dans son tableau par index, et le tableau dans son proprietaire, avant
de sauver. Idempotent : un second run dans un process neuf doit annoncer
'deja conforme'.

    UnrealEditor-Cmd.exe <projet>.uproject -run=pythonscript \
        -script=tools/unreal/set_tree_grammar.py -unattended -nosplash
"""
import unreal

REGISTRY_PATH = "/Game/Anastasis/Presentation/DA_AnastasisPresentation"
VEGETATION_MATERIAL = "/Game/Anastasis/Materials/M_AnastasisVegetation"
MESH_DIR = "/Game/Anastasis/Vegetation/"

# Enveloppe d'echelle de l'entree. Multipliee par les facteurs de strate de
# FAnastasisForestDressingSettings (jeune 0.25-0.45, secondaire 0.50-0.78,
# canopee 0.95-1.20) puis par le biais de la variante. Avant cette passe
# l'enveloppe valait 1.6-2.4 et l'instance mediane mesurait 104 uu -- un metre,
# dans un monde ou la tuile fait justement un metre.
MIN_SCALE = 3.6
MAX_SCALE = 5.0
JITTER = 0.30
MAX_LEAN_DEGREES = 5.0
TINT = (0.102, 0.243, 0.114)
ARCHETYPE = "Tree_Generic"

# (mesh, stature, biais d'echelle)
VARIANTS = [
    ("SM_Tree_Conifer_Understory_01", "UNDERSTORY", 1.00),
    ("SM_Tree_Conifer_Subcanopy_01", "SUBCANOPY", 1.00),
    ("SM_Tree_Broadleaf_Subcanopy_01", "SUBCANOPY", 0.92),
    ("SM_Tree_Conifer_Canopy_01", "CANOPY", 1.00),
    ("SM_Tree_Broadleaf_Canopy_01", "CANOPY", 0.85),
    ("SM_Tree_Conifer_Emergent_01", "EMERGENT", 1.35),
]


def log(msg):
    unreal.log("[set_tree_grammar] " + str(msg))


def stature(name):
    value = getattr(unreal.AnastasisStatureClass, name, None)
    if value is None:
        raise Exception("AnastasisStatureClass.%s inconnu de UE-Python "
                        "(module C++ pas recompile ?)" % name)
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
    material = unreal.EditorAssetLibrary.load_asset(VEGETATION_MATERIAL)
    if material is None:
        raise Exception("materiau introuvable %s -- lancer d'abord "
                        "tools/unreal/create_tree_asset.py" % VEGETATION_MATERIAL)
    out = []
    for mesh_name, stature_name, bias in VARIANTS:
        path = MESH_DIR + mesh_name
        mesh = unreal.EditorAssetLibrary.load_asset(path)
        if mesh is None:
            raise Exception("mesh introuvable %s -- lancer d'abord "
                            "tools/unreal/create_tree_asset.py" % path)
        variant = unreal.AnastasisPresentationVariant()
        variant.set_editor_property("mesh", mesh)
        variant.set_editor_property("material_override", material)
        variant.set_editor_property("stature", stature(stature_name))
        variant.set_editor_property("scale_bias", bias)
        out.append(variant)
    return out


def describe(entry):
    """Signature lisible d'une entree, pour comparer avant/apres sans bruit."""
    rows = []
    for variant in entry.get_editor_property("variants"):
        mesh = variant.get_editor_property("mesh")
        try:
            st = str(variant.get_editor_property("stature"))
            bias = round(float(variant.get_editor_property("scale_bias")), 4)
        except Exception:
            st, bias = "<absent>", None
        rows.append("%s/%s/%s" % (mesh.get_name() if mesh else "NONE", st, bias))
    try:
        lean = round(float(entry.get_editor_property("max_lean_degrees")), 3)
    except Exception:
        lean = "<absent>"
    return "scale=(%.2f,%.2f) jitter=%.2f lean=%s | %s" % (
        entry.get_editor_property("min_uniform_scale"),
        entry.get_editor_property("max_uniform_scale"),
        entry.get_editor_property("jitter_radius_fraction"),
        lean, ", ".join(rows))


def apply(entries):
    forest = getattr(unreal.AnastasisSemanticType, "FOREST")
    for i in range(len(entries)):
        entry = entries[i]
        if entry.get_editor_property("semantic_type") != forest:
            continue

        before = describe(entry)
        entry.set_editor_property("archetype_id", ARCHETYPE)
        set_prop(entry, ("enabled", "b_enabled"), True)
        entry.set_editor_property("tint", unreal.LinearColor(TINT[0], TINT[1], TINT[2], 1.0))
        entry.set_editor_property("min_uniform_scale", MIN_SCALE)
        entry.set_editor_property("max_uniform_scale", MAX_SCALE)
        entry.set_editor_property("jitter_radius_fraction", JITTER)
        set_prop(entry, ("random_yaw", "b_random_yaw"), True)
        entry.set_editor_property("max_lean_degrees", MAX_LEAN_DEGREES)
        entry.set_editor_property("variants", build_variants())
        entries[i] = entry

        after = describe(entry)
        if before == after:
            log("FOREST entry[%d] deja conforme -- rien a faire" % i)
            log("  %s" % after)
            return "already"
        log("FOREST entry[%d]" % i)
        log("  avant : %s" % before)
        log("  apres : %s" % after)
        return "changed"
    raise Exception("aucune entree FOREST dans le registre")


def verify():
    """Relecture disque quand la version d'UE le permet, memoire sinon -- meme
    honnetete que set_presentation_meshes.py : la preuve forte est un SECOND run
    dans un process neuf, ou le script doit dire 'deja conforme'."""
    strong = False
    sub = getattr(unreal, "EditorAssetSubsystem", None)
    if sub is not None:
        fn = getattr(unreal.get_editor_subsystem(sub), "unload_asset", None)
        if fn is not None:
            try:
                fn(REGISTRY_PATH)
                strong = True
            except Exception as exc:
                log("unload refuse: %s" % exc)
    log("VERIFY force=%s" % ("DISQUE" if strong else "MEMOIRE (relancer pour une preuve disque)"))

    asset = unreal.EditorAssetLibrary.load_asset(REGISTRY_PATH)
    forest = getattr(unreal.AnastasisSemanticType, "FOREST")
    for entry in asset.get_editor_property("entries"):
        if entry.get_editor_property("semantic_type") != forest:
            continue
        variants = entry.get_editor_property("variants")
        log("VERIFY FOREST %s" % describe(entry))
        statures = {str(v.get_editor_property("stature")) for v in variants}
        ok = len(variants) == len(VARIANTS) and len(statures) == 4
        log("VERIFY variants=%d attendu=%d statures=%d attendu=4 -> %s"
            % (len(variants), len(VARIANTS), len(statures), "OK" if ok else "MAUVAIS"))
        return ok
    log("VERIFY::FAIL entree FOREST disparue")
    return False


def main():
    asset = unreal.EditorAssetLibrary.load_asset(REGISTRY_PATH)
    if asset is None:
        log("FAIL registre introuvable %s" % REGISTRY_PATH)
        log("RESULT::FAIL")
        return False

    entries = asset.get_editor_property("entries")
    outcome = apply(entries)
    if outcome == "changed":
        asset.set_editor_property("entries", entries)
        asset.modify()
        log("save_asset -> %s" % unreal.EditorAssetLibrary.save_asset(
            REGISTRY_PATH, only_if_is_dirty=False))

    ok = verify()
    log("RESULT::%s" % ("PASS" if ok else "FAIL"))
    return ok


main()
