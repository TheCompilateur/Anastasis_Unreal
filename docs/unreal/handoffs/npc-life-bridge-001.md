# HANDOFF: npc-life-bridge-001

## MISSION

Faire du village initial de Play une première boucle PNJ habitable, en réutilisant la simulation existante et les Smart Objects déjà exposés par les bâtiments. Suite à l'observation d'Alexandre (« souvent immobiles, ne construisent rien »), rendre un chantier initial réellement actif et alimenté depuis les ressources finies du monde, attribuer la maison achevée à un bâtisseur et confier la production alimentaire à d'autres habitants accessibles, sans réécrire le planificateur collectif en attente d'intégration.

## FILES_OWNED

- `Source/Anastasis_UnrealV2/Sim/AnastasisSimulationSubsystem.cpp`, `.h` : foyer et grenier atteignables pour le premier habitant du village initial ; lecteur de preuve.
- `Source/Anastasis_UnrealV2/Village/AnastasisVillagePresentation.cpp`, `.h` : miroir claim/use/release des usages intérieurs ; debug décision/navigation.
- `Source/Anastasis_UnrealV2/Village/AnastasisVillageInteractionSubsystem.cpp`, `.h` : recherche d'un slot filtrée par ID du bâtiment simulé.
- `Source/Anastasis_UnrealV2/Village/AnastasisVillagerLooks.cpp`, `AnastasisVillagerTests.cpp` : fallback visuel du métier `builder` vers le pool `settler`, et test ciblé.
- `tools/unreal/npc-life-pie.py`, `tools/unreal/proofs.txt`, `AGENTS.md` : preuve PIE et index.
- `tools/unreal/villager-pie.py` : accepte ce fallback déclaré tout en vérifiant le métier réel dans la simulation.
- `Source/AnastasisSim/Public/Village/AnastasisVillage.h`, `Private/Village/AnastasisVillage.cpp`, `Private/Village/AnastasisVillageMaterialCourier.cpp`, `Private/Tests/AnastasisVillageMaterialCourierTests.cpp`, `ECARTS.md` : extension de porteur de matériaux, désactivée par défaut, avec conservation des ressources et test de chantier sec.
- `tools/unreal/material-courier-pie.py` : preuve PIE du scénario explicite à sec ; `FirstSite ... Delivered=0` choisit un bâtisseur porteur.

## COMMIT

Les commits de cette branche, après `da9d93aafae8cafebccc16ecae1293deb7bd9877`.

## MEC

- BUILD: PASS, `tools/unreal/anastasis-unreal.ps1 build` (UE 5.8.2 Editor Win64 Development, après ouverture du chantier initial a sec).
- `git diff --check` : PASS.
- Analyse syntaxique Python de `npc-life-pie.py`, `villager-pie.py` et `material-courier-pie.py` : PASS.
- TESTS première version : `tools/unreal/editor-batch.ps1 -Proofs npc-life-pie` → `PROOF::PASS npc-life-pie (77.3s)`, `EDITOR_BATCH::PASS 1/1`.
- TESTS ajout du chantier : premier PIE échoué, car les deux bâtisseurs imposés étaient sur des îlots de navigation incompatibles ; sélection corrigée selon les chemins réels. Second PIE : `PROOF::PASS npc-life-pie (66.3s)`, `EDITOR_BATCH::PASS 1/1`, log `Saved/EditorBatch/20261007-122559/editor-batch.log`.
- TESTS attribution de la maison : `PROOF::PASS npc-life-pie (68.8s)`, `EDITOR_BATCH::PASS 1/1`, log `Saved/EditorBatch/20261007-123303/editor-batch.log` ; `npc-3` propriétaire de `building-3`, `home=building-3`, `ownerRests=1` au verdict.
- TESTS travail alimentaire partagé : `PROOF::PASS npc-life-pie (99.2s)`, `EDITOR_BATCH::PASS 1/1`, log `Saved/EditorBatch/20261007-124625/editor-batch.log` ; `npc-4,npc-5` affectés au grenier, trois fermiers au total, cinq livraisons par des fermiers autres que `npc-0`.
- MATERIAUX : premier build invalidé car `ECARTS.md` a bougé pendant l'empreinte ; second `BUILD::PASS`. Test ciblé `Village.Chantier.PorteurMateriaux` PASS 1/1 avant les gardes de trajet/surplus. Après ces gardes, build PASS et `editor-batch.ps1 -Proofs material-courier-pie,npc-life-pie` → PASS 2/2, log `Saved/EditorBatch/20261007-132943/editor-batch.log`.
- PLAY INITIAL SANS STOCK : build PASS ; `editor-batch.ps1 -Proofs npc-life-pie` → `PROOF::PASS npc-life-pie (152.6s)`, log `Saved/EditorBatch/20261007-134926/editor-batch.log` ; ouverture `stock=0 wood 0 stone`, porteur `npc-1`, 32 matériaux livrés, 22 pièces posées, maison achevée et attribuée, `npc-0` boit/livre/dort, six livraisons des autres fermiers.

## PROOFS

PROOFS: npc-life-pie,material-courier-pie

## SCN

Smart Object instancié et occupé en PIE pour `npc-0` dans `building-1` (log `claimed` pendant `dort`), puis libéré aux sorties précédentes. Aucune capture visuelle jugée.

## PLY

Sans commande de scénario, `npc-0` a `home=building-1`, `work=building-2`, se déplace, livre, boit et dort. `building-3` s'ouvre sur une tuile accessible avec `npc-1,npc-3` comme bâtisseurs et ZERO stock ; `npc-1` se charge au terrain vivant et apporte 24 bois + 8 pierre au seuil ; 22 pièces sont posées et la maison est achevée (`progress=1`). La preuve suit les positions des deux bâtisseurs et constate leur déplacement. A l'achevement, `npc-1` adopte `building-3` comme foyer ; `ownerRests=13` au verdict. Deux autres habitants (`npc-4,npc-5`) rejoignent le grenier ; six livraisons sont attribuees aux fermiers autres que `npc-0`. Il s'agit d'une preuve PIE instrumentée, pas d'une validation joueur libre ni d'un verdict visuel. Les captures antérieures de `villager-pie` (`Saved/VillagerEvidence/pie/`) montrent un habitant présent, mais isolé dans le cadre ; elles ne rendent pas encore la relation maison/chantier lisible.

## ECARTS

n°18 modifié, A_TRANCHER : porteur explicite opt-in sur chantier sec, sans prétendre porter le camp, le stock collectif, les salaires, les demandes ou l'ouverture autonome de la référence. Le harnais reste désactivé par défaut.

## INTEGRATION_RISK

- Fichiers chauds : `AnastasisSimulationSubsystem.cpp`, `AnastasisVillagePresentation.cpp`, `AGENTS.md`, `proofs.txt`.
- Le Smart Object est un miroir de l'usage intérieur simulé ; il ne pilote pas encore la décision ou le pathfinding. Un slot occupé entraîne un retry de deux secondes, sans bloquer l'activité simulée.
- `OpeningConstruction=1` pose un seul chantier a sec ; un bâtisseur accessible est porteur. `OpeningConstruction=0` restaure le témoin sans chantier. L'ouverture périodique de nouveaux projets est le travail distinct du planificateur collectif, non versé.
- Seuls les PNJ pouvant atteindre les accès sont promus bâtisseurs (deux dans le PIE). Un bâtisseur sans foyer et capable d'atteindre l'entrée reçoit la maison achevée une seule fois. Les autres colons du village initial peuvent encore rester oisifs ; aucun peuplement général des logements ni projet récurrent n'est ajouté.
- Jusqu'à deux colons encore sans métier, dont la grille valide l'accès au grenier, y sont affectés comme fermiers au démarrage. Le dernier PIE observe six livraisons de fermiers autres que `npc-0` ; le compteur agrégé ne répartit pas ces actes entre `npc-4` et `npc-5`.
- Le porteur de matériaux couvre un seul PNJ et une seule charge a la fois ; le chantier initial de Play utilise ce trajet reel, prouvé par PIE. Le camp, le stock collectif, les salaires, demandes de livraison et nouveaux projets autonomes restent absents. Si la source est introuvable, le porteur attend et réessaie apres huit secondes, sans injection fictive.
- Le village initial change ; les commandes de scénario explicites continuent de le remplacer avant leurs preuves existantes.

## STOP

Pas de revendication de Character piloté par AIController, de NavMesh Unreal, de famille/âge/sexe, ni de sauvegarde supplémentaire. La navigation du village reste la grille simulée ; le corps Unreal suit par projection.
