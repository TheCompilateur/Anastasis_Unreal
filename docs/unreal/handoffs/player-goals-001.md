# HANDOFF: player-goals-001

## MISSION

Mandat d'Alexandre (2026-10-01) : le joueur choisit son but. Port de `choosePlayerGoal` / `decideAsPlayer`
(decisionProvider.js) : intention qui dure, décidée dans la table de Noûs sous les mêmes verrous, refus
motivé (`hors-table`, `verrou`, `le-corps-parle`) ; touches 1 à 5 et 0 en PIE ; mérite des bâtiments
achevés dans la réputation. Détail : `docs/unreal/PLAYER_MINIMAL_001.md`, section « La main du joueur ».

## FILES_OWNED

- `Source/AnastasisSim/Public/Village/AnastasisVillage.h` : `Standing::BuildGain`, `PlayerDecision`,
  `FPlayerGoalChoice` / `FPlayerRefusal` / `FPlayerGoalOption`, API `ChoosePlayerGoal` / `GetPlayerGoalChoice` /
  `GetPlayerRefusal` / `GetPlayerGoalOptions`, `ReputationAffinity` devenue membre (joueur seulement).
- `Source/AnastasisSim/Private/Village/AnastasisVillage.cpp` : `ChooseGoal` — sans biais ni porte de Noûs pour
  le joueur, seam `DecideAsPlayer` après les verrous, table du joueur gardée ; `Bind` / `RemoveNpc` oublient la main.
- `Source/AnastasisSim/Private/Village/AnastasisVillagePlayer.cpp` : décision, refus, remède, pensée du joueur,
  `Act` pour son but, réputation (`deeds.built`).
- `Source/AnastasisSim/Private/Tests/AnastasisPlayerTests.cpp` : `Anastasis.Sim.Joueur.Choix.Tient`,
  `.Choix.Refus`, réputation des bâtisseurs.
- `Source/Anastasis_UnrealV2/Sim/AnastasisSimulationPlayer.cpp` : `Anastasis.Player.Goal`, `.Choose`, ligne
  `BUTS` de l'overlay, `get_player_status` (choice, holds, yields, refusal, options, drinks, meals).
- `Config/DefaultInput.ini` : touches 1 à 5 → `Anastasis.Player.Choose n`, 0 → `Anastasis.Player.Goal none`.
- `tools/unreal/player-pie.py` : étapes 3b (refus `hors-table`) et 3c (il va boire).
- `AGENTS.md` (État, index de `player-pie`), `docs/unreal/PLAYER_MINIMAL_001.md`.

## COMMIT

PENDING

## PROOFS

PROOFS: player-pie

## MEC

- BUILD: PENDING (`finish` : build seul ; suite et preuve au lot).
- Tests Unreal : non lancés (organisation « une file, un éditeur ») ; le lot rejoue la suite (dont les
  nouveaux `Anastasis.Sim.Joueur.Choix.*`) et `player-pie`.
- Hors éditeur : `python -m py_compile tools/unreal/player-pie.py` → code 0.
- Touches : aucune entrée du jeu (`IMC_*`) n'utilise 0 à 5 (recherche dans les assets binaires).

## SCN

Sans objet.

## PLY

Au lot : `player-pie` (17 + 6 contrôles). À la main : en PIE, `Anastasis.Player.Arrive`, puis 1 à 5 selon la
ligne BUTS ; 0 retire l'intention.

## INTEGRATION_RISK

- **Empilée sur `time-keys-001`** (`f2c6f2a`) : même bloc de `DefaultInput.ini`. À verser après elle, ou dans
  le même lot.
- Déterminisme : tout le chemin nouveau est derrière `IsPlayer(Npc)` ; sans joueur, `ChooseGoal` est inchangé
  (biais et porte de Noûs appliqués comme avant). La réputation des habitants change désormais avec
  `BuildingsCompleted`, mais elle n'entre ni dans l'empreinte ni dans leurs décisions (`ReputationAffinity`
  ne joue que pour le joueur).
- Écart assumé avec la référence (EXTENSION) : le remède passe quand le corps parle.

## STOP

- Parole dirigée (`T`) : non portée. Mode visuel `PLAYER` : non commencé.
- Pas de touche dédiée par but : 1 à 5 suivent l'ordre de la table du moment, qui change avec les besoins
  (comme la référence). La ligne BUTS dit ce que chaque touche fait maintenant.

## ECARTS

- ouvert : n° 20 — Le joueur est un habitant : incarnation et main portées, sans flux joueur ni parole dirigée (A_TRANCHER)
- ouvert : n° 21 — Le remède passe quand le corps parle (EXTENSION, A_TRANCHER : à trancher par Alexandre)
- ouvert : n° 22 — Présence, oisiveté et réputation du joueur (EXTENSION, A_TRANCHER ; entré avec player-minimal-001 sans fiche, numéroté ici)
- ouvert : n° 23 — Réputation : seul le mérite des bâtiments est porté (REDUIT, A_TRANCHER)
- cités, inchangés : n° 1 (but non porté dans la table du joueur), n° 2 (collant non porté, ligne retouchée)
