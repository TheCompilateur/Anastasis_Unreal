# ATMOSPHERE_COHERENCE_001 — un seul astre directeur, un brouillard qui ne luit pas

Mission `atmosphere-fog-coherence` (branche `claude/atmosphere-fog-coherence-tgt2my`), 2026-10-01.
Suite de DAY_NIGHT_WEATHER_001 et SKY_TRANSITIONS_001. Le cycle ne change pas : on corrige la
cohérence de ce qu'il pilote déjà.

> **Statut de la preuve : NON VÉRIFIÉ DANS UNREAL.** Ce travail a été écrit dans un conteneur
> Linux, sans moteur ni éditeur, avec le serveur MCP `unreal` injoignable. Il n'a été ni compilé,
> ni testé dans l'éditeur, ni capturé. Ce qui est prouvé ici : la lecture du code, et les seuils
> des tests purs recalculés en Python sur deux ans de météo (voir § Preuves). Avant toute
> intégration : `build`, `report-tests`, `capture-sky.ps1 -Preset cycle`, puis
> `atmosphere-metrics.py`. Voir § Ce qu'il reste à faire.

## 1. Carte du système (audit)

Tout l'environnement a **un seul propriétaire C++**. Pas de Blueprint, pas de Timeline, pas de
CurveFloat : les courbes sont des tableaux et des fonctions pures du profil.

| Rôle | Où | Qui l'écrit |
|---|---|---|
| Propriétaire de l'atmosphère | `AAnastasisWorldAtmosphere` (`WorldView/AnastasisWorldAtmosphere.*`), créé par `AAnastasis_UnrealV2GameMode::BeginPlay` | `Apply()` (une fois), `Tick()` → `UpdateSky()` (chaque frame, horloge active) |
| Données | `UAnastasisAtmosphereProfile` (`DA_AnastasisAtmosphere`, repli sur les valeurs du code) | `tools/unreal/atmosphere-profile.py` |
| Horloge du ciel (pure) | `AnastasisSkyClock` : heure = `AnastasisRhythm::DayFracOf`, déclinaison, exposition par élévation, météo de la simulation | — |
| Météo | `AnastasisWeather` (`AnastasisSim`, parité JS) : nouveau tirage de base, de vent et de front **chaque jour** | — |
| Soleil | `DirectionalLight` **adoptée** : `Sun_SliceObservation` du rig `observe-slice.py` dans `Lvl_AnastasisSlice`, créée si absente | `Apply` (intensité 75 000 lux, couleur, lumière d'atmosphère index 0), `UpdateSky` (rotation, ombres) |
| Lune | `DirectionalLight` **créée**, tag `AnastasisMoon`, 0,3 lux, 4100 K, atmosphère index 1 | `ApplyRealism`, `UpdateSky` |
| Sky Atmosphere | adoptée (`SkyAtmosphere_Slice`) | `ApplyRealism` : Mie 0,0065, perspective aérienne ×3, albédo du sol |
| Sky Light | adoptée (`SkyLight_Slice`), capture temps réel | `Apply`, `ApplyRealism` (occlusion par les nuages) |
| Nuages volumétriques | créés (`Anastasis_Clouds`), matériau moteur `m_SimpleVolumetricCloud_Inst`, instance dynamique | `ApplyRealism`, `UpdateSky` (`Cloud_GlobalCoverage` ← couverture) |
| Exponential Height Fog | adopté ou créé | `Apply` (densité 0,012, falloff 0,2, départ 15 m, opacité max 0,85, inscattering), `ApplyRealism` (couche de vallée, brouillard volumétrique 60 m), `UpdateSky` (densité × (1 + 1,5·humidité), inscattering × 2^(EV − 14)) |
| Brume | `ALocalFogVolume` par cellule humide (`AnastasisMistField`, champ `Wetness`) | `ApplyMist`, `UpdateSky` (× facteur de brume : nuit/aube, soleil, humidité, vent) |
| Exposition | `PostProcessVolume` non borné adopté (`PP_SliceExposure`), histogramme min = max | `Apply` (EV 14), `UpdateSky` (EV par élévation, adaptation 3 EV/s, saturation et balance des blancs de nuit) |
| Cache Lumen | `r.EyeAdaptation.CachedLightingPreExposure` | suit l'EV appliqué |
| Debug à l'écran | `anastasis.Sim.Overlay` (ligne cyan, simulation), `anastasis.Village.Debug` (village — voie PNJ), `anastasis.Drainage.Debug`, messages moteur | chacun son interrupteur ; **aucun** mécanisme global avant cette mission |

## 2. Causes

### Le warning « plusieurs éclairages directionnels sont en concurrence »

Le moteur ne prend **qu'une** lumière directionnelle pour l'ombrage forward, la translucidité,
l'eau Single Layer Water et le brouillard volumétrique : la plus haute `ForwardShadingPriority`,
et à égalité la plus lumineuse — en le signalant à l'écran. Inventaire réel : **deux**
lumières directionnelles en jeu, le soleil adopté et la lune créée par la couche de réalisme
(ENV_REALISM_001). Toutes deux à la priorité par défaut, **0**. Le warning n'était donc pas un
doublon de carte, mais la lune elle-même, présente à toute heure.

Conséquence au-delà du message : le repli « la plus lumineuse » choisissait **toujours le soleil**
(75 000 lux contre 0,3), y compris couché, ombres coupées (`UpdateSky` les éteint sous
l'horizon). La nuit, brouillard volumétrique, eau et translucides étaient éclairés par un soleil
sous la planète, jamais par la lune.

### Le brouillard « mur lumineux »

Mesuré sur les captures de DAY_NIGHT_WEATHER_001 (`baseline-metrics.txt`) et vu sur la planche
`03_night_vision_contact_sheet.png` :

1. **Les poches de brume luisent.** Albédo 0,86–0,94, éclairées sans ombre par le ciel et le
   soleil bas, au-dessus d'un sol ombré et occlus (albédo ~0,25) : à 19 h 30, vue haute, les
   bancs culminent à 83 de luma (p99, max 124) pour un sol à 30 de médiane — ~1,5 diaphragme
   au-dessus du paysage qu'ils couvrent. Ils se lisent comme des sources de lumière.
2. **Le soleil couché éclaire encore le brouillard** : il reste la lumière forward (§ précédent),
   avec une intensité de diffusion volumétrique de 1.
3. **L'humidité saute à minuit.** `AnastasisWeather` tire une nouvelle base et un nouveau vent
   chaque jour : densité du brouillard (× 1 + 1,5·humidité) et brume (× 1 + humidité,
   × 1 − 0,6·vent) changeaient en une frame à 00 h 00. Sur deux ans de la graine 12345 :
   **153 minuits sur 239** font sauter l'humidité de plus de 0,05, jusqu'à **0,685**.
4. Aube et crépuscule (déjà notés par DAY_NIGHT_WEATHER_001) : vue haute à 6 h, médiane 199/255,
   18 % de pixels > 235, 44 % du sol en aplat clair sans détail → drapeau `WALL`. Vallée à
   18 h 30 : tiers médian sans relief → `HAZE`. Ces captures précèdent la courbe d'exposition de
   SKY_TRANSITIONS_001 ; une part de ce blanc a déjà été corrigée par elle.

## 3. Corrections

Toutes dans le propriétaire existant (`AAnastasisWorldAtmosphere`, `AnastasisSkyClock`, profil).
Aucun nouveau système ne pilote un paramètre déjà piloté.

| # | Quoi | Où |
|---|---|---|
| A | **Un seul astre directeur** : `ForwardShadingPriority` 1 au soleil tant qu'il est levé, à la lune dès qu'il est couché (même prédicat que les ombres, `IsBelowHorizon`) ; l'autre à 0. Écrit dans `Apply` et à chaque `Tick`, seulement s'il change. Toute 3e lumière directionnelle est **signalée** (`extra_directional_lights=`), pas adoptée ni tue | `ArbitrateDirectionalLights`, `AnastasisSkyClock::MoonLeadsForwardShading` |
| B1 | **Le soleil couché ne diffuse plus dans le brouillard** : `VolumetricScatteringIntensity` du soleil = 0 sous l'horizon, smoothstep jusqu'à 1 à +4° | `SunFogScatteringFor`, profil `SunFogScatterFullElevationDegrees` |
| B2 | **Albédo de la brume** 0,86/0,90/0,94 → **0,60/0,64/0,68** (~−0,6 diaphragme) | profil `MistAlbedo`, `atmosphere-profile.py` |
| C | **Exposition locale au crépuscule** : `LocalExposureHighlightContrastScale` = valeur du projet (`r.DefaultFeature.LocalExposure.HighlightContrastScale`, 0,8) de jour, **0,6** soleil parti, linéaire sur le facteur `Daylight` existant. L'EV épinglé ne bouge pas ; les hautes lumières seules — la nuit reste aussi sombre | `HighlightContrastFor`, profil `TwilightHighlightContrastScale` |
| D | **Météo du ciel continue à minuit** : humidité, vent et couverture fondus sur 4 h (22 h–2 h) entre le dernier instant du jour qui finit et le premier du suivant, smoothstep valant ½ des deux côtés de minuit. Le brouillard, la brume et les nuages lisent `SkyHumidity` / `SkyWind` / `SkyCover` ; `FSkyState::Weather` reste celle de la simulation, au bit | `SkyWeatherAt`, profil `WeatherBlendHours` |
| E | **Rien d'autre ne change** : courbe d'exposition, adaptation, vision de nuit, densité et couleur du brouillard, couche de vallée, nuages, lune, eau | — |

Présentation (§ 8 de la mission), en réutilisant les interrupteurs existants :

```
Anastasis.Presentation 1   PRESENTATION : Sim.Overlay 0, Village.Debug 0, Drainage.Debug 0, DisableAllScreenMessages
Anastasis.Presentation 0   DEVELOPMENT : chaque interrupteur retrouve la valeur qu'il avait
Anastasis.Presentation     bascule
```

Rien n'est supprimé ni redessiné : la commande pose des CVars et les rend.
`anastasis.Village.Debug` appartient à la voie village : sa **valeur** est posée, son code n'est
pas touché.

Captures (`anastasis.Sky.Humidity`, nouveau, comme `Sky.Hour` : épingle ce que le ciel
**montre**, jamais la simulation).

### Valeurs avant / après

| Paramètre | Avant | Après |
|---|---|---|
| Soleil `ForwardShadingPriority` | 0 | 1 levé, 0 couché |
| Lune `ForwardShadingPriority` | 0 | 0 soleil levé, 1 couché |
| Lumière forward la nuit (brouillard volumétrique, eau, translucides) | soleil (repli par luminosité) | lune |
| Soleil `VolumetricScatteringIntensity` | 1 à toute heure | 0 sous l'horizon → 1 à +4° |
| `MistAlbedo` | (0,86, 0,90, 0,94) | (0,60, 0,64, 0,68) |
| `LocalExposureHighlightContrastScale` | 0,8 (projet) à toute heure | 0,8 de jour → 0,6 soleil parti |
| Saut d'humidité du ciel à minuit (2 ans, graine 12345) | jusqu'à 0,685 | < 1e-6 ; pas maximal par minute 0,0197 (= la variation intrajournalière de la simulation elle-même) |

Le profil `DA_AnastasisAtmosphere` ne sérialise que ce qui diffère des valeurs du code à la
sauvegarde : semé aux valeurs du code de l'époque, il suit les nouvelles. **À vérifier** sur le
log : `ANASTASIS_ATMOSPHERE_PROFILE` et, si l'asset a été retouché à la main, relire `mist_albedo`.

## 4. Preuves

### Exécutées ici

- Seuils de `Anastasis.Sky.Clock.WeatherHasNoMidnightStep` recalculés par un portage Python de
  `WeatherAt` / `SampleCoverFront` / `WeatherHumidityAt` (240 jours, minute par minute) :
  `raw_steps_over_0.05=153 max_raw_step=0.685 max_sky_midnight_gap=9e-09 max_sky_minute_step=0.0197`
  — le test exige `> 0`, `< 1e-6`, `< 0,0685`.
- Seuil de `Anastasis.Sky.Clock.OneForwardLight` : élévation solaire maximale par minute sur
  l'année à 41° N = 0,189° → pas maximal de diffusion 0,071 ; le test exige `< 0,1`.
- `atmosphere-metrics.py` calé sur les 11 captures de DAY_NIGHT_WEATHER_001 :
  `docs/visual/atmosphere-coherence-001/baseline-metrics.txt`. Drapeaux exactement sur les deux
  images fautives à l'œil (aube vue haute `WALL HAZE`, vallée 18 h 30 `HAZE`), aucun à 10 h,
  23 h, pluie, neige, été clair.

### NON exécutées (pas de moteur dans cette session)

- `BUILD::PASS` : **non obtenu.** API utilisées, à confirmer par le build :
  `UDirectionalLightComponent::SetForwardShadingPriority` / `ForwardShadingPriority`,
  `ULightComponent::SetVolumetricScatteringIntensity` / `VolumetricScatteringIntensity`,
  `FPostProcessSettings::LocalExposureHighlightContrastScale` (+ `bOverride_`).
- Tests nouveaux : `Anastasis.Sky.Clock.WeatherHasNoMidnightStep`,
  `Anastasis.Sky.Clock.OneForwardLight`, `Anastasis.Atmosphere.ForwardLight.OneLeader`.
  Test modifié : `Anastasis.Sky.Clock.FogAndMistFollowTheLight` (fait varier `SkyHumidity` /
  `SkyWind`, que `MistFactorFor` lit désormais). À faire passer par `report-tests.ps1`.
- Disparition du warning à l'écran, aspect du brouillard, de l'eau, de l'aube et du coucher :
  **non observés.**

## 5. Procédure de validation reproductible

```powershell
tools\unreal\anastasis-unreal.ps1 build
tools\unreal\report-tests.ps1
tools\unreal\capture-sky.ps1 -Label coherence-after -Preset cycle
python tools\unreal\atmosphere-metrics.py Saved\SkyEvidence\coherence-after
```

`-Preset cycle` : jour 1 (équinoxe : lever 6 h, coucher 18 h), heures **06 09 12 16 18 20 00 03**,
humidité du ciel épinglée **0 / 0,5 / 1** (sec, humide, saturé), quatre vues (oblique, fond de
vallée, crête, contre-jour) : 96 images. Pour l'A/B, la même commande sur `main` :
`anastasis.Sky.Humidity` n'y existe pas, les trois colonnes y montrent donc la météo du jour 1. Sorties : `metrics.txt`, une
planche `contact_<vue>.png` par vue (lignes = heures, colonnes = humidité).

À regarder, image par image, pas seulement les drapeaux :

| Heure | Ce qui doit tenir |
|---|---|
| 12 h | rivière qui reflète le ciel, ombres nettes, aucun `WALL` |
| 16 h | lumière rasante, profondeur : `depth` décroît bas → haut sans tomber à 0 |
| 18 h / 06 h | coucher / lever forts, sans aplat blanc (`WALL`) ni voile uniforme (`HAZE`) |
| 20 h / 00 h / 03 h | nuit franche ; bancs de brume plus clairs que le sol mais pas lumineux ; reflet de lune sur l'eau possible |
| saturé | brouillard dense, silhouettes qui s'effacent avec la distance, pas d'image blanche |

En PIE : `sky-clock-pie.py` sur un jour entier — la ligne `ANASTASIS_SKY` porte maintenant
`forward_light=sun|moon`, `sun_fog_scatter=` et `sky_humidity=` ; et **le warning moteur ne doit
plus apparaître à l'écran** (`Anastasis.Presentation 0`, messages visibles). La sonde
(`probe-demo`) rapporte `sun_forward_shading_priority` / `moon_forward_shading_priority`.

## 6. Ce qui reste ouvert

1. **Tout ce qui est visuel est à constater** (statut en tête). Les valeurs `MistAlbedo` 0,6 et
   `TwilightHighlightContrastScale` 0,6 sont des jugements argumentés, pas des mesures : à
   recaler sur la première planche du cycle.
2. Les poches de brume restent des volumes sans ombre au-delà de 60 m (portée du brouillard
   volumétrique) : vues d'en haut à l'aube, des taches plus claires subsistent, moins vives.
3. Densité de brouillard à humidité 1 : × 2,5 (`FogHumidityGain` 1,5), inchangée. Si le cycle
   `sat` sort `WALL` en plein jour, c'est le prochain levier, pas l'exposition.
4. L'échange soleil → lune au passage de l'horizon est un pas pour l'eau (reflet spéculaire) ;
   pour le brouillard il est continu (diffusion solaire déjà à 0 à l'horizon).
5. Une troisième lumière directionnelle dans une carte ramènerait le warning : elle est loguée
   (`extra_directional_lights`), pas neutralisée — c'est une faute de carte.
6. Pluie visible (Niagara) et coût GPU de la couche : hors périmètre, inchangés.

## 7. Travail parallèle (PNJ)

Aucun fichier PNJ, village, personnage, sprite, perception ou simulateur n'est modifié. Seul
point de contact : `Anastasis.Presentation` **pose la valeur** de `anastasis.Village.Debug`
(et la rend). Si la voie village renomme cette CVar, la commande le signale
(`missing_switch=`) et continue.
