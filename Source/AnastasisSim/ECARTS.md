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
resource-targets-001 : la prévision de survie (`survivalForecastBias`) et le risque spatial
(`spatialRiskBiasMap`) sont portés. Ils s'ajoutent à chaque ligne, plancher compris, à leur place dans la
chaîne, comme la météo et la passe collective. Leurs replis réduits sont l'écart n° 33.
premiere-pensee-001 : `goalExplain` (les trois premières lignes, leur cause dominante) et `streetDecision`
(la marge avec la deuxième) sont écrits au commit, sur la table du C++. Tant qu'une ligne reste au plancher,
ses causes non portées (micro-plan, ambition, teinte du jour, mémoire sociale, souvenir, mandats) manquent, et
le rang, le score et la marge diffèrent de la référence.

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
resource-targets-001 : `pickDailyBuilding` (`pickDistrictAwareBuilding`, cible `maintain` du risque spatial)
prend sans quartiers son repli : le hachage FNV du jour, de l'habitant, du but et du sel. La référence, qui a
ses quartiers, score chaque bâtiment (`districtDestinationScore`) : le bâtiment entretenu peut différer.

### n° 4 — Pilotage réduit

- **classe** : REDUIT
- **destin** : A_FERMER
- **fermeture** : failure-target-001 (`navigation.destBuildingId` : `failureTargetBiasMap`, remise à zéro au commit) ; à attribuer : `separateCrowdedActors`, verrou de seuil domestique en route
- **statut** : OUVERT
- **entree** : tranches puits → grenier (first-building-001, house-rest-001, granary-eat-001)
- **reference** : `src/sim/crowdNav.js` (`applyCrowdSeparation`), `simulation.js` (`separateCrowdedActors`), `npc.js` (`act`, verrou du seuil domestique avant `moveActor`), `failureTargetBiasMap`
- **cpp** : `Village/AnastasisVillageNav.cpp` (la marche) ; `Village/AnastasisVillage.cpp` (`Act`, `CommitGoal`)
- **harnais** : actors
- **masques** : separationFoule
- **detail** : `docs/migration/phase3/P3_NAV_RELEVE.md`

Depuis nav-wiring-001, la marche passe par le service de navigation (`requestPath` : cache exact et de
zone, A* sous budget, file vidée avant et après la boucle des habitants), avec la file de porte, l'hésitation,
le facteur de vitesse de l'état porté (mode de vie compris), le contournement local, l'escalade anti-blocage et
les passages (`recordPassage`). Le harnais projette `navigation`, le chemin, `lastMoveDir`, `trafficTimer`,
`hesitation*` : au tick 32 d'endurance, ils sont identiques à la référence. Reste :
- `navigation.destBuildingId` : son dernier écrivain dans la référence est `failureTargetBiasMap` (`deliveryPos`),
  non porté, et `CommitGoal` l'efface à chaque décision, ce que la référence ne fait pas ;
- `separateCrowdedActors`, masqué (`separationFoule`) ;
- le verrou du seuil domestique en route (`isStableAccessTarget`, `doorApproachAt` avant `moveActor`) ;
- dans `movementSpeedFactor` : famille (n° 7), routes (aucune), quartiers (n° 3) valent 1 ;
- la file de porte calcule son point d'attente par `atan2` / `cos` / `sin` de la bibliothèque C++, et non par
  ceux de V8 : un écart au dernier bit reste possible.

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
resource-targets-001 : sans plan d'aide (`householdPlan` absent), la cible `aidHousehold` du risque spatial
est toujours celle du foyer, ou `socialPos`. La référence vise le parent à aider quand un plan existe.

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
Depuis lifestyle-decision-001, la table lit le mode de vie : `lifestyleBias` dans le terme
`rhythmBias + statusBias + lifestyleBias` (à l'heure du village), et `phaseWorkFactor` complet (garde la nuit,
aubergiste le soir, bourreau de travail, noctambule, matinal) dans le facteur de travail. Pour un habitant sans
mode de vie, la référence en tirerait un dans `sim.rng` : le penchant vaut 0. Restent `lifestyleTravelFactor`
(nav-service-001), `lifestyleIndoorDuration` et `lifestyleTarget`.

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
Depuis lifestyle-decision-001, la nature lue de la sauvegarde pèse dans la décision : `natureGoalBias` sur
chaque ligne, `natureWorkFactor` dans le facteur de travail, `natureStickBonus` dans le collant et
`goalExplain` (module `Life/AnastasisNature`, `Parite.Nature`). Un habitant créé par le C++ n'a pas de nature :
la référence lui en tirerait une dans le flux de secours (`rollNature`) ; il garde la nature moyenne. Restent
moyens : `natureSocialMods` (liens), `natureLearnFactor` (apprentissage), l'héritage.

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
spatial-risk-test-001 : depuis resource-targets-001, le lecteur relit et reprojette `mind.spots`. Un gisement
d'on-dit créé par le C++ (`CommitHearsaySpot`) n'a ni `speechActId`, ni `confidence` (0,72 dans la référence),
ni `viaPlayerId` : sa projection diffère de la référence dans `actors` dès qu'une rumeur de gisement passe.
`recallResource` ne lit aucun des trois : le choix du gisement n'en dépend pas.

### n° 17 — Météo des habitants : ce qui n'est pas porté

- **classe** : REDUIT
- **destin** : A_FERMER
- **fermeture** : goals-work-001 (`craft` après l'abri) ; beliefs-001 (`bestKnownBed`, écart n° 33) ; à attribuer : la taverne
- **statut** : OUVERT
- **entree** : village-weather-001 (932d1cc)
- **reference** : `weatherGoalBias.js`, `npc.js` (`shelterRain`, `bestKnownBed`), taverne
- **cpp** : `Life/AnastasisWeatherBehavior.*`, `Village/AnastasisVillage.cpp`
- **harnais** : actors
- **detail** : `Public/Village/AnastasisVillage.h`, n° 17 ; `docs/unreal/VILLAGE_WEATHER_001.md`

Les formules sont en parité (`Parite.MeteoHabitants`). Le foyer tient lieu de `bestKnownBed` ; après
l'abri, un but repris non exposé devient `craft` dans la référence, `observer` ici.
resource-targets-001 : le biais de la prévision de survie est porté, y compris `shelterRain` (−soif × 0,08).

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

Extension opt-in `SetMaterialCourier` (npc-life-bridge-001, A_TRANCHER) : un porteur nommé peut
retirer du bois ou de la pierre d'une tuile vivante accessible, porter jusqu'au seuil d'un chantier
sec et créditer son stock. Le mode est désactivé par défaut, donc le harnais de parité ne change pas.
L'hôte l'active pour le premier bâtisseur du scénario explicite `FirstSite ... Delivered=0` et
pour le premier chantier sec du village initial. Le scénario `Delivered=1` garde son devis livré.
Ce n'est pas encore `gatherWood` / `gatherStone` de la référence : pas de camp, de stock collectif,
de demandes de livraison, de salaire ni d'ouverture autonome ; un seul porteur explicite, charge
conservée si le chantier disparaît. Le test `Village.Chantier.PorteurMateriaux` vérifie les transferts
et la conservation sur un monde contrôlé.

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
- **fermeture** : goals-day-intent (`assignDayIntent`, `intentExploreHint`, `assignAmbition`)
- **statut** : OUVERT
- **entree** : perception-explore-001
- **reference** : `src/ai/dayIntent.js` (`ensureDayIntent`, `assignDayIntent`, `intentExploreHint`), `src/ai/ambitions.js` (`assignAmbition`), `src/sim/npc.js` (`spatialRiskBiasMap`, `spatialRiskTargetForGoal`)
- **cpp** : `Village/AnastasisVillage.cpp`, `ChooseGoal` (préparation d'adultScores) ; `Village/AnastasisVillageSpatialRisk.cpp` (`SpatialRiskTargetForGoal`, but `explore`)
- **harnais** : actors, rng, buildings
- **detail** : `docs/migration/phase3/P3_RNG_RELEVE_JOUR.md`, `docs/migration/phase3/P3_RISQUE_SPATIAL_RELEVE.md`

Avant `exploreTarget`, `chooseGoal` tire l'intention du jour quand elle est périmée (1 tirage, plus le
cap si elle vaut `explore` : 5 par jour dans le relevé, au changement de jour) et l'ambition d'un
habitant qui n'en a pas. Le C++ ne les tire pas : le flux se décale au premier changement de jour.
`intentExploreHint` n'est pas porté : avec une intention `explore`, la référence tirerait 1 ou 3 fois au
lieu d'`exploreTarget`.
resource-targets-001 : `spatialRiskBiasMap` est porté, avec `spatialRiskTargetForGoal` et `recallOrSearch`
(`Ai/AnastasisSpatialRisk`, `Village/AnastasisVillageSpatialRisk.cpp`). Sa cible `explore` lit encore
`intentExploreHint` : sans intention du jour, c'est `npc.target`. Dans le relevé sur 16 200 ticks, c'est là
que tombent ses 57 tirages : ils restent à cet écart. Ses écritures (seuils filtrés au premier appel,
`buildings`) sont faites.

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
- **cpp** : `Village/AnastasisVillageNav.cpp` (`MoveActor`, depuis nav-wiring-001) et `Public/Village/AnastasisVillage.h` (commutateur) ; hôte `AnastasisSimulationSubsystem.cpp`
- **harnais** : actors

Le même multiplicateur de la grille de navigation que lit l'A* devient le temps dépensé par segment de marche : une route réduit le temps par distance, une herbe humide l'augmente. Aucun nouveau graphe, tirage ni changement de choix de chemin. Le harnais n'active pas cette extension : `FVillage` démarre à 0 et seuls les pas de l'hôte Unreal la mettent à 1. Ce multiplicateur est une règle de jeu, pas une mesure physique ou une pente du maillage rendu. À trancher : conserver cette divergence ou rapprocher la référence JS lors d'une décision de simulation commune. Depuis relay-sim-chain-001 (marche portée de nav-wiring-001), le seuil de blocage de la référence (0,05 case par pas) est divisé par ce même multiplicateur quand il dépasse 1 : un marcheur lent sur sol lourd n'est plus pris pour coincé (test `Anastasis.Sim.Village.Recolte.RouteCostDelivery`, herbe détrempée). Mode référence inchangé.

### n° 30 — Recolte de bois bornee, sans logistique ni doctrine forestiere complete

- **classe** : REDUIT
- **destin** : A_TRANCHER
- **statut** : OUVERT
- **entree** : anthropic-wood-001 (portage autorise apres audit), verse par relay-wood-001 sans son transport vers le chantier (option c d'Alexandre)
- **reference** : fee66ae, src/sim/npc.js progressCraftGather/resourceScore, craftWork.js chop, forestSustain.js, metiers/catalog.js woodcutter
- **cpp** : Village/AnastasisVillageWood.cpp, Village/AnastasisVillage.cpp, Work/AnastasisWoodHarvest.h
- **harnais** : actors, tileDiff

SetJob accepte woodcutter, qui decide et preleve localement dans les tuiles connues. Rendement chop 3/4, ancrage 0.42, periode/fatigue et stock minimal 4 pour une foret aux attributs crown/clearing absents sont portes. Le helper porte aussi le plancher profond 10 et l'exemption frontier, mais le runtime ne dispose pas de ces autorites et ne les invente pas. Pas de colonisation, densite forestiere, pression de chantier, depot bois, transfert chantier, vente, relais, regeneration, technique, boost joueur, changement d'outil ou tirage de rate. La recolte est restreinte au metier explicite ; score et rappel sont des sous-ensembles declares. Le sac reste conserve et la recolte cesse au seuil de transport existant (>9, nourriture incluse), sans faux credit a un depot. Filtre de disponibilite sur tuiles memorisees et arret observer remplacent exploration/livraison manquantes. Une session distante ne permet pas de couper hors voisinage. Pas de flux aleatoire ajoute.

Avant labor-social-001, le bois coupe n'avait aucun puits : il restait dans `InventoryWood`, le planificateur ne le comptait pas comme mobilisable, et aucun chemin ne le livrait au chantier. Le porteur opt-in gardait sa propre charge ; le porteur n'est jamais traite comme bucheron (`IsWoodHarvester`), les deux flux ne partagent pas un habitant.

Mise a jour labor-social-001 : le transfert direct du sac du bucheron a un chantier ouvert est desormais porte par l'ecart n°43. Les autres limites ci-dessus demeurent.

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

### n° 33 — Prévision de survie et risque spatial : croyances des lieux sûrs et doctrine de lisière absentes

- **classe** : REDUIT
- **destin** : A_FERMER
- **fermeture** : beliefs-001 (`bestKnownWater`, `bestKnownBed`, `seedHomeBedBelief`) ; à attribuer : doctrine de lisière (`wantsColonizationClear`)
- **statut** : OUVERT
- **entree** : resource-targets-001
- **reference** : `src/ai/memory.js` (`bestKnownWater`, `bestKnownBed`, `seedHomeBedBelief`, `bestKnownBelief`), `src/sim/npc.js` (`forecastDrinkTarget`, `forecastRestTarget`, `shelterRainAccess`, `gatherWoodTarget`), `src/sim/colonizationDoctrine.js` (`wantsColonizationClear`)
- **cpp** : `Village/AnastasisVillageSpatialRisk.cpp` (`ForecastDrinkTarget`, `KnownBedOf`, `RecallOrSearch`)
- **harnais** : actors, buildings
- **detail** : `docs/migration/phase3/P3_RISQUE_SPATIAL_RELEVE.md`

Les replis de la prévision de survie et du risque spatial passent dans la référence par les croyances
`mind.beliefs` (puits et berges vus, lits vus ou entendus). Le C++ ne les a pas. Il boit donc au puits le
plus proche (`drinkAccessPoint`), sinon à la berge. Il dort au lit de son foyer s'il est achevé (ce que
sème `seedHomeBedBelief`, dont l'écriture dans `mind.beliefs` n'est pas faite), sinon au camp. Dans le
relevé, les deux croyances répondent toujours (381 décisions sur 381) : les distances, donc les biais, en
diffèrent dès que la croyance n'est pas le repli du C++. Ce repli appelle aussi `buildingAccessPoint`, ce
que la croyance ne fait pas : seuils filtrés et destination posée en plus. La doctrine de lisière est
lue comme fausse ; elle ne l'est jamais devenue dans le relevé.

### n° 34 — Traces de repos non posées au changement d'activité

- **classe** : REDUIT
- **destin** : A_FERMER
- **fermeture** : à attribuer (traces de vie : `life/restTraces.js`)
- **statut** : OUVERT
- **entree** : premiere-pensee-001
- **reference** : `src/sim/npc.js` (`setActivity`, l. 3811), `src/life/restTraces.js` (`maybeStampRestTrace`, `stampRestTrace`)
- **cpp** : `Village/AnastasisVillage.cpp`, `FVillage::SetActivity`
- **harnais** : buildings

`setActivity` est porté (l'activité et `activitySince`, à chaque changement). Quand la nouvelle activité est
`repose`, `relaxe` ou `dort`, la référence pose en plus une trace de repos sur le bâtiment où l'habitant se
repose (son foyer, son poste pour les outils, ou le bâtiment le plus proche dehors). Ces traces ne sont pas
portées : aucun bâtiment C++ n'en porte.

### n° 35 — Noûs : le paquet de débogage `_algoDebug` n'est pas tenu

- **classe** : REDUIT
- **destin** : A_FERMER
- **fermeture** : à attribuer (`nous-debug-001`)
- **statut** : OUVERT
- **entree** : premiere-pensee-001
- **reference** : `src/ai/algorithmic/runtime.js` (`stampDebug`, `computeAlgorithmicDecision`), `src/ai/algorithmic/bridge.js`
- **cpp** : `Village/AnastasisVillage.cpp`, `ComputeAlgorithmicDecision`
- **harnais** : actors

Dès sa première décision Noûs, la référence écrit `npc._algoDebug` : décision choisie, tous les candidats avec
leurs métadonnées, contexte de perception (faim, sources connues, gisements, or), exclusions, inertie, but
traduit, trace du pont (`bridgeTrace`) et compte des changements récents. Le pont relit une partie de ce
paquet (`decision`, `inertia`, `mappedGoal`). Le C++ tient la décision dans `FNpc::AlgoDecision`, mais pas
ce paquet : la clé manque à chaque habitant qui a pensé.
### n° 36 — `workTimer` remis à zéro au changement de but

- **classe** : SUBSTITUT
- **destin** : A_FERMER
- **fermeture** : worktimer-001
- **statut** : OUVERT
- **entree** : premiere-pensee-001
- **reference** : `src/sim/npc.js` (`commitGoalChoice` l. 2110-2135 : aucune remise à zéro ; `act` l. 3387 et 3522-3527 : seules écritures)
- **cpp** : `Village/AnastasisVillage.cpp`, `CommitGoal`
- **harnais** : actors

Dans la référence, `workTimer` ne revient à zéro qu'au geste (seuil d'1 s atteint) ou dans l'attente du joueur : le
reste d'une attente passe au but suivant. Au tick 32, npc-2 garde ainsi 0,5167 (31 ticks d'attente en `observer`)
en partant bâtir. Le C++ le remet à zéro à chaque changement de but. Retirer cette remise à zéro (mesuré dans
premiere-pensee-001) change trois tests de scénario écrits sur l'ancien comportement : `Village.Liens.Conversation`
(13 échanges avant la session au lieu de 3, la relation passe l'amitié), `Village.Liens.Amitie` (deux
conversations au premier échange) et `Village.Endurance` (un jour sans livraison). Les refaire est la mission
worktimer-001.

### n° 37 — Réserve d'eau expérimentale des champs

- **classe** : EXTENSION
- **destin** : A_TRANCHER
- **statut** : OUVERT
- **entree** : soil-water-budget-001
- **activation** : `anastasis.Village.SoilWaterBudget=1` dans l'hôte Unreal ; défaut 0, comme `FVillage` seul.
- **reference** : `src/sim/simulation.js` (`regrowFieldsDaily`) et `src/sim/weather.js` (`weatherAt`) ne font pas évoluer une réserve d'eau de champ.
- **cpp** : `World/AnastasisSoilWater.h`, `Village/AnastasisVillage.cpp` (`RegrowFieldsDaily`), `Public/Village/AnastasisVillage.h` ; hôte `AnastasisSimulationSubsystem.cpp`.
- **harnais** : tileDiff, actors

Un réservoir indiciel par champ reçoit la pluie normalisée du jour précédent et perd évaporation, drainage et débordement. Sa réponse modifie la fertilité effective lors de la repousse, sans réécrire la fertilité générée. Aucun tirage supplémentaire. Les coefficients sont des paramètres de jeu, non des millimètres observés. À 0, l'ancien chemin reste identique. À 1, l'état n'est pas encore sérialisé dans les sauvegardes JS, et le rendu du sol n'en lit pas encore la valeur ; cette version est expérimentale et ne constitue pas une preuve de carte ni de bénéfice villageois.

### n° 38 — Monde extérieur : la géopolitique comme pression qui se propage jusqu'au village

- **classe** : EXTENSION
- **destin** : A_TRANCHER
- **statut** : OUVERT
- **entree** : geopolitical-world-001
- **activation** : hôte seulement — `FAnastasisSimulation::GetGeo().Load(scenario)`, appelé par la commande Unreal `Anastasis.Geo.Load` ; défaut : déchargé. Aucun scénario du harnais ne charge de monde extérieur ; déchargé, `OnNewDay` ne fait rien de plus, au bit près (`Anastasis.Sim.Geo.CalmeSansEffet`).
- **reference** : aucune — demande d'Alexandre du 2026-10-07 (mission ANASTASIS_GEOPOLITICAL_WORLD_V1). Le plus proche dans la référence : `src/life/historicalRumors.js` (nouvelles datées sans effet mécanique) et `maybeImmigrate` de `src/sim/simulation.js` (arrivées sans cause), tous deux non portés.
- **cpp** : `Geo/AnastasisGeo.{h,cpp}` (module pur), `Sim/AnastasisSimulation.cpp` (`OnNewDay`, `AdmitGeoMigration`, `Reset`), `Village/AnastasisVillageGeo.cpp` (`AdmitExternalArrivals`)
- **harnais** : aucune
- **detail** : `docs/unreal/GEOPOLITICAL_WORLD_001.md`

Un scénario JSON (lieux, routes, acteurs, chocs, chacun avec sa provenance) décrit le monde autour du village.
Un choc émet des paquets de pression qui voyagent de nœud en nœud, avec délai et atténuation, et une nouvelle
qui voyage à part, plus vite, en se déformant. Le village ne lit que son exposition et son savoir. Le seul
effet local branché : un groupe de migrants arrivé au village devient des habitants par `spawnNpc`, comme
`arriveAsPlayer`. Aucun tirage aléatoire : le flux `sim.rng` n'est jamais lu. À trancher : garder ce monde
extérieur comme extension Unreal, ou le porter un jour dans la référence JS.

### n° 39 — Réservation d'un repas choisi par un joueur sans décision Noûs

- **classe** : SUBSTITUT
- **destin** : A_TRANCHER
- **statut** : OUVERT
- **entree** : player-food-loop-001
- **reference** : `src/sim/npc.js` (`algoOn = !playerControlled`), `src/ai/algorithmic/runtime.js` (`onAlgorithmicGoalCommitted`), `src/ai/algorithmic/mealReservation.js` (`reserveMeal`)
- **cpp** : `Village/AnastasisVillage.cpp` (`OnAlgorithmicGoalCommitted`)
- **harnais** : actors

La référence ne calcule pas de décision Noûs pour un joueur incarné, puis cherche malgré tout
la source du repas dans les métadonnées de cette décision. Un joueur fraîchement arrivé peut
donc choisir `eat`, mais la réservation échoue avec `no_known_source` même s'il connaît un grenier
approvisionné. Le C++ prend l'identifiant de la meilleure source dans les **croyances de ce même
habitant**, via `PerceiveFoodContext` ; `ReserveMeal` garde toutes ses validations physiques.
Cette correction de jouabilité diverge de la référence JS et reste à trancher par Alexandre.
Les scénarios actuels du harnais n'incarnent pas de joueur.

### n° 41 — Interception partielle de la pluie par une couronne incarnée

- **classe** : EXTENSION
- **destin** : A_TRANCHER
- **statut** : OUVERT
- **entree** : canopy-rain-shelter-001
- **activation** : uniquement dans l'hôte Unreal, lorsque la couronne est effectivement incarnée et `anastasis.Village.CanopyRain=1` ; défaut 1. Le village C++ seul reçoit une couverture nulle et conserve la parité.
- **reference** : `src/sim/npc.js` (`applyRainExposure`) n'a pas de couverture d'arbre.
- **cpp** : `Life/AnastasisWeatherBehavior.cpp` (`ApplyRainExposure`), `Village/AnastasisVillage.cpp` (lecture de couverture) ; hôte `AnastasisWorldEmbodiment.cpp` et `AnastasisSimulationSubsystem.cpp`.
- **harnais** : actors

La couverture géométrique d'une couronne réellement posée réduit au plus de 20 % la perte d'énergie et de santé due à la pluie. Les seuils de l'orage et la décision de chercher un bâtiment restent ceux de la référence ; un arbre ne devient pas un toit. Le facteur 20 % est un paramètre de jeu borné, pas une mesure validée pour ces essences. À trancher : garder cette extension dans le jeu ou porter une couverture du couvert arboré dans la référence JS.

### n° 42 — Sentiers de désir : posés sans paiement, sans classes formelles, sans raser de ressource

- **classe** : REDUIT
- **destin** : A_TRANCHER
- **statut** : OUVERT
- **entree** : settlement-morphogenesis-001 (numéroté 38 sur sa branche), versé par relay-settlement-001 sur le trafic de nav-wiring-001
- **activation** : `anastasis.Village.RoadEvolution=1` dans l'hôte Unreal ; défaut 1. Le village C++ nu reste à 0 (`SetRoadEvolutionEnabled`) : le harnais ne pose jamais de sentier.
- **reference** : `src/sim/simulation.js` (`recordPassage`, `updateRoadEvolutionDaily`, `evolveDesirePathTileDaily`, `setPlannedRoad`, `claimRoadFootprint`, `evolveFormalRoadTileDaily`, `canAffordRoadCost`), `src/sim/trafficDecay.js` (`decayFootTraffic`, `decayHaulTraffic`)
- **cpp** : `World/AnastasisTraffic.{h,cpp}` ; `Village/AnastasisVillage.cpp` (`DecayTrafficDaily`, `UpdateRoadEvolutionDaily`) ; `Village/AnastasisVillageNav.cpp` (`RecordPassage`, `MoveActor`, de nav-wiring-001) ; `Village/AnastasisVillagePlayer.cpp` (`DrivePlayer`) ; `Sim/AnastasisSimulation.cpp` (`OnNewDay`, travail 8 `roadEvolution`)
- **harnais** : actors
- **fermeture** : à attribuer (portage du marché et de son stock de bois, puis `evolveFormalRoadTileDaily` — ruelle, axe, pavage —, `claimRoadFootprint` sur les cases porteuses d'une ressource, `decayHaulTraffic` avec le transport, lecture de `traffic` dans `AnastasisJsSave`)

Un seul compteur : `sim.traffic` et `recordPassage` sont ceux de nav-wiring-001 (`FVillage::Traffic`,
`FVillage::RecordPassage`, `FNpc::TrafficTimer`). Le compteur passe d'entiers à `float` (Float32Array de la
référence) : la décroissance de minuit en a besoin. Le test « bâtiment » de `recordPassage` est celui de main
(liste vivante des bâtiments), lu à la case d'ancrage `floor(x), floor(y)` comme `buildingIndex` de la
référence ; `updateRoadEvolutionDaily` lit la même chose. Porté fidèlement : le compteur (f32, +1 par passage,
plafond 180), le pas de 0,85 s de marche (`trafficTimer` ; pour le joueur, seulement s'il a bougé, comme
`drivePlayerActor`), la décroissance de minuit `decayFootTraffic` (×0,88 − 0,55, nul sous 0,35), l'effort de
défrichage du sentier de désir (seuil 14, +1 + min(0,75, (t − 14)/42) par nuit, ×0,72 sous le seuil, 18 jours
+ 6 champ + 4 forêt + ⌈bois/10⌉), la classe `roadClassForTraffic` plafonnée à `lane`, le coût de marche du
profil posé sur la case, la version de navigation incrémentée s'il y a des habitants.

Réduit : la référence paie un bois au marché (`canAffordRoadCost`) ; le C++ n'a pas de marché, le sentier se
pose sans payer. Une case qui porte encore une ressource (champ semé, bois debout) n'est pas rasée : la
référence la rase et laisse un fantôme de culture. Les classes formelles (montée en ruelle ou en axe, pavage)
ne sont pas portées : un sentier reste de sa classe de naissance. Pas de journal ni de cadastre
(`markCadastreDirty`, `refreshUrbanIntents`), pas de `decayHaulTraffic` (aucun transport porté). Le travail
de nuit garde son rang 8 dans la file. Harnais : `traffic` n'est pas dans la projection JS (`Digest`) ni lu
de la sauvegarde (`packTraffic`) ; il est haché par `StateDigest`, avec les sentiers et leurs efforts. Le
sentier ne naît jamais sans l'activation de l'hôte ; la décroissance de minuit, fidèle, tourne partout.

### n° 43 — Livraison du bois coupe vers un chantier reel

- **classe** : REDUIT
- **destin** : A_TRANCHER
- **statut** : OUVERT
- **entree** : labor-social-001
- **reference** : `src/sim/npc.js` (`deliver`, `progressCraftGather`, `progressProduceWork`), `src/sim/transport/stockLedger.js` (`transferNpcToBuilding`)
- **cpp** : `Village/AnastasisVillageWood.cpp`, `Village/AnastasisVillage.cpp`, `Public/Village/AnastasisVillage.h`
- **harnais** : actors, buildings, tileDiff

Le bucheron qui porte du bois peut choisir `deliver` pour un chantier ouvert dont le devis a un manque de bois, en concurrence avec ses besoins vitaux. Quand aucun chantier ouvert ne peut poser une piece et que son manque de bois bloque le devis, sa ligne `build` est ineligible : elle ne doit pas retenir le bucheron sur un chantier improductif. Il suit ensuite le seuil de batiment existant ; `Perform` traite son sac avant le repli generique vers l'approvisionnement alimentaire. Le bois passe de `InventoryWood` a `Materials.StockWood` uniquement a l'arrivee, dans la limite du manque et de la capacite du site. Le constructeur utilise ensuite les regles existantes de consommation et d'achevement. Aucun rendement de coupe ni devis n'est change ; aucune ressource n'est creditee sans retrait du sac. Ce lien direct vers le chantier est un sous-ensemble du transport JS, qui passe normalement par ses depots, jobs de haul et reservations. Il ne porte ni scierie, ni planches, ni vente, ni marche, ni politique de production generale. Le porteur opt-in garde son flux separe. À trancher : conserver ce lien direct comme mecanisme de jeu, ou porter la logistique complete de la reference.

### n° 44 — Foyers fondateurs posés par l'hôte : identité et parenté comme données, sans vie familiale

- **classe** : EXTENSION
- **destin** : ASSUME
- **decision** : 2026-10-08 Alexandre — « On ignore JS pour fonder Valmire » (docs/unreal/FAMILLES_FEU_001.md, Décision)
- **statut** : OUVERT
- **entree** : familles-feu-001 (mandat d'Alexandre du 2026-10-08, `docs/unreal/FAMILLES_FEU_001.md`)
- **activation** : seulement quand l'hôte Unreal pose les fondateurs de Valmire (`anastasis.Village.Founders 1`, défaut 1, `Content/Anastasis/Scenario/valmire-fondateurs.json`). `SpawnNpc` seul laisse l'identité vide et aucun foyer : le harnais n'en pose jamais.
- **reference** : `src/sim/npc.js` (`createNpc` : `name`, `familyName`, `gender`, `age`, `familyId`), `src/life/household.js` et `src/sim/life.js` (`createFamily`, `leavePreviousFamily`, `seedStartingFamilies`), `src/simulation.js` (`populateFoundingLife`, `ROMAN_FOUNDERS` de `src/sim/romanChronicle.js`)
- **cpp** : `Village/AnastasisVillage.h` (`FNpc::Name` … `KinRole`, `FVillage::FFamily`, `AddFamily`, `JoinFamily`, `SetIdentity`), `Village/AnastasisVillage.cpp` (mêmes fonctions, `RemoveNpc`), `Village/AnastasisVillageStateDigest.cpp` ; hôte `Anastasis_UnrealV2/Sim/AnastasisValmireFounders.cpp`
- **harnais** : aucune
- **fermeture** : à attribuer (`goals-family-001` du P3_PLAN porte la vie familiale de la référence)

Les champs d'identité de `createNpc` sont portés tels quels, mais seulement comme données : aucune décision ne les lit encore (pas de `lifeStage`, pas d'enfant qui marche moins vite ou ne travaille pas, pas de `isFamilyWith` dans les liens, pas de couple ni de naissance). Le foyer reprend `adults`, `dependents` et `homeId` de `createFamily`, sans `births` ni `lastBirthDay`. Ce qui n'existe pas dans la référence : quatre foyers fondateurs de treize personnes et un moine au lieu des cinq fondateurs de `ROMAN_FOUNDERS` (l'invariant « trois hommes, deux femmes » tombe), des enfants, un frère, un pupille et un engagé dès le départ, un nom de foyer (`FFamily::Name`) et un rôle de parenté (`KinRole`). Le passé de chacun (où il était quand la Ville est tombée, ce qu'il a emporté, qui n'est pas venu) reste dans l'hôte, hors de la simulation. Tranché par Alexandre le 2026-10-08 : la fondation de Valmire ne suit pas la référence ; ces quatre foyers et le moine sont la fondation d'ANÁSTASIS. Reste ouvert, sans changer ce destin : la vie familiale de la référence (couples, naissances, parenté dans les liens).

### n° 45 — Sauvegarde de la simulation : format propre, parcours de l'empreinte d'état

- **classe** : EXTENSION
- **destin** : A_TRANCHER
- **statut** : OUVERT
- **entree** : save-state-001 (chantier 4 d'IRON_CRUSADE_001)
- **activation** : `FAnastasisSimulation::SaveState` / `LoadState`, appelés seulement par l'hôte (`Anastasis.Sim.Save` / `Anastasis.Sim.Load`) et par les tests `Anastasis.Sim.Sauvegarde.*`. Aucune règle de la simulation ne les appelle.
- **reference** : `src/sim/simulation.js` (`serialize`, `deserialize`), `src/sim/save.js` (lecture seule côté C++ : `Harness/AnastasisJsSave.h`, inchangé)
- **cpp** : `Core/AnastasisStateArchive.{h,cpp}` ; `Village/AnastasisVillageStateDigest.cpp` (`VisitState`, `FVillage::ArchiveState`, `AfterStateLoaded`) ; `Sim/AnastasisSimulation.cpp` (`ArchiveState`, `SaveState`, `ReadSaveHeader`, `LoadState`)
- **harnais** : aucune
- **fermeture** : à trancher (garder le format propre, ou écrire aussi le format JS)

La référence sauve par `serialize(sim)`, un JSON qui ne garde qu'une partie de l'état (ni la file de minuit,
ni la file de navigation, ni plusieurs caches qui décident pourtant du futur) et dont le C++ ne sait que lire
une projection (`AnastasisJsSave`). Le C++ sauve autre chose : un flux binaire étiqueté (nom de chaque clé,
taille de chaque nombre), produit par le **même parcours** que `StateDigest` (STATE_ORACLE_001). Il garde donc
tout ce que l'oracle juge décisif, et `check-state-fields` interdit d'oublier un champ. Une sauvegarde
JS ne se recharge pas par `LoadState`, ni l'inverse. Le hachage est inchangé, bit pour bit (test temporaire
d'identité contre l'ancien hacheur, 13 points, consigné dans la fiche de save-state-001). Les deux lacunes
du registre (`Colony`, `MarketStock`, posées seulement par `RestoreColonyForHarness`) ne sont pas sauvées.

### n° 46 — Biographie des bâtiments observée par la simulation

- **classe** : EXTENSION
- **destin** : A_TRANCHER
- **statut** : OUVERT
- **entree** : save-history-001 (suite du chantier 4 d'IRON_CRUSADE_001 ; portée depuis l'hôte Unreal, SETTLEMENT_MORPHOGENESIS_001)
- **activation** : `FVillage::SetBiographyEnabled(true)`, posé par l'hôte Unreal dans `ResetCanonical`. Le village C++ nu reste à faux : le harnais n'écrit aucune biographie.
- **reference** : aucune (la référence n'a pas d'histoire des bâtiments ; le plus proche : `resolveHouseUpgrades`, non porté)
- **cpp** : `Village/AnastasisVillageBiography.cpp` (`ObserveBiographies`) ; `Village/AnastasisVillage.{h,cpp}` (`FBuildingBiography`, appel en fin d'`UpdateActors`, remise à zéro dans `Bind`) ; `Village/AnastasisVillageStateDigest.cpp` (parcours d'état)
- **harnais** : aucune
- **fermeture** : à trancher

Qui a fondé un bâtiment, pour quel foyer, quand il a changé de mains, combien de nuits il a été plein :
l'hôte Unreal l'observait image par image pour fixer la forme des maisons. Cette histoire ne se relit
pas dans un instantané, et une partie rechargée la perdait. Elle est maintenant observée à chaque pas
de la simulation, après les habitants. Elle entre dans l'empreinte d'état et dans la sauvegarde
(`SaveFormatVersion` 2), et un saut de temps n'en perd plus aucune transition. Aucune règle ne la lit.
La forme (programme d'architecture) reste déduite dans l'hôte, des faits sauvés : phase de la maison,
métier et foyer du fondateur.

### n° 47 — Mémoire épisodique : portée sans croyances sur les personnes, sans culture, sans famine, et étendue au feu et à l'aide

- **classe** : REDUIT
- **destin** : A_TRANCHER
- **statut** : OUVERT
- **entree** : memoire-decisions-001 (mandat d'Alexandre du 2026-10-08, `docs/unreal/MEMOIRE_DECISIONS_001.md`)
- **reference** : `src/ai/episodes.js` (`KINDS`, `EPISODE`, `recordEpisode`, `recordWitnesses`, `trimChronicle`, `fadeEpisodes`, `repairChronicleReferences`, `storytellerBias`, `shareEpisodes`, `tellEpisodes`, `retell`, `episodeFeeling`, `applyEpisodeFeelings`, `episodeGoalBias`, `reasonFor`, `variantIndex`), `src/life/speechActs.js` (`createGossipEpisode`, `commitReceivedEpisodeBelief`), `src/ai/rumorSpread.js` (`spreadRumorExchange`), `src/life/mortality.js`, `src/sim/simulation.js` (`shareRumors`, arrivées, `raised`, travail `memory`), `src/sim/life.js` (`runLifeRelationsPhase`), `src/sim/npc.js` (`score.episode`)
- **cpp** : `Life/AnastasisEpisodes.{h,cpp}` ; `Village/AnastasisVillage.{h,cpp}` (`FNpc::Chronicle`, `RecordEpisode`, `RecordWitnesses`, `ShareEpisodes`, `TellEpisodes`, `ReceiveEpisode`, `TellEpisode`, `FadeEpisodesDaily`, `ApplyEpisodeFeelingsDaily`, `EpisodeGoalBiasOf`, mortalité, `raised`, les six lignes de décision) ; `Village/AnastasisVillageGeo.cpp` (arrivées) ; `Village/AnastasisVillageWood.cpp` ; `Village/AnastasisVillageStateDigest.cpp` ; `Sim/AnastasisSimulation.cpp` (travaux `lifeDaily` et `memory`) ; hôte `Anastasis_UnrealV2/Sim/AnastasisValmireFounders.cpp` (le feu)
- **harnais** : actors, rng
- **fermeture** : à attribuer (portage de `socialMemory.js` — `sharePeopleBeliefs`, `noteSocialFromEpisode` —, de `culture.js` et de l'échec de repas d'`eat()`)

Porté fidèlement : les dix-sept types de souvenir et leurs poids, la capacité de huit (tri stable par poids et avance du vécu), l'oubli de minuit (−0,7 par jour, trente jours, poids 4), le récit à la rencontre (un par rencontre, poids 12, six bouches au plus, le tirage du départ puis de l'envie de répéter dans `sim.rng`), la déformation (poids qui baisse, chiffre qui enfle, nom qui se perd et lieu qui dérive après deux bouches, légende après trois), la réception (`createGossipEpisode`, confiance 0,95 / 0,75 × 0,9), le ressenti du soir sur les relations, le biais de but à sa place (après `completionBias`), les témoins (six cases, quatre au plus, pas les enfants), le deuil, l'arrivée, le chantier achevé. REFERENCE : `createGossipEpisode` reprend `sourceEpisode.aboutName` quand `retell` l'a perdu (`??` lit `null` comme absent) : dans la référence comme ici, le nom ne se perd jamais vraiment.

Réduit : `sharePeopleBeliefs` n'est pas porté, et son tirage (`sim.rng`, avant les histoires) manque au flux ; `cultureEpisodeSpread` vaut 1 ; pas d'`socialAppraisal` ni de croyance sociale née d'un fait ; pas de famine (l'échec de repas d'`eat()`, seul moment où la référence l'enregistre, n'existe pas ici : on mange « à vide » chez soi) ; pas de vol, de sauvetage, de naissance, d'union, d'héritage, d'ambition ni de conseil (rien ne les produit encore) ; un mort reste nommé dans les souvenirs (la référence le garde par le mémorial que `rememberGone` pose à chaque mort, absent ici). Le deuil va aux membres du foyer (écart n°44) au lieu du conjoint et des enfants seuls. EXTENSION : les types `fall`, `carried`, `leftBehind` (le premier soir au feu, enregistrés et racontés par l'hôte) et `helped`, `refusedHelp` (l'aide demandée, Bible §29), avec leurs biais de but ; `TellEpisode`, le récit d'un souvenir précis de vive voix (même `retell`, sans les deux tirages du choix).

### n° 48 — Maison de famille : décider de bâtir, demander de l'aide, le toit à plusieurs

- **classe** : EXTENSION
- **destin** : A_TRANCHER
- **statut** : OUVERT
- **entree** : memoire-decisions-001 (mandat d'Alexandre du 2026-10-08, `docs/unreal/MEMOIRE_DECISIONS_001.md` ; « seul pour un abri, à plusieurs pour une vraie maison », `FAMILLES_FEU_001.md`)
- **activation** : seulement quand des foyers existent (écart n°44 : les fondateurs posés par l'hôte, `anastasis.Village.Founders 1`). Sans foyer, `UpdateFamilyHousesDaily` ne fait rien, aucun chantier n'a de liste d'admis, et la ligne `build` reste celle de la référence : le harnais n'est pas touché.
- **reference** : aucune (`tryOpenNewConstruction`, l'ouverture collective, n'est pas porté : écart n°18 ; `evaluateRequest` est dans la Bible §29, pas dans le JS)
- **cpp** : `Village/AnastasisVillage.{h,cpp}` (`FBuilding::OwnerFamilyId`, `AllowedBuilders`, `AskedIds` ; `FHelpAnswer`, `HelpLog` ; `UpdateFamilyHousesDaily`, `EvaluateHelp`, `CanBuildAt`, `AwaitsHelp`, `SettleFamilyHouse` ; accès au chantier dans `ChooseGoal`, `BoundBuildSite`, `PickBuildSite`, `ConstructionAccessPoint`, `WorkConstruction`) ; `Village/AnastasisVillageStateDigest.cpp` ; `Sim/AnastasisSimulation.cpp` (travail `lifeDaily`)
- **harnais** : aucune

Chaque soir, une famille sans toit à soi (une par soir ; celle dont un seul a un toit, comme le chef de la maison d'ouverture, après celles qui n'en ont aucun) décide de bâtir : son chef trace la parcelle près de lui, sur une case d'où il atteint le puits et qui ne coupe personne de l'eau (devis livré : la famille apporte ses matériaux). Seuls la famille et ceux qui ont dit oui y travaillent. Les murs montent par les siens ; le toit (la moitié des pièces) attend qu'un aidant hors de la famille y ait posé ses pièces : les siens s'arrêtent, l'aidant pose seul les premières. Chaque jour, le chef va demander de l'aide à deux personnes de plus, d'abord celles qu'il apprécie ; chacune pèse sa réponse (Bible §29 : une somme, la raison dominante retournée) — souvenir d'une aide reçue de lui (+40, vécue : une aide racontée par un autre n'oblige pas), amitié (×0,5), entraide (+8), souvenir d'un refus de lui (−40, vécu aussi), son propre toit d'abord (−45), dette non rendue (−25), autre chantier (−20) ou son métier (−10), faiblesse (−40), inconnu (−12). Le refus se retient (`refusedHelp`) ; tout le monde sollicité, le tour recommence. La maison levée revient à la famille (le chef propriétaire, les siens y logent sans la capacité d'une maison de phase 1), et le chef retient chacun de ceux qui l'ont aidée (`helped`, avec ses coups de marteau). La demande ne se marche pas : elle se fait le soir, sans tour physique des maisons. À trancher : ces règles comme base de l'économie de l'entraide, ou les porter d'abord dans la référence.

### n° 51 — L'eau du monde suit le réseau de drainage canonique

- **classe** : EXTENSION
- **destin** : A_TRANCHER
- **statut** : OUVERT
- **entree** : water-network-001
- **activation** : hôte seulement — `FAnastasisSimulation::ApplyWaterMask`, appelé par `UAnastasisSimulationSubsystem::ResetCanonical` quand `anastasis.Sim.WaterNetwork` vaut 1 (défaut). Aucun test de la simulation ni scénario du harnais ne l'appelle : `GenerateWorld` et `Reset` restent ceux de la référence, au bit près.
- **reference** : `src/world/hydrology.js` (`carveChannels`, `stampLakeBasins`, `prunePuddles`) — l'eau de la référence est faite de tranchées sous le niveau de la mer (27 plans d'eau, la plupart sans exutoire, HYDRO_NETWORK_001) ; décision d'Alexandre du 2026-10-08 : « l'eau doit respecter son réseau ».
- **cpp** : `World/AnastasisWorld.cpp` (`RestampWater`), `Sim/AnastasisSimulation.cpp` (`ApplyWaterMask`)
- **harnais** : aucune
- **detail** : `docs/unreal/WATER_NETWORK_001.md`

L'hôte calcule le réseau de drainage canonique (`Anastasis_UnrealV2/WorldView/AnastasisCanonicalGeography` :
graine seule, recette fixe, aucune CVar de rendu) et en tire un masque d'eau par tuile ; la simulation
le prend. Une tuile devenue eau perd sa ressource ; une tuile d'eau rendue à la terre devient prairie,
juste au-dessus de la mer. `Shore` et `Wetness` sont recalculés sur la nouvelle eau avec les formules
de la génération ; l'humidité de fond (`Moist`) et les types qui en découlent (champs, forêt) ne sont
pas refaits. `FlowX` / `FlowZ` / `FlowAmt` restent ceux de la référence.
