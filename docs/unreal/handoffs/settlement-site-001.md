# HANDOFF: settlement-site-001

## MISSION

Remplacer le choix initial centre + première case libre par un choix géographique reproductible. Mandat direct : « Corrige ça Et lance prochaine étape ». Cette mission est distincte du lot graphique.

## FILES_OWNED

- Source/Anastasis_UnrealV2/WorldView/AnastasisSettlementSite.h
- Source/Anastasis_UnrealV2/WorldView/AnastasisSettlementSite.cpp
- Source/Anastasis_UnrealV2/WorldView/AnastasisSettlementSiteTests.cpp
- Source/Anastasis_UnrealV2/WorldView/AnastasisSettlementSurvey.h
- Source/Anastasis_UnrealV2/WorldView/AnastasisSettlementSurvey.cpp
- Source/Anastasis_UnrealV2/Sim/AnastasisSimulationSubsystem.h
- Source/Anastasis_UnrealV2/Sim/AnastasisSimulationSubsystem.cpp
- tools/unreal/settlement-site-pie.py
- tools/unreal/proofs.txt (entrée settlement-site-pie)
- AGENTS.md (ligne d'index)
- docs/unreal/handoffs/settlement-site-001.md

## COMMIT

HEAD de agent/settlement-site-001 ; base 506f6db49aea640d4f0a8f96373b2e84d8853590. Ne pas inclure implicitement dans le premier assemblage graphique : l'implantation change les conditions de ses captures.

## MEC

- BUILD: PASS, première compilation 182.76 s, compilation de l'API de preuve 45.17 s.
- TESTS: PENDING. Trois tests ciblés : GeographicChoice, BarriersAndMissingEvidence, ResourcesDriveChoice. Le script de preuve les exécute dans le même éditeur avant le PIE, exige les trois fins Success dans la portion courante du log.
- Commande : tools/unreal/editor-batch.ps1 -Proofs settlement-site-pie
- PYTHON_AST: PASS.

## PROOFS

PROOFS: settlement-site-pie

## CHANGEMENT

Le premier tick attend l'incarnation de son propre monde. Le relevé lit les triangles des sections 0 (sol), 1 (lacs) et 2 (rubans d'eau) du composant ExperimentalTerrain, avec identité graine/dimensions et transformation vérifiées. Aucun cache Forge global ni nouveau relief n'est utilisé.

Neuf sondes par tuile donnent une pente locale conservatrice et une marge au-dessus de l'eau rendue. La sélection croise ces données avec IsFootBlocked, les types et quantités de ressources existants. Elle exige : site herbe/broussailles hors zone très humide, pente <=8 degrés, marge d'eau rendue >=1 m, au moins trois sorties cardinales, >=9 cellules contiguës d'expansion <=12 degrés ; accès à une rive eau sémantique ET rendue <=300 m, champ productif <=600 m et ressource bois <=600 m.

Les distances sont des longueurs de parcours sur un sous-graphe cardinal praticable, filtré à 18 degrés, pas des distances à vol d'oiseau. Elles ne sont PAS le coût exact de l'A* des PNJ. Le score favorise surface disponible (40), eau (25), nourriture (20), bois (15) ; départage par index de tuile. Aucun nouveau tirage aléatoire.

Ces seuils et poids sont une politique de démarrage explicite, pas des constantes historiques ou agronomiques. La marge à l'eau n'est pas une prévision de crue. Le relevé ne remplace pas une vérification humaine du site.

Le choix passe par SetSettlement déjà existant puis SeedFirstWell. L'ancien scénario de douze habitants autour du puits est conservé. Ni type/altitude/fertilité/ressource ni pathfinding du simulateur ne sont réécrits. Les scénarios explicites annulent l'ouverture en attente ; un village déjà peuplé n'est pas déplacé.

CVar anastasis.Village.SiteSelection = 1 par défaut. 0 conserve l'ancien choix pour comparaison lors d'un NOUVEAU démarrage ; ne téléporte pas le village en cours. Aucun candidat : rapport unavailable, aucun repli silencieux au centre. Terrain manquant : attente bornée à 10 s avant rapport unavailable.

## SCN

PENDING. Rapport JSON lu via AnastasisSimulationDebugLibrary.get_settlement_site_status(world) : ancien site, meilleur, top cinq, pentes, surfaces et distances. Le script exige le puits vraiment posé aux coordonnées retenues, douze PNJ, progression temporelle et déplacement réel ; produit selected_eye.png (1,7 m au sol échantillonné), selected_oblique.png et legacy_terrain_oblique.png dans Saved/SettlementSiteEvidence/. Le troisième cadre montre l'ancien TERRAIN, pas un faux village avant/après.

## PLY

UNKNOWN. Aucune fermeture de la boucle dynamique terrain modifié -> navigation générale. Le contrôle PNJ et les images ne prouvent pas une navigation joueur complète.

## ECARTS

AUCUN — Source/AnastasisSim inchangé. Le choix d'implantation relève de l'hôte Unreal et emploie son API existante ; pas de claim de parité exécutée.

## INTEGRATION_RISK

Sim/AnastasisSimulationSubsystem.cpp partagé avec anthropic-landscape-001 (reset) : préserver son appel ResetPresentation si un conflit apparaît. Main a avancé après la création du worktree. Registre/index à fusionner sélectivement. Aucun asset binaire modifié.

La politique peut refuser un monde où aucun site n'a tous les accès requis. Il faut le rapport réel pour ajuster les contraintes, pas affaiblir les tests pour produire un PASS. Temps de sélection à mesurer dans SETTLEMENT_SITE survey_ms.

## STOP

Aucune intégration ni push indépendant. Ne pas déplacer un village existant. Ne pas attribuer au simulateur la pente filtrée uniquement pour le choix initial. Pas d'histoire locale inventée. La preuve réelle et les captures attendent la fenêtre d'éditeur coordonnée.
