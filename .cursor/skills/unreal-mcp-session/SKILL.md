---
name: unreal-mcp-session
description: Connects to Unreal Engine 5.8.2 MCP, discovers toolsets, and inspects the live Anastasis editor before any mutation. Use when the editor is running, when using unreal-mcp, list_toolsets, describe_toolset, call_tool, AnastasisInspectTools, or when the user asks to look at the level, actors, or viewport session.
---

# Unreal MCP session (5.8.2)

Endpoint: `http://127.0.0.1:8000/mcp` (project `.cursor/mcp.json`).

## Before mutating anything

1. Confirm the Unreal Editor is open on `Anastasis_UnrealV2.uproject`.
2. Discover: `list_toolsets` then `describe_toolset` for `AnastasisInspectTools` and any Epic set you need.
3. Call `AnastasisInspectTools.get_session_snapshot`.
4. Then `list_level_actors` / `list_selected_actors` as needed.
5. Stop if MCP is down. Do not invent scene state from `Intermediate/` or binary `.uasset` text.

## If MCP is unreachable

Ask Alexandre to:

1. Restart the editor after plugin enable (Unreal MCP, EditorToolset, Python).
2. Confirm **Editor Preferences → Model Context Protocol → Auto Start Server**, or run `ModelContextProtocol.StartServer`.
3. Run `ModelContextProtocol.RefreshTools` after Python toolset edits.
4. Reload Cursor MCP for `unreal-mcp`.

Health check: `GET http://127.0.0.1:8000/mcp` returns **405** when the server is alive.

## Rules

- Tool Search is on: native tools are `list_toolsets`, `describe_toolset`, `call_tool`.
- Serial game-thread calls only. No parallel floods.
- Read-only until Alexandre asks for a write toolset.
- Use Epic `EditorToolset` / `StateTreeToolset` for editor ops that already exist.
---
