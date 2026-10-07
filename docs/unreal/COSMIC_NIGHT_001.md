# COSMIC_NIGHT_001 — une nuit à attendre

## Intention

Les trois références d'Alexandre (lune cratérisée violette, voie lactée dense,
dôme composé en couches) guident l'image. Le ciel ne doit pas être un décor
figé. Un joueur doit pouvoir guetter une nuit à météores et être surpris, plus
rarement, par un voile céleste étrange. L'événement n'est pas une récompense
pour un clic ni une réaction à la caméra : il appartient au calendrier du monde.

## Autorités

- `AnastasisSkyClock` fournit le jour, l'heure, le soleil, la lune et la météo.
- `AnastasisCosmicNight` choisit la nuit à partir du seed et du jour de son soir.
  Le même soir avant et après minuit conserve son plan. Cette couche ne touche
  ni la simulation, ni son RNG, ni le relief.
- `AAnastasisWorldAtmosphere` est le seul propriétaire du dôme transitoire et
  du matériau dynamique. Il peut apparaître dans toute carte qui incarne le
  monde : aucun nom de `.umap` n'est codé dans le rendu.
- `ArtSource/Celestial/moon_lavender.png` et `cosmic_river.png` sont des assets
  originaux générés pour cette direction. `tools/unreal/cosmic-sky-material.py`
  les importe ; la rivière panoramique se mêle à des étoiles, météores et
  voiles procéduraux dans un matériau Unreal.

## Calendrier initial

Un tirage déterministe par nuit choisit environ 13 % de nuits à météores et
2 % de nuits à voile. Certaines nuits ordinaires portent une seule étoile
filante. Une nuit à météores en programme trois à cinq entre environ 22 h et
4 h ; chacune dure 0,30 heure simulée (~1,1 s au rythme normal de 90 s/jour).
Le voile se lève lentement de 22 h à 23 h, reste jusqu'à 3 h et disparaît vers
4 h. Nuages, pluie et crépuscule en réduisent la visibilité sans changer le
tirage de la nuit. Ce sont des valeurs de départ, pas encore un équilibre de jeu.

## Commandes de diagnostic

- `anastasis.Sky.Cosmic 0/1` : A/B global, restaure l'ancien ciel à 0.
- `anastasis.Sky.CosmicEvent -1/0/1/2` : calendrier / ordinaire / météores /
  voile, uniquement pour l'observation.
- `anastasis.Sky.CosmicIntensity` : intensité bornée 0..3, sans exposition auto.
- `anastasis.Sky.Day`, `.Hour`, `.Cover`, `.Rain` : épingles déjà existantes.

## Frontière de preuve

Un build et les tests du calendrier prouveront les règles, pas la beauté. Le
matériau compilé prouvera son chargement, pas sa lisibilité depuis le joueur.
Alexandre a dispensé cette mission des captures le 2026-10-07. Le verdict
artistique reste donc ouvert. Si on le reprend, `capture-sky.ps1 -Level` peut
tester la carte courante et une seconde carte sans modifier le ciel.
