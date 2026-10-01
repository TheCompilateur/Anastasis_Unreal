# HANDOFF: village-buildings-001

## MISSION

Trois meshes à l'échelle de la tuile (4 m) pour les bâtiments que la simulation pose déjà : puits, maison, grenier. Même matière que les props (couleur de sommet, grain bois / pierre / tuile). Ils remplacent les volumes de debug dès qu'ils sont chargés.

## FILES_OWNED

- `tools/unreal/create-village-buildings.py`
- `tools/unreal/create-village-buildings.ps1`
- `Content/Anastasis/VillageBuildings/M_VillageBuilding_Surface.uasset`
- `Content/Anastasis/VillageBuildings/SM_Well_Stone_01.uasset`
- `Content/Anastasis/VillageBuildings/SM_House_Refuge_01.uasset`
- `Content/Anastasis/VillageBuildings/SM_Granary_Raised_01.uasset`
- `Source/Anastasis_UnrealV2/Village/AnastasisVillageBuilding.h`
- `Source/Anastasis_UnrealV2/Village/AnastasisVillageBuilding.cpp`
- `Source/Anastasis_UnrealV2/Village/AnastasisVillagePresentation.h`
- `Source/Anastasis_UnrealV2/Village/AnastasisVillagePresentation.cpp`
- `AGENTS.md` — une ligne d'index
- cette fiche

## COMMIT

Voir le commit qui ajoute cette fiche sur `agent/village-buildings-001`.

## MEC

- BUILD: PASS — `tools\unreal\anastasis-unreal.ps1 build` dans ce worktree, avant le commit. `AnastasisVillageBuilding.cpp` et `AnastasisVillagePresentation.cpp` compilés hors unity.
- TESTS: le portail finish relance `report-tests.ps1`. Pas de suite lancée dans cette fiche avant ce commit.
- COMMANDS:
  - `$env:ANASTASIS_BUILDINGS_GEOMETRY_ONLY='1'; py -3 tools\unreal\create-village-buildings.py` — géométrie seule, COMPLETE
  - `tools\unreal\create-village-buildings.ps1` — éditeur, `BUILDINGS::PASS`, trois assets sauvés

## SCN

Les meshes sont sauvés. Emprises mesurées par le script : puits 189 × 232 × 198 cm, maison 360 × 310 × 438 cm, grenier 252 × 258 × 299 cm. Pivot au sol, centre XY d'auteur. Aucune capture du village avec ces meshes à la place des boîtes.

## PLY

UNKNOWN — les habitants restent des sphères de debug. `PLAYER` reste NOT_IMPLEMENTED.

## INTEGRATION_RISK

- `AnastasisVillagePresentation.cpp` : le spawn oriente la porte vers le premier point d'accès (+Y d'auteur pour la maison et le grenier, -90°). Les tests de position ne lisent que la translation.
- Si le mesh manque, les boîtes de debug restent. Un asset absent ne casse pas la simulation.
- Collision `BlockAll` sur le mesh. La navigation Unreal n'est pas affectée (`SetCanEverAffectNavigation(false)`).
- Pas de rebase revendiqué. Avance rapide seulement.

## STOP

Pas de PNJ. Pas de matériau de sol. Pas de preuve visuelle dans le niveau. L'atelier n'a pas de mesh.
