import unreal
import toolset_registry


def _actor_line(actor: unreal.Actor) -> str:
    loc = actor.get_actor_location()
    return (
        f"{actor.get_actor_label()} | {actor.get_class().get_name()} | "
        f"{loc.x:.1f},{loc.y:.1f},{loc.z:.1f}"
    )


def _editor_actor_subsystem() -> unreal.EditorActorSubsystem:
    subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    if subsystem is None:
        raise RuntimeError("EditorActorSubsystem is not available.")
    return subsystem


@unreal.uclass()
class AnastasisInspectTools(unreal.ToolsetDefinition):
    """Read-only inspection of the live Anastasis editor session (map, actors, selection)."""

    @toolset_registry.tool_call
    @staticmethod
    def get_session_snapshot() -> dict[str, str]:
        """Returns engine, project, current editor world, actor count, and PIE state.

        Returns:
            String-valued snapshot keys. actor_count is a decimal integer string.
            pie is 'true' or 'false'.
        """
        editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
        world = editor.get_editor_world() if editor else None
        actors = []
        if world is not None:
            actors = list(_editor_actor_subsystem().get_all_level_actors())

        game_world = editor.get_game_world() if editor is not None else None
        pie = game_world is not None

        world_name = world.get_name() if world is not None else ""
        return {
            "engine": unreal.SystemLibrary.get_engine_version(),
            "project": unreal.SystemLibrary.get_game_name(),
            "world": world_name,
            "actor_count": str(len(actors)),
            "pie": "true" if pie else "false",
        }

    @toolset_registry.tool_call
    @staticmethod
    def list_level_actors(name_filter: str = "", max_count: int = 80) -> list[str]:
        """Lists loaded level actors as 'Label | Class | X,Y,Z'.

        Args:
            name_filter: Case-insensitive substring of actor label. Empty matches all.
            max_count: Maximum rows to return. Must be > 0.

        Returns:
            Actor lines, truncated to max_count.
        """
        if max_count <= 0:
            raise ValueError("max_count must be greater than 0.")

        needle = name_filter.lower()
        lines: list[str] = []
        for actor in _editor_actor_subsystem().get_all_level_actors():
            label = actor.get_actor_label()
            if needle and needle not in label.lower():
                continue
            lines.append(_actor_line(actor))
            if len(lines) >= max_count:
                break
        return lines

    @toolset_registry.tool_call
    @staticmethod
    def list_selected_actors() -> list[str]:
        """Lists currently selected level actors as 'Label | Class | X,Y,Z'.

        Returns:
            Empty list when nothing is selected.
        """
        return [
            _actor_line(actor)
            for actor in _editor_actor_subsystem().get_selected_level_actors()
        ]
