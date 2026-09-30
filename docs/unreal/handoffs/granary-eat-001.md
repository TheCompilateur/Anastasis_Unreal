# HANDOFF: granary-eat-001

## MISSION

Brancher le grenier au simulateur **par Noûs** (le chemin actif par défaut de la référence) : un
habitant affamé réserve une portion, marche au grenier, y entre, et à la fin du repas
`building.stock.food.physical` baisse de 1 pendant que sa faim tombe. Détail :
`docs/unreal/GRANARY_EAT_001.md`.

## FILES_OWNED

Créés :

- `Source/AnastasisSim/Public/Ai/AnastasisNous.h`, `Private/Ai/AnastasisNous.cpp`
- `Source/AnastasisSim/Private/Tests/AnastasisNousTests.cpp`, `AnastasisNousVectors.inl`, `AnastasisNousInertiaVectors.inl` (générés), `AnastasisVillageGranaryTests.cpp`
- `tools/migration/parity/nous.mjs`, `tools/migration/parity/nous-inertia.mjs`, `tools/unreal/granary-eat-pie.py`
- `docs/unreal/GRANARY_EAT_001.md`, cette fiche

Modifiés :

- `Source/AnastasisSim/{Public,Private}/Life/AnastasisNeeds.*`, `{Public,Private}/Village/AnastasisVillage.*`
- `Source/AnastasisSim/Private/Tests/AnastasisNeedsTests.cpp`, `AnastasisNeedsVectors.inl`, `AnastasisVillageSimTests.cpp`, `AnastasisVillageHouseTests.cpp`
- `Source/Anastasis_UnrealV2/Sim/AnastasisSimulationSubsystem.*`
- `Source/Anastasis_UnrealV2/Village/AnastasisVillageBuilding.h`, `AnastasisVillageInteractionSubsystem.*`, `AnastasisVillageTags.*`, `AnastasisVillagePresentation.cpp`, `AnastasisFirstBuildingTests.cpp`
- `tools/migration/parity/needs.mjs`, `Source/AnastasisSim/PORTAGE.md`, `AGENTS.md`

## COMMIT

BRANCH_HEAD

## MEC

- BUILD: PASS — `tools\unreal\anastasis-unreal.ps1 build` sur `eaa93a8` (base `main@17d5e5a`), `Module.Anastasis_UnrealV2.{1,2,3}.cpp` compilés en unity.
- TESTS: PASS — `tools\unreal\report-tests.ps1 -Filter Anastasis`, run complet (135 annoncés, 135 exécutés) :
  ```
  PASS                  : 131
  KNOWN_EXPECTED_FAILURE: 4   (le registre : Sim.Parite.Fbm, Sim.Parite.SemantiqueJs, 2 x AI.Toolsets.AnastasisInspect)
  FAIL                  : 0
  ```
  Nouveaux, tous PASS : `Sim.Parite.Nous` ; `Sim.Village.Grenier.Enregistrement`, `.Perception`,
  `.Repas` (réservation meal_1, stock 12 -> 11 au premier repas, tick dedans et tick du repas
  égaux au bit près à `tickNeedsEat` + `satisfyEat`, faim 65,6 -> 29,9 ; puis 5 repas d'affilée
  par inertie Noûs, stock = 12 - repas), `.MultiAgents` (3 affamés, 2 portions : 2 repas, le
  troisième `unavailable` ; retrait d'un réservant : la portion revient), `.Destruction` (démoli
  en route et démoli pendant le repas : aucune référence, aucune réservation morte),
  `.FoyerDabord` (faim 70 -> 10,75, grenier 5 -> 5 : comportement de la référence), `.Hote`
  (monde canonique, grenier 6 -> 5) ; `Village.FirstBuilding.GrenierPresentation`.
  Les tests du puits et de la maison restent PASS, adaptés à la cadence Noûs (2,2 s).
  En cours de route : 2 FAIL dus à mes attentes (seuil de satiété ; un seul repas), corrigées
  après lecture de la référence — pas le code.
- PARITÉ (exécutée contre la référence `fee66ae`, `node tools/migration/gen-parity.mjs needs.mjs nous.mjs nous-inertia.mjs`) :
  `Anastasis.Sim.Parite.Besoins` 542 vecteurs, `Anastasis.Sim.Parite.Nous` 312, `Anastasis.Sim.Parite.Rythme` 649 — 0 écart.
- COMMANDS:
  - `tools\unreal\anastasis-unreal.ps1 build`
  - `tools\unreal\report-tests.ps1 -Filter Anastasis`
  - `UnrealEditor-Cmd ... -ExecCmds="py tools/unreal/granary-eat-pie.py"`

## SCN

PASS (log runtime, pas de capture) — `tools/unreal/granary-eat-pie.py` en PIE sur
`Lvl_AnastasisSlice`, `eaa93a8`, exit 0, `GRANARY_EAT_COMPLETE`, piloté par l'état de la simulation :

- `FirstGranary 4 6` -> `building-0` (granary) tuile (47,47), acteur à (95000, 95000) ; 4 affamés
  sans toit, qui le voient.
- premier repas confirmé à t=46,1 : stock 6 -> 5, 3 réservations en cours, `npc-3` dedans.
- t=48,5 : 3 repas, stock 3, 3 réservations.
- `RemoveBuilding building-0` avec 3 réservations et un habitant dedans : « actor destroyed »,
  `reservations=0`, `navVersion` 1 -> 2 ; 10 s simulées plus tard, rien ne casse.
- Seules erreurs du log : 20 x `Condition failed` (bruit de démarrage connu).

## PLY

UNKNOWN — aucun contrôle humain ; les habitants sont des tracés de debug. `PLAYER` reste NOT_IMPLEMENTED.

## INTEGRATION_RISK

- **La cadence de pensée change pour TOUS les buts** : 2,2 s (Noûs) au lieu de 0,12 s. Puits et
  maison réagissent plus lentement, comme dans la référence par défaut. Les tests du puits et de
  la maison ont été adaptés (fenêtres d'attente), leurs assertions de fond sont inchangées.
- **Extension de la fondation Village de `main`** : `EAnastasisVillageBuildingKind::Granary`, tag
  natif `Building.Granary`, `GranaryDef`. Collision possible avec toute mission qui touche cet enum.
- Trois comportements de la référence reproduits et testés, à signaler côté JS (voir
  GRANARY_EAT_001 : foyer d'abord, inertie périmée, capacité non contrôlée à l'entrée).
- Aucun `.uasset` ni `.umap` touché ; rien ne se passe en PIE sans `Anastasis.Village.FirstGranary`.

## STOP

- Pas de chemin classique (`acquireMealStock`, rations, achat) : Noûs seulement, comme la référence par défaut.
- Pas de production ni de portage : le grenier se remplit par `CreditFood`.
- Pas d'or, pas de marché, pas de danger, pas d'oubli des croyances, pas de mendicité.
- Pas de parité de trajectoire : la table reste réduite (buts non portés = 42 + rythme).
