# HANDOFF: lumen-hit-lighting-001

## MISSION

Faire contribuer le terrain et la vÃ©gÃ©tation Ã  la lumiÃ¨re indirecte de Lumen.

Diagnostic (sondes en lecture seule, 2026-09-30/10-01, atmosphÃ¨re du jeu appliquÃ©e comme
en PIE) :

- Lumen actif, ray tracing matÃ©riel (RTX 3060, D3D12 tier 1.1),
  `r.Lumen.HardwareRayTracing=1`, Ã©clairage des impacts par le **cache de surface**
  (`LightingMode=0`, dÃ©faut moteur).
- Le terrain est un `ProceduralMeshComponent` : prÃ©sent dans la scÃ¨ne de ray tracing
  (`r.RayTracing.Geometry.ProceduralMeshes=1`) mais **sans cartes Lumen**. Visualisation
  `r.Lumen.Visualize 3` : terrain noir ; `r.Lumen.Visualize 5` : tout Â« culled Â». Les
  arbres (HISM) sont noirs eux aussi â€” cause non Ã©tablie.
- ConsÃ©quence mesurÃ©e : en sous-bois, Lumen **assombrit** (ombres 17,6/255 contre 20,9
  sans lumiÃ¨re indirecte) : il retire le ciel occultÃ© par le feuillage et ne rend rien
  du sol Ã©clairÃ©, qu'il voit noir.

Correction : `r.Lumen.HardwareRayTracing.LightingMode=1` dans
`Config/DefaultEngine.ini` (paramÃ¨tre de projet Â« Ray Lighting Mode Â») : la lumiÃ¨re est
Ã©valuÃ©e au point d'impact, sans passer par le cache.

## FILES_OWNED

- `Config/DefaultEngine.ini` (une ligne + commentaire)

## COMMIT

PENDING

## MEC

- BUILD: voir `finish`
- TESTS: voir `finish`
- Valeur au dÃ©marrage de l'Ã©diteur : voir `CVAR_CHECK` ci-dessous.

Gain, luminance moyenne des zones d'ombre (/255), mÃªme camÃ©ra, 1920Ã—1080 :

| Vue | sans indirect | cache (avant) | impacts (aprÃ¨s) | gain |
|---|---|---|---|---|
| sous-bois (73 % d'ombre) | 20,9 | 17,6 | 20,9 | +19 % |
| berge, 1,7 m | 75,6 | 74,4 | 79,1 | +6 % |
| vue haute, 250 m | 74,3 | 73,6 | 76,5 | +4 % |

CoÃ»t GPU, mÃ©diane de 95 images (`CsvProfile Frames=90`, colonne GPUTime), viewport de
l'Ã©diteur, machine sans autre Ã©diteur ni build :

| Vue | cache | impacts | Ã©cart |
|---|---|---|---|
| sous-bois | 17,50 ms | 20,86 ms | +3,36 ms |
| berge | 16,17 ms | 16,96 ms | +0,79 ms |
| vue haute | 19,66 ms | 20,15 ms | +0,49 ms |

## SCN

`Lvl_AnastasisSlice`, graine 12345, `EmbodyCanonical`, `AAnastasisWorldAtmosphere`
`Apply()` + `ApplyMist()`.

## PLY

NOT_IMPLEMENTED

## INTEGRATION_RISK

- CoÃ»t GPU en forÃªt (+3,4 ms, soit ~57 â†’ 48 images/s sur RTX 3060 dans le viewport
  Ã©diteur). RÃ©versible : remettre 0, ou `r.Lumen.HardwareRayTracing.LightingMode 0` en
  console pour comparer.
- Sans effet sur une machine sans ray tracing matÃ©riel (Lumen logiciel).

## STOP

Correctif de fond non fait : donner des cartes Lumen au terrain (`UDynamicMeshComponent`,
qui gÃ©nÃ¨re des cartes depuis ses bornes, Ã©ventuellement par morceaux) et comprendre
pourquoi les arbres manquent au cache. Il rendrait le mÃªme gain au coÃ»t du mode cache.

