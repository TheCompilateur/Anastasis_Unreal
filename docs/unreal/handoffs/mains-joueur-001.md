# HANDOFF: mains-joueur-001

## MISSION

Demande d'Alexandre (2026-10-09), en jouant : « on a donné son corps, mais pas la capacité de ses mains ni de sa bouche ;
le joueur est prisonnier de son corps ». Choix d'Alexandre : **les gestes du corps d'abord** (la parole plus tard), et
**avant** la règle « quand on meurt de soif, on boit d'abord ». Modèle donné : Medieval Dynasty (« je suis face à un arbre,
je maintiens la souris pour le couper ») et Minecraft (« 8 cases, mon inventaire »), « simplifier la complexité, autant pour
le joueur humain que pour le joueur IA ».

Ce que fait la mission :

- **viser** : ce que regarde la caméra, un bâtiment de la simulation (trace contre son acteur) ou la tuile sous le point visé ;
- **maintenir le clic gauche** : une barre se remplit (1 à 2,5 s selon le geste), le geste se fait, puis se refait tant qu'on
  tient. Message à l'écran : « Maintenir clic gauche : couper du bois », « … : approchez-vous », ou le refus (« le sac est
  plein », « ce chantier n'est pas le vôtre : demandez à la famille ») ;
- **gestes** (simulation, écart n°57) : boire au puits, récolter une tuile de nourriture, couper une tuile de bois, tailler
  une pierre, déposer sa nourriture au grenier ou y prendre une portion libre, donner au chantier le bois et la pierre qui
  manquent, poser une pièce (admis du chantier seulement), manger une portion du sac ;
- **barre de 8 cases** en bas de l'écran : le sac du corps (1 Nourriture, 2 Bois, 3 Pierre, 4 à 8 libres), touches 1 à 7 et
  molette ; **clic droit** = utiliser la case (la nourriture se mange) ;
- les gestes de l'IA et du joueur sont **les mêmes verbes** : tuiles, grenier et chantiers de la simulation ; rien ne se
  crée ni ne se perd.

Touches : 8 et 9 restent au temps (demande d'Alexandre, TIME_WARP_001), la 8e case se prend à la molette. Les buts de
débogage de player-goals-001 passent de 1–5 / 0 à **Maj + 1–5 / Maj + 0** (la ligne BUTS le dit). E, Entrée, F, X, J
(scène d'entraide) inchangés.

## FILES_OWNED

- `Source/AnastasisSim/Private/Village/AnastasisVillageHands.cpp` (nouveau) : `PlayerHandOffer`, `PlayerHandDo`,
  `PlayerHandLabel`, `PlayerStone`
- `Source/AnastasisSim/Public/Village/AnastasisVillage.h` : `EPlayerHand`, `FPlayerHandResult`, `PlayerHandReach` (1,6 tuile),
  `PlayerBagCap` (10)
- `Source/AnastasisSim/Private/Tests/AnastasisPlayerHandsTests.cpp` (nouveau) : `Anastasis.Sim.Joueur.Mains`
- `Source/AnastasisSim/ECARTS.md` : fiche n° 57
- `Source/Anastasis_UnrealV2/Sim/AnastasisPlayerHands.cpp` (nouveau) : visée, maintien, barre Slate, statut JSON,
  `GetPlayerHandTargets`, commandes `Anastasis.Player.Use`, `Anastasis.Player.Hold 1|0`, `Anastasis.Player.EatFromBag`
- `Source/Anastasis_UnrealV2/Sim/AnastasisSimulationSubsystem.{h,cpp}` : membres des mains, appel par image
- `Source/Anastasis_UnrealV2/Sim/AnastasisSimulationPlayer.cpp` : ligne BUTS « (Maj +) »
- `Source/Anastasis_UnrealV2/Anastasis_UnrealV2Character.{h,cpp}` : clic gauche/droit, molette, 1 à 7
- `Config/DefaultInput.ini` : buts sur Maj + chiffre
- `tools/unreal/player-hands-pie.py` (nouveau), `tools/unreal/proofs.txt` (`player-hands-pie`), `AGENTS.md` (index)

## COMMIT

Voir `git log main..agent/mains-joueur-001`, base `main` = `16623526e`.

## MEC

- BUILD: PASS (worktree)
- TESTS: `tools\unreal\report-tests.ps1 -Filter 'Anastasis.Sim.Joueur'` → PASS 8 / KNOWN_EXPECTED_FAILURE 0 / FAIL 0 ;
  `Anastasis.Sim.Joueur.Mains` : boire, récolter jusqu'à épuisement, couper, tailler, trop loin refusé, déposer et prendre
  au grenier (conservation), manger, donner au chantier (ce que le sac perd, le chantier le gagne), poser une pièce
  (`pieces=0->1`). Rebasée sur `main` après player-start-002 (l'arrivée au Play) : suite complète jouée par `finish`.
- `check-ecarts` : `ECARTS::PASS fiches=51` ; `check-state-fields` : `STATE_FIELDS::PASS structures=46 lacunes=2` (aucun champ
  nouveau : le sac est celui du corps) ; index `tools/unreal/` à jour.
- Format de sauvegarde **inchangé**.
- Pas d'éditeur avec rendu lancé (EDITOR_QUEUE_001) : `player-hands-pie` est écrite pour le lot, jamais jouée.

## PROOFS

PROOFS: player-hands-pie, player-pie, player-food-loop-pie

## SCN

Dans le jeu, une fois incarné (`Anastasis.Player.Arrive`, ou l'arrivée automatique quand elle sera versée) : la barre de 8
cases apparaît en bas. Devant le puits, « Maintenir clic gauche : boire » ; on tient, la barre se remplit, on boit. Devant un
champ, on récolte ; devant un arbre, on coupe ; au grenier, on dépose ou on prend ; au chantier, on donne et on bâtit. Clic
droit sur la case Nourriture : on mange. L'arbre coupé, le grenier rempli, la pièce posée le sont pour tout le village.

## PLY

NOT_JUDGED — Alexandre joue. À juger : durées de maintien, portée (1,6 tuile = environ 30 m du centre au monde de la
tranche : peut-être trop loin à l'œil), lisibilité de la barre.

## ECARTS

- n° 57 — nouvelle, OUVERT, A_TRANCHER, EXTENSION : les mains du joueur, des gestes directs aux verbes des habitants.
  Marques `ecart n°57` dans `AnastasisVillageHands.cpp`, `AnastasisVillage.h`, hôte `AnastasisPlayerHands.cpp`.

## INTEGRATION_RISK

- **Touches** : `Config/DefaultInput.ini` déplace les buts de 1–5 vers Maj + 1–5. `player-pie` et les autres preuves passent
  par la console (`Anastasis.Player.Choose`, `Goal`), pas par les touches : non touchées.
- **`movement-audit-001`** (autre agent, relais de player-walk-001) touche `AnastasisSimulationPlayer.cpp` et
  `AnastasisSimulationSubsystem.{h,cpp}` : conflit textuel possible (fonctions différentes). Les deux se complètent : avec la
  marche à la main, on va jusqu'à l'arbre ; sans elle, les gestes marchent aussi (la portée se juge sur le corps simulé).
- **Numéro d'écart** : 56 ici. Le n° 55 est pris **deux fois** par d'autres missions (`ma-cabane-001` / `dormir-couche-001`
  « La cabane du joueur », et `movement-audit-001` « La main d'Unreal pose le corps du joueur ») : à renuméroter au lot.
- `player-start-002` (autre agent) : l'arrivée automatique ; sans conflit attendu sur ces fichiers, à vérifier.

## STOP

- Pas de parole (E sur un habitant reste la demande d'aide de player-help-scene-001).
- Pas de dormir à la main, pas d'outils, pas de fabrication : à venir si Alexandre le veut.
- Ne change ni la vitesse ni les touches de déplacement du gabarit.
