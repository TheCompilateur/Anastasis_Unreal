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

`ab73b47` (code), puis cette fiche.

## MEC

- BUILD: `BUILD::PASS` (adaptatif), puis `Build.bat ... -DisableAdaptiveUnity` : `Module.Anastasis_UnrealV2.1.cpp`
  et `.3.cpp` recompilés avec les nouveaux fichiers, `Result: Succeeded`. Le premier build avait échoué dans
  `AnastasisUnderstoryTests.cpp` (`FInputs` / `FPlan` / `EKind` ambigus) : corrigé dans ce commit.
- TESTS (`finish`, 2026-10-01 19:15) : PASS 231, KNOWN_EXPECTED_FAILURE 4 (les quatre du registre), FAIL 0,
  TOTAL 235 = annoncés 235. `HANDOFF_READY::YES`.
  - `Anastasis.Sim.TimeWarp.Presets` / `.ParseAdvance` / `.Advance` / `.Pump` / `.Witness` : Success
  - `Anastasis.Sim.Tick.HostPumps` (chemin `Warp 1`) : Success
  - `Anastasis.Understory.EdgesRiversAndSpecies` / `.SlopeAltitudeAndReserves` : Success
- Valeurs vérifiées par les tests : un jour = 540 pas de 1/6 s, temps exact à 1e-9 ; ×1000 pendant 1 s = 1000 s
  simulées, ~100 pas/frame ; une semaine sautée → présence e^-3,5 ≈ 3 %, 7 jours oisifs ; un jour joué ensuite → 64 %.
- COMMANDS:
  - `tools\unreal\anastasis-unreal.ps1 build`
  - `Build.bat Anastasis_UnrealV2Editor Win64 Development -Project=... -DisableAdaptiveUnity`
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
