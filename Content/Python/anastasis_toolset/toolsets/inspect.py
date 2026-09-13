from __future__ import annotations

import json
import time
from pathlib import Path

import unreal
import toolset_registry


@unreal.ustruct()
class AnastasisInspectionResult(unreal.StructBase):
    """Structured result for read-only Anastasis inspection tools."""

    success = unreal.uproperty(bool)
    status = unreal.uproperty(str)
    schema = unreal.uproperty(str)
    path = unreal.uproperty(str)
    json = unreal.uproperty(str)


@unreal.ustruct()
class AnastasisVerificationResult(unreal.StructBase):
    """Structured result for read-only Anastasis verification tools."""

    success = unreal.uproperty(bool)
    status = unreal.uproperty(str)
    schema = unreal.uproperty(str)
    path = unreal.uproperty(str)
    json = unreal.uproperty(str)


@unreal.ustruct()
class AnastasisProbeResult(unreal.StructBase):
    """Structured result for deterministic Anastasis probe tools."""

    success = unreal.uproperty(bool)
    status = unreal.uproperty(str)
    schema = unreal.uproperty(str)
    path = unreal.uproperty(str)
    json = unreal.uproperty(str)


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


def _world_context() -> unreal.World:
    editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    if editor is None:
        raise RuntimeError("UnrealEditorSubsystem is not available.")
    world = editor.get_game_world() or editor.get_editor_world()
    if world is None:
        raise RuntimeError("No editor or PIE world is available.")
    return world


def _latest_inspection_path(slug: str) -> Path:
    return (
        Path(unreal.Paths.project_saved_dir())
        / "Anastasis"
        / "Diagnostics"
        / "inspections"
        / f"latest-{slug}.json"
    )


def _latest_verification_path(slug: str) -> Path:
    return (
        Path(unreal.Paths.project_saved_dir())
        / "Anastasis"
        / "Diagnostics"
        / "verifications"
        / f"latest-{slug}.json"
    )


def _latest_probe_path(slug: str) -> Path:
    return (
        Path(unreal.Paths.project_saved_dir())
        / "Anastasis"
        / "Diagnostics"
        / "probes"
        / f"latest-{slug}.json"
    )


def _failure_result(schema: str, reason: str) -> AnastasisInspectionResult:
    result = AnastasisInspectionResult()
    result.success = False
    result.status = "ERROR"
    result.schema = schema
    result.path = ""
    result.json = json.dumps(
        {
            "schema": schema,
            "status": "ERROR",
            "error": reason,
            "evidence_scope": {
                "mode": "READ_ONLY",
                "mec": "failed before inspection JSON could be read",
                "scn": "UNKNOWN",
                "ply": "UNKNOWN",
            },
        },
        sort_keys=True,
    )
    return result


def _verification_failure_result(schema: str, reason: str) -> AnastasisVerificationResult:
    result = AnastasisVerificationResult()
    result.success = False
    result.status = "ERROR"
    result.schema = schema
    result.path = ""
    result.json = json.dumps(
        {
            "schema": schema,
            "status": "ERROR",
            "error": reason,
            "evidence_scope": {
                "mode": "VERIFY_READ_ONLY",
                "mec": "failed before verification JSON could be read",
                "scn": "UNKNOWN",
                "ply": "UNKNOWN",
            },
        },
        sort_keys=True,
    )
    return result


def _probe_failure_result(schema: str, reason: str) -> AnastasisProbeResult:
    result = AnastasisProbeResult()
    result.success = False
    result.status = "ERROR"
    result.schema = schema
    result.path = ""
    result.json = json.dumps(
        {
            "schema": schema,
            "status": "ERROR",
            "error": reason,
            "evidence_scope": {
                "mode": "EXPERIMENTAL_PROBE",
                "mec": "failed before probe JSON could be read",
                "scn": "UNKNOWN",
                "ply": "UNKNOWN",
            },
        },
        sort_keys=True,
    )
    return result


def _run_cpp_inspection(
    command: str, latest_slug: str, expected_schema: str
) -> AnastasisInspectionResult:
    started_at = time.time() - 2.0
    try:
        unreal.SystemLibrary.execute_console_command(_world_context(), command)
        path = _latest_inspection_path(latest_slug)
        if not path.exists():
            return _failure_result(expected_schema, f"Inspection JSON not found: {path}")
        if path.stat().st_mtime < started_at:
            return _failure_result(expected_schema, f"Inspection JSON appears stale: {path}")

        raw_json = path.read_text(encoding="utf-8")
        payload = json.loads(raw_json)
        result = AnastasisInspectionResult()
        result.success = payload.get("status") not in ("ERROR", "")
        result.status = str(payload.get("status", "UNKNOWN"))
        result.schema = str(payload.get("schema", expected_schema))
        result.path = str(path)
        result.json = raw_json
        return result
    except Exception as exc:
        return _failure_result(expected_schema, str(exc))


def _run_cpp_verification(
    command: str, latest_slug: str, expected_schema: str
) -> AnastasisVerificationResult:
    started_at = time.time() - 2.0
    try:
        unreal.SystemLibrary.execute_console_command(_world_context(), command)
        path = _latest_verification_path(latest_slug)
        if not path.exists():
            return _verification_failure_result(
                expected_schema, f"Verification JSON not found: {path}"
            )
        if path.stat().st_mtime < started_at:
            return _verification_failure_result(
                expected_schema, f"Verification JSON appears stale: {path}"
            )

        raw_json = path.read_text(encoding="utf-8")
        payload = json.loads(raw_json)
        result = AnastasisVerificationResult()
        result.success = payload.get("status") == "PASS"
        result.status = str(payload.get("status", "UNKNOWN"))
        result.schema = str(payload.get("schema", expected_schema))
        result.path = str(path)
        result.json = raw_json
        return result
    except Exception as exc:
        return _verification_failure_result(expected_schema, str(exc))


def _run_cpp_probe(
    command: str, latest_slug: str, expected_schema: str
) -> AnastasisProbeResult:
    started_at = time.time() - 2.0
    try:
        unreal.SystemLibrary.execute_console_command(_world_context(), command)
        path = _latest_probe_path(latest_slug)
        if not path.exists():
            return _probe_failure_result(
                expected_schema, f"Probe JSON not found: {path}"
            )
        if path.stat().st_mtime < started_at:
            return _probe_failure_result(
                expected_schema, f"Probe JSON appears stale: {path}"
            )

        raw_json = path.read_text(encoding="utf-8")
        payload = json.loads(raw_json)
        result = AnastasisProbeResult()
        result.success = payload.get("status") == "PASS"
        result.status = str(payload.get("status", "UNKNOWN"))
        result.schema = str(payload.get("schema", expected_schema))
        result.path = str(path)
        result.json = raw_json
        return result
    except Exception as exc:
        return _probe_failure_result(expected_schema, str(exc))


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

    @toolset_registry.tool_call
    @staticmethod
    def inspect_anastasis_world() -> AnastasisInspectionResult:
        """Runs a read-only Anastasis world inspection and returns the JSON snapshot.

        Returns:
            Structured result with status, schema, path, and raw JSON.
        """
        return _run_cpp_inspection(
            "Anastasis.Inspect.World",
            "inspect_world",
            "anastasis.inspect_world.v1",
        )

    @toolset_registry.tool_call
    @staticmethod
    def inspect_tile(x: int, y: int) -> AnastasisInspectionResult:
        """Inspects one embodied Anastasis tile by canonical/source coordinates.

        Args:
            x: Source tile X coordinate.
            y: Source tile Y coordinate.

        Returns:
            Structured result with status, schema, path, and raw JSON.
        """
        return _run_cpp_inspection(
            f"Anastasis.Inspect.Tile {x} {y}",
            "inspect_tile",
            "anastasis.inspect_tile.v1",
        )

    @toolset_registry.tool_call
    @staticmethod
    def inspect_settlement(settlement_id: str = "") -> AnastasisInspectionResult:
        """Reports Unreal-side settlement observability without claiming implementation.

        Args:
            settlement_id: Optional settlement identifier or actor-name hint.

        Returns:
            Structured result. Current status is expected to be NOT_IMPLEMENTED until
            a canonical settlement runtime exists in Unreal.
        """
        command = "Anastasis.Inspect.Settlement"
        if settlement_id:
            command = f"{command} {settlement_id}"
        return _run_cpp_inspection(
            command,
            "inspect_settlement",
            "anastasis.inspect_settlement.v1",
        )

    @toolset_registry.tool_call
    @staticmethod
    def inspect_actor(query: str) -> AnastasisInspectionResult:
        """Inspects level actors whose name, label, class, or tag matches query.

        Args:
            query: Case-insensitive actor name, label, class, or tag fragment.

        Returns:
            Structured result with up to 20 actor matches in raw JSON.
        """
        return _run_cpp_inspection(
            f"Anastasis.Inspect.Actor {query}",
            "inspect_actor",
            "anastasis.inspect_actor.v1",
        )

    @toolset_registry.tool_call
    @staticmethod
    def inspect_visual_scene_state() -> AnastasisInspectionResult:
        """Inspects read-only visual scene/component delivery facts.

        Returns:
            Structured result with mode, CVar, embodiment, and component summary.
        """
        return _run_cpp_inspection(
            "Anastasis.Inspect.VisualSceneState",
            "inspect_visual_scene_state",
            "anastasis.inspect_visual_scene_state.v1",
        )

    @toolset_registry.tool_call
    @staticmethod
    def verify_world_contract() -> AnastasisVerificationResult:
        """Verifies core Anastasis world invariants as PASS/FAIL/UNKNOWN.

        Returns:
            Structured verification result with raw JSON checks.
        """
        return _run_cpp_verification(
            "Anastasis.Verify.WorldContract",
            "verify_world_contract",
            "anastasis.verify_world_contract.v1",
        )

    @toolset_registry.tool_call
    @staticmethod
    def verify_settlement_contract(
        settlement_id: str = "",
    ) -> AnastasisVerificationResult:
        """Verifies settlement delivery without promoting unavailable evidence.

        Args:
            settlement_id: Optional settlement identifier or actor-name hint.

        Returns:
            UNKNOWN until a canonical Unreal settlement runtime exists.
        """
        command = "Anastasis.Verify.SettlementContract"
        if settlement_id:
            command = f"{command} {settlement_id}"
        return _run_cpp_verification(
            command,
            "verify_settlement_contract",
            "anastasis.verify_settlement_contract.v1",
        )

    @toolset_registry.tool_call
    @staticmethod
    def verify_navigation_contract() -> AnastasisVerificationResult:
        """Verifies NavigationSystem/NavData/build state as PASS/FAIL/UNKNOWN.

        Returns:
            Structured verification result with raw JSON checks.
        """
        return _run_cpp_verification(
            "Anastasis.Verify.NavigationContract",
            "verify_navigation_contract",
            "anastasis.verify_navigation_contract.v1",
        )

    @toolset_registry.tool_call
    @staticmethod
    def verify_visual_delivery() -> AnastasisVerificationResult:
        """Separates MEC visual delivery from SCN and PLY proof.

        Returns:
            Structured verification result. Headless runs should keep SCN/PLY
            UNKNOWN unless paired capture/player evidence exists.
        """
        return _run_cpp_verification(
            "Anastasis.Verify.VisualDelivery",
            "verify_visual_delivery",
            "anastasis.verify_visual_delivery.v1",
        )

    @toolset_registry.tool_call
    @staticmethod
    def verify_semantic_slice(
        origin_x: int = 0,
        origin_y: int = 0,
        size: int = 32,
    ) -> AnastasisVerificationResult:
        """Verifies semantic slice richness as PASS/FAIL/UNKNOWN.

        Args:
            origin_x: Source tile X coordinate of the slice origin.
            origin_y: Source tile Y coordinate of the slice origin.
            size: Square slice size in tiles.

        Returns:
            Structured verification result with water/forest/field/clearing/shore metrics.
        """
        return _run_cpp_verification(
            f"Anastasis.Verify.SemanticSlice {origin_x} {origin_y} {size}",
            "verify_semantic_slice",
            "anastasis.verify_semantic_slice.v1",
        )

    @toolset_registry.tool_call
    @staticmethod
    def run_worldgen_probe(
        seed: int = 12345,
        source_width: int = 96,
        source_height: int = 96,
        hour: float = 12.0,
        profile: str = "canonical",
    ) -> AnastasisProbeResult:
        """Runs a deterministic worldgen probe with locked parameters.

        Args:
            seed: World seed.
            source_width: Source world width.
            source_height: Source world height.
            hour: Declared comparison hour for the probe protocol.
            profile: Probe profile label, e.g. canonical.

        Returns:
            Structured probe result with deterministic fingerprint and terrain counts.
        """
        return _run_cpp_probe(
            f"Anastasis.Probe.Worldgen {seed} {source_width} {source_height} {hour} {profile}",
            "run_worldgen_probe",
            "anastasis.probe_worldgen.v1",
        )

    @toolset_registry.tool_call
    @staticmethod
    def find_semantic_slice(
        seed: int = 12345,
        size: int = 32,
        profile: str = "canonical",
    ) -> AnastasisProbeResult:
        """Finds the best deterministic semantic slice in the canonical 96x96 world.

        Args:
            seed: World seed.
            size: Square slice size.
            profile: Probe profile label, e.g. canonical.

        Returns:
            Structured probe result with ranked slice candidates.
        """
        return _run_cpp_probe(
            f"Anastasis.Probe.FindSemanticSlice {seed} {size} {profile}",
            "find_semantic_slice",
            "anastasis.probe_find_semantic_slice.v1",
        )

    @toolset_registry.tool_call
    @staticmethod
    def compare_slice_candidates(
        candidates: str,
        seed: int = 12345,
        size: int = 32,
        profile: str = "canonical",
    ) -> AnastasisProbeResult:
        """Compares explicit slice candidates with the same deterministic scoring.

        Args:
            candidates: Semicolon-separated origins, e.g. "0,0;24,12;48,48".
            seed: World seed.
            size: Square slice size.
            profile: Probe profile label, e.g. canonical.

        Returns:
            Structured probe result with ranked candidate metrics.
        """
        return _run_cpp_probe(
            f"Anastasis.Probe.CompareSliceCandidates {seed} {size} {candidates} {profile}",
            "compare_slice_candidates",
            "anastasis.probe_compare_slice_candidates.v1",
        )

    @toolset_registry.tool_call
    @staticmethod
    def capture_fixed_view_probe(
        seed: int = 12345,
        origin_x: int = 0,
        origin_y: int = 0,
        size: int = 32,
        hour: float = 12.0,
        camera: str = "OVERVIEW",
        profile: str = "canonical",
    ) -> AnastasisProbeResult:
        """Requests a fixed-view capture probe without claiming player proof.

        Args:
            seed: Expected embodied world seed.
            origin_x: Expected source tile X origin of the embodied slice.
            origin_y: Expected source tile Y origin of the embodied slice.
            size: Expected square embodied slice size.
            hour: Declared comparison hour for the probe protocol.
            camera: Bookmark/camera name, e.g. OVERVIEW, GROUND, SHORE.
            profile: Probe profile label, e.g. canonical.

        Returns:
            UNKNOWN in headless/nullrhi unless an actual paired capture can be requested.
        """
        return _run_cpp_probe(
            (
                "Anastasis.Probe.CaptureFixedView "
                f"{seed} {origin_x} {origin_y} {size} {hour} {camera} {profile}"
            ),
            "capture_fixed_view_probe",
            "anastasis.probe_capture_fixed_view.v1",
        )
