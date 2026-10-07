# HANDOFF: npc-life-bridge-001

## MISSION

Faire du village initial de Play une première boucle PNJ habitable, en réutilisant la simulation existante et les Smart Objects déjà exposés par les bâtiments.

## FILES_OWNED

- `Source/Anastasis_UnrealV2/Sim/AnastasisSimulationSubsystem.cpp`, `.h` : foyer et grenier atteignables pour le premier habitant du village initial ; lecteur de preuve.
- `Source/Anastasis_UnrealV2/Village/AnastasisVillagePresentation.cpp`, `.h` : miroir claim/use/release des usages intérieurs ; debug décision/navigation.
- `Source/Anastasis_UnrealV2/Village/AnastasisVillageInteractionSubsystem.cpp`, `.h` : recherche d'un slot filtrée par ID du bâtiment simulé.
- `tools/unreal/npc-life-pie.py`, `tools/unreal/proofs.txt`, `AGENTS.md` : preuve PIE et index.

## COMMIT

PENDING

## MEC

- BUILD: PASS, `tools/unreal/anastasis-unreal.ps1 build` (UE 5.8.2 Editor Win64 Development, après correction d'un include Smart Object).
- `git diff --check` : PASS.
- `python -m py_compile tools/unreal/npc-life-pie.py` : PASS.
- TESTS: `tools/unreal/editor-batch.ps1 -Proofs npc-life-pie` → `PROOF::PASS npc-life-pie (77.3s)`, `EDITOR_BATCH::PASS 1/1`.

## PROOFS

PROOFS: npc-life-pie

## SCN

Smart Object instancié et occupé en PIE pour `npc-0` dans `building-1` (log `claimed` pendant `dort`), puis libéré aux sorties précédentes. Aucune capture visuelle jugée.

## PLY

PIE instrumenté PASS : sans commande de scénario, `npc-0` a `home=building-1`, `work=building-2`; buts observés `deliver,drink,eat,gatherFood,observer,relax,rest,socialize`; `drinks=1`, `deliveries=1`, `rests=1`, déplacement > 1 case et claim Smart Object. Log : `Saved/EditorBatch/20261007-115438/editor-batch.log`, ligne `NPC_LIFE_PIE PASS`. Cela ne juge pas encore la lisibilité visuelle à hauteur de joueur.

## ECARTS

AUCUN : `Source/AnastasisSim/` est inchangé. Le comportement villageois existant reste l'autorité.

## INTEGRATION_RISK

- Fichiers chauds : `AnastasisSimulationSubsystem.cpp`, `AnastasisVillagePresentation.cpp`, `AGENTS.md`, `proofs.txt`.
- Le Smart Object est un miroir de l'usage intérieur simulé ; il ne pilote pas encore la décision ou le pathfinding. Un slot occupé entraîne un retry de deux secondes, sans bloquer l'activité simulée.
- Le village initial change ; les commandes de scénario explicites continuent de le remplacer avant leurs preuves existantes.

## STOP

Pas de revendication de Character piloté par AIController, de NavMesh Unreal, de famille/âge/sexe, ni de sauvegarde supplémentaire. La navigation du village reste la grille simulée ; le corps Unreal suit par projection.
