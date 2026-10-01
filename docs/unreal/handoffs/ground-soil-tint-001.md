# HANDOFF: ground-soil-tint-001

## MISSION

Sol sous l'herbe : teinter la couleur de sommet du sol par la couverture d'herbe réellement posée
(prairie : sol foncé et verdi ; laîches : plus sombre ; lande : terre brune), pour que les touffes
se fondent dans leur sol de près et que la vallée se lise en prairie de loin, où l'herbe est coupée.
Voir `docs/unreal/GROUND_COVER_001.md`, section « Sol sous l'herbe ».

## FILES_OWNED

- `Source/Anastasis_UnrealV2/WorldView/AnastasisGroundCover.h` / `.cpp` — `FCoverField`, `BuildCoverField`, `FSoilTint`, `TintSoil` (purs)
- `Source/Anastasis_UnrealV2/WorldView/AnastasisGroundCoverTests.cpp` — nouveau `Anastasis.GroundCover.SoilTint`
- `Source/Anastasis_UnrealV2/WorldView/AnastasisWorldEmbodiment.cpp` — CVar `anastasis.GroundCover.SoilTint`, teinte de la section de sol en fin de `PlaceGroundCover`, ligne `ANASTASIS_SOIL_TINT`
- `tools/unreal/ground-cover-capture.py` (état `notint`, vue `aerien`), index dans `AGENTS.md`
- `docs/unreal/GROUND_COVER_001.md`, cette fiche, `docs/visual/ground-soil-tint-001/`

Aucun asset modifié : `M_AnastasisGround` / `MI_AnastasisGround` (autre chantier) ne sont pas touchés.

## COMMIT

BRANCH_HEAD

## MEC

- BUILD: `BUILD::PASS` (`agent-worktree.ps1 finish`, branche rebasée sur `main` 13936e4).
- UNITY: `Build.bat` direct sur arbre propre après commit : aucune exclusion adaptative,
  `Module.Anastasis_UnrealV2.{1,2,3}.cpp` et `Module.AnastasisSim.cpp` recompilés, `Result: Succeeded`.
- TESTS: `TESTS::PASS` — 188 PASS / 4 KNOWN_EXPECTED_FAILURE / 0 FAIL, 192 annoncés, 192 vus.
  - PASS `Anastasis.GroundCover.SoilTint` (couverture prairie à l'ouest 0,47, nulle à l'est et hors
    grille ; sol nu intact ; alpha intact ; prairie plus sombre ; sable sous prairie plus vert ;
    lande plus brune que prairie ; laîches les plus sombres ; couverture clairsemée = teinte moindre)
- COMMANDS:
  - `tools\unreal\capture-ground-cover.ps1 -Label tint-v2 -States on,notint,off`
  - `tools\unreal\agent-worktree.ps1 finish -Mission ground-soil-tint-001`

## SCN

PASS (éditeur dédié, `Lvl_AnastasisSlice`, `EmbodyCanonical(12345)`, capture `tint-v2`) :

```
ANASTASIS_SOIL_TINT enabled=1 tinted_vertices=135805 mean_amount=0.522
ANASTASIS_SOIL_TINT enabled=0 tinted_vertices=0 mean_amount=0.000
```

A/B à mêmes touffes : `docs/visual/ground-soil-tint-001/`. GPU identique avec et sans teinte.

## PLY

NOT_IMPLEMENTED.

## INTEGRATION_RISK

- Écrit dans la couleur de sommet de la section de sol APRÈS `CreateMeshSection` : toute mission qui
  recrée ou met à jour la section 0 après `PlaceDressing` effacerait la teinte (aucune aujourd'hui).
- `UpdateMeshSection` repasse les positions (inchangées) : la collision reçoit les mêmes sommets
  (`UpdateTriMeshVertices`), sans recuisson.
- Le matériau de sol multiplie par la couleur de sommet : un changement de `ground-material.py` qui
  cesserait de le faire rendrait la teinte invisible.

## STOP

Ne revendique pas : modification du matériau de sol ; teinte hors herbe (sable, roche inchangés) ;
calibration contre une planche autre que la lecture d'EZ5 (vert sourd sous prairie).
