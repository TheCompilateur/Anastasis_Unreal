# HANDOFF: player-minimal-001

## MISSION

Joueur minimal (mandat d'Alexandre, 2026-10-01) et branchement de la conséquence du temps accéléré :
le joueur est un habitant de la simulation (`FVillage::PlayerPersonId`, canon de la référence), il attend
sans commande et marche à la main ; le village le voit d'autant moins qu'il a accéléré le temps, et ses
jours oisifs font baisser sa réputation. Détail : `docs/unreal/PLAYER_MINIMAL_001.md`.

## FILES_OWNED

- `Source/AnastasisSim/Private/Village/AnastasisVillagePlayer.cpp` (nouveau)
- `Source/AnastasisSim/Private/Tests/AnastasisPlayerTests.cpp` (nouveau)
- `Source/AnastasisSim/Public/Village/AnastasisVillage.h` (partagé : `GoalIdle`, `Standing`, champs `Reputation` /
  `Presence` / `IdleSeconds` de `FNpc`, API joueur, membres privés)
- `Source/AnastasisSim/Private/Village/AnastasisVillage.cpp` (partagé : seam dans `UpdateNpc`, `Bind`, `RemoveNpc`,
  `PickSocialCompanion`, `BondSocialTarget`, `PickRememberedSeekFor`)
- `Source/AnastasisSim/Private/Sim/AnastasisSimulation.cpp` (partagé : `UpdateReputationDaily` à minuit)
- `Source/Anastasis_UnrealV2/Sim/AnastasisSimulationPlayer.cpp` (nouveau)
- `Source/Anastasis_UnrealV2/Sim/AnastasisSimulationSubsystem.h/.cpp` (partagé : entrée, témoin, pawn, overlay, `AdvanceBy` par tranches)
- `tools/unreal/player-pie.ps1`, `tools/unreal/player-pie.py` (nouveaux)
- `AGENTS.md` (index, « Temps accéléré », « État »), `docs/unreal/PLAYER_MINIMAL_001.md`, `docs/unreal/TIME_WARP_001.md`

## COMMIT

PENDING

## MEC

- BUILD: PENDING
- TESTS: PENDING
- COMMANDS:
  - `tools\unreal\anastasis-unreal.ps1 build`
  - `tools\unreal\player-pie.ps1`
  - `tools\unreal\agent-worktree.ps1 finish -Mission player-minimal-001`

## SCN

Pas de capture d'image. La preuve PIE (`player-pie.ps1`) lit l'état par `get_player_status` et la position
du pawn.

## PLY

`tools\unreal\player-pie.ps1` (2026-10-01, `Lvl_AnastasisSlice`) : `PLAYER_PIE PASS checks=16 failed=0`.

| Contrôle | Valeur |
|---|---|
| arrivée + incarnation | `npc-12` (après les 12 habitants du lancement) |
| attente sans commande | but `idle`, activité `attend`, position inchangée après 3 s simulées |
| pawn lié, posé sur le corps | écart 0,0 uu |
| `Move 1 0`, 2 s simulées | +8,067 tuiles (vitesse 4), activité `marche`, pawn suit (0,0 uu) |
| `Advance 7d` | présence 0,0304, 7,0000 jours oisifs, vu par 0 habitant (3 avant), réputation 29,776 |
| `Advance 1d` | réputation 25,866 |
| `Release` | observateur, pawn en `MOVE_WALKING` |

## INTEGRATION_RISK

- `AnastasisVillage.cpp` / `.h` sont chauds. Les changements de perception sont bit-exacts pour tout
  habitant de présence 1 et de réputation 50 (`Anastasis.Sim.Joueur.Observateur`, et les suites de
  déterminisme existantes).
- `FNpc` gagne trois champs hors empreinte (`Digest`) : les vecteurs de parité JS ne bougent pas.
- `AdvanceBy` avance par tranches de 15 s (90 pas de 1/6 s) pour que le témoin soit informé au fil du saut ;
  les bornes de pas peuvent différer d'un saut d'un bloc à l'arrondi flottant près.
- `anastasis.Player.Pawn 1` coupe la marche Unreal du pawn tant qu'un joueur est incarné. Sans
  `Anastasis.Player.Arrive`, rien ne change pour les preuves existantes.

## STOP

- Choix de but par le joueur, parole dirigée, mode visuel `PLAYER` du GameMode : NOT_IMPLEMENTED.
- Réputation des autres habitants (actes, conflits) : non portée ; ils restent à 50.
- Aucune vérification à la main au clavier en PIE : la preuve pilote la marche par `Anastasis.Player.Move`.
