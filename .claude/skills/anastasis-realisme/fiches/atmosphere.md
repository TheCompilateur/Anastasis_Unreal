# Ciel, atmosphère, brouillard, nuages

## Unreal

- **SkyAtmosphere** simule la diffusion de la lumière par l'air (Rayleigh, qui donne le bleu) et par les
  aérosols (Mie, qui donne la brume blanche et le halo du soleil). Elle produit la couleur du ciel, le
  lever et le coucher, et la **perspective aérienne** : les lointains bleuissent et pâlissent. C'est le
  premier indice de profondeur d'un paysage.
- **ExponentialHeightFog** : un brouillard dense en bas, qui s'éclaircit avec l'altitude
  (`Density`, `HeightFalloff`, `StartDistance`, `MaxOpacity`). Mal réglé, il fait un **mur** : un aplat
  clair qui couvre le sol au lieu de le révéler.
- **Volumetric Fog** : le même brouillard, éclairé par les lumières, avec des ombres volumétriques. Il
  donne les rayons à travers les arbres, et coûte cher.
- **Local Fog Volumes** : des nappes locales (brume de fond de vallée, de rivière).
- **VolumetricCloud** : des nuages en volume, raymarchés, éclairés par le soleil et qui projettent leur
  ombre au sol.
- Le soleil sous l'horizon diffuse encore dans le brouillard si on ne coupe pas sa diffusion
  volumétrique : c'est le « flash » ou la lueur orange à minuit.

## ANÁSTASIS aujourd'hui

Un seul acteur pose tout : `AAnastasisWorldAtmosphere`. Il adopte ou crée le soleil, la SkyAtmosphere,
le SkyLight, l'ExponentialHeightFog, un PostProcessVolume global, la lune (tag `AnastasisMoon`), les
nuages et jusqu'à 192 `ALocalFogVolume` de brume. Les valeurs viennent de `DA_AnastasisAtmosphere`
(script d'autorité `tools/unreal/atmosphere-profile.py`), avec des valeurs de repli codées dans
`AnastasisAtmosphereProfile.h`. Le temps est donné par `AnastasisSkyClock`, la météo par
`AnastasisWeather` (portage à parité de `weather.js`).

| Quoi | Valeur (`AnastasisAtmosphereProfile.h`) |
|---|---|
| Brouillard | densité 0,012, falloff 0,2, départ 1 500 uu, opacité max 0,85 |
| Brouillard de vallée | densité 0,006, falloff 2,0 |
| Brouillard volumétrique | distance 6 000 uu, distribution de diffusion 0,3 |
| Ciel | diffusion de Mie × 0,0065, perspective aérienne × 3 |
| Nuages | base 1,5 km, épaisseur 3 km, couverture −0,55 (clair) à 0,45 (couvert) |
| Brume locale | extinction max 0,65, seuil d'humidité 0,35, se dissipe au-dessus de 25° d'élévation solaire |
| Diffusion du soleil dans le brouillard | 0 sous l'horizon, pleine à +4° |

CVars : `anastasis.Atmosphere`, `anastasis.Atmosphere.Mist`, `anastasis.Atmosphere.Realism`,
`anastasis.Sky.Clock`, `anastasis.Sky.Weather` (1 par défaut) ; `anastasis.Sky.Hour`, `anastasis.Sky.Day`,
`anastasis.Sky.Humidity` (−1 = suit la simulation, sinon épingle l'état pour une capture).

## Règles

- **ATM-01** — Le brouillard révèle l'échelle, il ne cache pas un décor faible (direction artistique,
  « cinematic restraint »). Un brouillard qui couvre le sol est une erreur, pas une ambiance.
- **ATM-02** — Une seule lumière directionnelle principale ; la diffusion volumétrique du soleil est nulle
  sous l'horizon (`ATMOSPHERE_COHERENCE_001.md`). Toute nouvelle source de lumière céleste passe par
  `AAnastasisWorldAtmosphere`.
- **ATM-03** — La brume a un albédo de matière réelle (0,60 à 0,68), pas de blanc pur (anciennement 0,86 à
  0,94, ce qui faisait le voile laiteux).
- **ATM-04** — La météo change sans palier : les transitions sont fondues (4 h autour de minuit). Aucun
  réglage ne doit réintroduire de saut d'une image à l'autre.
- **ATM-05** — Pas de rayons divins partout, pas de noirs écrasés, pas de sursaturation
  (`P1_6_PONTIC_BYZANTINE_ART_DIRECTION.md`).
- **ATM-06** — Une valeur d'atmosphère se change dans `atmosphere-profile.py` (et, si elle sert de repli,
  dans `AnastasisAtmosphereProfile.h`), jamais à la main sur l'acteur.

## Vérifier

```powershell
tools\unreal\capture-sky.ps1 -Label <avant|apres> -Preset cycle     # 06 09 12 16 18 20 00 03 × sec / humide / saturé
python tools\unreal\atmosphere-metrics.py Saved\SkyEvidence\<Label>  # hors éditeur
```

Drapeaux d'`atmosphere-metrics.py`, à zéro sur un état sain :

| Drapeau | Seuil | Défaut |
|---|---|---|
| `WALL` | blocs clairs > 35 % des deux tiers bas, p50 > 150 | mur de brouillard |
| `HAZE` | tiers médian sans relief (depth < 4), p50 > 80 | voile uniforme |
| `CLIPPED` | plus de 20 % de pixels à luma > 235 | ciel ou brume cramés |
| `BLACK` | p99 < 12 | nuit bouchée |

Seuils empiriques, posés sur les captures de `DAY_NIGHT_WEATHER_001`. Autres preuves :
`sky-clock-pie.py` (horloge en PIE), `capture-horizon.ps1` (lointains).

## Ne pas faire

- Monter la densité du brouillard pour « faire profond » : c'est le mur (`WALL`).
- Laisser le soleil diffuser sous l'horizon : flash au coucher.
- Croire un réglage d'atmosphère validé sans l'avoir vu : `ATMOSPHERE_COHERENCE_001.md` note lui-même
  que l'aube, le coucher et l'aspect du brouillard n'ont **pas été observés** en moteur après sa
  correction.

## Ouvert

- Pluie et neige invisibles : pas de Niagara (`handoffs/env-realism-001.md`). Mission à part.
- Seuils de `atmosphere-metrics.py` à recaler sur un plus grand nombre de captures.
