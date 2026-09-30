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

- BUILD: PENDING
- TESTS: PENDING
- PARITÉ (exécutée contre la référence `fee66ae`, `node tools/migration/gen-parity.mjs needs.mjs nous.mjs nous-inertia.mjs`) :
  `Anastasis.Sim.Parite.Besoins` 542 vecteurs, `Anastasis.Sim.Parite.Nous` 312, `Anastasis.Sim.Parite.Rythme` 649 — 0 écart.
- COMMANDS:
  - `tools\unreal\anastasis-unreal.ps1 build`
  - `tools\unreal\report-tests.ps1 -Filter Anastasis`
  - `UnrealEditor-Cmd ... -ExecCmds="py tools/unreal/granary-eat-pie.py"`

## SCN

PENDING

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
