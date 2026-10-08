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
- Relais (commits repris, voir INTEGRATION_RISK) : `site-from-sim-001` et `water-network-001` (`geo-measure-001` est déjà sur `main`)

## COMMIT

Voir `git log main..agent/opening-in-sim-001` (base `main` = `640fa3e8`) : les deux commits de `site-from-sim-001`,
les deux de `water-network-001` (mêmes hashes que sa branche, sommet `7c6be7e1`), puis le commit de cette mission.

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

### MEC empilée (2026-10-08, `main` = `640fa3e8` + `water-network-001` `7c6be7e1` + cette mission)

- BUILD: PASS (`tools\unreal\anastasis-unreal.ps1 build`, worktree)
- TESTS: `tools\unreal\report-tests.ps1 -Filter 'Anastasis.Sim.Village.Ouverture+Anastasis.WaterNetwork+Anastasis.Sim.Empreinte+Anastasis.SettlementSite+Anastasis.Sim.Monde.Eau.Reseau+Anastasis.Sim.Village.Chantier'`
  → PASS 19 / KNOWN_EXPECTED_FAILURE 0 / FAIL 0, 19 annoncés, run complet.
  - `OPENING_IN_SIM` : rapport et attribution identiques à la mesure d'avant (`home status=assigned npc=npc-1 home=building-3 steps=12620`).
  - `WATER_NETWORK seed=12345 network_water=893 js_water=1198 changed=833` ; `WATER_NETWORK_SITE site=(36,20)`.
- PROOFS: `tools\unreal\editor-batch.ps1 -Proofs geography-concordance-pie,settlement-sensitivity-pie,settlement-site-pie,npc-life-pie,material-courier-pie,terrain-access-pie,villager-pie,village-fabric-pie`
  → `Saved/EditorBatch/20261008-150624/`, `EDITOR_BATCH::PASS 8/8` :
  - `geography-concordance-pie` PASS : 0 case en désaccord dans l'état de référence ;
  - `settlement-sensitivity-pie` PASS : site (36 ; 20) dans les quatre états (`ref`, `drainage0`, `humangeo0`, `ref2`), STABLE ;
  - `settlement-site-pie` PASS ;
  - `npc-life-pie` PASS (nouveau script Warp 10 de `npc-life-warp-001`) : 22 pièces, propriétaire `npc-1`,
    32 matériaux livrés, 99 s simulées ;
  - `material-courier-pie` PASS : 32 livrés (24 bois, 8 pierre) ;
  - `terrain-access-pie` INSTRUMENT_PASS ; `villager-pie` PASS ; `village-fabric-pie` PASS.
- Lignes d'ouverture en PIE (10 démarrages) : `opening household npc=npc-0 home=building-1 work=building-2`,
  `opening construction site=building-3 tile=(40,21) builders=npc-1,npc-2 courier=npc-1 stock=0 wood 0 stone`,
  `opening workforce granary=building-2 farmers=npc-3,npc-4`, et **`opening home assigned npc=npc-1 home=building-3`** :
  sur le site du réseau, la maison d'ouverture s'achève et la **simulation** l'attribue, en jeu.
- `check-ecarts` : `ECARTS::PASS fiches=41` ; `check-state-fields` : `STATE_FIELDS::PASS structures=44 lacunes=2` (strict).

## PROOFS

Preuves PIE que le lot rejoue pour cette mission, noms de `tools/unreal/proofs.txt` (EDITOR_QUEUE_001) :

RELAIS: site-from-sim-001, water-network-001

PROOFS: geography-concordance-pie, settlement-sensitivity-pie, settlement-site-pie, npc-life-pie, material-courier-pie, terrain-access-pie, villager-pie, village-fabric-pie, save-load-pie, chronicle-pie

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
- n° 51 — repris du relais `water-network-001`, inchangé : OUVERT, A_TRANCHER (l'eau de la simulation suit le réseau
  de drainage rendu). Fiche et marques : voir la fiche de `water-network-001`.

## INTEGRATION_RISK

- **Rebasée une troisième fois le 2026-10-08 sur `main` = `e82fc307`** (après `relay-memoire-001`) : seul conflit le numéro de
  format (`SaveFormatVersion` 5). `settlement-site-pie` : son contrôle `twelve_npcs` attendait exactement 12 habitants ;
  depuis `familles-feu-001` (versée), le village part de 14 fondateurs. Il échouait donc aussi sur `main`. Il vérifie
  désormais qu'au moins 12 habitants sont posés (`at_least_twelve_npcs`).
- **Rebasée de nouveau le 2026-10-08 sur `main` = `da0c3094`** (après `familles-feu-001` et `save-history-001`) :
  - `SeedStartVillage` : fondateurs (`AnastasisFounders::Seed`), puis l'ouverture décidée par la simulation,
    puis le récit de la fondation (`TellFounding`) ;
  - la correction des familles dans l'ouverture (un bâtiment posé ne doit pas couper le résident du puits,
    `ReachesWell`) est **portée dans `FVillage::SeedOpeningVillage`** : sans elle, la régression « mort de
    soif chez lui » revenait ;
  - fin d'`UpdateActors` : la maison d'ouverture est attribuée, puis la biographie l'observe dans le même pas ;
  - `SaveFormatVersion` 3 (`main` en est à 2) ;
  - `chronicle-pie` ajoutée aux preuves : l'ouverture précède le récit de la fondation.
- **Rebasée le 2026-10-08 sur `main` = `ad18bb99`** (après `chronique-village-001` et `save-state-001`), conflits
  résolus à la main :
  - `TryStartVillage` : garde `SeedStartVillage` de `chronique-village-001`, dont le corps devient
    `SeedOpeningVillage` (la simulation décide) ;
  - `ResetCanonical` : chronique et eau du réseau gardées toutes deux ;
  - `AnastasisVillageStateDigest.cpp` : les champs d'ouverture sont écrits dans le nouveau parcours
    `VisitState` de la sauvegarde ;
  - le verrou d'ouverture n'est plus un champ de l'hôte : retiré du `USaveGame` (`HostFormatVersion` 2), et
    `SaveFormatVersion` 2 (le parcours a changé).
  - Preuves rejouées sur cet arbre : `EDITOR_BATCH::PASS 9/9`, `save-load-pie` comprise (la sauvegarde
    porte maintenant l'ouverture).
- **`SaveFormatVersion`** : 5 sur cette pile (`main` = 4, après `relay-memoire-001`). `arrivants-001` et `valmire-grows-001` le
  montent aussi à 5 : la mission versée en second prend 6 (rebase mécanique, je le fais).
- **Empilée sur `water-network-001`, qui porte `site-from-sim-001`** (2026-10-08, rebase sur `agent/water-network-001`
  = `7e7bf55f`) :
  - verser `water-network-001` d'abord, ou dans le même lot ; cette mission peut aussi passer seule comme relais
    des deux (`RELAIS: site-from-sim-001, water-network-001`, toutes leurs preuves dans `PROOFS:`) ;
  - **ne jamais verser `site-from-sim-001` seule** : sur `main` récent elle fait échouer `npc-life-pie` (voir sa fiche).
- **`geo-measure-001`** : déjà sur `main` (28c70855).
- **ECARTS** : n°40 (cette mission) et n°51 (`water-network-001`, renuméroté à chaque versement voisin : n°43 à 50 sont pris par `labor-social-001`, `familles-feu-001`, `save-state-001`, `save-history-001`, `relay-memoire-001`, `arrivants-001` et `valmire-grows-001`) ; `check-ecarts` PASS sur l'arbre empilé.
- **Mesures de la section MEC** : faites sur l'ancienne base (`site-from-sim-001` seule) ; l'arbre empilé est rejugé
  ci-dessous (section `MEC empilée`) et par le lot.
- **`npc-life-pie`** : elle échouait sur l'ancienne pile (`site-from-sim-001` seule, site (39 ; 42), porteur bloqué),
  avant comme après ce changement. Sur la pile actuelle (eau du réseau, site (36 ; 20)), elle passe.
- **STATE_ORACLE_001** (`state-oracle-001`, désormais sur `main`) : nouveaux champs d'état `FVillage::OpeningSiteId`
  et `FVillage::OpeningHome` (struct `FOpeningHomeOutcome` : `Status`, `SiteId`, `NpcId`, `Time`), **hachés** dans
  `FVillage::StateDigest` ; rien à classer dans `tools/migration/state-fields.json`. `FOpeningReport` est un
  rapport de retour, pas un champ.
- `FVillage::Digest()` (projection de parité JS) n'est pas touché.
- Test qui crédite le devis : il ne prouve pas le porteur, seulement l'attribution par la simulation.

## STOP

- L'A/B d'identité (MEC, première partie) a été fait sur l'ancienne pile, où la maison ne s'achevait pas : il
  ne compare pas la ligne `opening home assigned`, vue seulement sur la pile actuelle (après le changement).
- Ne porte pas `populateFoundingLife` ; la politique de site (`AnastasisSettlementSite`) reste dans l'hôte.
- Suite complète non lancée ici : elle attend le lot (`finish` → `queued`). Les tests ciblés et les 8 preuves ont tourné.
