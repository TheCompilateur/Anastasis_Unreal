"""Create/rebuild the complete presentation registry; otherwise inspect only.

The saved Data Asset owns the runtime look. An explicit rebuild replaces its
entries with the existing tree grammar and dedicated ruin binding. Dependencies
are loaded before any asset mutation; the package is never deleted/recreated.
"""
import importlib.util
import os
from pathlib import Path
import unreal

ASSET_DIR = "/Game/Anastasis/Presentation"
ASSET_NAME = "DA_AnastasisPresentation"
ASSET_PATH = ASSET_DIR + "/" + ASSET_NAME


def log(msg):
    unreal.log("PRESENTATION_REGISTRY " + str(msg))


def recipe(filename):
    path = Path(__file__).resolve().with_name(filename)
    spec = importlib.util.spec_from_file_location("_registry_" + path.stem, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def set_prop(obj, names, value):
    for name in names:
        try:
            obj.set_editor_property(name, value)
            return
        except Exception:
            continue
    raise RuntimeError("No writable property: " + ",".join(names))


def make_entry(semantic, archetype, tint, minimum, maximum, jitter, lean, variants):
    if not variants:
        raise RuntimeError("No variants for " + archetype)
    entry = unreal.AnastasisPresentationEntry()
    entry.set_editor_property("semantic_type", semantic)
    entry.set_editor_property("archetype_id", archetype)
    set_prop(entry, ("enabled", "b_enabled"), True)
    entry.set_editor_property("tint", unreal.LinearColor(*tint, 1.0))
    entry.set_editor_property("min_uniform_scale", minimum)
    entry.set_editor_property("max_uniform_scale", maximum)
    entry.set_editor_property("jitter_radius_fraction", jitter)
    entry.set_editor_property("max_lean_degrees", lean)
    set_prop(entry, ("random_yaw", "b_random_yaw"), True)
    entry.set_editor_property("variants", variants)
    return entry


def make_seed_entries():
    # Import-safe existing owners: no duplicate tree paths, biases or materials.
    trees = recipe("set_tree_grammar.py")
    bindings = recipe("set_presentation_meshes.py")
    tree_variants = trees.build_variants()
    ruin_paths = [path for semantic, path in bindings.TARGETS if semantic == "RUIN"]
    if len(ruin_paths) != 1:
        raise RuntimeError("Expected exactly one RUIN binding")
    ruin_path = ruin_paths[0]
    ruin_mesh = unreal.EditorAssetLibrary.load_asset(ruin_path)
    if ruin_mesh is None:
        raise RuntimeError("Missing RUIN mesh: " + ruin_path)
    ruin = unreal.AnastasisPresentationVariant()
    ruin.set_editor_property("mesh", ruin_mesh)
    return [
        make_entry(unreal.AnastasisSemanticType.FOREST, trees.ARCHETYPE, trees.TINT,
                   trees.MIN_SCALE, trees.MAX_SCALE, trees.JITTER,
                   trees.MAX_LEAN_DEGREES, tree_variants),
        make_entry(unreal.AnastasisSemanticType.RUIN, "Ruin_Generic",
                   (0.353, 0.302, 0.318), 0.6, 1.1, 0.20, 0.0, [ruin]),
    ]


def inspect(asset):
    entries = asset.get_editor_property("entries")
    log("INSPECT entries=%d evidence=memory" % len(entries))
    for entry in entries:
        variants = entry.get_editor_property("variants")
        meshes = [v.get_editor_property("mesh") for v in variants]
        log("entry semantic=%s variants=%d meshes=%s" % (
            entry.get_editor_property("semantic_type"), len(variants),
            ",".join(m.get_path_name() if m else "NONE" for m in meshes)))


def main(rebuild=None):
    if rebuild is None:
        rebuild = os.environ.get("ANASTASIS_PRESENTATION_REBUILD", "0") == "1"
    eal = unreal.EditorAssetLibrary
    exists = eal.does_asset_exist(ASSET_PATH)
    asset = eal.load_asset(ASSET_PATH) if exists else None
    if exists and asset is None:
        raise RuntimeError("Existing registry cannot be loaded: " + ASSET_PATH)
    if exists and not isinstance(asset, unreal.AnastasisPresentationRegistry):
        raise RuntimeError("Wrong registry class: " + ASSET_PATH)
    if exists and not rebuild:
        inspect(asset)
        log("UNCHANGED (no rebuild requested)")
        return asset

    # Complete dependency preflight. A missing mesh/material leaves the current
    # package and its entries untouched, including when rebuild was requested.
    entries = make_seed_entries()
    old_entries = list(asset.get_editor_property("entries")) if exists else None
    if not exists:
        factory = unreal.DataAssetFactory()
        factory.set_editor_property("data_asset_class", unreal.AnastasisPresentationRegistry)
        asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            ASSET_NAME, ASSET_DIR, unreal.AnastasisPresentationRegistry, factory)
        if asset is None:
            raise RuntimeError("Registry creation failed: " + ASSET_PATH)
    try:
        asset.modify()
        asset.set_editor_property("entries", entries)
        if not eal.save_asset(ASSET_PATH, only_if_is_dirty=False):
            raise RuntimeError("Registry save failed: " + ASSET_PATH)
    except Exception:
        if old_entries is not None:
            asset.set_editor_property("entries", old_entries)
        raise
    log("SAVED entries=%d forest_variants=%d ruin_variants=%d" % (
        len(entries), len(entries[0].get_editor_property("variants")),
        len(entries[1].get_editor_property("variants"))))
    inspect(asset)
    log("COMPLETE (saved; disk reload requires a fresh editor process)")
    return asset


if __name__ == "__main__":
    try:
        main()
    finally:
        if os.environ.get("ANASTASIS_PRESENTATION_KEEP_EDITOR", "0") != "1":
            unreal.SystemLibrary.quit_editor()
