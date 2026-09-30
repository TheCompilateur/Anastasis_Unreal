# HOUSE_REST_001 — la maison : le foyer, l'intérieur, la nuit

Branche `agent/house-rest-001`, base `main@984b0c7` (après FIRST_BUILDING_001). Date : 2026-09-29.

Deuxième bâtiment branché au simulateur, et le premier dans lequel on **entre**. Un habitant
rentre chez lui la nuit, y dort, et son énergie remonte selon la qualité de son lit :
1,12 dans son foyer, 0,78 dans un abri, 0,42 ailleurs ou dehors.

## Ce que la maison débloque

Le puits n'utilisait qu'une boucle dehors. La maison apporte trois mécanismes que les
prochains buts réutiliseront tels quels :

| Mécanisme | Référence JS | Réutilisé par |
|---|---|---|
| **Intérieur** : `enterBuilding` / `updateInside` / `exitBuilding`, `npc.inside`, branche intérieure de `tickNeeds` | `simulation.js:3974-4032`, `npc.js:3541-3565, 3588-3640` | manger, se soulager, se détendre, socialiser |
| **Foyer** : `npc.home`, `npc.shelter`, propriétaire, capacité, `assignSheltersDaily` à minuit | `life/domestic.js`, `simulation.js:4588-4599, 2408` | famille, héritage, achat de maison |
| **Rythme** : les phases du jour et `phaseBias` pour **tous** les buts | `life/villageRhythm.js:1-260` | toute la table de décision |

## Ce que le code JS a imposé

- **Le coucher dépend du rythme, pas de la fatigue seule.** `phaseBias(rest)` vaut environ +91 la
  nuit avec un toit et environ +69 sans ; `needs.rest` d'un habitant reposé vaut 0. Sans le rythme,
  personne ne dort. Le rythme est donc porté en entier, parité comprise.
- **Le rythme s'applique aussi à `drink`.** À midi, `drink` y gagne +69,6. Le plancher constant de
  FIRST_BUILDING_001 aurait fait boire tout le village en boucle à midi. Le plancher devient donc
  « 42 **+ le vrai `phaseBias`** du meilleur but non porté ». C'est le même principe, désormais
  cohérent pour toutes les lignes de la table. Conséquence mesurée : le seuil de soif vaut 40 la
  nuit, à l'aube et à midi ; il monte vers 64 le matin, quand le travail pèse 42 + 62.
- **L'entrée ne vérifie ni la capacité ni le propriétaire** (`buildingForIndoorAction`). La
  capacité ne compte qu'au moment de viser (`findOpenShelter`) et à l'attribution de minuit. Un
  quatrième sans-toit entre donc dans une maison de 3 places et y dort à 0,42. C'est la référence,
  et un test le prouve.
- **La durée se décide à l'entrée** : 11,5 s la nuit, 4,2 s de sieste le jour, sans prolongation.
  Un dormeur ressort, redécide et se recouche aussitôt tant qu'il fait nuit.
- **Le puits n'a pas d'état ; la maison en a un** : propriétaire, phase, occupants dérivés, ceux
  qui sont dedans.

## Fichiers

Créés :

| Fichier | Rôle |
|---|---|
| `Source/AnastasisSim/Public/Life/AnastasisVillageRhythm.h`, `Private/Life/AnastasisVillageRhythm.cpp` | phases, `villagePhase`, `isNightPhase`, `phaseBias` (tous les buts) |
| `Source/AnastasisSim/Private/Tests/AnastasisRhythmTests.cpp` + `AnastasisRhythmVectors.inl`, `AnastasisDomesticVectors.inl` (générés) | `Anastasis.Sim.Parite.Rythme` |
| `Source/AnastasisSim/Private/Tests/AnastasisVillageHouseTests.cpp` | `Anastasis.Sim.Village.Maison.*` (7 tests) |
| `tools/migration/parity/village-rhythm.mjs`, `tools/migration/parity/domestic.mjs` | déclarations de vecteurs |
| `tools/unreal/house-rest-pie.py` | preuve PIE pilotée par l'état de la simulation |
| `docs/unreal/HOUSE_REST_001.md`, `docs/unreal/handoffs/house-rest-001.md` | ce document, la fiche de passation |

Modifiés :

| Fichier | Changement |
|---|---|
| `Life/AnastasisNeeds.h/.cpp` | branche intérieure `rest` de `tickNeeds`, `satisfyRest`, `sleepQuality`, constantes `DOMESTIC` |
| `Village/AnastasisVillage.h/.cpp` | maison, foyer, abri, intérieur, table rest + drink + plancher rythmé, `AssignHome`, `AssignSheltersDaily`, `RemoveBuilding` étendu (dedans, foyer, abri) |
| `Sim/AnastasisSimulation.cpp` | `onNewDay` : `assignSheltersDaily` (section critique de minuit) |
| `Sim/AnastasisSimulationSubsystem.*` | `Anastasis.Village.FirstHouse`, `UAnastasisSimulationDebugLibrary` (lecteurs pour Python) |
| `Village/AnastasisVillagePresentation.cpp` | `house` → `Kind::House` ; debug : maison, occupants, dedans/dehors, énergie, pourquoi |
| `Private/Tests/AnastasisNeedsTests.cpp`, `tools/migration/parity/needs.mjs` | vecteurs `TickNeedsRest`, `SatisfyRest` |
| `Private/Tests/AnastasisVillageSimTests.cpp` | les tests du puits deviennent conscients de la phase |
| `Village/AnastasisFirstBuildingTests.cpp` | `Anastasis.Village.FirstBuilding.MaisonPresentation` |
| `AGENTS.md`, `Source/AnastasisSim/PORTAGE.md` | index d'outils ; état du portage |

## Flux

```
tick(dt) ─ onNewDay (minuit) ─ AssignSheltersDaily : les sans-toit reçoivent un abri
         └ UpdateActors
             dedans ?  TickNeedsRestInside(nuit, sleepQuality) ; à `until` : SatisfyRest, ExitBuilding
             dehors :  TickNeeds ; pensée -> ChooseGoal
                 phase = villagePhase(dayFrac)
                 rest  = needs.rest + 5,425 + phaseBias(rest)       [parité]
                 drink = needs.drink + 6 + phaseBias(drink)          [parité]
                 plancher = 42 + max phaseBias(but non porté)        [phaseBias en parité, 42 déclaré]
                 rest gagne -> RestTarget : foyer > abri > logement libre (nearestHousing)
                                             > camp ; puis couche domestique : foyer ou abri ouvert
             Act : A* + marche -> arrivée -> TryEnterIndoorAction -> EnterBuilding
                   (foyer inaccessible : attente à la porte, puis repos dehors ;
                    sans toit ni logement : repos dehors, qualité 0,42)
```

## Écarts déclarés

Ils sont écrits en tête de `AnastasisVillage.h`, numérotés de 1 à 8. Ceux qu'ajoute la maison :

- **(1)** Le plancher est rythmé. Les lignes n'ont ni `statusBias` (la misère lit l'or, qui
  n'existe pas encore), ni mode de vie, district, météo ou prévision de survie.
- **(2)** Il n'y a pas de collant de but (`goalStickinessBonus`).
- **(7)** Le foyer est minimal : pas de famille, pas d'achat de maison, pas d'agrandissement
  (phase 1, capacité 3), pas d'hospitalité, pas de dortoir. `explore` n'est pas porté : l'échec à
  la porte bascule sur `observer`.
- **(8)** Tous les habitants sont des adultes ni gardes, sans famille. `jobPriority(rest)` vaut la
  constante du catalogue.
- `RemoveNpc` libère la maison d'un propriétaire retiré : la référence le fait à la mort
  (`mortality.js`), sans quoi la maison resterait close.

La parité bit à bit couvre les besoins (495 vecteurs), le rythme et la qualité du lit
(649 vecteurs). La boucle assemblée est déterministe, mais ce n'est **pas** la trajectoire JS.

## Dette découverte, laissée hors périmètre

- **Le seuil de soif du puits a changé** : 40 la nuit, à l'aube et à midi, environ 64 le matin.
  C'est plus fidèle, mais un habitant assoiffé le matin attend désormais que le « travail »
  cède. FIRST_BUILDING_001 décrivait un seuil fixe à 40 ; ce n'est plus vrai.
- **Un habitant retiré pendant son sommeil** disparaît de l'intérieur sans passer par
  `exitBuilding`. La maison ne garde rien, il n'y a donc pas de référence morte, mais aucun
  événement de sortie n'est émis.
- **Pas de verrou de seuil en route** (`stableBuildingAccess`) : la cible n'est pas relockée pendant
  la marche.
- **Présentation** : un habitant dedans reste dessiné à son seuil d'entrée (petite sphère bleue).
  Le Smart Object `Activity.Sleep` de `main` a deux emplacements à 80 uu du centre, sans rapport
  avec la capacité de 3 ni avec les seuils de la simulation.
- **Preuve PIE** : le premier PIE d'un worktree neuf compile des shaders et le temps simulé gèle
  plusieurs secondes. D'où un script piloté par l'état de la simulation
  (`UAnastasisSimulationDebugLibrary`), jamais par l'horloge murale.
