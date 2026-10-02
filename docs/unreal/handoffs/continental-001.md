# HANDOFF: continental-001

## MISSION

Donner un continent à la carte. Jusqu'ici, au-delà des 96 tuiles (1,9 km), l'anneau d'horizon
n'était qu'un plateau vert : collines de 7 à 60 m puis une chaîne de 100 à 300 m, sans cause. Depuis
le sol, rien à lever la tête vers ; depuis les airs, une table. Mandat d'Alexandre (2026-10-02) :
« une infrastructure continentale avec tectonique, réaliste — le joueur doit d'abord être immergé
dans un monde avant de comprendre et connaître le simulateur ».

Livré : `AnastasisTectonics`, un modèle tectonique **pur** (fonction de x, y, graine, sens de
l'eau), et l'anneau d'horizon qui le porte, étendu de 20 à 60 km et rendu à résolution angulaire
constante.

- **Sens de l'eau** : somme des normales sortantes des colonnes d'eau du bord de la carte. L'eau sort
  où le continent descend ; les chaînes sont à l'opposé, une rivière ne remonte jamais vers une
  montagne. Sans eau au bord, la graine décide.
- **Structure** (`AnastasisTectonics.cpp`) : piémont puis plateau continental ; ceinture de plis
  à front raide et revers doux (hogbacks, 2,3 km d'écart, coupés de cluses) ; faille décrochante
  rectiligne à 8 km (tranchée de 100 m, crêtes de blocage) ; escarpement de faille normale à 7 km
  côté basses terres ; chaîne principale à 12,5 km (2 à 3,4 km, bruit érodé, cols tous les 7 à 13 km,
  contrefort parallèle, arêtes vives) ; seconde chaîne à 40 km (3 à 4 km) ; collines et chaîne
  extérieure côté basses terres ; cuvette qui ferme l'horizon dans toutes les directions.
- **Surface des montagnes** (`SurfaceAt`) : forêt montagnarde, alpage, roche, neige, lus de
  l'altitude et de la pente **rendue** du sommet. La vallée garde la palette de prairie de la carte.
- **Résolution** : le pas radial ne dépasse jamais 1,5 % de la distance au centre (`FarAngularStep`),
  soit 0,86° vu du centre jusqu'à 60 km (avant : cellules de 1,5 × 3 km à 10 km, une montagne n'y
  pouvait être qu'un plateau). 285 anneaux, 221 920 sommets, 441 560 triangles (avant : 20 805 / 39 995).
- **Talus** (`TalusNearDeg` 42°, `TalusFarDeg` 58°, 48 passes d'érosion thermique de Jacobi sur les
  arêtes des triangles) : aucun mur de 75° entre deux sommets de 150 m. Le raccord et les anneaux de
  la rivière sortante ne bougent pas.

## FILES_OWNED

- `Source/Anastasis_UnrealV2/WorldView/AnastasisTectonics.h/.cpp`, `AnastasisTectonicsTests.cpp` (nouveaux)
- `Source/Anastasis_UnrealV2/WorldView/AnastasisTerrainHorizon.h/.cpp`, `AnastasisTerrainHorizonTests.cpp`
- `Source/Anastasis_UnrealV2/WorldView/AnastasisWorldEmbodiment.cpp` (section 2 de l'anneau, CVar, journal)
- `Source/Anastasis_UnrealV2/WorldView/AnastasisWorldAtmosphere.cpp` (plafond de la perspective aérienne, un bloc)
- `tools/unreal/far-terrain-material.py/.ps1` (nouveau), `capture-horizon.ps1/.py` (`-Mode skyline`, `-AtmoProps`, GPU par vue)
- `Content/Anastasis/Materials/M_AnastasisFarTerrain`
- `AGENTS.md` (index), `.claude/skills/anastasis-realisme/fiches/terrain.md`

## COMMIT

PENDING

## MEC

- BUILD: voir `finish`
- TESTS: `report-tests.ps1 -Filter 'Anastasis.Terrain.Tectonics+Anastasis.Terrain.Horizon'` → **10 PASS, 0 FAIL** :
  `Tectonics.{Deterministic,Structure,Frame,Surface,Horizon}`, `Horizon.{Seam,Closed,Gentle,NoSlivers,Palette}`
  Valeurs : max 3 875 m, 285 anneaux, 221 920 sommets, 441 560 triangles, chaîne vue à 15,2° depuis le bassin,
  horizon fermé partout (pire direction +1,1° à +1,8°), allongement p99 5,47, couture exacte (écarts 0),
  pente : champ proche 50,4° ≤ forge au bord 50,4°, chaînes 65,0° (talus 58° + convergence), 0 triangle > 45° dans les
  4 km hors prolongement de la berge. Construction de l'anneau : 0,7 à 1,0 s.
- COMMANDS:
  - `tools/unreal/far-terrain-material.ps1` → `FAR_TERRAIN_MATERIAL::PASS`
  - `tools/unreal/capture-horizon.ps1 -Label <nom> -Mode skyline -Atmosphere -States AB`

## PROOFS

PROOFS: (aucune)

## SCN

**KEEP pour la forme, avec une dépendance atmosphérique.** Images regardées : `Saved/HorizonEvidence/` dans ce worktree
(`continental-sky*`, `continental-nofog`, `atmo-aerial1*`, `continental-final*`).

- Avant (main) : un plateau vert plat jusqu'à l'horizon, partout (voir `docs/visual/horizon-ring-001/`).
- Après, vers l'intérieur : une chaîne de 3,4 km à 14 km, des couloirs d'érosion, des cols, une seconde chaîne
  en arrière-plan ; dans les autres directions, collines, crêtes et chaîne extérieure basse (plus discret).
- **Défaut majeur trouvé en cours de route, et sa cause :** la chaîne sortait chalk-blanche. Diagnostic par
  captures contrôlées (une seule variable à la fois) : sans brouillard (`ShowFlag.Fog 0`) elle a ses ombres mais
  une teinte beige (matériau de sol) ; avec la perspective aérienne à ×1 au lieu de ×3,45 elle redevient lisible ;
  plafonner l'opacité du brouillard exponentiel à 25 % n'ajoute presque rien. Deux corrections :
  1. **matériau** `M_AnastasisFarTerrain` au-delà de 8 km (`anastasis.Terrain.HorizonFarMaterial`, 1 par défaut) :
     le matériau de sol de la carte applique ses propres couleurs de famille et ignorait mes teintes de sommet ;
  2. **plafond** de l'échelle de perspective aérienne (`anastasis.Atmosphere.AerialCap`, 1,2 par défaut, 0 = aucun) :
     la valeur du profil (3,0, ×1,15 à hauteur d'œil) date d'un monde de 1,9 km ; sa note de conception le dit.
- Limite assumée : à 1,7 la chaîne restait pâle ; 1,2 est le réglage retenu. La neige est rare (1 273 sommets,
  contre 4 581 avant relèvement de la limite à 3 150 m) : la chaîne se lit surtout en roche et en alpage.
- Ciel : non traité, voir STOP.

**Coût (RTX 3060, viewport éditeur 1920×1080 `HighResShot`, p50 GPU sur les vues stabilisées)** :
sans l'anneau 23,4 ms ; avec l'anneau continental (442 k triangles, matériau lointain) 24,5 ms, soit **+1,1 ms**.
Les deux premières vues d'une série sont des échauffements de shaders (77 et 185 ms) et ne comptent pas.
Aucune mesure en jeu packagé.

## PLY

UNKNOWN : aucune session de jeu.

## ECARTS

AUCUN — `Source/AnastasisSim` inchangé : le continent est de la présentation, il ne lit ni n'écrit `AnastasisWorld`.

## INTEGRATION_RISK

- `AnastasisTerrainHorizon.cpp` est le seul fichier chaud ; `soil-slope-002` (matériau de sol,
  lecture de roche sur les pentes) change l'aspect des montagnes de l'anneau sans conflit de fichier.
- Les tests `Gentle` et `Palette` ont été ajustés (voir plus bas) : toute mission qui les relit doit
  repartir de ces versions.
- Le `Build` de l'anneau passe de 20 k à 222 k sommets (0,7 à 1,0 s à l'incarnation, +1,1 ms de GPU, mesurés ci-dessus).
- **`AnastasisWorldAtmosphere.cpp` change l'aspect de TOUTES les captures** : la perspective aérienne passe de ×3,45 à
  ×1,2 au plus. `air-relief-002` et `valley-air-001` y travaillent : à relire avec eux ; `anastasis.Atmosphere.AerialCap 0`
  rend leur réglage. Les captures de vallée n'ont pas été comparées image par image avec l'ancienne échelle.
- Le maillage n'est pas sauvé dans la carte : reconstruit à chaque `EmbodyCanonical`.

## STOP

Ce que cette mission ne revendique pas : un ciel (la couverture claire `CloudCoverageClear` est à -0,55 : aucun
nuage par beau temps, mission à part), la mer ou un lac à l'horizon, la flore des montagnes (la couleur de forêt
d'altitude est une teinte de sommet, pas des arbres), un coût GPU en jeu packagé 1080p.
