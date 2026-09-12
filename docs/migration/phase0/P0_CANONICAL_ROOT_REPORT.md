# P0-B Canonical Unreal identity

Date: 2026-09-10. Mode: forensic / read-mostly. No project deleted, moved, or renamed.

Epistemic: OBS = file/log/read. INF = comparison. CLM withheld unless evidence_scope covers it.

## PREEXISTING

- JS repo `C:\dev\Jeux IV Kingdoms`: git HEAD branch `codex/p0-temporal-hud` @ `fee66ae8`. `main` @ `f461eda1`. Last COMMIT_EDITMSG on this branch is a `render3d` rig refactor. Uncommitted dirty status: UNKNOWN (git status not obtained this session).
- Unreal V2 / Final: no `.git` (`/.git/HEAD` missing). Not versioned here.
- Third Unreal root OBSERVED, not in the two-name brief: `Anastasis_Unreal` (UE 5.7). Not archived.

## Identity table

| Field | Anastasis_UnrealV2 | Final_AnastasisUR | Anastasis_Unreal (extra) |
|---|---|---|---|
| Path | `...\Unreal Projects\Anastasis_UnrealV2` | `...\Unreal Projects\Final_AnastasisUR` | `...\Unreal Projects\Anastasis_Unreal` |
| `.uproject` EngineAssociation | GUID `{2C7D17E4-42D1-2935-EB7E-098AE6DEDC25}` | `"5.8"` | `"5.7"` |
| Modules | `AnastasisSim` (PreDefault) + `Anastasis_UnrealV2` | `Final_AnastasisUR` only | `AnastasisCore` + `Anastasis_Unreal` |
| Sim C++ | `AnastasisSim`: Core+CoreUObject; rng/math/clock/spatial; parity tests | none | `AnastasisCore`: Core+CoreUObject+Json; Wonderland-probe contract (header), not JS `src/sim` bit-parity |
| Gameplay C++ | FirstPerson + Variant_Shooter + Variant_Horror (Epic template) | ThirdPerson + Variant_Combat + Variant_Platforming + Variant_SideScrolling (Epic template) | FirstPerson + Shooter/Horror (prior session) |
| Default map | `/Game/FirstPerson/Lvl_FirstPerson` | `/Game/ThirdPerson/Lvl_ThirdPerson` | UNKNOWN this session |
| GameMode (ini) | `BP_FirstPersonGameMode` | `BP_ThirdPersonGameMode` | UNKNOWN |
| Plugins unique | Python, EditorScripting, ModelContextProtocol, EditorToolset, StateTreeToolset, AutomationTestToolset | none of those | Python, EditorScripting, RemoteControl (no MCP) |
| Agent Python | `Content/Python/init_unreal.py` + `AnastasisInspectTools` | no `Content/**/*.py` found | UNKNOWN |
| Git | none | none | UNKNOWN |
| Build artifacts | Binaries/Logs: permission denied this session (`.cursorignore`) | `Binaries/Win64/UnrealEditor.modules` BuildId `55116800`; log 2026-09-10 23:04 | UNKNOWN |

## Engine facts (machine)

- OBS: Epic launcher install `C:\Program Files\Epic Games\UE_5.8` = `5.8.2-56702186`, CompatibleChangelist `55116800`.
- OBS: Final log loads that engine, UBT `Build.bat`, map `Lvl_ThirdPerson`, PIE ~23:14:43 then teardown ~23:14:59.
- OBS: V2 association is a GUID, not `"5.8"`. Disk path of that GUID: UNKNOWN. Whether V2 currently opens in launcher 5.8.2: UNKNOWN.
- HYP (not claimed): GUID = source-built 5.8.2 used when MCP plugins were enabled.

## CANONICAL_CANDIDATE

`Anastasis_UnrealV2` — only root that contains JS-sim C++ (`AnastasisSim` + `Anastasis.Sim.Parite.*` + `tools/unreal/gen-parity-vectors.mjs` output path) and Unreal 5.8 MCP toolsets.

This is a candidate, not a proven unique product root.

## UNIQUE_CONTENT_V2

- `Source/AnastasisSim/**` (JsNumeric, Rng, SimMath, SimClock, SpatialGrid, parity `.inl` + tests).
- `Content/Python/anastasis_toolset/**`.
- MCP / EditorToolset / StateTreeToolset / AutomationTestToolset in `.uproject`.
- `.cursor/mcp.json` → `http://127.0.0.1:8000/mcp`.
- `PORTAGE.md`, skills `unreal-mcp-session`.
- Template variants Shooter/Horror + FirstPerson map.

## UNIQUE_CONTENT_FINAL

- Template variants Combat / Platforming / SideScrolling.
- ThirdPerson map + PIE evidence today.
- Launcher association `"5.8"` (reachable without resolving a GUID).
- Editor module DLL name `UnrealEditor-Final_AnastasisUR.dll`.

## SHARED_CONTENT

- UE 5.8 Target `BuildSettingsVersion.V7` / `EngineIncludeOrderVersion.Unreal5_8`.
- Plugins: ModelingToolsEditorMode, StateTree, GameplayStateTree.
- Gameplay deps: Engine, EnhancedInput, AIModule, UMG, Slate.
- Abstract C++ GameMode wrapping an Epic sample; not Anastasis village/sim.
- Renderer ini: Lumen-class settings, DX12, SM6, RayTracing, Substrate (copied template, not unique art).

## BUILD_STATUS

- Final editor: [SCN] compiled and launched today against launcher 5.8.2. Game/packaged target: UNKNOWN. Startup log contains many `LogAutomationTest: Error: Condition failed` — cause UNKNOWN.
- V2 editor/game: UNKNOWN this session (binaries unreadable; no live compile).
- 5.7 project: UNKNOWN this session.

## UNKNOWN

- GUID `{2C7D17E4-...}` install path.
- Whether V2 Content/FirstPerson differs from a stock Epic sample (`.uasset` not inspected).
- Whether Final or V2 holds any unique Anastasis art (binary Content not inventoried).
- Drift between `AnastasisCore` (5.7) and `AnastasisSim` (V2).

## Verdict

```
CANONICAL_UNREAL_ROOT::UNKNOWN
CANONICAL_CANDIDATE::Anastasis_UnrealV2
EXISTS != CANONICAL
```

Stop: do not delete/move Final or 5.7. Sealing the root requires (1) V2 editor actually opens on a known engine, (2) a written decision superseding DEC-001 if Unreal is the 3D authority.
