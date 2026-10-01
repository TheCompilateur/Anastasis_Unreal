# HANDOFF: villager-body-3d-001

## MISSION

Critique d'Alexandre (2026-10-01) : les PNJ sont des images collees sur des objets en mouvement, visage,
mains et pieds immobiles. Option C : corps 3D animes de pres, cartes portrait au loin. Puis les teintes du
corps mesurees sur le portrait. Simulateur intouche. Voir `docs/unreal/VILLAGER_BODY_3D_001.md`.

## FILES_OWNED

- `Source/Anastasis_UnrealV2/Village/AnastasisVillagerVisual.h/.cpp` (corps squelettique, bascule, CVars `anastasis.Village.Bodies` / `BodyDistance`)
- `Source/Anastasis_UnrealV2/Village/AnastasisVillagerLooks.h/.cpp` (`FBodyLook`, `BodyLookFor`, `ColourContrast`)
- `Source/Anastasis_UnrealV2/Village/AnastasisVillagePresentation.h/.cpp` (`SetBody` a la creation de la carte)
- `Source/Anastasis_UnrealV2/Village/AnastasisVillagerTests.cpp` (`Villagers.BodyLook`, `Villagers.Body`)
- `Source/Anastasis_UnrealV2/WorldView/AnastasisPresentationRegistry.h` (`VillagerBody*`, `VillagerLocomotion`, `FAnastasisVillagerLook::Body*`)
- `Source/Anastasis_UnrealV2/Sim/AnastasisSimulationSubsystem.cpp` (`GetVillagerCards` : `has_body`, `body`, `speed`, `heading`)
- `tools/unreal/create-villager-body.ps1/.py` -> `Content/Anastasis/Characters/M_AnastasisVillagerBody.uasset`
- `tools/unreal/villager-body-pie.ps1/.py`
- `tools/unreal/villager-png.py` (commande `colours`) -> `SourceArt/Characters/villager-colours.json`
- `tools/unreal/import-villagers.py` (teintes `Body*` au registre) -> `Content/Anastasis/Presentation/DA_AnastasisPresentation.uasset`
- `docs/unreal/VILLAGER_BODY_3D_001.md`, `docs/visual/villager-body-3d-001/`, `AGENTS.md` (index)

## COMMIT

voir `git log agent/villager-body-3d-001`

## MEC

- BUILD: `BUILD::PASS` (worktree, apres la derniere modification C++)
- TESTS: voir la sortie de `finish` (report-tests) ; nouveaux : `Anastasis.Village.Villagers.BodyLook`, `Anastasis.Village.Villagers.Body`
- COMMANDS:
  - `tools\unreal\create-villager-body.ps1` -> `VILLAGER_BODY::PASS`, `VILLAGER_BODY::MATERIAL_OK` ; Manny 180,5 cm, Quinn 180,2 cm ; seuils cou 0,868 ceinture 0,575 epaule 0,111 manche 0,154 cheville 0,041
  - `python tools/unreal/villager-png.py colours` -> `COLOURS::TOTAL 104 partiels=0`
  - `tools\unreal\import-villagers.ps1` -> `VILLAGERS_IMPORT::PASS imported=104 registry=104 failures=0`, `MATERIAL_OK echecs_intermediaires=0`
  - `tools\unreal\villager-body-pie.ps1` -> `VILLAGER_BODY_PIE::PASS` : pieds 88,3 / 83,5 cm sur 10 echantillons, cap 0 deg ; arret (vitesse 0) main 2,85 cm, tete 1,80 cm ; bascule 2 corps / 10 cartes puis 12 cartes a 120 m, 0 erreur

## SCN

`Lvl_AnastasisSlice`, village du lancement (12 habitants, aucune commande de scenario), rythme du jeu (`anastasis.Sim.TimeScale` 0,0375).

## PLY

PLAYER non touche (NOT_IMPLEMENTED).

## INTEGRATION_RISK

- `DA_AnastasisPresentation.uasset` (LFS, binaire) reecrit par `import-villagers.py` : toute autre mission qui ecrit le registre entre en conflit binaire -> regenerer par les scripts apres fusion, ne pas fusionner a la main.
- `AnastasisSimulationSubsystem.cpp`, `AnastasisVillagePresentation.cpp` : fichiers chauds du village.
- Les textures `CHR_*` et `M_AnastasisVillager` reecrites a l'identique par le reimport ont ete restaurees : non commitees.
- Defaut `anastasis.Village.Bodies 1` : les preuves PIE des autres missions voient desormais des corps 3D de pres au lieu des cartes ; `villager-pie.py` lit `hidden` de l'acteur, inchange.
- Assets du mannequin (`/Game/Characters/Mannequins`) deviennent une dependance du jeu (deja dans le depot).

## STOP

- Pas de visage (mannequin d'Epic), vetement peint sans volume, une seule marche et un seul repos pour tous, pas de gestes d'activite, pas d'objet porte.
- Teintes mesurees fideles au lavis sepia des planches : beaucoup d'habitants sortent bruns. Pas un rendu final.
- Cout memoire / GPU des 12 corps non mesure.
