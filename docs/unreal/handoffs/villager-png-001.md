# HANDOFF: villager-png-001

## MISSION

Donner un visage aux habitants simules, par des PNG detoures, sans toucher au simulateur, et les faire
apparaitre dans le jeu. 104 individus decoupes des planches d'Alexandre (series 1, 2, 4, 5), une carte
portrait par habitant simule, 12 habitants au lancement. Fiche complete : `docs/unreal/VILLAGER_PNG_001.md`.

## FILES_OWNED

- `Source/Anastasis_UnrealV2/Village/AnastasisVillagerLooks.h` / `.cpp`
- `Source/Anastasis_UnrealV2/Village/AnastasisVillagerVisual.h` / `.cpp`
- `Source/Anastasis_UnrealV2/Village/AnastasisVillagerTests.cpp`
- `Source/Anastasis_UnrealV2/Village/AnastasisVillagePresentation.h` / `.cpp` — `SyncVillagers`, `FindVillager`, `Clear` (partage avec village-buildings-001)
- `Source/Anastasis_UnrealV2/Sim/AnastasisSimulationSubsystem.h` / `.cpp` — appel dans `Tick`, CVars `anastasis.Village.Portraits` / `anastasis.Village.StartVillagers`, village du lancement, `ReplaceStartVillage` en tete des cinq `Seed*`, `GetVillagerCards`
- `Source/Anastasis_UnrealV2/WorldView/AnastasisPresentationRegistry.h` — `EAnastasisVillagerCategory`, `FAnastasisVillagerLook`, `Villagers`, `VillagerMaterial`
- `Content/Anastasis/Characters/` — 104 textures `PNG/<Categorie>/CHR_*`, `M_AnastasisVillager`
- `Content/Anastasis/Presentation/DA_AnastasisPresentation.uasset` — champs `Villagers` et `VillagerMaterial` (le reste du registre inchange)
- `SourceArt/Characters/` — planches, manifeste, extraction, `Raw/`, `PNG/`
- `tools/unreal/villager-png.py`, `import-villagers.ps1` / `.py`, `villager-lineup.ps1` / `.py`, `villager-pie.ps1` / `.py`
- `docs/unreal/VILLAGER_PNG_001.md`, `docs/visual/villager-png-001/`, `AGENTS.md` — une section d'index
- cette fiche

## COMMIT

Voir le commit qui ajoute cette fiche sur `agent/villager-png-001`.

## MEC

- BUILD: PASS — `tools\unreal\anastasis-unreal.ps1 build` dans ce worktree ; unity force verifie une fois (`Build.bat ... -DisableAdaptiveUnity`, Result: Succeeded) sur l'etat d'avant le village du lancement.
- TESTS: `Automation RunTests Anastasis.Village` : 7/7 Success, dont `Anastasis.Village.Villagers.LookPool` et `.Presentation`. Suite complete relancee par le portail finish.
- COMMANDS:
  - `python tools/unreal/villager-png.py sheets` — `SHEETS::TOTAL ... total=104 en_jeu=62`, 0 fusion
  - `python tools/unreal/villager-png.py prep` / `board` / `check`
  - `tools\unreal\import-villagers.ps1` — `VILLAGERS_IMPORT::PASS imported=104 registry=104 failures=0`, `MATERIAL_OK`
  - `tools\unreal\villager-lineup.ps1 -Label population-v1` — `VILLAGER_LINEUP::PASS shots=13/13`
  - `tools\unreal\villager-pie.ps1` — `VILLAGER_PIE PASS`

## SCN

Planches hors moteur (`docs/visual/villager-png-001/A`..`E`) et dans Unreal (`I`..`K`) : 104 individus, statures
lisibles contre un temoin de 180 cm, cartes eclairees comme le sol, ombre partant des pieds.

## PLY

PIE sur `Lvl_AnastasisSlice` sans aucune commande : 12 habitants et 12 cartes au lancement
(`F_jeu_demarrage_sans_commande.png`). `Anastasis.Village.FirstWell 12` remplace ce village (12, pas 24) ;
sphere de simulation et carte aux memes pieds (`G`) ; portraits distincts, aucun assis ; `RemoveNpc` retire la
carte. `PLAYER` reste NOT_IMPLEMENTED.

## INTEGRATION_RISK

- **Le jeu ne s'ouvre plus vide.** Toute preuve PIE qui compte les habitants ou les batiments SANS lancer de
  scenario voit desormais 12 habitants et un puits. Les scenarios `First*` / `FoodSupply` remplacent ce
  village (meme graine, temps remis a 0) : leurs comptes sont ceux d'avant. `anastasis.Village.StartVillagers 0`
  rend l'ancien comportement.
- `DA_AnastasisPresentation.uasset` est binaire : une autre branche qui le modifie ne se fusionne pas ; il
  faudra rejouer `import-villagers.ps1` sur l'asset de l'autre.
- `AnastasisVillagePresentation.h` : conflit de commentaire deja resolu avec village-buildings-001.
- **Rythme du jeu change (point 4)** : `anastasis.Sim.TimeScale` 0.0375 par defaut -- un jour dure ~40 min
  reelles au lieu de 90 s, habitants ~3 m/s au lieu de 80 m/s. Toute preuve PIE qui attend sur le temps
  simule doit poser `anastasis.Sim.TimeScale 1` (fait pour les neuf scripts existants de `tools/unreal/`).
  `anastasis.Sim.Speed 0` ne gele pas : geler par `TimeScale 0`.
- Avance rapide seulement.

## SUITE (critique d'Alexandre, point 1)

Portraits choisis par metier simule (`FAnastasisVillagerLook::Jobs`, pool par metier, demographie visee,
carte redessinee a l'embauche) : un garde, un moine, une mere au bebe ne sont plus jamais attribues.
`Anastasis.Village` 7/7 Success ; `VILLAGER_PIE PASS` avec `FirstFarmer 1` (fermier en portrait de
fermier).

## SUITE (critique d'Alexandre, point 4)

`anastasis.Sim.TimeScale` 0.0375 et cartes interpolees entre les pas : en PIE, 2,99 m/s de moyenne et 19,5 cm
au plus par frame a ~28 images/s (run 12:54) ; `VILLAGER_PIE PASS` (run 13:01, pointe 3,0 m/s). Neuf preuves PIE
existantes posent `TimeScale 1`. `Anastasis.Village` 7/7 Success, dont l'interpolation.

## STOP

Aucun code de simulation PNJ modifie. Pas d'animation, pas de vues de dos ou de profil, pas de portrait par
metier, pas d'enfants en jeu (la simulation n'en a pas). La diversite des hommes adultes est celle des
planches. La premiere prise `02-debug` vide n'a pas de cause etablie.
