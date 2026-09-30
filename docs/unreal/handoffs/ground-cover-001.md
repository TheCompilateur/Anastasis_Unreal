# HANDOFF: ground-cover-001

## MISSION

GROUND_COVER_001 : première strate herbacée sur la carte, lue sur les planches État Zéro
(`docs/visual/reference/`, EZ1/2/4/5). Trois familles — prairie haute (H1), prairie basse (H2),
prairie humide / laîches (H3) — posées par règles sur les espaces ouverts de la vallée écrite
(Human_Geography_V2 : vallées A, B, passage), sur le sol et l'eau rendus, après les arbres.
Voir `docs/unreal/GROUND_COVER_001.md`.

## FILES_OWNED

- `Source/Anastasis_UnrealV2/WorldView/AnastasisGroundCover.h` / `.cpp` (nouveaux)
- `Source/Anastasis_UnrealV2/WorldView/AnastasisGroundCoverTests.cpp` (nouveau)
- `Source/Anastasis_UnrealV2/WorldView/AnastasisWorldEmbodiment.h` / `.cpp` — CVar `anastasis.Dressing.GroundCover`, `PlaceGroundCover` appelé en fin de `PlaceDressing`, couronnes posées collectées dans la boucle forêt (aucun changement de placement des arbres)
- `Content/Anastasis/GroundCover/SM_Grass_{MeadowTall,MeadowShort,Sedge}_01.uasset`, `Content/Anastasis/Materials/M_AnastasisGrass.uasset` (nouveaux, LFS, générés)
- `tools/unreal/create-ground-cover.ps1` / `.py`, `tools/unreal/capture-ground-cover.ps1`, `tools/unreal/ground-cover-capture.py` (nouveaux), index dans `AGENTS.md`
- `docs/unreal/GROUND_COVER_001.md`, cette fiche, `docs/visual/ground-cover-001/`

## COMMIT

BRANCH_HEAD

## MEC

- BUILD: `BUILD::PASS` (`agent-worktree.ps1 finish`, branche rebasée sur `main` e05875c).
- UNITY: prouvé après commit et rebase — `Build.bat` direct sur arbre propre, aucune exclusion
  adaptative, `Module.Anastasis_UnrealV2.{1,2,3}.cpp` recompilés, `Result: Succeeded`.
- TESTS: `TESTS::PASS` — 163 PASS / 4 KNOWN_EXPECTED_FAILURE / 0 FAIL, 167 annoncés, 167 vus
  (`finish` → `report-tests.ps1`, après rebase ; avant rebase : 153/4/0 sur 157), dont les nouveaux :
  - PASS `Anastasis.GroundCover.Determinism` (plan bit à bit, la graine le change, instances sur le sol lu)
  - PASS `Anastasis.GroundCover.SlopeBands` (plat : tall=3803 short=1069 ; 16° : tall=0 short=3743 ; 26° : 0)
  - PASS `Anastasis.GroundCover.WaterAndWetness` (rien sous l'eau ; laîches à < 5 m de la rivière : 512, prairie : 0 ; sol à 20 cm de la nappe : 100 % laîches)
  - PASS `Anastasis.GroundCover.CanopyAndClearing` (rien sous 0,8 rayon de couronne ; clairière piétinée : 275 → 84 touffes, 0 haute)
  - PASS `Anastasis.GroundCover.PatchesAndMask` (rien hors masque ; carrés de 10 m p10/p50/p90 = 30/50/66 ; entrées invalides refusées ; plafond signalé)
- ASSETS: `GROUND_COVER_ASSETS::PASS SM_Grass_MeadowTall_01=[1738, 620, 67] SM_Grass_MeadowShort_01=[1200, 339, 75] SM_Grass_Sedge_01=[1080, 336, 34]`, 0 ensure (éditeur ouvert sur `/Engine/Maps/Entry`)
- COMMANDS:
  - `tools\unreal\anastasis-unreal.ps1 build`
  - `tools\unreal\create-ground-cover.ps1`
  - `tools\unreal\capture-ground-cover.ps1 -Label v3`
  - `tools\unreal\report-tests.ps1`

## SCN

PASS (éditeur dédié, `Lvl_AnastasisSlice`, `EmbodyCanonical(12345)`, trois incarnations au plan identique) :

```
ANASTASIS_GROUND_COVER enabled=1 tall=136447 short=79033 sedge=52672 placed=268152
  candidates=630309 refused_mask=64355 refused_ground=0 refused_water=48595 refused_slope=48864
  refused_canopy=17955 refused_density=182388 crowns=16424 clearings=1 truncated=0
  missing_meshes=0 plan_ms=701.1 total_ms=1302.7
```

A/B à caméras identiques (`anastasis.Dressing.GroundCover 1/0`) : `docs/visual/ground-cover-001/`.
Jugement : à hauteur d'homme, la vallée se lit comme une prairie de fin d'été jusqu'à ~100 m ;
la rive humide est distincte. Coût : frame p50 +3 à +14 ms selon la vue (machine partagée).

## PLY

NOT_IMPLEMENTED — aucune interaction joueur ; l'herbe n'a ni collision ni navigation.

## INTEGRATION_RISK

- `AnastasisWorldEmbodiment.cpp` est chaud (drainage, lieux, forêt) : l'ajout est une fonction
  isolée + trois lignes dans `PlaceDressing`.
- Noms `AnastasisGroundCover::*` qualifiés partout (tests compris) : les using-directives de
  fichier d'`AnastasisPlaces` / `AnastasisWorldView` déclarent aussi `FPlan` / `FInputs` en unity.
- Les éditeurs ouverts sur `Lvl_AnastasisSlice` chargent désormais ~268 000 instances d'herbe :
  mémoire et temps d'incarnation en hausse (+0,7-1,3 s). `anastasis.Dressing.GroundCover 0` les coupe.
- Crash Python à la fermeture des éditeurs de capture (après `GROUND_CAPTURE_COMPLETE`) : même
  signature préexistante dans `places-capture.py` ; non corrigé ici.

## STOP

Ne revendique pas : joncs (H4), herbacées de sous-bois (H5), lande d'éboulis (H6), fleurs,
buissons ; aucune modification du matériau de sol (`MI_AnastasisGround`), dont le vert pâle
perce entre les touffes ; aucun budget de performance validé ; la clairière du hameau n'a pas
d'effet visible sur la carte réelle (hameau hors masque de vallée).
