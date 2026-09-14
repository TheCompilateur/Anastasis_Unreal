# VILLAGE INTERACTION FOUNDATION-001 — discovery / mission log

GATE 0 completed before feature edits, 2026-09-14 (Toronto).
Base: 1cbeef680c3ce51f21cd2cd273d4771b56af9de3 (claude/anastasis-village-foundation-28ac68).
Worktree clean at start; clean of other agents' work throughout. HEAD did not move
during the mission. No graphics chantier touched: no Landscape, material, mesh,
vegetation, atmosphere, terrain or world-generation file is in the diff.

Scope: the functional infrastructure that lets future generated buildings and NPCs
exist as usable places rather than meshes. No NPC AI, no Mass, no parallel economy,
no village content.

## Discovery

Modules: `AnastasisSim` (portable deterministic contract, JS parity, no Engine dep)
and `Anastasis_UnrealV2` (gameplay + WorldView). Plugins already enabled: StateTree,
GameplayStateTree, ProceduralMeshComponent, plus editor-only toolsets.

SmartObjects: present in UE 5.8 at `Engine/Plugins/Runtime/SmartObjects`, **not**
enabled by default, `EnabledByDefault: false`. Prebuilt for Win64 editor.
StateTree C++ node precedent already in-repo: `Variant_Shooter/AI/ShooterStateTreeUtility.{h,cpp}`
(FStateTreeTaskCommonBase / FStateTreeConditionCommonBase, instance-data pattern).
GameplayTags: not previously used by ANASTASIS code; `Config/DefaultGameplayTags.ini`
does not exist and was deliberately not created.

No pre-existing building abstraction. Existing NPC representation is the template's
`Variant_Shooter/AI/ShooterNPC` + `ShooterAIController` — shooter template, not a
villager; not extended, not depended upon.

Interaction ontology placed in `Anastasis_UnrealV2`, not `AnastasisSim`: the sim module
forbids Engine dependencies and `GameplayTags` pulls Engine. This matches the mission
boundary — the sim decides, Unreal incarnates.

## Architecture

```
SIMULATION
  -> FAnastasisBuildingSpec              (BuildingId, BuildingTag, Transform, N slot specs)
  -> UAnastasisVillageSubsystem::RegisterBuilding
  -> USmartObjectDefinition built in memory, capacities unfolded
  -> USmartObjectSubsystem::CreateSmartObject          (actor-less dynamic path)

AGENT
  -> FindInteractions(ActivityTag, Origin, Radius, UserTags)
  -> ClaimInteraction  (exclusive at equal priority)
  -> BeginInteraction  (Claimed -> Occupied)
  -> ReleaseInteraction
  -> FAnastasisInteractionAddress {BuildingId, SlotId, Occurrence}  -> SIMULATION
```

The reservation truth lives in `USmartObjectSubsystem`; the village subsystem adds only
what the engine cannot carry — the stable simulation-side address of each place. A
`FSmartObjectSlotHandle` is a session identifier and never leaves Unreal.

Capacity is unfolded, not counted: a Smart Object slot holds one occupant, so
`Capacity = N` becomes N engine slots, each with its own pose from `OccurrencePoses`.
`Capacity = 1` is therefore exclusivity by construction. There is no unlimited shared
slot — such a slot could refuse nothing, and a reservation system that refuses nothing
reserves nothing.

Claim priority is fixed at `Normal` everywhere. Preemption is deliberately unused:
conflict arbitration between villagers belongs to the simulation, so two Unreal agents
contending for one bed are equals — first arrival holds, the second is refused and
looks elsewhere.

## Registration is dynamic, with no editor scene

Nothing goes through `USmartObjectComponent`, `ASmartObjectPersistentCollection`, or a
saved `USmartObjectDefinition` asset. Definitions are `NewObject` + `RF_Transient`,
outered to the subsystem (which is why the record keeps a `TObjectPtr` — `FSmartObjectRuntime`
holds only a non-owning pointer and GC has no other reason to keep it alive).
Slots are added through `USmartObjectDefinition::DebugAddSlot()`, the only non-editor-gated
public way to append a slot at runtime in 5.8 (`GetMutableSlots()` is `#if WITH_EDITOR`).

Readiness gate: `USmartObjectSubsystem::CreateSmartObject` ensures on
`bRuntimeInitialized`, which is private. `UWorldSubsystem::HasCalledBeginPlay()` is the
public equivalent — both are set inside the same `UWorld::BeginPlay`. Registering before
BeginPlay returns `SubsystemUnavailable` instead of tripping the engine ensure.

Preconditions (`WorldConditions`) are left empty on purpose, and this is a finding, not
an omission: `USmartObjectSubsystem::TryActivatePreconditionsInternal` returns false when
the runtime has no owner actor, and the dynamic path has none. On actor-less Smart
Objects, world-condition preconditions cannot be activated at all. Slot gating therefore
uses `FGameplayTagQuery` (`RequiredUserTags` -> `MakeQuery_MatchAllTags`), which the
engine evaluates without an actor.

Query radius: the engine's spatial prefilter tests the building ORIGIN against a box
(`FindSmartObjects` -> `QueryBox.IsInside(Runtime->GetTransform().GetLocation())`). The
declared contract is spherical on the PLACE. The box is therefore widened by
`MaxSlotOffsetLength` (largest building-origin-to-slot distance registered so far) to make
the prefilter a strict superset, then the exact spherical test runs on the slot location.
Without the widening, a well rim within reach would be missed because the well's centre
is outside the box.

Query ordering is a total order: distance, then BuildingId, SlotId, Occurrence. The
engine hash grid has no guaranteed iteration order, and the same seed must pick the same bed.

## Interactions implemented

Native gameplay tags (no `.ini`, no DataTable — the village is generated, tags must exist
before any asset loads, and a multi-agent repo does not need three missions fighting over
`Config/DefaultGameplayTags.ini`):

```
Anastasis.Activity           .Sleep .Eat .Drink .Work .Socialize
Anastasis.Activity.Storage   .Deposit .Take
Anastasis.Building           .House .Farm .Tavern .Well .Workshop
```

Activity routes intention; Building is debug/presentation only and routes nothing — asking
"where can I sleep" must find a tavern bench as readily as a house bed. Querying a parent
tag finds children (`Activity.Storage` returns both Take and Deposit) because the engine
matches through `FGameplayTagContainer::HasTag`, which consults parent tags.

Three archetypes, each exposing MORE THAN ONE activity, on distinct slots:

| Archetype | Slots |
|---|---|
| House    | `bed` × BedCount — Sleep |
| Well     | `rim` × RimCount — Drink ; `draw` × 1 — Storage.Take |
| Workshop | `station` × StationCount — Work ; `store` × 1 — Storage.Deposit |

Fetching water is `Activity.Storage.Take`, not a new `Activity.FetchWater`: a well's water
is a world reserve, and a dedicated tag would have doubled the ontology for one case.
No archetype declares a duration, a yield, a trade or a craft — those are simulation decisions.

## StateTree bridge

`FAnastasisHasInteractionIntentCondition`, `FAnastasisFindInteractionTask`,
`FAnastasisUseInteractionTask`, `FAnastasisReleaseInteractionTask`. Each is a thin adapter
over `UAnastasisVillagerInteractionComponent`; none contains logic. The tree ORCHESTRATES,
it does not decide. `SetIntent` is a letterbox the simulation writes to; the condition only
READS it. If a task ever chooses its own ActivityTag, the boundary has been crossed.

Reservation ownership is the component's, not a task's instance data: a claim crosses
several states (find, walk, use, release) and task instance data does not survive a state
change. A claim dropped between states is a place lost for the rest of the session, so
`EndPlay` releases.

Movement stays out. The component publishes the pose to reach and answers `HasArrived()`;
getting there is a movement task's job. A MoveTo here would force a NavMesh, therefore a
level, therefore a scene — and the foundation would stop being testable empty.

## Files

Created, all under `Source/Anastasis_UnrealV2/Village/`:

```
AnastasisVillageTags.h/.cpp                    ontology (native tags)
AnastasisVillageInteraction.h                  data contract + UAnastasisVillageBehavior
AnastasisVillageSubsystem.h/.cpp               registry, query, reservation
AnastasisVillageArchetypes.h/.cpp              house / well / workshop
AnastasisVillagerInteractionComponent.h/.cpp   agent-side sequence ownership
AnastasisVillageStateTreeTasks.h/.cpp          the StateTree bridge
AnastasisVillageCommands.cpp                   Anastasis.Village.* console commands
AnastasisVillageTests.cpp                      the proof
```

Modified, two files, four lines total:

```
Anastasis_UnrealV2.uproject                    + SmartObjects plugin
Source/Anastasis_UnrealV2/Anastasis_UnrealV2.Build.cs   + GameplayTags, SmartObjectsModule
```

`UAnastasisVillageBehavior : USmartObjectBehaviorDefinition` is structurally required, not
decorative: `MarkSlotAsOccupied` returns nullptr and leaves the slot in `Claimed` when no
behavior definition of the requested class is attached — claim would succeed and use would
fail silently.

## Plugin consequence, stated plainly

Enabling SmartObjects transitively enables, per its `.uplugin`: GameplayAbilities,
TargetingSystem, WorldConditions, PropertyBindingUtils, and its module additionally
depends on MassCore and MassEntity. This project does not implement Mass and adds no Mass
code — but once this branch reaches `main`, every agent's editor loads those plugins.
They ship prebuilt with UE 5.8 (no engine compilation was triggered; only the two project
modules were rebuilt), so the cost is editor load time and memory, not build time.

Side effect, checked and benign: `USmartObjectSubsystem::OnWorldComponentsUpdated` spawns
an `ASmartObjectSubsystemRenderingActor` into every world, including the editor world,
under `UE_ENABLE_DEBUG_DRAWING`. The engine passes no `RF_Transient` in its
`FActorSpawnParameters`, but the flag is not needed there: the class is declared
`UCLASS(MinimalAPI, Transient, NotBlueprintable, NotPlaceable)`, and
`StaticAllocateObject` forces `RF_Transient` onto every non-CDO, non-archetype instance of
a `CLASS_Transient` class (`UObjectGlobals.cpp`: "If class is transient, non-archetype
objects must be transient"). It also overrides `ShouldExport()` to false and clears
`bListedInSceneOutliner`. Saving `Lvl_AnastasisSlice` therefore cannot bake it into the
`.umap`, and the `Anastasis.Level.HoldsNoWorldTruth` invariant needs no extension for it.

## Verification

Build: `Build.bat Anastasis_UnrealV2Editor Win64 Development -NoHotReloadFromIDE`
→ `Result: Succeeded`, exit 0, zero warnings in the new files.

`-NoHotReloadFromIDE` is required, and for a reason worth recording: UBT's Live Coding
guard (`HotReload.CheckForLiveCodingSessionActive`) keys off the target's executable,
which for an installed engine is the SHARED `Engine/Binaries/Win64/UnrealEditor.exe`. Any
agent with an editor open therefore blocks every worktree's build until that flag is
passed. `tools/unreal/anastasis-unreal.ps1` already passes it.

Tests: `tools\unreal\report-tests.ps1 -Filter Anastasis.Village`

```
PASS                  : 4
KNOWN_EXPECTED_FAILURE: 0
FAIL                  : 0
TOTAL                 : 4
```

Run completion independently confirmed against the log: `Found 4 automation tests`,
4 × `Test Completed. Result={Success}`, `Automation Test Queue Empty`,
`TEST COMPLETE. EXIT CODE: 0`, zero `Critical error` / `Assertion failed` / `appError`.
The `Condition failed` lines are the documented startup noise (frame 0, ~19 s before the
first test starts), not an ANASTASIS failure.

| Test | Proves |
|---|---|
| `Anastasis.Village.RegistrationLifecycle` | 3 buildings, 9 slots; query finds slots; handle → stable address round-trip; a building exposing several activities on distinct slots; parent-tag query; destroy → 6 slots and Work unfindable; re-register same id → new handle, 11 slots; six named refusals leave the registry at 3 |
| `Anastasis.Village.ConcurrentReservation` | A claims (Free→Claimed); B on the same slot REJECTED and the slot vanishes from queries; B finds an alternative building; A releases → Free; double release fails; slot reusable; capacity 3 grants exactly 3 claims with 3 distinct occurrences and 3 distinct positions |
| `Anastasis.Village.SpatialQuery` | nearest is nearest; distance ordering; distance measured on the place (1120, not the 1000 of the building centre); radius 5000→2, 2000→1, 100→0; two identical queries return identical ordering; absent activity not found; `RequiredUserTags` hides then reveals a slot; invalid tag / zero / negative radius refused |
| `Anastasis.Village.ExecutionSequence` | Idle with no intent refuses; simulation deposits intent; find+reserve → Reserved; a second agent refused and KEEPS its intent; BeginUse refused before arrival WITHOUT losing the claim; arrival → InUse → engine Occupied; address resolves during occupation; release → Complete, intent consumed; B takes the freed place; building destroyed under its occupant → slot Invalid, release fails, sequence still terminates; abandon returns to Idle, frees the slot, intent survives |

Full-suite regression, `report-tests.ps1 -Filter Anastasis`:

```
PASS                  : 59
KNOWN_EXPECTED_FAILURE: 4
FAIL                  : 0
TOTAL                 : 63
```

63 discovered, 63 completed, 0 asserts. The 4 KNOWN_EXPECTED_FAILURE are exactly the four
in `tools/unreal/known-expected-failures.txt` (Sim.Parite.Fbm, Sim.Parite.SemantiqueJs, and
the two `AI.Toolsets.AnastasisInspect` python cases). No new failure, no regression: 55
pre-existing PASS + 4 new village tests. In particular `Anastasis.Level.HoldsNoWorldTruth`
and `Anastasis.Visual.SingleEmbodiment` still pass with SmartObjects enabled.

One tooling finding was raised and deliberately NOT fixed here (out of mandate):
`tools/unreal/report-tests.ps1`
printed `TESTS::PASS 2/2` on an earlier run where the editor crashed after 2 of 4 tests.
It parses only `Test Completed` lines and never compares against the
`Found N automation tests` count or the launcher exit code.

## Integration state

`main` was merged into this branch at `8e05631` (three commits: `8060b2a`, `3a98b8b`
vegetation, `ea87acc` terrain forge). `main` moved from `3a98b8b` to `ea87acc` between the
divergence check and the merge itself — the multi-agent race AGENTS.md describes. Collision
was re-checked across the full `1cbeef68..ea87acc` range before continuing: no file
overlap, and nothing under `Village/` references `TerrainForge`, `WorldEmbodiment`,
`WorldAtmosphere` or the presentation registry.

Merged tree, what IS proven: build `Result: Succeeded`, exit 0, zero warnings, the whole
`Anastasis_UnrealV2` unity module recompiled and linked — so the village foundation and the
newly merged `AnastasisTerrainForge` compile together, not merely side by side.

Merged tree, what is NOT proven: the automation suite. The run died during editor startup
with `Ran out of memory allocating 4.0 MiB / Le fichier de pagination est insuffisant`
(AvailablePhysical 0.08 GiB, AvailableVirtual 0.10 GiB) — four Unreal processes from other
worktrees were live (canonical editor, tree-visuals and ground-materials capture sessions,
a headless shoreline run) on a 15.9 GiB host. **Zero tests executed.** This is a host
resource condition, not a verdict on the merge, and it was NOT retried: an editor boot here
costs ~2 GiB and could have been the allocation that killed another agent's capture.

The suite re-verification therefore belongs to the integration window, which AGENTS.md
already requires to be quiescent — the same condition that makes the run succeed. Until it
is run, the proof of record for this work is the one taken on `1cbeef68` + the two village
commits, reported above.

Second sighting of the reporter hole: on this run `report-tests.ps1` printed `TESTS::PASS`
with `TOTAL : 0`. Zero tests ran, two fatal errors were in the log, and it still reported
green. See the finding at the end of the Verification section.

## Limitations

- `DebugAddSlot()` is the load-bearing engine call for runtime slot construction. Public
  and unguarded in 5.8, but named for testing; an engine upgrade could gate it.
- No StateTree asset. The four nodes compile and register; nothing wires them into a tree.
  Asset-level behavior is unverified, and authoring a `.uasset` has no place in a
  foundation that must hold without a scene.
- No reachability. `HasArrived` is a distance check. A slot inside a wall would be claimed
  and never reached. `USmartObjectSubsystem::FindEntranceLocationForSlot` is the engine
  answer and is unused.
- `MaxSlotOffsetLength` is global and monotonic — one far-flung slot widens every query's
  prefilter box. Costs prefilter work, never falsifies a result.
- No replication. `USmartObjectSubsystem::ShouldCreateSubsystem` returns false on clients.
- Slot Z is whatever the spec says. Nothing grounds a slot onto the procedural terrain.
- Queries are game-thread only; the engine explicitly forbids `Find*` from multiple threads.

## Collision risks with future systems

- `Anastasis_UnrealV2.uproject` Plugins array and `Anastasis_UnrealV2.Build.cs`
  dependency list: the only two files with a textual merge hazard. Four added lines.
- `Config/DefaultGameplayTags.ini` was NOT created. If another mission introduces village
  tags by `.ini` or DataTable, duplicates against the native tags must be reconciled.
- A graphics agent attaching `USmartObjectComponent` to building meshes would create a
  SECOND registration path. Such objects are absent from `Buildings` and are skipped by
  `FindInteractions` (guarded), so they would be invisible to the village rather than
  wrong — but one building would then have two representations.
- A future Mass integration would want async batched queries; the claim path here is
  synchronous and game-thread only.
- Village generation: the intersection point with the terrain chantier is slot grounding.
  The generator must sample terrain height when it fills `FAnastasisBuildingSpec::Transform`.

## Recommended next extension

Slot reachability, before anything else. `FindEntranceLocationForSlot` +
`FSmartObjectSlotEntranceAnnotation`, so a query returns a place an agent can actually
stand at and walk to, validated against the NavMesh. Everything else built on this
foundation inherits whatever answer is given there, and a claimed-but-unreachable slot is
worse than no slot: it is a place permanently held by an agent that never arrives.
