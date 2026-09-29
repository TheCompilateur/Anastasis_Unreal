# HANDOFF: terrain-relief-001

## MISSION

Correction par étapes du relief rendu par TERRAIN_FORGE (diagnostic validé par
Alexandre). Présentation seulement : AnastasisSim (contrat de parité JS) n'est pas touché.

- Étape 1 : couper terrasses (versants + couronne du bassin) et escarpements, qui
  jugeaient la pente avant l'exagération verticale et fabriquaient escalier et parois.
- Étape 2 : altitude fine bicubique (Catmull-Rom borné) au lieu du bilinéaire plié sur
  chaque ligne de tuile ; couper l'amplification du Laplacien et l'affûtage du point haut,
  qui levaient une frange de lames au bord des chenaux.

## FILES_OWNED

- Source/Anastasis_UnrealV2/WorldView/AnastasisTerrainForge.cpp
- Source/Anastasis_UnrealV2/WorldView/AnastasisTerrainForge.h
- Source/Anastasis_UnrealV2/WorldView/AnastasisTerrainForgeTests.cpp
- Source/Anastasis_UnrealV2/WorldView/AnastasisWorldEmbodiment.cpp (marge du halo seulement)
- Source/Anastasis_UnrealV2/WorldView/AnastasisTerrainSurface.h (commentaire de ShoreDepthSpan seulement)
- tools/unreal/capture-terrain-relief.ps1
- tools/unreal/terrain-relief-capture.py
- docs/visual/terrain-relief-001/

## COMMIT

Voir `git log agent/terrain-relief-001`.

## MEC

- BUILD: PASS (`anastasis-unreal.ps1 build`, worktree)
- TESTS: `report-tests.ps1` — PASS 82, KNOWN_EXPECTED_FAILURE 4, FAIL 0, total 86
- `Anastasis.Terrain.Forge.NoStaircase` (étape 1, Bicubic/Sharpen épinglés à 0/1) :
  rugosité 22.29 → 18.98 uu, >60° 20.83 % → 20.53 %, p99 81.6° → 81.4°
- `Anastasis.Terrain.Forge.NoSpikes` (étape 2) :
  lames 651 → 44, pli de grille 2.97 → 1.39, rugosité 18.98 → 9.86 uu,
  >60° 20.53 % → 18.36 %, p99 81.4° → 80.3°
- `Anastasis.Terrain.Forge.ChunkSeam` : erreur de Laplacien au bord, halo 2 tuiles = 0
- COMMANDS:
  - `tools\unreal\anastasis-unreal.ps1 build`
  - `tools\unreal\report-tests.ps1`
  - `tools\unreal\capture-terrain-relief.ps1 -Step 1|2`

## SCN

Captures avant/après : `docs/visual/terrain-relief-001/` (étape 1) et `.../step2/`,
README avec lecture.

## PLY

Non vérifié en PIE ; captures éditeur seulement.

## INTEGRATION_RISK

- CVars nouvelles : `anastasis.Terrain.Forge.Terraces` (0), `.Escarpments` (0),
  `.Bicubic` (1), `.Sharpen` (0). Le rendu par défaut change.
- **Contrat de halo** : `AnastasisTerrainForge::HaloTiles = 2` (était 1 en dur dans
  l'incarnation). Tout appelant qui bâtit un HaloCrop doit utiliser cette constante.
  Avec un HaloCrop, l'altitude fine de TOUT Crop est lue dans le halo.
- Bicubic 0 reproduit la forge précédente bit à bit (même formule que BilinearSample).
- **Le bassin habitable se déplace** avec le relief par défaut : (2100,6350,328) →
  (6650,3550,493). Tout ce qui lit `GetTerrainForgeBasin` (caméras, futur village)
  voit le nouveau site. Point haut quasi inchangé.
- Le point le plus bas passe de −339 à +93 uu : plus de terre enfoncée sous la nappe.
  `Anastasis.Terrain.Shoreline` reste vert. `ShoreDepthSpan` (60 uu) inchangé ; son
  commentaire est re-mesuré (p70 58.1, p80 68.1, max 614 → 181.7 uu).
- `AnastasisTerrainForge.cpp` est chaud (`terrain-forge-chunk-seam-halo` vient d'y passer).

## STOP

Ne revendique pas un relief corrigé : 18 % de la terre reste rendue au-delà de 60°
(exagération ×3.6 sur des tuiles de 1 m, étape 3), et les lacs gardent bords droits et
parois (masque d'eau à la tuile la plus proche, étape 4). Non commencées.
