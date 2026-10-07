# HANDOFF: npc-life-bridge-001

## MISSION

Faire du village initial de Play une première boucle PNJ habitable, en réutilisant la simulation existante et les Smart Objects déjà exposés par les bâtiments. Suite à l'observation d'Alexandre (« souvent immobiles, ne construisent rien »), rendre un chantier initial réellement actif sans réécrire le planificateur collectif en attente d'intégration.

## FILES_OWNED

- `Source/Anastasis_UnrealV2/Sim/AnastasisSimulationSubsystem.cpp`, `.h` : foyer et grenier atteignables pour le premier habitant du village initial ; lecteur de preuve.
- `Source/Anastasis_UnrealV2/Village/AnastasisVillagePresentation.cpp`, `.h` : miroir claim/use/release des usages intérieurs ; debug décision/navigation.
- `Source/Anastasis_UnrealV2/Village/AnastasisVillageInteractionSubsystem.cpp`, `.h` : recherche d'un slot filtrée par ID du bâtiment simulé.
- `Source/Anastasis_UnrealV2/Village/AnastasisVillagerLooks.cpp`, `AnastasisVillagerTests.cpp` : fallback visuel du métier `builder` vers le pool `settler`, et test ciblé.
- `tools/unreal/npc-life-pie.py`, `tools/unreal/proofs.txt`, `AGENTS.md` : preuve PIE et index.
- `tools/unreal/villager-pie.py` : accepte ce fallback déclaré tout en vérifiant le métier réel dans la simulation.

## COMMIT

Ce commit de mission, après `da9d93aafae8cafebccc16ecae1293deb7bd9877`.

## MEC

- BUILD: PASS, `tools/unreal/anastasis-unreal.ps1 build` (UE 5.8.2 Editor Win64 Development, après sélection de chantier corrigée).
- `git diff --check` : PASS.
- Analyse syntaxique Python de `npc-life-pie.py` et `villager-pie.py` : PASS.
- TESTS première version : `tools/unreal/editor-batch.ps1 -Proofs npc-life-pie` → `PROOF::PASS npc-life-pie (77.3s)`, `EDITOR_BATCH::PASS 1/1`.
- TESTS ajout du chantier : premier PIE échoué, car les deux bâtisseurs imposés étaient sur des îlots de navigation incompatibles ; sélection corrigée selon les chemins réels. Second PIE : `PROOF::PASS npc-life-pie (66.3s)`, `EDITOR_BATCH::PASS 1/1`, log `Saved/EditorBatch/20261007-122559/editor-batch.log`.

## PROOFS

PROOFS: npc-life-pie

## SCN

Smart Object instancié et occupé en PIE pour `npc-0` dans `building-1` (log `claimed` pendant `dort`), puis libéré aux sorties précédentes. Aucune capture visuelle jugée.

## PLY

Sans commande de scénario, `npc-0` a `home=building-1`, `work=building-2`, se déplace, livre, boit et dort. `building-3` s'ouvre sur une tuile accessible avec `npc-1,npc-3` comme bâtisseurs ; 22 pièces sont posées par des PNJ et la maison est achevée (`progress=1`, bois consommé 24, pierre 8). La preuve suit les positions des deux bâtisseurs et constate leur déplacement. Il s'agit d'une preuve PIE instrumentée, pas d'une validation joueur libre ni d'un verdict visuel. Les captures antérieures de `villager-pie` (`Saved/VillagerEvidence/pie/`) montrent un habitant présent, mais isolé dans le cadre ; elles ne rendent pas encore la relation maison/chantier lisible.

## ECARTS

AUCUN : `Source/AnastasisSim/` est inchangé. Le comportement villageois existant reste l'autorité.

## INTEGRATION_RISK

- Fichiers chauds : `AnastasisSimulationSubsystem.cpp`, `AnastasisVillagePresentation.cpp`, `AGENTS.md`, `proofs.txt`.
- Le Smart Object est un miroir de l'usage intérieur simulé ; il ne pilote pas encore la décision ou le pathfinding. Un slot occupé entraîne un retry de deux secondes, sans bloquer l'activité simulée.
- `OpeningConstruction=1` pose un seul chantier avec bois/pierre initiaux ; le transport autonome n'est pas porté. `OpeningConstruction=0` restaure le témoin sans chantier. L'ouverture périodique de nouveaux projets est le travail distinct du planificateur collectif, non versé.
- Seuls les PNJ pouvant atteindre les accès sont promus bâtisseurs (deux dans le PIE). Les autres colons du village initial peuvent encore rester oisifs ; aucun peuplement automatique de la maison achevée n'est ajouté.
- Le village initial change ; les commandes de scénario explicites continuent de le remplacer avant leurs preuves existantes.

## STOP

Pas de revendication de Character piloté par AIController, de NavMesh Unreal, de famille/âge/sexe, ni de sauvegarde supplémentaire. La navigation du village reste la grille simulée ; le corps Unreal suit par projection.
