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

## Integration et preuve canonique — 2026-09-29

Integration autorisee par Alexandre : commit auditeur `50a66d8` sur `main`, apres
rebase sur `afaf2b6`. Les neuf fichiers du lot sont identiques au commit initial.
Les trois modifications locales Input/Character ont ete conservees octet pour octet.
Le smoke importe maintenant le module depuis le canonique : six controles PASS,
mesures repetables, Actors/cartes dirty inchanges, dessin borne et callback retire.
Les 27 tests Python du lot sont PASS ; 0 KNOWN_EXPECTED_FAILURE ; 0 FAIL.

Le premier smoke post-integration chargeait encore une emprise de 95 x 95 m.
Apres compilation canonique reelle (13 actions, `BUILD::PASS`) et relance dediee,
la carte charge directement 1900 x 1900 m, avant toute regeneration manuelle.
Les valeurs runtime sont Scale=5 et HumanGeography=1. Une regeneration explicite
conserve cette emprise. Aucun patch de terrain ni sauvegarde de carte necessaire.

Paire live Baseline/V2 : seed 12345, meme carte, grille demandee 8 m, rugosite
locale 8 m, relief 32 m ; toggles HumanGeography=0 puis 1, regeneration en memoire.
Les sommets ET triangles des sections sol/eau de chaque variante correspondent
exactement aux exports corrected_relief cites dans human-geography-v2.

| Mesure geometrique | Baseline | V2 |
|---|---:|---:|
| Emprise echantillonnee | 361 ha | 361 ha |
| Plus grande zone habitable candidate | 11,3888 ha | 34,5806 ha |
| Surface agricole candidate | 38,9079 % | 42,4617 % |
| Composantes terrain traversables | 128 | 148 |
| Navigation Unreal | UNKNOWN | UNKNOWN |

Chaque analyse conserve les Actors et les cartes dirty. Les 171 fichiers suivis
Source/Config/Python/Maps controles sont inchanges pendant la sonde ; HEAD stable.
L'editeur dedie s'est ferme. Les autres editeurs n'ont pas ete controles ou fermes.
Le build comprend les modifications locales preexistantes Input/Character :
ceci n'est pas un seal d'un arbre Git propre, ni une preuve joueur ou visuelle V2.
La fragmentation globale n'est pas uniformement amelioree ; corridors plafonnes a 64.

Preuves brutes sous
`C:/Users/alex_/.codex/visualizations/2026/09/29/01a0eec3-f3bb-7ea3-b905-b86ada136008/` :
- `integration_result.json`, `integrated_smoke/Integration.json` ;
- `canonical_regeneration_build.log`, `canonical_regeneration.log` ;
- `canonical_regeneration_probe.py`, `canonical_regeneration/Diagnostic.json` ;
- `canonical_regeneration/Baseline.json`, `HumanGeographyV2.json`,
  `Comparison_Baseline_V2.json` et leurs rapports Markdown dans ce meme dossier ;
- `regeneration_source_before.json`, `regeneration_postcheck.json`.

## Preuve commune du village — 2026-09-30

Mandat : observer ensemble quatre habitants, un puits, deux logements et un grenier,
pendant deux jours simules, sans commande corrective apres l'assemblage. Aucun code
runtime nouveau : seules les commandes existantes FirstHouse / FirstWell / FirstGranary
sont composees dans un editeur PIE dedie. Base mesuree : `ee575f8`.

Protocole initial : seed 12345, Speed=1, FirstHouse 4 47 47, FirstWell 0 49 48,
FirstGranary 0 96 50 48. Deux jours = 180 secondes de simulation, jour de 90 secondes.
L'implantation recherche ses cases libres : positions effectives dans les logs.
355 snapshots : quatre habitants et quatre batiments dans chacun. Aucun apport de
nourriture, retrait, teleportation d'habitant ou changement de besoin apres le depart.
Le deplacement de la camera d'observation ne commande pas les habitants.

| Habitant | Boissons confirmees | Repos confirmes | Repas avec consommation confirmee |
|---|---:|---:|---:|
| npc-0 | 2 | 6 | 0 |
| npc-1 | 1 | 9 | 2 |
| npc-2 | 1 | 9 | 0 |
| npc-3 | 0 | 30 | 0 |

- RUN COMPLETE : 180,000 secondes observees. Stock 96 -> 94 ; invariant
  `stock = 96 - repas comptabilises` vrai dans les 355 snapshots.
- Quatre recuperations d'energie observees. Trois habitants boivent ; trois ont
  une baisse de faim. npc-3 termine energie=100, faim=48,84, soif=46,32, sante=100,
  encore au repos. Ce run ne prouve ni mortalite ni rupture de survie.
- npc-0 et npc-2 reduisent leur faim a domicile sans repas comptabilise, inventaire
  alimentaire nul. Cela rejoint la dette "foyer avant grenier" de GRANARY_EAT_001.
- L'urgence de sommeil conservee de npc-3 reste elevee malgre energie=100.
  L'inertie perimee est une hypothese causale soutenue par la trace et la dette
  documentee ; pas un effet de correction demontre par A/B.
- Les vitesses echantillonnees atteignent environ 80 m par seconde simulee
  (4 tuiles/s x 20 m/tuile). Il s'agit des marqueurs projetes, pas d'une marche humaine.
- Navigation physique / evitement des obstacles 3D : UNKNOWN. Aucun NavigationData
  charge ; le deplacement observe suit la grille de simulation et sa projection.

SCN : controle visuel separe de 24 secondes, meme assemblage, capture standard
`Shot showui` : etiquettes superposees et tronquees, marqueurs de debug peu lisibles.
Critere de lisibilite NON SATISFAIT dans ces vues. Les HighResShot du run de deux
jours ne montrent pas le debug et ne fondent pas ce verdict. Aucun PLY humain.

Verification : build canonique `BUILD::PASS::CACHED`, empreintes sources/modules
inchangees depuis le build valide. HEAD et fichiers suivis Source/Config/Python/Maps
stables, etat dirty des cartes inchange, git propre apres la preuve. Aucun nouveau
run de suite automation ; aucun ajout au registre KNOWN_EXPECTED_FAILURE.
Les editeurs de preuve se sont fermes. Le PID de l'editeur utilisateur initial
n'etait plus present a la fin ; sa fermeture n'a pas ete observee ni attribuee.

Preuves locales dans le dossier de preuves deja cite :
- `village_common_probe.py`, `analyze_village_common.py`, `village_common.log` ;
- `village_common/Run.json`, `Analysis.json`, `Snapshots.json` ;
- `village_common_visual_probe.py`, `village_common_visual.log` ;
- `village_common_visual/overview_008.png`, `ground_014.png`, `Run.json` ;
- `village_common_preflight.json`, `village_common_postflight.json`.
Deux tentatives de lancement preliminaires sont exclues : sortie prematuree du
mode ExecutePythonScript, puis mauvais accesseur Python de CameraActor. Journaux
conserves sous `village_common_bootstrap.log` et `village_common_camera_error.*`.
Les preuves generees et scripts experimentaux restent hors Git ; ce commit ne
modifie que cette fiche de passation, pas les mecanismes observes.

## STOP

Auditeur integre ; comparaison geometrique et preuve commune terminees.
Le critere de village autonome et lisible n'est pas valide par cette campagne.
NEXT : A/B borne du recalcul d'urgence d'une decision Nous conservee, en respectant
la reference JS. Puis verifier le lien nourriture physique -> soulagement de faim.
Ces corrections ne sont pas implementees par la mission d'audit. Aucun push demande.
