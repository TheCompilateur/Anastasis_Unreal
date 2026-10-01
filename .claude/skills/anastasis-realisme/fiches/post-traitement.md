# Post-traitement

## Unreal

- Le **PostProcessVolume** (global si `Unbound`) porte l'exposition, le tonemapper, le bloom, la
  correction colorimétrique, la LUT, la profondeur de champ, l'occlusion ambiante, le vignettage, le flou
  de mouvement.
- **Tonemapper** : il compresse la lumière HDR de la scène vers l'écran. Celui d'Unreal est filmique par
  défaut (courbe type ACES) : les hautes lumières se tassent au lieu de couper net.
- **Exposition locale** (`LocalExposure`) : compresse séparément les hautes et basses lumières, comme
  l'œil ou un HDR photo ; trop forte, elle aplatit l'image.
- **Bloom, LUT, vignettage, aberration chromatique, flares** : des effets de caméra. Ils ne rendent pas
  une matière plus vraie ; ils peuvent masquer qu'elle est fausse.
- La règle des studios qui visent le photoréel : régler la scène en lumière neutre (exposition fixe,
  post-traitement minimal) jusqu'à ce qu'elle tienne, et ne graduer qu'ensuite.

## ANÁSTASIS aujourd'hui

`AAnastasisWorldAtmosphere` crée un PostProcessVolume global. Il n'y règle que :

| Quoi | Valeur |
|---|---|
| Exposition | fixe, EV100 14 le jour, suivant la courbe `ExposureStopsBelowDay` la nuit (`fiches/eclairage.md`) |
| Saturation et température de blanc | baissées la nuit (vision nocturne : la couleur s'efface, le blanc suit la lune) |
| Exposition locale | 0,8 / 0,8 le jour (`DefaultEngine.ini`) ; contraste des hautes lumières baissé au crépuscule |

Le projet ne règle **ni** tonemapper, **ni** bloom, **ni** LUT, **ni** DOF, **ni** AO : ce sont les défauts
du moteur. Les scripts de capture coupent le flou de mouvement (`r.MotionBlurQuality 0`).

## Règles

- **POST-01** — Pas de LUT ni de bloom pour contrefaire la qualité des matériaux
  (`P1_6_PONTIC_BYZANTINE_ART_DIRECTION.md`). Une image fausse se corrige dans la scène.
- **POST-02** — Pas de profondeur de champ ni d'auto-exposition dans les captures de preuve : elles
  changent l'image d'un run à l'autre et faussent l'A/B.
- **POST-03** — Un réglage de post-traitement passe par `AAnastasisWorldAtmosphere` (code ou profil),
  jamais par un volume posé à la main dans la map.
- **POST-04** — Un profil de lookdev (bloom 0,35, `r.ScreenPercentage`…) s'applique **dans la console**
  de l'éditeur du labo, jamais dans `DefaultEngine.ini` (`AAA_VISUAL_TARGET_LAB.md`).
- **POST-05** — L'exposition locale peut aplatir un matériau peu contrasté : la baisser ou la monter est
  un A/B, pas un réglage de goût.

## Vérifier

A/B par `-PreCmds` (skill `anastasis-capture`), puis `atmosphere-metrics.py` pour les drapeaux
`CLIPPED` (hautes lumières coupées) et `BLACK` (noirs bouchés).

## Ne pas faire

- « Bloom à 50 % », « LUT cinématique », « vignettage léger » appliqués d'office : interdits par la
  direction artistique tant que la scène n'est pas juste sans eux.
- Activer l'eye adaptation pour « cinématiser » : elle casse l'exposition fixe (`ECL-01`).

## Ouvert

- Une gradation finale, une fois la scène juste : décision de direction artistique, pas d'un agent.
