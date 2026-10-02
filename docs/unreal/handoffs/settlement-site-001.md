# HANDOFF: settlement-site-001

## MISSION

Remplacer le choix initial centre + premi√®re case libre par un choix g√©ographique reproductible. Mandat direct : ¬´ Corrige √ßa Et lance prochaine √©tape ¬ª. Cette mission est distincte du lot graphique.

## FILES_OWNED

- Source/Anastasis_UnrealV2/WorldView/AnastasisSettlementSite.h
- Source/Anastasis_UnrealV2/WorldView/AnastasisSettlementSite.cpp
- Source/Anastasis_UnrealV2/WorldView/AnastasisSettlementSiteTests.cpp
- Source/Anastasis_UnrealV2/WorldView/AnastasisSettlementSurvey.h
- Source/Anastasis_UnrealV2/WorldView/AnastasisSettlementSurvey.cpp
- Source/Anastasis_UnrealV2/Sim/AnastasisSimulationSubsystem.h
- Source/Anastasis_UnrealV2/Sim/AnastasisSimulationSubsystem.cpp
- tools/unreal/settlement-site-pie.py
- tools/unreal/proofs.txt (entr√©e settlement-site-pie)
- AGENTS.md (ligne d'index)
- docs/unreal/handoffs/settlement-site-001.md

## COMMIT

HEAD de agent/settlement-site-001 ; base 506f6db49aea640d4f0a8f96373b2e84d8853590. Ne pas inclure implicitement dans le premier assemblage graphique : l'implantation change les conditions de ses captures.

## MEC

- BUILD: PASS, premi√®re compilation 182.76 s, compilation de l'API de preuve 45.17 s ; correction de la marge de placement 175.06 s.
- TESTS: PENDING. Trois tests cibl√©s : GeographicChoice, BarriersAndMissingEvidence, ResourcesDriveChoice. Le script de preuve les ex√©cute dans le m√™me √©diteur avant le PIE, exige les trois fins Success dans la portion courante du log.
- Commande : tools/unreal/editor-batch.ps1 -Proofs settlement-site-pie
- PYTHON_AST: PASS.

## PROOFS

PROOFS: settlement-site-pie

## CHANGEMENT

Le premier tick attend l'incarnation de son propre monde. Le relev√© lit les triangles des sections 0 (sol), 1 (lacs) et 2 (rubans d'eau) du composant ExperimentalTerrain, avec identit√© graine/dimensions et transformation v√©rifi√©es. Aucun cache Forge global ni nouveau relief n'est utilis√©.

Neuf sondes par tuile donnent une pente locale conservatrice et une marge au-dessus de l'eau rendue. La s√©lection croise ces donn√©es avec IsFootBlocked, les types et quantit√©s de ressources existants. Elle exige : site herbe/broussailles hors zone tr√®s humide, pente <=8 degr√©s, marge d'eau rendue >=1 m, marge de deux cellules aux bords conforme ‡ SeedFirstWell, au moins trois sorties cardinales, >=9 cellules contigu√´s d'expansion <=12 degr√©s ; acc√®s √† une rive eau s√©mantique ET rendue <=300 m, champ productif <=600 m et ressource bois <=600 m.

Les distances sont des longueurs de parcours sur un sous-graphe cardinal praticable, filtr√© √† 18 degr√©s, pas des distances √† vol d'oiseau. Elles ne sont PAS le co√ªt exact de l'A* des PNJ. Le score favorise surface disponible (40), eau (25), nourriture (20), bois (15) ; d√©partage par index de tuile. Aucun nouveau tirage al√©atoire.

Ces seuils et poids sont une politique de d√©marrage explicite, pas des constantes historiques ou agronomiques. La marge √† l'eau n'est pas une pr√©vision de crue. Le relev√© ne remplace pas une v√©rification humaine du site.

Le choix passe par SetSettlement d√©j√† existant puis SeedFirstWell. L'ancien sc√©nario de douze habitants autour du puits est conserv√©. Ni type/altitude/fertilit√©/ressource ni pathfinding du simulateur ne sont r√©√©crits. Les sc√©narios explicites annulent l'ouverture en attente ; un village d√©j√† peupl√© n'est pas d√©plac√©.

CVar anastasis.Village.SiteSelection = 1 par d√©faut. 0 conserve l'ancien choix pour comparaison lors d'un NOUVEAU d√©marrage ; ne t√©l√©porte pas le village en cours. Aucun candidat : rapport unavailable, aucun repli silencieux au centre. Terrain manquant : attente born√©e √† 10 s avant rapport unavailable.

## SCN

OBSERV… sur c790e8a, run 20261002-022202. Rapport JSON lu via AnastasisSimulationDebugLibrary.get_settlement_site_status(world) : ancien site, meilleur, top cinq, pentes, surfaces et distances. Le script exige le puits vraiment pos√© aux coordonn√©es retenues, douze PNJ, progression temporelle et d√©placement r√©el ; produit selected_eye.png (1,7 m au sol √©chantillonn√©), selected_oblique.png et legacy_terrain_oblique.png dans Saved/SettlementSiteEvidence/. Le troisi√®me cadre montre l'ancien TERRAIN, pas un faux village avant/apr√®s.

RÈsultat : SETTLEMENT_SITE_PIE PASS, 52.3 s, douze PNJ prÈsents et au moins un dÈplacement observÈ aprËs dix secondes simulÈes. RelevÈ 69.428 ms, 8836 cellules sondÈes, 59 sites Èligibles. Site retenu (74,36) : pente 6.077 degrÈs, surface contiguÎ 9600 m≤, accËs eau 60 m, champ 120 m, accËs bois adjacent (0 m jusquí‡ la cellule de prÈlËvement). Ancien site (47,47) : non Èligible, accËs eau 660 m sur le sous-graphe. Le zÈro de surface de líancien site signifie que le calcul a ÈtÈ ÈcartÈ par les prÈconditions, pas une absence mesurÈe de terrain disponible.

Les trois images ont ÈtÈ regardÈes : puits posÈ dans une ouverture en lisiËre, sol lisible ‡ hauteur humaine, forÍt proche. Des repËres de debug PNJ restent visibles ; ces images sont fonctionnelles et ne constituent pas une validation artistique finale. La vue de líancien site montre son terrain ouvert et la riviËre ; la distance calculÈe est un parcours filtrÈ vers une eau ‡ la fois sÈmantique et rendue, pas sa distance visuelle ‡ toute eau du dÈcor.

Preuves locales non versionnÈes : Saved/EditorBatch/20261002-022202/editor-batch.log et Saved/SettlementSiteEvidence/comparison.json + trois PNG. Le log termine la preuve ‡ 06:23:40, puis relËve un crash de fermeture ‡ 06:23:47 (RequestExitWithStatus 3). Le lanceur affiche EDITOR_BATCH::PASS car il juge les motifs du travail ; cela NE prouve PAS une fermeture propre. PID 4892 absent aprËs le run, crÈneau rendu. StabilitÈ gÈnÈrale : UNKNOWN.

## PLY

UNKNOWN. Aucune fermeture de la boucle dynamique terrain modifi√© -> navigation g√©n√©rale. Le contr√¥le PNJ et les images ne prouvent pas une navigation joueur compl√®te.

## ECARTS

AUCUN ‚Äî Source/AnastasisSim inchang√©. Le choix d'implantation rel√®ve de l'h√¥te Unreal et emploie son API existante ; pas de claim de parit√© ex√©cut√©e.

## INTEGRATION_RISK

Sim/AnastasisSimulationSubsystem.cpp partag√© avec anthropic-landscape-001 (reset) : pr√©server son appel ResetPresentation si un conflit appara√Æt. Main a avanc√© apr√®s la cr√©ation du worktree. Registre/index √† fusionner s√©lectivement. Aucun asset binaire modifi√©.

La politique peut refuser un monde o√π aucun site n'a tous les acc√®s requis. Il faut le rapport r√©el pour ajuster les contraintes, pas affaiblir les tests pour produire un PASS. Temps de s√©lection √† mesurer dans SETTLEMENT_SITE survey_ms.

## STOP

Aucune int√©gration ni push ind√©pendant. Ne pas d√©placer un village existant. Ne pas attribuer au simulateur la pente filtr√©e uniquement pour le choix initial. Pas d'histoire locale invent√©e. La preuve r√©elle et les captures attendent la fen√™tre d'√©diteur coordonn√©e.
