# GRANARY_EAT_001 — le grenier : Noûs, la réservation, le stock qui baisse

Branche `agent/granary-eat-001`, base `main@394448c` (après HOUSE_REST_001). Date : 2026-09-29.

Troisième bâtiment, et le premier dont l'**état propre change** : un habitant affamé réserve une
portion au grenier, y marche, y entre, et à la fin du repas `building.stock.food.physical` baisse
de 1 tandis que sa faim tombe.

## La décision qui a cadré la mission

La carte JS a montré que **l'IA algorithmique « Noûs » est active par défaut** dans la référence
(`ai/algorithmic/flags.js:7`, et rien dans `src` n'écrit `sim.flags`). Alexandre a tranché :
porter le comportement par défaut, pas le chemin classique. Conséquences, toutes portées :

| Ce que Noûs change | Référence | Portage |
|---|---|---|
| Cadence de pensée : 2,2 s (0,55 s en besoin critique), décalée par identifiant | `scheduler.js`, `npc.js:877-916` | pour **tous** les buts, puits et maison compris |
| Bascule de phase : force une pensée | `syncVillagePhase`, `npc.js:883` | porté |
| Décision faim à chaque pensée : manger / chercher / acheter / travailler / dormir / fuir / attendre | `hungerUtility.js` | **parité bit à bit** (252 vecteurs) |
| Inertie : garder l'action si la nouvelle n'est pas meilleure de +0,20 | `inertia.js` | **parité bit à bit** (60 vecteurs) |
| Pont : biais sur **toute** la table de buts, porte de commit (force `eat` / `rest` en urgence) | `bridge.js` | porté |
| Réservation d'une portion, TTL 45-180 s, renouvellements, balayage 2,5 s | `mealReservation.js` | porté |
| Action faim : réserver → naviguer → confirmer → consommer | `hungerAction.js` | porté |
| Le grenier doit être **connu** : croyance acquise à 7 tuiles, balayage toutes les 9 s | `ai/memory.js perceiveBeliefs` | porté (branche nourriture) |

La table de décision n'est plus « deux lignes contre un plancher ». Ce sont les 25 lignes
d'`adultScores`, dans l'ordre de la référence, triées de façon stable. `eat`, `rest` et `drink`
sont calculées ; chaque but non porté vaut 42 + son `phaseBias` et reçoit les biais Noûs.

## Ce que la référence fait, et que le portage reproduit tel quel

Trois comportements surprenants, mesurés, reproduits et **testés**. Ce sont des candidats à
corriger côté JS, pas en C++.

1. **Le foyer passe avant le grenier.** Pour `eat`, les couches rythme (`mealPlace`) et
   domestique (`domesticTarget`) envoient chez lui l'habitant qui a un toit ou un abri, même si
   sa portion est réservée au grenier. Il « mange » chez lui sans nourriture (la branche repas
   de `tickNeeds` baisse sa faim de 2,4/s), le grenier ne perd rien, et la réserve est rendue
   quand le but change. Mesuré : faim 70 → 10,75, grenier 5 → 5
   (`Anastasis.Sim.Village.Grenier.FoyerDabord`).
2. **L'inertie garde une décision périmée.** La décision `seek_food` prise à faim 70 (score 0,45)
   reste la décision courante tant qu'aucun candidat ne fait mieux de +0,20. Le pont pousse donc
   `eat` d'environ +62 alors que l'habitant est rassasié. Il remange, une portion réservée à
   chaque fois, jusqu'à ce que la fatigue fasse gagner `rest`. Mesuré : **5 repas d'affilée**,
   faim finale 4,6 (`Grenier.Repas`).
3. **La capacité ne se contrôle pas à l'entrée** (déjà vu pour la maison) : l'entrée au grenier
   dépend de `buildingForIndoorAction` (groupe `food`), pas de la réservation.

Et ce qui marche comme prévu : avec trois affamés et deux portions, `reserveStock` tranche. Le
troisième reçoit `unavailable`, un cooldown de 4 s, le type `seek_food` exclu, et il ne mange pas
(`Grenier.MultiAgents`).

## Fichiers

Créés :

| Fichier | Rôle |
|---|---|
| `Source/AnastasisSim/Public/Ai/AnastasisNous.h`, `Private/Ai/AnastasisNous.cpp` | Noûs pur : urgence, 7 candidats, tri, cooldown, inertie, but, cadence |
| `Source/AnastasisSim/Private/Tests/AnastasisNousTests.cpp` + `AnastasisNousVectors.inl`, `AnastasisNousInertiaVectors.inl` (générés) | `Anastasis.Sim.Parite.Nous` |
| `Source/AnastasisSim/Private/Tests/AnastasisVillageGranaryTests.cpp` | `Anastasis.Sim.Village.Grenier.*` (7 tests) |
| `tools/migration/parity/nous.mjs`, `nous-inertia.mjs` | déclarations de vecteurs |
| `tools/unreal/granary-eat-pie.py` | preuve PIE pilotée par l'état |
| `docs/unreal/GRANARY_EAT_001.md`, `docs/unreal/handoffs/granary-eat-001.md` | ce document, la fiche |

Modifiés :

| Fichier | Changement |
|---|---|
| `Life/AnastasisNeeds.*` | branche intérieure `eat` de `tickNeeds`, `satisfyEat` (parité : 542 vecteurs au total) |
| `Village/AnastasisVillage.*` | grenier et stock, croyances, registre des réservations, action faim, pont Noûs, table complète, cadence Noûs, cible `eat` par couches, intérieur généralisé (rest + eat) |
| `Sim/AnastasisSimulationSubsystem.*` | `Anastasis.Village.FirstGranary`, lecteurs `GetFoodStock` / `CountMealsTaken` |
| `Village/AnastasisVillageBuilding.h`, `AnastasisVillageInteractionSubsystem.*`, `AnastasisVillageTags.*` | `Kind::Granary`, tag `Building.Granary`, emplacement `Activity.Eat` — extension de la fondation de `main` |
| `Village/AnastasisVillagePresentation.cpp`, `AnastasisFirstBuildingTests.cpp` | grenier → acteur ; debug (stock, réservations, pourquoi) ; `GrenierPresentation` |
| `Private/Tests/AnastasisVillageSimTests.cpp`, `AnastasisVillageHouseTests.cpp` | attentes adaptées à la cadence Noûs (2,2 s) ; le seuil « midi » du puits retiré (voir dette) |
| `tools/migration/parity/needs.mjs`, `AGENTS.md`, `Source/AnastasisSim/PORTAGE.md` | vecteurs repas ; index d'outils ; état du portage |

## Flux

```
UpdateNpc
  dedans (eat) : TickNeedsEatInside (-2,4/s) ; à `until` : Perform -> Eat -> RunHungerActionStep
                 -> ConfirmMeal (physique -1, réserve -1, inventaire +1) -> inventaire -1 -> SatisfyEat
  dehors : TickNeeds ; bascule de phase ? pensée ; TickAlgorithmicNpc (renouvellements, balayage 2,5 s)
    pensée (2,2 s) : Perceive (croyances, 9 s) -> ComputeAlgorithmicDecision (Noûs + inertie)
      sans cible : ChooseGoal
        table 25 lignes (+ phaseBias) -> ApplyAlgorithmicScoreBias -> tri stable -> porte de commit
        CommitGoal : OnAlgorithmicGoalCommitted -> ReserveMeal (stock réservé +1)
        EatTarget : source réservée -> mealPlace (rythme) -> foyer / abri ouvert (domestique)
    Act : A* -> arrivée -> TryEnterIndoorAction(eat) -> EnterBuilding (2,1 s)
```

## Écarts déclarés

Ils sont écrits en tête de `AnastasisVillage.h` : l'écart 1 est réécrit, les écarts 5 et 9 sont
nouveaux.

- **(1)** Table complète, mais les buts non portés valent 42 + `phaseBias`. Un but non porté qui
  gagne donne `observer`. Il n'y a ni `goalNoise`, ni `statusBias`, ni collant de but.
- **(9)** Pas d'or ni de marché : `buy_food` ne gagne jamais et `believedStock` vaut 0. Pas de
  danger : `flee` ne gagne jamais. Ne sont pas portés non plus : l'oubli des croyances, la
  mendicité, la mémoire d'échec, l'agrégat `market.stock`, les gisements. Le stock ne porte que
  la nourriture. `marketAccessPoint` retombe sur le camp (le site planifié du marché n'est pas
  porté).
- `RemoveBuilding` supprime tout de suite les réservations visant le bâtiment démoli (la
  référence le fait au renouvellement suivant, raison `source_gone`). `RemoveNpc` rend la
  portion réservée (branche « habitant disparu » de `expireMealReservations`). Les croyances
  survivent à la démolition : un souvenir peut être faux, et Noûs échoue proprement avec
  `no_known_source`.

## Dette découverte

- Les trois comportements JS listés plus haut : foyer d'abord, inertie périmée, capacité non
  contrôlée. Ils sont à corriger dans la référence ; le portage les suivra.
- **Le seuil de soif de midi a changé** depuis que `eat` est une ligne calculée. L'ancien
  plancher surestimait le repas de midi à 42 + 75,6 ; c'est maintenant `relax` qui plafonne à
  midi. L'assertion « midi : 40 » du puits a été retirée, pas maquillée.
- Aucun grenier ne se remplit tout seul. Pas de production, pas de portage (`haulJob`) : le
  stock vient de `CreditFood`, exactement comme dans `verify-algorithmic-npc.mjs`.
- Les croyances ne s'oublient pas (`staleAfterDays` n'est pas porté).
- Sauvegarde : `Digest()` projette le stock, l'inventaire, l'état de l'action faim et le registre
  des réservations. La référence sauvegarde ce registre (`mealReservation.js` 386-438) ; le futur
  format natif devra le faire aussi, sinon la réserve posée sur le stock survivrait à sa
  réservation.
