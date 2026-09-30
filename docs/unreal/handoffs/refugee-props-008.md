# HANDOFF: refugee-props-008

## MISSION

Produire quatre nouveaux assets pertinents d'après les images modèles, en arrêtant
la finition de la rive. Branche agent/refugee-props-008, base ccaeee0.
Worktree : C:/Users/alex_/.codex/worktrees/asset-map-002/ANASTASIS_UNREAL.

Référence principale inspectée : docs/visual/reference/pontique-props-materiel-refugies.png.
Référence de contexte inspectée : docs/visual/reference/pontique-camp-installation-rive.png.
Les modèles reprennent des catégories et des formes visibles dans ces planches ;
ils ne constituent ni une reproduction photoréaliste ni une validation historique.

## FILES_OWNED

- tools/unreal/create-refugee-props.py
- tools/unreal/capture-refugee-props.py
- tools/unreal/capture-refugee-props.ps1
- docs/unreal/handoffs/refugee-props-008.md
- AGENTS.md : index des outils de ce worktree, dont les trois nouvelles entrées.

Les quatre StaticMesh et leur matériau sont effectivement sauvegardés localement
dans Content/Anastasis/RefugeeProps008/. Les cinq packages livrés sont désormais suivis en Git LFS sur mandat explicite
« Commit integrate push ». Le ZIP initial reste une livraison autonome. EcotoneReview004 et tout travail concurrent exclus.

## COMMIT

Voir git log agent/refugee-props-008. Pas de merge/push automatique.
Le lot peut se transporter indépendamment des changements de rive des commits parents.

## LIVRAISON

| Asset | Dimensions approximatives (cm) | Triangles |
|---|---|---:|
| SM_Amphora_Transport_01 | 64 x 44 x 80 | 7 384 |
| SM_Chest_Travel_01 | 120 x 68 x 69 | 3 004 |
| SM_FishTrap_Wicker_01 | 111 x 64 x 85 | 63 800 |
| SM_Tripod_Cauldron_01 | 116 x 114 x 186 | 13 316 |

- Amphore : corps creux, col ouvert, deux anses, lèvres et anneaux de potier.
- Coffre : lattes séparées, couvercle bombé, ferrures, rivets, moraillon et poignées.
- Nasse : corps ajouré, cerclages, montants entrelacés, entrée en entonnoir et corde.
- Cuisson : trois perches liées, chaîne alternée, anse et chaudron ouvert.

Chaque objet est un StaticMesh statique complet, pivot centré au sol, unités cm.
Un matériau PBR partagé utilise couleur/roughness par sommet et grain procédural.
Le seuil de métal distingue les ferrures du bois, de la terre cuite et de l'osier.
Pas de texture externe nécessaire.

Collision : une enveloppe 26-DOP par objet. Elle bouche volontairement les espaces
internes (notamment sous le trépied) : proxy de placement, pas collision d'interaction.
LOD0 uniquement ; nasse particulièrement détaillée, à alléger avant multiplication.
UV0 de projection pour cette matière procédurale, pas de dépliage de lightmap validé.
Coffre non ouvrable, nasse non fonctionnelle, chaîne non simulée. Pas de gameplay ajouté.

## MEC

Géométrie pure vérifiée pour les quatre objets : sommets finis, triangles non
dégénérés, normales unitaires, pivot au sol et orientation gauche Unreal cohérente.
Total : 87 504 triangles. Le nombre n'est pas une mesure de performance.

Premier rendu (v1) REJECT : convention de faces inversée et anses vrillées.
Correction v2 : winding inversé en conservant les normales extérieures, transport
continu du repère des tubes, amplitude du tressage réduite près de la pointe.
La vérification impose une concordance positive normale géométrique/sommets.
Source moteur consultée : GeometryCore/Public/VectorUtil.h, NormalDirection.

Les anciens binaires v1 sont archivés hors dépôt avant reconstruction. La recette
refuse d'écraser un asset étranger ; REBUILD exige le nom et le marqueur Recipe
du lot. Par défaut elle recharge sans reconstruire et vérifie version et triangles.

Aucun changement C++, donc pas de build ni de suite Automation revendiqués.

## SCN / PREUVES

Racine : C:/Users/alex_/.codex/visualizations/2026/09/29/01a0eab3-eaab-7610-8f6d-bb6dd660a518/

- refugee-props-008-render-a : diagnostic incomplet de connexion RGB du matériau.
- refugee-props-008-render-b : neuf vues v1 rejetées ; sources exactes archivées.
- refugee-props-008-v1-rejected.zip : anciens assets conservés.
- refugee-props-008-render-c : v2, sauvegarde des quatre meshes et six vues rapprochées
  obtenues ; la planche initiale grise n'est pas acceptée (shaders en préparation).
  Run interrompu ensuite par épuisement de mémoire/pagefile, explicitement INCOMPLETE.
- refugee-props-008-render-d : relecture des assets sauvegardés, planche finale et
  deux vues du chaudron, sans reconstruction ; voir log/manifeste/empreintes.
- refugee-props-008-validation.json et refugee-props-008.zip : contrôle et livraison.

Le run D termine bien dans Unreal avec COMPLETE shots=3 assets=4 et sortie propre.
Son wrapper signalait ensuite neuf PNG attendus au lieu de trois : comptage corrigé,
puis contrôles de sortie rejoués sans relancer le rendu. Les neuf vues retenues
combinent les six détails de C et les trois captures de D.

Éclairage de studio et vues de détail. Aucun niveau sauvegardé.
Contours de sélection d'éditeur résiduels dans certaines vues rapprochées : ils
n'appartiennent pas aux meshes. Matériaux de première passe, sans patine détaillée.
PLY et budget GPU : UNKNOWN.

## REPRODUCTION

~~~powershell
$env:ANASTASIS_PROPS_REBUILD='0'
tools/unreal/capture-refugee-props.ps1 -OutDir <nouveau-dossier>
~~~

Le premier run crée les assets absents. REBUILD=1 reconstruit uniquement les assets
de cette recette dans RefugeeProps008. -FinishOnly recharge les quatre objets et
ne capture que la planche et les deux vues du chaudron.

Le chemin des packages du ZIP doit rester Content/Anastasis/RefugeeProps008 dans
un projet UE 5.8.2. Pas de branche de rive à intégrer pour utiliser ces objets.

## STOP

Quatre objets livrés comme première passe statique. Arrêt après la livraison ;
pas de nouvelle passe sur la rive ni de nouveau système lancé.

## INTEGRATION_RISK

Mandat utilisateur : Commit integrate push. Intégration sélective depuis f420119
sur main af29954 ; aucune étude de rive parente incluse. Cinq packages neufs,
aucun asset existant remplacé. Branche agent/refugee-props-008-integration.
Le worktree Codex est hors du préfixe accepté par le lanceur projet : compilation
via Build.bat Unreal dans ce worktree, puis gate projet normal sur le canonique.
Commit : BRANCH_HEAD. Les preuves visuelles restent celles du lot produit.

## PLY

NOT_IMPLEMENTED ; aucun gameplay ni placement dans la carte livré.
