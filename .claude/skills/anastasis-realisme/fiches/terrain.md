# Terrain et relief

## Unreal

- **Landscape** : le terrain standard d'Unreal. C'est un champ de hauteur 2,5D (une altitude par point,
  pas de surplomb), découpé en composants et sections, avec ses LOD, ses couches de peinture, ses
  calques d'édition (Edit Layers) et ses « patches ». L'herbe automatique (Landscape Grass), le carving du
  plugin Water et le RVT de terrain sont construits autour de lui.
- **Mesh Terrain** (5.8, **expérimental**) : un terrain en vrai maillage 3D (surplombs, tunnels), avec
  tessellation variable et Nanite.
- **World Partition** : découpe la carte en cellules chargées selon la distance au joueur, avec un
  fichier par acteur (One File Per Actor). Le **HLOD** remplace les cellules lointaines par des maillages
  fusionnés et simplifiés. Ces outils servent les mondes de plusieurs kilomètres peuplés d'acteurs placés
  à la main.
- Les générateurs externes (Gaea, World Creator, Houdini) produisent une heightmap (16 bits) érodée,
  importée dans un Landscape.
- Ce qui rend un relief crédible tient à des faits de géomorphologie, pas à l'outil : érosion (talus
  d'éboulis à 30–40°), pentes continues sans marche, vallées qui suivent l'écoulement, absence de motif
  de grille.

## ANÁSTASIS aujourd'hui

Le terrain **n'est pas un Landscape**. C'est une projection de la simulation, rebâtie à chaque
`EmbodyCanonical` (`SHORELINE_FORGE_001.md`) :

```
AnastasisWorld::GenerateWorld(seed 12345, 96×96)      tuiles : Type, Alt, Shore, Wetness, FlowAmt
  -> AnastasisWorldView::CaptureSnapshot
  -> AnastasisTerrainSurface::Build                    UProceduralMeshComponent « ExperimentalTerrain »
  -> AnastasisTerrainForge                             subdivision ×4, bicubique, érosion thermique
  -> AnastasisHumanGeography (Human_Geography_V2)      couche réversible : bassins, rivières, chemins
  -> AAnastasisWorldEmbodiment                         + HISM de décor ; anneau « HorizonTerrain » jusqu'à 20 km
```

| Quoi | Valeur |
|---|---|
| Tuile | `TileWorldSize` 400 uu × `anastasis.WorldView.Scale` 5 → monde d'environ 1,9 km de côté |
| Sections du maillage | 0 = sol, 1 = nappe d'eau, 2 = rubans de rivière |
| Forge | `Forge.Subdiv` 4, `Forge.Exaggerate` 3,6, `Forge.Bicubic` 1, `Forge.TalusDeg` 40, `Forge.ErosionIterations` 300, `Forge.Terraces` 0, `Forge.Escarpments` 0, `Forge.Sharpen` 0 |
| Horizon | `anastasis.Terrain.Horizon` 1 : pixels « vides » de la vue d'ensemble 59 % → 1,9 % (`handoffs/horizon-ring-001.md`) |
| Absents | Landscape, World Partition, HLOD, Mesh Terrain, Edit Layers, Landscape Patch |

Le code est dans `Source/Anastasis_UnrealV2/WorldView/` (`AnastasisTerrainSurface*`,
`AnastasisTerrainForge*`, `AnastasisHumanGeography*`, `AnastasisWorldEmbodiment*`). `Source/AnastasisSim/`
est le contrat de parité JS : **on n'y touche pas pour une raison visuelle**.

## Règles

- **TER-01** — Le relief se corrige dans la forge ou dans Human Geography (présentation), jamais dans
  `AnastasisSim` ni par un Landscape posé à côté.
- **TER-02** — Pas de chunking, de World Partition ni de HLOD tant qu'une mesure ne montre pas que le
  maillage unique coûte : « la complexité doit se mériter » (`TERRAIN_SURFACE_EXTENT.md`). Le monde est
  petit (1,9 km), une seule carte, aucun acteur placé à la main.
- **TER-03** — Une pente se juge **après** l'exagération verticale. Les terrasses et escarpements ont été
  coupés parce qu'ils jugeaient la pente avant, et fabriquaient escaliers et parois.
- **TER-04** — Le talus d'érosion reste à 40° (pente d'éboulis réelle). Valeurs mesurées à surveiller :
  berges p50 26,3°, p90 48,5° ; intérieur à plus de 60° = 0 %.
- **TER-05** — Le bord du monde ne doit pas se voir : l'anneau d'horizon reste actif. Toute vue nouvelle se
  vérifie à `capture-horizon.ps1`.

## Vérifier

| Quoi | Comment |
|---|---|
| Une étape de forge | `capture-terrain-relief.ps1 -Step 1/2/3/scale` (décor masqué) |
| Relief avant/après | `capture-terrain-forge.ps1` |
| Human Geography | `capture-human-geography.py` |
| Lointains | `capture-horizon.ps1` (part de pixels vides par image) |
| Pentes | tests `Anastasis.Terrain.Forge.Banks`, `Anastasis.Terrain.Forge.NoCliffs` (`report-tests.ps1`) |

## Ne pas faire

- Convertir en Landscape ou en World Partition « pour faire AAA » : cela remplace la génération du
  terrain par la simulation, ce qui est hors mandat.
- Rétablir terrasses ou escarpements.
- Dériver la rive du relief (étape 4 de `terrain-relief-001`) : abandonnée par Alexandre, car adoucir les
  berges à cette échelle obligeait à raboter les collines.
- Garder un masque d'eau par contour : 1,4 % de pixels changés pour des arêtes à plus de 60° passées de
  3 % à 12,5 %.

## Ouvert

- Mesh Terrain (5.8) : intéressant pour les surplombs, mais expérimental et construit pour l'édition à la
  main. Pas de mandat. Il repose sur le plugin `MeshPartition` (modificateurs non destructifs, sans
  Landscape), et `MeshPartitionWater` y branche le plugin Water (RU-002-10, RU-002-11). Y passer
  remplacerait la génération du terrain par la simulation : décision d'Alexandre, pas d'un agent.
- Le seuil de roche du sol n'est plus atteint après érosion (`fiches/sol.md`, SOL-05).
