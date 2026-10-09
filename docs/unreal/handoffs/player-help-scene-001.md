# HANDOFF: player-help-scene-001

## MISSION

Premier acte social jouable de Valmire : incarner le chef d'une famille devant son chantier,
demander en personne une aide motivee, choisir d'y travailler, lire les reponses et les recits
dans un panneau joueur. Contrat detaille : `docs/unreal/PLAYER_HELP_SCENE_001.md`.

## FILES_OWNED

- `Source/AnastasisSim/Public/Village/AnastasisVillage.h`, `Private/Village/AnastasisVillage.cpp`,
  `Private/Tests/AnastasisEpisodesTests.cpp`, `ECARTS.md` : demande generique, portes et test.
- `Source/Anastasis_UnrealV2/Sim/AnastasisSimulationSubsystem.h/.cpp`,
  `AnastasisPlayerHelpInterface.cpp`, `Anastasis_UnrealV2Character.h/.cpp`,
  `Anastasis_UnrealV2.Build.cs` : panneau Slate, touches, scene et lecture instrumentale.
- `tools/unreal/player-help-scene-pie.py`, `proofs.txt`, `AGENTS.md` : preuve declaree et index.
- `docs/unreal/PLAYER_HELP_SCENE_001.md`, cette fiche.

## COMMIT

`agent/player-help-scene-001` HEAD, marque par `finish` apres commit.

## MEC

- BUILD: `BUILD::PASS` initial (14 actions, apres ajout de `SlateCore`), puis
  `BUILD::PASS` apres rebase sur `3eb8e320` (15 actions, 374,41 s).
- TESTS: `Anastasis.Sim.Episodes.DemandeParlee` — `TESTS::PASS`, 1/1, zero echec,
  run sans rendu de 261 s avant rebase. `finish` sur le code rebase :
  `TESTS::PASS`, 395 PASS, 4 KNOWN_EXPECTED_FAILURE exclus du PASS, 0 FAIL,
  399/399 annonces, run sans rendu de 375 s.
- `HANDOFF_READY::YES (proved)` au premier portail `finish` du code rebase.
- `git diff --check` : code 0.

## PROOFS

PROOFS: player-help-scene-pie

## SCN

`PROOF::PASS player-help-scene-pie` initial (87,4 s, 7/7 controles), puis sur le
code rebase (68,0 s, 7/7) : chef `npc-6` incarne, site `building-4` ouvert,
panneau cree, demande a `npc-13` a 4,47 m, oui motive par `voisin`, aucune piece
et aucune teleportation dans la reponse immediate. Le voisin etait deja a portee ;
cette execution ne prouve pas la marche du joueur. L'accord est une admission au
chantier ; le trajet de l'aidant et ses pieces ulterieurs restent inconnus.

## PLY

UNKNOWN — ni essai manuel au clavier ni jugement de lisibilite du panneau n'ont eu lieu.
Les captures `Shot` ne montrent pas le panneau PIE ; la seconde presente une grande
geometrie vert vif au premier plan. Aucune image ne prouve l'interface affichee au joueur,
et la cause de cette geometrie reste inconnue.

## ECARTS

- modifie : n°48 — un habitant peut adresser `AskHelp` a portee de voix pendant la journee ;
  l'hote ouvre une parcelle familiale sur demande humaine au debut de cette scene. EXTENSION,
  A_TRANCHER ; aucun changement du format de sauvegarde ni du village sans demande.

## INTEGRATION_RISK

- `player-start-001`, `player-walk-001`, `player-survie-001` sont actifs sur la meme machine ;
  rebaser sur leur contenu quand ils atteignent `main`, puis refaire `finish` avant admission.
- `AnastasisSimulationSubsystem.h/.cpp`, `AnastasisVillage.h/.cpp`, `AGENTS.md` et `proofs.txt`
  sont des fichiers chauds ; resoudre les conflits par intention, pas par union pour `AGENTS.md`.
- La preuve PIE commande une demande explicite ; elle ne simule pas une vraie pression de touche.
  Son PASS n'etablira pas `PLY`.

## STOP

Pas de menu principal final, de promesse minutee, de garantie qu'un oui soit tenu, ni de verdict
commercial sur le plaisir de jouer. Pas d'integration ni de push par cette session.
