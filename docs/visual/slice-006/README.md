# WORLD_SLICE_006 — preuve visuelle finale

Comparaison contrôlée. **Tout est identique entre les deux images** : carte
`/Game/Anastasis/Maps/Lvl_AnastasisSlice`, seed `12345`, monde canonique 96×96,
caméra `(-1800,-1800,3500)` pitch `-32.8` yaw `45`, soleil 75 000 lux,
exposition figée EV100 = 14, `viewmode lit`. Seule change la CVar
`anastasis.Terrain.Surface`.

| Image | CVar | Ce qu'on voit |
|---|---|---|
| `A_legacy_debug.png` | `0` | métrologie DEBUG : 9216 cubes HISM du monde 96×96, une couleur plate par type de tuile |
| `B_slice_surface.png` | `1` | tranche canonique 32×32 : surface continue, relief, rive, nappe d'eau |

Les deux emprises diffèrent volontairement : le chemin DEBUG incarne le monde
**96×96** complet, la surface ne traite que le crop canonique **32×32**. Ce n'est
pas un recadrage de la même chose — c'est le périmètre que `AnastasisTerrainSurface`
accepte aujourd'hui, et il rejette tout le reste.

Reproduire :

```powershell
tools\unreal\capture-slice.ps1 -Mode 0 -Out A_legacy_debug.png
tools\unreal\capture-slice.ps1 -Mode 1 -Out B_slice_surface.png
```

Les captures brutes et leurs logs d'éditeur restent dans `Saved/SliceEvidence/`
(non versionné). Ces deux copies-ci sont versionnées parce qu'elles étayent le
sceau.
