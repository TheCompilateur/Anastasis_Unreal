# P1.5 Worldgen embodiment report

Mission: `Ω::ANASTASIS_UNREAL_P1_5_WORLDGEN_EMBODIMENT`. Mode A. Nav not started.

## Owner map (consumed, not mutated)

```
WORLDGEN_OWNER:: AnastasisWorld (AnastasisSim)
WORLD_DATA_TYPE:: AnastasisWorld::FWorld
TILE_TYPE:: AnastasisWorld::FTile
WORLDGEN_ENTRYPOINT:: AnastasisWorld::GenerateWorld(uint32 Seed, int32 W, int32 H)
DIMENSION_SOURCE:: FWorld.W / FWorld.H
COORDINATE_CONVENTION:: index = Y * W + X
ALTITUDE_FIELD:: FTile.Alt
TERRAIN_FIELD:: FTile.Type
SEED_INPUT:: uint32 argument (not stored on FWorld)
PUBLIC_READ_SURFACE:: FWorld.Tiles, TileTypeName
```

## Adapter boundary

`AnastasisSim` still depends only on Core/CoreUObject.  
`AnastasisWorldView` + `AAnastasisWorldEmbodiment` live in `Anastasis_UnrealV2`.  
`GenerateWorld` is called, never reimplemented.

## Coordinate contract

```
UE_X = (SIM_TILE_X + 0.5) * 100
UE_Y = (SIM_TILE_Y + 0.5) * 100
UE_Z = SIM_ALTITUDE * 1000
TILE_WORLD_SIZE = 100 UU
ALTITUDE_SCALE = 1000
ORIGIN_POLICY = tile-center; tile (0,0) at (50, 50, alt*1000)
AXIS_MAPPING = SimX→UEX, SimY→UEY, Alt→UEZ
```

Technique: 7 `UHierarchicalInstancedStaticMeshComponent` (one per `ETileType`), Engine cube `/Engine/BasicShapes/Cube.Cube`. Not Landscape. Not 9216 AActors.

## Build / test / runtime

Build (Final editor was holding Live Coding; V2 compiled with `-NoHotReloadFromIDE`):

```
Build.bat Anastasis_UnrealV2Editor Win64 Development -Project=...\Anastasis_UnrealV2.uproject -WaitMutex -NoHotReloadFromIDE
```

Automation (`UnrealEditor-Cmd -nullrhi`, engine 5.8.2-56702186, `Running engine for game: Anastasis_UnrealV2`):

```
Anastasis.Sim.Parite.Monde          Success
Anastasis.WorldView.Coordinates     Success
Anastasis.WorldView.InstanceCount   Success
Anastasis.WorldView.SampleMapping   Success
Anastasis.WorldView.Bounds          Success
Anastasis.WorldView.EmbodimentSpawn Success
**** TEST COMPLETE. EXIT CODE: 0 ****
```

Game path (`-game` `/Game/FirstPerson/Lvl_FirstPerson`): GameMode `BeginPlay` spawned the actor; same log envelope.

## Runtime proof (seed 12345, 96×96)

```
ANASTASIS_WORLDVIEW seed=12345 w=96 h=96 tiles=9216 instances=9216
minAlt=0.184 maxAlt=0.907
counts=3249,1198,1396,462,716,725,1470
```

Terrain counts match `WorldMapVectors` 12345/96×96.

| i | x,y | type | alt | expected UE | HISMC instance |
| --- | --- | --- | --- | --- | --- |
| 0 | 0,0 | 1 water | 0.187 | (50, 50, 187) | identical |
| 97 | 1,1 | 1 | 0.194 | (150, 150, 194) | identical |
| 675 | 3,7 | 6 field | 0.580 | (350, 750, 580) | identical |
| 1930 | 10,20 | 0 grass | 0.554 | (1050, 2050, 554) | identical |
| 1455 | 15,15 | 6 | 0.454 | (1550, 1550, 454) | identical |
| 3007 | 31,31 | 0 | 0.390 | (3150, 3150, 390) | identical |
| 414 | 30,4 | 2 stone | 0.621 | (3050, 450, 621) | identical |
| 2792 | 8,29 | 0 | 0.596 | (850, 2950, 596) | identical |
| 9215 | 95,95 | 2 | 0.772 | (9550, 9550, 772) | identical |

Z = alt × 1000 on every sample.

## MCP

`GET http://127.0.0.1:8000/mcp` connection failed. No `list_toolsets`.  
`HTTP_ENDPOINT_REACHABLE::NO` therefore `MCP_SESSION::NOT_EXECUTED`.  
Game `-game` also logged `unreal` missing `ToolsetDefinition` (editor-only). Not used as scene evidence.

## Scars / regressions / unknowns

- SCAR::P1_NUMERIC_FBM_001 and DEBT::JS_SEMANTICS_001 not re-run this session; sim worldgen files not edited.
- Final_AnastasisUR editor still open (pid observed). Worked around with `-NoHotReloadFromIDE`.
- Viewport pixels UNKNOWN (`-nullrhi`). Machine logs + HISMC transforms are the evidence.
- BasicShapeMaterial `Color` parameter application not independently verified (types are distinguishable by 7 named HISMC + counts).

## Files

Created: `WorldView/AnastasisWorldView.h/.cpp`, `AnastasisWorldEmbodiment.h/.cpp`, `AnastasisWorldViewTests.cpp`, this report, `P1_5_HANDOFF.md`.  
Modified: `Anastasis_UnrealV2GameMode.h/.cpp`.  
Not modified: `AnastasisSim` worldgen, JS `src/sim`, `src/render3d`, nav.
