# HANDOFF: ground-cover-lande-001

## MISSION

GROUND_COVER_001, étape H6 : la lande des versants 20-45°, lue sur la planche État Zéro 1
(« pente subalpine : conifères, landes », « rochers, éboulis : affleurements »). Deux familles :
H6a touffe d'éboulis (`HeathTussock`, graminée dure et serrée) et H6b callune (`Heather`, petit
buisson brun-vert à épis mauves de fin d'été). Au-delà de 45° (falaise) : rien.
Voir `docs/unreal/GROUND_COVER_001.md`, section Lande.

## FILES_OWNED

- `Source/Anastasis_UnrealV2/WorldView/AnastasisGroundCover.h` / `.cpp` — familles `HeathTussock`, `Heather` ; bandes prairie 0-20 / lande 20-45 avec fondu 18-22 ; `LandeMask`, fond de vallée (`bHasValleyFloor` / `ValleyFloorZ`) ; histogramme de pentes et hauteurs de lande dans `FPlan`
- `Source/Anastasis_UnrealV2/WorldView/AnastasisGroundCoverTests.cpp` — `SlopeBands` mis à jour, nouveau `Anastasis.GroundCover.Lande`
- `Source/Anastasis_UnrealV2/WorldView/AnastasisWorldEmbodiment.cpp` — `LandeOpenness` (roche = 1), fond de vallée = bassin de la forge, journal `heath= heather=`, ligne `ANASTASIS_GROUND_SLOPES`
- `Content/Anastasis/GroundCover/SM_Grass_HeathTussock_01.uasset`, `SM_Grass_Heather_01.uasset` (nouveaux, LFS, générés)
- `tools/unreal/create-ground-cover.py` (deux familles, taille d'épi par famille), `tools/unreal/ground-cover-capture.py` (vue `lande_eye`)
- `docs/unreal/GROUND_COVER_001.md`, cette fiche, `docs/visual/ground-cover-lande-001/`

Les trois touffes de prairie et `M_AnastasisGrass` ont été régénérés à l'identique (même graine,
mêmes triangles) pendant la mission ; leurs `.uasset` ne sont pas versés (seuls les identifiants
internes du paquet changeaient).

## COMMIT

BRANCH_HEAD

## MEC

- BUILD: `BUILD::PASS` (`agent-worktree.ps1 finish`, branche rebasée sur `main` 3e0ef81).
- UNITY: `Build.bat` direct sur arbre propre après commit : aucune exclusion adaptative,
  `Result: Succeeded` (lot `Module.Anastasis_UnrealV2.2.cpp` recompilé pour le test corrigé).
- TESTS: `TESTS::PASS` — 179 PASS / 4 KNOWN_EXPECTED_FAILURE / 0 FAIL, 183 annoncés, 183 vus.
  Premier passage : `Anastasis.GroundCover.Lande` en FAIL sur « lande deux fois plus maigre que
  la prairie », seuil de la v1 devenu faux après le relèvement mesuré de la densité (24° : 3 764
  touffes, prairie 4° : 4 872). Assertion ramenée à « plus maigre », commit séparé `c0862b4`.
  - PASS `Anastasis.GroundCover.Lande` (roche sans prairie : lande seule ; hors habitat : rien ;
    24° 3 764 > 40° 1 575 > 0 ; callune : 0 sous 8 m du fond, 698 au-dessus de 28 m, 718 touffes
    d'éboulis au pied ; fond de vallée non fini refusé)
  - PASS `Anastasis.GroundCover.SlopeBands` (26° : lande seule ; 16° : aucune lande ; 50° : rien)
- ASSETS: `GROUND_COVER_ASSETS::PASS ... SM_Grass_HeathTussock_01=[1216, 362, 68] SM_Grass_Heather_01=[1810, 759, 81]`
- COMMANDS:
  - `tools\unreal\create-ground-cover.ps1`
  - `tools\unreal\capture-ground-cover.ps1 -Label lande-v5 -States on,off,on2`
  - `tools\unreal\agent-worktree.ps1 finish -Mission ground-cover-lande-001`

## SCN

PASS (éditeur dédié, `Lvl_AnastasisSlice`, `EmbodyCanonical(12345)`) :

```
ANASTASIS_GROUND_COVER enabled=1 tall=507198 short=416894 sedge=63773 heath=64836 heather=17777
  placed=1070478 chunks=1207 refused_slope=37193 truncated=0 missing_meshes=0
ANASTASIS_GROUND_SLOPES dry_candidates_by_slope 0-10=1442584 10-20=537577 20-30=122705
  30-45=111957 45-60=32582 60+=4611 steep_refused_canopy=31435 lande_above_floor_m=[0.6 14.9 34.8]
```

A/B : `docs/visual/ground-cover-lande-001/`. GPU +1,1 à 2,6 ms pour toute la strate herbacée ;
durée de frame dans le bruit (on / off / on2 se chevauchent). Deux captures interrompues de
l'extérieur pendant la mission (fenêtre principale détruite en pleine capture) : relancées.

## PLY

NOT_IMPLEMENTED — aucune interaction joueur ; la lande n'a ni collision ni navigation.

## INTEGRATION_RISK

- Mêmes fichiers chauds que ground-cover-001 (`AnastasisWorldEmbodiment.cpp`).
- `FSettings::MaxSlopeDegrees` change de sens : 20 → 45 (borne haute de la lande) ; la borne de
  la prairie est `MeadowSlopeDegrees`. Aucun autre appelant hors de ce module.
- ~1 207 HISM d'herbe et de lande au lieu de 683 sur la carte de référence.

## STOP

Ne revendique pas : joncs (H4), sous-bois (H5), fleurs de prairie ; la lande ne couvre pas tous
les versants visibles (certains restent nus, voir Limites) ; aucun budget de performance hors
éditeur.
