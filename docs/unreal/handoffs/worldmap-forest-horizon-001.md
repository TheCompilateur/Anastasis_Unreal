# HANDOFF: worldmap-forest-horizon-001

## MISSION

Audit actuel de la World Map et essai A/B d'une continuation forestiere hors du bord
simule. Base `main` ba327baa961918499756a95e660cf20b9f7d1f69 ; worktree
`agent/worldmap-forest-horizon-001`. L'essai a ete **rejete et son code retire**.
Le seul changement livre est ce compte rendu d'audit. Le `download.png` non suivi
dans la racine canonique et les autres worktrees sont exclus.

## ETAT REEL

Autorites : `Config/DefaultEngine.ini`, `.uproject`, `WorldView` C++, fiches terrain,
vegetation et eau ; capture fraiche dans ce worktree sur `Lvl_AnastasisSlice`, seed
12345, profil d'atmosphere du jeu. Les images plus anciennes ne servent pas de
preuve de la carte actuelle.

- Carte par defaut : `Lvl_AnastasisSlice`. Autres maps du projet :
  `Lvl_HumanOccupation` et `Lvl_AAA_VisualLab` (labo). La carte simulee fait
  96 x 96 tuiles, 400 cm par tuile, facteur de presentation 5 : environ 1,9 km
  entre centres extremes. L'anneau de presentation va jusqu'a 60 km.
- Le terrain est `ExperimentalTerrain`, un `UProceduralMeshComponent` : sol section
  0, nappe section 1, rubans de riviere section 2. Pas de Landscape natif, de
  Landscape Layers, de RVT de Landscape, de World Partition ou de HLOD dans le
  pipeline documente. Le streaming effectif et les Data Layers du niveau charge
  ne sont pas verifies par cette mission.
- Relief rendu : interpolation bicubique, Human Geography et erosion thermique ;
  drainage et berges proceduraux. Le simulateur garde la verite semantique ; le
  maillage en est une presentation.
- Vegetation : arbres, strates basses, herbe et rive en HISM transitoires, avec
  LOD ; pas de PCG/Foliage Tool de production. Nanite des arbres est desactive.
- Atmosphere de la capture : soleil 75 000 lux, exposition fixe EV100 14,
  brouillard exponentiel/volumetrique et perspective aerienne 1,20. Config :
  Lumen et VSM actifs. GPU p50 editeur 17,56 a 24,74 ms sur cinq vues
  1920 x 1080 ; aucune preuve de performance jeu package. L'anneau n'a ni
  collision ni navigation.

## CINQ DEFAUTS PRIORITAIRES

1. **Fin de la canopee au bord carre de la simulation.** Visible dans la vue
   aerienne actuelle H4 ; le sol continue, les silhouettes s'arretent.
2. **Avant-pays lointain nu.** Les vues humaines H2/H3 montrent une large surface
   sans structure ecologique apparente entre la carte et les reliefs lointains.
3. **Berges et plans d'eau anguleux.** La vue H4 montre encore plusieurs contours
   a segments lisibles ; tout correctif doit controler l'accord eau rendue/eau
   semantique, et non seulement arrondir l'image.
4. **Masse et silhouette des peuplements.** Les arbres existent, mais les groupes
   et silhouettes restent insuffisants a moyenne distance. D'autres branches
   travaillent deja sur canopee, essences, lisiere et sols.
5. **Occupation humaine peu imprimee dans le territoire.** Le code experimental
   de traces de passage est desactive par defaut ; la branche `lived-paths-001`
   travaille en parallele. Aucun trajet joueur/PNJ n'est deduit des captures.

Ce classement est qualitatif, issu des captures et du code. Ce n'est pas une
mesure d'impact en Play. Les PNG courants sont sous
`Saved/HorizonEvidence/worldmap-forest-reference/` de ce worktree ; les cinq
images ont ete vues.

## EXPERIMENTATION ET DECISION

Une premiere version prolongeait la litiere du bord dans la couleur du sol :
build PASS, H4 seulement 0,43 % de pixels >16/255, H3 0,11 %. Une seconde version
ajoutait des silhouettes via HISM sans collision, conditionnees par le sol forestier
du bord. Build PASS, 23 arbres prevus et 23 places. H3 montre quatre arbres
isoles sur une plaine nue : **effet plus artificiel**, H4 0,20 % et H3 0,31 % de
pixels >16/255 contre la reference. GPU p50 H4 24,74 -> 25,67 ms, H3 17,56 ->
18,11 ms entre runs non simultanes : attribution du delta UNKNOWN. Ces captures
separent bien l'effet visuel, pas le cout precis.

Decision : **REJECT**. Les cinq fichiers C++ de l'essai ont ete restaures exactement
depuis `main` dans ce worktree. Aucun asset, code ou niveau de la carte n'est livre
par cette mission. Ajouter quelques arbres au bord serait de la decoration, pas
une continuite territoriale. Prochaine intervention utile : traiter l'avant-pays
comme une unite geographique complete (relief, bassin et couvert a l'echelle du
kilometre) apres integration des branches actuelles ; la capture H2/H3 est le
temoin a battre, H4 controle la couture en plan.

## FILES_OWNED

- `docs/unreal/handoffs/worldmap-forest-horizon-001.md` uniquement.

## COMMIT

PENDING

## MEC

- Builds d'essai : PASS. Source livree : `main` inchangee.
- `git diff --check` : PASS.
- Tests Unreal/PIE de l'essai : non executes, code rejete.

## PROOFS

PROOFS: (aucune)

## SCN

Reference observee dans l'editeur du worktree. Hypothese de continuation
forestiere REJECT. Captures ciblees completes ; aucune amelioration livree.

## PLY

UNKNOWN : aucune marche joueur ni trajectoire PNJ testee.

## ECARTS

AUCUN : `Source/AnastasisSim` intact.

## INTEGRATION_RISK

Documentation seule. La baseline `main` precede plusieurs branches actives de
sol, eau, canopee et atmosphere ; leurs resultats combines peuvent changer ce
classement. Les captures non suivies restent dans le worktree.

## STOP

Ne pas appeler cette mission une amelioration AAA ni un gain de performance.
Ne pas reintroduire les 23 arbres isoles ou la teinte non discriminante sans
nouvelle hypothese et A/B sur la carte chargee.
