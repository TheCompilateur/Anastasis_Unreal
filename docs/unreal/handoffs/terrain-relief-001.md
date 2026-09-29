# HANDOFF: terrain-relief-001

## MISSION

Correction par étapes du relief rendu par TERRAIN_FORGE (diagnostic validé par
Alexandre). Présentation seulement : AnastasisSim (contrat de parité JS) n'est pas touché.

- Étape 1 : couper terrasses (versants + couronne du bassin) et escarpements, qui
  jugeaient la pente avant l'exagération verticale et fabriquaient escalier et parois.
- Étape 2 : altitude fine bicubique (Catmull-Rom borné) au lieu du bilinéaire plié sur
  chaque ligne de tuile ; couper l'amplification du Laplacien et l'affûtage du point haut,
  qui levaient une frange de lames au bord des chenaux.
- Étape 3 : érosion thermique sur la hauteur RENDUE (talus 40°, 300 itérations,
  conservative, l'eau figée). Exagération laissée à 3.6.
- Étape 4 (rive dérivée du relief) : mesurée, **abandonnée** sur décision d'Alexandre —
  à 1 m/tuile, adoucir les berges exigeait de raboter les collines ou de combler les
  rivières. Rien n'en est commité.
- **Échelle** : `AnastasisWorldView::TileWorldSize` 100 → 400 uu (4 m), choisi par
  Alexandre. 100 était l'échelle de diagnostic P1.5, gelée sans être choisie.

## FILES_OWNED

- Source/Anastasis_UnrealV2/WorldView/AnastasisTerrainForge.cpp
- Source/Anastasis_UnrealV2/WorldView/AnastasisTerrainForge.h
- Source/Anastasis_UnrealV2/WorldView/AnastasisTerrainForgeTests.cpp
- Source/Anastasis_UnrealV2/WorldView/AnastasisWorldView.h (TileWorldSize)
- Source/Anastasis_UnrealV2/WorldView/AnastasisTerrainSurfaceTests.cpp (coordonnées en tuiles)
- Source/Anastasis_UnrealV2/WorldView/AnastasisEcologicalDressingTests.cpp (échelle hors du test)
- Source/Anastasis_UnrealV2/WorldView/AnastasisWorldEmbodiment.cpp (marge du halo seulement)
- Source/Anastasis_UnrealV2/WorldView/AnastasisTerrainSurface.h (commentaire de ShoreDepthSpan seulement)
- docs/unreal/GROUND_SURFACE_001.md (note de couplage seulement)
- docs/unreal/PRESENTATION_ASSET_BINDING.md (section échelles)
- tools/unreal/capture-terrain-relief.ps1
- tools/unreal/terrain-relief-capture.py
- docs/visual/terrain-relief-001/

## COMMIT

Voir `git log agent/terrain-relief-001`.

## MEC

- BUILD: PASS (`anastasis-unreal.ps1 build`, worktree)
- TESTS: `report-tests.ps1` — PASS 84, KNOWN_EXPECTED_FAILURE 4, FAIL 0, total 88
- Chiffres à 4 m/tuile (ceux des étapes 1–3 à 1 m sont dans leurs commits et le README) :
  - `Anastasis.Terrain.Forge.Banks` : berges p50 26.3°, p90 48.5°, > 60° 3.2 %
    (1 m : 68.3° / 84.5° / 72.6 %) ; fosses sèches 303 (1 m : 11)
  - `Anastasis.Terrain.Forge.NoCliffs` : > 60° avant érosion 0.34 %, intérieur 0.25 %
    → 0 % ; > 45° après érosion 1.2 % ; point haut 1353 uu
  - `NoStaircase`, `NoSpikes` : comparaisons relatives, toujours vertes
- COMMANDS:
  - `tools\unreal\anastasis-unreal.ps1 build`
  - `tools\unreal\report-tests.ps1`
  - `tools\unreal\capture-terrain-relief.ps1 -Step 1|2|3|scale`

## SCN

Captures : `docs/visual/terrain-relief-001/` (étape 1), `step2/`, `step3/`, `scale/`
(à comparer à `step3/*_after.png`), README avec lecture.

## PLY

Non vérifié en PIE ; captures éditeur seulement. Le monde fait 384 m au lieu de 96 m.

## INTEGRATION_RISK

- **Échelle — risque majeur, à relire avant intégration.** Tout ce qui est exprimé en
  tuiles suit (terrain, dressing, brume, cubes de débogage). Ce qui est en uu absolus ne
  suit pas :
  - caméras codées en dur dans d'autres outils de capture : `astral-observe.py`,
    `observe-slice.py`, `capture-tree-lineup.py`, `terrain-forge-capture.py` visent
    désormais un coin du monde ;
  - atmosphère : `FogStartDistance` 1500 uu, profils réglés pour un monde de 96 m ;
  - ruines : cylindres de 0.6–1.1 m, par tuile de 4 m ;
  - acteurs posés dans `Lvl_AnastasisSlice` (lumières, volumes) : non inspectés ;
  - branches d'autres agents réglées à 1 m/tuile (ecotone, hydra, forêts…).
- Forêts : l'espacement des troncs, en fraction de tuile, passe de 55 cm à 2.2 m ; le
  nombre d'arbres est inchangé, leur densité au m² est divisée par 16.
- **Bords carrés de l'eau** : défaut devenu dominant à 4 m (fosses sèches 11 → 303).
  Masque d'eau à la tuile la plus proche. Le masque par contour (partie sûre de
  l'étape 4) les ramenait à 0 ; non repris sans accord.
- CVars de la forge : `Terraces` (0), `Escarpments` (0), `Bicubic` (1), `Sharpen` (0),
  `TalusDeg` (40), `ErosionIterations` (300). À 4 m l'érosion n'a presque plus rien à
  faire mais coûte encore ~0.9 s.
- **Matériau de sol** : `SlopeRockStart/End` calibrés sur 68–80° rendus ; plus aucune
  pente n'y arrive. Couleurs de sommet par tuile : taches de 4 m aux bords nets. Motif
  « léopard » visible sur tout versant éclairé.
- **Contrat de halo** : `AnastasisTerrainForge::HaloTiles = 2`.
- `ShoreDepthSpan` (60 uu, en Z) inchangé : l'échelle ne touche pas aux profondeurs.
- `AnastasisTerrainForge.cpp` est chaud (`terrain-forge-chunk-seam-halo` vient d'y passer).

## STOP

Ne revendique pas une carte finie : les bords carrés de l'eau, le vide autour de la
carte, la bordure de rochers, l'échelle des ruines, l'atmosphère à 384 m et le
matériau ne sont pas traités.
