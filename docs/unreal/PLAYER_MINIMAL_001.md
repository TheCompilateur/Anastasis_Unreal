# PLAYER_MINIMAL_001 — le joueur minimal, branché sur le village

Mandat d'Alexandre (2026-10-01), après TIME_WARP_001 : « je te donne mandat pour brancher — joueur
minimal oui ». La conséquence à brancher : un joueur qui abuse du temps accéléré devient
progressivement invisible pour les PNJ et perd de la réputation, parce qu'à leurs yeux il ne fait rien.

## Le canon : le joueur est un habitant

La référence JS l'a tranché (`docs/PLAYER_AS_HABITANT.md`, `src/sim/decisionProvider.js`) :

> un habitant supplémentaire dont la source de décision est humaine.

Pas d'avatar à côté du monde : un habitant ordinaire de `actors`, désigné par **une seule vérité**,
`playerPersonId`. Aucun `isPlayer` disséminé. Vide = mode observateur, et alors la simulation est
celle d'avant, **au bit près**. Ce portage reprend ce contrat tel quel.

| Référence | Ici (`FVillage`, `Source/AnastasisSim`) |
|---|---|
| `sim.playerPersonId` | `GetPlayerPersonId()`, `IsPlayer(Npc)` |
| `incarnate(id)` / `release()` | `Incarnate(Id)` / `Release()` |
| `arriveAsPlayer(options)` | `ArriveAsPlayer(X, Y)` : `spawnNpc` à `settlement + (2, 3)`, premier sol libre |
| `decideAsPlayer` sans commande → `PLAYER_IDLE_GOAL` | `UpdatePlayer` : but `idle`, activité `attend`, Noûs ne décide pas |
| `setPlayerMovementInput` / `drivePlayerActor` | `SetPlayerMovementInput` / `DrivePlayer` : glissement par axe, mêmes `IsFootBlocked` que `MoveActor` |
| `updateReputationDaily` | `UpdateReputationDaily` à minuit : `rep += (cible − rep) × 0,4` |

`spawnNpc` ne tire ici aucun aléatoire : le flux joueur séparé de la référence (`_playerRng`) n'a
pas d'objet tant que l'identité, le nom de lignée et le métier tirés au hasard ne sont pas portés.

## La conséquence branchée (EXTENSION)

Ce qui suit n'est pas dans la référence : c'est la demande d'Alexandre, marquée EXTENSION dans le code.

1. **Le témoin** (TIME_WARP_001, hôte Unreal) regarde l'habitant incarné, et lui seul. Une seconde
   simulée à l'accélération M est, pour le village, `1 − 1/M` seconde où le joueur n'a rien fait
   (`Advance` : tout). Il en tire une **présence** (1 → 0) et des **secondes oisives**, et les écrit
   chaque frame sur la personne (`FVillage::ObservePlayer`). Sans joueur, il ne compte rien.
2. **Les habitants le voient moins.** `FVillage::Sees` : une personne de présence p est vue jusqu'à
   p × la portée, plus du tout sous 5 %. Appliqué là où un PNJ remarque quelqu'un :
   `PickSocialCompanion` (avec qui parler), `BondSocialTarget` (vers qui aller), et l'oubli dans la
   recherche des personnes mémorisées (`PickRememberedSeek`, sous 25 %).
3. **Sa réputation baisse.** L'oisiveté est un acte, comme le vol dans la référence : un mérite négatif
   permanent, 4 points par jour oisif (`theftLoss` vaut 14). À chaque minuit, la réputation rattrape
   40 % de l'écart à sa cible `50 − 4 × jours oisifs`. Les autres habitants n'ayant aucun acte porté,
   leur cible est 50 et ils y restent.
4. **On a moins envie de lui parler.** `ReputationAffinity` : `(réputation − 50) × 0,4` dans l'affinité
   de compagnon. À la base, exactement 0.

Pour tout habitant qui n'est pas le joueur (présence 1, réputation 50), chaque règle rend au bit près
ce qu'elle rendait : `D <= Range` est le même test, `x + 0.0` vaut `x`.

Repères (tests et preuve PIE) :

| Le joueur… | Présence | Jours oisifs | Réputation |
|---|---|---|---|
| saute une semaine (`Advance 7d`) | ~3 % : personne ne le voit | 7 | baisse dès les minuits de la semaine |
| …et le mois qui suit, sans plus rien sauter | remonte en jouant | 7 (reste) | se pose sur 22 |
| un mois oisif de plus | — | 37 | 0 (plancher) |

## Côté Unreal

- `Anastasis.Player.Arrive [TileX TileY]`, `Incarnate <npc-N>`, `Release`, `Move <dx> <dy>`, `Status`.
- Le **pawn local suit l'habitant** (`anastasis.Player.Pawn 1`) : sa direction d'entrée (ZQSD/WASD du
  template) devient celle du corps simulé avant les pas ; après les pas, le pawn est posé sur le corps et
  sa marche Unreal est coupée (`MOVE_None`) ; sa carte portrait est cachée. La caméra suit l'habitant,
  jamais un fantôme. `Release` lui rend sa marche.
- Overlay PIE : `JOUEUR npc-N  attend  présence …%  réputation …  oisif … j  vu par N`, en orange sous 50 %.
- `AnastasisSimulationDebugLibrary.get_player_status` (JSON) pour les preuves.

## La main du joueur (player-goals-001)

Port de `choosePlayerGoal` / `decideAsPlayer` (decisionProvider.js). Le joueur pose une **intention**
qui dure jusqu'à ce qu'il la retire ; elle est décidée au même point que celle d'un habitant
(`ChooseGoal`, après le tri, l'éligibilité et les verrous), dans la table que Noûs aurait lue :

| Ce qui arrive | Résultat | Refus affiché |
|---|---|---|
| le but est dans la table et rien ne s'y oppose | il le commet, comme n'importe qui (mêmes cibles, portes, effets) | — |
| le but n'est pas dans la table, ou pas porté (`build` sans chantier, `craft`) | il attend | `hors-table` : impossible ici et maintenant |
| un verrou impose autre chose (orage → `shelterRain`, livraison en cours) | il attend | `verrou` |
| faim ≥ 92, soif ≥ 88 ou énergie ≤ 12 | il attend, **sauf si le but est le remède** | `le-corps-parle` |
| le joueur reprend la direction à la main | l'intention est retirée | — |

L'intention n'est jamais remplacée par un choix de Noûs : un refus fait **attendre**. Noûs ne pense pas pour
l'habitant incarné (ni biais, ni porte de commit). Écart assumé (EXTENSION) : la référence refuse aussi le
remède quand le corps parle, donc un joueur à soif 88 ne pourrait plus jamais boire. Son propre commentaire
dit « le joueur doit choisir le remède » : ici, le remède passe. **Assumé par Alexandre le 2026-10-01** (écart n° 21) : « Il faut pas que le joueur meurt de soif pour une règle absurde ».

En PIE : touches **1 à 5** = les buts de la ligne `BUTS` à l'écran (dans l'ordre de la table), **0** =
retirer l'intention ; console `Anastasis.Player.Goal <but|none>`, `Anastasis.Player.Choose <n>`.
`get_player_status` donne `choice`, `holds`, `yields`, `refusal`, `options`, `drinks`, `meals`.

Réputation : le mérite des bâtiments achevés (`deeds.built × 3`, `STANDING.buildGain`) est porté pour tous.
Entre habitants, la réputation ne change pas l'envie de se parler (le portage reste au bit près) ; seule celle
du joueur compte.

## STOP — ce qui n'est pas fait

- **La parole dirigée** (`T`, `requestPlayerTellResourceSpot`) : non portée.
- Les options affichées sont celles de la dernière décision du joueur (rafraîchies à chaque pensée) :
  aucune lecture d'interface ne recalcule la table, à dessein (piège payé deux fois par la référence).
- La réputation des autres habitants (actes, envie, rivalité, jalousie, vols) : non portée.
- Le mode visuel `PLAYER` du GameMode, et un pawn propre au jeu : le pawn est celui du template.
- Le témoin vit dans l'hôte ; la présence et l'oisiveté sont sur la personne, pas dans l'empreinte
  (`Digest`) ni dans une sauvegarde.

## Banc d'épreuve alimentaire du joueur (player-food-loop-001)

En PIE, **F6** (`Anastasis.Player.FoodLoop`) remplace le village d'ouverture par le scénario fini
`SeedFoodSupply` déjà existant et incarne son unique habitant. Une source générée contient une quantité
finie, le grenier commence vide. **F7** pose `gatherFood`, **F8** pose `deliver`, **F9** pose `eat` :
ce sont trois intentions humaines distinctes. Une action terminée n'enchaîne pas la suivante à la place
du joueur. L'overlay montre les portions au champ, dans le sac et au grenier, le nombre de repas et la
faim. `get_player_status` expose ces mêmes quantités pour la preuve.

`player-food-loop-pie` contrôle la prise, le trajet avec charge, le dépôt, le repas, la baisse de faim
et la conservation `champ + sac + grenier + repas = stock initial` à chaque échantillon. Il pilote les
commandes que F6–F9 déclenchent ; cela reste une preuve runtime, pas un essai manuel des touches ni un
verdict sur la lisibilité de l'image. Ce banc ne constitue pas une boucle de scène complète :
aucun autre humain n'y réagit, aucune obligation ni relation ne pèse sur le choix. Le scénario remplace le village d'ouverture : son unique habitant
et son stock fini ne démontrent pas une économie de village durable. Les touches sont des
`DebugExecBindings` PIE et ne définissent pas les contrôles d'un build Shipping.

Premier run PIE du 2026-10-07 : récolte et livraison passaient, mais `eat` restait sans repas
pendant 90 secondes simulées. La réservation recevait une source vide pour un joueur sans
décision Noûs. Le correctif (écart n°39) reprend la source de ses croyances, puis laisse
`ReserveMeal` vérifier le stock et réserver. Reprise sur le même script : **PASS**, source
initiale 19, sac 2, grenier 2, puis grenier 1 après un repas ; faim 16,37 → 11,30.
Ce verdict porte sur les commandes et l'état runtime du worktree, pas sur les touches ou la
lisibilité visuelle.

### Porte de décision pour la première scène sociale

Le banc précédent vérifie une chaîne de matière. La prochaine unité de gameplay doit être
une **décision avec un autre habitant** : un voisin a faim, voit le grenier vide, et le joueur
peut y apporter une portion. Le voisin décide et mange selon les règles ordinaires du village ;
aucune récompense ni commande spéciale ne lui est attribuée parce que le joueur a livré.

Avant d'ajouter ce voisin, rejouer `player-food-loop-pie`. **KEEP** du banc matériel seulement
si la récolte, le transport, le dépôt, le repas et la conservation passent en PIE ; sinon
corriger ce chemin et ne pas empiler une seconde causalité sur un chemin inconnu.

Pour la scène sociale, comparer deux exécutions depuis la même graine et le même état initial :

| Bras | Intervention unique | Mesure au même temps simulé |
|---|---|---|
| A | Le joueur attend. | Portion disponible, repas et faim du voisin. |
| B | Le joueur cueille et livre au grenier. | Mêmes valeurs, avec l'heure du dépôt, de la réservation et du repas. |

Le voisin doit commencer **hors de perception de la source** mais à portée du grenier. Le
scénario actuel place source et grenier à quatre cases environ, pour une perception de sept :
poser simplement le voisin au seuil ne l'isolerait pas de la source. Sa position et ses
croyances doivent donc être relevées dans les deux bras ; s'il découvre la source dans A,
le test ne peut pas attribuer son repas au joueur. Les deux bras gardent Noûs actif et les
mêmes besoins, météo, chemins et rythme. À chaque échantillon :
`source restante + sacs + stock physique + repas = source initiale`.

**KEEP social** si le voisin ne mange pas dans A, mange après le dépôt dans B, que sa faim
baisse, et que le journal relie sans saut source → sac du joueur → grenier → repas du voisin.
**REJECT** si les deux bras aboutissent au même repas, si le voisin cueille seul, si un stock
est injecté, si la conservation échoue, ou si la seule différence observée est un compteur
global. Un verdict inconnu reste `UNKNOWN`.

Même un KEEP social reste `[MEC]` tant que la scène n'est pas jouée au clavier, à hauteur
d'humain, avec le voisin visible, une intention et sa conséquence compréhensibles sans
console ni JSON. La boucle canonique de trente minutes exige encore une obligation, un
arbitrage de temps et une trace qui persiste au jour suivant.
