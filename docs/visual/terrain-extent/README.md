# Captures — généralisation de l'emprise de la surface

Preuve vivante de `docs/unreal/TERRAIN_SURFACE_EXTENT.md`. Seed `12345`, monde
canonique `96×96`, soleil 75 000 lux, exposition figée EV100 = 14, `viewmode lit`,
rig `/Game/Anastasis/Maps/Lvl_AnastasisSlice`. Seules varient la CVar
`anastasis.Terrain.Surface` et la position de caméra, indiquées par fichier.

| Fichier | Octets | SHA-256 (16) | Mode | Caméra |
|---|---:|---|---|---|
| `C_world_surface.png` | 1 051 580 | `5054F585AEFF5939` | 2 | `(-5400,-5400,10500)` |
| `C_world_debug.png` | 1 080 857 | `6F3D595A6C6A9E54` | 0 | `(-5400,-5400,10500)` |
| `D_legacy_slicecam.png` | 1 545 613 | `16F6AF3DC40DBE1A` | 0 | `(-1800,-1800,3500)` |

Toutes les caméras portent le même angle : pitch `-32.8`, yaw `45`. La caméra
`(-5400,-5400,10500)` est exactement celle de la tranche reculée d'un facteur 3,
le rapport des emprises (`3200` UU contre `9600`).

## Ce que chaque image prouve, et ce qu'elle ne prouve pas

**`C_world_surface.png`** — le monde entier d'un seul tenant :
`vertices=9216 triangles=18050 water_triangles=3544 legacy_visible=0`. Le réseau
d'eau se lit de bout en bout, les rives sableuses dessinent les côtes, il n'y a
aucun trou. Le noir autour n'est pas un défaut de rendu : au-delà de la tuile 96
il n'y a rien, le monde est une île dans le vide.

**`D_legacy_slicecam.png`** — le chemin DEBUG à la caméra scellée. Reproduit
`docs/visual/slice-006/A_legacy_debug.png`, dressing de `f6fa234` en plus. C'est
la pièce qui établit qu'**aucune régression de couleur n'a été introduite**. On y
voit aussi, franchement, les trous noirs entre dalles : une marche d'altitude de
0.05 fait 50 UU pour une dalle épaisse de 20 UU.

**`C_world_debug.png`** — la même chose reculée ×3, **entièrement grise**. Gardée
comme contre-exemple : à ce recul les instances HISM perdent toute sémantique de
couleur. Ce n'est pas une régression — `D_legacy_slicecam.png` le prouve — c'est
une limite de la capture. Ne pas prendre de preuve visuelle du chemin DEBUG
depuis cette distance.

## Ce qui manque

L'A/B mode 0 / mode 2 **à caméra identique n'existe pas**. Cette combinaison
précise (mode 2 + caméra de la tranche) ne produit aucun PNG : reproduit deux
fois, huit `HighResShot` émis, aucune erreur au log. Voir `KNOWN_DEBT` n°1 dans
`docs/unreal/TERRAIN_SURFACE_EXTENT.md`. La comparaison ci-dessus se lit donc
entre deux cadrages, pas entre deux images superposables.
