# HANDOFF: env-realism-001

## MISSION

Le ciel suit l'horloge et la météo de la simulation (DAY_NIGHT_WEATHER_001) : portage de
`weather.js` dans `AnastasisSim` avec parité, horloge du ciel pure (`AnastasisSkyClock`),
soleil / lune / exposition / vision de nuit / nuages / brouillard / brume pilotés en PIE par la
simulation ; couche de réalisme atmosphérique réversible (ENV_REALISM_001). Détail et preuves :
`docs/unreal/DAY_NIGHT_WEATHER_001.md`.

## FILES_OWNED

- Source/AnastasisSim/Public/World/AnastasisWeather.h
- Source/AnastasisSim/Private/World/AnastasisWeather.cpp
- Source/AnastasisSim/Private/Tests/AnastasisWeatherTests.cpp
- Source/AnastasisSim/Private/Tests/AnastasisWeatherVectors.inl (généré)
- Source/AnastasisSim/Public/Core/AnastasisJsNumeric.h (ajout `AnastasisJs::Exp`, fdlibm ; rien de modifié)
- Source/Anastasis_UnrealV2/WorldView/AnastasisSkyClock.h / .cpp / Tests.cpp
- Source/Anastasis_UnrealV2/WorldView/AnastasisWorldAtmosphere.h / .cpp
- Source/Anastasis_UnrealV2/WorldView/AnastasisAtmosphereProfile.h
- Source/Anastasis_UnrealV2/WorldView/AnastasisAtmosphereResolver.h / .cpp
- Source/Anastasis_UnrealV2/WorldView/AnastasisAtmosphereTests.cpp
- Source/Anastasis_UnrealV2/WorldView/AnastasisWorldProbeSubsystem.cpp (bloc atmosphère : lune, nuages, brume de vallée)
- tools/migration/parity/weather.mjs
- tools/unreal/capture-sky.ps1 / .py, tools/unreal/sky-clock-pie.py
- docs/unreal/DAY_NIGHT_WEATHER_001.md, docs/visual/day-night-weather-001/

## COMMIT

Voir `git log agent/env-realism-001` (tête au moment du `finish`).

## MEC

- BUILD: PASS (après fusion de `main` à 9175a37)
- TESTS: voir la sortie `report-tests` du portail `finish` ; tests du chantier tous Success
  (Parite.Meteo 12212 comparées / 0 échec / 39 à 1 ULP sur cos-sin ; Sky.Clock.* ; Atmosphere.* ; Mist.*)
- COMMANDS:
  - `tools\unreal\anastasis-unreal.ps1 build`
  - `node tools/migration/gen-parity.mjs weather.mjs`
  - `tools\unreal\capture-sky.ps1 -Label <l> -States 'h06=anastasis.Sky.Hour 6|h23=anastasis.Sky.Hour 23'`
  - `py tools/unreal/sky-clock-pie.py` (PIE, Lvl_AnastasisSlice)

## SCN

PIE `Lvl_AnastasisSlice`, 108 s réelles = 10 jours de simulation : une ligne `ANASTASIS_SKY` par
phase du village, soleil sous l'horizon à chaque `night`, déclinaison croissante, jours de pluie
de la référence (`docs/visual/day-night-weather-001/sky-clock-pie.txt`).

## PLY

UNKNOWN — aucun joueur (PLAYER NOT_IMPLEMENTED). Coût GPU de la couche non mesuré.

## INTEGRATION_RISK

- **Change l'image par défaut de tout le monde** : le soleil n'est plus à −38 / −55 mais à
  l'heure de la simulation (10 h 05 dans l'éditeur, azimut différent), nuages volumétriques,
  lune, brume modulée. `anastasis.Sky.Clock 0` et `anastasis.Atmosphere.Realism 0` rendent
  l'image d'avant ; les A/B d'autres missions pris dans une même session restent comparables.
- `AAnastasisWorldAtmosphere` tick désormais (PIE seulement ; sort tout de suite horloge coupée).
- Une deuxième `DirectionalLight` (la lune, tag `AnastasisMoon`) existe en PIE : tout code qui
  prend « la première DirectionalLight » comme soleil doit sauter ce tag (fait ici pour
  l'atmosphère, la sonde et les tests).
- Le volume de post-process épinglé reçoit désormais saturation et balance des blancs (nuit).
- `anastasis.Sim.Speed` = 10 : un jour ≈ 12 s réelles, le soleil balaie ~30°/s. Décision de
  design laissée à Alexandre.

## STOP

Ne revendique pas : une aube et un crépuscule justes (trop roses), la pluie ou la neige
visibles (aucun Niagara du projet), le câblage de `WeatherWetnessAt` au sol, un coût GPU, ni
l'hydrologie des berges ou la distribution écologique du brief d'origine.
