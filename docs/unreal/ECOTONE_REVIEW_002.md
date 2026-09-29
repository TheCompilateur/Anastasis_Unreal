# Registre corrigé et rendu Ecotone — 2026-09-28

## Résultat

Correction de reconstruction livrée sur `agent/asset-map-002`, commit `592a35a`.
Rendu obtenu dans le worktree Ecotone existant, propre, sans fusion et sans
sauvegarde de niveau ou d'asset. Aucun changement C++.

**Décision artistique proposée : conserver la logique de placement comme base
d'évaluation ; ne pas accepter ces meshes comme rendu final pontique.**
La couche est réellement visible, mais ses formes restent des prototypes.

## Correction du registre

La création/reconstruction de `DA_AnastasisPresentation` réutilise désormais
les huit variantes et matériaux de `set_tree_grammar.py`, ainsi que le binding
RUIN de `set_presentation_meshes.py`. Les imports ne déclenchent plus de mutation.

- Un registre existant est inspecté sans écriture par défaut.
- Une reconstruction explicite remplace ses entrées sans supprimer le package.
- Les dépendances sont chargées avant toute modification.
- Échec de chargement, création ou sauvegarde : erreur explicite.
- Les tests vérifient aussi que les entrées existantes sont conservées après un
  préflight incomplet ou rétablies en mémoire si la sauvegarde échoue.
- Le fallback C++ reste inchangé : huit arbres et cylindre de secours pour Ruin.
  Il n'est pas la recette de reconstruction complète.

Validation : `python -B tools/unreal/tests/test_presentation_registry.py` :
**10 PASS, 0 KNOWN_EXPECTED_FAILURE, 0 FAIL** sur API éditeur simulée.
Cette suite vérifie les contrats, pas la sérialisation Unreal.

Puis, dans Unreal 5.8.2 :
`REGISTRY_RECIPE_RUNTIME::PASS forest=8 ruin=1 no_asset_saved=1`.
La nouvelle recette construit les structures et charge les dépendances réelles.
Le registre sauvegardé d'Ecotone a aussi été chargé dans un processus neuf :
Forest=8, Ruin=`SM_Ruin_Generic_01`. Il était déjà correctement branché.

**Limite** : aucune reconstruction sauvegardée de ce registre n'a été effectuée ;
le cycle écriture/relecture du nouveau script n'a pas été validé dans le moteur.
Le registre existant et les références artistiques sont préservés.

## Projet réellement photographié

- Racine : `C:/dev/ANASTASIS_WORKTREES/ecotone-forge-001`.
- HEAD : `fb683983c22c381bf00390d312634b419f585e24`, propre avant/après.
- Build opérateur : `BUILD::PASS::CACHED` (empreintes source et modules inchangées).
- Moteur : UE 5.8.2 / CL 56702186.
- Niveau : `/Game/Anastasis/Maps/Lvl_AnastasisSlice`.
- Monde : seed 12345, 96×96, terrain historique de cette branche.
- Ce n'est ni `main`, ni le terrain courant de Claude.
- Mode : scène éditeur, pas preuve de gameplay PLAYER.
- Un seul processus de capture à la fois ; fermeture normale des deux processus.

Le script `capture-ecotone-review.py` est une copie corrigée de l'ancien outil :
la fonction de neutralisation avait disparu alors que son appel subsistait.
La première passe a donné neuf images, mais la caméra forêt était dans le relief.
Ces images sont conservées comme diagnostic, pas acceptées comme preuve forêt.

La seconde passe échantillonne les triangles du terrain réellement rendu
(381×381 sommets), place chaque caméra 165 uu au-dessus du sol et filtre les
trajets vers la cible traversant le terrain. Trois caméras identiques entre A/B.
Cela ne garantit pas l'absence d'occlusion par arbres ou ruines.

Paramètres commandés : lumière directionnelle 75 000 lux, exposition min/max 14,
bloom/vignette/motion blur à zéro, brouillard exponentiel retiré, atmosphère
désactivée. Orientation solaire héritée de la même scène pour toutes les vues ;
aucune nouvelle heure simulée définie. A/B change seulement la CVar Ecotone
et reconstruit l'incarnation. Résolution : 1600×900.

## Preuves brutes

Dossiers hors Git :
- Première passe : `C:/Users/alex_/.codex/visualizations/2026/09/29/01a0eab3-eaab-7610-8f6d-bb6dd660a518/ecotone-review-001`.
- Paires rapprochées retenues : `C:/Users/alex_/.codex/visualizations/2026/09/29/01a0eab3-eaab-7610-8f6d-bb6dd660a518/ecotone-review-002`.

Le second dossier contient :
`capture.log`, `manifest.json` (chemins et bounds des 12 meshes, positions,
rotations et hauteurs des caméras), `A_forest_eye.png`, `B_forest_eye.png`,
`A_shore_eye.png`, `B_shore_eye.png`, `A_rock_eye.png`, `B_rock_eye.png`.
A=Ecotone OFF, B=ON. Aucun PNG commité.

Observation répétée dans le log :
`placed=2271 planned=2271 companions=348 understory=202 edge=229 shore=1092 rock=748 missing_meshes=0 ungrounded=0`.
Le compteur global d'ancrage mentionne séparément 14 refus : ne pas l'assimiler
au `ungrounded=0` propre à la couche Ecotone.
Une ancre valide ne démontre pas que toute l'emprise d'un mesh repose sur le sol.

## Lecture visuelle

| Vue | Ce que l'image permet de conclure |
|---|---|
| Planche des 12 meshes (passe 1) | Objets présents et différenciables, mais géométrie simplifiée. Aucun transfert photoréaliste des références. |
| Forêt A/B (passe 2) | Caméra hors terrain ; arbres/ruines et masses proches dominent encore le cadre. Apport du sous-bois peu lisible ; jugement de composition PARTIAL. |
| Rive A/B (passe 2) | La couche ajoute visiblement des groupes de tiges/roseaux. Silhouettes rigides, répétition et contact avec un relief très accidenté ; résultat loin d'une rive pontique crédible. |
| Pierres A/B (passe 2) | Blocs pâles/cubiques, groupes de tiges et branches se lisent comme objets ajoutés sur une surface pauvre en détail. Placement opérationnel, intégration matérielle insuffisante. |

[MEC] chargement/placement : observés sur cette ref ; aucune nouvelle suite
C++ lancée. [SCN] images obtenues, qualité PARTIAL ; cible artistique proposée
REJECT pour ce lot tel quel. [PLY] UNKNOWN / PLAYER NOT_IMPLEMENTED.
Performance GPU non mesurée.

## Suite bornée

Ne pas ajouter une autre bibliothèque par défaut. Après fixation du terrain de
Claude, tester une seule famille très lisible — les roseaux — sur une petite
rive, avec une référence précise et un A/B contrôlé. Il faudra juger silhouette,
matière et ancrage de l'emprise, pas seulement le nombre d'instances.

Pas de merge automatique : le correctif Python et l'évaluation Ecotone sont
deux lots séparés. Les assets Ecotone et le canonique n'ont pas été modifiés.
