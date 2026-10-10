# HANDOFF: arrivant-seul-001

## MISSION

Alexandre a joué (2026-10-09) : « mon joueur peut rien faire de lui-même, les touches marchent pas, je sais pas comment
jouer ». Le log de sa partie montre que la marche, les touches 1 à 3 et 8 passaient ; ce qui était mort, c'était la scène
d'entraide : depuis player-start-002 le Play incarne un **arrivant sans famille**, `StartHelpScene` refuse dès qu'un joueur
existe (sans rien dire), et E, F, X, J sont gardés par `bHelpSceneActive`. Le panneau Valmire annonçait pourtant
`[F] Batir  [X] Retirer l'intention  [J] Carnet`.

Choix d'Alexandre (2026-10-09) : **« Je veux rester seul arrivant »**. Donc le joueur n'est pas le chef d'une famille ; la
scène d'entraide n'est pas son début de partie.

Ce que fait la mission, et seulement cela :

- le panneau de l'arrivant seul (scène non ouverte) dit ce qui marche : qui il est, marcher (Z Q S D ou W A S D, souris),
  où lire ses buts (la ligne `BUTS` et la touche de chaque but), le temps (8 le double, 9 le divise, et le village ne le voit
  plus s'il va trop vite) ; **plus aucune touche de la scène d'entraide** ;
- Entrée, pressée par un arrivant seul, répond dans le panneau (« Entree ne fait rien ») au lieu de rester muette ;
- la scène d'entraide elle-même, son panneau et ses touches sont **inchangés** pour qui l'ouvre (départ en observateur,
  `anastasis.Player.AutoArrive 0`) ;
- la décision est écrite dans `PLAYER_START_001.md` et `PLAYER_HELP_SCENE_001.md`.

## FILES_OWNED

- `Source/Anastasis_UnrealV2/Sim/AnastasisPlayerHelpInterface.cpp` : `ArrivalHelpText`, branche « arrivant seul » de `HelpPanelText`,
  retour d'Entrée dans `StartHelpScene`
- `Source/Anastasis_UnrealV2/Sim/AnastasisSimulationSubsystem.h` : déclaration de `ArrivalHelpText`
- `Source/Anastasis_UnrealV2/Sim/AnastasisArrivalHelpTests.cpp` (nouveau) : `Anastasis.Sim.Joueur.ArrivantSeul.Panneau`
- `docs/unreal/PLAYER_START_001.md`, `docs/unreal/PLAYER_HELP_SCENE_001.md`

## COMMIT

Voir `git log main..agent/arrivant-seul-001`.

## MEC

- BUILD: UNKNOWN
- TESTS: UNKNOWN
- COMMANDS:
  - `tools\unreal\report-tests.ps1 -Filter 'Anastasis.Sim.Joueur.ArrivantSeul'`

## PROOFS

PROOFS: (aucune)

## SCN

Au Play : le panneau en bas à gauche dit « Tu es arrivé seul, sans famille », comment marcher, où lire les buts, comment régler
le temps. Aucune touche annoncée n'est morte. Pas d'éditeur avec rendu lancé (EDITOR_QUEUE_001) : le texte est une fonction
pure, jugée par un test, pas par une image.

## PLY

NOT_JUDGED — Alexandre joue. À juger : le texte est-il assez clair pour commencer sans aide ? Y manque-t-il quelque chose ?

## ECARTS

AUCUN — présentation et hôte seulement (`Source/Anastasis_UnrealV2/`), rien dans `Source/AnastasisSim/`.

## INTEGRATION_RISK

- **`mains-joueur-001`** (prête, `proved`) change les touches du joueur : clic gauche maintenu pour agir, 1 à 7 et molette pour la
  barre de 8 cases, clic droit pour utiliser, **Maj + 1 à 5** pour les buts. Le texte d'ici reste juste avec ou sans elle (il
  renvoie à la ligne `BUTS`, qui affichera « Maj + »), mais il ne parle pas des mains. **Suite à prévoir après le versement des
  deux** : une ligne de plus dans `ArrivalHelpText` pour le clic maintenu, la barre et le clic droit. Aucun conflit de fichier :
  `mains-joueur-001` ne touche ni `AnastasisPlayerHelpInterface.cpp` ni le test d'ici ; le `.h` reçoit des ajouts distincts.
- Le panneau ne s'affiche toujours que dans le village des fondateurs (`bStartVillage && !Founders.IsEmpty()`) ou pendant la
  scène : dans un scénario `Anastasis.Village.First*` qui remplace le village, il reste masqué, comme avant.
- E, F, X, J restent sans effet pour l'arrivant seul (ils n'ont pas d'objet : pas de toit de famille à bâtir). Ce n'est pas
  annoncé, donc pas trompeur. Leur donner un usage serait une autre décision de jeu.

## STOP

- Ne donne pas de famille, de chantier ni de rôle à l'arrivant.
- Ne change ni la scène d'entraide, ni la simulation, ni les touches.
- Pas de jugement de lisibilité de l'image : le panneau Slate n'apparaît pas dans les captures `Shot`.
