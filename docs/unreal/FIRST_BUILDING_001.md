# FIRST_BUILDING_001 — le premier bâtiment branché au simulateur

Branche `agent/first-building-001`, worktree `C:\dev\ANASTASIS_WORKTREES\first-building-001`,
base `main@bff3b03`. Date : 2026-09-29.

Premier lien **HUMAIN ↔ ESPACE BÂTI** : un habitant du simulateur choisit un puits, y marche,
y boit, et sa soif — une donnée que le simulateur relit pour décider — baisse d'une quantité
prouvée au bit près.

## 1. Carte de l'architecture existante

Lue dans le code, pas déduite des noms.

### Le constat qui commande tout

**Le simulateur de PNJ existe — en JavaScript. Il n'existe pas encore en C++.**

| Où | Ce qui existe |
|---|---|
| `C:\dev\Jeux IV Kingdoms\src` (référence) | le simulateur complet : `sim/npc.js` (6 070 l.), `life/needs.js`, `sim/simulation.js` (8 663 l.), `sim/batiments/catalog.js` (≈ 35 types), `sim/save.js` |
| `Source/AnastasisSim` sur `main` | couches 0-3 du portage : RNG, math, horloge, monde, hydrologie, grille de nav, A*, budget, table d'entités, empreinte. **Aucun acteur, aucun besoin, aucun bâtiment.** |
| `agent/sim-tick-day` (non intégré) | l'hôte `tick(dt)` : temps, jour, `onNewDay` — « sans encore porter acteurs ni économie » |
| `Source/Anastasis_UnrealV2/Village` sur `main` | pompe Smart Object : `AAnastasisVillageBuilding` (SimId, Kind House/Well/Workshop), spawn, requête, claim/release, tâches StateTree dont `MoveToSlot` ne bouge rien |
| `claude/anastasis-village-foundation-28ac68` (non intégré) | seconde fondation Smart Object, sans acteur, 3 057 lignes, ontologie de tags **différente** (`Anastasis.Activity.*` contre `Activity.*` sur `main`) |
| `Variant_Shooter/AI` | PNJ du template shooter, pas un villageois |

Tous les points d'entrée Unreal renvoient la décision à la simulation (« SimId remonte à la
simulation JS — Unreal n'invente pas l'identité »). Personne n'avait encore porté cette simulation.

### Référence JS — les systèmes que la boucle traverse

| Élément | Fichier : lignes | Responsabilité | Dépendances |
|---|---|---|---|
| PNJ | `sim/npc.js:554-743` `createNpc` | ~150 champs : position, 8 besoins, but, `target` (un **point**), `home`/`workplace` (objets), `navigation.destBuildingId` (**identifiant**) | rng, culture, génome |
| Besoins | `life/needs.js:31-107, 208-297` `NEEDS`, `tickNeeds`, `tickVitality` | faim/soif montent, énergie/social/loisir/hygiène baissent, santé drainée par un besoin à la fois | villageRhythm, domestic, moodlets, conditioning |
| Décision | `sim/npc.js:811-906` `updateNpc`, `:949-1261` `scoreGoals`/`adultScores`, `:1854-2168` `commitGoalChoice` | IA d'utilité : ~25 buts scorés, verrous, `npc.goal`, `assignTarget` | needs, économie, métiers, mémoire |
| Cadence | `sim/npc.js:405, 480-487` `NPC_AI`, `aiThinkStagger` | pensée toutes les 0,12 s (0,045 s en crise) ; `act` à chaque tick | LOD logique |
| Cible | `sim/destination.js:56-84` `resolveTarget` ; `life/villageRhythm.js:280-339` `rhythmTarget`, `nearestWell` | couches base → mode de vie → rythme → domestique ; pour `drink`, le puits le plus proche l'emporte | |
| Accès | `sim/simulation.js:5900-5980` `buildingAccessPoint`, `accessPointNear` ; `sim/navGrid.js:293-414` | seuils sur l'anneau 1 orientés vers le camp, choix pénalisé par l'occupation (×3,2) | `urban/intent.js` (façades) |
| Déplacement | `sim/simulation.js:8142-8419` `moveActor`, `nextWaypoint` ; `sim/navService.js` | A* en file budgétée + cache, pas de marche à budget, escalade anti-blocage | crowdNav (portes) |
| Action | `sim/npc.js:3353-3540, 3714` `reachedMoveTarget`, `act`, `perform` | arrivée à 0,75 ; une seconde sur place ; `perform` | |
| Bâtiment | `sim/simulation.js:5678-5733` `addBuilding` | `{id:"building-N", type, x, y, progress, …, accessPoints}` ; case bloquée ; **jamais démoli** | urbanisme, transport, emplois |
| Stocks | `sim/transport/stockLedger.js` | `building.stock[res] = {physical, reserved}` | transport |
| Réservations | `ai/algorithmic/mealReservation.js`, compteur `reserved`, occupation des seuils | repas et stocks ; **les puits n'en ont aucune** | |
| Sauvegarde | `sim/save.js:56-125, 647-735` | `buildings` copiés tels quels, acteurs par `packActor` ; réparation des références au chargement | |
| Tests JS | `tools/verify/verify-thirst*.mjs` | la boucle du puits, déjà testée côté JS | |

### Unreal — ce qui a été réutilisé

| Élément | Fichier | Usage dans cette mission |
|---|---|---|
| Table d'entités | `AnastasisSim/Public/World/AnastasisEntityTable.h` | `actors` et `buildings` (ordre JS, index par identifiant) |
| Grille de nav + A* | `World/AnastasisNavGrid.h`, `World/AnastasisPathfinding.h` | blocage des bâtiments, chemins (déjà prouvés par parité) |
| Math JS | `Core/AnastasisSimMath.h`, `Core/AnastasisJsNumeric.h` | `Clamp`, `Dist`/`JsHypot`, `Imul`, `ToUint32` |
| Empreinte | `Core/AnastasisStateDigest.h` | projection canonique du village |
| Hôte `tick(dt)` | `agent/sim-tick-day@b0f487f` repris par cherry-pick (auteur conservé) | la boucle des acteurs s'y accroche là où la référence l'appelle |
| Bâtiment Unreal | `Village/AnastasisVillageBuilding.h`, `AnastasisVillageInteractionSubsystem::SpawnBuilding` | représentation du puits (`Kind::Well`, Smart Object `Activity.Drink`) |
| Atelier de parité | `tools/migration/parity-kit.mjs` | 165 vecteurs besoins exécutés contre la référence |

## 2. Décision d'intégration : le puits

La référence a déjà tranché, et le code le montre :

| Bâtiment | Ce que la boucle exige dans le JS | Verdict |
|---|---|---|
| **Puits** (`drink`) | besoins + choix du puits le plus proche + seuils + marche + `satisfyDrink`. `drink` n'a **pas** de branche dans `buildingForIndoorAction` : on n'entre pas, on ne réserve pas, on ne stocke pas | **le plus petit cycle complet** |
| Maison (`rest`) | foyer / abri (`resolveHousePurchases`, `assignSheltersDaily`), portes domestiques, `enterBuilding`/`updateInside`/`exitBuilding` | trois systèmes de plus |
| Grenier (`eat`) | registre des repas (438 l.), perception alimentaire, grand livre des stocks, rations du marché | le plus gros |
| Atelier | métiers, sessions d'artisanat, transport | le plus lourd |

Conséquence honnête : **le puits n'a pas d'état propre dans la référence**. L'état que la boucle
modifie, ce sont les besoins de l'habitant (`thirst`, `hygiene`, `morale`, `health`), et c'est
`thirst` que la décision relit au tick suivant. Aucun état de puits n'a été inventé pour faire
joli. L'« état du bâtiment » exposé au debug est l'enregistrement (`id`, case, `progress`,
seuils) et ses usagers **dérivés** (`UsersOf`, jamais stockés).

## 3. Fichiers

### Créés

| Fichier | Rôle |
|---|---|
| `Source/AnastasisSim/Public/Life/AnastasisNeeds.h`, `Private/Life/AnastasisNeeds.cpp` | portage de `needs.js` : `urgeScore`, `needGoalScores` (6 buts), `tickNeeds` hors intérieur + `tickVitality`, `satisfyDrink` |
| `Source/AnastasisSim/Public/Village/AnastasisVillage.h`, `Private/Village/AnastasisVillage.cpp` | `FBuilding`, `FNpc`, `FVillage` : `addBuilding`, seuils, occupation, `nearestWell`, `drinkAccessPoint`, `accessPointNear`, `reachedMoveTarget`, `moveActor`/`nextWaypoint`, `updateNpc`/`act`/`perform` pour `drink` ; extension `RemoveBuilding` ; `Digest` |
| `Source/AnastasisSim/Private/Tests/AnastasisNeedsTests.cpp` + `AnastasisNeedsVectors.inl` (généré) | `Anastasis.Sim.Parite.Besoins` |
| `Source/AnastasisSim/Private/Tests/AnastasisVillageSimTests.cpp` | `Anastasis.Sim.Village.Puits.*` (7 tests) |
| `tools/migration/parity/needs.mjs` | déclaration des vecteurs besoins |
| `Source/Anastasis_UnrealV2/Village/AnastasisVillagePresentation.h/.cpp` | lien sim → acteur par `SimId`, debug dessiné, journal d'état |
| `Source/Anastasis_UnrealV2/Village/AnastasisFirstBuildingTests.cpp` | `Anastasis.Village.FirstBuilding.Presentation` |
| `docs/unreal/FIRST_BUILDING_001.md`, `docs/unreal/handoffs/first-building-001.md` | ce document, la fiche de passation |

### Repris de `agent/sim-tick-day` (cherry-pick de `b0f487f`, auteur Codex)

`Source/AnastasisSim/{Public,Private}/Sim/AnastasisSimulation.*`, `Private/Tests/AnastasisSimulationTests.cpp`,
`Source/Anastasis_UnrealV2/Sim/AnastasisSimulationSubsystem.*`, `Sim/AnastasisSimulationHostTests.cpp`,
`tools/unreal/smoke-pie.py`. Intégrer cette branche intègre aussi ce commit.

### Modifiés

| Fichier | Changement |
|---|---|
| `Sim/AnastasisSimulation.h/.cpp` | l'hôte possède un `FVillage`, le lie au monde dans `Reset`, appelle `UpdateActors` dans `Tick` après le jour — l'ordre de `tick(dt)` ; copie interdite (le village pointe sur le monde) |
| `Sim/AnastasisSimulationSubsystem.h/.cpp` | `SyncVillagePresentation` à chaque tick, `SeedFirstWell`, debug `anastasis.Village.Debug`, commandes `Anastasis.Village.*` |
| `Source/AnastasisSim/PORTAGE.md` | la tranche et ses écarts |

## 4. Flux de simulation

```
UAnastasisSimulationSubsystem::Tick                       (Unreal, PIE)
  FAnastasisSimulation::PumpFrame -> Tick(dt)             (hôte : time, day, onNewDay)
    FVillage::UpdateActors(time, dt)                      `for (npc of actors) updateNpc`
      UpdateNpc
        AnastasisNeeds::TickNeeds                         la soif monte de 0,48/s          [parité bit à bit]
        aiThinkAt échu, pas de cible -> ChooseGoal
          NeedGoalScores(needs, puits, tavernes)                                            [parité bit à bit]
          ligne drink = scores.drink + 6  >  UnportedGoalsFloor (42) ?   -> goal = "drink"
          DrinkTarget -> NearestWell -> BuildingAccessPoint
            PickBuildingAccessPoint (seuil le plus proche, occupation ×3,2)
            npc.DestBuildingId = "building-N"             référence PAR IDENTIFIANT
      Act
        pas arrivé -> MoveActor -> NextWaypoint -> AnastasisPath::FindPath (A* porté)
        arrivé (≤ 0,75) -> WorkTimer 1 s ; TickNeeds branche « boit » (−9,5/s au puits)
        Perform -> SatisfyDrink : thirst −62, hygiene +6, morale +2, health +4             [parité bit à bit]
        target = null -> nouvelle décision à la pensée suivante -> « observer »
  FAnastasisVillagePresentation::Sync                     building-N -> AAnastasisVillageBuilding(SimId)
  DrawDebug                                               puits, seuils, habitants, cibles, soif
```

`NPC → updateNpc → ChooseGoal(drink) → puits building-N (seuil) → A* + marche → Perform → npc.thirst`.

## 5. Résultats des tests

Run complet `report-tests.ps1 -Filter Anastasis`, base `main@19e0709` : **106 exécutés,
102 PASS, 4 KNOWN_EXPECTED_FAILURE (le registre), 0 FAIL.**

| Exigence de la mission | Test | Valeur mesurée |
|---|---|---|
| bâtiment enregistré dans la simulation | `Anastasis.Sim.Village.Puits.Enregistrement` | `building-0`, case bloquée, 4 seuils sur l'anneau 1, porte vers le camp, inachevé non compté |
| habitant capable de le sélectionner | `…Puits.Selection` | but `drink`, le plus proche de deux puits, cible = un seuil ; déclenchement exactement à soif 40 |
| habitant capable de l'atteindre | `…Puits.AtteinteEtEffet` | contourne un mur d'eau par A*, arrive au seuil, jamais sur une case bloquée |
| interaction modifiant l'état réel | `…Puits.AtteinteEtEffet` | tick de l'acte = `tickNeeds` + `satisfyDrink` **au bit près** ; soif 62,04 -> 0 ; il revient boire quand elle remonte |
| destruction sans référence morte | `…Puits.Destruction`, `Village.FirstBuilding.Presentation` | aucun `DestBuildingId`, cible ni chemin restant ; case libérée ; acteur et Smart Object détruits ; identifiant jamais recyclé |
| bâtiment inaccessible | `…Puits.Inaccessible` | emmuré : boit depuis la case libre la plus proche ; hors d'atteinte : abandon, ne traverse jamais l'eau |
| plusieurs agents, état partagé cohérent | `…Puits.MultiAgents` | 6 habitants -> 4 seuils distincts ; un retiré en plein puisage ; `UsersOf` exact à chaque tick ; enregistrement intact ; empreinte déterministe |
| l'hôte fait vivre la boucle | `…Puits.Hote` | monde canonique 12345, `tick(dt)` fait boire l'habitant |
| parité des besoins | `Anastasis.Sim.Parite.Besoins` | 165 vecteurs, 0 écart |

Preuve en scène : `tools/unreal/first-building-pie.py` (PIE, `Lvl_AnastasisSlice`) — voir la
fiche de passation, section `SCN`.

## 6. Écarts déclarés et dette architecturale

### Écarts de la tranche (écrits aussi en tête de `AnastasisVillage.h`)

1. **Table de décision réduite.** Seul `drink` a sa boucle. Les ~24 autres buts sont remplacés
   par un plancher déclaré, `UnportedGoalsFloor = 42` : `drink` part exactement quand la soif
   atteint `thirstUrge` (40). Ce n'est pas une valeur de la référence. Sans `goalNoise` : un flux
   rng partiel serait faux plus sournoisement qu'un flux absent.
   **Remplacé par HOUSE_REST_001** : le rythme du jour est porté et s'applique à toutes les lignes.
   Chaque but non porté vaut désormais 42 + son `phaseBias`. Le seuil de soif vaut 40 la nuit, à
   l'aube et à midi, environ 64 le matin.
2. **Pas de reconsidération aléatoire** en route (`sim.rng() < chance`).
3. **Seuils sans intention urbaine** (`urban/intent.js`, vague 5) : l'anneau 1, repli exact de la référence.
4. **Pilotage réduit** : ni file de porte, ni hésitation, ni facteur de vitesse, ni contournement
   local ; escalade anti-blocage en 3 paliers.
5. **Pas de cadence LOD** : chaque habitant est « proche ».
6. **`RemoveBuilding` est une extension** : la référence ne démolit jamais. Garanties : case
   rendue au terrain, version de nav incrémentée, aucun habitant ne garde l'identifiant, la
   cible, ni le chemin.
7. Hors de la table : croyances (`bestKnownWater`, `perceive`), génome et conditionnement
   (multiplicateurs à 1), `markDrink` (gestuelle), branches intérieures de `tickNeeds`.

**Parité** : prouvée pour les fonctions de besoins (165 vecteurs). La boucle assemblée est
déterministe (même entrée → même empreinte), elle n'est **pas** la trajectoire JS — l'écart 1
suffit à l'interdire. Le harnais différentiel ne peut pas encore la juger.

### Dette découverte, laissée hors périmètre

- **Deux fondations Village concurrentes.** `main/Village` (acteur + `USmartObjectComponent`,
  tags `Activity.*`) et `claude/anastasis-village-foundation-28ac68` (sans acteur, tags
  `Anastasis.Activity.*`, 3 057 lignes, non intégrée). Cette mission utilise celle de `main`.
  Intégrer l'autre créerait une seconde façon de poser un bâtiment : c'est à trancher par
  l'intégrateur.
- **Les slots Smart Object ne sont pas les seuils de la simulation.** `main` pose le slot du
  puits à `(80, 0, 0)` uu du centre — une valeur en uu absolus qui ne suit pas `TileWorldSize`
  (100 → 400 sur le `main` local de terrain-relief-001), et qui ignore les 4 seuils calculés
  par la simulation. Rien dans la boucle ne lit ces slots ; ils devraient être dérivés des
  `AccessPoints` le jour où un agent Unreal s'en servira.
- **Pas d'habitant incarné.** Les habitants sont dessinés en debug, pas des pawns. Les incarner
  (mesh, animation) est un chantier de présentation distinct ; `PLAYER` reste NOT_IMPLEMENTED.
- **Z = altitude de tuile**, pas la surface rendue (TerrainForge bicubique) : le puits peut
  flotter ou s'enfoncer de quelques dizaines d'uu.
- **Boucle d'échec sans mémoire.** Un puits hors d'atteinte est réessayé à chaque pensée :
  la référence l'évite par `knownBlockedTargets` (croyances, non portées).
- **Tâches StateTree de `main`** (`FAnastasisMoveToSlotTask` n'avance personne) : non utilisées
  ici, toujours creuses.
- **Persistance** : la référence sauvegarde `buildings` et `actors` (`save.js`). Unreal n'a pas
  encore de format (`P2_MODELE_DONNEES.md` : natif, « à écrire quand il y aura un état à
  sauver »). Il y en a un maintenant : `FVillage::Digest` en donne déjà la projection canonique
  (bâtiments puis acteurs, dans l'ordre). La contrainte pour le futur format : références
  **par identifiant** (`DestBuildingId`), compteurs `NextBuildingId`/`NextNpcId` à sauver pour
  ne jamais recycler un identifiant, `AccessPoints` persistés comme dans la référence.
- `anastasis.Sim.Speed` vaut 10 par défaut (hérité de sim-tick-day) : en PIE, le village vit
  dix fois plus vite que la référence.
