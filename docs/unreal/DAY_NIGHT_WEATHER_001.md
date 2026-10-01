# DAY_NIGHT_WEATHER_001 — le ciel suit l'horloge et la météo de la simulation

Mission `env-realism-001` (branche `agent/env-realism-001`), 2026-09-30. Commencée comme
passe de réalisme environnemental ; réorientée par Alexandre sur le jour, la nuit et le
temps (« now ça n'a aucun sens »).

## Ce qui n'avait pas de sens

Le village et le ciel vivaient sur deux horloges.

| | Avant |
|---|---|
| Simulation | `DAY_LENGTH = 90 s`, première image ~10 h, le village dort de 21 h à 5 h (`AnastasisRhythm`) |
| Soleil | figé par le profil à pitch −38 / yaw −55, quelle que soit l'heure |
| Exposition | épinglée à EV100 14 : une nuit aurait été noire, mais il n'y avait pas de nuit |
| Météo | aucune dans Unreal ; la référence JS (`src/sim/weather.js`) en calcule une chaque jour et la simulation la lit (`weather.rain > 0.2`) |
| Lune, nuages | aucun |

Le village se couchait donc sous un soleil de midi.

## Ce qui est livré

### 1. La météo est une vérité de simulation — `AnastasisSim`

`Source/AnastasisSim/{Public,Private}/World/AnastasisWeather.*` : portage de `weather.js`
(`weatherAt`, front intrajournalier, éclaircie après orage, pluie, neige et givre d'hiver,
vent, `weatherWetnessAt`, `weatherHumidityAt`) et de `fieldSeasonFromDay` (année de 120 jours,
quatre saisons de 30). Rien n'est inventé côté Unreal : le ciel montre le front que la
simulation lit.

Parité `Anastasis.Sim.Parite.Meteo`, **1112 vecteurs** générés depuis la référence exécutée
(`tools/migration/parity/weather.mjs`), 12 212 valeurs comparées :

```
ANASTASIS_WEATHER_PARITY compared=12212 not_bit_exact=39 max_ulp=1 failures=0
```

Les 39 valeurs à 1 ULP sont toutes `dirX` / `dirZ` (Math.cos / Math.sin : V8 ne les prend pas
au CRT MSVC — même cause que la divergence connue de `Parite.Fbm`). Tout le reste est au bit.

**Premier run : 2 échecs** (`cover` à 8 ULP, `rain` à 6 ULP). `cover = base + amp·(lobe·1.2 − 0.42)`
soustrait deux grandeurs voisines et amplifie l'ULP d'écart de `Math.exp` entre le CRT MSVC et
V8. Correction : `AnastasisJs::Exp`, portage de l'exp de fdlibm (celle de V8, `base/ieee754.cc`,
cas `exp(1) = E` compris). La tolérance n'a pas été élargie ; elle a été resserrée.

`Anastasis.Sim.Meteo.Invariants` : saisons aux jours 1/31/61/91/121, sorties bornées, neige et
givre seulement en hiver, déterminisme — deux ans, toutes les heures.

### 2. Une seule horloge — `AnastasisSkyClock` (pur)

`Source/Anastasis_UnrealV2/WorldView/AnastasisSkyClock.*`. Du temps de simulation au ciel, sans
acteur ni état moteur :

- heure = `AnastasisRhythm::DayFracOf(time) × 24` — la fraction même qui décide « nuit » pour le
  village ;
- déclinaison = 23,44° × sin(2π (jour − 1) / 120) : jour 1 équinoxe, 31 solstice d'été,
  91 solstice d'hiver ;
- soleil = `SunRotationForTimeOfDay` existant (41° N) ; lune pleine = heure + 12 h, déclinaison
  opposée (approximation déclarée : pas de phase, pas d'orbite) ;
- exposition épinglée par élévation solaire : EV 14 au-dessus de +10°, EV −1 sous −12°,
  smoothstep entre — déterministe, deux captures à la même heure restent comparables ;
- vision de nuit sur la même courbe : saturation 1 → 0,45, balance des blancs 6500 K → 4100 K
  (la lune rendue neutre, ni ocre ni bleue) ;
- météo = `WeatherAt(seed, jour, null, dayFrac)`, l'appel du rendu de la référence.

### 3. Le ciel dans le monde — `AAnastasisWorldAtmosphere`

L'acteur tick en PIE et écrit à chaque frame : soleil et lune (orientation, ombres : le soleil
n'en paie plus sous l'horizon, la lune seulement quand elle est levée et le soleil couché),
exposition et vision de nuit, couverture nuageuse (`Cloud_GlobalCoverage` d'une instance
dynamique du nuage moteur), densité du brouillard (× 1 + 1,5·humidité), luminance
d'inscattering du brouillard (× 2^(EV − EV_jour)), brume.

| CVar | Rôle |
|---|---|
| `anastasis.Sky.Clock` | 1 (défaut) le ciel suit la simulation ; 0 le soleil fixe du profil (le rig d'observation) |
| `anastasis.Sky.Hour` | épingle l'heure **du ciel** pour une capture (−1 = suivre) ; ne touche jamais l'horloge de simulation |
| `anastasis.Sky.Day` | épingle le jour **du ciel** (saison, météo) ; idem |
| `anastasis.Sky.Weather` | 0 = ciel de beau temps fixe |
| `anastasis.Atmosphere.Realism` | couche de réalisme (ciel, nuages volumétriques, lune, brume de vallée, brouillard volumétrique) ; 0 = valeurs moteur, image d'avant |

L'éditeur n'a pas d'horloge : il montre l'instant `DAY_LENGTH × 0,42` de la référence (10 h 05,
jour 1), la première image du PIE.

### 4. La brume naît la nuit et brûle au soleil

Les poches de brume d'ATMOSPHERE_002 (où : champ `Wetness` de la simulation) gardent leur
emplacement ; l'horloge décide quand et combien. C'est du brouillard de rayonnement : pleine
force soleil couché, 15 % soleil au-dessus de 25°, × (1 + humidité), × (1 − 0,6·vent). Sous
l'horloge une poche est aplatie (hauteur = 0,3 × rayon) : un banc, pas une coupole.

## Preuves

Build : `BUILD::PASS` (worktree `env-realism-001`, après fusion de `main` à `9175a37`).

Tests du chantier, tous `Success` (run `-nullrhi`, 2026-09-30) :

```
Anastasis.Sim.Parite.Meteo                       compared=12212 not_bit_exact=39 max_ulp=1 failures=0
Anastasis.Sim.Meteo.Invariants
Anastasis.Sky.Clock.OneClock                     58 000+ instants sur deux ans : 0 désaccord ciel / village
Anastasis.Sky.Clock.VillageSleepsInTheDark       max_night_sun_elev=4.69 min_midday_sun_elev=23.25 max_night_ev=11.80 min_midday_ev=14.00
Anastasis.Sky.Clock.SeasonsAndExposure           midi d'équinoxe 49,0°, vision de nuit neutre le jour
Anastasis.Sky.Clock.WeatherIsTheSimulations      rainy_instants_over_a_year=749
Anastasis.Sky.Clock.FogAndMistFollowTheLight     ANASTASIS_SKY_MIST dawn=0.719 noon=0.150
Anastasis.Atmosphere.{RigParity,SunFromTime,ProfileFallback,Idempotence,CachedLightingPreExposure}
Anastasis.Atmosphere.Realism.{MoonIsNotTheSun,Reversible,MoonGeometry}
Anastasis.Mist.{Causality,Threshold,DeterminismAndBounds,Refusals}
```

Le rapport complet PASS / KNOWN_EXPECTED_FAILURE / FAIL est celui du portail `finish`
(fiche de passation).

**PIE** (`tools/unreal/sky-clock-pie.py`, `Lvl_AnastasisSlice`, 108 s réelles, 10 jours de
simulation) : `docs/visual/day-night-weather-001/sky-clock-pie.txt`. Chaque changement de phase
du village porte son ciel — 21 h 01 `night` soleil −32°, EV −1 ; 5 h 01 `dawn` ; déclinaison
0 → 10,6° en dix jours ; pluie la nuit du jour 4 (humidité 0,70), jour 8 pluvieux du matin au
soir, comme dans la référence.

**Captures** (`tools/unreal/capture-sky.ps1`, mêmes caméras, heure et jour épinglés) :
`docs/visual/day-night-weather-001/`. Regardées une par une. Constaté :

- 10 h 00 : vallée lisible (rive, arbres, ombres, voile sur les collines lointaines) — au premier
  passage la même caméra était dans un blanc, la brume ne brûlait pas ;
- 23 h 00 : nuit sombre et désaturée, banc de brume bas, ombre de lune — au deuxième passage la
  même heure se lisait comme un jour ocre terne, et au premier le brouillard y luisait (bande
  blanche à l'horizon) ;
- jour 8 pluvieux : ciel couvert gris ; jour 94 : ciel d'hiver chargé ; jour 34 : ciel d'été clair.

## Ce qui reste faux — dit, pas masqué

1. **Un jour dure ~12 s réelles en PIE.** `anastasis.Sim.Speed` vaut 10 par défaut (« pour voir
   minuit dans un PIE court ») et `DAY_LENGTH = 90` est verrouillé par la parité JS. Le ciel suit
   maintenant fidèlement : le soleil balaie ~30°/s. Décision de game design, pas de rendu :
   vitesse par défaut à 1 (jour de 90 s, comme la référence), ou `DAY_LENGTH` plus long (rompt la
   parité). **Non tranché ici.**
2. **Aube et crépuscule trop roses** (6 h 00, 18 h 30) : le brouillard, la brume à pleine force
   et l'aérosol prennent la couleur de l'horizon et la répandent sur tout le cadre.
   *Repris par `ENV_REALISM_002.md` (bande crépusculaire, blanc de lune différé) — non capturé.*
3. **Pas de pluie ni de neige visibles** : le projet n'a aucun système Niagara à lui ; la pluie
   se voit au ciel couvert et à l'humidité, pas en gouttes. `WeatherWetnessAt` est calculé mais
   pas encore câblé au matériau de sol.
   *Câblé par `ENV_REALISM_002.md` (`MPC_AnastasisWeather`) — matériau à régénérer.*
4. Vues d'en haut : les poches de brume restent des taches à l'aube et au crépuscule.
5. Coût GPU de la couche (nuages et brouillard volumétriques, soleil mobile qui invalide les
   ombres virtuelles chaque frame) : **non mesuré**.

## Hors périmètre de cette passe

Hydrologie des berges et distribution écologique (phases 7–8 du brief d'origine) : le monde est
un maillage procédural piloté par la simulation, sans Landscape ; `PCG` et `Water` ne sont pas
activés dans le projet, et les voies `hydro-network`, `ecotone-forge`, `lithos_forge` et
`macro-forest` tiennent ces sujets. Les assets Ecotone / Lithos / Rock existent (inventaire du
2026-09-30) ; les placer relève de ces voies.
