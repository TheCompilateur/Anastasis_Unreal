"""Read-only: dump the current contents of DA_AnastasisPresentation."""
import unreal

ASSET_PATH = "/Game/Anastasis/Presentation/DA_AnastasisPresentation"


def log(msg):
    unreal.log("[inspect_registry] " + str(msg))


def main():
    exists = unreal.EditorAssetLibrary.does_asset_exist(ASSET_PATH)
    log("asset_exists=%s" % exists)
    if not exists:
        return
    asset = unreal.EditorAssetLibrary.load_asset(ASSET_PATH)
    log("loaded=%s class=%s" % (asset, asset.get_class().get_name() if asset else None))
    entries = asset.get_editor_property("entries")
    log("entry_count=%s" % len(entries))
    for i, entry in enumerate(entries):
        semantic = entry.get_editor_property("semantic_type")
        archetype_id = entry.get_editor_property("archetype_id")
        enabled = entry.get_editor_property("enabled")
        tint = entry.get_editor_property("tint")
        min_s = entry.get_editor_property("min_uniform_scale")
        max_s = entry.get_editor_property("max_uniform_scale")
        jitter = entry.get_editor_property("jitter_radius_fraction")
        variants = entry.get_editor_property("variants")
        log("[%d] type=%s id=%s enabled=%s tint=%s scale=%s..%s jitter=%s variants=%d" % (
            i, semantic, archetype_id, enabled, tint, min_s, max_s, jitter, len(variants)))
        for j, v in enumerate(variants):
            mesh = v.get_editor_property("mesh")
            mat = v.get_editor_property("material_override")
            log("    variant[%d] mesh=%s material_override=%s" % (j, mesh, mat))


main()
