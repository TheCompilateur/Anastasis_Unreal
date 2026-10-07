# HANDOFF: state-oracle-001

## MISSION

Chantier C1 d'IRON_CRUSADE_001, demandé par Alexandre le 2026-10-07 (« Fait 1 ») : un oracle
d'égalité d'état séparé de la projection de parité JS, les assertions de déterminisme et de
non-écriture branchées dessus, et un garde-fou contre les champs d'état non lus.

Constat de départ (IRON_CRUSADE_001, `Anastasis.Iron.Empreinte.AveugleAuxEcritures`) :
- `FVillage::Digest()` lit 22 champs de `FNpc` sur ~110 et 5 membres de `FVillage` sur 46.
- Une écriture invisible dans `Speed` ou dans `sim.rng` fait diverger le futur en 7,8 s et 9,0 s
  simulées.

## FILES_OWNED

- `Source/AnastasisSim/Private/Village/AnastasisVillageStateDigest.cpp` (nouveau) : `FVillage::StateDigest()`
  et un `HashState` par type d'état
- `Source/AnastasisSim/Public/Village/AnastasisVillage.h` : déclaration ; commentaire de `Digest()` précisé
  (projection de parité, pas un oracle)
- `Source/AnastasisSim/Public/Sim/AnastasisSimulation.h`, `Private/Sim/AnastasisSimulation.cpp` :
  `FAnastasisSimulation::StateDigest()` (horloge, file de minuit, monde entier, village)
- `Source/AnastasisSim/Private/Tests/AnastasisStateDigestTests.cpp` (nouveau) :
  `Anastasis.Sim.Empreinte.Etat.Deterministe`, `Anastasis.Sim.Empreinte.Etat.VoitLesEcritures`
- Cinq tests, assertion d'état complet AJOUTÉE à côté de l'assertion `Digest()` existante (rien de retiré) :
  - `AnastasisVillageSimTests.cpp` (`Puits.MultiAgents`)
  - `AnastasisPlayerTests.cpp` (`Joueur.Observateur`)
  - `AnastasisMortalityTests.cpp` (`Mortality.Daily`)
  - `AnastasisVillageEnduranceTests.cpp` (`Repousse.Hote`)
  - `Anastasis_UnrealV2/Village/AnastasisVillagerTests.cpp` (`Villagers.Presentation`)
- `tools/migration/check-state-fields.mjs` (nouveau), `tools/migration/state-fields.json` (nouveau, 37 structures)
- `tools/unreal/agent-worktree.ps1` : `finish` appelle `check-state-fields.mjs` après `check-ecarts.mjs`
- `tools/unreal/test-agent-worktree.ps1` : copie du contrôleur dans le dépôt jetable (il y sort `SKIP`)
- `AGENTS.md` : section « Deux empreintes : parité ≠ égalité d'état »

## COMMIT

PENDING — rien n'est commité (consigne d'Alexandre : pas de commit sans son accord).

## MEC

- BUILD: PASS (`tools\unreal\anastasis-unreal.ps1 build`, worktree, 119,8 s)
- TESTS: PASS 7 / KNOWN_EXPECTED_FAILURE 0 / FAIL 0, run complet. La suite complète n'a PAS été jouée :
  elle attend le lot.
- COMMANDS:
  - `node tools/migration/check-state-fields.mjs -liste`, puis `-base main` → `STATE_FIELDS::PASS structures=37 lacunes=2`
  - Mode `-base main`, testé sur un dépôt git jetable :
    - un champ non classé déjà sur `main` sort en WARN, et le contrôle passe ;
    - un champ ajouté par la branche sort en FAIL.
  - Garde-fou testé par mutation sur une copie. Chacune des cinq mutations suivantes donne
    `STATE_FIELDS::FAIL` avec la ligne attendue ; sans registre, le contrôleur sort
    `STATE_FIELDS::SKIP` (code 0) :
    - champ ajouté à `FNpc` ;
    - lecture de `Speed` retirée ;
    - entrée périmée ;
    - champ classé et lu à la fois ;
    - raison sans préfixe.
  - `tools\unreal\test-agent-worktree.ps1` → PASS 46, FAIL 0, rejoué après le dernier changement de `agent-worktree.ps1`
  - `node tools/migration/check-ecarts.mjs -base main -handoff docs/unreal/handoffs/state-oracle-001.md` → `ECARTS::PASS`
    (les 6 WARN sont antérieurs à la mission)
  - `tools\unreal\report-tests.ps1 -Filter 'Anastasis.Sim.Empreinte.Etat+Anastasis.Sim.Village.Puits.MultiAgents+Anastasis.Sim.Joueur.Observateur+Anastasis.Sim.Mortality.Daily+Anastasis.Sim.Village.Repousse.Hote+Anastasis.Village.Villagers.Presentation'`
    → 7 tests, tous `Success` (10 min d'attente à la porte mémoire) :
    - `Anastasis.Sim.Empreinte.Etat.Deterministe` : même état complet après 2 jours simulés, et cet état a changé.
    - `Anastasis.Sim.Empreinte.Etat.VoitLesEcritures` : chacune des 8 écritures change `StateDigest` à l'instant
      même et laisse `Digest()` égal. Les 8 écritures : `Speed`, `AiThinkAt`, `Reputation`, `Relations`,
      `KnownCells`, `Path`, `sim.rng`, météo forcée.
    - `Puits.MultiAgents`, `Joueur.Observateur`, `Mortality.Daily`, `Repousse.Hote` et `Villagers.Presentation`,
      chacun avec sa nouvelle assertion d'état complet.

## PROOFS

PROOFS: (aucune)

## SCN

N/A — aucun changement de comportement du jeu : `StateDigest()` n'est appelé que par des tests.

## PLY

N/A

## ECARTS

AUCUN — `StateDigest()` est un lecteur pur de l'état (aucune écriture, aucun tirage), sans équivalent
de jeu dans la référence JS. `Digest()`, la projection de parité, n'a pas changé d'un octet : le test
`VoitLesEcritures` vérifie qu'elle reste aveugle aux huit écritures, comme avant.

## INTEGRATION_RISK

- Une mission qui ajoute un champ à `FNpc`, `FBuilding`, `FVillage` ou à un type du registre échouera
  désormais à `finish` (`STATE_FIELDS::FAIL`) tant qu'elle ne le lit pas dans `HashState` ou ne le classe
  pas. C'est voulu.
- 13 branches non versées modifient ces en-têtes (`git diff` depuis leur base de fusion, relevé du
  2026-10-07). Celles qui ajoutent un champ devront le classer à leur prochain `finish` :
  - `abandon-001`, `anthropic-wood-001`, `build-decision-001`, `canopy-rain-shelter-001` ;
  - `geopolitical-world-001`, `lifestyle-decision-001`, `nav-wiring-001`, `npc-life-bridge-001` ;
  - `premiere-pensee-001`, `relay-lot6-001`, `settlement-morphogenesis-001`, `soil-water-budget-001`,
    `spatial-risk-test-001`.
- `finish` appelle le contrôle avec `-base main` : seul ce que la branche ajoute la bloque. Un champ
  versé par le lot sans classement (une branche finie avant ce versement) sort en WARN chez les missions
  suivantes, sans les bloquer. Le lot lui-même ne rejoue pas le contrôle.
- Deux lacunes déclarées : `FVillage::Colony` et `MarketStock`. L'état du planificateur n'est posé que
  par `RestoreColonyForHarness` (harnais), jamais pour un village créé par le C++. Il faut les lire avant
  toute sauvegarde (chantier C4).
- Coût : `StateDigest()` trie les clés de ~110 champs par habitant. Il n'est appelé qu'en test, jamais par
  tick de jeu.

## STOP

- Ne revendique pas la sauvegarde (C4), ni le rapatriement des décisions de l'hôte (C3).
- Ne prouve pas que l'état est **complet** au sens sémantique. Il prouve seulement que chaque champ
  **déclaré** des 37 structures est lu ou classé.
  - Un état caché ailleurs (une variable statique, l'hôte Unreal) n'est pas couvert. L'état de l'hôte
    (`OpeningSiteId`, etc.) relève de C3.
  - `FAnastasisRng` n'est lu que par `GetState()`. Un champ ajouté à ce type ne serait pas vu.
- Le test d'expérience `Anastasis.Iron.*` reste dans le worktree `iron-crusade-001`. Il n'est pas versé ici.
