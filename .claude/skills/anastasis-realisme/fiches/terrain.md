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

## Le continent autour de la carte (continental-001)

Au-delà des 96 tuiles, l'anneau d'horizon (`AnastasisTerrainHorizon`) n'est plus un plateau : sa
hauteur vient de `AnastasisTectonics` (`WorldView/AnastasisTectonics.{h,cpp}`), une fonction **pure** de
(x, y, graine, sens de l'eau), en kilomètres et mètres à l'échelle de référence 5.

| Élément | Où / valeur |
|---|---|
| Sens des basses terres | somme des normales sortantes des colonnes d'eau du bord de la carte : l'eau sort là où le continent descend, les chaînes sont à l'opposé |
| Plateau continental | piémont 220 m dès 2 km, plateau +640 m de 16 à 46 km |
| Ceinture de plis | 2 à 13 km : crêtes parallèles à front raide et revers doux (hogbacks), espacées de 2,3 km, coupées de cluses ; jusqu'à 380 m |
| Faille décrochante | à 8 km : tranchée de 100 m, 0,6 km de large, crêtes de blocage décalées |
| Escarpement de faille normale | à 7 km côté basses terres, 85 m : la carte est une terrasse |
| Chaîne principale | axe à 12,5 km (± 3), 2 à 3,4 km, front raide côté carte, bruit érodé (vallées en V), cols tous les 7 à 13 km, contrefort parallèle, arêtes vives |
| Seconde chaîne | à 40 km, 3 à 4 km, enneigée |
| Côté basses terres | collines et crêtes de 3 à 16 km, chaîne extérieure basse à 16-34 km |
| Cuvette | rien ne s'ouvre sur le vide : 650 m à 64 km, dans toutes les directions |
| Résolution | pas radial ≤ 1,5 % de la distance au centre (`FarAngularStep`) : ~0,86° vu du centre, jusqu'à 60 km (`OuterTiles` 3 000) |
| Surface | forêt montagnarde, alpage, roche, neige lus de l'altitude et de la **pente rendue** du sommet (`SurfaceAt`) ; la vallée garde la palette de prairie de la carte |

- **TER-06** — Une montagne se juge de la **ligne de crête vue de 1,7 m**, pas du plan : `capture-horizon.ps1
  -Mode skyline`, huit vues tous les 45°. Un relief qui ne fait pas lever la tête depuis le bassin (la chaîne
  monte à plus de 5° dans le ciel, test `Anastasis.Terrain.Tectonics.Horizon`) n'est pas un continent.
- **TER-07** — Le champ proche (5 km) garde la règle de 45° ; les chaînes sont bornées à 62° par triangle.
- **TER-08** — Le continent est de la présentation : jamais dans `AnastasisSim`, jamais une dérivation de la
  simulation. Une nouvelle composante s'ajoute à `AnastasisTectonics::Evaluate` (et à `FBreakdown`), avec son
  test de structure.

### Reliefs 3D pontiques (PONTIC_MOUNTAINS_001)

Huit `StaticMesh` fermés sous `/Game/Anastasis/PonticMountains/` traduisent les silhouettes de la
planche `docs/visual/reference/pontic-mountain-assets.png` en volumes distincts. Le script d'autorité
`tools/unreal/create-pontic-mountains.py` règle la forme et l'albédo de chaque mesh ; la planche
ne contient aucune géométrie importable. Ces assets sont **des sources d'auteur**, non des instances
dans la carte : cinq essais de pose et de modification de l'anneau ont été rejetés après captures
à hauteur humaine. Le code expérimental a été retiré. Aucun verdict SCN ou PLY positif ne découle
de l'existence des assets. Repartir de la géométrie globale de l'anneau et de références de terrain
documentées avant de proposer une nouvelle intégration visuelle.

## Vérifier

| Quoi | Comment |
|---|---|
| Une étape de forge | `capture-terrain-relief.ps1 -Step 1/2/3/scale` (décor masqué) |
| Relief avant/après | `capture-terrain-forge.ps1` |
| Human Geography | `capture-human-geography.py` |
| Lointains | `capture-horizon.ps1` (part de pixels vides par image) ; `-Mode skyline` : la ligne de crête dans huit directions |
| Continent | tests `Anastasis.Terrain.Tectonics.*` (structure, cadre, surface, horizon) ; banc de forme hors éditeur : compiler `AnastasisTectonics.cpp` seul avec un `CoreMinimal.h` minimal (g++) et lancer un raycast de heightfield -- la forme se juge en secondes, le matériau en éditeur |
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
