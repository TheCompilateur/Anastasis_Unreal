# HANDOFF: ground-cover-001

## MISSION

GROUND_COVER_001 : strate herbacée sur toute la carte, lue sur les planches État Zéro
(`docs/visual/reference/`, EZ1/2/4/5). Trois familles — prairie haute (H1), prairie basse (H2),
prairie humide / laîches (H3) — posées par règles sur les espaces ouverts (vallée écrite
Human_Geography_V2 et ouverture des tuiles de simulation), sur le sol et l'eau rendus, après les
arbres ; coût ramené à ~+1,5-2 ms de frame par tuilage des HISM.
Voir `docs/unreal/GROUND_COVER_001.md`.

## FILES_OWNED

- `Source/Anastasis_UnrealV2/WorldView/AnastasisGroundCover.h` / `.cpp` (nouveaux)
- `Source/Anastasis_UnrealV2/WorldView/AnastasisGroundCoverTests.cpp` (nouveau)
- `Source/Anastasis_UnrealV2/WorldView/AnastasisWorldEmbodiment.h` / `.cpp` — CVars `anastasis.Dressing.GroundCover`, `anastasis.GroundCover.Shadows`, `PlaceGroundCover` appelé en fin de `PlaceDressing`, couronnes posées collectées dans la boucle forêt (aucun changement de placement des arbres), `GetFrameTimingsMs` (métrologie, BlueprintCallable)
- `Source/Anastasis_UnrealV2/Anastasis_UnrealV2.Build.cs` — dépendances privées `RenderCore`, `RHI` (temps de stat unit)
- `Content/Anastasis/GroundCover/SM_Grass_{MeadowTall,MeadowShort,Sedge}_01.uasset`, `Content/Anastasis/Materials/M_AnastasisGrass.uasset` (nouveaux, LFS, générés)
- `tools/unreal/create-ground-cover.ps1` / `.py`, `tools/unreal/capture-ground-cover.ps1`, `tools/unreal/ground-cover-capture.py` (nouveaux), index dans `AGENTS.md`
- `docs/unreal/GROUND_COVER_001.md`, cette fiche, `docs/visual/ground-cover-001/`

## COMMIT

BRANCH_HEAD

## MEC

- BUILD / UNITY / TESTS : voir la section « Preuves finales » en bas, refaites après le dernier rebase.
- Nouveaux tests `Anastasis.GroundCover.*` (valeurs du run avant tuilage, règles inchangées depuis) :
  - `Determinism` (plan bit à bit y compris le tirage proche/lointain, graine qui le change, instances sur le sol lu ; Build parallèle)
  - `SlopeBands` (plat : tall=3803 short=1069 ; 16° : tall=0 short=3743 ; 26° : 0)
  - `WaterAndWetness` (rien sous l'eau ; laîches à < 5 m de la rivière : 512, prairie : 0 ; sol à 20 cm de la nappe : 100 % laîches)
  - `CanopyAndClearing` (rien sous 0,8 rayon de couronne ; clairière piétinée : 275 → 84 touffes, 0 haute)
  - `PatchesAndMask` (rien hors masque ; carrés de 10 m p10/p50/p90 = 30/50/66 ; entrées invalides refusées ; plafond signalé)
- ASSETS: `GROUND_COVER_ASSETS::PASS SM_Grass_MeadowTall_01=[1738, 620, 67] SM_Grass_MeadowShort_01=[1200, 339, 75] SM_Grass_Sedge_01=[1080, 336, 34]`, 0 ensure (éditeur ouvert sur `/Engine/Maps/Entry`)
- COMMANDS:
  - `tools\unreal\anastasis-unreal.ps1 build`
  - `tools\unreal\create-ground-cover.ps1`
  - `tools\unreal\capture-ground-cover.ps1 -Label v6 -States on,off,on2`
  - `tools\unreal\agent-worktree.ps1 finish -Mission ground-cover-001`

## SCN

PASS (éditeur dédié, `Lvl_AnastasisSlice`, `EmbodyCanonical(12345)`, quatre incarnations au plan identique) :

```
ANASTASIS_GROUND_COVER enabled=1 tall=507198 short=416763 sedge=63666 placed=987627
  near=652862 far=334765 chunks=683 outside_valley=690560 shadows=1 candidates=2560000
  refused_mask=0 refused_ground=56232 refused_water=251752 refused_slope=271855
  refused_canopy=272081 refused_density=720453 crowns=16424 clearings=1 truncated=0
  missing_meshes=0 plan_ms=475.8 total_ms=1865.2
```

A/B à caméras identiques : `docs/visual/ground-cover-001/` (dont `hors_vallee_eye`, choisie
parmi les touffes posées loin de la vallée). Coût, frame p50 même session : off 9,9-12,2 ms,
on 10,5-14,9 ms, on2 10,7-13,6 ms ; GPU +~1,5 ms. Avant tuilage (v4) : +12 ms dans chaque vue.
Ombres coupées : aucun gain mesurable (v5).

## PLY

NOT_IMPLEMENTED — aucune interaction joueur ; l'herbe n'a ni collision ni navigation.

## INTEGRATION_RISK

- `AnastasisWorldEmbodiment.cpp` est chaud (drainage, lieux, forêt) : l'ajout est une fonction
  isolée, un espace `AnastasisGroundCoverEmbody`, trois lignes dans `PlaceDressing`.
- Noms `AnastasisGroundCover::*` qualifiés partout (tests compris) : les using-directives de
  fichier d'`AnastasisPlaces` / `AnastasisWorldView` déclarent aussi `FPlan` / `FInputs` en unity.
- Tout éditeur ouvert sur `Lvl_AnastasisSlice` charge ~1 M d'instances d'herbe en 683 HISM :
  mémoire en hausse, incarnation +1,4-2 s. `anastasis.Dressing.GroundCover 0` les coupe.
- Crash Python à la fermeture des éditeurs de capture (après `GROUND_CAPTURE_COMPLETE`) : même
  signature préexistante dans `places-capture.py` ; non corrigé ici.

## STOP

Ne revendique pas : joncs (H4), herbacées de sous-bois (H5), lande d'éboulis (H6), fleurs,
buissons ; aucune modification du matériau de sol (`MI_AnastasisGround`), qui perce entre les
touffes ; aucun budget de performance validé hors éditeur (render thread non mesuré, jeu cuit
non mesuré) ; la clairière du hameau n'a pas d'effet visible sur la carte réelle.

## Preuves finales

Refaites après chaque rebase (`main` a avancé trois fois pendant la passation) :

- UNITY : `Build.bat` direct sur arbre propre, aucune exclusion adaptative,
  `Module.Anastasis_UnrealV2.{1,2,3}.cpp` + `Module.AnastasisSim.cpp` recompilés, `Result: Succeeded`.
- `agent-worktree.ps1 finish` sur `bc8974b` (base `e0be35f`) : `BUILD::PASS`, 168 PASS /
  4 KNOWN_EXPECTED_FAILURE / 0 FAIL, 172 annoncés, 172 vus, `HANDOFF_READY::YES`.
- Rejoués sur la base finale juste avant `integrate` (même commande, enchaînée) ; le versement
  s'arrête au premier échec.
