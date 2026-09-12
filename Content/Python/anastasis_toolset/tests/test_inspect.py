import unittest

from anastasis_toolset.toolsets.inspect import AnastasisInspectTools, _actor_line


class AnastasisInspectToolsTestCase(unittest.TestCase):
    """Read-only AnastasisInspectTools checks."""

    # Known preexisting failures (AGENTS.md "Echecs preexistants connus"). Not
    # a mandate to fix here. Root cause, confirmed by reading tool_call_impl.py
    # in the ToolsetRegistry plugin: @toolset_registry.tool_call turns the
    # method into a real UFunction, which changes its calling convention.
    # Exceptions are caught and converted to script errors instead of being
    # raised as ValueError (unless called inside
    # `toolset_registry.tool_raising_exceptions()`), and list-typed returns are
    # marshaled to `unreal.Array`, not a native `list`. The tests below assert
    # the undecorated Python semantics, which the decorator intentionally
    # changes — expected, not asserted as a fix, until a mandate revisits them.
    @unittest.expectedFailure
    def test_list_level_actors_raises_on_non_positive_max_count(self):
        with self.assertRaises(ValueError):
            AnastasisInspectTools.list_level_actors("", 0)

    def test_get_session_snapshot_has_required_keys(self):
        snapshot = AnastasisInspectTools.get_session_snapshot()
        for key in ("engine", "project", "world", "actor_count", "pie"):
            self.assertIn(key, snapshot)
        self.assertIn("5.8", snapshot["engine"])

    @unittest.expectedFailure
    def test_list_selected_actors_returns_list(self):
        selected = AnastasisInspectTools.list_selected_actors()
        self.assertIsInstance(selected, list)

    def test_actor_line_helper_shape(self):
        self.assertTrue(callable(_actor_line))
