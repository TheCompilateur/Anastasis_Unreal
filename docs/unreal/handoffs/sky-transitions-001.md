# HANDOFF: sky-transitions-001

## MISSION

Supprimer le flash jour/nuit et la journée de 12 s : courbe d'exposition calibrée sur une
mesure du coucher, adaptation de l'œil bornée, `anastasis.Sim.Speed` = 1 par défaut.
Détail et mesures : `docs/unreal/SKY_TRANSITIONS_001.md`.

## FILES_OWNED

- Source/Anastasis_UnrealV2/WorldView/AnastasisAtmosphereProfile.h (ExposureStopsBelowDay, MaxExposureChangePerSecond)
- Source/Anastasis_UnrealV2/WorldView/AnastasisSkyClock.h / .cpp / Tests.cpp
- Source/Anastasis_UnrealV2/WorldView/AnastasisWorldAtmosphere.h / .cpp (exposition adaptée en Tick)
- Source/Anastasis_UnrealV2/Sim/AnastasisSimulationSubsystem.cpp (défaut de anastasis.Sim.Speed)
- tools/unreal/capture-sky.ps1 / .py (-Views)
- tools/unreal/first-building-pie.py, house-rest-pie.py, granary-eat-pie.py (posent Sim.Speed 10)
- docs/unreal/SKY_TRANSITIONS_001.md, docs/visual/sky-transitions-001/

## COMMIT

Voir `git log agent/sky-transitions-001` (tête au moment du `finish`).

## MEC

- BUILD: PASS
- TESTS: voir la sortie `report-tests` du portail `finish` ; Anastasis.Sky.* tous Success,
  dont SunsetOnlyDarkens et EyeAdaptation (nouveaux)
- COMMANDS:
  - `tools\unreal\capture-sky.ps1 -Label <l> -Views valley_long,ov_sw -States 'h1800=anastasis.Sky.Day 1;anastasis.Sky.Hour 18'`
  - `py tools/unreal/sky-clock-pie.py`

## SCN

PIE Lvl_AnastasisSlice à vitesse 1 : un jour = 90 s réelles ; balayage du coucher : l'image ne
fait plus que s'assombrir (valeurs dans le doc).

## PLY

UNKNOWN — PLAYER NOT_IMPLEMENTED.

## INTEGRATION_RISK

- **`anastasis.Sim.Speed` passe de 10 à 1** : tout PIE qui comptait sur un village 10× plus
  rapide sans le dire voit 10× moins de simulation par seconde. Les trois preuves village qui en
  dépendaient posent 10 elles-mêmes ; `gather-deliver-pie.py` et `food-supply-pie.py` réglaient
  déjà leur vitesse.
- Exposition différente au crépuscule pour toute capture prise entre ~+10° et −9° de soleil.

## STOP

Ne revendique pas : des journées plus longues que 90 s (DAY_LENGTH, parité JS), la remontée
résiduelle de la vue haute avant le coucher, les poches de brume vues d'en haut, un coût GPU.
