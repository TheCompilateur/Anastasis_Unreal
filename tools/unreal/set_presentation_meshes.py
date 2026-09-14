"""
Cable les vrais assets Anastasis dans DA_AnastasisPresentation.

Generalise set_ruin_variant.py (ASSET_AGENT_002) a tous les archetypes au lieu
du seul Ruin. Motif : apres e7c53d8, l'entree Ruin pointait bien sur
SM_Ruin_Generic_01, mais l'entree Forest pointait toujours sur
/Engine/BasicShapes/Cone.Cone -- SM_Tree_Generic_01, le premier vrai asset du
projet (ASSET_AGENT_001), n'etait donc pas rendu dans le monde. Le data asset
fait autorite quand il se charge ; le fallback code de
AnastasisPresentationRegistry.cpp ne sert que s'il manque.

Le piege UE-Python que set_ruin_variant.py a paye une fois est conserve ici :
get_editor_property sur un struct rend une COPIE. Muter la copie puis appeler
save_asset ne salit rien et save_asset ne fait rien. Chaque struct mute doit
etre reecrit dans son tableau par index, et le tableau reecrit dans son
proprietaire, avant de sauver.

La verification, elle, ne peut PAS toujours relire le disque : UE 5.8 n'expose
pas EditorAssetLibrary.unload_assets, et l'appeler sans garde leve
AttributeError apres le save -- la mutation passe, la verification ne sort
jamais. C'est ce qui arrive a set_ruin_variant.py. Ici le dechargement est
tente puis signale : VERIFY force=DISQUE s'il a marche, force=MEMOIRE sinon.
Dans le second cas la preuve solide est un SECOND run dans un process neuf, ou
le script idempotent doit annoncer 'deja cable' pour chaque cible.

Idempotent : une entree deja correcte est signalee et laissee telle quelle.

    UnrealEditor-Cmd.exe <projet>.uproject -run=pythonscript \
        -script=tools/unreal/set_presentation_meshes.py -unattended -nosplash
"""
import unreal

REGISTRY_PATH = "/Game/Anastasis/Presentation/DA_AnastasisPresentation"

# (type semantique, chemin du mesh reel). Ajouter une ligne quand un archetype
# recoit son premier vrai asset.
TARGETS = [
    ("FOREST", "/Game/Anastasis/Vegetation/SM_Tree_Generic_01"),
    ("RUIN", "/Game/Anastasis/Architecture/SM_Ruin_Generic_01"),
]


def log(msg):
    unreal.log("[set_presentation_meshes] " + str(msg))


def semantic(name):
    """Le UENUM expose par UE-Python, ou None si le nom a derive cote C++."""
    return getattr(unreal.AnastasisSemanticType, name, None)


def wire_one(entries, sem_name, mesh_path):
    """Retourne 'changed', 'already', ou 'fail'. Mute `entries` sur place."""
    sem = semantic(sem_name)
    if sem is None:
        log("FAIL %s: AnastasisSemanticType.%s inconnu de UE-Python" % (sem_name, sem_name))
        return "fail"

    mesh = unreal.EditorAssetLibrary.load_asset(mesh_path)
    if mesh is None:
        log("FAIL %s: mesh introuvable %s" % (sem_name, mesh_path))
        return "fail"

    for i in range(len(entries)):
        entry = entries[i]
        if entry.get_editor_property("semantic_type") != sem:
            continue

        variants = entry.get_editor_property("variants")
        if len(variants) == 0:
            log("FAIL %s: entry[%d] n'a aucun variant" % (sem_name, i))
            return "fail"

        old = variants[0].get_editor_property("mesh")
        old_path = old.get_path_name() if old is not None else "<None>"
        if old is not None and old.get_path_name().startswith(mesh_path):
            log("%s entry[%d] deja cable sur %s -- rien a faire" % (sem_name, i, old_path))
            return "already"

        log("%s entry[%d], %d variant(s), ancien mesh=%s" % (sem_name, i, len(variants), old_path))

        # Reecriture par index a chaque niveau : struct -> tableau -> proprietaire.
        variant0 = variants[0]
        variant0.set_editor_property("mesh", mesh)
        variants[0] = variant0
        entry.set_editor_property("variants", variants)
        entries[i] = entry

        log("%s variant[0].mesh -> %s (en attente de sauvegarde)" % (sem_name, mesh_path))
        return "changed"

    log("FAIL %s: aucune entree de ce type semantique dans le registre" % sem_name)
    return "fail"


def try_unload():
    """Decharge le registre si l'API de cette version d'UE le permet.

    EditorAssetLibrary.unload_assets n'existe pas en UE 5.8 : s'en servir sans
    garde levait AttributeError APRES le save, donc la mutation passait mais la
    verification ne sortait jamais (c'est le cas de set_ruin_variant.py). On
    essaie les points d'entree connus et on dit franchement si aucun n'a marche.
    """
    sub = getattr(unreal, "EditorAssetSubsystem", None)
    if sub is not None:
        try:
            fn = getattr(unreal.get_editor_subsystem(sub), "unload_asset", None)
            if fn is not None:
                fn(REGISTRY_PATH)
                return True
        except Exception as exc:
            log("unload via EditorAssetSubsystem refuse: %s" % exc)
    fn = getattr(unreal.EditorAssetLibrary, "unload_assets", None)
    if fn is not None:
        try:
            fn([REGISTRY_PATH])
            return True
        except Exception as exc:
            log("unload via EditorAssetLibrary refuse: %s" % exc)
    return False


def verify(results):
    """Controle chaque cible visee.

    Fort si le dechargement a reussi (relecture disque). Sinon FAIBLE : on lit
    l'etat memoire que ce process vient de muter. La preuve solide est alors un
    SECOND run dans un process neuf : le script etant idempotent, il doit y
    annoncer 'deja cable' pour chaque cible.
    """
    strong = try_unload()
    log("VERIFY force=%s" % ("DISQUE" if strong else "MEMOIRE (relancer pour une preuve disque)"))
    reloaded = unreal.EditorAssetLibrary.load_asset(REGISTRY_PATH)
    if reloaded is None:
        log("VERIFY::FAIL rechargement impossible")
        return False

    on_disk = {}
    for entry in reloaded.get_editor_property("entries"):
        variants = entry.get_editor_property("variants")
        if len(variants) == 0:
            continue
        mesh = variants[0].get_editor_property("mesh")
        on_disk[entry.get_editor_property("semantic_type")] = (
            mesh.get_path_name() if mesh is not None else "<None>")

    ok = True
    for sem_name, mesh_path in TARGETS:
        if results.get(sem_name) == "fail":
            ok = False
            continue
        actual = on_disk.get(semantic(sem_name), "<absent>")
        hit = actual.startswith(mesh_path)
        log("VERIFY %s = %s [%s]" % (sem_name, actual, "OK" if hit else "MAUVAIS"))
        ok = ok and hit
    return ok


def main():
    asset = unreal.EditorAssetLibrary.load_asset(REGISTRY_PATH)
    if asset is None:
        log("FAIL: registre introuvable %s" % REGISTRY_PATH)
        log("RESULT::FAIL")
        return False

    entries = asset.get_editor_property("entries")
    results = {}
    for sem_name, mesh_path in TARGETS:
        results[sem_name] = wire_one(entries, sem_name, mesh_path)

    if "changed" in results.values():
        asset.set_editor_property("entries", entries)
        asset.modify()
        log("save_asset -> %s" % unreal.EditorAssetLibrary.save_asset(
            REGISTRY_PATH, only_if_is_dirty=False))
    else:
        log("aucune modification a sauver")

    ok = verify(results)
    log("RESULT::%s" % ("PASS" if ok else "FAIL"))
    return ok


main()
