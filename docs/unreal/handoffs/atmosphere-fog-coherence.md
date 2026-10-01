# HANDOFF: atmosphere-fog-coherence

## MISSION

ATMOSPHERE_COHERENCE_001 (`docs/unreal/ATMOSPHERE_COHERENCE_001.md`) : cohérence du ciel existant,
sans le remplacer. Un seul astre directeur (fin du warning « plusieurs éclairages directionnels en
concurrence »), brouillard qui ne luit pas (diffusion du soleil couché, albédo de la brume), météo
du ciel continue à minuit, compression des hautes lumières au crépuscule, commande
`Anastasis.Presentation`, procédure de capture 8 heures × 3 humidités et métriques.

## FILES_OWNED

- `Source/Anastasis_UnrealV2/WorldView/AnastasisWorldAtmosphere.{h,cpp}`
- `Source/Anastasis_UnrealV2/WorldView/AnastasisSkyClock.{h,cpp}`
- `Source/Anastasis_UnrealV2/WorldView/AnastasisAtmosphereProfile.h`
- `Source/Anastasis_UnrealV2/WorldView/AnastasisPresentationMode.cpp` (nouveau)
- `Source/Anastasis_UnrealV2/WorldView/AnastasisWorldProbeSubsystem.cpp` (bloc atmosphère seul)
- `Source/Anastasis_UnrealV2/WorldView/AnastasisSkyClockTests.cpp`, `AnastasisAtmosphereTests.cpp`
- `tools/unreal/atmosphere-profile.py`, `tools/unreal/capture-sky.ps1`, `tools/unreal/atmosphere-metrics.py` (nouveau)
- `docs/unreal/ATMOSPHERE_COHERENCE_001.md`, `docs/visual/atmosphere-coherence-001/baseline-metrics.txt`
- `AGENTS.md` (index : ligne `capture-sky`, ligne `atmosphere-metrics.py`)

## COMMIT

`8fb2399` (mission) + `95daf58` (fusion de `main` à `e70871b`, sans conflit), branche
`claude/atmosphere-fog-coherence-tgt2my`. Écrit dans une session cloud Linux, pas dans un worktree
`C:\dev\ANASTASIS_WORKTREES` : l'intégrateur la récupère en `agent/atmosphere-fog-coherence`.

## MEC

- BUILD: **UNKNOWN** — aucun moteur dans la session, jamais compilé.
- TESTS: **UNKNOWN** — aucun run d'automatisation.
- Vérifié hors moteur : seuils des tests purs recalculés par un portage Python de `WeatherAt`
  (`raw_steps_over_0.05=153 max_raw_step=0.685 max_sky_midnight_gap=9e-09 max_sky_minute_step=0.0197` ;
  diffusion solaire : pas maximal 0.071 par minute pour un seuil de 0.1) ; `atmosphere-metrics.py`
  calé sur les 11 captures de day-night-weather-001 (drapeaux sur les deux images fautives seulement).
- COMMANDS (à lancer par l'intégrateur, sur Windows) :
  - `git fetch origin claude/atmosphere-fog-coherence-tgt2my`
  - `git branch agent/atmosphere-fog-coherence FETCH_HEAD`
  - `tools\unreal\agent-worktree.ps1 finish -Mission atmosphere-fog-coherence`
  - `tools\unreal\capture-sky.ps1 -Label coherence-after -Preset cycle`
  - `python tools\unreal\atmosphere-metrics.py Saved\SkyEvidence\coherence-after`

Tests nouveaux : `Anastasis.Sky.Clock.WeatherHasNoMidnightStep`, `Anastasis.Sky.Clock.OneForwardLight`,
`Anastasis.Atmosphere.ForwardLight.OneLeader`. Modifié : `Anastasis.Sky.Clock.FogAndMistFollowTheLight`.

## SCN

UNKNOWN — aucune capture. Attendu : `capture-sky -Preset cycle`, 96 images, planches `contact_<vue>.png`.

## PLY

UNKNOWN — attendu en PIE : plus de warning moteur à l'écran ; lignes `ANASTASIS_SKY ... forward_light=sun|moon`.

## INTEGRATION_RISK

- **Compilation non prouvée.** API utilisées, existence et signatures confirmées dans la doc
  d'Epic (2026-10-01) : `UDirectionalLightComponent::SetForwardShadingPriority(int32)` et la
  propriété `ForwardShadingPriority` (lue directement, comme `bCastCloudShadows` /
  `CloudShadowStrength` le sont déjà dans `ApplyRealism`) ; `ULightComponent::SetVolumetricScatteringIntensity(float)`
  et `VolumetricScatteringIntensity` ; `FPostProcessSettings::LocalExposureHighlightContrastScale`
  (float, plage usuelle 0,6–1,0 selon Epic) + `bOverride_`. La commande console suit la signature
  de `AnastasisWorldProbeCommands.cpp`. Reste possible : une faute que seul le compilateur voit.
- Fichiers chauds : `AnastasisWorldAtmosphere.cpp` et `AnastasisAtmosphereProfile.h` (déjà touchés par
  env-realism-001 et sky-transitions-001).
- `DA_AnastasisAtmosphere` : si l'asset sérialise un `mist_albedo` retouché à la main, le nouvel albédo du
  code ne s'applique pas — relire `ANASTASIS_ATMOSPHERE_PROFILE` au log.
- Voie village (PNJ) : aucun fichier touché ; `Anastasis.Presentation` pose et rend la **valeur** de
  `anastasis.Village.Debug`.

## STOP

Ne revendique ni build, ni test vert, ni aspect visuel. `MistAlbedo` 0,6 et
`TwilightHighlightContrastScale` 0,6 sont des jugements à recaler sur la première planche du cycle.
