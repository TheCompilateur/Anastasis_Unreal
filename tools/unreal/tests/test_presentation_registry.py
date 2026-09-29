"""Regression checks for registry creation, preservation and failed rebuilds.

Runs the real registry/grammar/binding modules against an in-memory editor API.
This tests lifecycle and data contracts, not Unreal serialization or rendering.
"""
import importlib.util
from pathlib import Path
import sys
import types
import unittest


class Struct:
    def __init__(self):
        self.props = {}
    def set_editor_property(self, key, value):
        self.props[key] = value
    def get_editor_property(self, key):
        return self.props.get(key)
    def modify(self):
        pass


class Registry(Struct):
    pass


class Asset:
    def __init__(self, path):
        self.path = path
    def get_path_name(self):
        return self.path


class Editor:
    def __init__(self):
        self.registry = None
        self.missing = None
        self.saved = 0
        self.created = 0
        self.save_ok = True
        self.create_ok = True
        self.unloadable = False
    def does_asset_exist(self, path):
        return self.registry is not None
    def load_asset(self, path):
        if "DA_AnastasisPresentation" in path:
            return None if self.unloadable else self.registry
        return None if path == self.missing else Asset(path)
    def save_asset(self, path, **kwargs):
        self.saved += 1
        return self.save_ok
    def create_asset(self, *args):
        self.created += 1
        if not self.create_ok:
            return None
        self.registry = Registry()
        return self.registry
    def delete_asset(self, *args):
        raise AssertionError("Registry deletion is forbidden")


class RegistryTests(unittest.TestCase):
    def setUp(self):
        self.editor = Editor()
        self.logs = []
        self.unreal = types.SimpleNamespace(
            log=self.logs.append,
            EditorAssetLibrary=self.editor,
            AssetToolsHelpers=types.SimpleNamespace(get_asset_tools=lambda: self.editor),
            AnastasisPresentationRegistry=Registry,
            AnastasisPresentationEntry=Struct,
            AnastasisPresentationVariant=Struct,
            DataAssetFactory=Struct,
            LinearColor=lambda *args: args,
            AnastasisSemanticType=types.SimpleNamespace(FOREST="FOREST", RUIN="RUIN"),
            AnastasisStatureClass=types.SimpleNamespace(
                UNDERSTORY="UNDERSTORY", SUBCANOPY="SUBCANOPY",
                CANOPY="CANOPY", EMERGENT="EMERGENT"),
            AnastasisFoliageFamily=types.SimpleNamespace(
                CONIFER="CONIFER", BROADLEAF="BROADLEAF"),
        )
        self.previous = sys.modules.get("unreal")
        sys.modules["unreal"] = self.unreal
        path = Path(__file__).resolve().parents[1] / "presentation-registry.py"
        spec = importlib.util.spec_from_file_location("registry_under_test", path)
        self.module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(self.module)

    def tearDown(self):
        if self.previous is None:
            sys.modules.pop("unreal", None)
        else:
            sys.modules["unreal"] = self.previous

    def existing(self):
        self.editor.registry = Registry()
        custom = [object()]
        self.editor.registry.set_editor_property("entries", custom)
        return custom

    def test_import_is_read_only_including_recipe_imports(self):
        self.module.recipe("set_tree_grammar.py")
        self.module.recipe("set_presentation_meshes.py")
        self.assertEqual(self.editor.created, 0)
        self.assertEqual(self.editor.saved, 0)
        self.assertEqual(self.logs, [])

    def test_fresh_registry_has_complete_tree_grid_materials_and_dedicated_ruin(self):
        asset = self.module.main(rebuild=False)
        entries = asset.get_editor_property("entries")
        self.assertEqual([e.get_editor_property("semantic_type") for e in entries], ["FOREST", "RUIN"])
        variants = entries[0].get_editor_property("variants")
        expected = {(s, f) for s in ("UNDERSTORY", "SUBCANOPY", "CANOPY", "EMERGENT")
                    for f in ("CONIFER", "BROADLEAF")}
        self.assertEqual(len(variants), 8)
        self.assertEqual({(v.get_editor_property("stature"), v.get_editor_property("family"))
                          for v in variants}, expected)
        for v in variants:
            self.assertTrue(v.get_editor_property("material_override").path.endswith("/M_AnastasisVegetation"))
            self.assertEqual(len(v.get_editor_property("additional_material_overrides")), 1)
            self.assertTrue(v.get_editor_property("additional_material_overrides")[0].path.endswith("/M_AnastasisBark"))
        self.assertEqual(entries[1].get_editor_property("variants")[0].get_editor_property("mesh").path,
                         "/Game/Anastasis/Architecture/SM_Ruin_Generic_01")
        self.assertEqual(self.editor.saved, 1)

    def test_existing_artist_values_are_not_reseeded(self):
        asset = self.module.main()
        entries = asset.get_editor_property("entries")
        entries[0].set_editor_property("min_uniform_scale", 9.25)
        self.module.main(rebuild=False)
        self.assertIs(asset.get_editor_property("entries"), entries)
        self.assertEqual(entries[0].get_editor_property("min_uniform_scale"), 9.25)
        self.assertEqual(self.editor.saved, 1)

    def test_rebuild_keeps_asset_identity(self):
        self.existing()
        original = self.editor.registry
        self.module.main(rebuild=True)
        self.assertIs(self.editor.registry, original)
        self.assertEqual(self.editor.created, 0)
        self.assertEqual(len(original.get_editor_property("entries")[0].get_editor_property("variants")), 8)

    def test_missing_dependencies_do_not_mutate_existing_registry(self):
        for missing in ("/Game/Anastasis/Materials/M_AnastasisBark",
                        "/Game/Anastasis/Vegetation/SM_Tree_Broadleaf_Emergent_01",
                        "/Game/Anastasis/Architecture/SM_Ruin_Generic_01"):
            with self.subTest(missing=missing):
                custom = self.existing()
                self.editor.missing = missing
                with self.assertRaises(Exception):
                    self.module.main(rebuild=True)
                self.assertIs(self.editor.registry.get_editor_property("entries"), custom)
                self.assertEqual(self.editor.saved, 0)

    def test_missing_dependency_does_not_create_incomplete_registry(self):
        self.editor.missing = "/Game/Anastasis/Materials/M_AnastasisVegetation"
        with self.assertRaises(Exception):
            self.module.main()
        self.assertEqual(self.editor.created, 0)

    def test_failed_save_is_failure_and_restores_existing_entries(self):
        custom = self.existing()
        self.editor.save_ok = False
        with self.assertRaisesRegex(RuntimeError, "save failed"):
            self.module.main(rebuild=True)
        self.assertEqual(self.editor.registry.get_editor_property("entries"), custom)
        self.assertFalse(any("COMPLETE" in line for line in self.logs))

    def test_unloadable_registry_is_not_replaced(self):
        self.existing()
        self.editor.unloadable = True
        with self.assertRaisesRegex(RuntimeError, "cannot be loaded"):
            self.module.main(rebuild=True)
        self.assertEqual(self.editor.created, 0)
        self.assertEqual(self.editor.saved, 0)

    def test_wrong_asset_class_is_rejected(self):
        self.editor.registry = Struct()
        with self.assertRaisesRegex(RuntimeError, "Wrong registry class"):
            self.module.main(rebuild=True)
        self.assertEqual(self.editor.saved, 0)

    def test_factory_failure_is_reported(self):
        self.editor.create_ok = False
        with self.assertRaisesRegex(RuntimeError, "creation failed"):
            self.module.main()


if __name__ == "__main__":
    unittest.main(verbosity=2)
