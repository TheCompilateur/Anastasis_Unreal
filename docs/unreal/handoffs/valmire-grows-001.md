# HANDOFF: valmire-grows-001

## MISSION

Jalon « Valmire pousse toute seule », chantier « Valmire continue de pousser » (« Oui » d'Alexandre, 2026-10-08).
Décision d'Alexandre en cours de mission : **les maisons restent l'affaire des familles** (écart n°48,
relay-memoire-001), **le village décide des bâtiments communs**. Chaque soir, s'il manque un grenier (aucun,
ou tous pleins) ou un puits (plus de 15 âmes par puits), le village en ouvre le chantier, un habitant en trace
l'emplacement, et la chronique dit pourquoi :
« Le village décide de bâtir le deuxième puits : 16 âmes pour un seul puits. Basile en trace l'emplacement. »

Au passage : le porteur de matériaux ne s'arrêtait plus jamais dès qu'un besoin secondaire (solitude, ennui,
hygiène, moral) devenait critique. Après quelques jours, les chantiers restaient à 0 %. Il ne s'arrête plus
que sur la faim, la soif, la fatigue ou la santé, comme son commentaire le disait.

## FILES_OWNED

- `Source/AnastasisSim/Private/Village/AnastasisVillageGrowth.cpp` (nouveau) : `SetGrowthEnabled`, `GrowthOpenSlots`,
  `FindBuildSpot`, `PickCommonBuilding`, `UpdateCommonBuildingsDaily`
- `Source/AnastasisSim/Public/Village/AnastasisVillage.h` : déclarations, `FBuilding::OpenedById` / `OpenCause`,
  `GetMaterialCourierId`, champs `bGrowthEnabled`, `GrowthSitesOpened`, `LastBuildDecision`
- `Source/AnastasisSim/Private/Village/AnastasisVillage.cpp` : `SurvivalCritical` et le portage gardé par elle ;
  remise à zéro dans `Bind`
- `Source/AnastasisSim/Private/Village/AnastasisVillageStateDigest.cpp` : les nouveaux champs dans le parcours
- `Source/AnastasisSim/Public/Sim/AnastasisSimulation.h`, `Private/Sim/AnastasisSimulation.cpp` : travail de minuit 1
  `collective` (`DayJobCollective`), `SaveFormatVersion` 5
- `Source/AnastasisSim/Private/Tests/AnastasisVillageGrowthTests.cpp` (nouveau) : `Anastasis.Sim.Village.Croissance.*`
- `Source/AnastasisSim/ECARTS.md` : fiche n° 50
- `Source/Anastasis_UnrealV2/Sim/AnastasisSimulationSubsystem.cpp` : CVar `anastasis.Village.Growth` ; allumée dans
  `SeedStartVillage` seulement, éteinte dans `ResetCanonical` (les scénarios de preuve restent tels quels)
- `Source/Anastasis_UnrealV2/Sim/AnastasisVillageChronicle.cpp` : « Le village décide de bâtir … », la raison en clair
- `Source/Anastasis_UnrealV2/Sim/AnastasisVillageGrowthHostTests.cpp` (nouveau) : `Anastasis.Village.Croissance.Valmire`

## COMMIT

Voir `git log main..agent/valmire-grows-001`, base `main` = `e82fc307` (après `relay-memoire-001`).

## MEC

- BUILD: PASS (worktree)
- TESTS: `tools\unreal\report-tests.ps1 -Filter 'Anastasis.Sim.Village.Croissance+Anastasis.Village.Croissance+Anastasis.Memoire+Anastasis.Familles+Anastasis.Chronique+Anastasis.Sim.Sauvegarde'`
  → PASS 18 / KNOWN_EXPECTED_FAILURE 0 / FAIL 0, 18 annoncés, run complet :
  - `Anastasis.Sim.Village.Croissance.BatimentsCommuns` : sans grenier, un grenier (`aucun grenier`), puis plus
    rien le soir suivant (créneau pris) ; 16 âmes et un puits, un puits (`16 ames pour 1 puits`) ; grenier plein, un
    second grenier (`le grenier deborde`) ; rien ne manque, rien ne s'ouvre ;
  - `…Croissance.Deterministe` : deux parties identiques, même `StateDigest` à trois jours, le grenier décidé dans les deux ;
  - `…Croissance.Eteinte` : croissance éteinte, rien, même après trois minuits ;
  - `Anastasis.Village.Croissance.Valmire` : le vrai village du lancement, 30 jours, contre le témoin éteint.
    Mêmes chiffres des deux côtés : 14 habitants, 3 maisons, 0 mort, aucun bâtiment commun décidé (14 âmes
    pour un puits, le grenier ne déborde pas). La décision ne prend rien aux familles. Chroniques :
    `Saved/Chronicle/croissance-30-jours.txt` et `croissance-temoin-30-jours.txt`.
- Essais retirés, consignés pour mémoire (même jour, même graine) :
  - portage de l'ouverture par l'IA d'un habitant, avec la colonie entière : 3 morts de soif, grenier vide
    14 jours ;
  - colonie réduite au besoin de bâtir : 5 maisons en 30 jours **avant** les maisons de famille, mais 2 au lieu
    de 3 **avec** elles (concurrence).
- `check-ecarts` : `ECARTS::PASS fiches=46` ; `check-state-fields` : `STATE_FIELDS::PASS structures=46 lacunes=2`

PROOFS_RUN: au lot (EDITOR_QUEUE_001) ; `finish` joue build + suite sans rendu.

## PROOFS

PROOFS: chronicle-pie, villager-pie, npc-life-pie, material-courier-pie, save-load-pie, memory-pie

## SCN

Dans le jeu : rien ne change tant que rien ne manque. Quand le village grossit (arrivants) ou que la récolte
déborde, il décide d'un puits ou d'un grenier, un habitant le trace, les bâtisseurs le lèvent, et la chronique
le raconte. Le porteur reprend son travail après quelques jours au lieu de s'arrêter pour toujours.

## PLY

NOT_JUDGED — Alexandre lit la chronique.

## ECARTS

- n° 50 — nouvelle, OUVERT, A_TRANCHER, EXTENSION : le village décide de ses bâtiments communs (grenier, puits),
  par des règles propres. Marques `ecart n°50` dans `AnastasisVillageGrowth.cpp`, `AnastasisVillage.{h,cpp}`,
  `AnastasisVillageStateDigest.cpp`, `AnastasisSimulation.{h,cpp}`.
- modifié : n° 18 — le porteur de matériaux (extension opt-in) ne s'arrête plus que sur les besoins vitaux.

## INTEGRATION_RISK

- **`SaveFormatVersion`** 5 (`main` = 4). `arrivants-001` et `opening-in-sim-001` le montent aussi à 5 : la mission
  versée en second prend 6 (rebase mécanique, je le fais pour les miennes).
- **Numéros d'écart** : n°49 pris par `arrivants-001`, n°50 ici, n°51 par `water-network-001`.
- **Le porteur** (`SurvivalCritical`) change le comportement de `npc-life-pie` et de `material-courier-pie`
  (déclarées).
- **Avec `arrivants-001`** : c'est elle qui fera grossir le village. Au-delà de 15 âmes, le village décidera
  d'un puits. À vérifier dans le lot qui les réunit.

## STOP

- N'ouvre pas de maisons : elles restent aux familles (écart n°48).
- Ne fait pas venir d'arrivants (arrivants-001).
- Ne porte pas le classement du planificateur ni l'ouverture par l'IA d'un habitant (essayés, retirés).
- Suite complète : jouée par `finish`.
