# HANDOFF: sim-time-warp-001

## MISSION

Accélérer le temps pour le joueur et pour les agents (TIME_WARP_001) : `anastasis.Sim.Warp` (temps réel
accéléré, pause, paliers au clavier en PIE), `Anastasis.Sim.Advance` (saut instantané pour les preuves),
et le témoin de la conséquence demandée par Alexandre : la présence du joueur s'efface quand il accélère,
ses jours oisifs s'accumulent. Détail : `docs/unreal/TIME_WARP_001.md`.

## FILES_OWNED

- `Source/Anastasis_UnrealV2/Sim/AnastasisTimeWarp.h`
- `Source/Anastasis_UnrealV2/Sim/AnastasisTimeWarp.cpp`
- `Source/Anastasis_UnrealV2/Sim/AnastasisTimeWarpTests.cpp`
- `Source/Anastasis_UnrealV2/Sim/AnastasisSimulationSubsystem.h` (partagé : membres warp, `AdvanceBy`, `GetTimeWarpStatus`)
- `Source/Anastasis_UnrealV2/Sim/AnastasisSimulationSubsystem.cpp` (partagé : `Tick`, `DrawOverlay`, commandes)
- `Config/DefaultInput.ini` (section `[/Script/Engine.PlayerInput]` ajoutée)
- `AGENTS.md` (section « Temps accéléré »)
- `docs/unreal/TIME_WARP_001.md`

## COMMIT

PENDING

## MEC

- BUILD: PENDING
- TESTS: PENDING
- COMMANDS:
  - `tools\unreal\anastasis-unreal.ps1 build`
  - `tools\unreal\agent-worktree.ps1 finish -Mission sim-time-warp-001`

## SCN

Non capturé. Aucune preuve PIE de l'overlay ni des touches du pavé numérique dans cette mission.

## PLY

`PLAYER` NOT_IMPLEMENTED, inchangé. Les touches passent par les liaisons de debug du moteur
(`DebugExecBindings`), absentes d'un build Shipping.

## INTEGRATION_RISK

- `AnastasisSimulationSubsystem.cpp` est chaud (chaque mission village y ajoute une commande) : conflits
  textuels probables, sans recouvrement logique. `Tick` : la branche `Warp == 1` est l'ancien code à l'identique.
- `Witness` n'est lu par aucun habitant : sans effet sur la simulation.
- Le gardien de parité JS (`AnastasisSim`) n'est pas touché.

## STOP

- Aucun habitant ne réagit à la présence ni à l'oisiveté : il n'y a ni joueur ni réputation portés.
- Les scripts `*-pie.py` existants ne sont pas convertis à `Advance`.
- Pas de preuve visuelle de l'overlay.
