# Protocole asset V2 — fabriquer, brancher, comparer

## Verdict et périmètre

Un asset n'est livré au jeu que si sa source de fabrication, son `.uasset`, son
consommateur réel et son effet à l'écran sont reliés par des preuves. Une image
IA est une **hypothèse de forme** ; elle ne fournit ni maillage exploitable, ni
UV, ni provenance historique, ni droit d'utilisation par défaut. Un spécialiste
C++/Unreal peut choisir une géométrie procédurale, un import 3D documenté, ou
une bibliothèque sous licence vérifiée. Il inscrit le choix dans le contrat.

Le pilote V2 est `SM_Pontic_Horsetail_02`, candidat procédural agentique. Il
réutilise le placement écologique existant, sans toucher à `FVillage`, au relief
ou aux trois autres assets de microvie. `anastasis.Dressing.PonticHorsetailCandidate`
vaut **0** par défaut. `1` choisit le nouveau mesh aux mêmes instances. Le
contrat et la capture V2 se limitent à ce candidat. La V1 reste archivée pour
tracer ce qui a été corrigé.

## Responsabilités bornées

| Rôle | Livre et s'arrête quand |
|---|---|
| Agent asset Unreal | Source rejouable, `.uasset` dans son worktree, contrat et mesures MEC. N'écrit que ses assets déclarés. |
| Agent C++ consommateur | Route l'asset à un emplacement réel derrière une CVar, garde le témoin et la simulation inchangés. |
| Agent preuve | Exécute la validation et l'A/B/A, rapporte valeurs, images inspectées, KEEP/REJECT/UNKNOWN. Il ne choisit pas la stratégie suivante. |
| Intégrateur désigné | Rejoue les preuves déclarées dans la file unique, puis décide l'admission du commit marqué par `finish`. |

Une personne peut remplir plusieurs rôles sur un ticket étroit ; les fichiers
possédés et le SHA du commit restent explicites dans la fiche de passation.

## Contrat minimal

Copier `contracts/_TEMPLATE_V2.json`. Définir une seule fonction visuelle,
la provenance et les droits de chaque référence, le générateur et son SHA-256,
un budget de triangles **maximum par LOD**, les matériaux, dimensions, centre XY
et contact Z. `stage=library` autorise un asset sans consommateur ni capture :
son verdict reste `MEC` seulement. `stage=scene` impose une référence source
C++ et une preuve de capture enregistrée. Une chaîne présente dans du C++ est
une vérification statique, pas une preuve d'exécution.

## Chaîne de production

1. **Définir KEEP/REJECT avant fabrication.** Préciser le témoin et la variable
   unique, la distance joueur, le plafond géométrique, le seuil GPU et le
   critère visuel. Si l'existant répond déjà au besoin, arrêter.
2. **Fabriquer dans un worktree.** Garder le script ou la source 3D, son origine,
   ses droits et sa version. Le générateur n'écrit que ses assets déclarés.
   Aucun `.uasset` n'est édité comme du texte. Inspecter les fichiers LFS.
3. **Contrôler MEC dans un éditeur neuf.** Lancer
   `tools/unreal/validate-asset-contract.ps1 -Contract <contrat>` sans rendu,
   puis faire rejouer le validateur enregistré au lot. Il
   vérifie le projet, le SHA des générateurs, le mesh sauvé, LOD, budgets,
   matériaux, bounds et référence C++ source. Il ne garantit pas la
   reproductibilité binaire du `.uasset`, la collision ni l'image.
4. **Prouver SCN sur la carte chargée.** Faire A/B/A ancien/candidat/ancien,
   caméra, graine, soleil et placements identiques. La preuve instrumentale
   doit établir qu'un mesh réellement consommé change et que le retour au
   témoin est identique. Regarder toutes les images puis mesurer l'écart
   A/B contre A/A et le GPU aux mêmes poses. Une capture terminée seule ne
   donne pas `SCN PASS`.
5. **Prouver PLY seulement si revendiqué.** Lecture à hauteur joueur, accès et
   collision se jugent en parcours normal. Une capture d'éditeur n'y suffit pas.
6. **Décider.** `KEEP` seulement si critères et preuves passent. `REJECT` si
   l'asset manque, dépasse un plafond, n'est pas discernable au-delà du bruit,
   dégrade l'image ou le GPU. `UNKNOWN` si l'éditeur, la capture ou la mesure
   manque : laisser la CVar à 0. Le spécialiste arrête son ticket après cette
   décision ; l'intégrateur décide l'admission au lot.

## Pilote chiffré

- Source : `tools/unreal/create-pontic-horsetail-v2.py`, seed 20261009,
  matériau existant `M_AnastasisGrass`, maillage de lames et verticilles sans
  image externe importée. La forme botanique reste une hypothèse visuelle.
- MEC : trois LOD, au plus 500/180/70 triangles, largeur/profondeur chacune
  ≤ 60 cm, hauteur ≤ 85 cm, Z minimal dans [-5, 5] cm, centre XY à ≤ 20 cm
  de l'origine. HISM sans collision dans le consommateur. Les triangles sont
  des plafonds ; une valeur exacte relève du rapport d'exécution.
- SCN : `horsetail_old,horsetail_new,horsetail_old2`, vue proche
  `micro_horsetail` et vue de berge `riviere_eye`. Inventaire des placements
  identique, seul le nom du mesh de prêle change. Même map et seed 12345,
  soleil à 11 h. Examiner aussi les autres vues si elles sont capturées.
- KEEP : silhouette de prêle plus lisible dans la vue proche, différence
  localisée sur la prêle et supérieure à la dérive ancien/ancien répété ;
  aucune régression visible de rive ; coût GPU médian de la pose ≤ ancien
  +0,2 ms et hausse ≤ 5 % si ancien ≥ 4 ms. Si les deux mesures GPU anciennes
  divergent elles-mêmes de plus de 0,2 ms, déclarer `UNKNOWN` et refaire la
  mesure sur machine calme. Le seuil vaut pour cette machine et cette scène,
  pas pour un jeu packagé.
- REJECT : un placement ou autre mesh change, retour ancien divergent,
  dépassement MEC/GPU, silhouette moins lisible, ou absence d'écart fiable.
  Si la machine est saturée, le GPU est `UNKNOWN` et la décision reste ouverte.

Le script `create-pontic-horsetail-v2.ps1` fabrique le candidat dans son
worktree sur carte vide, avec `-nullrhi` ; un échec de création dans ce mode
reste un échec à diagnostiquer, jamais un asset présumé sauvé. Puis
`editor-batch.ps1 -Proofs
asset-contract-horsetail-v2,pontic-horsetail-v2-capture` rejoue les deux
preuves. Les images et mesures restent sous `Saved/`, avec leurs chemins et
valeurs dans la fiche de passation. Le lot d'intégration rejoue les preuves
déclarées dans un seul éditeur selon la file partagée.
