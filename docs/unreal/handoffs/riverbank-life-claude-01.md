# HANDOFF: riverbank-life-claude-01

## MISSION

RIVERBANK_LIFE_001 : (0) regarder l'eau à 1,7 m et mesurer son coût ; (1) rives vivantes — vase et
roseaux en eau calme, gravier et galets en eau vive, selon la vitesse du réseau. Voir
`docs/unreal/RIVERBANK_LIFE_001.md`.

## FILES_OWNED

- `Source/Anastasis_UnrealV2/WorldView/AnastasisRiverbank.h` / `.cpp` / `AnastasisRiverbankTests.cpp` (nouveaux)
- `Source/Anastasis_UnrealV2/WorldView/AnastasisWorldEmbodiment.h` / `.cpp` — CVars `anastasis.Dressing.Riverbank`, `anastasis.Riverbank.InAutomation`, `PaintBanks` avant la section de sol, `PlaceRiverbank` après l'herbe
- `Source/Anastasis_UnrealV2/WorldView/AnastasisDrainage.cpp` — rive de lac continue (distance signée au masque flouté), sous `bWaterLook`
- `tools/unreal/riverbank-capture.ps1` / `.py` (nouveaux), index dans `AGENTS.md`
- `docs/unreal/RIVERBANK_LIFE_001.md`, cette fiche, `docs/visual/riverbank-life-001/`

## COMMIT

BRANCH_HEAD

## MEC

- BUILD: `BUILD::PASS` (`tools\unreal\anastasis-unreal.ps1 build`, code final)
- FINISH: `agent-worktree.ps1 finish`, branche rebasée sur `main` — build **unity** `BUILD::PASS`, `TESTS::PASS`
  198 PASS / 4 KNOWN_EXPECTED_FAILURE / 0 FAIL, 202 annoncés (sur `main` e231348 ; refait au versement).
  - Premier build unity cassé : `using namespace AnastasisRiverbank` dans les tests, ambigu (FPlan, EFamily) dans
    un lot unity — corrigé (« tests Riverbank sans using namespace »).
  - Un run sorti avec 2 FAIL dans `AI.Toolsets.AnastasisInspect` : changement de périphérique audio Windows en
    plein run (`PostDeviceSwap`), relancé sans changement ; piège ajouté à `PIEGES_UNREAL.md`.
- TESTS (avant rebase) : `TESTS::PASS` — 195 PASS / 4 KNOWN_EXPECTED_FAILURE / 0 FAIL, 199 annoncés (`tools\unreal\report-tests.ps1`), dont :
  - PASS `Anastasis.Terrain.Riverbank.CalmAndFast` — chenal synthétique : 376 roseaux / 29 massifs côté calme, 1 307 galets et 25 blocs côté vif, 0 du mauvais côté, 0 pied hors de sa bande
  - PASS `Anastasis.Terrain.Riverbank.PaintBanks` — 9 sommets de vase, 11 de gravier ; fond immergé, terre lointaine et alpha intacts
  - PASS `Anastasis.Terrain.Riverbank.CanonicalWorld` — 16 133 roseaux / 1 270 massifs, 35 465 galets, 431 blocs, vitesse max 3,50 m/s ; contrôles du réseau à 0 avec les rives de lac continues ; 0 instance hors du sol rendu
  - PASS `Anastasis.Terrain.Drainage.*` (dont `WaterLook`) inchangés
  - Un premier run est sorti `TESTS::FAIL lanceur bloque` : 11 min de démarrage de l'éditeur (shaders) sur les 900 s du lanceur, suite coupée à `HumanGeography.CollisionAndDressing`. Relancé sans changement de code : vert.
- VISUAL: `tools\unreal\riverbank-capture.ps1 -Label v3 -States "nobanks,banks"` ; planches `docs/visual/riverbank-life-001/` ; écarts `compare.py` dans la fiche de mission (avec la variance du même état, 6–16 % : l'eau est animée).
- COST: `riverbank-capture.ps1 -Label cost -States "water,flat,water2" -Profile` — une mesure valide : Single Layer Water ≈ 1,07 ms / 11,66 ms de GPU (plaine, 1,7 m). Les autres profils sont vides (voir la fiche).
- COMMANDS:
  - `tools\unreal\anastasis-unreal.ps1 build`
  - `tools\unreal\report-tests.ps1`
  - `tools\unreal\riverbank-capture.ps1 -Label audit -States "water,flat,water2"`
  - `tools\unreal\riverbank-capture.ps1 -Label v3 -States "nobanks,banks"`

## SCN

`Lvl_AnastasisSlice`, seed 12345, Human_Geography_V2 ; caméras dans `Saved/RiverbankEvidence/<label>/cameras.json`.

## PLY

NOT_IMPLEMENTED (aucun changement joueur).

## INTEGRATION_RISK

- `river-look-001` (autre agent, non versé à l'écriture de cette fiche) modifie aussi `AnastasisDrainage.cpp`
  (couleur du lit immergé, rubans), `AnastasisWorldEmbodiment.cpp` et réécrit `tools/unreal/water-look.py`.
  Pas de recouvrement de lignes avec cette branche (arrondi et grève des lacs ici ; lit et rubans là-bas) ;
  cette mission ne touche ni `water-look.py` ni `M_AnastasisWater`. Rebase à prévoir selon l'ordre de versement.
- Les vagues de rivière en bandes vues à 1,7 m (audit) relèvent de `river-look-001`.
- Instances de rive coupées sous automatisation : les tests couvrent le plan (pur), pas les HISM.

## STOP

- Les bandes de sol (vase, gravier) se voient à peine sous `M_AnastasisGround` aux caméras de preuve.
- Le coût de l'eau n'est établi qu'en un point de vue.
- `SM_Ecotone_ShoreTuft` essayée puis retirée (bulbes verts à 1,7 m).
- Pas de retouche des maillages existants (roseau, galets), ni des coins de ruban dans les coudes serrés.
