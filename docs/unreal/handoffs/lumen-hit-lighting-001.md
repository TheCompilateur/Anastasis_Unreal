# HANDOFF: lumen-hit-lighting-001

## MISSION

Faire contribuer le terrain et la végétation à la lumière indirecte de Lumen.

Diagnostic (sondes en lecture seule, 2026-09-30/10-01, atmosphère du jeu appliquée comme
en PIE) :

- Lumen actif, ray tracing matériel (RTX 3060, D3D12 tier 1.1),
  `r.Lumen.HardwareRayTracing=1`, éclairage des impacts par le **cache de surface**
  (`LightingMode=0`, défaut moteur).
- Le terrain est un `ProceduralMeshComponent` : présent dans la scène de ray tracing
  (`r.RayTracing.Geometry.ProceduralMeshes=1`) mais **sans cartes Lumen**. Visualisation
  `r.Lumen.Visualize 3` : terrain noir ; `r.Lumen.Visualize 5` : tout « culled ». Les
  arbres (HISM) sont noirs eux aussi — cause non établie.
- Conséquence mesurée : en sous-bois, Lumen **assombrit** (ombres 17,6/255 contre 20,9
  sans lumière indirecte) : il retire le ciel occulté par le feuillage et ne rend rien
  du sol éclairé, qu'il voit noir.

Correction : `r.Lumen.HardwareRayTracing.LightingMode=1` dans
`Config/DefaultEngine.ini` (paramètre de projet « Ray Lighting Mode ») : la lumière est
évaluée au point d'impact, sans passer par le cache.

## FILES_OWNED

- `Config/DefaultEngine.ini` (une ligne + commentaire)

## COMMIT

PENDING

## MEC

- BUILD: PASS (worktree) ; portail : voir `finish`
- TESTS: voir `finish`
- Valeur au démarrage de l'éditeur du worktree :
  `LogConfig: Set CVar [[r.Lumen.HardwareRayTracing.LightingMode:1]]`, relue
  `CVAR_CHECK r.Lumen.HardwareRayTracing.LightingMode=1`.

Gain, luminance moyenne des zones d'ombre (/255), même caméra, 1920×1080 :

| Vue | sans indirect | cache (avant) | impacts (après) | gain |
|---|---|---|---|---|
| sous-bois (73 % d'ombre) | 20,9 | 17,6 | 20,9 | +19 % |
| berge, 1,7 m | 75,6 | 74,4 | 79,1 | +6 % |
| vue haute, 250 m | 74,3 | 73,6 | 76,5 | +4 % |

Coût GPU, médiane de 95 images (`CsvProfile Frames=90`, colonne GPUTime), viewport de
l'éditeur, machine sans autre éditeur ni build :

| Vue | cache | impacts | écart |
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

- Coût GPU en forêt (+3,4 ms, soit ~57 → 48 images/s sur RTX 3060 dans le viewport
  éditeur). Réversible : remettre 0, ou `r.Lumen.HardwareRayTracing.LightingMode 0` en
  console pour comparer.
- Sans effet sur une machine sans ray tracing matériel (Lumen logiciel).

## STOP

Correctif de fond non fait : donner des cartes Lumen au terrain (`UDynamicMeshComponent`,
qui génère des cartes depuis ses bornes, éventuellement par morceaux) et comprendre
pourquoi les arbres manquent au cache. Il rendrait le même gain au coût du mode cache.
