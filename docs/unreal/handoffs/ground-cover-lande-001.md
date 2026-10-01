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

PENDING

## SCN

PENDING

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
