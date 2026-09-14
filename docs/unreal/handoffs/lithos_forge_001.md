# HANDOFF: lithos_forge_001

## MISSION

Première grammaire géologique modulaire : transformer les ruptures de relief existantes en parois, strates, corniches, affleurements, fractures, éboulis et crêtes qui appartiennent au terrain.

## FILES_OWNED

- Source/Anastasis_UnrealV2/WorldView/AnastasisGeologicalDressing.h
- Source/Anastasis_UnrealV2/WorldView/AnastasisGeologicalDressing.cpp
- Source/Anastasis_UnrealV2/WorldView/AnastasisGeologicalDressingTests.cpp
- Source/Anastasis_UnrealV2/WorldView/AnastasisWorldEmbodiment.h
- Source/Anastasis_UnrealV2/WorldView/AnastasisWorldEmbodiment.cpp
- Source/Anastasis_UnrealV2/WorldView/AnastasisTerrainSurfaceTests.cpp
- tools/unreal/create_lithos_asset.py
- tools/unreal/create-lithos-asset.ps1
- tools/unreal/capture-lithos-views.py
- tools/unreal/capture-lithos-views.ps1
- Content/Anastasis/Lithos/SM_Lithos_VerticalWall_01.uasset
- Content/Anastasis/Lithos/SM_Lithos_InclinedWall_01.uasset
- Content/Anastasis/Lithos/SM_Lithos_Stratum_01.uasset
- Content/Anastasis/Lithos/SM_Lithos_Cornice_01.uasset
- Content/Anastasis/Lithos/SM_Lithos_Outcrop_01.uasset
- Content/Anastasis/Lithos/SM_Lithos_Fractured_01.uasset
- Content/Anastasis/Lithos/SM_Lithos_DetachedBlock_01.uasset
- Content/Anastasis/Lithos/SM_Lithos_TalusCluster_01.uasset
- Content/Anastasis/Lithos/SM_Lithos_Transition_01.uasset
- Content/Anastasis/Lithos/SM_Lithos_Summit_01.uasset
- Content/Anastasis/Materials/M_AnastasisLithos.uasset
- docs/unreal/handoffs/lithos_forge_001.md

## COMMIT

BRANCH_HEAD

## MEC

- BUILD: PASS (`tools\unreal\anastasis-unreal.ps1 build` → `BUILD::PASS`, worktree `lithos_forge_001`)
- MESHES: PASS (`tools\unreal\create-lithos-asset.ps1` → `LITHOS_MESH::PASS meshes=10`)
- TESTS: PASS 3 / 0 known / 0 fail
  - `Anastasis.Lithos.DeterminismAndCausality` PASS — seed=12345 instances=735 cliff=217 slope=123 talus=393 summit=2 families=10, snapshot non muté
  - `Anastasis.Lithos.CliffFootAndFlat` PASS — step=66 cliff=28 talus=29 on_break=66 high_interior=0 wallish=21 talusish=29
  - `Anastasis.Lithos.RejectInvalidInput` PASS
- COMMANDS:
  - `tools\unreal\anastasis-unreal.ps1 build`
  - `tools\unreal\create-lithos-asset.ps1`
  - `tools\unreal\report-tests.ps1 -Filter Anastasis.Lithos`
  - `tools\unreal\capture-lithos-views.ps1` (éditeur crashé, voir PLY)

## SCN

PASS — embodiment éditeur, carte `Lvl_AnastasisSlice`, seed 12345, `anastasis.Terrain.Forge 1` :

```
ANASTASIS_LITHOS placed=714 plan=735 cliff=217 slope=123 talus=393 summit=2 missing_mesh=0
refused_water=1968 refused_flat=3894 refused_spacing=791
wall=36 inclined=28 stratum=76 cornice=18 outcrop=102 fractured=53
detached=160 talus_mesh=122 transition=118 peak=1
```

Les 10 meshes sont chargés. 21 candidats du plan hors emprise échantillonnée (714/735). CVar `anastasis.Dressing.Lithos` (défaut 1).

## PLY

UNKNOWN — `capture-lithos-views.ps1` a ouvert l'éditeur, incarné le monde (ligne SCN ci-dessus), puis crashé dans `AssetRegistry` pendant le démarrage (`FPlatformMisc::RequestExit` status 3). Aucun PNG écrit. Cause probable : plusieurs UnrealEditor concurrents (ecotone-forge, hydra-forge, shoreline, tree-visuals) + mémoire physique saturée (~16 Go). Relancer la capture sur une machine quiescente.

## INTEGRATION_RISK

- `WorldEmbodiment.{h,cpp}` est un fichier chaud. Le crochet Lithos est isolé derrière `LithosDressing` + CVar `anastasis.Dressing.Lithos`.
- `AnastasisTerrainSurfaceTests.cpp` : `LithosDressing.bEnabled = false` dans `DressingOnGround`, même motif que Forest, pour ne pas casser l'identité `0.5 * EngineBasicShapeSize * Scale`.
- `DA_AnastasisPresentation.uasset` et `PresentationRegistry.cpp` **non touchés** — évite le conflit binaire LFS avec `claude/anastasis-rock-grammar-e67075`.
- `Content/Anastasis/Rock/` non touché. Les meshes vivent dans `Content/Anastasis/Lithos/`.
- Placement morphologique (pente / pied / sommet), pas 1 rocher par tuile Stone. Compatible avec une entrée Stone ultérieure.
- Premier `create_lithos_asset` a loggé `ForceDeleteObject failed` en réécriture ; le run PASS a tout de même sauvé les 10 bounds valides (jupe sous Z=-50).

## STOP

- Ne revendique pas l'intégration dans `main`.
- Ne revendique pas un nouveau worldgen, ni PLAYER, ni un matériau de Landscape.
- Ne revendique pas la grammaire `Rock` de l'autre agent.
- Ne revendique pas de captures aériennes / humaines tant que l'éditeur de capture n'a pas produit les PNG.
