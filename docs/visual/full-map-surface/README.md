# Surface sur le monde entier — preuve du chemin par défaut

Ce dossier remplace `docs/visual/slice-006/B_slice_surface.png` comme **état
courant** de la représentation du terrain. Il ne remplace pas le sceau
WORLD_SLICE_006, qui reste le compte rendu vérifié de `2c0b330`.

## Ce qui a changé

`AnastasisTerrainSurface::Build` n'est plus bornée au crop canonique 32×32.
Sa garde d'entrée refusait tout ce qui n'était pas exactement `32×32`, et elle
employait les constantes `CropW`/`CropH` comme pas de grille — la triangulation,
les normales, les couleurs et l'encastrement de l'eau étaient déjà génériques.
Elle accepte désormais n'importe quel crop du monde canonique, et l'embodiment
surface tout ce qu'il incarne.

| | Avant | Maintenant |
|---|---|---|
| Emprise surfacée | 32×32 (1/9 du monde) | 96×96 complet |
| Sommets | 1 024 | 9 216 |
| Triangles | 1 922 | 18 050 |
| Triangles d'eau | 374 | 3 544 |
| CVar `anastasis.Terrain.Surface` par défaut | `0` (cubes DEBUG) | `1` (surface) |

`Anastasis.Terrain.Contract` vérifie toujours 1024/1922 sur un crop 32×32 qu'il
fournit explicitement : `Build` reste exacte au sommet près sur le cas scellé.
Ces chiffres ne décrivent simplement plus ce que le projet rend par défaut.

Effet de bord corrigé au passage : l'habillage (arbres, ruines) était résolu sur
tout le Plan alors que le sol s'arrêtait à 32×32. Huit neuvièmes des 1 178
instances flottaient donc dans le vide. Invisible tant que les cubes DEBUG —
qui couvrent tout le Plan — étaient le mode par défaut. Sol et habillage
partagent maintenant une seule emprise, ce qui rend la divergence impossible
plutôt que rattrapée après coup.

## Images

| Fichier | Octets | SHA-256 (16) | Ce que ça prouve |
|---|---:|---|---|
| `full_surface.png` | 1 332 706 | `20E1C9EEDB220BC1` | Capture contrôlée, **même banc que le sceau 006** : carte `Lvl_AnastasisSlice`, seed `12345`, caméra `(-1800,-1800,3500)` pitch `-32.8` yaw `45`, soleil 75 000 lux, EV100 = 14, `viewmode lit`. Directement comparable à `B_slice_surface.png`. |
| `default_open.png` | 1 334 804 | `01FAA5BB7FC28469` | Le chemin **zéro clic** : éditeur lancé sans argument de carte, script d'observation interdit d'appeler `load_level` ou `EmbodyCanonical`. Tout ce qu'on voit vient de `EditorStartupMap` + `OnConstruction`. |

Reproduire la première :

```powershell
tools\unreal\capture-slice.ps1 -Mode 1 -Out B_full_surface.png
```

## Journal moteur au moment de la capture

```
ANASTASIS_TERRAIN source=96x96 crop=(0,0) 96x96 tiles=9216 vertices=9216
                  triangles=18050 water_triangles=3544 material=slice
                  boundary=tile_centers legacy_visible=0
ANASTASIS_PRESENTATION dressing_instances=1178 tree_tiles=716 ruin_tiles=462
```

Le marqueur `ANASTASIS_TERRAIN` rapporte maintenant les dimensions réelles.
Il annonçait auparavant `crop=(0,0) 32x32 tiles=1024` en dur, donc il mentait
dès qu'on changeait le crop.

## Autorisation

Extension décidée par Alexandre en session le 2026-09-13, après constat visuel
des instances flottantes. WORLD_SLICE_006 déclarait `NEXT_PHASE_AUTHORIZED:
Aucune` en listant forêt, PCG, bâtiments, PNJ et navigation — aucun de ces
cinq chantiers n'est ouvert ici. Seule l'emprise de la représentation du
terrain change.
