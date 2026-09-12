# P0 Handoff — Anastasis Unreal Phase 0

Mission: `Ω::ANASTASIS_UNREAL_PHASE_0` / `CANONICAL_GROUND_TRUTH`. Mode B, this phase only. No Phase 1 started.

## 1. Mission

Build provenance, Unreal identity, simulator owner map, Unreal reachability, migration boundary. Stop.

## 2. Perimetre reel

Read-mostly. Writes: these five files under `Anastasis_UnrealV2/docs/migration/phase0/`.  
JS tree not written (foreign branch `codex/p0-temporal-hud`). Final/5.7 not moved.

## 3. Constat

- Three Unreal roots exist. Only V2 has JS-sim C++ + MCP. Only Final has [SCN] editor+PIE today. Canonical Unreal root not proven.
- JS sim owners recovered enough for a causal skeleton; `simulation.js` / `npc.js` not fully read.
- Three.js is workspace-legacy, still Active in DEC-001, still in CURRENT_STATE, still not in JS `.cursorignore`.

## 4. Preuves

See sibling reports. Key machine facts: launcher UE_5.8.2 at `C:\Program Files\Epic Games\UE_5.8`; Final log 2026-09-10; V2 GUID association; JS HEAD `fee66ae8` on `codex/p0-temporal-hud`.

## 5. Fichiers

Created (this phase):

- `P0_CANONICAL_ROOT_REPORT.md`
- `P0_SIMULATOR_OWNER_MAP.md`
- `P0_UNREAL_REACHABILITY_REPORT.md`
- `P0_MIGRATION_BOUNDARY.md`
- `P0_HANDOFF.md`

Prior session (not this phase): `Anastasis.code-workspace`, `.cursor/rules/js-sim-reference.mdc`.

## 6. Tests

N/A — none executed.

## 7. Risks

- Opening V2 may fail until GUID engine is identified.
- Compiling V2 while Final editor is open may mutex.
- JS branch is not `main`; sim drift vs documented CURRENT_STATE possible.
- Dual NPC brains.

## 8. Next authorized step (do not execute here)

Prove V2 reachability: open `Anastasis_UnrealV2.uproject` on a known 5.8.2 engine, confirm MCP GET `127.0.0.1:8000/mcp` returns 405, optionally run `Anastasis.Sim.Parite` — still no worldgen port. Do not delete Final.

`docs/ai/ACTIVE_MISSION.md` not updated: would write the JS repo on a non-Unreal branch.
