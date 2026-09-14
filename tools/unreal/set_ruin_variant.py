"""
ANASTASIS_ASSET_AGENT_002 -- Phase 5 integration.

Points the Ruin entry's Variants[0].Mesh in DA_AnastasisPresentation at
SM_Ruin_Generic_01 instead of /Engine/BasicShapes/Cylinder.Cylinder.
Leaves the Forest entry, Tint, scale envelope, jitter and bRandomYaw
untouched -- this is a one-field data edit, not a resolver or placement
change.
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
    for entry in entries:
        semantic = entry.get_editor_property("semantic_type")
        if semantic == unreal.AnastasisSemanticType.RUIN:
            variants = entry.get_editor_property("variants")
            log("Ruin entry found, %d variant(s), old mesh=%s" % (
                len(variants), variants[0].get_editor_property("mesh") if variants else None))
            variants[0].set_editor_property("mesh", new_mesh)
            entry.set_editor_property("variants", variants)
            changed = True
            log("Ruin variant[0].mesh set to %s" % new_mesh)
            break

    if not changed:
        log("FAIL: no Ruin entry found in registry")
        return False

    asset.set_editor_property("entries", entries)
    unreal.EditorAssetLibrary.save_asset(REGISTRY_PATH)
    log("saved %s" % REGISTRY_PATH)
    return True


main()
