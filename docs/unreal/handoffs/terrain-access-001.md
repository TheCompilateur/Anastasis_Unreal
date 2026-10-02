# HANDOFF: terrain-access-001

## MISSION
Diagnostic borne des chemins maison/eau/champ/grenier compares au maillage rendu,
et observation separee d'un fermier autonome. Aucune correction de terrain/navigation.

## FILES_OWNED
- Source/Anastasis_UnrealV2/Sim/AnastasisTerrainAccessProbe.h
- Source/Anastasis_UnrealV2/Sim/AnastasisTerrainAccessProbe.cpp
- Content/Python/anastasis_terrain_access.py
- Content/Python/anastasis_map_intelligence/tests/test_terrain_access.py
- tools/unreal/terrain-access-pie.py
- tools/unreal/proofs.txt (une ligne)
- AGENTS.md (une ligne d'index)
- docs/unreal/TERRAIN_ACCESS_001.md
- docs/unreal/handoffs/terrain-access-001.md

## COMMIT
Commit contenant cette fiche sur agent/terrain-access-001.
Base 73248bcf6139156d9ade2f7207126c93b742912f.

## MEC
- Python : 12 tests synthetiques PASS (unittest), dont rupture entre deux surfaces planes.
- Script PIE : AST PASS.
- BUILD : PASS, UBT 229.56 s, arbre C++ final.
- Index outils : Missing={}, Stale={}. Registre : DRYRUN 1 preuve terrain-access-pie.
- ECARTS : NON_CONCERNE, fail=0, warn=4 preexistants.
- Runtime : QUEUED. Aucun editeur lance pour cette mission.

## PROOFS
PROOFS: terrain-access-pie
INSTRUMENT_PASS ne signifie pas que les chemins sont coherents ou tous parcourus.
Lire audit.json, les statuts par chemin et movement_summary.

## SCN
UNKNOWN avant execution du lot. Exports numeriques des maillages et positions visuelles,
pas de capture artistique ni de validation physique des collisions.

## PLY
UNKNOWN. Aucun joueur pilote.

## ECARTS
AUCUN : aucun fichier ni comportement AnastasisSim modifie.

## INTEGRATION_RISK
- AGENTS.md et proofs.txt partages : conserver les autres lignes lors de l'empilement.
- Aucune dependance aux branches geography-concordance-001, river-use-001 ou fertility-food-ab-001.
- La sonde demande un village vide et cree sa fixture via SeedFirstFarmer plus maison.
- 180 secondes simulees a TimeScale 0.5 ; plafond 480 s murales, registre 540 s.
- Coherence et parcours complets restent UNKNOWN si les donnees sont insuffisantes,
  meme lorsqu'un INSTRUMENT_PASS indique une acquisition valide.
- Le budget de recherche par defaut et le premier seuil du grenier peuvent produire
  un chemin absent. Cela ne prouve pas une impossibilite physique generale.
- L'integrateur execute la preuve ; ne pas lancer un deuxieme editeur ni integrer ici.

## STOP
Aucune nouvelle regle de pente, aucun asset modifie, aucune conclusion runtime avant le lot.
