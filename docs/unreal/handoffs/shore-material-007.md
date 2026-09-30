# HANDOFF: shore-material-007

## MISSION

Revue des matières sol/eau sur la rive de shore-contact-006 (643738a).
Worktree C:/Users/alex_/.codex/worktrees/asset-map-002/ANASTASIS_UNREAL,
branche agent/shore-material-007. Géométrie ShoreProfile=1, graine 12345,
21 roseaux conservés, caméras et éclairage des études précédentes.

## FILES_OWNED

- tools/unreal/capture-shore-material.py
- tools/unreal/capture-shore-contact.py (entrée principale protégée pour réutilisation)
- tools/unreal/capture-reed-form.ps1 (sélection du nouveau script)
- docs/unreal/handoffs/shore-material-007.md

Aucun C++, asset binaire ou niveau modifié. EcotoneReview004 reste local, hors
commit. Les variantes de matériau sont des duplications non sauvegardées dans
l'éditeur dédié ; la recette est rejouable, mais aucune variante n'est déployée.

## COMMIT

Voir git log agent/shore-material-007. Quatre fichiers possédés uniquement.
Pas de merge/push automatique ni de seal canonique.

## PATCH ET DÉCISION

KEEP local : réglage fin du sol, sur une copie de MI_AnastasisGround.

| Paramètre | Avant | Candidat |
|---|---|---|
| DetailTiling | 1/70 | 1/12 |
| DetailContrast | 0.26 | 0.14 |
| BumpStrength | 0.16 | 0.07 |
| MesoContrast | 0.30 | 0.12 |
| DampRoughness | 0.38 | 0.26 |
| RoughnessGrain | 0.14 | 0.08 |

Les grandes taches de relief apparent deviennent un grain plus fin ; le pied du
versant se lit mieux. Ce réglage groupé a été comparé au matériau livré, pas chaque
paramètre individuellement. Pas de texture photographique ni de gain GPU revendiqué.

EAU : dérive réelle entre l'asset chargé et sa recette shore-water.py.
Exemples observés dans les deux nœuds Custom :

- ShallowWater chargé : (0.102, 0.361, 0.427) ; recette : (0.072, 0.196, 0.262).
- FoamColor chargé : (0.520, 0.560, 0.555) ; recette : (0.315, 0.335, 0.330).
- Opacité de marge chargée : 0.62 ; recette : 0.88.
- Largeur/force de l'ourlet et fondu de bord divergent aussi.

La copie candidate remplace seulement le code des deux nœuds par les constantes
HLSL de la recette existante, lues par AST sans exécuter son entrée de sauvegarde.
Zéro erreur de compilation retournée. Les deux graphes HLSL exacts, avant/après,
sont dans les manifestes. Aucun réglage alternatif de palette n'a été retenu.

KEEP partiel de cette synchronisation comme candidat de revue : bande humide
plus sombre en vue de contexte. REJECT de la revendication « eau réparée » :
le cyan reste dominant au premier plan. Ne pas déployer la recette aveuglément ;
la dérive explique un écart de configuration, pas à elle seule le défaut visuel.

## MEC

Pas de changement C++ : build et suite Automation non relancés.
Contrôles de capture exécutés dans Unreal :

- paramètres d'instance relus après chaque écriture (tolérance 1e-6) ;
- deux copies candidates créées, sources non sauvegardées ;
- compilation du matériau d'eau : aucune erreur retournée ;
- bindings relus pour huit poses par campagne ;
- empreinte géométrique identique avant/après chaque changement de matériau ;
- caméras vérifiées et comparées au manifeste de composition retenu ;
- six fichiers de matériaux originaux contrôlés par SHA256 après les captures.

L'empreinte Python couvre positions, indices, normales, UV0 et tangentes des
deux sections. Elle n'expose pas les couleurs/UV1 ; ne pas la présenter comme
une empreinte de toutes les données GPU.

Deux démarrages de diagnostic incomplets sont conservés :
capture-a s'arrête sur le faux retour booléen du setter ; capture-b s'arrête sur
la dérive inattendue de la palette. Ce ne sont pas des captures réussies.

Le code moteur UE 5.8.2, MaterialEditingLibrary.cpp:1485-1493, initialise bResult
à false et ne l'actualise jamais dans SetMaterialInstanceScalarParameterValue.
Le script ne prend donc plus ce booléen pour preuve : il vérifie la valeur relue.

## SCN

Huit captures principales 1600x900 inspectées, deux cadrages fixes :
bare=livré, A=sol fin, B=recette d'eau actuelle, C=les deux.
Même rive, mêmes placements, soleil 75000 lux, EV100=14.

Une deuxième campagne de huit vues masque la section d'eau dans B/C.
Ses quatre vues sans nappe ont été inspectées : le cyan du premier plan
persiste. La source TileColor (AnastasisTerrainSurface.cpp) renvoie effectivement
ShallowWater/DeepWater/ChannelWater pour le SOL des tuiles Water.
Observation : le terrain porte ce cyan. La part exacte de terrain émergé versus
immergé dans chaque pixel n'a pas été mesurée.

Le sol fin est une amélioration locale ; eau réaliste, rive entièrement lisible,
scène finale : PARTIAL. Horizon noir, repère orange et ombres végétales granuleuses
persistent. Aucun de ces défauts n'est masqué par un autre cadrage.

## PLY

UNKNOWN. Pas de PIE ni de preuve joueur. Pas de mesure temporelle du scintillement,
ni de coût GPU. PLAYER NOT_IMPLEMENTED dans la base de cette étude.

## PREUVES ET REPRODUCTION

Racine des preuves :
C:/Users/alex_/.codex/visualizations/2026/09/29/01a0eab3-eaab-7610-8f6d-bb6dd660a518/

- shore-material-007-capture-c/ : comparaison principale complète, huit PNG,
  capture.log, manifest.json, capture-hashes.json, script exécuté.
- shore-material-007-water-diagnostic/ : nappe masquée, mêmes pièces.
- shore-material-007-original-assets.json : hashes avant.
- shore-material-007-validation.json : contrôles après et hashes des sources.
- capture-a / capture-b : diagnostics incomplets, logs et scripts exacts conservés.

La légende comparison du manifeste brut de la campagne diagnostique est héritée
de la première campagne ; states, diagnostic_hide_water et les bindings avec
water_visible=false font autorité. Le script final corrige uniquement cette
légende et sa docstring après le rendu ; aucun pixel ou réglage de rendu retouché.

~~~powershell
$env:ANASTASIS_SHORE_PLACEMENTS='<chemin absolu du manifeste shore-composition-005a>'
$env:ANASTASIS_MATERIAL_HIDE_WATER='0' # 1 pour le diagnostic
tools/unreal/capture-reed-form.ps1 -CaptureScript capture-shore-material.py -OutDir <nouveau-dossier>
~~~

Prérequis : les assets de roseaux de reed-form-004 et son manifeste de composition.
La recette sait recréer les roseaux si absents, comme les études précédentes ;
les runs présents les ont rechargés. Le matériau d'eau candidat reste temporaire.

## INTEGRATION_RISK

Validation sur un seul site/graine et deux angles, à l'arrêt.
Le grain fin peut scintiller en mouvement ; ce n'est pas évalué.
La rugosité humide agit sur l'ensemble du champ Wetness de cette instance.
Le look hors de ce site et les autres biomes ne sont pas validés.
La synchronisation des deux nœuds HLSL ne démontre pas l'identité complète de tous
les réglages d'asset avec une régénération intégrale de la recette.
Les travaux concurrents et les binaires locaux sont exclus du commit.

## STOP / NEXT

Lot de revue terminé. Prochaine branche causale : séparer la couleur du fond
sédimentaire de celle de la nappe, avec eau visible/masquée comme contrôle.
Conserver la sémantique et la géométrie d'eau ; ne pas transformer cette correction
de projection en réécriture hydrologique. Intégration artistique distincte.
