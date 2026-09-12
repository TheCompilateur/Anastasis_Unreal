# P0 SEAL — author declaration 2026-09-11

Alexandre sealed Phase 0 and authorized Phase 1. This file records the seal as **DEC / author**, not as a new machine measurement by this agent.

```
UNREAL_ROOT::PROVEN
SIMULATOR_BASELINE::PROVEN
EDITOR_BUILD::PASS
EDITOR_RUNTIME::PASS
AGENT_OBSERVABILITY_PATH::PASS
THREEJS::LEGACY_SEALED
PHASE_0::SEALED
PHASE_1::AUTHORIZED
```

Product name: **AnastasisUR**. Canonical Unreal working folder/module: `Anastasis_UnrealV2` (UE 5.8.2).  
Simulator authority: `C:\dev\Jeux IV Kingdoms` `src/sim|life|ai|lang|runtime`.  
`src/render3d/` remains legacy. Do not port. Do not use as Unreal architecture.

This Cursor session still has **no** `unreal-mcp` dynamic tools attached. Author seal of AGENT_OBSERVABILITY_PATH is not collapsed with this agent's tool catalog.

Phase 1 (authorized): worldgen C++ in `AnastasisSim` with bit-parity vs JS `generateWorld`. No template-gameplay rewrite. No Three.js. Do not delete `Final_AnastasisUR`.
