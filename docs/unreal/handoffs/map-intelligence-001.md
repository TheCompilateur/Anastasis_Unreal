# HANDOFF: map-intelligence-001

## MISSION

Mesurer en lecture seule la géographie exploitable de la carte ANASTASIS : pentes,
rugosité, relief, continuité, eau, navigation, candidats et corridors, avec snapshots
appariés et interface Editor-only. Aucun changement de topographie ou gameplay.

## FILES_OWNED

- Content/Python/init_unreal.py (enregistrement du menu seulement)
- Content/Python/anastasis_map_intelligence/__init__.py
- Content/Python/anastasis_map_intelligence/core.py
- Content/Python/anastasis_map_intelligence/editor.py
- Content/Python/anastasis_map_intelligence/smoke.py
- Content/Python/anastasis_map_intelligence/tests/test_core.py
- Content/Python/anastasis_map_intelligence/tests/test_adapter.py
- docs/unreal/MAP_INTELLIGENCE.md
- docs/unreal/handoffs/map-intelligence-001.md

## COMMIT

BRANCH_HEAD — branche agent/map-intelligence-001, base ce9ecfa73e8398c24bc1f7b72f044479b6d3238e.
Worktree géré par Codex : C:/Users/alex_/.codex/worktrees/map-intelligence-001/ANASTASIS_UNREAL.
Le fournisseur de worktree impose ce chemin ; branche de mission conservée.
Aucune fusion, aucun push, aucune intégration canonique.

## MEC

- C++ BUILD: NOT_APPLICABLE, aucun changement C++/Build.cs/uproject/plugin.
- Tests ciblés : PASS=27, KNOWN_EXPECTED_FAILURE=0, FAIL=0.
- Suite Unreal générale : NOT_RUN ; ne pas confondre ces 27 tests avec la suite globale.
- Tests synthétiques : plan incliné connu, rugosité plane nulle, relief accidenté,
  grandes/petites poches, contacts diagonaux, rupture de hauteur, eau, corridors,
  NavMesh inconnu, séparation d'îlots nav, export JSON/Markdown, refus d'écrasement,
  comparaison incompatible, changement de couverture chargée, budgets et transforms.
- Tests d'adaptateur : doublures des retours UE Python, pas preuve live de Landscape/WaterBody/NavMesh.
- Smoke dédié UE 5.8.2 CL 56702186 sur /Game/Anastasis/Maps/Lvl_AnastasisSlice :
  menu enregistré, deux échantillonnages identiques, exports et comparaison produits,
  aucun Actor ajouté/retiré, aucun package de carte marqué dirty.
- Relecture finale : fonctions d'acquisition sans setter de terrain, save de niveau,
  création/destruction d'Actors ou modification d'assets. Diff limité aux neuf fichiers ci-dessus.

Commande de tests dans le guide MAP_INTELLIGENCE.md ; smoke permanent :
`from anastasis_map_intelligence import smoke; smoke.run()` dans la console Python de l'éditeur.

Dossier des preuves brutes (hors Git) :
C:/Users/alex_/.codex/visualizations/2026/09/29/01a0eec3-f3bb-7ea3-b905-b86ada136008/

- tests.txt : résultats nommés des 27 tests.
- source_manifest.json : empreintes SHA256 des sources Python de livraison.
- api_probe.json / api_probe.log : signatures réellement exposées par UE 5.8.2.
- permanent_smoke/Smoke.json et First.json / Second.json : répétabilité et non-mutation.
- final_live_smoke.json : sensibilité au pas 2 m / 1 m, refus de les comparer comme une paire.
- final_live/MapAudit_Baseline.json et MapAudit_FineResolution.json : cellules et paramètres bruts.
- visual_check_v3/Latest.json : acquisition finale depuis une instance avec rendu D3D12.
- visual_v3_proof.json / visual_v3_*.png / visual_v3_smoke.log : affichage et Clear vérifiés.

Les sorties sous final_live précèdent l'ajout des chemins de terrain/moteur à la clé de
comparaison ; les mesures géométriques sont inchangées. La répétabilité finale utilise
la clé étendue dans permanent_smoke, puis visual_check_v3. Aucun seal source/binaire du
canonique n'est revendiqué : la géométrie réellement chargée est la source mesurée.

## SCN

SCN::PASS pour la branche procédurale observée, le menu et les deux couches contrôlées.
Les images visual_v3_slope.png et visual_v3_habitability.png montrent les points sur
le terrain ; visual_v3_clear.png montre leur disparition. Revue visuelle effectuée.
visual_v3_proof.json confirme la même empreinte des échantillons et aucune carte dirty.
Les premières images visual_*.png (ciel) et visual_v2_*.png (points non visibles) sont
EXCLUES de la preuve. Le défaut d'affichage a été corrigé par des points en avant-plan
à durée 0,25 s, renouvelés toutes les 0,2 s. Clear retire le callback ; la dernière
émission expire au tick éditeur, sans flush des dessins des autres outils.
Les modes agriculture/navigation/connectivité/candidats/corridors partagent ce renderer,
mais n'ont pas tous fait l'objet d'une capture interprétée sur une fixture réelle.

Inspection réelle : terrain procédural ExperimentalTerrain (pas de Landscape chargé),
section 0 sol / section 1 eau ; six Actors chargés ; World Partition=false.
PCG et Water sont montés dans le journal moteur mais la carte n'a pas de WaterBody
ou de NavigationData chargé. Aucun plugin n'a été activé par cette mission.
Le .uproject garde ses deux modules Runtime AnastasisSim et Anastasis_UnrealV2 ;
Blueprints de carte et gameplay restent consommés, sans modification d'assets.

Mesures individuelles (pas des estimations de population nourrissable) :

| Mesure | Pas demandé 2 m | Pas demandé 1 m |
|---|---:|---:|
| Grille | 48 x 48 | 95 x 95 |
| Emprise XY | 9 025 m² | 9 025 m² |
| Couverture échantillonnée | 100 % | 100 % |
| Terrain traversable selon seuil géométrique 30° | 18,27 % | 22,65 % |
| Composantes terrain | 91 | 189 |
| Plus grande composante terrain | 509,22 m² | 578 m² |
| Agriculture candidate après filtre 100 m² | 0 m² | 105 m² |
| Bassins habitat après filtres 200 m² / largeur 8 m | 0 | 0 |
| Navigabilité NavMesh | UNKNOWN | UNKNOWN |
| Temps acquisition/analyse observé | 4,60 s | 5,68 s |

Rugosité locale à 2 m, relief à 8 m. Les différences de résolution sont une sensibilité,
pas un traitement avant/après. Les règles de classification sont des paramètres de design.

## PLY

UNKNOWN / NOT_ATTEMPTED. Pas de parcours joueur, de route construite, de village,
ni de preuve de fertilité, de risque d'inondation ou de capacité de subsistance.

## INTEGRATION_RISK

- Point de conflit possible : Content/Python/init_unreal.py ; conserver les autres imports.
- Préexistant dans le canonique et exclu : Config/DefaultInput.ini,
  Source/Anastasis_UnrealV2/Anastasis_UnrealV2Character.cpp/.h, .claude/.
- Les acquisitions utilisent les binaires existants dans une instance canonique dédiée,
  avec import du module Python depuis ce worktree ; pas de contrôle de l'éditeur des autres agents.
- Les branches natives Landscape/WaterBody/NavMesh attendent des fixtures réelles dédiées.
- Une représentation de collision WaterBody absente rend la métrique d'eau inconnue ;
  les critères secs sont alors explicitement conditionnels.
- Analyse des objets chargés seulement, pas chargement automatique World Partition.
- Un changement d'échelle vers 1,9 km exige un protocole d'emprise commun pour comparer.
- Aucun acteur par cellule ; rendu plafonné ; le noyau synchrone reste soumis aux budgets.
- agent-worktree.ps1 finish n'est pas exécuté : ce portail cible le répertoire traditionnel
  C:/dev/ANASTASIS_WORKTREES, différent du worktree géré. Aucun HANDOFF_READY::YES inventé.

## STOP

Lot isolé soumis à revue de l'intégrateur. Ne pas fusionner automatiquement.
NEXT : appliquer sélectivement ce commit, conserver le hook Python existant et lancer
le smoke sur la carte choisie ; ensuite constituer la vraie paire Baseline/V2 avec
même emprise, grille et paramètres. Terrain, assets, Player et travaux voisins hors mission.
