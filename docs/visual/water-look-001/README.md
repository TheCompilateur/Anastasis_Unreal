# water-look-001 — preuves visuelles

Mission : `docs/unreal/WATER_LOOK_001.md`. Seed 12345, Human_Geography_V2, brume active, mêmes
caméras que la vitrine de HYDRO_NETWORK_001, même build (branche `agent/water-look-claude-01`
rebasée sur `main` 998537f, Lumen hit lighting compris).

| Fichier | Contenu |
|---|---|
| `1_plaine_vers_le_lac_avant_apres.png` | plaine et confluences vers le lac |
| `2_lacs_altitude_avant_apres.png` | lacs d'altitude ; les halos clairs du sol sont identiques des deux côtés (résidu hors périmètre) |
| `3_riviere_rasante_avant_apres.png` | rive en escalier de la grille avant, rive lisse après |
| `4_ruisseau_vers_la_mer_avant_apres.png` | ruisseau de la vallée B |
| `3_riviere_rasante.png` | vue 3 après, pleine résolution |

Gauche : `anastasis.Terrain.WaterLook 0`. Droite : `1`. Regénérer :

```powershell
tools\unreal\hydro-network-capture.ps1 -Label final_on -States "1" -Shots <showcase_shots.json> `
  -PreCmds "anastasis.Terrain.WaterLook 1;ShowFlag.Fog 1;ShowFlag.VolumetricFog 1"
```

Caméras (`showcase_shots.json`) :

```json
[["1_plaine_vers_le_lac",[28000,38000,26000],[125000,128000,1500]],
 ["2_lacs_altitude",[68000,98000,15000],[125000,45000,2500]],
 ["3_riviere_rasante",[62000,112000,7500],[112000,132000,600]],
 ["4_ruisseau_vers_la_mer",[88000,78000,13000],[38000,20000,0]]]
```
