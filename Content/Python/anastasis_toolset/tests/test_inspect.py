import unittest

from anastasis_toolset.toolsets.inspect import AnastasisInspectTools, _actor_line


class AnastasisInspectToolsTestCase(unittest.TestCase):
    """Read-only AnastasisInspectTools checks."""

    def test_list_level_actors_raises_on_non_positive_max_count(self):
        with self.assertRaises(ValueError):
            AnastasisInspectTools.list_level_actors("", 0)

    def test_get_session_snapshot_has_required_keys(self):
        snapshot = AnastasisInspectTools.get_session_snapshot()
        for key in ("engine", "project", "world", "actor_count", "pie"):
            self.assertIn(key, snapshot)
        self.assertIn("5.8", snapshot["engine"])

    def test_list_selected_actors_returns_list(self):
        selected = AnastasisInspectTools.list_selected_actors()
        self.assertIsInstance(selected, list)

    def test_actor_line_helper_shape(self):
        self.assertTrue(callable(_actor_line))
