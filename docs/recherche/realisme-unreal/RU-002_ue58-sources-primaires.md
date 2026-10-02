# RU-002 — Outils de réalisme et de 3D d'Unreal 5.8, sources primaires

Recherche du 2026-10-01, faite pour Alexandre. Deux sources seulement, dans cet ordre d'autorité :

1. **Le moteur installé** : `C:\Program Files\Epic Games\UE_5.8` (5.8.2, CL 56702186). Statut des plugins
   lu dans les `.uplugin` (`IsExperimentalVersion`, `IsBetaVersion`), CVars lues dans
   `Engine/Source/Runtime/Renderer/Private` et `Engine/Source/Runtime/Engine`. Ce qui est marqué
   **[moteur]** a été vu dans ce code.
2. **Epic** :
   - [RN58] https://dev.epicgames.com/documentation/en-us/unreal-engine/unreal-engine-5-8-release-notes
   - [RN57] https://dev.epicgames.com/documentation/en-us/unreal-engine/unreal-engine-5-7-release-notes?application_version=5.7
   - [ANN58] https://forums.unrealengine.com/t/unreal-engine-5-8-released/2729274 (annonce officielle ;
     le billet unrealengine.com renvoyait 403)
   - [DOC-MT] https://dev.epicgames.com/documentation/unreal-engine/mesh-terrain-in-unreal-engine
   - [DOC-NF] https://dev.epicgames.com/documentation/en-us/unreal-engine/nanite-foliage
   - [DOC-PVE] https://dev.epicgames.com/documentation/en-us/unreal-engine/procedural-vegetation-editor-in-unreal-engine

Quand les notes de version et le moteur divergent, le moteur fait foi.

## Éclairage

- **Lumen Lite** (Beta, [RN58]), aussi appelé « Lumen Medium Quality ». Annoncé deux fois plus rapide
  que Lumen en haute qualité, pour les PC modestes et les consoles portables. S'obtient par
  `sg.GlobalIlluminationQuality 1` + `sg.ReflectionQuality 1`. Les réflexions n'y tracent aucun rayon
  (SSR + sondes), le cache de surface est plus petit, les skeletal meshes sont exclus du tracing.
  **[moteur]** `r.Lumen.FinalGatherMethod` : `0` = *Irradiance Field Gather*, décrit dans le code comme
  « Faster but lower quality GI. Targeted at mid range PC and Switch 2 » ; `1` = *Screen Probe Gather*
  (défaut, haute qualité) ; `2` = ReSTIR, ray tracing matériel seulement. `BaseScalability.ini` met `0`
  au niveau GI 1 et `1` aux niveaux 2 et 3.
- **MegaLights** : Production Ready en 5.8 (Beta en 5.7) [RN58, RN57]. Ombres de très nombreuses lumières
  locales. **[moteur]** `r.MegaLights.EnableForProject` existe.
- `r.Lumen.HeightFog` : 1 par défaut en 5.8 (brouillard appliqué aux rayons de Lumen) [RN58].
  **[moteur]** confirmé, défaut 1.
- SSGI déprécié au profit de Lumen [RN58].
- **VSM** : ombres lointaines préfiltrées expérimentales, `r.Shadow.Virtual.PrefilteredDistant.ProjectEnable`
  [RN58]. **[moteur]** CVar introuvable dans `Renderer/Private` ni `Engine/Private` sous ce nom.
  `r.Shadow.Virtual.Max` n'existe pas ; `r.Shadow.Virtual.MaxPhysicalPages` et
  `r.Shadow.Virtual.ResolutionLodBiasDirectional` existent.

## Atmosphère

- **Fog Screen Space Scattering (FSSS)**, expérimental [RN58] : diffusion multiple approchée de
  l'ExponentialHeightFog (halo autour des sources dans la brume). Ne s'applique ni aux nuages volumétriques
  ni aux Heterogeneous Volumes. **[moteur]** deux interrupteurs : `r.Fog.ScreenSpaceScattering` (1 par
  défaut) **et** la propriété de composant `UExponentialHeightFogComponent::bEnableFSSS` (catégorie
  « Fog Screen Space Scattering - EXPERIMENTAL », **false** par défaut), réglée par `FSSSSpreadScale`
  (0,1 par défaut). Sans `bEnableFSSS`, rien ne se passe.
- SkyAtmosphere et Volumetric Clouds : aucune nouveauté en 5.8, des correctifs [RN58].
- Heterogeneous Volumes : Beta depuis 5.7 [RN57].

## Matériaux

- Substrate : production depuis 5.7, actif par défaut [RN57]. En 5.8, la BSDF diffuse rugueuse passe au
  modèle EON [RN58] : effet automatique sur tout projet Substrate.
- Toon / NPR Substrate : expérimental [RN58]. **[moteur]** `r.Substrate.ToonProfile.*`,
  `r.Substrate.Experimental.ToonUnifiedDiffuse`.
- RVT : outillage seulement en 5.8 [RN58].

## Géométrie et terrain

- **Mesh Terrain** : expérimental [RN58, DOC-MT], « overhangs, tunnels, sheer cliff walls ».
  **[moteur]** repose sur le plugin `MeshPartition` (expérimental, « large-scale mesh authoring system
  through spatial partitioning, non-destructive modifier editing ») ; l'outil d'édition est
  `MeshTerrainMode` (expérimental). Aucune référence à `ALandscape` dans les sources de MeshPartition.
- **Plugin Water sans Landscape** : **[moteur]** `MeshPartitionWater` (expérimental, « Interoperability of
  Mesh Partition with the Water plugin »).
- **Nanite Foliage, Assemblies, Voxels, Skinning** : expérimentaux depuis 5.7 [RN57, DOC-NF]. Le vent passe
  par des os (plugin `DynamicWind`), plus par le WPO ; pas de collision ; vent global seulement.
  **[moteur]** `r.Nanite.Foliage` (lecture seule, 0 par défaut, réglage de projet + redémarrage),
  `r.Nanite.AllowVoxels`, `r.Nanite.AllowAssemblies` ; `DynamicWind.uplugin` : « Extremely experimental
  dynamic wind support for Nanite foliage ».

## Végétation et procédural

- **Procedural Vegetation Editor (PVE)** : expérimental en 5.7 et 5.8 [RN57, RN58, DOC-PVE]. Graphe de
  nœuds qui pousse des arbres dans l'éditeur, exporte en Nanite Foliage ou en static mesh. Les assets 5.7
  ne sont pas compatibles 5.8.
- **PCG** : production depuis 5.7 [RN57], dispersion GPU au runtime en 5.8 [RN58].
  **[moteur]** `PCGGeometryScriptInterop` (Beta), `PCGBiomeCore` (expérimental, aucun statut publié par
  Epic).
- Megaplants : packs Quixel / Fab, pas une fonctionnalité du moteur.

## Eau

- Single Layer Water écrit sa vélocité en pré-passe de profondeur [RN58]. **[moteur]**
  `r.Water.SingleLayer.VelocityOutputPass` vaut **1 par défaut** (« Depth Prepass ») : déjà actif.

## Post-traitement et anti-crénelage

- ACES 2.0 en SDR, `r.LUT.Shaper`, outils de visualisation de la gradation [RN58].
- **TSR, géométrie fine** [RN58]. **[moteur]** l'interrupteur est `r.TSR.ThinGeometryDetection`
  (**0** par défaut ; détecte les pixels de feuillage à couverture partielle et assouplit le rejet
  d'historique, visible en `r.TSR.Visualize 15`). `r.TSR.ThinGeometryDetection.AntiFlickering` vaut déjà 1
  mais n'agit qu'avec la détection.

## Création 3D dans l'éditeur

- Modeling Tools plus rapides sur les maillages denses, Geometry Script : weight maps, bruit de Perlin
  [RN58]. **[moteur]** `ModelingToolsEditorMode` reste Beta, `GeometryScripting` sans marqueur.
- MetaHuman : Creator dans le moteur (5.7), foules expérimentales [RN57, RN58].

## Performance

- `stat unit` affiche la VRAM utilisée et son budget [RN58].
- Insights : World Streaming Insights, TraceQuery ; ProfileGPU plus détaillé [RN58].

## Affirmations du rapport RU-001 recoupées

| RU-001 | Verdict RU-002 |
|---|---|
| 01 toon Substrate | confirmé, expérimental |
| 06 Mesh Terrain, surplombs et tunnels | confirmé, expérimental, sur Mesh Partition |
| 18 PVE expérimental 5.7–5.8 | confirmé ; assets 5.7 incompatibles |
| 31 Lumen Lite deux fois plus rapide | confirmé, Beta ; c'est Irradiance Field Gather, une voie de coût |
| 35 `r.Shadow.Virtual.Max` | toujours absent du moteur |
| 52 Chaos Cloth pour plantes | faux : Chaos Cloth est prêt pour les vêtements ; les plantes passent par Dynamic Wind |
| 53 « Chaos Terrain de Trajectoire » | introuvable, ni dans les notes ni dans le moteur |

## Non vérifié

- Statut de BiomeCore, prérequis matériels de Mesh Terrain, émetteurs Niagara allégés.
- CVar des ombres lointaines préfiltrées sous le nom cité.
- Aucune de ces fonctionnalités n'a été lancée ni mesurée sur la RTX 3060.
