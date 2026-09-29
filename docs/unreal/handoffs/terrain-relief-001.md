# HANDOFF: terrain-relief-001

## MISSION

Étape 1 de la correction du relief rendu (diagnostic validé par Alexandre) : couper les
terrasses (versants + couronne du bassin) et les escarpements de TERRAIN_FORGE, qui
jugeaient la pente avant l'exagération verticale et fabriquaient escalier et parois.
Présentation seulement : AnastasisSim (contrat de parité JS) n'est pas touché.

## FILES_OWNED

- Source/Anastasis_UnrealV2/WorldView/AnastasisTerrainForge.cpp
- Source/Anastasis_UnrealV2/WorldView/AnastasisTerrainForgeTests.cpp
- tools/unreal/capture-terrain-relief.ps1
- tools/unreal/terrain-relief-capture.py
- docs/visual/terrain-relief-001/

## COMMIT

Voir `git log agent/terrain-relief-001`.

## MEC

- BUILD: PASS (`anastasis-unreal.ps1 build`, worktree)
- TESTS: `report-tests.ps1` — PASS 81, KNOWN_EXPECTED_FAILURE 4, FAIL 0, total 85
- Nouveau test : `Anastasis.Terrain.Forge.NoStaircase`
  - rugosité 22.29 → 18.98 uu, >60° 20.83 % → 20.53 %, p99 81.6° → 81.4°
- COMMANDS:
  - `tools\unreal\anastasis-unreal.ps1 build`
  - `tools\unreal\report-tests.ps1`
  - `tools\unreal\capture-terrain-relief.ps1`

## SCN

Captures avant/après : `docs/visual/terrain-relief-001/` (README avec lecture).

## PLY

Non vérifié en PIE ; captures éditeur seulement.

## INTEGRATION_RISK

- Deux CVars nouvelles, défaut 0 : `anastasis.Terrain.Forge.Terraces`,
  `anastasis.Terrain.Forge.Escarpments`. Le rendu par défaut change (plus de terrasses).
- `AnastasisTerrainForge.cpp` est chaud (`terrain-forge-chunk-seam-halo` vient d'y passer).
  Le Laplacien et le halo ne sont pas modifiés : `ChunkSeam` reste vert.
- Bassin et point haut : leur recherche ne dépend pas des passes coupées, positions
  inchangées dans la capture (basin=(2100,6350,328)).

## STOP

Ne revendique pas un relief corrigé : un sommet de terre sur cinq reste rendu au-delà de
60°, et les lames près de l'eau persistent (amplification du Laplacien, exagération ×3.6).
Étapes 2 (bicubique, suppression de l'amplification) et 3 (exagération, lissage) non
commencées, en attente de validation.
