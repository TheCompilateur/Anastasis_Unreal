# HANDOFF: village-weather-001

## MISSION

Les habitants réagissent à la météo de la simulation, comme dans la référence JS : biais météo sur
chaque ligne de la table de décision, abri sous l'orage (score, porte de `commitGoalChoice`, cible,
entrée, durée, récupération, délai de grâce), exposition à l'orage, pas pressé sous la pluie.
Détail : `docs/unreal/VILLAGE_WEATHER_001.md`.

## FILES_OWNED

- Source/AnastasisSim/Public/Life/AnastasisWeatherBehavior.h
- Source/AnastasisSim/Private/Life/AnastasisWeatherBehavior.cpp
- Source/AnastasisSim/Private/Tests/AnastasisWeatherBehaviorTests.cpp
- Source/AnastasisSim/Private/Tests/AnastasisWeatherBehaviorVectors.inl (généré)
- Source/AnastasisSim/Private/Tests/AnastasisVillageWeatherTests.cpp
- Source/AnastasisSim/Public/Village/AnastasisVillage.h (écart n° 17, champs d'abri, météo du village)
- Source/AnastasisSim/Private/Village/AnastasisVillage.cpp (table, porte d'orage, abri, exposition, marche)
- Source/AnastasisSim/Private/Sim/AnastasisSimulation.cpp (graine météo au Reset)
- Source/AnastasisSim/PORTAGE.md
- Source/Anastasis_UnrealV2/Sim/AnastasisSimulationSubsystem.cpp (`Anastasis.Village.ForceWeather`)
- tools/migration/parity/weather-behavior.mjs
- tools/unreal/village-weather-pie.py
- docs/unreal/VILLAGE_WEATHER_001.md

## COMMIT

Voir `git log agent/village-weather-001` (tête au moment du `finish`).

## MEC

- BUILD: PASS
- TESTS (run ciblé, `-nullrhi`, 2026-10-01) : tous Success
  - `Anastasis.Sim.Parite.MeteoHabitants` : compared=4212 failures=0 (2 724 vecteurs)
  - `Anastasis.Sim.MeteoHabitants.Formules`
  - `Anastasis.Sim.MeteoHabitants.Orage` : abri au grenier, durée `shelterRainDuration(0,9)`,
    énergie récupérée, délai 18 s ; après l'orage `phase=evening goal=socialize`
  - `Anastasis.Sim.MeteoHabitants.TempsSec` : pluie 0,5 → `gate_fired=2 table_shelter=1 shelters=2`,
    chaque déclenchement de la porte retient un but exposé
  - `Anastasis.Sim.MeteoHabitants.CielDeLaSimulation` : jour 3, la météo lue = `readSimWeather(12345, …)`
  - Suite complète : voir la sortie `report-tests` du portail `finish`
- COMMANDS:
  - `node tools/migration/gen-parity.mjs weather-behavior.mjs`
  - `Automation RunTests Anastasis.Sim.MeteoHabitants+Anastasis.Sim.Parite.MeteoHabitants`
  - `py tools/unreal/village-weather-pie.py` (PIE, `Lvl_AnastasisSlice`)

## SCN

PIE `Lvl_AnastasisSlice`, `tools/unreal/village-weather-pie.py`, 2026-10-01 (temps simulé = réel) :

```
VILLAGE_WEATHER_BEFORE        t=45.833  eat|mange|building-0|eat|0          il mange au grenier
ForceWeather 0.9
VILLAGE_WEATHER_GOAL_SHELTER  t=48.000  after_storm=2.167  shelterRain       repas fini, il choisit l'abri
VILLAGE_WEATHER_SHELTERED     t=48.017  shelterRain|abrite|building-0|shelterRain
VILLAGE_WEATHER_LEFT_SHELTER  t=84.950  stayed=36.933  observer|-|1           = shelterRainDuration(0,9)
ForceWeather off
VILLAGE_WEATHER_AFTER         t=104.967 rest|dort                            la nuit est tombee
```

Premiere version du script : elle comptait « dedans » comme un abri, alors que le fermier etait au
grenier pour MANGER. Corrige : seul un interieur pour `shelterRain` compte (lecteur `GetNpcState`).

## PLY

UNKNOWN — PLAYER NOT_IMPLEMENTED.

## INTEGRATION_RISK

- Change les décisions de TOUT habitant en jeu (hôte de simulation) : il pleut, neige, vente
  désormais pour eux. Sans hôte (tests d'assemblage), aucune météo : comportement d'avant au bit près.
- `AnastasisVillage.cpp` est un fichier chaud du lot village.
- `agent/villager-png-001` (non intégrée) touche aussi `AnastasisSimulationSubsystem.cpp`
  (`anastasis.Sim.TimeScale`) : zones différentes, fusion à vérifier.
- Le digest JSON du village n'inclut pas `ShelterResumeGoal` / `ShelterCooldownUntil` (pour ne pas
  casser les digests existants) : deux trajectoires qui ne diffèrent que par eux auraient le même digest.

## STOP

Ne revendique pas : la pluie ou la neige visibles, le sol mouillé, la trajectoire JS au tirage près
(écart n° 1 du village), `bestKnownBed`, la taverne, les métiers d'extérieur absents du portage.
