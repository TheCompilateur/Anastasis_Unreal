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

`62d823a` — code et preuve enregistrés en branche expérimentale. `finish` non lancé :
**pas de HANDOFF_READY, pas d'entrée dans le lot**.

## MEC

- BUILD: `BUILD::PASS` sur l'état final (`anastasis-unreal.ps1 build`, 4 actions, `Result: Succeeded`).
  Le premier essai avait été invalidé par Source/Config modifiés pendant le build ; il n'est pas compté.
- `python -m py_compile tools/unreal/player-food-loop-pie.py` : code 0.
- `git diff --check` : code 0.
- `Test-AnastasisToolsIndex` : Missing 0, Stale 0.
- Suite Unreal : QUEUED pour le lot.
- `editor-batch.ps1 -Proofs player-food-loop-pie -DryRun` : registre et job valides.
- Essai PIE : `PROOF::FAIL ... jamais demarree`. L'éditeur de ce worktree est resté bloqué à
  `Build.bat -Mode=QueryTargets` pendant des builds concurrents (`crossing-site-001`, puis
  `npc-life-bridge-001`). Je l'ai fermé après contrôle de son chemin ; **aucune étape du script n'a tourné**.
  Ce résultat est une preuve absente, pas un échec comportemental.

## PROOFS

PROOFS: player-food-loop-pie

## SCN

UNKNOWN. Le banc PIE doit montrer les mutations de stock et de faim ; son premier essai n'a pas démarré.

## PLY

UNKNOWN. Le script utilise les commandes des touches F6 à F9 ; il ne prouve ni les touches physiques,
ni la lecture de l'overlay par un humain, ni une scène sociale de jeu complète.

## ECARTS

AUCUN — aucune règle de `Source/AnastasisSim/` n'est modifiée. Les verbes `gatherFood`, `deliver` et `eat`
existent déjà pour les habitants autonomes.

## INTEGRATION_RISK

- Le scénario remplace le village de départ. Son unique habitant ne constitue pas un village viable ;
  F6 est un banc de test, pas une entrée de partie canonique.
- Il réutilise le stock générique `Food` du portage courant. Cela ne démontre pas les économies distinctes
  du grain, de l'huile et du vin décrites par la Bible canonique de gameplay transmise le 2026-10-07.
- `AnastasisSimulationPlayer.cpp` et `DefaultInput.ini` sont partagés avec les missions joueur futures.
- Les touches F6 à F9 sont des `DebugExecBindings` PIE, absents en Shipping.
- Le script PIE nouveau doit être mis au point dans l'éditeur avant le lot ; un build ne prouve pas son verdict.
- La machine enchaînait les builds globaux pendant l'essai ; le lot devra exécuter le script pour la première fois.
- Nouvelle tentative le 2026-10-07 : `editor-batch` est resté à `EDITOR_GATE::WAIT`, derrière deux
  autres éditeurs et sous le seuil RAM. Arrêt de notre attente avant création de notre éditeur ;
  le statut PIE demeure `UNKNOWN`.

## NEXT — décision avant extension

Exécuter d'abord `player-food-loop-pie` sur ce commit. La fiche `PLAYER_MINIMAL_001.md`
fixe ensuite un A/B à état initial identique : attente du joueur contre récolte et dépôt,
avec repas et faim d'un voisin mesurés individuellement. Le voisin doit voir le grenier
mais ignorer la source au départ. La portée actuelle de perception (7 cases) et la distance
source–grenier (~4) rendent un simple second acteur au seuil non discriminant. Ne pas coder
ce scénario tant que la chaîne matérielle de base n'a pas un verdict runtime.

## STOP

Ne revendique pas une boucle de 30 minutes, une obligation, une relation, une économie durable, une
preuve de clavier ni une expérience joueur validée. Un stock déplacé n'est qu'un effet local.
