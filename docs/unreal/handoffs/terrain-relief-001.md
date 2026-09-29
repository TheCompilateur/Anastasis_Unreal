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
  conservative, l'eau figée). Exagération laissée à 3.6 : mesuré, la baisser n'apporte
  rien à l'intérieur des terres et coûte du relief.

## FILES_OWNED

- Source/Anastasis_UnrealV2/WorldView/AnastasisTerrainForge.cpp
- Source/Anastasis_UnrealV2/WorldView/AnastasisTerrainForge.h
- Source/Anastasis_UnrealV2/WorldView/AnastasisTerrainForgeTests.cpp
- Source/Anastasis_UnrealV2/WorldView/AnastasisWorldEmbodiment.cpp (marge du halo seulement)
- Source/Anastasis_UnrealV2/WorldView/AnastasisTerrainSurface.h (commentaire de ShoreDepthSpan seulement)
- docs/unreal/GROUND_SURFACE_001.md (note de couplage seulement)
- tools/unreal/capture-terrain-relief.ps1
- tools/unreal/terrain-relief-capture.py
- docs/visual/terrain-relief-001/

## COMMIT

Voir `git log agent/terrain-relief-001`.

## MEC

- BUILD: PASS (`anastasis-unreal.ps1 build`, worktree)
- TESTS: `report-tests.ps1` — PASS 83, KNOWN_EXPECTED_FAILURE 4, FAIL 0, total 87
- `Anastasis.Terrain.Forge.NoStaircase` (étape 1, Bicubic 0 / Sharpen 1 / Talus 0 épinglés) :
  rugosité 22.29 → 18.98 uu, >60° 20.83 % → 20.53 %, p99 81.6° → 81.4°
- `Anastasis.Terrain.Forge.NoSpikes` (étape 2, Talus 0 épinglé) :
  lames 651 → 44, pli de grille 2.97 → 1.39, rugosité 18.98 → 9.86 uu,
  >60° 20.53 % → 18.36 %, p99 81.4° → 80.3°
- `Anastasis.Terrain.Forge.NoCliffs` (étape 3) :
  >60° intérieur (> 1 tuile de l'eau) 14.24 % → 0.00 %, >60° total 18.36 % → 3.13 %,
  >45° 32.3 % → 3.6 %, lames 44 → 10, point haut 1353 → 1156 uu,
  masse conservée (Δ = 1e-6 uu), déplacement max 770 uu, forge 1.1 s
- `Anastasis.Terrain.Forge.ChunkSeam` : erreur de Laplacien au bord, halo 2 tuiles = 0
- COMMANDS:
  - `tools\unreal\anastasis-unreal.ps1 build`
  - `tools\unreal\report-tests.ps1`
  - `tools\unreal\capture-terrain-relief.ps1 -Step 1|2|3`

## SCN

Captures avant/après : `docs/visual/terrain-relief-001/` (étape 1), `.../step2/`,
`.../step3/`, README avec lecture.

## PLY

Non vérifié en PIE ; captures éditeur seulement.

## INTEGRATION_RISK

- CVars nouvelles : `anastasis.Terrain.Forge.Terraces` (0), `.Escarpments` (0),
  `.Bicubic` (1), `.Sharpen` (0), `.TalusDeg` (40), `.ErosionIterations` (300).
  Le rendu par défaut change.
- **Coût** : l'érosion fait passer `Apply` de ~85 ms à ~1.1 s (monde 96×96, subdiv 4).
  Une liste active a été essayée : même résultat bit à bit, mesurée deux fois plus lente.
- **Matériau de sol** : `SlopeRockStart/End` (0.62 / 0.82) sont calibrés sur des pentes
  rendues de 68–80°. Après érosion, l'intérieur est sous ~40° : la roche de pente ne
  s'affiche plus que sur les berges. Non recalibré (hors mandat), noté dans
  GROUND_SURFACE_001.md.
- **Chunks** : l'érosion est non locale. Un chunk érodé seul ne raccorde pas en hauteur
  au monde entier érodé (le Laplacien, lui, reste exact). Sans effet aujourd'hui : le
  mode 2 incarne le monde en un seul morceau. À traiter si le monde est découpé.
- **Contrat de halo** : `AnastasisTerrainForge::HaloTiles = 2` (était 1 en dur dans
  l'incarnation). Tout appelant qui bâtit un HaloCrop doit utiliser cette constante.
- Bicubic 0 reproduit la forge précédente bit à bit (même formule que BilinearSample).
- **Le bassin habitable s'est déplacé** à l'étape 2 : (2100,6350) → (6650,3550). Z du
  bassin après érosion 474 uu.
- `ShoreDepthSpan` (60 uu) inchangé ; commentaire re-mesuré (p70 59.0, p80 68.9,
  max 614 → 154.7 uu). `Anastasis.Terrain.Shoreline` vert.
- `AnastasisTerrainForge.cpp` est chaud (`terrain-forge-chunk-seam-halo` vient d'y passer).

## STOP

Ne revendique pas un relief corrigé partout : 3 % de la terre reste au-delà de 60°, et
tout ce reste est à moins d'une tuile de l'eau (berges). Les lacs gardent bords droits
et parois : masque d'eau à la tuile la plus proche, exagération différente de part et
d'autre de la rive, sommets d'eau hors érosion. C'est l'étape 4, non commencée.
