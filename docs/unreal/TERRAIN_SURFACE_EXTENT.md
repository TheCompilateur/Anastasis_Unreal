# Terrain surface — généralisation de l'emprise

Mission `terrain-surface-world`, branchée sur `main` à `f6fa234`. Additive.
Aucun fichier de `Source/AnastasisSim/` n'est ouvert en écriture.

## Le problème

`AnastasisTerrainSurface::Build` refusait toute emprise autre que le crop
canonique `32×32` — la garde comparait `Crop.W`/`Crop.H` aux constantes
`CropW`/`CropH`. En face, `AAnastasisWorldEmbodiment` masquait **les sept HISM**
dès que la surface se construisait (`legacy_visible=0`).

Conséquence, avant ce changement : le projet n'avait que deux états, et aucun
n'était une carte.

| `anastasis.Terrain.Surface` | Ce qu'on voit |
|---|---|
| `0` | le monde `96×96` entier, en dalles de 100×100×**20 uu** posées à `Alt × 1000` |
| `1` | `32×32` en surface continue, **et rien autour** |

En mode `0`, une marche d'altitude de 0.05 fait 50 uu pour une dalle épaisse de
20 uu : il reste 30 uu d'air ouvert entre deux voisines. Les trous noirs dans
les falaises ne sont pas un défaut d'éclairage, ce sont des trous.

## Le changement

`Build` accepte désormais **n'importe quelle emprise tenant dans le monde
canonique** : de la cellule unique (2×2 sommets) au `96×96` complet. Ce qui
reste refusé, et qui compte :

- un monde source qui n'est pas le `96×96` canonique — la surface est adossée à
  la vérité de simulation, pas à une taille arbitraire ;
- une emprise qui déborde du monde ;
- une bande large d'une seule tuile : elle ne porte aucune cellule.

La CVar gagne une valeur, elle n'en change aucune :

| Valeur | Emprise | Statut |
|---|---|---|
| `0` | — | dalles DEBUG, inchangé |
| `1` | crop canonique `32×32` | **tranche scellée WORLD_SLICE_006, inchangée** |
| `2` | toute l'emprise incarnée (`96×96` au spawn) | nouveau |

## Pourquoi le sceau tient toujours

`TERRAIN_CONTRACT` (1024 sommets / 1922 triangles) n'est pas élargi : il est
devenu une référence de non-régression. Avec `W=32, H=32`, chaque expression
généralisée se réduit terme à terme à l'ancienne — mêmes opérations flottantes,
même ordre, même géométrie. `Anastasis.Terrain.Contract` et
`Anastasis.Terrain.Semantics` passent **sans modification**, et
`Anastasis.Terrain.WorldExtent` vérifie en plus que les 1024 sommets de la
tranche scellée sont exactement les sommets du coin `(0,0)` du monde entier.

La ligne de log de la preuve est désormais formatée depuis l'emprise réelle. En
mode `1` elle imprime caractère pour caractère la chaîne scellée :
`source=96x96 crop=(0,0) 32x32 tiles=1024`.

## Limites connues, assumées

- **La palette est relative à l'emprise.** `TileColor` normalise l'altitude sur
  `MinAlt`/`MaxAlt` *du crop*. Le monde entier ne se colore donc pas comme le
  `32×32` : c'est plus juste à l'échelle carte, mais ce n'est pas la même image.
  Les couleurs ne sont volontairement pas comparées entre les deux emprises.
- **Le dressing lit encore `Plan.Alts`**, l'altitude de tuile, alors que le
  jitter XY le pose entre deux sommets interpolés. Flottement/enfoncement
  proportionnel à la pente locale. Hors périmètre ici — c'est le point 2 du plan.
- **Aucun chunking.** `96×96` = 9216 sommets / 18050 triangles, une seule
  section de `ProceduralMeshComponent`. Découper n'a rien à gagner à cette
  échelle : la complexité doit se mériter.
- **Aucune texture, aucune vague.** `KNOWN_VISUAL_LIMITS` de WORLD_SLICE_006
  reste vrai mot pour mot, simplement appliqué à une emprise plus large.
- **Le bandeau d'exposition Lumen** visible dans les captures est hors de ce
  changement : le rig fige EV100=14 sans accorder
  `r.EyeAdaptation.CachedLightingPreExposure`. Tant qu'il est là, aucune
  comparaison visuelle A/B n'a valeur de preuve.

## Preuve

Build sur sources quiescentes, puis suite complète via `report-tests.ps1` :

```
BUILD::PASS
PASS                  : 30      (29 + Anastasis.Terrain.WorldExtent)
KNOWN_EXPECTED_FAILURE: 4       (les 4 du registre, inchangées)
FAIL                  : 0
TOTAL                 : 34
```

Valeurs sorties des tests, pas des affirmations :

| Marqueur | Valeur |
|---|---|
| `TERRAIN_CONTRACT` | `vertices=1024 triangles=1922 max_error=0.000000000 boundary_edges=124` |
| `TERRAIN_EXTENT` | `world=96x96 vertices=9216 triangles=18050 boundary_edges=380` |
| `TERRAIN_SEMANTICS` | `water_tiles=151 land_tiles=873 shore_tiles=158 water_quads=187` |

`Anastasis.Terrain.Contract` et `Anastasis.Terrain.Semantics` passent **sans avoir
été modifiés**, à erreur maximale nulle : la tranche scellée sort de la fonction
généralisée exactement comme avant. Les 380 arêtes de périmètre sur 18 050
triangles disent qu'aucune arête interne n'est orpheline — aucune fissure.

Captures vivantes, classees dans `docs/visual/terrain-extent/` (`Saved/` est ignore par git) :

| Fichier | Mode | Caméra | Log |
|---|---|---|---|
| `C_world_surface.png` | 2 | `(-5400,-5400,10500)` | `crop=(0,0) 96x96 tiles=9216 vertices=9216 triangles=18050 water_triangles=3544` |
| `C_world_debug.png` | 0 | `(-5400,-5400,10500)` | `ANASTASIS_TERRAIN disabled legacy_visible=1` |
| `D_legacy_slicecam.png` | 0 | `(-1800,-1800,3500)` | idem, reproduit `docs/visual/slice-006/A_legacy_debug.png` |

## KNOWN_DEBT introduite par cette mission

1. **`HighResShot` ne rend rien en mode 2 à la caméra de la tranche.** Reproduit
   deux fois : la surface se bâtit (`vertices=9216` au log), la caméra se pose
   (`SLICE_CAMERA_APPLIED loc=(-1800,-1800,3500)`), huit `HighResShot` sont émis,
   **aucun PNG n'est écrit** et aucune erreur de rendu n'apparaît au log. Les
   trois autres combinaisons capturent normalement. Cause non trouvée, pas
   contournée. Conséquence : l'A/B mode 0 / mode 2 à caméra identique **n'existe
   pas encore** — ne pas prétendre le contraire.
2. **À 3× de distance, les instances HISM du chemin DEBUG rendent en gris
   neutre** (`C_world_debug.png`), toute sémantique de couleur perdue. Ce n'est
   pas une régression : `D_legacy_slicecam.png` reproduit la capture scellée.
   Aucune preuve visuelle du chemin DEBUG ne doit être prise depuis ce recul.
3. **Le chemin DEBUG est beaucoup plus lourd que la surface.** 9216 dalles
   instanciées n'ont pas atteint la demande de capture en 300 s au premier essai ;
   la surface, une seule section de 18 050 triangles, capture sans peine. D'où
   le `-TimeoutSec` ajouté à `capture-slice.ps1` (défaut 300 inchangé).
