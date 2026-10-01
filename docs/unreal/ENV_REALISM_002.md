# ENV_REALISM_002 — le fond de l'eau, la pluie au sol, les aubes

Mission `env-realism-002` (branche `agent/env-realism-002`), 2026-10-01. Trois corrections,
toutes nommées par une fiche antérieure comme « ce qui reste faux » :

| # | Défaut | Source |
|---|---|---|
| 1 | le fond immergé est peint en bleu | `SHORELINE_FORGE_001`, limites n° 1 |
| 2 | `WeatherWetnessAt` est calculé, le sol ne le lit pas | `DAY_NIGHT_WEATHER_001`, n° 3 |
| 3 | aube et crépuscule roses à violets sur tout le cadre | `DAY_NIGHT_WEATHER_001`, n° 2 |

**Statut : écrit, NON BUILDÉ, NON TESTÉ, NON CAPTURÉ.** La session qui a écrit ce code tournait
dans un conteneur Linux, sans Unreal ni Windows. Rien ici n'est un `BUILD::PASS`, et aucun
gain visuel n'est revendiqué. La section *Preuves à produire* donne les commandes.

## 1. Le fond de l'eau n'est plus bleu

`AnastasisTerrainSurface::TileColor`, branche eau. La couleur de sommet d'une tuile d'eau est
celle de la **section 0, le relief sous la nappe**. La nappe est une autre section, avec son
propre matériau (`M_AnastasisShoreWater`, ou des couleurs constantes en mode historique) :
le bleu de l'ancien `ShallowWater` (0,10 / 0,36 / 0,43) n'était donc jamais de l'eau, c'était
un albédo de fond. Deux effets, tous deux visibles :

- la marge translucide de la nappe révélait du bleu au lieu du limon ;
- la forge interpole la couleur entre sommets fins : le bleu du premier sommet immergé
  débordait sur la berge émergée, une bande bleutée au-dessus du trait d'eau.

Nouveau fond, sans aucune donnée inventée (profondeur = `WaterDepthFromShade`, courant =
`FlowAmt`) :

| | RGB | Luminance |
|---|---|---|
| `ShallowBed` — limon saturé au bord | 0,135 / 0,118 / 0,082 | 0,119 |
| `DeepBed` — fond sombre, neutre | 0,040 / 0,041 / 0,036 | 0,040 |
| `ChannelBed` — gravier de lit courant | 0,120 / 0,115 / 0,100 | — |

`lerp(ShallowBed, DeepBed, depth^0.7)`, puis 55 % max vers `ChannelBed` selon le courant, comme
l'ancien mélange. Alpha reste 1 : c'est le marqueur « immergé » que lisent `M_AnastasisSlice`
(lisse, spéculaire), l'anneau d'horizon et la forge.

Effet de bord voulu : le drainage recolore la terre réparée par interpolation harmonique depuis
ses voisins, et l'anneau d'horizon fait la moyenne des couleurs de bord. Les deux tiraient du
bleu des sommets d'eau ; ils tirent maintenant du limon.

**Tests réécrits.** Trois tests exigeaient un fond « bleu dominant ». C'est exactement la règle
que cette mission retire, sur demande explicite d'Alexandre (2026-10-01) ; ils l'affirment
désormais à l'envers :

- `Anastasis.Terrain.Semantics` : `le lit immerge n'est pas peint en bleu` (alpha 1 conservé) ;
- `Anastasis.Terrain.HydrologyGradient` : lit stagnant = limon, lit courant = gravier, le
  courant éclaircit le lit ;
- `Anastasis.Terrain.SlopeShade` : le fond profond est sombre (luminance < 0,06) et pas bleu,
  le limon du bord reste dans la plage des sols humides (0,06–0,25).

## 2. La pluie mouille le sol

La référence décrit `weatherWetnessAt` comme « humidité visuelle partagée (sol, routes,
bâtiments) ». `FSkyState::Wetness` la portait déjà ; personne ne la lisait.

- **Canal** : une Material Parameter Collection, `/Game/Anastasis/Materials/MPC_AnastasisWeather`,
  scalaire `RainWetness` (défaut 0 = sol sec, l'image d'avant). Une collection et pas un paramètre
  d'instance : le sol est posé par l'incarnation, la pluie est connue de l'atmosphère, et les
  routes ou bâtiments à venir liront la même valeur.
- **Écrivain** : `AAnastasisWorldAtmosphere::UpdateSky` →
  `AnastasisSkyClock::GroundWetnessFor`, à chaque tick, écriture sautée si la valeur n'a pas bougé.
  Collection absente : une ligne `ANASTASIS_ATMOSPHERE ground_wetness_unavailable` en Display
  (pas en Warning, pour ne pas polluer la base de référence des logs), sol sec, aucun crash.
- **Lecteur** : `M_AnastasisGround`, régénéré par `tools/unreal/ground-material.py`.
  - albédo × `RainDarken` (0,70), pondéré par la porosité : la roche n'en prend que
    `RainRockPorosity` (0,40) ;
  - rugosité vers `RainRoughness` (0,30) : en plein sur le plat (N.z 0,80 → 0,97), à
    `RainSlopeGloss` (0,55) sur les pentes, où le film d'eau ruisselle.
  
  Ce sont des paramètres d'instance, retouchables dans `MI_AnastasisGround` sans régénérer.
- `anastasis.Sky.GroundWetness <0..1>` épingle la valeur pour une capture ; −1 (défaut) suit la
  météo. Profil : `bWeatherWetsGround`, `WeatherParameterCollection`.

**Le maître committé ne lit pas encore la collection.** Il faut le régénérer (voir plus bas).
Tant que ce n'est pas fait, le C++ écrit dans une collection que personne ne lit, ou ne trouve
pas de collection : le sol reste exactement comme avant.

## 3. Les aubes ne sont plus roses

Lu sur `docs/visual/day-night-weather-001/ov_sw_d1h06.png` et `valley_long_d1h1830.png` :

- **6 h** : les poches de brume, à pleine force au lever (brouillard de rayonnement), sont des
  boules blanches éclairées par un soleil rougi. C'est ce qu'il y a de plus rose dans l'image.
  Le voile rose couvre le reste.
- **18 h 30** (soleil à −5,7°) : tout le cadre est **violet**. Cause trouvée dans le code : la
  balance des blancs de nuit suit la courbe d'exposition, donc elle valait déjà 4 584 K à −5,7°.
  Une balance des blancs vers le bleu, posée sur un crépuscule rose, donne du magenta. Le blanc
  de la lune s'appliquait alors que la lune n'éclairait pas encore le paysage.

La lumière reste physique. Seuls le milieu et l'étalonnage changent, et seulement dans une
**bande crépusculaire** (`AnastasisSkyClock::TwilightFor`) : 0 sous −12°, plein de −4° à +6°,
0 au-dessus de +14°, smoothstep sur les deux bords. À pleine bande :

| Levier | Multiplicateur | Profil |
|---|---|---|
| brume locale | × 0,5 | `TwilightMistScale` |
| densité du brouillard de hauteur (donc du volumétrique) | × 0,6 | `TwilightFogDensityScale` |
| aérosol : Mie et perspective aérienne (couche de réalisme seulement) | × 0,5 | `TwilightAerosolScale` |
| saturation post-process | × 0,85 | `TwilightColorSaturation` |

Et, hors bande : le blanc de la lune (`NightWhiteTemp`) n'entre plus qu'entre −4° et −12°
(`NightWhiteBalanceStartDegrees`). Valeurs calculées pour l'équinoxe, vérifiées par le test :

| | élév. | bande | blanc (avant) | saturation (avant) |
|---|---|---|---|---|
| 6 h | 0,0° | 1,00 | 6 500 K (5 463) | 0,648 (0,762) |
| 18 h 30 | −5,7° | 0,89 | 6 235 K (4 584) | 0,486 (0,561) |

**Le jour et la nuit ne bougent pas, par construction.** Hors bande, chaque valeur touchée est
identique au bit près à `DAY_NIGHT_WEATHER_001`. `Anastasis.Sky.Clock.TwilightIsABand` le vérifie
sur un an d'instants. Les captures de jour et de nuit déjà versées restent donc valides.

A/B : `anastasis.Sky.Twilight 0` rend le crépuscule d'avant (`AnastasisSkyClock::WithoutTwilight` :
pas de bande, saturation et blanc sur la courbe d'exposition).

Le journal `ANASTASIS_SKY` porte maintenant `twilight= saturation= white_temp= ground_wetness=`.

## Fichiers

| Fichier | Changement |
|---|---|
| `WorldView/AnastasisTerrainSurface.cpp/.h` | fond immergé |
| `WorldView/AnastasisTerrainSurfaceTests.cpp` | trois tests réécrits (voir § 1) |
| `WorldView/AnastasisAtmosphereProfile.h` | bande crépusculaire, blanc de lune, sol mouillé |
| `WorldView/AnastasisSkyClock.cpp/.h` | `TwilightFor`, `TwilightScale`, `WithoutTwilight`, `GroundWetnessFor`, `FSkyState::Twilight` |
| `WorldView/AnastasisSkyClockTests.cpp` | `TwilightIsABand`, `RainWetsTheGround` |
| `WorldView/AnastasisWorldAtmosphere.cpp/.h` | bande appliquée, aérosol, MPC, CVars `anastasis.Sky.Twilight` / `Sky.GroundWetness` |
| `tools/unreal/ground-material.py` | `MPC_AnastasisWeather`, réponse du sol à la pluie |

Aucun fichier de `Source/AnastasisSim/` n'est touché. Aucun `.uasset` n'est committé : ils
n'ont pas pu être générés ici.

## Preuves à produire (Windows, worktree `env-realism-002`)

```powershell
tools\unreal\agent-worktree.ps1 create -Mission env-realism-002   # puis checkout de la branche poussee
tools\unreal\anastasis-unreal.ps1 build
tools\unreal\report-tests.ps1
# Regenere M_AnastasisGround / MI_AnastasisGround et cree MPC_AnastasisWeather -- puis les committer
tools\unreal\ground-material.ps1 -Rebuild
# Aube et crepuscule, A/B sur la seule bande
tools\unreal\capture-sky.ps1 -Label twilight-002 -States 'h06old=anastasis.Sky.Hour 6;anastasis.Sky.Twilight 0|h06=anastasis.Sky.Hour 6;anastasis.Sky.Twilight 1|h1830old=anastasis.Sky.Hour 18.5;anastasis.Sky.Twilight 0|h1830=anastasis.Sky.Hour 18.5;anastasis.Sky.Twilight 1|h10=anastasis.Sky.Hour 10'
# Sol sec / mouille, meme heure
tools\unreal\capture-sky.ps1 -Label rain-ground-002 -States 'dry=anastasis.Sky.Hour 10;anastasis.Sky.GroundWetness 0|wet=anastasis.Sky.Hour 10;anastasis.Sky.GroundWetness 1'
# Fond de l'eau : la rive, avant (main) / apres (cette branche)
tools\unreal\shore-capture.ps1
```

Tests à lire dans le rapport :

- `Anastasis.Terrain.{Semantics,HydrologyGradient,SlopeShade}` ;
- `Anastasis.Sky.Clock.{TwilightIsABand,RainWetsTheGround,FogAndMistFollowTheLight,SeasonsAndExposure}` ;
- `Anastasis.Atmosphere.Realism.Reversible` : il compare Mie et perspective aérienne aux valeurs
  du profil après `Apply()`. Il passe parce que l'éditeur montre 10 h 05, hors bande (aérosol × 1).
  Il échouerait sous un `anastasis.Sky.Hour` épinglé à l'aube.

## Risques

- **Réglages à l'aveugle.** Les multiplicateurs de la bande ont été choisis sur l'analyse des deux
  captures, pas sur un A/B. Ils sont dans le profil, donc retouchables sans recompiler.
- **Le crépuscule peut paraître trop terne.** La saturation de vision de nuit et celle du
  crépuscule se multiplient : 0,49 à 18 h 30. À juger sur capture ; levier `TwilightColorSaturation`.
- **`MaterialExpressionCollectionParameter` via Python** : la collection doit être posée avant le
  nom (le GUID se résout à la réception du nom). Si ce n'est pas le cas, le script lève sur
  `recompile_material`, et aucun asset n'est sauvegardé.
- **Coût** : une lecture de collection et quelques instructions de plus dans le matériau de sol,
  non mesurés. `ground-material.ps1` rapporte les `STATS`.
