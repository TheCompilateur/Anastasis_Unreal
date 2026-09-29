# HANDOFF: first-building-001

## MISSION

Brancher le premier bâtiment fonctionnel au simulateur : un habitant choisit un puits, y
marche, y boit, et `npc.thirst` — relu par la décision au tick suivant — baisse d'une
quantité prouvée au bit près. Détail : `docs/unreal/FIRST_BUILDING_001.md`.

## FILES_OWNED

Créés :

- `Source/AnastasisSim/Public/Life/AnastasisNeeds.h`, `Private/Life/AnastasisNeeds.cpp`
- `Source/AnastasisSim/Public/Village/AnastasisVillage.h`, `Private/Village/AnastasisVillage.cpp`
- `Source/AnastasisSim/Private/Tests/AnastasisNeedsTests.cpp`, `AnastasisNeedsVectors.inl` (généré), `AnastasisVillageSimTests.cpp`
- `Source/Anastasis_UnrealV2/Village/AnastasisVillagePresentation.h/.cpp`, `AnastasisFirstBuildingTests.cpp`
- `tools/migration/parity/needs.mjs`, `tools/unreal/first-building-pie.py`
- `docs/unreal/FIRST_BUILDING_001.md`, cette fiche

Modifiés :

- `Source/AnastasisSim/{Public,Private}/Sim/AnastasisSimulation.*` — l'hôte possède le village
- `Source/Anastasis_UnrealV2/Sim/AnastasisSimulationSubsystem.*` — sync présentation, debug, commandes
- `Source/AnastasisSim/PORTAGE.md` — la tranche et ses écarts
- `AGENTS.md` — une ligne d'index pour `first-building-pie.py`

Repris par cherry-pick (auteur Codex, `agent/sim-tick-day@b0f487f`, aujourd'hui `4ab8ede`) :
`Sim/AnastasisSimulation.*`, `Sim/AnastasisSimulationSubsystem.*`, `AnastasisSimulationTests.cpp`,
`AnastasisSimulationHostTests.cpp`, `tools/unreal/smoke-pie.py`.

## COMMIT

BRANCH_HEAD

## MEC

- BUILD: PASS — `tools\unreal\anastasis-unreal.ps1 build` sur la base `main@19e0709`,
  `Module.Anastasis_UnrealV2.cpp` compilé en unity avec les nouveaux fichiers (AnastasisSim
  compile fichier par fichier). Zéro warning dans les fichiers de la mission.
- TESTS: PASS — `tools\unreal\report-tests.ps1 -Filter Anastasis` sur `a6e20b7` (base `main@19e0709`),
  run complet (106 annoncés, 106 exécutés) :
  ```
  PASS                  : 102
  KNOWN_EXPECTED_FAILURE: 4   (Sim.Parite.Fbm, Sim.Parite.SemantiqueJs, 2 x AI.Toolsets.AnastasisInspect — le registre)
  FAIL                  : 0
  ```
  Dont les 17 de la mission (9 nouveaux + 8 `Sim.Tick.*` repris de sim-tick-day), tous PASS :
  `Sim.Parite.Besoins` (165 vecteurs, 0 écart) ; `Sim.Village.Puits.Enregistrement`,
  `.Selection`, `.AtteinteEtEffet` (soif pic 62,04 -> 0 ; tick de l'acte égal au bit près à
  `tickNeeds` + `satisfyDrink`), `.Destruction`, `.Inaccessible`, `.MultiAgents` (6 habitants
  répartis sur 4 seuils, un retiré en plein puisage, empreinte déterministe), `.Hote` (monde
  canonique 12345, puits en (46,46)) ; `Village.FirstBuilding.Presentation`.
  Premier run (avant correction) : 1 FAIL, `AtteinteEtEffet` — l'assertion de vraisemblance
  comparait au mauvais instant (le contrôle au bit près passait) ; test corrigé, pas le code.
- PARITÉ: `Anastasis.Sim.Parite.Besoins` — 165 vecteurs bit à bit contre `src/life/needs.js`
  @ `fee66ae` exécuté (`node tools/migration/gen-parity.mjs needs.mjs`), 0 écart.
- COMMANDS:
  - `tools\unreal\anastasis-unreal.ps1 build`
  - `tools\unreal\report-tests.ps1 -Filter Anastasis`
  - `node tools/migration/gen-parity.mjs needs.mjs` (régénère les vecteurs, ne jamais les éditer)

## SCN

PASS (log runtime, pas de capture) — `tools/unreal/first-building-pie.py` en PIE sur
`Lvl_AnastasisSlice`, `a6e20b7`, exit 0, `FIRST_BUILDING_COMPLETE` :

- `Anastasis.Village.FirstWell 4` -> `building-0` tuile (47,47), acteur
  `AnastasisVillageBuilding_0` à (19000, 19000, 400) — 47,5 × `TileWorldSize` 400.
- les habitants partent un par un, dans l'ordre de leur soif (58, 45, 32, 19) ; à t=83 s de
  simulation, trois ont bu (`drinks=1`, soif 15,4 / 13,8 / 9,4), le quatrième suit.
- `RemoveNpc npc-1` : `actors=3`, le puits et les autres continuent.
- `RemoveBuilding building-0` : « actor destroyed », `buildings=0`, `navVersion` 1 -> 2,
  aucun `dest=building-0` restant.
- Seules erreurs du log : 20 × `Condition failed` (bruit de démarrage connu) et le port MCP
  8000 déjà pris par un autre éditeur.

## PLY

UNKNOWN — aucun contrôle humain. Les habitants sont des tracés de debug, pas des pawns ;
`PLAYER` reste NOT_IMPLEMENTED.

## INTEGRATION_RISK

- **Intégrer cette branche intègre aussi `agent/sim-tick-day@b0f487f`** (l'hôte `tick(dt)`).
  Le second commit de sim-tick-day (`2a6460a`, canopée + `DA_AnastasisPresentation.uasset`)
  n'est PAS repris. Si sim-tick-day est intégrée plus tard, son `b0f487f` est déjà dans `main`
  sous `4ab8ede` : git le reconnaîtra au rebase (même patch), sinon l'écarter à la main.
- `FAnastasisSimulation` n'est plus copiable (le village pointe sur son monde) : tout code
  futur qui le copierait ne compilera pas — voulu.
- Deux fondations Village concurrentes : `main/Village` (utilisée ici) et
  `claude/anastasis-village-foundation-28ac68` (non intégrée, tags `Anastasis.Activity.*`).
  L'intégrer créerait une seconde façon de poser un bâtiment — décision d'intégrateur.
- Le slot Smart Object du puits (`main`, offset `(80,0,0)` uu) ne suit ni `TileWorldSize`
  (désormais 400) ni les seuils de la simulation. Rien ne le lit dans cette boucle.
- `anastasis.Sim.Speed` = 10 par défaut (hérité) : le village vit 10× plus vite en PIE.
- Le PIE n'a rien de village tant que personne ne tape `Anastasis.Village.FirstWell` :
  aucun changement visible pour les autres missions, aucun `.umap` touché
  (`Anastasis.Level.HoldsNoWorldTruth` reste vrai : le puits vient de la simulation).

## STOP

- Pas de parité de **trajectoire** : la table de décision est réduite à `drink` contre un
  plancher déclaré (`UnportedGoalsFloor = 42`). Le harnais différentiel ne peut pas juger
  cette boucle ; seules les fonctions de besoins sont en parité bit à bit.
- Pas de maison, pas de stock, pas de réservation : la référence n'en a pas pour le puits.
- Pas de sauvegarde : Unreal n'a pas encore de format (`P2_MODELE_DONNEES.md`). `Digest()`
  donne la projection canonique ; contraintes pour le futur format dans FIRST_BUILDING_001.md §6.
- Pas de preuve visuelle (capture) : la preuve de scène est le log PIE.
- `RemoveBuilding` est une extension sans équivalent JS : la référence ne démolit jamais.
