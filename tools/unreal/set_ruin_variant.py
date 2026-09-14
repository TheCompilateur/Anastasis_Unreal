"""
ANASTASIS_ASSET_AGENT_002 -- Phase 5 integration (fixed).

Points the Ruin entry's Variants[0].Mesh in DA_AnastasisPresentation at
SM_Ruin_Generic_01 instead of /Engine/BasicShapes/Cylinder.Cylinder.

First attempt at this script mutated copies of the Entries/Variants structs
without writing them back into their containing arrays before reassigning
the array to the asset -- classic UE-Python struct-value-copy trap. That
version logged success and called save_asset, but save_asset is a no-op
when nothing is actually dirty, and grepping the saved .uasset afterwards
showed it still only referenced Cone/Cylinder, never SM_Ruin_Generic_01.
This version writes each mutated struct back into its array by index
(variants[0] = variant; entry.set_editor_property("variants", variants);
entries[i] = entry) before calling asset.set_editor_property("entries", ...),
and re-loads the asset fresh at the end to verify on disk, not from
in-memory state left over from this same process.
"""
import unreal

REGISTRY_PATH = "/Game/Anastasis/Presentation/DA_AnastasisPresentation"
RUIN_MESH_PATH = "/Game/Anastasis/Architecture/SM_Ruin_Generic_01"


def log(msg):
    unreal.log("[set_ruin_variant] " + str(msg))


def main():
    asset = unreal.EditorAssetLibrary.load_asset(REGISTRY_PATH)
    if asset is None:
        log("FAIL: could not load %s" % REGISTRY_PATH)
        return False

    new_mesh = unreal.EditorAssetLibrary.load_asset(RUIN_MESH_PATH)
    if new_mesh is None:
        log("FAIL: could not load %s" % RUIN_MESH_PATH)
        return False

    entries = asset.get_editor_property("entries")
    changed = False
    for i in range(len(entries)):
        entry = entries[i]
        if entry.get_editor_property("semantic_type") != unreal.AnastasisSemanticType.RUIN:
            continue
        variants = entry.get_editor_property("variants")
        log("Ruin entry[%d] found, %d variant(s), old mesh=%s" % (
            i, len(variants), variants[0].get_editor_property("mesh")))
        variant0 = variants[0]
        variant0.set_editor_property("mesh", new_mesh)
        variants[0] = variant0                              # write struct copy back into its array
        entry.set_editor_property("variants", variants)      # write array back into its owning struct
        entries[i] = entry                                   # write struct copy back into its array
        changed = True
        log("Ruin variant[0].mesh set to %s (pending save)" % new_mesh)
        break

    if not changed:
        log("FAIL: no Ruin entry found in registry")
        return False

    asset.set_editor_property("entries", entries)             # write array back into the asset
    asset.modify()
    saved = unreal.EditorAssetLibrary.save_asset(REGISTRY_PATH, only_if_is_dirty=False)
    log("save_asset returned %s" % saved)

    # Independent verification: reload fresh (not the in-memory `asset` object
    # this process already mutated) and check what is actually on disk now.
    unreal.EditorAssetLibrary.unload_assets([REGISTRY_PATH])
    reloaded = unreal.EditorAssetLibrary.load_asset(REGISTRY_PATH)
    ok = False
    for entry in reloaded.get_editor_property("entries"):
        if entry.get_editor_property("semantic_type") == unreal.AnastasisSemanticType.RUIN:
            mesh_now = entry.get_editor_property("variants")[0].get_editor_property("mesh")
            log("VERIFY reloaded Ruin variant[0].mesh = %s" % mesh_now)
            ok = mesh_now is not None and mesh_now.get_path_name().startswith(RUIN_MESH_PATH)
            break
    log("VERIFY::%s" % ("PASS" if ok else "FAIL"))
    return ok


main()
