# HANDOFF: dormir-couche-001

## MISSION

« Dormir couché » (consigne d'Alexandre, 2026-10-09, après les images de la cabane : « ainsi pour tout le monde, PNJ »,
`docs/unreal/DORMIR_COUCHE_001.md`) : qui dort dans un logis — le joueur comme les habitants — est couché sur une
banquette-lit (puis une natte au sol), et la nuit l'intérieur d'un logis n'est plus brûlé de blanc par le foyer.

RELAIS: ma-cabane-001

## FILES_OWNED

- `Source/Anastasis_UnrealV2/Village/AnastasisArchitecture.{h,cpp}` (`FBench`, `FSleepSpot`, `Benches`, `Interior`, `SleepSpots`) ; `AnastasisArchitectureTests.cpp` (`Rapport` relit banquettes et volume habité, nouveau `Couchages`)
- `Source/Anastasis_UnrealV2/Village/AnastasisVillagerVisual.{h,cpp}` (`SetLying`, `IsLying`) ; `AnastasisVillagePresentation.{h,cpp}` (`SleepSpotFor`, les dormeurs couchés au lieu de cachés, `SetInteriorDaylight`) ; `AnastasisVillageBuilding.{h,cpp}` (boîte du volume habité + post-process d'exposition, `anastasis.Village.InteriorLight` / `InteriorNightEV` / `InteriorDayEV`)
- `Source/Anastasis_UnrealV2/Sim/AnastasisSimulationPlayer.cpp` (le pawn couché à sa place, `get_sleep_status`) ; `AnastasisSimulationSubsystem.h`
- `tools/unreal/create-village-architecture.py` (`benches`, `interior` au rapport), `docs/unreal/architecture/architecture-kit-001.json` (ces deux champs ajoutés, rien d'autre ne bouge)
- `tools/unreal/sommeil-pie.py` (nouveau), `tools/unreal/proofs.txt`, `AGENTS.md` (une ligne d'index)
- `docs/unreal/DORMIR_COUCHE_001.md`, `docs/unreal/dormir-couche-001/`, cette fiche

## COMMIT

Le dernier commit de la branche `agent/dormir-couche-001` (marqué par `finish`).

## MEC

- BUILD : `anastasis-unreal.ps1 build` -> `BUILD::PASS`.
- TESTS : la suite sans rendu de `finish` (dont `Anastasis.Village.Architecture.Rapport` et `.Couchages`).
- PROOF : `editor-batch.ps1 -Proofs sommeil-pie,cabane-pie` -> `PROOF::PASS sommeil-pie`, `PROOF::PASS cabane-pie` ; `SOMMEIL_PIE PASS sleepers=2 house=building-1 bad=[] player_tilt=0 checks=player_lying=1,house_sleepers=1,all_lying=1,interior_night_ev=1,shots=1`.
- EXPOSITION (même caméra, luminosité moyenne 0-255, `ANASTASIS_SLEEP_EV_SWEEP=1,12`) : ferme, exposition du dehors 223,0 (blanc) -> intérieur EV 5,5 : 88,8 ; cabane EV 1 : 167,3, EV 5,5 : 37,0, EV 12 : 0,1.

## PROOFS

PROOFS: sommeil-pie, cabane-pie, house-rest-pie, metabolism-pie

## SCN

`sommeil-pie`, échantillon dans `docs/unreal/dormir-couche-001/` (images réduites + `sleep.json`) :

- la nuit du jour 2, trois dormeurs dans un logis : npc-0 dans la ferme (building-1), npc-1 dans la maison moyenne (building-3), le joueur dans sa cabane ; tous trois couchés à leur place (corps à 0° de l'horizontale), visibles, dans le volume habité ; les quatre logis à l'exposition d'intérieur de nuit (EV100 5,5) ;
- `01-maison-nuit-avant` / `02-maison-nuit` : la même caméra dans la ferme, exposition du dehors (tout blanc) puis d'intérieur : la pièce se lit, l'habitant dort sur la banquette à côté de l'âtre ;
- `03-cabane-nuit` : le joueur couché sur sa banquette, la pièce sombre éclairée par le foyer ;
- `ev-1-cabane` / `ev-12-cabane` : témoins, l'exposition d'intérieur commande bien l'image ;
- limites visibles : le corps du joueur garde un genou relevé (son animation de personnage, pas celle des habitants) ; `04-maison-dehors-nuit` est presque noire (la nuit au clair de lune), seules les fenêtres de l'étage éclairées se lisent.

Piège trouvé : le post-process borné ne s'applique que si sa boîte a une collision de requête (`QueryOnly`, aucun canal) ; sans elle (`NoCollision`), aucune caméra n'est jamais « dedans » et l'image ne change pas (mesuré : 208,7 / 208,6 à EV 7 / 8,5).

## PLY

NOT_JUDGED — Alexandre regarde les images.

## ECARTS

AUCUN — rien dans `Source/AnastasisSim/` : ni la simulation ni la référence ne sont touchées, seulement la présentation
(où le corps est posé, l'exposition dans un logis). Relayé : n° 56 (ma-cabane-001).

## INTEGRATION_RISK

- Relais : porte `ma-cabane-001`. La verser avant, ou dans le même lot en la nommant d'abord.
- Les habitants qui dorment ne sont plus cachés : une preuve qui comptait les acteurs visibles la nuit en voit davantage.
- Un post-process borné par logis (priorité 10) : il ne règle que l'exposition (min = max), rien d'autre.

## STOP

- Pas d'animation de sommeil : le corps couché garde la pose de repos debout ; celui du joueur garde un genou relevé.
- La vue du joueur endormi n'est pas traitée.
- Manger ou se détendre dedans cache encore le corps.
