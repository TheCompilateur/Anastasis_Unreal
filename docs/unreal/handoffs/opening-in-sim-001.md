# HANDOFF: opening-in-sim-001

## MISSION

Chantier 3 d'IRON_CRUSADE_001, suite (accordé par Alexandre) : les décisions d'ouverture du village, prises
jusqu'ici par l'hôte Unreal (`UAnastasisSimulationSubsystem::SeedOpeningHousehold`, `SeedOpeningConstruction`,
`SeedOpeningWorkforce`, `AssignCompletedOpeningHome` et son verrou `OpeningSiteId`), entrent dans la simulation
portable `AnastasisSim`, à comportement identique pour la même graine et le même site. L'hôte ne fait plus que
les appeler et les dire au log. Plus : `ResetCanonical` remet enfin `FarmerGranaryId` / `FarmerField`.

## FILES_OWNED

- `Source/AnastasisSim/Private/Village/AnastasisVillageOpening.cpp` (nouveau) : `FVillage::SeedOpeningVillage`,
  `FVillage::AssignCompletedOpeningHome`, `OpeningHomeStatusName`
- `Source/AnastasisSim/Public/Village/AnastasisVillage.h` : `FOpeningReport`, `EOpeningHomeStatus`,
  `FOpeningHomeOutcome` ; champs `FVillage::OpeningSiteId`, `FVillage::OpeningHome`
- `Source/AnastasisSim/Private/Village/AnastasisVillage.cpp` : `Bind` vide le verrou ; fin d'`UpdateActors`
  appelle la règle
- `Source/AnastasisSim/Private/Village/AnastasisVillageStateDigest.cpp` : les deux champs hachés
- `Source/AnastasisSim/Private/Tests/AnastasisVillageOpeningTests.cpp` (nouveau) :
  `Anastasis.Sim.Village.Ouverture.DecideeParLaSimulation`
- `Source/AnastasisSim/ECARTS.md` : fiche n° 40
- `Source/Anastasis_UnrealV2/Sim/AnastasisSimulationSubsystem.{h,cpp}` : les quatre fonctions d'hôte retirées,
  `LogOpeningReport` / `LogOpeningHome` (mêmes lignes de log), remise de `FarmerGranaryId` / `FarmerField`
- Relais (commits repris, voir INTEGRATION_RISK) : `site-from-sim-001` (et `geo-measure-001`, déjà sur `main`)

## COMMIT

Voir `git log main..agent/opening-in-sim-001` : les deux commits de `site-from-sim-001` rebasés sur `main`, puis
le commit de cette mission.

## MEC

API nouvelle de la simulation :

- `FOpeningReport FVillage::SeedOpeningVillage(int32 Day, bool bOpenConstruction)` : foyer + grenier du premier
  colon, chantier sec + un ou deux bâtisseurs + porteur, puis deux colons libres embauchés au grenier. Même
  parcours, mêmes `findPath`, même ordre que l'hôte ; aucun tirage. Pose le verrou `OpeningSiteId`.
- `void FVillage::AssignCompletedOpeningHome()` : appelée à la fin de chaque `UpdateActors` ; dès que le chantier
  d'ouverture est achevé, une tentative, le bâtisseur sans toit au chemin le plus court reçoit la maison
  (`AssignHome`), le verrou tombe, l'issue reste dans `GetOpeningHome()`. Sans verrou : ne fait rien.

Mesures :

- BUILD: PASS (`tools\unreal\anastasis-unreal.ps1 build`, worktree, avant et après le rebase sur `main` 70237070)
- TESTS (`tools\unreal\report-tests.ps1`) :
  - `-Filter Anastasis.Sim.Village.Ouverture` (après rebase) : PASS 1 / KNOWN_EXPECTED_FAILURE 0 / FAIL 0, run complet.
    - `OPENING_IN_SIM seed=12345 npc=npc-0 home=building-1 work=building-2 site=building-3 tile=(52,48) builders=npc-1,npc-2 courier=npc-1 stock=0/0 farmers=npc-3,npc-4`
    - `OPENING_IN_SIM home status=assigned npc=npc-1 home=building-3 steps=12620 time=248.1500 pieces=22 delivered=0 credited=32`
    - Deux simulations, même graine : même rapport, même `Digest()` après l'ouverture et après l'attribution.
    - Le porteur n'a rien livré en deux jours simulés au centre par défaut (48 ; 48) : le test crédite alors le
      reste du devis par `CreditSiteMaterials` (dans les deux runs au même pas), les bâtisseurs finissent, et la
      **simulation seule** attribue la maison (aucun appel d'hôte).
  - Une première version du test (douze jours simulés sans crédit) a fini `FAIL` + `RUN_INCOMPLET`
    (`GIsCriticalError=1`, sortie 255) ; son log a été écrasé par le run suivant, la cause n'est pas établie.
    Remplacée par la version ci-dessus.
  - `-Filter Anastasis.Sim.Village.Chantier` (avant rebase, même code moins le hachage `StateDigest`) : PASS 6 /
    KNOWN_EXPECTED_FAILURE 0 / FAIL 0.
- A/B d'identité de comportement, `tools\unreal\editor-batch.ps1 -Proofs npc-life-pie,material-courier-pie,settlement-site-pie`,
  base empilée (`site-from-sim-001` sur `0c06f5ad`) avant / après le changement, même base :
  - avant : `Saved/EditorBatch/20261008-113338/` ; après : `Saved/EditorBatch/20261008-114918/` ;
  - lignes `ANASTASIS_VILLAGE opening …` : **identiques** (diff vide), dans les trois preuves :
    - `opening household npc=npc-0 home=building-1 work=building-2`
    - `opening construction site=building-3 tile=(47,43) builders=npc-1,npc-2 courier=npc-1 stock=0 wood 0 stone`
    - `opening workforce granary=building-2 farmers=npc-4,npc-5`
  - `opening home assigned` : absente **avant comme après** — la maison d'ouverture ne s'achève pas dans ces
    preuves (voir ci-dessous) ; l'attribution n'est donc prouvée que par le test.
  - verdicts identiques avant / après : `material-courier-pie` PASS, `settlement-site-pie` PASS,
    `npc-life-pie` **FAIL timeout** dans les deux : chantier `building-3` à 24 bois / 0 pierre, 0 pièce posée ;
    le porteur `npc-1` est en `gatherStone` « va a la pierre ».
- `node tools/migration/check-ecarts.mjs` : `ECARTS::PASS fiches=38 ouvertes=38 fail=0 warn=9` (avertissements préexistants)
- `node tools/migration/check-state-fields.mjs -base main` : `STATE_FIELDS::PASS`

## PROOFS

Preuves PIE que le lot rejoue pour cette mission, noms de `tools/unreal/proofs.txt` (EDITOR_QUEUE_001) :

RELAIS: geo-measure-001, site-from-sim-001

PROOFS: settlement-sensitivity-pie, settlement-site-pie, npc-life-pie, villager-pie, terrain-access-pie, village-fabric-pie, material-courier-pie

## SCN

Aucun changement voulu de ce que voit le joueur : mêmes habitants, mêmes maisons, même grenier, même chantier,
mêmes bâtisseurs et porteur (A/B ci-dessus). Seule différence de moment : la maison achevée est attribuée au pas
de simulation où elle s'achève, et non plus à la frame d'hôte suivante (à `Speed` > 1, jusqu'à quelques pas plus
tôt).

## PLY

N/A

## ECARTS

- n° 40 — nouvelle, OUVERT, A_TRANCHER, EXTENSION : village d'ouverture du jeu Unreal (foyer, poste, chantier,
  bâtisseurs, porteur, embauches, attribution de la maison) décidé dans `AnastasisSim`. Pas d'équivalent dans la
  référence JS (le plus proche : `populateFoundingLife`, `seedStarterHomes`, `ensureWorkplacesDaily`,
  `restoreFounderJobs`, non portés). Activé par l'hôte seulement ; harnais : aucune section (sans verrou, la règle
  ne lit ni n'écrit rien). Marques `ecart n°40` dans `AnastasisVillageOpening.cpp`, `AnastasisVillage.{h,cpp}`,
  `AnastasisVillageStateDigest.cpp`.

## INTEGRATION_RISK

- **Dépend de `site-from-sim-001`** (même fichier `AnastasisSimulationSubsystem.cpp`, `TryStartVillage`) :
  - ses deux commits sont repris ici, rebasés sur `main` (hashes différents de sa branche) ;
  - verser `site-from-sim-001` avant ou dans le même lot, ou cette mission comme relais
    (`RELAIS: geo-measure-001, site-from-sim-001`, toutes leurs preuves dans `PROOFS:`).
- **`geo-measure-001`** : déjà sur `main` (28c70855) ; son commit a été sauté au rebase.
- **`npc-life-pie` échoue déjà sur la base empilée**, avant ce changement comme après (timeout, maison
  d'ouverture sans pierre). C'est une preuve déclarée par `site-from-sim-001` : le lot la verra `FAIL` tant que
  le porteur n'apporte pas la pierre au site (39 ; 42). Ce n'est pas causé par cette mission (A/B identique).
- **STATE_ORACLE_001** (`state-oracle-001`, désormais sur `main`) : nouveaux champs d'état `FVillage::OpeningSiteId`
  et `FVillage::OpeningHome` (struct `FOpeningHomeOutcome` : `Status`, `SiteId`, `NpcId`, `Time`), **hachés** dans
  `FVillage::StateDigest` ; rien à classer dans `tools/migration/state-fields.json`. `FOpeningReport` est un
  rapport de retour, pas un champ.
- `FVillage::Digest()` (projection de parité JS) n'est pas touché.
- Test qui crédite le devis : il ne prouve pas le porteur, seulement l'attribution par la simulation.

## STOP

- Ne revendique pas que la maison d'ouverture s'achève en PIE : elle ne s'achève pas sur la base empilée
  (`npc-life-pie` FAIL, avant comme après).
- Ne revendique pas l'identité de la ligne `opening home assigned` en PIE : absente des deux côtés de l'A/B.
- Ne porte pas `populateFoundingLife` ; la politique de site (`AnastasisSettlementSite`) reste dans l'hôte.
- Suite complète non lancée ici : elle attend le lot (`finish` → `queued`).
