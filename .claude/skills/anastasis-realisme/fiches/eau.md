# Eau : rivières, lacs, rives

## Unreal

- **Plugin Water** : `WaterBodyRiver` / `Lake` / `Ocean` sur des splines, avec vagues, zone d'eau, et
  **carving** du terrain. Le carving écrit dans un **Landscape** : sans Landscape, il ne fait rien.
- **Single Layer Water** : le modèle d'ombrage d'eau d'Unreal, utilisable sur n'importe quel maillage.
  Il calcule l'absorption et la diffusion selon la profondeur réellement vue, la réfraction et les
  reflets, en une passe dédiée.
- Ce qui rend une eau crédible :
  - la couleur vient de la **profondeur** : claire au bord, sombre au large ;
  - le ciel se reflète, et la rugosité est faible ;
  - la surface bouge **dans le sens du courant** (flowmap) ;
  - la rive est un contact : une pente qui entre dans l'eau, un fond visible, une berge humide ;
  - aucune eau naturelle n'est un aplat bleu (interdit par la direction artistique : « blue water tiles »).

## ANÁSTASIS aujourd'hui

| Quoi | Mécanisme / valeur | Où |
|---|---|---|
| Matériau | `M_AnastasisWater`, Single Layer Water sur le maillage procédural ; repli `M_AnastasisShoreWater` | script d'autorité `tools/unreal/water-look.ps1` + `.py` |
| Optique | **volume actif depuis WATER_VOLUME_001** : Opacity = couverture d'écume (0 ailleurs), absorption (0,45 ; 0,22 ; 0,40) /m et diffusion (0,045 ; 0,045 ; 0,025) /m (palette « trouble », choix d'Alexandre 2026-10-10) × `CoefToCm` 0,01, Specular 0,25, rugosité 0,035 ; `VolumeOn` 0 = l'eau d'avant | `docs/unreal/handoffs/water-opacity-001.md` |
| Mouvement | cinq vagues directionnelles en HLSL (0,8 à 5,5 m), advectées par une flowmap à deux phases (UV2 = sens × vitesse) | idem |
| Rivières | rubans lisses par rivière (`AnastasisDrainage::BuildRiverRibbons`, section 2) ; largeur 4,5–42 m, profondeur 0,18–2,4 m | `RIVER_LOOK_001.md` |
| Lacs, mers, mares | grille de la nappe (section 1) | `HYDRO_NETWORK_001.md` |
| Berges | revanche 1 m en plaine, talus 1,5 × la largeur, talus droit qui traverse la ligne d'eau (pente 0,15) | `WATER_LOOK_001.md` |
| Coût | environ 1,07 ms (Single Layer Water 0,940 + pré-passe de profondeur 0,128) sur une image de 11,66 ms ; **un seul échantillon valide** | `RIVERBANK_LIFE_001.md` |
| Absents | plugin Water, WaterBody, splines de rivière, Landscape | décision d'Alexandre, 2026-09-30 |

CVars : `anastasis.Terrain.WaterLook` (1 ; 0 = eau d'avant), `anastasis.Terrain.Shoreline`,
`anastasis.Terrain.Drainage`, `anastasis.Dressing.Riverbank`.

## Règles

- **EAU-01** — Pas de plugin Water : l'eau est une couche de présentation de WorldView sur le maillage
  procédural (choix d'Alexandre du 2026-09-30).
- **EAU-02** — La couleur de l'eau vient de l'absorption selon la profondeur, jamais d'une couleur de
  sommet ni d'une teinte constante.
- **EAU-03** — Aucune terre sèche ne porte de bleu : une teinte de terre a toujours plus de rouge que de
  bleu (B > R + 0,04 = défaut). C'était le liseré cyan autour des lacs.
- **EAU-04** — Le lustre humide du sol reste près de l'eau réellement rendue (plafonné à 40 m).
- **EAU-05** — À une confluence, le récepteur couvre : un affluent est posé 1,5 cm sous lui, une rivière
  2 cm sous un lac. Ne pas aligner les hauteurs (z-fighting).
- **EAU-06** — `water-look.ps1 -Rebuild` recâble le matériau **en place** : quand une carte le tient,
  `delete_asset` échoue sans bruit. Il vide une COPIE de la liste des nœuds : en 5.8,
  `DeleteAllMaterialExpressions` en laisse (`MaterialEditingLibrary.cpp:559,590`).
- **EAU-07** — Le pin **Opacity** de `M_AnastasisWater` n'est jamais à 1 sur de l'eau. Le moteur calcule le
  volume (absorption, diffusion, fond lu, réfraction) seulement si `WaterVisibility = 1 - Opacity > 0`
  (`BasePassPixelShader.usf:1141`, `SingleLayerWaterShading.ush:74`) et ne garde de la BaseColor que
  `Opacity × diffuse` (`:1383`). Opacity = couverture d'une pellicule de surface (l'écume), 0 ailleurs.
  À 1 (valeur par défaut d'un pin non branché), l'eau est une teinte peinte et EAU-02 est faux : c'est ce
  qu'étaient « aplat peint » et « le sol ne se voit pas » (WATER_LOOK_001, RIVER_LOOK_001).
- **EAU-08** — Les coefficients de `Absorption` / `Scattering` sont lus **par centimètre**
  (`SingleLayerWaterShading.ush:221`, profondeur de scène en cm) : le matériau écrit des « par mètre » × `CoefToCm`
  0,01. Essayer ce facteur avec l'Opacity à 1 ne montre rien : tout le volume est sauté.
- **EAU-09** — `Specular` 0,25 pour l'eau (F0 0,02, indice 1,33) ; 0,5 est l'indice 1,5 du verre, et Single
  Layer Water en tire aussi son indice de réfraction (`SingleLayerWaterShading.ush:96`).

## Vérifier

| Quoi | Comment |
|---|---|
| L'eau et ses rives à 1,7 m | `riverbank-capture.ps1` (états `water` / `flat`, `banks` / `nobanks`), `-Profile` pour un `ProfileGPU` par vue |
| Réseau | `hydro-network-capture.ps1` (`anastasis.Terrain.Drainage 0/1`, `-Debug 1..4`) |
| Bord d'eau | `shore-capture.ps1` |
| Contenance des rives | `bank_containment` 0,947 → 0,980 (`WATER_LOOK_001.md`) : ne doit pas régresser |
| Volume d'eau actif | `riverbank-capture.ps1 -States 'vol_old,vol_on,vol_ior,vol_old2'` (preuve `water-volume-capture`) : un seul état change, les paramètres `VolumeOn` / `Specular` du matériau (instance dynamique par section d'eau) ; **passer les états entre guillemets** (PowerShell 5.1 sinon les sépare par des espaces) |

## Ne pas faire

- Activer le plugin Water « parce que c'est le standard » : le carving demande un Landscape, donc de
  remplacer la génération du terrain.
- Eau translucide d'une seule couleur (l'ancien `M_AnastasisShoreWater` à 0,88–0,96) : aplat bleu.
- Rive quantifiée à la maille (fond 10 cm sous l'eau et berge 12 cm au-dessus dans la même maille) :
  escalier.
- Régler absorption, diffusion ou `ColorScaleBehindWater` avec le pin Opacity non branché (valeur 1) : rien
  ne bouge, ces coefficients ne sont pas lus (EAU-07).
- Conclure que « Single Layer Water ne montre pas le sol vu d'aplomb » : c'était l'Opacity à 1
  (`RIVER_LOOK_001.md`, « Fond à 18 cm »).
- Garder les anciens coefficients (0,55 ; 0,16 ; 0,11 / 0,015 ; 0,040 ; 0,048) une fois le volume actif : eau
  turquoise de piscine tropicale, refusée pour une vallée pontique humide.

## Ouvert

- Coût de l'eau sur plus d'un échantillon.
- `MeshPartitionWater` (5.8, expérimental) fait marcher le plugin Water sur Mesh Partition, sans Landscape
  (RU-002-11). C'est un fait nouveau contre la raison d'EAU-01, mais il suppose de passer le terrain à
  Mesh Terrain : décision d'Alexandre. La vélocité de Single Layer Water est déjà écrite par défaut
  (RU-002-12).
- Le fond de l'eau, la pluie au sol et les aubes sont sur la branche `agent/env-realism-002`, non versée
  au 2026-10-01 (`docs/unreal/ENV_REALISM_002.md` sur cette branche).

## Vent partage (atmosphere-crusade-001)

Le materiau lit WeatherWind dans `MPC_AnastasisWeather` : les lacs gardent 12 % des pentes de rides
historiques sans vent, jusqu'a 100 % au vent maximal ; le courant des rivieres conserve ses rides.
Une perturbation supplementaire est orientee selon le vent monde, avec modulation transversale de
phase. L'optique Single Layer Water reste celle du ciel/lumiere reels. `Atmosphere.Coupling 0`
restitue la normale historique. Aucune ecriture dans l'hydrologie. La preuve et le cout sont dans
`handoffs/atmosphere-crusade-001.md`, sans assimiler generation de materiau et victoire visuelle.
