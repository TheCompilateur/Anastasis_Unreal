# SKY_TRANSITIONS_001 — plus de flash au coucher, plus de journée de 12 secondes

Mission `sky-transitions-001`, 2026-09-30, suite de DAY_NIGHT_WEATHER_001. Retour d'Alexandre
sur la version intégrée : « la transition jour et nuit en 12 secondes est horrible, on dirait la
magie, et changement jour nuit = flash ».

## Deux causes, mesurées avant d'être corrigées

**1. Le flash venait de l'exposition, pas de la lumière.** Balayage du coucher du jour 1
(soleil à 0° à 18 h 00) de 17 h 36 à 18 h 48 par pas de 6 min, mêmes caméras
(`capture-sky.ps1 -Views valley_long,ov_sw`), luminance moyenne de chaque image :

| heure | 17h48 | 17h54 | 18h00 | 18h06 | 18h18 | 18h30 | 18h36 | 18h42 | 18h48 |
|---|---|---|---|---|---|---|---|---|---|
| vallée, première courbe | 141 | 155 | 171 | **183** | 176 | 114 | 74 | 46 | **24** |
| vue haute, première courbe | 177 | 183 | **192** | 190 | 187 | 110 | 84 | 61 | **24** |
| vallée, courbe finale | 115 | 114 | 115 | 116 | 88 | 62 | 45 | 35 | 37 |
| vue haute, courbe finale | 152 | 145 | 142 | 127 | 101 | 61 | 53 | 48 | 36 |

La première courbe (smoothstep d'exposition entre −12° et +10°) compensait plus que la lumière
ne baissait : **l'image s'éclaircissait au coucher du soleil**, puis s'effondrait d'un facteur 7
en 0,4 h de jeu. À `Sim.Speed` 10, ces 0,4 h duraient moins de deux secondes : un flash.

**2. La journée de 12 s venait de la vitesse de simulation.** `anastasis.Sim.Speed` valait 10
par défaut (« pour voir minuit dans un PIE court ») ; maintenant que le ciel suit la simulation,
le soleil balayait ~30°/s.

## Corrections

- **Courbe d'exposition calibrée** (`ExposureStopsBelowDay`, profil) : diaphragmes sous
  l'exposition de jour par élévation solaire, clés tirées de la mesure (lumière de scène =
  log2(luminance) + EV, vallée et vue haute, la plus claire gouverne) pour une image qui baisse
  d'environ 0,15 diaphragme par degré de soleil. Empirique, dépendant de la scène et du
  tonemapper — dit comme tel.
- **Adaptation de l'œil** (`MaxExposureChangePerSecond` = 3 EV par seconde réelle) : en jeu,
  l'exposition à l'écran suit la cible sans jamais sauter, quelle que soit la vitesse de
  simulation. Les captures (`Apply`) épinglent la valeur exacte de l'heure.
- **`anastasis.Sim.Speed` = 1 par défaut** : le temps réel de la référence JS, un jour = 90 s.
  `first-building-pie.py`, `house-rest-pie.py` et `granary-eat-pie.py`, établis à 10, posent
  désormais `anastasis.Sim.Speed 10` eux-mêmes (comme `gather-deliver-pie.py` le faisait déjà) :
  leurs conditions prouvées ne changent pas.
- `capture-sky.ps1 -Views` : sous-ensemble de vues, pour les balayages fins.

## Preuves

- `Anastasis.Sky.Clock.SunsetOnlyDarkens` : rejoue la mesure ; exige que la courbe actuelle
  n'éclaircisse aucune des deux vues de plus d'un quart de diaphragme (la dispersion de la mesure
  elle-même) et **que la première courbe échoue** sur les mêmes données.
- `Anastasis.Sky.Clock.EyeAdaptation` : aucun frame ne bouge l'exposition plus vite que 3 EV/s ;
  jour → nuit en 5,0 s réelles au minimum.
- PIE à vitesse 1 (`docs/visual/sky-transitions-001/sky-clock-pie-speed1.txt`) : le jour 1 se
  termine à 62 s réelles en partant de 10 h ; soir EV 12,4 → nuit EV −1.
- Planche avant / après : `docs/visual/sky-transitions-001/sunset_before_after.png`.

## Ce qui reste

- Vue haute : une légère remontée juste avant le coucher (123 → 152 entre 17 h 36 et 17 h 48,
  l'horizon s'embrase), bornée par le test.
- À 90 s par jour, un crépuscule entier dure ~5 s réelles : naturel en lumière, rapide en durée.
  Des journées plus longues demandent de toucher `DAY_LENGTH`, verrouillé par la parité JS —
  décision d'Alexandre.
- Les poches de brume restent des taches claires vues d'en haut au crépuscule.
