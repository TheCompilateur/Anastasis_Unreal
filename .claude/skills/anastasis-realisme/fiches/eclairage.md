# Éclairage et ombres

## Unreal

- **Lumen** calcule la lumière indirecte (GI) et les réflexions en temps réel. Il trace des rayons de
  deux façons : en **logiciel**, contre les champs de distance des meshes, ou en **matériel**, par le ray
  tracing du GPU. Il n'y a pas de mode « Surfel » dans Lumen.
- En ray tracing matériel, la lumière au point d'impact d'un rayon se lit soit dans le **cache de
  surface** (« cartes Lumen », `r.Lumen.HardwareRayTracing.LightingMode=0`, défaut, rapide), soit en
  l'évaluant au point d'impact (**Hit Lighting**, `=1`, plus cher, plus juste).
- **Une géométrie sans cartes Lumen est noire pour le cache.** Lumen retire alors le ciel qu'elle masque
  sans rendre la lumière qu'elle renvoie : il **assombrit** au lieu d'éclairer.
  Diagnostic : `r.Lumen.Visualize 3` (cache de surface), `r.Lumen.Visualize 5`.
- Lumen veut un éclairage **entièrement dynamique** : lumières `Movable`, SkyLight `Movable` en capture
  temps réel, pas de lightmaps. Un SkyLight `Stationary` est un réflexe de l'éclairage précalculé.
- **Lumen Lite** (5.8, Beta) : mode de GI annoncé deux fois plus rapide que Lumen en haute qualité, pensé
  pour les consoles et le moyen de gamme. C'est l'**Irradiance Field Gather** (`r.Lumen.FinalGatherMethod
  0`, que la scalabilité pose au niveau GI 1 : `sg.GlobalIlluminationQuality 1`), contre le Screen Probe
  Gather (`1`, défaut). Ses reflets ne tracent pas de rayons. Une voie de coût, pas de réalisme ; rien ne
  l'a mesuré ici (RU-002-01). `2` = ReSTIR, ray tracing matériel seulement, non documenté (RU-002-02).
- **Virtual Shadow Maps (VSM)** : ombres de très haute résolution (16k virtuels par niveau de clipmap)
  qui remplacent les cascades et suivent le détail géométrique. Il n'y a pas de « taille max » à régler :
  la netteté se règle par biais de résolution (`r.Shadow.Virtual.ResolutionLodBiasDirectional`). La
  douceur vient de la taille apparente de la source (`Source Angle` du soleil), échantillonnée par SMRT.
- **Exposition physique** : un soleil de midi d'environ 75 000 à 100 000 lux correspond à EV100 ≈ 14 à 15
  (« règle du f/16 »). Avec une exposition fixe, une valeur d'albédo a un sens. L'auto-exposition, elle,
  compense, et masque donc les erreurs d'albédo et d'intensité.

## ANÁSTASIS aujourd'hui

| Quoi | Valeur | Où |
|---|---|---|
| Méthode de GI | Lumen (`r.DynamicGlobalIlluminationMethod=1`), réflexions Lumen | `Config/DefaultEngine.ini` |
| Ray tracing | matériel, `r.RayTracing=True` | idem |
| Éclairage des impacts | **Hit Lighting**, `r.Lumen.HardwareRayTracing.LightingMode=1` | idem, `handoffs/lumen-hit-lighting-001.md` |
| Lumière statique | interdite, `r.AllowStaticLighting=False` | idem |
| Ombres | VSM, `r.Shadow.Virtual.Enable=1` | idem |
| Shading | Substrate, `r.Substrate=True` | idem |
| Soleil | `Movable`, 75 000 lux, 5 900 K | `AnastasisAtmosphereProfile.h` (`SunIntensityLux`, `SunTemperatureKelvin`) |
| Ombre des nuages au sol | 0,6 | idem (`SunCloudShadowStrength`) |
| Lune | 0,3 lux, 4 100 K | idem |
| SkyLight | `Movable`, capture temps réel, intensité 1,0 | idem (`SkyLightIntensity`) |
| Exposition | **fixe** : histogramme avec min = max = EV100 14 ; `r.EyeAdaptation.CachedLightingPreExposure` = 14 posé par le code | `AnastasisWorldAtmosphere.cpp` |
| Exposition locale | contraste hautes et basses lumières 0,8 | `DefaultEngine.ini` |
| Lumière directionnelle principale | une seule à la fois : le soleil quand il est levé, la lune après | `ATMOSPHERE_COHERENCE_001.md` |

Tout l'éclairage est posé par `AAnastasisWorldAtmosphere` à partir de `DA_AnastasisAtmosphere`
(`tools/unreal/atmosphere-profile.py`). Une retouche dans l'éditeur ne survit pas.

## Règles

- **ECL-01** — L'exposition de jour reste fixe à EV100 14. Une image trop claire ou trop sombre se
  corrige à la source : albédo (`fiches/sol.md`), intensité, densité de brouillard. On ne réactive pas
  l'auto-exposition.
- **ECL-02** — Toute lumière ajoutée est `Movable`. Pas de lightmap, pas de SkyLight `Stationary`.
- **ECL-03** — Le Hit Lighting reste en place tant que le terrain (`ProceduralMeshComponent`) et les arbres
  (HISM) n'ont pas de cartes Lumen. Revenir au cache (`LightingMode=0`) exige d'abord une preuve que
  `r.Lumen.Visualize 3` ne les montre plus noirs.
- **ECL-04** — Une seule lumière directionnelle éclaire la passe principale. Ajouter un soleil ou une
  lune passe par `AAnastasisWorldAtmosphere`, qui arbitre déjà les deux.
- **ECL-05** — Les VSM restent actives. On n'ajuste pas les ombres par les cascades (CSM), qui ne servent
  plus.
- **ECL-06** — Une ombre trop dure se corrige par la taille apparente de la source (`Source Angle`), pas
  par un flou de post-traitement.
- **ECL-07** — Changer de méthode de GI (Lumen Lite, logiciel, cache) est un A/B **image et ms**, aux
  mêmes caméras, en forêt, en berge et en vue haute : ce sont là que le Hit Lighting a été mesuré.

## Vérifier

| Quoi | Comment |
|---|---|
| Ce que Lumen voit | console : `r.Lumen.Visualize 3`, puis `r.Lumen.Visualize 5` ; noir = absent du cache |
| Effet d'un réglage Lumen | `capture-slice.ps1 -PreCmds '<cvar> <valeur>'` en A/B, puis `compare.py` (skill `anastasis-capture`) |
| Coût | `riverbank-capture.ps1 -Profile` (un `ProfileGPU` par vue) ; `stat GPU` dans l'éditeur |
| Référence mesurée | Hit Lighting : sous-bois 17,6 → 20,9 /255 (+19 %), +3,36 ms (17,50 → 20,86 ms) ; berge +0,79 ms ; vue haute +0,49 ms |
| Ciel et lumière au fil du jour | `capture-sky.ps1 -Preset cycle`, puis `atmosphere-metrics.py` (`fiches/atmosphere.md`) |

## Ne pas faire

- Revenir au cache de surface sans cartes Lumen : le sous-bois s'assombrit (17,6 contre 20,9 sans GI).
- Compenser par l'auto-exposition, le bloom ou une LUT (loi 1 et loi 2 du skill).
- Recopier les intensités d'un tutoriel : elles supposent souvent une auto-exposition active.

## Ouvert

- **Lumen Lite** : candidat si le budget GPU l'exige, et seulement alors (RU-002-01). Non mesuré. A/B :
  `capture-slice.ps1 -PreCmds 'r.Lumen.FinalGatherMethod 0'`, image **et** ms (ECL-07).
- **Arbres noirs pour Lumen** : la cause n'est pas établie (`lumen-hit-lighting-001.md`).
- Le `Source Angle` du soleil n'est pas posé par le projet (valeur par défaut du moteur) et n'a jamais été
  mesuré en A/B.
