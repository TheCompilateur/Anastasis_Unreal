# Registre des écarts — module `AnastasisSim`

**Ce fichier fait autorité.** Tout comportement C++ de `Source/AnastasisSim/` qui n'est pas la copie
fidèle de la référence JS (`docs/migration/phase3/REFERENCE_JS.md`) a ici une fiche, **dès le commit
qui l'introduit**. Le protocole, ses raisons et ses portails : `docs/migration/PROTOCOLE_ECARTS.md`.

Contrôle :

```bash
node tools/migration/check-ecarts.mjs                      # registre + marques du code
node tools/migration/check-ecarts.mjs -bilan               # où en est la stabilisation
node tools/migration/check-ecarts.mjs -section actors      # écarts ouverts qui touchent une section du harnais
```

Le détail historique des n° 1 à 18 reste dans le bloc `ECARTS DECLARES` de
`Public/Village/AnastasisVillage.h` ; une fiche ici le résume et le classe. Pour un écart nouveau, la
fiche **est** le détail.

## Format d'une fiche

Titre `### n° N — titre`, puis une liste de champs `- **cle** : valeur`, puis 2 à 6 lignes de texte.
Les numéros ne se réutilisent jamais ; un écart fermé garde sa fiche (`statut : FERME`).

| Champ | Valeurs | Requis |
|---|---|---|
| `classe` | `REDUIT` le C++ fait moins (branche, cas, fonction absente) · `SUBSTITUT` le C++ fait **autre chose** à la place · `EXTENSION` n'existe pas dans la référence · `REFERENCE` note sur la référence elle-même, pas sur le C++ | toujours |
| `destin` | `A_FERMER` provisoire, une mission le fermera · `A_TRANCHER` personne n'a décidé · `ASSUME` divergence voulue et définitive : la référence JS cesse de faire foi sur ce point | toujours |
| `statut` | `OUVERT` · `FERME` | toujours |
| `entree` | mission (et commit si connu) qui l'a introduit | toujours |
| `reference` | fichiers / fonctions JS concernés, ou `aucune` | toujours |
| `cpp` | où vit l'écart dans le C++ | toujours |
| `harnais` | sections de `serialize` qu'il fait diverger (`actors`, `rng`, `buildings`…), ou `aucune` | toujours |
| `masques` | masques de scénario qui le couvrent (`tools/migration/scenarios/masks.mjs`) | si un masque existe |
| `fermeture` | mission(s) qui le fermeront (`P3_PLAN.md`) ; `à attribuer` est permis mais signalé | si `A_FERMER` |
| `decision` | `AAAA-MM-JJ Alexandre — <où c'est écrit>` | si `ASSUME` |
| `activation` | comment il s'active ; un scénario du harnais ne doit jamais l'activer | si `EXTENSION` |
| `ferme_par` | mission et commit qui l'ont fermé | si `FERME` |
| `detail` | document ou bloc de code qui en dit plus | non |
| `jugement` | dossier du laboratoire (`docs/migration/ecarts/nNN.md`) et ses deux verdicts par terrain : **au bit** (`IDENTIQUE` / `DIVERGE`) et **statistique** (`NEUTRE` / `DERIVE` / `RUPTURE` / `INDETERMINE` / `DORMANT`). Une mesure dans la référence JS, pas une décision : il ne change pas le `destin` | non |

**Seul Alexandre fait passer un écart à `ASSUME`.** Un agent écrit `A_TRANCHER` et pose la question.

Dans le code, l'endroit de l'écart porte la marque `ecart n°N` en commentaire. Le contrôleur refuse
une marque vers un numéro sans fiche, ou vers une fiche `FERME`.

---

## Écarts

### n° 1 — Table de décision réduite : plancher 42, et `observer` pour un but non porté

- **classe** : SUBSTITUT
- **destin** : A_FERMER
- **fermeture** : jalon B — goal-noise-001, goals-body-001, goals-resources-001, economy-gold-001, goals-work-001, goals-haul-001, goals-family-001
- **statut** : OUVERT
- **entree** : tranches puits → grenier (first-building-001, house-rest-001, granary-eat-001)
- **reference** : `src/sim/npc.js` `adultScores`, `goalNoise`, `statusBias`, mode de vie, prévision de survie
- **cpp** : `Village/AnastasisVillage.cpp`, table adulte, `UnportedGoalsFloor`
- **harnais** : actors, rng
- **detail** : `Public/Village/AnastasisVillage.h`, bloc ECARTS DECLARES, n° 1

16 des 25 lignes d'`adultScores` valent `UnportedGoalsFloor` (42) + leur vrai `phaseBias` ; un but
non porté qui gagne donne `observer`. Sans `goalNoise`, la décision ne consomme pas `sim.rng` : tant
que cet écart reste, la trajectoire JS est interdite.

### n° 2 — Ni reconsidération aléatoire, ni collant de but

- **classe** : REDUIT
- **destin** : A_FERMER
- **fermeture** : goal-noise-001
- **statut** : OUVERT
- **entree** : tranches puits → grenier (first-building-001, house-rest-001, granary-eat-001)
- **reference** : `src/sim/npc.js` reconsidération (`sim.rng() < chance`), `goalStickinessBonus`
- **cpp** : `Village/AnastasisVillage.cpp`, reconsidération seulement sans cible
- **harnais** : actors, rng
- **detail** : `Public/Village/AnastasisVillage.h`, n° 2

Un habitant qui a une cible la garde jusqu'à l'arrivée, l'échec ou la disparition ; il redécide dès
qu'il n'en a plus. La référence tire au sort une reconsidération à chaque pensée.

### n° 3 — Points d'accès sans intention urbaine

- **classe** : REDUIT
- **destin** : A_FERMER
- **fermeture** : urban-001
- **statut** : OUVERT
- **entree** : tranches puits → grenier (first-building-001, house-rest-001, granary-eat-001)
- **reference** : `src/sim/urban/intent.js`, `urbanIntentAccessCandidates`
- **cpp** : `Village/AnastasisVillage.cpp`, liste vide de candidats urbains
- **harnais** : actors
- **masques** : urbanIntent
- **detail** : `Public/Village/AnastasisVillage.h`, n° 3

L'anneau 1 oriente vers le camp, qui est exactement le repli de la référence quand l'intention
urbaine ne propose rien.

### n° 4 — Pilotage réduit

- **classe** : REDUIT
- **destin** : A_FERMER
- **fermeture** : nav-service-001
- **statut** : OUVERT
- **entree** : tranches puits → grenier (first-building-001, house-rest-001, granary-eat-001)
- **reference** : `src/sim/crowdNav.js`, `moveActor`, `separateCrowdedActors`, `movementSpeedFactor` hors pluie
- **cpp** : `Village/AnastasisVillage.cpp`, pilotage et anti-blocage en trois paliers
- **harnais** : actors
- **masques** : separationFoule
- **detail** : `Public/Village/AnastasisVillage.h`, n° 4

Pas de file de porte, pas d'hésitation, pas de facteur de vitesse (sauf le bloc pluie, n° 17), pas de
contournement local, pas de verrou de seuil domestique en route.

### n° 5 — Pas de cadence de simulation par bande sans vue posée

- **classe** : REDUIT
- **destin** : A_FERMER
- **fermeture** : budget-cadence-001 (cb44bdf) a fermé le cas avec vue ; reste, sans vue posée : à attribuer
- **statut** : OUVERT
- **entree** : tranches puits → grenier (first-building-001, house-rest-001, granary-eat-001) ; réduit par budget-cadence-001 (cb44bdf)
- **reference** : `src/sim/simulationBudget.js` `consumeNpcSimulationCadence`, `src/sim/logicalLod.js`
- **cpp** : `Village/AnastasisVillage.cpp`, tête d'`UpdateNpc` : `Core/AnastasisSimBudget.h` branché dès que `SetSimulationView` pose une vue ; sans vue, chaque habitant pense à chaque tick
- **harnais** : actors
- **masques** : lodLogique
- **detail** : `docs/migration/phase3/P3_PREMIER_RAPPORT.md` (rapports 1 et 2) ; `Public/Village/AnastasisVillage.h`, n° 5
- **jugement** : `docs/migration/ecarts/n05.md` (2026-10-01, labo-ecarts-001, mesuré AVANT budget-cadence-001, sur l'écart entier) — au bit `DIVERGE` tick 1 ; statistique `RUPTURE` contre la référence du harnais (vue 0,0 : bande far), indiscernable du bruit A/A contre la référence vue du village (`endurance` N=40, `genese` N=30, 3 j)

**Premier tick divergent du premier rapport JS / Unreal (tick 1).** En JS, un habitant en bande
*far* pense à 1 Hz avec un `dt` accumulé (`_simBudgetAccum`) ; le C++ d'alors faisait penser chaque
habitant à chaque tick. Le masque `lodLogique` neutralise le LOD logique, pas la cadence du budget.
Depuis budget-cadence-001 (décision d'Alexandre, option B), le scénario `endurance` (format 2) épingle
la vue du budget sur le village des deux côtés, et le C++ applique la cadence. Le reste de l'écart est
le cas **sans vue posée** (tests d'assemblage, jeu sans caméra branchée), où la référence a toujours une
vue, (0, 0) sans caméra.

### n° 6 — `RemoveBuilding` : la démolition

- **classe** : EXTENSION
- **destin** : A_TRANCHER
- **activation** : hôte seulement — `RemoveBuilding` (consoles `Anastasis.Village.*`, preuves PIE) ; aucun scénario du harnais ne l'appelle
- **statut** : OUVERT
- **entree** : tranches puits → grenier (first-building-001, house-rest-001, granary-eat-001)
- **reference** : aucune — la référence ne démolit jamais
- **cpp** : `Village/AnastasisVillage.cpp`, `RemoveBuilding`
- **harnais** : aucune
- **detail** : `Public/Village/AnastasisVillage.h`, n° 6

À trancher : la démolition est-elle une fonction du jeu (alors `ASSUME`) ou un outil de preuve ?

### n° 7 — Foyer minimal

- **classe** : REDUIT
- **destin** : A_FERMER
- **fermeture** : goals-family-001 (famille), day-critical-001 (achats et agrandissements), economy-gold-001 (or)
- **statut** : OUVERT
- **entree** : house-rest-001
- **reference** : `src/life/domestic.js` `assignHomeToHousehold`, `assignSheltersDaily`, `redirectDomesticDoorFailure`
- **cpp** : `Village/AnastasisVillage.cpp`, foyer et abris de minuit
- **harnais** : actors, buildings
- **detail** : `Public/Village/AnastasisVillage.h`, n° 7

Foyer sans famille (le propriétaire seul), capacité 3, ni achat, ni agrandissement, ni hospitalité,
ni dortoir. `redirectDomesticDoorFailure` bascule sur `explore`, non porté : ici `observer`.

### n° 8 — Adultes sans métier de garde, sans famille, sans mode de vie

- **classe** : REDUIT
- **destin** : A_FERMER
- **fermeture** : goals-family-001, puis jalon D
- **statut** : OUVERT
- **entree** : tranches puits → grenier (first-building-001, house-rest-001, granary-eat-001)
- **reference** : `createNpc` (âge, famille, mode de vie, `ensureGenome`, `ensureConditioning`), `jobPriority`
- **cpp** : `Village/AnastasisVillage.cpp`, création des habitants ; `FNpc::Phenotype` / `FNpc::Conditioning` / `FNpc::Lifestyle` non posés
- **harnais** : actors
- **detail** : `Public/Village/AnastasisVillage.h`, n° 8

`jobPriority(rest)` = 15,5 et `jobPriority(eat)` = 18 sont repris en constantes : vrais pour tous les
métiers du catalogue. Le rendu (`Anastasis_UnrealV2/Village`) n'a donc pas d'enfants à dessiner.
Depuis needs-wiring-001, les besoins lisent le phénotype et le conditionnement de l'habitant ; un
habitant créé par le C++ n'a ni génome ni conditionnement, il reste médian (facteurs 1, conditionnement
immobile). Le harnais n'en souffre pas : ses habitants sont lus, `deserialize` les complète.
Depuis lifestyle-wiring-001, un habitant qui a un mode de vie le tient jour après jour
(`lifestyleDailyUpdate`, après la cadence) ; un habitant créé par le C++ n'en a pas, et l'`ensureLifestyle`
de tête d'`updateNpc`, qui le tirerait dans `sim.rng`, n'est pas branché. Les autres lectures du mode de
vie (`lifestyleBias` dans la table, `lifestyleTravelFactor`, `lifestyleIndoorDuration`, `lifestyleTarget`,
`lifestyleNotePlaceUse`) attendent la décision et `placeMemory` (n° 1).

### n° 9 — Noûs partiel

- **classe** : REDUIT
- **destin** : A_FERMER
- **fermeture** : economy-gold-001 (or, marché, `buy_food`, `believedStock`, `market.stock`), goal-noise-001 (`noteGoalFailure`), à attribuer : danger, oubli des croyances, mendicité, gisements
- **statut** : OUVERT
- **entree** : granary-eat-001
- **reference** : `src/ai/*` (`buy_food`, `flee`, `staleAfterDays`, `begForFood`, `noteGoalFailure`, `spots`)
- **cpp** : `Ai/AnastasisNous.*`, `Village/AnastasisVillage.cpp`
- **harnais** : actors
- **detail** : `Public/Village/AnastasisVillage.h`, n° 9

Le stock ne porte que la nourriture. Comportement de la référence **reproduit** : un habitant qui a un
toit va manger chez lui « à vide », même si sa réservation est au grenier.

### n° 10 — Deux métiers seulement, une nature moyenne

- **classe** : REDUIT
- **destin** : A_FERMER
- **fermeture** : goals-work-001 (marché de l'emploi), goals-resources-001 (cueillette des sans-métier)
- **statut** : OUVERT
- **entree** : gather-deliver-001 (9e2057c)
- **reference** : `metiers/*`, `createNpc` (ambition, plan, technique, nature, trait, compétences)
- **cpp** : `Village/AnastasisVillage.cpp`, `AssignWorkplace`
- **harnais** : actors
- **detail** : `Public/Village/AnastasisVillage.h`, n° 10

`settler` et `farmer` ; `gatherFood` et `deliver` ne sont calculées que pour le fermier d'un grenier
achevé. Nature moyenne, trait « gardien », compétences à 1 : des options légales de `createNpc`, pas
des tirages.

### n° 11 — Récolte sans aléa ni exploration ; monde généré immuable

- **classe** : REDUIT
- **destin** : A_FERMER
- **fermeture** : goals-resources-001 (`rollCraftMiss`, `exploreTarget`)
- **statut** : OUVERT
- **entree** : gather-deliver-001 (9e2057c)
- **reference** : `src/sim/npc.js`, `craftWork.js` (`rollCraftMiss`, `exploreTarget`), écriture dans `sim.tiles`
- **cpp** : `Work/AnastasisGather.*`, `Village/AnastasisVillage.cpp` (`LiveTileAt`)
- **harnais** : actors, rng, tileDiff
- **detail** : `Public/Village/AnastasisVillage.h`, n° 11

Pas de raté de coup, pas d'exploration sans gisement connu (il vaque). Le village tient l'état vivant
des tuiles touchées au lieu d'écrire dans le monde : mêmes lectures, mêmes valeurs. **Le bloc d'en-tête
dit « pas de repousse » : c'est périmé**, la repousse est portée depuis (field-regrow-001,
`RegrowFieldsDaily`).

### n° 12 — Livraison à son dépôt seulement

- **classe** : REDUIT
- **destin** : A_FERMER
- **fermeture** : economy-gold-001
- **statut** : OUVERT
- **entree** : gather-deliver-001 (9e2057c)
- **reference** : `deliver` (marché, vente), `sim.market.stock.food`
- **cpp** : `Village/AnastasisVillage.cpp`
- **harnais** : actors, buildings
- **detail** : `Public/Village/AnastasisVillage.h`, n° 12

Sans poste, la référence livre au marché ou vend : ici il vaque. Le stock de marché est la somme des
stocks physiques, recalculée à la lecture.

### n° 13 — La copie de travail JS d'Alexandre diffère de la référence épinglée

- **classe** : REFERENCE
- **destin** : A_TRANCHER
- **statut** : OUVERT
- **entree** : gather-deliver-001 (9e2057c) ; précisé par js-ref-pin-001
- **reference** : `src/sim/npc.js` de la copie de travail : `shouldHaulGatherLoad` `load > 11` (épinglé : `> 9`), `deliver` `maxBatch : 8` (épinglé : `3`)
- **cpp** : aucun — le C++ suit le commit épinglé
- **harnais** : aucune
- **detail** : `docs/migration/phase3/REFERENCE_JS.md`

À trancher par Alexandre : ces deux valeurs non commitées sont-elles une intention de design ? Si oui,
elles entrent par un nouvel épinglage (procédure de `REFERENCE_JS.md`), jamais en copiant la copie.

### n° 14 — Cohabitation du fermier et de l'extension food-supply

- **classe** : EXTENSION
- **destin** : A_TRANCHER
- **activation** : n'existe que si l'hôte active une source (`ActivateFoodSource`, n° 19)
- **statut** : OUVERT
- **entree** : gather-deliver-001 (9e2057c)
- **reference** : aucune
- **cpp** : `Village/AnastasisVillage.cpp`
- **harnais** : aucune
- **detail** : `Public/Village/AnastasisVillage.h`, n° 14

Food-supply ne s'applique qu'aux habitants qui ne sont pas le fermier d'un grenier ; une tuile ouverte
par `ActivateFoodSource` n'a qu'une vérité, son registre fini. Suit le destin du n° 19.

### n° 15 — Socialiser : la cible est le puits, sans place ni marché

- **classe** : SUBSTITUT
- **destin** : A_FERMER
- **fermeture** : urban-001 (la place), goals-body-001 (`hearthInviteScore`), economy-gold-001 (le marché)
- **statut** : OUVERT
- **entree** : social-relax-001 (f43a2cd)
- **reference** : `socialize()`, `socialPos`, `hearthInviteScore`
- **cpp** : `Village/AnastasisVillage.cpp`
- **harnais** : actors
- **detail** : `Public/Village/AnastasisVillage.h`, n° 15 ; `docs/unreal/SOCIAL_RELAX_001.md`

Sans puits, la référence vise la place (routes) puis le marché ; ici, le point d'accès près de
l'origine. La scène de foyer vaut 0.

### n° 16 — Liens et rumeurs : un flux aléatoire propre au village

- **classe** : SUBSTITUT
- **destin** : A_FERMER
- **fermeture** : goal-noise-001 (un seul flux `sim.rng`), goals-family-001 (`updateConflictsDaily`), jalon D (générateur de répliques, `shareRumors` complet, rencontres)
- **statut** : OUVERT
- **entree** : bonds-rumors-001 (13936e4)
- **reference** : `bonds.js`, `talk.js`, `socialMemory.js`, `shareRumors`, `runSocialEncounters`
- **cpp** : `Village/AnastasisVillage.cpp` ; `VillageRng` dans `Public/Village/AnastasisVillage.h`
- **harnais** : actors, rng
- **detail** : `Public/Village/AnastasisVillage.h`, n° 16 ; `docs/unreal/BONDS_RUMORS_001.md`
- **jugement** : `docs/migration/ecarts/n16.md` (2026-10-01, labo-ecarts-001), substitution de flux seule — au bit `DIVERGE` au premier tirage détourné ; statistique `INDETERMINE`, même profil que la calibration A/A : chaotique (`INF`), `NEUTRE` non démontrable à N=40

Les tirages des rumeurs viennent de `VillageRng`, pas de `sim.rng` : la trajectoire JS n'est pas
promise au tirage près. Rumeurs de gisements seulement ; texte des répliques non porté (le refus est
évalué au premier tirage). Dans le harnais, la section `rng` rend l'état lu, figé.

### n° 17 — Météo des habitants : ce qui n'est pas porté

- **classe** : REDUIT
- **destin** : A_FERMER
- **fermeture** : goals-work-001 (`craft` après l'abri) ; à attribuer : `bestKnownBed`, la taverne, le biais de la prévision de survie
- **statut** : OUVERT
- **entree** : village-weather-001 (932d1cc)
- **reference** : `weatherGoalBias.js`, `npc.js` (`shelterRain`, `bestKnownBed`), taverne
- **cpp** : `Life/AnastasisWeatherBehavior.*`, `Village/AnastasisVillage.cpp`
- **harnais** : actors
- **detail** : `Public/Village/AnastasisVillage.h`, n° 17 ; `docs/unreal/VILLAGE_WEATHER_001.md`

Les formules sont en parité (`Parite.MeteoHabitants`). Le foyer tient lieu de `bestKnownBed` ; après
l'abri, un but repris non exposé devient `craft` dans la référence, `observer` ici.

### n° 18 — Chantier ouvert par l'hôte, sans livraisons

- **classe** : SUBSTITUT
- **destin** : A_FERMER
- **fermeture** : build-002
- **statut** : OUVERT
- **entree** : build-001 (08aa017)
- **reference** : `tryOpenNewConstruction`, `requestSiteDeliveries`, `simulation.js` (chantier)
- **cpp** : `Village/AnastasisVillage.cpp` (`OpenSite`), `Work/AnastasisBuild.*`
- **harnais** : actors, buildings
- **detail** : `Public/Village/AnastasisVillage.h`, n° 18 ; `docs/unreal/BUILD_001.md`

C'est l'hôte qui ouvre un chantier (`OpenSite`), pas un choix collectif des habitants ; un chantier à
sec attend, le bois et la pierre ne se récoltent pas encore.

### n° 19 — Circuit vivrier food-supply : une extension opt-in

- **classe** : EXTENSION
- **destin** : A_TRANCHER
- **activation** : hôte seulement — `ActivateFoodSource` ; aucun scénario du harnais ne l'appelle
- **statut** : OUVERT
- **entree** : food-supply-001 ; numéroté par ecarts-protocole-001 (il n'avait pas de numéro)
- **reference** : aucune — « extension déterministe, PAS un portage de trajectoire JS »
- **cpp** : `Village/AnastasisVillage.cpp` (`ActivateFoodSource`, `CreditFood`)
- **harnais** : aucune
- **detail** : `Public/Village/AnastasisVillage.h`, bloc FOOD SUPPLY

Registre fini lu sur une tuile `Food` générée, collecte et livraison à scores bornés, sans repousse.
À trancher : fonction du jeu (`ASSUME`), ou banc d'essai à retirer quand la cueillette des sans-métier
sera portée (goals-resources-001) ?

### n° 20 — Le joueur est un habitant : incarnation et main portées, sans flux joueur ni parole dirigée

- **classe** : REDUIT
- **destin** : A_TRANCHER
- **statut** : OUVERT
- **entree** : player-minimal-001 (incarnation, attente, marche directe), player-goals-001 (choix de but, refus)
- **reference** : `src/sim/simulation.js` (`incarnate`, `release`, `arriveAsPlayer`, `setPlayerMovementInput`, `drivePlayerActor`, `choosePlayerGoal`, `playerGoalOptions`), `src/sim/decisionProvider.js`
- **cpp** : `Village/AnastasisVillagePlayer.cpp` ; seams dans `Village/AnastasisVillage.cpp` (`UpdateNpc`, `ChooseGoal`)
- **harnais** : aucune
- **detail** : `docs/unreal/PLAYER_MINIMAL_001.md`

Aucun scénario du harnais n'incarne de joueur, et `player*` est hors périmètre projeté. Porté : `playerPersonId` seule vérité, attente `idle`, marche directe, intention qui dure et cède avec
`hors-table` / `verrou` / `le-corps-parle`, Noûs muet pour l'incarné. Non porté : `_playerRng` (ici
`spawnNpc` ne tire rien, il n'y a donc pas de flux à isoler), la parole dirigée (`playerTalkIntent`,
`requestPlayerTellResourceSpot`). `ArriveAsPlayer` cherche le premier sol libre en anneaux, là où la
référence pose sans vérifier. Les options affichées sont celles de la dernière décision, sans relecture.

### n° 21 — Le remède passe quand le corps parle

- **classe** : EXTENSION
- **destin** : A_TRANCHER
- **activation** : joueur incarné dont l'intention est `drink` (soif ≥ 88), `eat` (faim ≥ 92) ou `rest` (énergie ≤ 12) ; aucun scénario du harnais n'incarne de joueur
- **statut** : OUVERT
- **entree** : player-goals-001
- **reference** : `src/sim/decisionProvider.js` `decideAsPlayer` (`bodyOverrides` testé avant la table)
- **cpp** : `Village/AnastasisVillagePlayer.cpp` `DecideAsPlayer`, `IsRemedyFor`
- **harnais** : aucune

La référence cède à TOUTE intention quand le corps parle, remède compris : un joueur à soif 88 ne
pourrait plus jamais boire et finirait par mourir. Son propre commentaire dit « le joueur doit choisir
le remède ». À trancher par Alexandre : `ASSUME` (le jeu garde ce comportement) ou corriger la référence.

### n° 22 — Présence, oisiveté et réputation du joueur (temps accéléré)

- **classe** : EXTENSION
- **destin** : A_TRANCHER
- **activation** : hôte seulement — `FVillage::ObservePlayer`, appelé par le témoin TIME_WARP_001 pour un joueur incarné ; sans joueur, présence 1 et réputation 50 rendent chaque règle au bit près ; aucun scénario du harnais ne l'appelle
- **statut** : OUVERT
- **entree** : player-minimal-001 (sans fiche à l'époque) ; numéroté par player-goals-001
- **reference** : aucune — demande d'Alexandre du 2026-10-01 (`docs/unreal/TIME_WARP_001.md`)
- **cpp** : `Village/AnastasisVillagePlayer.cpp` (`ObservePlayer`, `Sees`, `ReputationAffinity`, oisiveté dans `UpdateReputationDaily`) ; `Village/AnastasisVillage.cpp` (`PickSocialCompanion`, `BondSocialTarget`, `PickRememberedSeekFor`)
- **harnais** : aucune

`Presence`, `IdleSeconds` et `Reputation` sont hors empreinte. Un joueur qui accélère le temps ne fait rien aux yeux du village : il est vu jusqu'à présence × portée
(plus du tout sous 5 %), oublié sous 25 %, et ses jours oisifs sont un mérite négatif permanent de sa
réputation, qui pèse sur l'envie de lui parler (le joueur seulement).

### n° 23 — Réputation : seul le mérite des bâtiments est porté

- **classe** : REDUIT
- **destin** : A_TRANCHER
- **statut** : OUVERT
- **entree** : player-minimal-001 (structure), player-goals-001 (`deeds.built`)
- **reference** : `src/life/standing.js` `updateReputationDaily` (ambitions, jalons, conseils d'aîné, vols, plancher des aînés)
- **cpp** : `Village/AnastasisVillagePlayer.cpp` `UpdateReputationDaily`, appelé par `FAnastasisSimulation::OnNewDay`
- **harnais** : aucune

La réputation n'est pas dans l'empreinte C++. `reputation += (cible − reputation) × 0,4` à minuit, cible = 50 + `BuildingsCompleted × 3`. Ambitions,
jalons, conseils, vols et le plancher des aînés ne sont pas portés. Entre habitants, la réputation ne
pèse sur aucune décision (n° 22 : seul le joueur).
