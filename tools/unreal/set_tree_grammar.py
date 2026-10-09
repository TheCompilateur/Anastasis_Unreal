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
# Slot 1 de chaque mesh d'arbre. Le bois n'est pas une feuille : Default Lit,
# opaque, mat -- distinct du feuillage deux faces du slot 0.
BARK_MATERIAL = "/Game/Anastasis/Materials/M_AnastasisBark"
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

# (mesh, stature, famille, biais d'echelle). Quatre statures x deux familles :
# la grille que la planche de reference autorise, remplie. La stature vient de
# l'ecologie, la famille vient du site (Shade et Wetness) -- deux axes
# orthogonaux, donc un peuplement peut changer d'espece sans changer de
# structure d'age, et l'inverse.
VARIANTS = [
    ("SM_Tree_Conifer_Understory_01", "UNDERSTORY", "CONIFER", 1.00),
    ("SM_Tree_Broadleaf_Understory_01", "UNDERSTORY", "BROADLEAF", 0.90),
    ("SM_Tree_Conifer_Subcanopy_01", "SUBCANOPY", "CONIFER", 1.00),
    ("SM_Tree_Broadleaf_Subcanopy_01", "SUBCANOPY", "BROADLEAF", 0.92),
    ("SM_Tree_Conifer_Canopy_01", "CANOPY", "CONIFER", 1.00),
    ("SM_Tree_Broadleaf_Canopy_01", "CANOPY", "BROADLEAF", 0.85),
    ("SM_Tree_Conifer_Emergent_01", "EMERGENT", "CONIFER", 1.35),
    ("SM_Tree_Broadleaf_Emergent_01", "EMERGENT", "BROADLEAF", 1.12),
]

# FOREST_TERRAIN_P1 -- les essences. (prefixe de mesh, espece, famille, hauteur adulte en m).
# Trois formes par espece (_01.._03), produites par create_tree_asset.py. Stature ANY : l'age
# se lit a la hauteur (HeightRangeM x maturite), pas a un autre mesh. Ajoutees APRES la grille
# pontique, qui reste le repli non etiquete. Memes valeurs que SpeciesDefaults dans
# AnastasisPresentationRegistry.cpp.
SHAPES_PER_SPECIES = 3
SPECIES = [
    ("SM_Tree_AleppoPine", "ALEPPO_PINE", "CONIFER", (11.0, 18.0)),
    ("SM_Tree_Cypress", "CYPRESS", "CONIFER", (12.0, 20.0)),
    ("SM_Tree_HolmOak", "HOLM_OAK", "BROADLEAF", (8.0, 14.0)),
    ("SM_Tree_Olive", "OLIVE", "BROADLEAF", (4.5, 8.0)),
    ("SM_Tree_PlaneTree", "PLANE_TREE", "BROADLEAF", (17.0, 24.0)),
    ("SM_Tree_BlackPine", "BLACK_PINE", "CONIFER", (15.0, 23.0)),
    ("SM_Tree_GreekFir", "GREEK_FIR", "CONIFER", (14.0, 22.0)),
]


# GPT_FLORA_001 -- les arbres tires de la planche GPT (create-gpt-flora.py), une forme chacun, dans
# /Game/Anastasis/Vegetation/Gpt/, avec leur propre materiau de feuillage. (maillage, espece, famille,
# hauteur adulte en m). Pin sombre et cypres sont une forme de plus des essences BlackPine et Cypress.
# Memes valeurs que GptSpeciesDefaults dans AnastasisPresentationRegistry.cpp.
GPT_MESH_DIR = "/Game/Anastasis/Vegetation/Gpt/"
GPT_MATERIAL = "/Game/Anastasis/Materials/M_AnastasisGptFoliage"
GPT_SPECIES = [
    ("SM_Gpt_Chene", "DECIDUOUS_OAK", "BROADLEAF", (14.0, 22.0)),
    ("SM_Gpt_Bouleau", "BIRCH", "BROADLEAF", (12.0, 18.0)),
    ("SM_Gpt_PinSylvestre", "SCOTS_PINE", "CONIFER", (20.0, 30.0)),
    ("SM_Gpt_SaulePleureur", "WILLOW", "BROADLEAF", (10.0, 16.0)),
    ("SM_Gpt_MarronnierFleuri", "HORSE_CHESTNUT", "BROADLEAF", (14.0, 20.0)),
    ("SM_Gpt_PinSombre", "BLACK_PINE", "CONIFER", (15.0, 23.0)),
    ("SM_Gpt_Cypres", "CYPRESS", "CONIFER", (12.0, 20.0)),
]


def log(msg):
    unreal.log("[set_tree_grammar] " + str(msg))


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
    material = unreal.EditorAssetLibrary.load_asset(VEGETATION_MATERIAL)
    if material is None:
        raise Exception("materiau introuvable %s -- lancer d'abord "
                        "tools/unreal/create_tree_asset.py" % VEGETATION_MATERIAL)
    bark = unreal.EditorAssetLibrary.load_asset(BARK_MATERIAL)
    if bark is None:
        raise Exception("materiau introuvable %s -- lancer d'abord "
                        "tools/unreal/create_tree_asset.py" % BARK_MATERIAL)
    out = []
    for mesh_name, stature_name, family_name, bias in VARIANTS:
        path = MESH_DIR + mesh_name
        mesh = unreal.EditorAssetLibrary.load_asset(path)
        if mesh is None:
            raise Exception("mesh introuvable %s -- lancer d'abord "
                            "tools/unreal/create_tree_asset.py" % path)
        variant = unreal.AnastasisPresentationVariant()
        variant.set_editor_property("mesh", mesh)
        variant.set_editor_property("material_override", material)
        variant.set_editor_property("additional_material_overrides", [bark])
        variant.set_editor_property("stature", enum_value("AnastasisStatureClass", stature_name))
        variant.set_editor_property("family", enum_value("AnastasisFoliageFamily", family_name))
        variant.set_editor_property("scale_bias", bias)
        variant.set_editor_property("species", enum_value("AnastasisTreeSpecies", "ANY"))
        variant.set_editor_property("height_range_m", unreal.Vector2D(0.0, 0.0))
        out.append(variant)
    for stem, species_name, family_name, (low, high) in SPECIES:
        for shape in range(1, SHAPES_PER_SPECIES + 1):
            path = "%s%s_%02d" % (MESH_DIR, stem, shape)
            mesh = unreal.EditorAssetLibrary.load_asset(path)
            if mesh is None:
                raise Exception("mesh introuvable %s -- lancer d'abord "
                                "tools/unreal/create_tree_asset.py" % path)
            variant = unreal.AnastasisPresentationVariant()
            variant.set_editor_property("mesh", mesh)
            variant.set_editor_property("material_override", material)
            variant.set_editor_property("additional_material_overrides", [bark])
            variant.set_editor_property("stature", enum_value("AnastasisStatureClass", "ANY"))
            variant.set_editor_property("family", enum_value("AnastasisFoliageFamily", family_name))
            variant.set_editor_property("scale_bias", 1.0)
            variant.set_editor_property("species", enum_value("AnastasisTreeSpecies", species_name))
            variant.set_editor_property("height_range_m", unreal.Vector2D(low, high))
            out.append(variant)
    gpt_material = unreal.EditorAssetLibrary.load_asset(GPT_MATERIAL)
    if gpt_material is None:
        raise Exception("materiau introuvable %s -- lancer d'abord "
                        "tools/unreal/create-gpt-flora.ps1" % GPT_MATERIAL)
    for mesh_name, species_name, family_name, (low, high) in GPT_SPECIES:
        mesh = unreal.EditorAssetLibrary.load_asset(GPT_MESH_DIR + mesh_name)
        if mesh is None:
            raise Exception("mesh introuvable %s%s -- lancer d'abord "
                            "tools/unreal/create-gpt-flora.ps1" % (GPT_MESH_DIR, mesh_name))
        variant = unreal.AnastasisPresentationVariant()
        variant.set_editor_property("mesh", mesh)
        variant.set_editor_property("material_override", gpt_material)
        variant.set_editor_property("additional_material_overrides", [bark])
        variant.set_editor_property("stature", enum_value("AnastasisStatureClass", "ANY"))
        variant.set_editor_property("family", enum_value("AnastasisFoliageFamily", family_name))
        variant.set_editor_property("scale_bias", 1.0)
        variant.set_editor_property("species", enum_value("AnastasisTreeSpecies", species_name))
        variant.set_editor_property("height_range_m", unreal.Vector2D(low, high))
        out.append(variant)
    return out


def describe(entry):
    """Signature lisible d'une entree, pour comparer avant/apres sans bruit."""
    rows = []
    for variant in entry.get_editor_property("variants"):
        mesh = variant.get_editor_property("mesh")
        try:
            st = str(variant.get_editor_property("stature")).split('.')[-1]
            fam = str(variant.get_editor_property("family")).split('.')[-1]
            bias = round(float(variant.get_editor_property("scale_bias")), 4)
        except Exception:
            st, fam, bias = "<absent>", "<absent>", None
        try:
            sp = str(variant.get_editor_property("species")).split('.')[-1]
            hr = variant.get_editor_property("height_range_m")
            fam = "%s/%s/%.1f-%.1fm" % (fam, sp, hr.x, hr.y)
        except Exception:
            fam = fam + "/<pas d'espece>"
        try:
            extra = len(variant.get_editor_property("additional_material_overrides"))
        except Exception:
            extra = "<absent>"
        rows.append("%s/%s/%s/%s/+%s" % (mesh.get_name() if mesh else "NONE", st, fam, bias, extra))
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
        expected = len(VARIANTS) + SHAPES_PER_SPECIES * len(SPECIES) + len(GPT_SPECIES)
        statures = {str(v.get_editor_property("stature")) for v in variants}
        families = {str(v.get_editor_property("family")) for v in variants}
        species = {str(v.get_editor_property("species")) for v in variants}
        slots = {len(v.get_editor_property("additional_material_overrides")) for v in variants}
        # 4 statures pontiques + ANY des essences ; 7 essences + 5 nouvelles (GPT) + ANY de la grille pontique.
        gpt_new = len({s for _m, s, _f, _h in GPT_SPECIES} - {s for _p, s, _f, _h in SPECIES})
        ok = (len(variants) == expected and len(statures) == 5 and len(families) == 2
              and len(species) == len(SPECIES) + gpt_new + 1 and slots == {1})
        log("VERIFY variants=%d attendu=%d statures=%d attendu=5 familles=%d attendu=2 "
            "especes=%d attendu=%d slots_supplementaires=%s attendu={1} -> %s"
            % (len(variants), expected, len(statures), len(families), len(species),
               len(SPECIES) + gpt_new + 1, sorted(slots), "OK" if ok else "MAUVAIS"))
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
