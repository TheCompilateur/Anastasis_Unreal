# Portage du simulateur JS vers Unreal — module `AnastasisSim`

Source de référence : `C:\dev\Jeux IV Kingdoms`, dossiers `src/sim`, `src/runtime`,
`src/life`, `src/ai`. Le rendu (`src/render3d`) n'est **pas** porté : il est écrit
pour three.js et sera remplacé par le rendu Unreal.

## Règle du module

`AnastasisSim` ne dépend que de `Core` et `CoreUObject`. Pas d'`Engine`, pas
d'acteur, pas de rendu. La simulation doit tourner en headless — tests
d'automation, rejeu de bug, serveur — exactement comme en jeu. Tout ce qui touche
à l'affichage vit dans le module `Anastasis_UnrealV2`.

Corollaire : le module de jeu peut dépendre de `AnastasisSim`, jamais l'inverse.

## Le contrat : parité bit à bit, pas « à peu près »

Le simulateur JS est la référence. Le portage doit rendre **exactement** les mêmes
doubles, pas des valeurs proches. Deux raisons concrètes :

1. Un état JS doit pouvoir se rejouer dans Unreal et donner le même village. C'est le
   **harnais** qui relit cet état, pas le jeu : le format de sauvegarde joueur est natif
   Unreal, décision prise dans `docs/migration/phase2/P2_MODELE_DONNEES.md`.
2. Un bug reproduit avec une graine doit se reproduire des deux côtés.

Un écart d'un ulp n'est pas cosmétique : `Math.floor(hash2d(...) * n)` bascule de
catégorie au bord, et ce sont des arbres, des roches et des herbes qui changent de
place. Sur une comparaison `dist < radius`, c'est une décision de PNJ qui bascule.

Ce qui n'est pas fidèle se **déclare** : registre `ECARTS.md` (dans ce dossier), une fiche par
écart dès le commit qui l'introduit, classe et destin (`A_FERMER` / `A_TRANCHER` / `ASSUME`).
Protocole : `docs/migration/PROTOCOLE_ECARTS.md`.

### Comment la parité est vérifiée

`tools/unreal/gen-parity-vectors.mjs` (dans le dépôt JS) importe les modules de
référence **tels quels** et émet `Private/Tests/AnastasisParityVectors.inl` : des
vecteurs d'entrée/sortie où tous les doubles sont donnés par leur motif binaire —
un littéral décimal perdrait le dernier bit, et c'est ce bit qu'on teste.

```bash
cd "C:/dev/Jeux IV Kingdoms" && node tools/unreal/gen-parity-vectors.mjs
```

Les tests `Anastasis.Sim.Parite.*` comparent bit à bit. **Ne jamais corriger un
vecteur à la main pour faire passer un test** : soit le portage a dévié, soit la
référence JS a changé et il faut regénérer.

## Pièges rencontrés (à relire avant de porter la suite)

**La sémantique numérique de JavaScript.** Tout nombre JS est un double ; les
opérateurs binaires (`>>> 0`, `| 0`, `^`, `<<`, `Math.imul`) convertissent d'abord
ce double en entier 32 bits selon ECMA-262. Un portage « raisonnable » écrit
`(uint32)(x * 374761393)` et croit avoir traduit — mais si `x` vaut 3.5, JS calcule
`1311664875.5` en double **puis** tronque. `AnastasisJs::ToUint32` reproduit cette
conversion ; l'utiliser partout où le JS écrit `>>> 0` sur autre chose qu'un entier
32 bits déjà formé.

**`Number(v) || 1`.** En JS, `0` est *falsy* : `Number(0) || 1` vaut 1. D'où
`AnastasisJs::NumberOr`, qui retombe sur le défaut pour NaN **et** pour 0. Sans
cette subtilité, `StepPlan(0)` diviserait par zéro au lieu de rendre 1x.

**`Math.hypot`.** V8 l'implémente par mise à l'échelle sur le max puis sommation
compensée de Kahan. `std::hypot` et `sqrt(dx*dx+dy*dy)` donnent des résultats
voisins mais différents. `AnastasisMath::JsHypot` reproduit l'algorithme de V8.

**`float` interdit.** La référence calcule en 64 bits. Une simulation qui tourne
des heures accumule l'écart jusqu'à la divergence. Tout est en `double`, y compris
les positions.

**Pas de `FMath::Lerp`, pas de `FRandomStream`.** `FMath::Lerp` n'a pas la même
forme algébrique que `a + (b - a) * t`, donc pas le même dernier bit.
`FRandomStream` est un LCG différent de mulberry32 : il produirait une suite valide
mais *autre*, et toute sauvegarde JS deviendrait injouable.

## État du portage

### Fait — couche 0, socle déterministe

| Unreal | Source JS |
| --- | --- |
| `Core/AnastasisJsNumeric.h` | (nouveau — sémantique ECMA-262) |
| `Core/AnastasisRng.h/.cpp` | `src/sim/rng.js` |
| `Core/AnastasisSimMath.h/.cpp` | `src/sim/util.js` |
| `Core/AnastasisSimClock.h/.cpp` | `src/runtime/simClock.js`, `frameDeltaSeconds` de `src/runtime/gameLoop.js` |
| `Core/AnastasisSpatialGrid.h/.cpp` | `src/sim/spatialGrid.js` |

### Fait — couche 1, génération du monde (Phase 1)

| Unreal | Source JS |
| --- | --- |
| `World/AnastasisWorldNoise.h` | `valueNoise` / `smoothNoise` / `fbm` de `src/sim/world.js` |
| `World/AnastasisWorldArchetype.h/.cpp` | knobs sim de `src/sim/worldArchetypes.js` (pas air/forest/wardrobe) |
| `World/AnastasisHydrology.h/.cpp` | `src/sim/hydrology.js` |
| `World/AnastasisWorld.h/.cpp` | `generateWorld`, plafond forêt, `pickFieldCropId` |

Tests mesurés 2026-09-11 (`UnrealEditor-Cmd -nullrhi`) : `Monde` / `Archetype` / `Culture` / `Libm` **PASS**. `Fbm` **FAIL** 1 vecteur (~2.5 ulp sur `fbm(0.1, 0.2, 12345)`) — le stockage tuile est f32+`round3`, les cartes vecteurs restent identiques. Si le bit-exact double de `fbm` devient requis : remplacer `AnastasisJs::Sin` (fdlibm), jamais les vecteurs `.inl`. `SemantiqueJs::ToUint32(1e21)` échoue (couche 0, hors worldgen).

### Fait — couche 2, navigation (terrain + A*)

| Unreal | Source JS |
| --- | --- |
| `World/AnastasisNavGrid.h/.cpp` | couche terrain de `src/sim/navGrid.js` + `blockedAt` / `footBlockedAt` / `tileTraversalCost` de `simulation.js` |
| `World/AnastasisPathfinding.h/.cpp` | `src/sim/pathfinding.js` |

Tests mesurés 2026-09-13 : `Parite.NavCout` / `Parite.NavChemin` / `Parite.NavInvariants` **PASS**
(suite `Anastasis.Sim` : 17 PASS, 2 KNOWN_EXPECTED_FAILURE, 0 FAIL). Vecteurs générés par
`tools/migration/gen-nav-vectors.mjs` — 32 chemins comparés point par point, motif binaire
compris, plus 24 cases de coût.

**Ce que la mutation a appris.** Trois mutations posées exprès pour vérifier que les vecteurs
mordent :

| Mutation | Détectée | Ce que ça dit |
| --- | --- | --- |
| échanger deux voisins dans `NEIGHBORS` | **non** | l'ordre des voisins ne change aucun chemin : le comparateur du tas est un ordre total, la suite des `pop` ne dépend pas de l'ordre des poussées |
| retirer le départage par `h` du comparateur | oui (`NavChemin`) | c'est **lui** qui rend le chemin reproductible |
| stocker `MoveCost` en `double` au lieu de `float` | oui (`NavCout`) | la référence stocke en `Float32Array` : un `double` est plus précis, donc faux |

La première a corrigé un commentaire que j'avais écrit faux dans `AnastasisPathfinding.h`.

#### Le service de navigation (mission nav-service-001) — module seul

| Unreal | Source JS |
| --- | --- |
| `World/AnastasisNavService.h/.cpp` | `src/sim/navService.js` entier ; de `navGrid.js` : `createNavMetrics` (compteurs de navigation seuls), `recordNavTransition`, `navTraceSnapshot`, `navigationTargetKey` |

**Pas branché** : le village cherche toujours ses chemins par `AnastasisPath::FindPath`. Le branchement
(`beginNavTick` + `processNavQueue` avant et après la boucle des PNJ, `requestPath` dans la marche) est la
mission suivante. L'acteur est vu à travers `FNavAgent` (les champs de chemin que le service écrit) et le
monde à travers `INavServiceHost` (temps, vitesse, `navVersion`, A*, acteurs vivants, métriques).

Ce qui décide, et qu'il ne faut pas « améliorer » : le cache est une `Map` JS, **ordonnée** — le garde-fou
mémoire efface les 80 premières clés au-delà de 480, d'où `FNavCache` et pas un `TMap` ; le tri de la file est
**stable** ; un chemin vide est rangé comme un échec par `applyPathToActor` mais servi par le cache. Détails dans
l'en-tête de `AnastasisNavService.h`.

Preuve : `Parite.NavServiceFonctions` (202 vitesses, 36 clés, 5 clés de cible, 140 priorités) et
`Parite.NavService` — 5 scénarios, 5 473 opérations rejouées des deux côtés, SHA-1 du texte canonique de l'état
complet du service après chacune. Vecteurs : `tools/migration/gen-nav-service-vectors.mjs -ref <clone>`.

**Mutations** (posées exprès, mesurées le 2026-10-02) :

| Mutation | Détectée | Ce que ça dit |
| --- | --- | --- |
| garde-fou : effacer les 80 **dernières** clés au lieu des 80 premières | oui (`garde-fou` #1355) | l'ordre d'insertion du cache est observable |
| cache de zone : seuil `NAV_ZONE` (8) au lieu de `NAV_ZONE + 1` (9) | **non** | mutation équivalente : le premier nœud d'un chemin en cache est voisin de son départ, dans la même zone 8×8 que le demandeur, donc à 8 cases au plus. Le seuil 9 n'est jamais atteint par un cache rempli en jeu |
| cache de zone : seuil 7 | oui (`garde-fou` #555) | la frontière réellement atteignable est couverte |

Non mesuré : un tri **non stable** de la file. Les files des scénarios ne dépassent pas 6 jobs, et sous
cette taille le tri d'Unreal procède par insertion, donc stable lui aussi ; la mutation serait invisible. Le
code garde `Algo::StableSort`, qui est la seule traduction fidèle d'`Array.prototype.sort`.

Pas encore porté de cette couche : `crowdNav.js`, les points d'accès des bâtiments (dépendent de
`src/sim/urban/intent.js`, chantier urbanisme), les compteurs de `navMetrics` propres à la marche des PNJ
(`stuck*`, `pathDoorWaits`...). Ils suivront leurs systèmes.

### Fait — couche 3, budget de simulation (noyau causal)

| Unreal | Source JS |
| --- | --- |
| `Core/AnastasisSimBudget.h/.cpp` | noyau causal de `src/sim/simulationBudget.js` |

**C'est la référence qui trace la ligne du portage.** Son en-tête porte une loi datée du
10/08/2026 — « la machine peut changer la vitesse à laquelle le monde est calculé, elle ne
doit pas changer le monde qui est calculé » — née d'un bug mesuré : `pressure` était une
moyenne mobile du temps mur, et la pression décide quels PNJ pensent. Même graine, mêmes
ticks : population 6 contre 5, trésor 328 contre 321.

Depuis, tout ce qui touche au chronomètre (`ema`, `noteSimulationBudgetFrame`,
`frameWallMs`, `simMs`, `observedTier`, le HUD) est déclaré observation seule et ne décide
plus rien. Ce n'est donc pas du ressort d'un module dont le contrat est le déterminisme :
non porté, à refaire dans la couche de présentation. Reste ici ce qui décide : palier,
multiplicateurs, bande de cadence, intervalle.

`logicalLod.js`, l'autre moitié de la vague 3, n'est pas porté : il agrège l'état du
village (habitants, bâtiments) et suivra ses systèmes.

### Fait — tranche verticale : le puits (mission first-building-001)

Hors de l'ordre des vagues, et assumé : une seule boucle d'habitant, de bout en bout, pour
brancher le premier bâtiment. Détail, écarts et dette : `docs/unreal/FIRST_BUILDING_001.md`.

| Unreal | Source JS | Preuve |
| --- | --- | --- |
| `Life/AnastasisNeeds.h/.cpp` | `life/needs.js` : `urgeScore`, `needGoalScores`, `tickNeeds` hors intérieur, `tickVitality`, `satisfyDrink` | `Parite.Besoins`, 165 vecteurs bit à bit (`tools/migration/parity/needs.mjs`) |
| `Village/AnastasisVillage.h/.cpp` | `addBuilding`, `countBuildings`, seuils (`navGrid.js`), `localOccupancy`, `accessPointNear`, `drinkAccessPoint`, `nearestWell`, `reachedMoveTarget`, `moveActor`/`nextWaypoint` réduits, `updateNpc`/`act`/`perform` pour `drink` | `Village.Puits.*` : assemblage déterministe, **pas** de parité de trajectoire |
| `Sim/AnastasisSimulation.*` (repris de `agent/sim-tick-day`) | `tick(dt)` → `for (npc of actors) updateNpc` | `Village.Puits.Hote` |

La table de décision est RÉDUITE : `drink` contre un plancher déclaré (`UnportedGoalsFloor`),
pas contre les ~24 autres buts. Tant qu'elle l'est, le harnais différentiel ne peut pas juger
cette boucle — seules les fonctions de besoins sont en parité.

### Fait — tranche verticale : la maison (mission house-rest-001)

Le deuxième bâtiment, et le premier où l'on entre. Détail : `docs/unreal/HOUSE_REST_001.md`.

| Unreal | Source JS | Preuve |
| --- | --- | --- |
| `Life/AnastasisVillageRhythm.h/.cpp` | `life/villageRhythm.js` : phases, `villagePhase`, `isNightPhase`, `phaseBias` | `Parite.Rythme`, 642 vecteurs |
| `Life/AnastasisNeeds.*` (ajouts) | `tickNeeds` branche intérieure `rest`, `satisfyRest` ; `sleepQuality` (`life/domestic.js`) | `Parite.Besoins` (495) ; `Parite.Rythme` (7 de foyer) |
| `Village/AnastasisVillage.*` (ajouts) | `enterBuilding`, `updateInside`, `exitBuilding`, `tryEnterIndoorAction`, `buildingForIndoorAction` (rest), `buildingNearActor`, `nearestHousing`, `findOpenShelter`, `countShelterOccupants`, `assignSheltersDaily`, `assignHomeToHousehold` (sans famille), `redirectDomesticDoorFailure` | `Village.Maison.*` |

La table compte désormais deux lignes portées, `rest` et `drink`. Chaque but non porté vaut 42
**plus son vrai `phaseBias`** : le rythme est en parité, seul le 42 est déclaré.

### Fait — tranche verticale : le grenier, par Noûs (mission granary-eat-001)

Noûs (`src/ai/algorithmic`) est ACTIF PAR DÉFAUT dans la référence : c'est lui qui est porté.
Détail et comportements JS reproduits : `docs/unreal/GRANARY_EAT_001.md`.

| Unreal | Source JS | Preuve |
| --- | --- | --- |
| `Ai/AnastasisNous.h/.cpp` | `hungerUtility.js` (urgence, candidats, tri, cooldown, but), `inertia.js`, `scheduler.js`, `decision.js` | `Parite.Nous`, 312 vecteurs |
| `Life/AnastasisNeeds.*` (ajouts) | `tickNeeds` branche intérieure `eat`, `satisfyEat` | `Parite.Besoins` (542 au total) |
| `Village/AnastasisVillage.*` (ajouts) | `mealReservation.js` (1-377), `hungerAction.js`, `runtime.js`, `bridge.js`, `stockLedger.js` (réserver / rendre / prélever), `memory.js` (croyances de stock), `mealPlace`, branche `eat` d'`assignTarget` et de `buildingForIndoorAction` | `Village.Grenier.*` |

La table compte les 25 lignes d'`adultScores` ; `eat`, `rest`, `drink` sont calculées, les autres
valent 42 + `phaseBias`, et Noûs les biaise toutes. La cadence de pensée est celle de Noûs pour tous
les buts (2,2 s ; 0,55 s en crise).

### Fait — tranche verticale : cueillir puis livrer (mission gather-deliver-001)

Le seul chemin fidèle « champ → grenier » de la référence : un **fermier** dont le poste est le
grenier. Un sans-métier qui cueille finit par vendre au marché (or), non porté. Détail :
`docs/unreal/GATHER_DELIVER_001.md`.

| Unreal | Source JS (`fee66ae`) | Preuve |
| --- | --- | --- |
| `Work/AnastasisGather.h/.cpp` | `npc.js` (resourceScore nourriture, deliveryScore dépôt, completionBias, traitGoalBias, jobPriority, workplaceGoalBias au grenier, mealPathBlocked, survivalWorkFactor, shouldHaulGatherLoad), `memory.js` (presumedNoise, believedStock), `needs.js` (workWillFactor), `moralPressure.js`, `craftWork.js` (swingPeriodFor, yieldPerSwing), `craftFatigue.js`, `fieldCrops.js`, `fieldWorkPosts.js`, `skills.js`, `content.js` TRAITS, `metiers/catalog.js` farmer/settler | `Parite.Recolte`, 2011 vecteurs |
| `Village/AnastasisVillage.*` (ajouts) | `perceive` (gisements, `rememberSpot`, `trimMemory`, `forgetEmptied`), `recallResource`, `progressCraftGather`, `ensureCraftSession`, `fieldWorkTarget` / `claimedFieldPosts`, `resourceTileNear`, `depleteTile`, `beginHaulToDepot`, `deliver` (branche dépôt), `applyGoalEligibility`, `atPost` 1,25 | `Village.Recolte.*` |

La table calcule `gatherFood` et `deliver` pour le fermier au grenier seulement ; pour les autres
elles restent au plancher. Le monde généré reste immuable : le village tient l'état vivant des
tuiles récoltées (`LiveTileAt`). L'extension food-supply-001 (non fidèle) cohabite pour les autres
habitants.

### Fait — la repousse des champs, et l'endurance (mission field-regrow-001)

| Unreal | Source JS (`fee66ae`) | Preuve |
| --- | --- | --- |
| `Work/AnastasisFields.h/.cpp` | `simulation.js` regrowFieldsDaily, regrowFieldTile, FIELD_FOOD_CAP / FIELD_REGEN_PER_DAY ; `fieldCrops.js` fieldSeasonRegenAmount, dailyChance, rotateFieldCropId, ensureFieldCropReady | `Parite.Repousse`, 686 vecteurs |
| `Village/AnastasisVillage.*` (ajout) | regrowFieldsDaily sur l'état vivant des tuiles | `Village.Repousse.*` |
| `Sim/AnastasisSimulation.*` (ajout) | `enqueueDayDeferred` : `landRegen` en tête (regrowForestDaily sans effet dans la référence) | `Village.Repousse.Hote` |

`Anastasis.Sim.Village.Endurance` fait vivre puits, maison, grenier et fermiers 8 jours : la
nourriture est conservée, personne n'a faim, et la limite de la table réduite apparaît — sans
`socialize`, la solitude devient critique au jour 3 et le travail s'arrête. Détail :
`docs/unreal/FIELD_REGROW_001.md`.

### Fait — socialiser et se détendre (mission social-relax-001)

| Unreal | Source JS (`fee66ae`) | Preuve |
| --- | --- | --- |
| `Life/AnastasisNeeds.*` (ajouts) | `tickNeeds` branches socialize / relax, `satisfySocial`, `satisfyRelax` | `Parite.Besoins` (+142) |
| `Work/AnastasisGather.*` (ajout) | `moralPressure(...).socialMul` | `Parite.Recolte`, cas Moral |
| `Village/AnastasisVillage.*` (ajouts) | lignes socialize / relax d'adultScores, `socialPos`, `rhythmTarget` / `domesticTarget` relax, `socialize()` sans compagnon, entrée relax | `Village.Endurance` |

La branche « compagnon » de `socialize()` (liens, paroles, rumeurs) est venue ensuite (bonds-rumors-001).
Avec ces deux remèdes, les fermiers de l'endurance livrent chaque jour sur 12 jours. Détail :
`docs/unreal/SOCIAL_RELAX_001.md`.

### Fait — les liens et les rumeurs (mission bonds-rumors-001)

| Unreal | Source JS (`fee66ae`) | Preuve |
| --- | --- | --- |
| `Life/AnastasisBonds.h/.cpp` | `bonds.js` (affinité, gain, paliers), `talk.js` (hash, portes, durées, tours, refus), `socialMemory.js` (fiches, théorie de l'esprit, biais, recherche), `moodlets.js` (newFriend) | `Parite.Liens`, 2 834 vecteurs |
| `Village/AnastasisVillage.*` (ajouts) | `socialize()` branche compagnon, `recordTalk`, `beginTalkSession`, `holdTalkAct`, `advanceTalkTurn`, `bondSocialTarget`, `rememberedSocialTarget`, `createInformResourceSpotActs`, `commitHearsayResourceSpot`, grille spatiale du tick | `Village.Liens.*` |
| `Sim/AnastasisSimulation.*` (ajout) | `enqueueDayDeferred` : les 17 travaux, `memory` (n° 14) porté | `Village.Liens.Oubli`, `Tick.DayAdvance` |

Deux habitants qui se croisent se parlent, se figent le temps de la session, se souviennent l'un de
l'autre et se racontent les gisements qu'ils ont vus. Texte des répliques et rumeurs hors gisements :
écart n° 16. Détail : `docs/unreal/BONDS_RUMORS_001.md`.

### Fait — la météo et les habitants (missions env-realism-001, village-weather-001)

| Unreal | Source JS (`fee66ae`) | Preuve |
| --- | --- | --- |
| `World/AnastasisWeather.h/.cpp` | `weather.js` : `weatherAt`, `sampleCoverFront`, `coverLobeAt`, `winterSnowAt`, `winterFrostAt`, `weatherWetnessAt`, `weatherHumidityAt` ; `fieldCrops.js` `fieldSeasonFromDay` | `Parite.Meteo`, 1 112 vecteurs (12 212 valeurs ; 39 à 1 ULP, toutes sur cos / sin) |
| `Core/AnastasisJsNumeric.h` (ajout) | `Math.exp` de V8 = fdlibm `__ieee754_exp` | `Parite.Meteo` : avec l'exp du CRT, 2 échecs à 6 et 8 ULP ; avec fdlibm, 0 |
| `Life/AnastasisWeatherBehavior.h/.cpp` | `weatherGoalBias.js` : `readSimWeather`, `weatherGoalBiasFromState`, `shouldSeekRainShelter`, `shelterRainScore` ; `npc.js` : `shelterRainDuration`, `applyRainExposure` ; `simulation.js` : bloc pluie de `movementSpeedFactor` | `Parite.MeteoHabitants`, 2 724 vecteurs (4 212 valeurs, 0 échec) ; `MeteoHabitants.Formules` pour les fonctions non exportées |
| `Village/AnastasisVillage.*` (ajouts) | `score.weather` sur chaque ligne, ligne `shelterRain`, porte d'orage de `commitGoalChoice`, `shelterRainAccess`, `buildingForIndoorAction` / `workplaceAcceptsIndoorGoal` pour `shelterRain`, `performShelterRain`, récupération d'`updateInside` | `MeteoHabitants.Orage`, `.TempsSec`, `.CielDeLaSimulation` |
| `Sim/AnastasisSimulation.*` (ajout) | `sim.seed` lu par `readSimWeather` | `MeteoHabitants.CielDeLaSimulation` |

Sous l'orage le fermier lâche la cueillette, s'abrite à son grenier le temps de l'averse,
récupère, puis reprend une activité de lui-même. Sans hôte (tests d'assemblage), pas de météo :
chaque terme vaut exactement 0. Écart n° 17 dans `Village/AnastasisVillage.h`.

### Fait — le chantier (mission build-001)

| Unreal | Source JS (`fee66ae`) | Preuve |
| --- | --- | --- |
| `Work/AnastasisBuild.h/.cpp` | `simulation.js` buildCost / costMultiplier / siteCanPlacePiece / consumeSiteMaterials, `constructionPieces.js`, `craftWork.js` (profil build), `craftToolSwitch.js`, tables métiers / traits | `Parite.Chantier`, 432 vecteurs |
| `Village/AnastasisVillage.*` (ajouts) | ligne `build` d'adultScores (besoin 85), `constructionAccessPoint`, `progressBuildWork`, `pickBuildSite`, `workConstruction` | `Village.Chantier.*` |

Un bâtiment ouvert par l'hôte monte en 22 pièces sous les coups des bâtisseurs, chaque pièce prend sa
part du devis au stock du site, et le bâtiment achevé sert. Ouverture par les habitants, livraisons,
bois et pierre : écart n° 18, build-002. Détail : `docs/unreal/BUILD_001.md`.

### Fait — le lecteur de sauvegarde JS du harnais (mission sim-state-reader-001)

Vague 4, côté harnais seulement : le format JS est **lu** comme instrument, jamais écrit
(`docs/migration/phase2/P2_MODELE_DONNEES.md`, décision 2). Il charge un scénario
(`tools/migration/scenarios/*.json`, `docs/migration/phase3/P3_SCENARIOS.md`) en état C++ et le
reprojette sur les sections de `serialize`.

| Unreal | Source JS (`anastasis-ref-p3`) | Preuve |
| --- | --- | --- |
| `Core/AnastasisJson.h/.cpp` | `JSON.parse` (nombres au bit près, clé répétée), `digestValue` sur un arbre | `Harnais.Json` |
| `Harness/AnastasisJsSave.h/.cpp` | `save.js` : `tileDiff`, `applyTileDiff` (format courant), lecture des sections `seed rng w h time day tileDiff buildings actors mealReservations` ; `pristineWorld.js` | `Harnais.Lecture`, vecteurs `tools/migration/gen-scenario-vectors.mjs` |

Au tick 0 du scénario `endurance`, les 10 sections du périmètre projetées depuis l'état C++ rendent
l'empreinte que la référence calcule (35 sections, global `47a2a2ffc0e5d98a`). `tileDiff` est
**recalculé** depuis le monde C++ contre une génération vierge : la génération C++ sur 108 × 114
rend exactement les 590 cases de la référence. Pour bâtiments, habitants et réservations, la
projection repart de l'objet d'origine et y **réécrit** chaque champ lu depuis la valeur C++ ; les
champs non lus (un habitant JS en a 105 au premier niveau) sont **recopiés**, figés. L'état lu est
repris par l'hôte et tourne depuis `sim-digest-emitter-001` (`Harness/AnastasisHarnessTrace.h`,
`FAnastasisSimulation::ResetFromWorld`, `FVillage::RestoreForHarness`) : la trace Unreal sort au format
JS. Premier rapport, première divergence (la cadence du budget, non branchée dans `UpdateActors`) :
`docs/migration/phase3/P3_PREMIER_RAPPORT.md`.

### Fait — les besoins au rythme de chaque habitant (mission needs-factors-001)

Le rapport 2 (`P3_PREMIER_RAPPORT.md`) place la première divergence au tick 1 sur les besoins : dans la
référence, faim, soif et énergie avancent au rythme propre de chaque habitant. Cinq facteurs le font,
lus au début de `tickNeeds` ; les branches C++ les reçoivent désormais en argument.

| Unreal | Source JS (`anastasis-ref-p3`) | Preuve |
| --- | --- | --- |
| `Life/AnastasisGenome.h/.cpp` | `life/genome.js` : `hashString`, `deriveGenomeSeed`, `createGenome`, `recombineGenome` (mutations comprises), `derivePhenotype` et ses six dérivées, `ensureGenome`, `genomeFingerprint` | `Parite.Genome` (232 vecteurs, allèle par allèle) |
| `Life/AnastasisConditioning.h/.cpp` | `life/conditioning.js` entier | `Parite.Conditionnement` (266) |
| `Life/AnastasisNeeds.*` (ajouts) | `needs.js` : les cinq facteurs, `FNeedFactors` en argument de chaque branche (défaut 1 partout), branche intérieure `relieve`, `needsCritical` (`AreNeedsCritical`), l'appel de `tickConditioning` en fin de tick (`TickNeedsConditioning`) | `Parite.BesoinsFacteurs` (1 908 vecteurs, quatorze situations, conditionnement relu après le tick) |

**Pas branché dans le village** : `FNpc` ne porte ni phénotype ni conditionnement, et les appels du village
passent le défaut — multiplier par 1,0 est exact, donc aucun test `Village.*` ne bouge. Le branchement
(champs de `FNpc`, lecteur, `UpdateNpc`) est une autre mission.

Deux choses que le branchement doit respecter : les facteurs sont lus **avant** la branche, et
`tickConditioning` passe **après** la branche et `tickMoodlets` (qui peut écrire `morale`, donc la porte
`overworked`). Le génome tire sur son **propre** flux, dérivé de `sim.seed` et de l'identifiant de
l'habitant, jamais sur `sim.rng`.

`AreNeedsCritical` et non `NeedsCritical` : `AnastasisVillage` porte déjà une fonction de ce nom sur
`FNeeds`, même formule. La recherche dépendante des arguments rendait ses appels ambigus.

**Branché par needs-wiring-001.** `FNpc` porte `Phenotype` et `Conditioning` (`TOptional`), le lecteur les
lit comme `deserialize` les complète (`ensureGenome(actor, sim.seed)` : phénotype lu, ou dérivé du génome,
ou d'un génome créé ; `ensureConditioning` : neutre s'il manque) et projette le conditionnement.
`UpdateNpc` lit les facteurs avant la branche, puis `tickMoodlets`, `TickNeedsConditioning` (travail =
`WORK_GOALS`, dedans compris ; repos = but `rest`), puis la pluie. `AnastasisVillage::NeedsCritical`
délègue à `AreNeedsCritical`. Un habitant créé par le C++ n'a ni phénotype ni conditionnement : il reste
médian et garde ses bits (écart n° 8). Preuve : `Anastasis.Sim.Village.BesoinsParHabitant` ; au forage du
tick 1 du scénario `endurance`, les huit mètres et le conditionnement sont égaux à la référence.

### Fait — le mode de vie (mission lifestyle-001), module seul

| Unreal | Source JS (`anastasis-ref-p3`) | Preuve |
| --- | --- | --- |
| `Life/AnastasisLifestyle.h/.cpp` | `sim/lifestyle.js` : `dayPhase`, `lifestyleForId` (table `LIFESTYLES`), `assignLifestyle`, `ensureLifestyle`, `lifestyleBias`, `lifestyleTravelFactor`, `lifestyleIndoorDuration`, `lifestyleTarget` (`LifestyleTargetBuilding`), `lifestyleNotePlaceUse`, `lifestyleDailyUpdate` | `Parite.ModeDeVie`, 7 465 vecteurs (`tools/migration/parity/lifestyle.mjs`) |

Hors portage : `lifestyleLabel` et `lifestyleColor`, du texte et une couleur d'affichage ; leur table
(`LIFESTYLES`) est portée, leur formatage non.

**Pas branché** : `FNpc` ne porte pas de `FLifestyle`. Ce que le branchement doit savoir :
`ensureLifestyle` TIRE dans `sim.rng` (un tirage) quand le mode de vie manque, et `updateNpc` l'appelle
en première ligne, AVANT la cadence. `lifestyleIndoorDuration` et `lifestyleNotePlaceUse` appellent
`ensureLifestyle` sans flux : un habitant sans mode de vie tirerait alors dans `fallbackRng`
(`GetAnastasisFallbackRng`), un flux global. `lifestyleTarget` rend un bâtiment ; l'appelant calcule le
point d'accès, une fois, comme la référence.

**Branché en partie par lifestyle-wiring-001.** `FNpc::Lifestyle` (`TOptional<FLifestyle>`) ; le lecteur
lit `npc.lifestyle` avec les `??=` d'`ensureLifestyle` et le projette, et refuse un habitant sans mode de vie
(ou d'identifiant inconnu) : `deserialize` le tirerait dans `sim.rng`. `UpdateNpc` appelle
`LifestyleDailyUpdate` juste après la cadence, avant les besoins, pour tout habitant qui en a un. Pas
branchés : l'`ensureLifestyle` de tête (aucun habitant C++ n'a de mode de vie, écart n° 8), `lifestyleBias`
et les autres lectures (décision, marche, intérieur, destination, `placeMemory`). Preuve :
`Anastasis.Sim.Village.ModeDeVie` ; au forage du tick 1, `lifestyle` est égal à la référence.

### Fait — le flux `sim.rng` et le bruit de décision (mission sim-rng-001), sans branchement

| Unreal | Source JS (`anastasis-ref-p3`) | Preuve |
| --- | --- | --- |
| `FVillage::GetSimRngState` / `SetSimRngState` | `sim.rng.state()` / `setState()` — `VillageRng` est le flux `makeRng(seed)` de `simulation.js` l. 1022 ; `save.rng` (save.js l. 66, l. 168) | `Village.FluxSimRng` |
| `Ai/AnastasisGoalNoise.h/.cpp` | `npc.js` : `goalNoise` (fonction pure), et la table de ses tirages dans `adultScores` (14 inconditionnels, 3 conditionnels), ordre et lignes de source | `Parite.BruitDeBut` : la fonction exécutée telle qu'écrite dans la source, les amplitudes lues dans la source, et 104 décisions MESURÉES rejouées depuis leur état |

**Le relevé** (`tools/migration/trace-sim-rng.mjs`, mesure partagée dans `rng-trace-lib.mjs`) :
`docs/migration/phase3/P3_RNG_RELEVE.md` (600 ticks) et `P3_RNG_RELEVE_JOUR.md` (5 400 ticks, un jour),
avec leurs CSV — chaque tirage du scénario `endurance` : tick, habitant, site, appelant, chemin. Sur
le jour : 1 854 tirages, 104 décisions, chacune = 2 à 8 tirages d'`exploreTarget` (ligne `explore`
de `failureTargetBiasMap`) PUIS les 14 bruits de la table ; hors décision : `npc.js` l. 893,
`rollCraftMiss`, rumeurs, `tellPerson`, `assignDayIntent`, `maybeChatOnHaul`.

**Pas branché**, et c'est voulu : les 14 bruits viennent APRÈS un nombre variable de tirages
d'`exploreTarget`, qui dépend de la mémoire des cases (`npc.mind.cells`). Brancher la table seule
coderait un faux ordre. Suite : perception-explore-001 (`npc.mind.cells`, `perceive`,
`exploreTarget`, `randomWalkTarget`, `recallOrSearch`).

### Fait — la mémoire des régions, l'exploration, et les tirages de la décision (mission perception-explore-001)

| Unreal | Source JS (`anastasis-ref-p3`) | Preuve |
| --- | --- | --- |
| `World/AnastasisExplore.h/.cpp` | `memory.js` : `cellIndex`, `markCell`, `exploreTarget` ; `simulation.js` : `randomWalkTarget` | `Parite.Exploration` : 188 cas `exploreTarget` (régions connues ou non, bâti, promenade, centre) et 48 `randomWalkTarget` |
| `FNpc::KnownCells` / `CellCount`, `FVillage::Perceive` | `npc.mind.cells` / `mind.cellCount`, écrits par `perceive` (`markCell` à la case arrondie) | idem |
| `FVillage::ChooseGoal` | la préparation d'`adultScores` (`failureTargetBiasMap` > `exploreTarget`) puis les 14 `goalNoise` et leurs 3 conditionnels, sur le flux partagé, dans les sommes de la référence | `Village.TiragesDecision` : les 104 décisions mesurées du scénario `endurance` (un jour), reprises sur le village du harnais, tirent exactement ce que la référence a tiré |

Le bruit d'un but non porté est tiré mais pas ajouté au plancher 42 (écart n° 1) : ajouté à un score
inventé, il faisait gagner `observer` au hasard (`Village.Endurance` le voyait). Pas branchés, déclarés :
la reconsidération (l. 893, n° 2), le rate de coup (n° 11), l'intention du jour, l'ambition et les cibles
de risque (n° 24). Relevé : `docs/migration/phase3/P3_RNG_RELEVE_JOUR.md`.

### Fait — la reconsidération, la phase personnelle et le quart de travail (mission reconsider-001)

| Unreal | Source JS (`anastasis-ref-p3`) | Preuve |
| --- | --- | --- |
| `Life/AnastasisReconsider.h/.cpp` | `needs.js` : `needsReconsiderChance` ; `villageRhythm.js` : `personalFrac`, `villagePhaseFor`, `phaseReconsiderChance` ; `npc.js` : `committedReconsiderChance` (`GOAL_AI` : 2,5 s / 7 s / 0,42) | `Parite.Reconsideration` : 128 cas `needsReconsiderChance`, 104 phases personnelles |
| `Life/AnastasisWorkShift.h/.cpp` | `workShift.js` : `noteShiftGoalCommit`, `noteShiftArrival`, `shiftShields`, `shiftEntryCommitted` ; `extractionPost.js` : `opensExtractionShift` (et `extractionPostFor`, `withinCourt`, `extractionResourceOfBuilding`, en ligne) | `Parite.Reconsideration` : 255 commits de quart (état, but, plancher, bouclier), 5 arrivées |
| `FNpc::PhaseChangedAt` / `FNpc::WorkShift`, `FVillage::UpdateNpc` / `ChooseGoal` / `CommitGoal` | `syncVillagePhase` à la phase personnelle, le tirage l. 893 (`!npc.target \|\| sim.rng() < chance`), le verrou de quart de `commitGoalChoice`, `noteShiftArrival` à la création des sessions de travail | `Village.Reconsideration` : les 75 tirages l. 893 mesurés sur un jour du scénario `endurance`, rejoués sur le village du harnais : chance au bit, même résultat (38 reconsidérations) |

La branche 0,92 de `phaseReconsiderChance` n'est jamais prise sur ce chemin : `updateNpc` synchronise la
phase en tête de chaque tick (npc.js l. 874), avant la pensée ; elle est portée mais morte, comme dans la
référence. `ReconsiderChanceAt` lit sans écrire. Relevé : `docs/migration/phase3/P3_RECONSIDERATION.md`
(`tools/migration/trace-reconsider.mjs`). Reste du n° 2 : le collant de but. `UpdatePlayer` lit encore la
phase du village (habitant incarné, mission player-goals).

### Fait — le coup raté et la causette au dépôt (mission chat-on-haul-001)

| Unreal | Source JS (`anastasis-ref-p3`) | Preuve |
| --- | --- | --- |
| `Work/AnastasisCraftMiss.h/.cpp` | `craftMiss.js` : `craftMissKindFor`, `craftMissChance`, `canRollCraftMiss` ; `craftFatigue.js` : `missMul` | `Parite.CoupRate` : 4 050 chances (profils, compétence, coups, énergie, maîtrise), 135 portes, et les 43 tirages `rollCraftMiss` mesurés sur endurance (tend, chop, quarry ; 1 raté) |
| `FVillage::RollCraftMiss`, `ApplyCraftMissRecovery` ; `ProgressCraftGather` (farm), `ProgressBuildWork` (build) | `rollCraftMiss`, `stampCraftMiss`, `applyCraftMissRecovery`, leurs appels dans `progressCraftGather` et `progressBuildWork` | `Village.Recolte.CoupRate` : un tirage, pas de rendement, estampille `whiff`, reprise × 1,38 au bit, refroidissement de 9,5 s sans tirage |
| `FVillage::MaybeChatOnHaul`, `TellSpots`, options de `RecordTalk` ; `AnastasisBonds::ShouldSpeakNow` (ambiance) | `maybeChatOnHaul` (npc.js l. 5503), `shareRumors` > `tellSpots`, `recordTalk(…, { kind, ambientChance })` | `Village.Recolte.CausetteDepot` : tirage consommé au-dessus de 0,42 ; dessous, gisements dans les deux sens et liens +3 / +2 ; `Parite.CoupRate` : 324 portes de parole à 0,1 |

`rollCraftMiss` est générique : `tend` (help-farm-001), `chop` et `quarry` n'ont plus qu'à l'appeler. Le
`shareRumors` de la causette ne fait que les gisements (n° 16) ; `maybeCounselPair` n'a pas d'aîné (n° 8).

### L'atelier de vecteurs — déclarer au lieu d'écrire

Trois modules portés, trois générateurs écrits à la main : à ce rythme, 198 modules
valent 198 générateurs. `tools/migration/parity-kit.mjs` rend la preuve déclarative pour
les fonctions à arguments et retours scalaires — on dit quelle fonction appeler et sur
quelles entrées, l'atelier exécute la référence et émet le `.inl`.

```bash
node tools/migration/gen-parity.mjs simulation-budget.mjs   # ou --tous
```

Les fonctions à état — un A*, un hacheur, une boucle de tick — gardent un générateur dédié :
leur difficulté est ailleurs que dans la plomberie.

Deux pièges payés une fois, corrigés dans l'atelier pour tous les modules à venir :

- **`UTF8_TO_TCHAR` dans un initialiseur statique rend un pointeur pendouillant.** L'objet
  de conversion est un temporaire ; la table ne gardait que des chaînes vides, et les
  vecteurs accusaient le portage en comparant `""` à `"normal"`. Les tables portent
  désormais des octets UTF-8, convertis au point d'usage.
- **Une claim non testée est une claim fausse.** L'en-tête affirmait que `Math.hypot`
  compte face à un `sqrt(dx²+dy²)`. Vérification : sur les 17 entrées de la batterie, deux
  faisaient diverger les bits et **aucune ne changeait de bande**. Trois entrées ont été
  cherchées exprès, où `hypot` rend exactement le rayon et `sqrt` le double juste
  au-dessus — near contre medium, medium contre far, far contre invisible.

Écarts assumés, documentés dans les en-têtes :

- La grille spatiale stocke des **index** `int32`, pas des références d'acteurs :
  en C++ un pointeur vers un élément de `TArray` meurt au premier realloc.
- `startGameLoop` (boucle `requestAnimationFrame`) n'est pas porté — c'est le
  `Tick` d'Unreal.
- `simSpeedRenderGate3d` / `applySimSpeedRenderGate3d` ne sont pas portés : ce sont
  des réglages three.js (ombres, particules, pixel ratio). L'équivalent Unreal
  appartient à la couche de présentation.

### Ce qu'il reste — l'inventaire

`docs/migration/phase2/P2_INVENTAIRE_JS.md` classe les 233 modules du noyau JS en
**porté / partiellement porté / porter / générer / jeter**, avec leur vague et leur chantier.
Il se régénère, il ne s'édite pas, et seulement contre un checkout **propre** du tag de
référence (`docs/migration/phase3/REFERENCE_JS.md`) — il refuse une copie de travail modifiée :

```bash
node tools/migration/inventory-js-sim.mjs -ref <checkout de anastasis-ref-p3> -out docs/migration/phase2/P2_INVENTAIRE_JS.md
```

Ce qui est porté se déclare **fonction par fonction** dans `tools/migration/ported-functions.mjs`,
recopie des tableaux ci-dessus : une mission qui porte met à jour les deux dans le même commit.

Au 2026-10-01 (tag `anastasis-ref-p3` = `fee66ae`) : 9 modules portés, 31 partiellement portés
(17 379 lignes de code y restent), 166 à porter (41 342) — **58 721 lignes de code à porter** au
total, commentaires et lignes vides déduits — 4 tables à générer, 23 modules à ne pas porter.
L'inventaire du 2026-09-13 (234 modules, 63 492 lignes) avait été tiré de la copie de travail
modifiée : il comptait `sim/observability.js`, qui n'est pas dans `fee66ae`.

### Comment la parité se vérifiera au-delà des fonctions pures

Les vecteurs bit à bit ne montent pas jusqu'à `simulation.js` : on ne fabrique pas un
vecteur pour « le tick de minuit ». Au-delà de la couche 1, la preuve est un **harnais
différentiel** — même graine, une empreinte de l'état à chaque tick des deux côtés, et un
rapport qui nomme le premier tick divergent et la section fautive.

`docs/migration/phase2/P2_HARNAIS_DIFFERENTIEL.md`. L'empreinte C++
(`Core/AnastasisStateDigest.h`) est déjà prouvée identique à la spécification JS
(`Anastasis.Sim.Empreinte.*`) ; l'émetteur Unreal arrivera avec la couche 4, quand il y
aura un état à décrire.

### Suite proposée — dans cet ordre

**Depuis le 2026-10-01, la phase 3 fait autorité sur l'ordre** : `docs/migration/phase3/P3_PLAN.md`
(la preuve d'abord, puis la table de décision complète, puis les vagues 5 et 6). La liste ci-dessous
reste la carte des dépendances.

L'ordre suit les dépendances réelles, pas l'intérêt du gameplay. Chaque étape doit
arriver avec ses vecteurs de parité avant qu'on empile la suivante.

1. **Génération du monde** — portée (Phase 1). `Anastasis.Sim.Parite.Monde` PASS 2026-09-11. `Fbm` 1 vecteur ~2.5 ulp (voir `docs/migration/phase1/P1_HANDOFF.md`).
2. **Navigation** — terrain et A* portés (couche 2 ci-dessus). Restent `navService.js`,
   `crowdNav.js`, et les points d'accès une fois `urban/intent.js` porté.
3. **Budget et LOD logique** — noyau causal du budget porté (couche 3 ci-dessus).
   Reste `logicalLod.js`, qui agrège l'état du village et suivra ses systèmes.
4. **État du monde et sauvegarde** — `src/sim/save.js`, `world.js` (structures).
   La décision est prise : `docs/migration/phase2/P2_MODELE_DONNEES.md`. Tableau de
   structures et non SoA, table ordonnée à la sémantique JS (`World/AnastasisEntityTable.h`),
   références par identifiant, format de sauvegarde natif Unreal — le format JS est lu par
   le harnais, jamais écrit, et ne promet rien au joueur. `saveStore.js` n'est pas porté
   (`localStorage`).
5. **Boucle de simulation** — `src/sim/simulation.js` (8 663 lignes). À découper,
   pas à traduire d'un bloc.
6. **Vie et IA** — `src/life/*` (66 fichiers), `src/ai/*` (39 fichiers). La masse
   du contenu, mais la couche la moins piégeuse : c'est de la logique de haut
   niveau posée sur le socle ci-dessus.

Non porté volontairement pour l'instant : `src/runtime/faultShield.js`,
`flightRecorder.js`, `watchdog.js`, `crashReport.js`, `postMortemUi.js`. Ce sont
des filets de sécurité conçus autour de contraintes navigateur ; Unreal a ses
propres équivalents et le choix est à refaire, pas à traduire.

## Lancer les tests

L'éditeur doit être fermé (Live Coding verrouille les DLL) :

```bash
"C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" Anastasis_UnrealV2Editor Win64 Development -Project="C:\Users\alex_\OneDrive\Documents\Unreal Projects\Anastasis_UnrealV2\Anastasis_UnrealV2.uproject" -WaitMutex
```

```bash
"C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "C:\Users\alex_\OneDrive\Documents\Unreal Projects\Anastasis_UnrealV2\Anastasis_UnrealV2.uproject" -ExecCmds="Automation RunTests Anastasis.Sim.Parite; Quit" -unattended -nopause -nullrhi -nosplash -log
```
