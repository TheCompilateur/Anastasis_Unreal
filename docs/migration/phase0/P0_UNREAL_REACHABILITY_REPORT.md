# P0-D Unreal reachability baseline

No gameplay added. No compile launched this session (would not be isolated if editor is open on Final).

Legend: [MEC] mechanism on disk. [SCN] observed inside an Anastasis Unreal project. [PLY] withheld.

## Engine / toolchain

| Item | Result | Class |
|---|---|---|
| Launcher UE 5.8.2 | `C:\Program Files\Epic Games\UE_5.8` Build.version 5.8.2 CL 56702186 compatible 55116800 | [MEC] |
| UBT | `Engine\Build\BatchFiles\Build.bat` readable | [MEC] |
| UHT | `UnrealHeaderTool.exe` exists (binary; not executed) | [MEC] |
| UnrealEditor-Cmd | `Engine\Binaries\Win64\UnrealEditor-Cmd.exe` exists (binary; not executed) | [MEC] |
| MSVC / toolchain | not probed | UNKNOWN |
| V2 EngineAssociation GUID | `{2C7D17E4-42D1-2935-EB7E-098AE6DEDC25}` path unresolved | UNKNOWN |

## Project opens

| Project | Evidence | Class |
|---|---|---|
| Final_AnastasisUR | Log 2026-09-10 23:04: engine 5.8.2, `Running engine for game: Final_AnastasisUR`, map load `Lvl_ThirdPerson` | [SCN] |
| Anastasis_UnrealV2 | no readable Saved/Logs or Binaries this session (permission denied / ignore) | UNKNOWN |
| Anastasis_Unreal 5.7 | not opened this session | UNKNOWN |

## Targets

| Target | Declared | Built this session | Class |
|---|---|---|---|
| Final Editor | `Final_AnastasisUREditor` (inferred from modules `UnrealEditor-Final_AnastasisUR.dll`) | prior/today, not by this agent | [SCN] editor binary present |
| Final Game | `Final_AnastasisUR.Target.cs` Type=Game | not observed | UNKNOWN |
| V2 Editor | `Anastasis_UnrealV2Editor.Target.cs` ExtraModuleNames Sim+Game | not observed | [MEC] declared |
| V2 Game | `Anastasis_UnrealV2.Target.cs` | not observed | [MEC] declared |

## Default map / runtime

- Final: EditorStartupMap / GameDefaultMap `/Game/ThirdPerson/Lvl_ThirdPerson`. [SCN] loaded. PIE created `UEDPIE_0_Lvl_ThirdPerson` 23:14:43, torn down 23:14:59. That is Epic Third Person, not Anastasis sim. **Not [PLY].**
- V2: ini says `/Game/FirstPerson/Lvl_FirstPerson`. Reachability UNKNOWN (map file not opened; uasset binary).

## Logging

- Final: `Saved/Logs/Final_AnastasisUR.log` readable. Contains UBT validate, map load, PIE, plus many unnamed `LogAutomationTest: Error: Condition failed` at 03:04:42.
- V2: log path expected `Saved/Logs/Anastasis_UnrealV2.log` — Read permission denied.

## MCP / editor agent loop

| Piece | Result | Class |
|---|---|---|
| V2 `.cursor/mcp.json` url `http://127.0.0.1:8000/mcp` | file exists | [MEC] |
| Plugins MCP + EditorToolset + Python on V2 uproject | declared Enabled | [MEC] |
| `AnastasisInspectTools` Python | `Content/Python/anastasis_toolset/toolsets/inspect.py` (snapshot, list actors, selection) | [MEC] |
| Cursor dynamic tools `unreal-mcp` | namespace absent this session | not [SCN] |
| GET localhost:8000 | fetch tool cannot hit 127.0.0.1 | UNKNOWN live |
| Final MCP plugins | not in Final `.uproject` | absent |

SOURCE CHANGE → BUILD → LAUNCH → OBSERVE → VERIFY for **V2 AnastasisSim**: not closed this session.

Closed historically for **Final template only**: launch editor + load map + PIE. Verify = log lines, not player-representative Anastasis.

## Automation (declared, not run)

PORTAGE.md (V2):

```
Build.bat Anastasis_UnrealV2Editor Win64 Development -Project=...Anastasis_UnrealV2.uproject
UnrealEditor-Cmd.exe ... -ExecCmds="Automation RunTests Anastasis.Sim.Parite; Quit" -unattended -nullrhi
```

Parity tests compiled in source: `Anastasis.Sim.Parite.{Rng,RngHelpers,Hash,Math,SemantiqueJs,Horloge,GrilleSpatiale}`. Pass/fail: UNKNOWN.

Python: `AI.Toolsets.AnastasisInspect` runner registered in `init_unreal.py` if ToolsetRegistry up. Not executed.

JS headless (not Unreal): `node tools/test.mjs quick` / `node tools/dev/headless.mjs` declared in package.json. Not run.

## Loop classification

```
SOURCE CHANGE  [MEC] C++/Python on disk
BUILD          [MEC] UBT bat exists; [SCN] only Final editor historically
LAUNCH         [SCN] Final editor+PIE today; V2 UNKNOWN
OBSERVE        [SCN] Final log; V2 UNKNOWN; MCP UNKNOWN
VERIFY         parity tests [MEC] source; [SCN] execution UNKNOWN
```

## Blockers for a closed V2 loop

1. GUID engine path unknown — V2 may not open until association is proven.
2. MCP not attached to this Cursor session.
3. This agent cannot read V2 Binaries/Saved (workspace ignore).
4. Final editor may hold Live Coding / mutex if still open — a V2 compile was not attempted.
