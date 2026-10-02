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
- **fermeture** : jalon B — goal-noise-001, perception-explore-001, goals-body-001, goals-resources-001, economy-gold-001, goals-work-001, goals-haul-001, goals-family-001
- **statut** : OUVERT
- **entree** : tranches puits → grenier (first-building-001, house-rest-001, granary-eat-001)
- **reference** : `src/sim/npc.js` `adultScores`, `goalNoise`, `statusBias`, mode de vie, prévision de survie
- **cpp** : `Village/AnastasisVillage.cpp`, table adulte, `UnportedGoalsFloor`
- **harnais** : actors, rng
- **detail** : `Public/Village/AnastasisVillage.h`, bloc ECARTS DECLARES, n° 1

16 des 25 lignes d'`adultScores` valent `UnportedGoalsFloor` (42) + leur vrai `phaseBias` ; un but
non porté qui gagne donne `observer`. Sans `goalNoise`, la décision ne consomme pas `sim.rng` : tant
que cet écart reste, la trajectoire JS est interdite.
sim-rng-001 : `goalNoise` et la table de ses 17 tirages sont portés (`Ai/AnastasisGoalNoise.h`,
`Parite.BruitDeBut`), NON branchés — chaque décision tire d'abord `exploreTarget` (2 à 8 fois, relevé
`docs/migration/phase3/P3_RNG_RELEVE.md`) : brancher la table seule coderait un faux ordre.
perception-explore-001 : branchés. Chaque décision tire `exploreTarget` (préparation, ligne `explore` de
`failureTargetBiasMap`) puis les 14 bruits et leurs 3 conditionnels, à leur place dans les sommes ;
`Village.TiragesDecision` rejoue les décisions mesurées au tirage près. Restent le plancher 42 et
`observer`. Le bruit d'une ligne non portée est TIRÉ (l'ordre du flux en dépend) mais PAS AJOUTÉ au
plancher : ajouté à un score inventé, il faisait gagner `observer` au hasard (`Village.Endurance`).
Depuis help-farm-001, la ligne `helpFarm` est calculée pour tous (`helpFarmScore * wf`, puis la chaîne des
biais) : sans ferme, elle vaut 0 au lieu du plancher 42. Son but est porté (cible : la parcelle faible ;
acte : la session `tend`). Sans parcelle, `farmPos` rendrait l'accès au marché prévu : il arrive avec
build-decision-001 ; d'ici là, l'habitant vaque.

### n° 2 — Des cibles relâchées que la référence garderait

- **classe** : REDUIT
- **destin** : A_FERMER
- **fermeture** : à attribuer (fin de chantier : suite de build-001 ; ancre sociale : suite de bonds-rumors-001)
- **statut** : OUVERT
- **entree** : tranches puits → grenier (first-building-001, house-rest-001, granary-eat-001)
- **reference** : `src/sim/npc.js` `progressBuildWork` (la cible reste posée), `holdTalkAct` ; reconsidération l. 893
- **cpp** : `Village/AnastasisVillage.cpp`, fin de chantier et fin de session sociale (`bHasTarget = false`)
- **harnais** : actors
- **detail** : `Public/Village/AnastasisVillage.h`, n° 2 ; `docs/migration/phase3/P3_RECONSIDERATION.md`

Depuis reconsider-001, le tirage l. 893 est fait : `if (!npc.target || sim.rng() < chance) chooseGoal`,
chance = `committedReconsiderChance(phaseReconsiderChance(needsReconsiderChance(npc, thinkDt)))`, phase
PERSONNELLE et quart de travail compris (75 tirages mesurés sur un jour rejoués au bit,
`Anastasis.Sim.Village.Reconsideration`). Le collant de but (`goalStickinessBonus`, `criticalReliefGoals`) est
porté aussi et entre dans la table avant Noûs (1 944 vecteurs `ReconsiderStickiness`, nature sans qualité ni
défaut : écart n° 10). Reste : deux fins d'action du C++ relâchent la cible (chantier fini ou à sec, ancre du
regard d'une session sociale) là où la référence la garde et laisse la reconsidération trancher.

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
vie (`lifestyleBias` dans la table, `lifestyleTravelFactor`, `lifestyleIndoorDuration`, `lifestyleTarget`)
attendent la décision (n° 1). Depuis act-gate-001, `notePlaceUse` appelle `lifestyleNotePlaceUse` pour
un habitant qui a un mode de vie ; pour un habitant sans, la référence en tirerait un dans le flux de
secours : ce tirage est sauté, l'habitant reste sans mode de vie.
chat-on-haul-001 : sans aîné, `maybeCounselPair` de la causette au dépôt ne conseille jamais.

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
chat-on-haul-001 : aucune technique (`npc.techniques` vide) ; la chance de raté lit une maîtrise de 0
(`bestCraftMastery`), comme la référence pour un habitant sans livre de techniques — vrai des cinq
habitants d'endurance.

### n° 11 — Récolte sans exploration ; ratés seulement dans les boucles portées ; monde généré immuable

- **classe** : REDUIT
- **destin** : A_FERMER
- **fermeture** : goals-resources-001 (`exploreTarget` dans la récolte), help-farm-001 (`rollCraftMiss` tend), build-materials-001 (`rollCraftMiss` chop, quarry)
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
perception-explore-001 : `exploreTarget` est porté (`World/AnastasisExplore.h`) et tire dans la décision ;
la récolte sans gisement connu ne l'appelle pas encore (`GatherTarget`, il vaque) — le branchement change
le comportement du fermier et revient à goals-resources-001.
chat-on-haul-001 : `rollCraftMiss` est porté, générique (`FVillage::RollCraftMiss(Npc, CraftId)`,
`Work/AnastasisCraftMiss.h`), et tiré dans les deux boucles portées qui le sautaient : la cueillette du
fermier (`farm`) et le chantier (`build`), avec la reprise allongée de 1,38. Reste : les profils dont la
boucle n'est pas portée (`tend` de `helpFarm`, `chop`, `quarry` : les 43 tirages du relevé endurance, le
premier au tick 257 par `helpFarm`), qui n'auront qu'à l'appeler. Maîtrise des techniques à 0 (n° 10).
Le soin de parcelle (`progressTendWork`, help-farm-001) est porté sans son `rollCraftMiss(sim, npc, "tend")` :
à brancher sur la fonction générique ci-dessus.

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

`VillageRng` est bien `sim.rng` (`makeRng(seed)`, sim-rng-001), et depuis reader-rng-001 le harnais le
reprend à l'état sauvé (`save.rng`) et projette son état VIVANT : sur `endurance`, la section `rng` est
identique à la référence jusqu'au tick 125. Ce qui reste : la position dans le flux décale dès qu'un site
de tirage de la référence n'est pas porté. Rumeurs de gisements seulement ; texte des répliques non porté
(le refus est évalué au premier tirage).
chat-on-haul-001 : `maybeChatOnHaul` est porté (tirage, compagnon à 3,4, `recordTalk` coworker / ambiance 0,1,
gisements dans les deux sens, gain de lien). Comme pour `socialize`, `shareRumors` n'y fait que les gisements :
ni accès bloqués, ni savoir négatif, ni croyance de marché, ni croyances (eau, lits, dangers), ni
`spreadRumorExchange` (personnes : `tellPerson` tire dans `sim.rng`, épisodes, chronique). `bondTalkGain`
sans partenaire, famille, aîné ni nature (« ami » seulement, comme `socialize`).

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
- **destin** : ASSUME
- **decision** : 2026-10-01 Alexandre — « Il faut pas que le joueur meurt de soif pour une règle absurde » (session « Mécanisme d'accélération temporelle », mission player-goals-001)
- **activation** : joueur incarné dont l'intention est `drink` (soif ≥ 88), `eat` (faim ≥ 92) ou `rest` (énergie ≤ 12) ; aucun scénario du harnais n'incarne de joueur
- **statut** : OUVERT
- **entree** : player-goals-001
- **reference** : `src/sim/decisionProvider.js` `decideAsPlayer` (`bodyOverrides` testé avant la table)
- **cpp** : `Village/AnastasisVillagePlayer.cpp` `DecideAsPlayer`, `IsRemedyFor`
- **harnais** : aucune

La référence cède à TOUTE intention quand le corps parle, remède compris : un joueur à soif 88 ne
pourrait plus jamais boire et finirait par mourir. Son propre commentaire dit « le joueur doit choisir
le remède ». **Assumé par Alexandre** : sur ce point, Unreal fait référence, le remède du besoin qui
parle passe toujours. Un scénario comparatif ne doit pas incarner de joueur.

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

### n° 24 — Intention du jour, ambition, cibles de risque : leurs tirages ne sont pas faits

- **classe** : REDUIT
- **destin** : A_FERMER
- **fermeture** : goals-day-intent (`assignDayIntent`, `intentExploreHint`, `assignAmbition`), goals-resources-001 (`spatialRiskBiasMap` > `recallOrSearch`)
- **statut** : OUVERT
- **entree** : perception-explore-001
- **reference** : `src/ai/dayIntent.js` (`ensureDayIntent`, `assignDayIntent`, `intentExploreHint`), `src/ai/ambitions.js` (`assignAmbition`), `src/sim/npc.js` (`spatialRiskBiasMap`, `spatialRiskTargetForGoal`)
- **cpp** : `Village/AnastasisVillage.cpp`, `ChooseGoal` (préparation d'adultScores)
- **harnais** : actors, rng
- **detail** : `docs/migration/phase3/P3_RNG_RELEVE_JOUR.md`

Avant `exploreTarget`, `chooseGoal` tire l'intention du jour quand elle est périmée (1 tirage, plus le
cap si elle vaut `explore` : 5 par jour dans le relevé, au changement de jour) et l'ambition d'un
habitant qui n'en a pas. Le C++ ne les tire pas : le flux se décale au premier changement de jour.
`intentExploreHint` n'est pas porté : avec une intention `explore`, la référence tirerait 1 ou 3 fois au
lieu d'`exploreTarget`. `spatialRiskBiasMap` ne tire pas dans le scénario (0 sur un jour) et n'est pas
évalué.

### n° 26 — Mémoire des lieux sans présence au poste

- **classe** : REDUIT
- **destin** : A_FERMER
- **fermeture** : workplace-presence-001
- **statut** : OUVERT
- **entree** : act-gate-001
- **reference** : `src/sim/simulation.js` (`notePlaceUse`, branche `actor.workPresence`), `src/sim/npc.js` (`workAtWorkplaceYard`, `updateInside` : `npc.workPresence`)
- **cpp** : `Village/AnastasisVillage.cpp`, `FVillage::NotePlaceUse`
- **harnais** : actors, buildings

`notePlaceUse` est porté, mais le bâtiment noté ne passe pas par `actor.workPresence` : dedans, sinon le
plus proche (1,9), sinon le poste pour un geste de travail. `workAtWorkplaceYard`, qui pose
`workPresence` et note un second geste (`dt * 0,4`) quand le fermier livre ou travaille à son propre
poste, n'est pas porté. Un geste fait au poste mais plus près d'un autre bâtiment est donc noté sur
l'autre, et le `laborToday` du poste en manque une part. `helpFarm` (help-farm-001) passe aussi par
`workAtWorkplaceYard` dans la référence : même manque.

### n° 27 — Planificateur collectif : branché sur une colonie reprise, pas sur le village du C++

- **classe** : REDUIT
- **destin** : A_FERMER
- **fermeture** : passe quotidienne des priorités (`updateCollectivePrioritiesDaily`), `pickCollectiveBuilding`, colonie du village C++
- **statut** : OUVERT
- **entree** : help-farm-001
- **modifie** : planner-wiring-001
- **reference** : `src/sim/collectivePriorities.js` (`collectiveGoalBias`, `collectiveGoalFloor`, `isFoodRush`, `farmStaffingGap`, `isWoodBootstrapDraftee`, `collectiveBuildingNeedScore`, `pickCollectiveBuilding`), `src/sim/npc.js` (`collectiveUrgencyBiasMap`, fin d'`adultScores` l. 1205-1258, `buildScore` l. 2733)
- **cpp** : `Village/AnastasisVillage.cpp`, `CollectiveDecisionOf`, `BuildPlannerView` / `WritePlannerView`, `BuildRowScore`
- **harnais** : actors, rng
- **detail** : `docs/migration/phase3/P3_PLANIFICATEUR.md`

Depuis planner-wiring-001, `CollectiveDecisionOf` est remplie par le planificateur (`AnastasisPlanner::DecisionFor`)
sur une vue du village, et ses écritures (vacance des maisons, stock des bâtiments, rapport de stock, charte,
caches) reviennent au village. La ligne `build` est calculée sans chantier (`buildScore` : besoin, liquidité,
traits, biais collectif) et sa cible est `marketAccessPoint`. Ce qui reste :

- **village sans colonie** : un village créé par le C++ n'a pas de `colony` ; il ne consulte pas le planificateur
  (décision vide, la table garde ses bits) et sa ligne `build` garde le besoin fixe 85 d'un chantier ouvert ;
- **passe quotidienne** : `updateCollectivePrioritiesDaily` (niveaux, métiers, scores stockés, focus, et la remise
  à zéro du cache `_effects`) n'est pas portée ; masquée dans le harnais. Le cache n'est donc vidé qu'à
  l'expiration de la charte, jamais au changement de jour ;
- **`isEssentialBuildType(chooseBuildingType())`** : `pickCollectiveBuilding` n'est pas porté, lu faux. Ne
  pèse que si le trésor est entre 1 et 14 pièces, sans chantier ni fondation possible (liquidité 0,35 au lieu de 1) ;
  son `stampDecision` n'est pas écrit ;
- **`colonizationBuildBias`, `colonySiteBuildBias`** : nuls (pas de lisière chaude ni de brief de chantier) ;
- **`liveHotPads`** : lu comme `doctrine.hotPads.length`, sans `pruneHotPads` ;
- **`findBuildSpot`** (frontière urbaine) : non fourni à la vue ;
- **ordre d'appel** : la référence interroge le planificateur ligne par ligne pendant la table ; le C++ une fois,
  avant la table. Seul le rafraîchissement paresseux du rapport de stock tire `sim.rng` (une fois par jour au plus) :
  le jour où il tire, son tirage peut changer de place dans le flux ;
- **arrivée sans chantier** : `tryOpenNewConstruction` / `tryBuild` ne sont pas portés (n° 18) : le bâtisseur
  arrivé au site du marché attend, puis se redirige après trois échecs.

### n° 28 — Mortalité : seule la mort par santé épuisée est portée

- **classe** : REDUIT
- **destin** : A_TRANCHER
- **statut** : OUVERT
- **entree** : mortality-001
- **reference** : `src/life/mortality.js` (`updateMortalityDaily`, `causeOfDeath`, `removeActor`, `scrubDeparted`, `settleEstate`), `src/life/life.js` (`updateLifeDaily`, `agePopulation`)
- **cpp** : `Village/AnastasisVillage.cpp` (`CauseOfDeath`, `UpdateMortalityDaily`) ; `Sim/AnastasisSimulation.cpp` (`RunDayJob`, travail 10 `lifeDaily`)
- **harnais** : actors, buildings
- **fermeture** : à attribuer (portage de l'âge — `agePopulation`, `age`, `lifeStage` — puis des tirages de famine et de vieillesse, du veuvage et de l'héritage)

Décision d'Alexandre du 2026-10-02 (conversation mortality-001) : la mortalité ne tue qu'à santé 0. Le C++
exécute la règle 1 de `causeOfDeath` (`health <= 0`) et rien d'autre : pas de mort au-delà de 92 ans, pas de
famine personnelle (`starvingDays` non porté), pas de crise de grenier, pas de risque de vieillesse (aucun
tirage, donc le flux aléatoire n'est jamais touché). Libellé « de faim » sans `starvingDays > 0`.
`removeActor` est réduit à ce que l'état de simulation porte : maison libérée (`RemoveNpc`), relations du
mort effacées chez les autres (`forgetTheDead`) ; pas de veuvage, d'orphelins, d'héritage, de moral de
colonie, de journal, de mémorial ni de deuil. Appelé par le travail différé 10, après `AssignSheltersDaily`
de minuit (la référence l'appelle après `agePopulation`, que le C++ n'a pas). `ensureNeeds` ne tire rien :
les besoins d'un habitant C++ existent toujours.

### n° 29 — Temps de trajet payé au coût du terrain dans le jeu Unreal

- **classe** : EXTENSION
- **destin** : A_TRANCHER
- **statut** : OUVERT
- **entree** : route-cost-001
- **activation** : `anastasis.Village.RouteCost=1` dans l'hôte Unreal ; défaut 1. Le village C++ seul reste à 0 pour la parité JS.
- **reference** : `src/sim/pathfinding.js` et `src/sim/simulation.js` — le chemin paie déjà les coûts de terrain, mais le budget de marche de la référence est uniforme.
- **cpp** : `Village/AnastasisVillage.cpp` (`MoveActor`) et `Public/Village/AnastasisVillage.h` (commutateur) ; hôte `AnastasisSimulationSubsystem.cpp`
- **harnais** : actors

Le même multiplicateur de la grille de navigation que lit l'A* devient le temps dépensé par segment de marche : une route réduit le temps par distance, une herbe humide l'augmente. Aucun nouveau graphe, tirage ni changement de choix de chemin. Le harnais n'active pas cette extension : `FVillage` démarre à 0 et seuls les pas de l'hôte Unreal la mettent à 1. Ce multiplicateur est une règle de jeu, pas une mesure physique ou une pente du maillage rendu. À trancher : conserver cette divergence ou rapprocher la référence JS lors d'une décision de simulation commune.

### n° 32 — Planificateur : la corvée de bois départage les égalités en ordre ordinal

- **classe** : SUBSTITUT
- **destin** : A_TRANCHER
- **statut** : OUVERT
- **entree** : planner-module-001
- **reference** : `src/sim/collectivePriorities.js` `woodBootstrapDraft` (`String(a.id).localeCompare(String(b.id))`)
- **cpp** : `Village/AnastasisPlanner.cpp`, `WoodBootstrapDraft`
- **harnais** : actors

À aptitude égale, la référence classe les habitants par `localeCompare` (collation ICU de la locale) ; le C++
les classe par ordre ordinal des identifiants. Les deux ordres coïncident pour des identifiants de même forme
(`npc-0`, `npc-12`, `npc-3` : chiffres comparés caractère par caractère), qui sont ceux de tous les scénarios ;
ils divergent pour des identifiants mêlant casses ou ponctuation. Branché depuis planner-wiring-001 : la
divergence toucherait `actors`. Porter la collation ICU ou restreindre la forme
des identifiants : à trancher.
