# ATMOSPHERE_001 — preuve visuelle A/B

Comparaison contrôlée. **Tout est identique entre A et B, et entre C et D** :
carte `/Game/FirstPerson/Lvl_FirstPerson` en PIE, seed `12345`, monde canonique
96×96, même signet de caméra (`OVERVIEW` : `(4800,4800,10047)` pitch `-75` fov
`80` ; `GROUND` : `(4850,4850,591)` pitch `-5` yaw `45` fov `90`). Seule change
la CVar `anastasis.Atmosphere`.

| Image | CVar | Ce qu'on voit |
|---|---|---|
| `A_overview_atmosphere_off.png` | `0` | éclairage du template : soleil à **6 lux**, roll de 112°, exposition automatique. Aucune ombre portée, aucun modelé. |
| `B_overview_atmosphere_on.png` | `1` | soleil ANÁSTASIS 75 000 lux, pitch −38 / yaw −55, sans roll : ombres portées des arbres, faces éclairée et ombrée sur le relief. |
| `C_ground_atmosphere_off.png` | `0` | brouillard du template : `start_distance=0`, `max_opacity=1` — la brume commence à l'objectif. |
| `D_ground_atmosphere_on.png` | `1` | brouillard piloté par les données : premier plan net (`start=1500 uu`), brume réservée à la distance (`max_opacity=0.85`). |

Le brouillard **ne se voit pas** sur la paire A/B : la caméra `OVERVIEW` est à
Z≈10047 et le brouillard est ancré à Z=0 avec un falloff de 0.2. C'est un indice
de sol ; l'écart visible en vue d'ensemble, c'est le soleil. Dire autre chose
serait surinterpréter l'image.

Reproduire :

```powershell
tools\unreal\probe-demo.ps1 -Mission overview-off -PreCmds 'anastasis.Atmosphere 0'
tools\unreal\probe-demo.ps1 -Mission overview-on
tools\unreal\probe-demo.ps1 -Mission ground-off -Bookmark GROUND -PreCmds 'anastasis.Atmosphere 0'
tools\unreal\probe-demo.ps1 -Mission ground-on -Bookmark GROUND
```

Ces quatre images ont été prises **après** le rebase sur `main` à `f16cf10`,
donc sur le même arbre que le `BUILD::PASS` et les 46 tests verts. Le dressing
qu'on y voit (1178 instances : 716 arbres, 462 ruines) vient de
`DA_AnastasisPresentation`, pas de cette mission.

Chaque exécution écrit aussi le snapshot `anastasis.world_snapshot.v1` à côté de
l'image : son nouveau bloc `atmosphere` contient les valeurs mesurées du tableau
ci-dessus (`sun_intensity`, `fog_start_distance`, `exposure_ev100`, …). Les
captures brutes et les logs d'éditeur restent dans `Saved/` (non versionné).
Ces quatre copies-ci sont versionnées parce qu'elles étayent la mission.

Voir `docs/unreal/ATMOSPHERE_001.md`.
