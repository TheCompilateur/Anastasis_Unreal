# HANDOFF: realisme-ru-002

## MISSION

Ingérer dans le skill `anastasis-realisme` une recherche sur les outils de réalisme et de 3D d'Unreal 5.8,
faite sur sources primaires : notes de version Epic 5.7 / 5.8, pages de doc Epic, et le moteur 5.8.2
installé (statut des plugins dans les `.uplugin`, CVars et valeurs par défaut lues dans le code du
renderer). Recouper au passage les affirmations de RU-001. Documentation seule : aucun réglage du jeu
n'est changé.

## FILES_OWNED

- `docs/recherche/realisme-unreal/RU-002_ue58-sources-primaires.md` (nouveau)
- `docs/recherche/realisme-unreal/README.md`
- `.claude/skills/anastasis-realisme/registre.md` (section RU-002, renvois dans RU-001-06, -18, -21, -31)
- `.claude/skills/anastasis-realisme/SKILL.md` (symptôme « scintillement » → post-traitement)
- `.claude/skills/anastasis-realisme/fiches/eclairage.md`, `vegetation.md`, `eau.md`, `atmosphere.md`,
  `terrain.md`, `post-traitement.md`, `performance.md`
- `docs/unreal/handoffs/realisme-ru-002.md`

## COMMIT

voir `git log agent/realisme-ru-002`

## MEC

- BUILD: SKIP (aucun fichier sous `Source/`, `Config/`, `Content/`, `Plugins/`)
- TESTS: SKIP (idem)
- Vérifications faites dans le moteur installé (`C:\Program Files\Epic Games\UE_5.8`, 5.8.2 CL 56702186) :
  - `r.Lumen.FinalGatherMethod` : 0 = Irradiance Field Gather (« Faster but lower quality GI. Targeted
    at mid range PC »), 1 = Screen Probe Gather (défaut), 2 = ReSTIR (HWRT). `BaseScalability.ini` :
    GI@1 → 0, GI@2 et GI@3 → 1.
  - `r.Nanite.Foliage` : lecture seule, défaut 0. `DynamicWind.uplugin` : expérimental.
  - `r.Fog.ScreenSpaceScattering` défaut 1 ; `UExponentialHeightFogComponent::bEnableFSSS` défaut false,
    `FSSSSpreadScale` 0,1. Le projet ne pose ni l'un ni l'autre (`grep FSSS Source` : rien).
  - `r.TSR.ThinGeometryDetection` défaut 0 ; `r.TSR.ThinGeometryDetection.AntiFlickering` défaut 1.
  - `r.Water.SingleLayer.VelocityOutputPass` défaut 1 (pré-passe) : déjà actif.
  - `r.Lumen.HeightFog` défaut 1.
  - `r.Shadow.Virtual.PrefilteredDistant.ProjectEnable` : **introuvable** sous ce nom dans
    `Renderer/Private` et `Engine/Private`.
  - Plugins expérimentaux : `MeshPartition`, `MeshTerrainMode`, `MeshPartitionWater`,
    `ProceduralVegetationEditor`, `PCGBiomeCore`, `Water` ; Beta : `PCGGeometryScriptInterop`,
    `ModelingToolsEditorMode`.
  - Lumières locales dans `Source/` : seulement `Variant_Horror/HorrorCharacter` (gabarit).

## SCN

Aucune scène ouverte, aucune capture : rien n'a été lancé ni mesuré dans Unreal.

## PLY

`PLAYER` non touché.

## INTEGRATION_RISK

- `registre.md` et les fiches sont aussi édités par toute mission visuelle qui corrige une valeur
  (« le code fait foi »). Conflits textuels possibles, faciles : les ajouts sont en fin de section.

## STOP

- Aucune des fonctionnalités citées n'a été activée, lancée ni mesurée sur la RTX 3060.
- Les candidats d'A/B (FSSS RU-002-06, TSR RU-002-16, Lumen Lite RU-002-01) restent `OUVERT` : missions à
  part, sur mandat.
- Nanite Foliage, PVE, Mesh Terrain et `MeshPartitionWater` touchent des décisions d'Alexandre (EAU-01,
  RU-001-21, terrain issu de la simulation) : la mission les signale comme faits nouveaux, sans trancher.
