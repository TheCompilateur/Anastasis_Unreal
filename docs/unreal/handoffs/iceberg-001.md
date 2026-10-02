# HANDOFF: iceberg-001

## MISSION

ICEBERG_001 : le monde visible dit-il la vérité sur le monde simulé ? Audit court du territoire
(`docs/unreal/ICEBERG_001.md`), puis une tranche verticale sur la seule branche libre : les maisons.
Une maison habitée s'allume la nuit, une maison que la simulation dit vide reste noire. Test mental :
une société meurt, ses bâtiments restent ; le joueur peut-il le voir ?

## FILES_OWNED

- `Source/Anastasis_UnrealV2/Village/AnastasisBuildingMetabolism.{h,cpp}` (traduction pure sim → foyer)
- `Source/Anastasis_UnrealV2/Village/AnastasisBuildingMetabolismTests.cpp`
- `Source/Anastasis_UnrealV2/Village/AnastasisVillageBuilding.{h,cpp}` (`SetHearth`, `UPointLightComponent`)
- `Source/Anastasis_UnrealV2/Village/AnastasisVillagePresentation.{h,cpp}` (`Sync` : lumière du jour, mode)
- `Source/Anastasis_UnrealV2/Sim/AnastasisSimulationSubsystem.cpp` (CVar, lecture de la lumière du ciel)
- `tools/unreal/metabolism-pie.py`, `tools/unreal/proofs.txt`, `AGENTS.md` (une ligne d'index)
- `docs/unreal/ICEBERG_001.md`, cette fiche

## COMMIT

`50a173d` — « feat(village): une maison habitee s'allume la nuit, une maison vide reste noire (ICEBERG_001) ».
Une fiche ajoutée ensuite exige un nouveau `finish`.

## MEC

- BUILD: PASS — `tools\unreal\anastasis-unreal.ps1 build` dans ce worktree.
- TESTS: PASS 3/3, 0 échec connu, `report-tests.ps1 -Filter Anastasis.Village.Metabolism` :
  `Anastasis.Village.Metabolism.Occupancy`, `.GhostSettlement`, `.Projection`.
  Valeurs : maison vide, lumière 0 à toute heure en mode Truth (balayage 0..1 par 0,01) ; le témoin faux
  l'allume à > 0,99 ; maison habitée en pleine nuit > 0,99 ; foyer sorti = 0,25 (braises) ; chantier = 0 ;
  `RemoveNpc` du seul habitant : le bâtiment reste et la lumière tombe à 0.
- COMMANDS:
  - `tools\unreal\anastasis-unreal.ps1 build`
  - `tools\unreal\report-tests.ps1 -Filter Anastasis.Village.Metabolism`
  - `tools\unreal\editor-batch.ps1 -Proofs metabolism-pie` → `PROOF::PASS metabolism-pie (109.3s)`

## PROOFS

PROOFS: metabolism-pie

## SCN

PASS en PIE sur `Lvl_AnastasisSlice` (`METABOLISM_PIE PASS`, `Saved/MetabolismEvidence/pie/metabolism.json`).
La preuve lit le composant de lumière réel de chaque acteur maison (deux maisons, `building-0` et
`building-1`), temps avancé par `Anastasis.Sim.Advance`, jamais attendu :

| Lecture | Phase | Lumière |
|---|---|---|
| midi | midday | 2 maisons éteintes (visible faux, intensité 0) |
| nuit, habitants dedans | night | 2 maisons allumées, intensité 400 |
| nuit, `RemoveNpc` de tous | night | 2 maisons éteintes, les acteurs sont toujours là |
| nuit, témoin faux (`Metabolism 2`) | night | les mêmes maisons vides rallumées, 400 |
| retour à `Metabolism 1` | night | éteintes de nouveau |

C'est la preuve que le témoin faux porte une information fausse que la projection corrige.

## PLY

UNKNOWN. La mortalité n'est pas portée dans la simulation Unreal : `RemoveNpc` n'est qu'une commande
console. Dans une partie réelle, une maison n'est vide que si elle n'a pas encore reçu de propriétaire.
Le mécanisme existe ; son occurrence naturelle, non. Aucune partie représentative n'a été jouée.

## ECARTS

AUCUN — la mission ne touche pas `Source/AnastasisSim/` : le projet ne lit la simulation qu'en lecture
seule (`CountShelterOccupants`, `InsideOf`, `IsCompleted`).

## INTEGRATION_RISK

- Aucun autre agent actif ne touche `Village/` (vérifié par `git diff main...agent/*` sur ces chemins).
- `AnastasisSimulationSubsystem.cpp` est touché par la branche `agent/push-replay` : revoir la fusion.
- `AnastasisWorldAtmosphere.h` n'est qu'inclus (lecture de `GetLastSkyState`) ; les branches
  `fog-fsss-001` et `air-relief-002` le modifient.
- `abandon-001` et `mortality-001` reposent sur cette branche : intégrer celle-ci d'abord.
- Coût : un `UPointLightComponent` par bâtiment (puits et greniers inclus, invisibles) ; ombres actives
  sur la lumière d'une maison allumée. Non mesuré.

## STOP

Ne revendique pas : l'apparence. Aucune capture n'a été faite ; l'intensité 400 cd (CVar
`anastasis.Village.HearthCandela`) n'est pas réglée à l'œil et peut éblouir ou disparaître selon
l'exposition de nuit. Pas de fumée de cheminée (aucun Niagara dans le projet), donc rien ne distingue
le jour une maison vivante d'une maison vide. Le reste de l'audit (routes, forêt, champs) est dans
`docs/unreal/ICEBERG_001.md` et appartient à d'autres agents.
