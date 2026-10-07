# HANDOFF: player-food-loop-001

## MISSION

Banc d'épreuve PIE pour une chaîne matérielle pilotée par l'habitant incarné : une source finie,
un grenier vide, puis trois intentions humaines distinctes (`gatherFood`, `deliver`, `eat`). Le banc
mesure les transferts et la conservation. Ce n'est pas encore la boucle de scène sociale du jeu.

## FILES_OWNED

- `Source/Anastasis_UnrealV2/Sim/AnastasisSimulationPlayer.cpp` : scénario opt-in, affichage et statut.
- `Config/DefaultInput.ini` : F6 à F9 en `DebugExecBindings` PIE.
- `tools/unreal/player-food-loop-pie.py` et `tools/unreal/proofs.txt` : preuve de la chaîne.
- `AGENTS.md` : index du nouveau script.
- `docs/unreal/PLAYER_MINIMAL_001.md` et cette fiche : usage et limites.

## COMMIT

`62d823a` — scénario et preuve initiaux. Le correctif du repas et sa fiche d'écart sont
dans le commit final de cette mission (voir `git log agent/player-food-loop-001`).

## MEC

- BUILD: `BUILD::PASS` après le correctif (`anastasis-unreal.ps1 build`, 8 actions,
  `Result: Succeeded`). Le premier essai de la mission avait été invalidé par Source/Config
  modifiés pendant le build ; il n'est pas compté.
- `python -m py_compile tools/unreal/player-food-loop-pie.py` : code 0.
- `git diff --check` : code 0.
- `Test-AnastasisToolsIndex` : Missing 0, Stale 0.
- Suite Unreal : QUEUED pour le lot.
- `editor-batch.ps1 -Proofs player-food-loop-pie -DryRun` : registre et job valides.
- Premier essai PIE : `PROOF::FAIL ... jamais demarree` (éditeur bloqué sur des builds
  concurrents) ; aucune étape du script n'a tourné.
- Premier run effectif : `PROOF::FAIL` dans `Saved/EditorBatch/20261007-132450/`. La récolte
  et la livraison passaient, mais `eat` n'a produit aucun repas en 90 s simulées ; choix tenu
  90 fois, aucun refus, grenier à 2 portions. Cause : réservation sans source pour un joueur
  fraîchement incarné, sans métadonnées de décision Noûs.
- Après correctif : `PROOF::PASS player-food-loop-pie (105.5s)` dans
  `Saved/EditorBatch/20261007-134411/`. Les dix contrôles passent : source initiale 19,
  sac 2, grenier 2, puis grenier 1 après repas ; faim 16,37 → 11,30. La conservation est
  contrôlée à chaque échantillon.

## PROOFS

PROOFS: player-food-loop-pie

## SCN

PASS borné au scénario et aux commandes PIE : mutations de stock, repas et faim observés.
Aucun effet sur un autre habitant n'est encore mesuré.

## PLY

UNKNOWN. Le script utilise les commandes des touches F6 à F9 ; il ne prouve ni les touches physiques,
ni la lecture de l'overlay par un humain, ni une scène sociale de jeu complète.

## ECARTS

Ouvert : **n°39, SUBSTITUT, A_TRANCHER**. Le joueur utilise une source qu'il connaît pour réserver
son repas, car la référence JS tente de lire la source d'une décision Noûs qu'elle ne calcule pas
pour lui. Aucun nouveau verbe : `gatherFood`, `deliver` et `eat` restent ceux des autres habitants.

## INTEGRATION_RISK

- Le scénario remplace le village de départ. Son unique habitant ne constitue pas un village viable ;
  F6 est un banc de test, pas une entrée de partie canonique.
- Il réutilise le stock générique `Food` du portage courant. Cela ne démontre pas les économies distinctes
  du grain, de l'huile et du vin décrites par la Bible canonique de gameplay transmise le 2026-10-07.
- `AnastasisSimulationPlayer.cpp` et `DefaultInput.ini` sont partagés avec les missions joueur futures.
- Les touches F6 à F9 sont des `DebugExecBindings` PIE, absents en Shipping.
- Le lot doit rejouer la preuve sur l'arbre intégré ; le PASS ci-dessus est propre au worktree.
- La divergence n°39 requiert une décision d'Alexandre avant d'être tenue pour règle définitive.
- Nouvelle tentative le 2026-10-07 : `editor-batch` est resté à `EDITOR_GATE::WAIT`, derrière deux
  autres éditeurs et sous le seuil RAM. Arrêt de notre attente avant création de notre éditeur ;
  le statut PIE demeure `UNKNOWN`.

## NEXT — décision avant extension

La preuve matérielle passe dans le worktree. La fiche `PLAYER_MINIMAL_001.md` fixe ensuite
un A/B à état initial identique : attente du joueur contre récolte et dépôt,
avec repas et faim d'un voisin mesurés individuellement. Le voisin doit voir le grenier
mais ignorer la source au départ. La portée actuelle de perception (7 cases) et la distance
source–grenier (~4) rendent un simple second acteur au seuil non discriminant. Ne pas coder
ce scénario après l'intégration du banc matériel, pour garder une seule branche causale ouverte.

## STOP

Ne revendique pas une boucle de 30 minutes, une obligation, une relation, une économie durable, une
preuve de clavier ni une expérience joueur validée. Un stock déplacé n'est qu'un effet local.
