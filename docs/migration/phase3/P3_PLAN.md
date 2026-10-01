# Phase 3 — Fermer la table, puis les vagues

Ouverte le 2026-10-01 sur décision d'Alexandre. Stratégie retenue : **d'abord la preuve et la
table de décision complète, ensuite le reste de la simulation par vagues**, au lieu de continuer
à empiler des tranches verticales.

Discipline de claim (celle de `P2_RAPPORT_MISSION_001.md`) : `OBS` observé · `EVD` mesuré ·
`INF` déduit · `DEC` décidé · `UNK` inconnu.

---

## 1. Pourquoi changer de méthode

**Où on en est `EVD` (main `d31a356`).**

| | JS (`P2_INVENTAIRE_JS.md`, 2026-09-13) | C++ `Source/AnastasisSim` |
|---|---:|---:|
| Code de simulation, sans commentaires ni lignes vides | 70 484 (dont 63 492 à porter) | ~9 500 hors tests |
| Lignes calculées de `adultScores` | 25 | 9 |

Les neuf lignes calculées sont `eat`, `rest`, `drink`, `relax`, `socialize`, `shelterRain`, `build`,
et `gatherFood` / `deliver` pour un fermier seulement. Les 16 autres valent
`UnportedGoalsFloor` (42) + `phaseBias`.

**Ce que les tranches verticales ont apporté `OBS`.** Neuf missions (puits → chantier) ont porté
des fonctions en parité bit à bit, avec plus de 11 000 vecteurs. Un village tourne dans Unreal et tient
douze jours.

**Ce qu'elles ne peuvent pas apporter `INF`.** Chaque tranche ajoute des écarts déclarés
(`Village/AnastasisVillage.h`, n° 1 à 18). Tant que ces écarts restent, **le harnais
différentiel ne peut juger aucune boucle** : la table est réduite, `goalNoise` ne consomme pas
`sim.rng` dans l'ordre de la référence, les rumeurs tirent sur un flux propre au village, et le
pilotage est réduit. Les fonctions sont prouvées, la simulation ne l'est pas. Plus on empile,
plus le rattrapage coûtera.

**Le but de la phase `DEC`.** Arriver au point où l'on peut écrire :

> Même graine, mêmes fondateurs, N jours : les empreintes JS et Unreal sont identiques à chaque tick.

Toutes les missions ci-dessous sont ordonnées pour s'en rapprocher de façon mesurable.

---

## 2. L'instrument de mesure de la phase

Le harnais (`P2_HARNAIS_DIFFERENTIEL.md`) existe côté JS, et l'empreinte C++
(`Core/AnastasisStateDigest.h`) est déjà prouvée identique à sa spécification. Il manque trois
choses, qui forment le jalon A :

1. **Un état de départ commun.** La référence démarre avec les fondateurs romains, les sagas,
   l'économie et Kosmos. Le C++ ne sait pas encore construire cet état. La sortie est la
   décision 2 de `P2_MODELE_DONNEES.md` : **le harnais lit l'état JS**. Un *scénario* est une
   sauvegarde JS (`serialize`) ; les deux côtés la chargent puis tournent.
2. **Un émetteur Unreal**, qui projette l'état C++ sur les sections de `serialize` et écrit la
   même trace JSONL que `emit-state-digests.mjs`.
3. **Un périmètre déclaré.** Chaque scénario dit quelles sections sont comparées et quels
   systèmes JS sont masqués (pas encore portés). `compare-digests.mjs` refuse de conclure sur
   une section hors périmètre.

**Indicateur de phase.** Pour chaque scénario : nombre de jours identiques, nombre de sections
comparées, nombre de systèmes masqués. Une mission ne se clôt que si elle **retire au moins un
masque ou ajoute au moins une section**, sans faire reculer les autres scénarios.

Les vecteurs bit à bit (`gen-parity.mjs`, `parity-kit.mjs`) restent la preuve au grain de la
fonction. Le harnais ne les remplace pas, il juge l'assemblage.

---

## 3. Jalon A — la preuve d'abord

Rien n'est porté dans ce jalon : on construit l'instrument qui jugera les jalons B à D.

| Mission | Contenu | Fin quand |
|---|---|---|
| `js-ref-pin-001` | Épingler la référence JS : le commit utilisé (`fee66ae` ou plus récent) est **poussé sur GitHub** et inscrit dans `docs/migration/phase3/REFERENCE_JS.md`. Régénérer `P2_INVENTAIRE_JS.md` contre lui, et passer en « déjà porté » les modules des tranches verticales (l'inventaire en compte encore 8). | le commit est joignable depuis un clone frais ; l'inventaire dit vrai |
| `sim-scenario-001` | Côté JS, dans `tools/migration/` : fabriquer des scénarios (graine + monde + bâtiments + habitants), les écrire par `serialize`, et masquer de l'extérieur les systèmes non portés (méthodes de `sim` remplacées par l'émetteur, jamais le dépôt JS modifié). Premier scénario : le village de l'endurance (puits, maison, grenier, fermiers). `compare-digests.mjs` gagne `-sections`. | `selftest-harness.mjs` vert avec masques ; le JS contre lui-même donne 0 divergence |
| `sim-state-reader-001` | Vague 4 : lecteur C++ du format JS, `World/AnastasisEntityTable.h` comme socle. Champs nécessaires aux sections du premier scénario seulement : graine, état `rng`, horloge, `tileDiff`, bâtiments, habitants (sous-ensemble des 151 champs), `mealReservations`. | `Anastasis.Sim.Harnais.Lecture` : JS `serialize` → C++ → projection → empreinte identique **au tick 0** |
| `sim-digest-emitter-001` | Émetteur Unreal : projection de l'état C++ (`FAnastasisVillage` + `FAnastasisSimulation`) sur les sections de `serialize`, trace JSONL au format JS, lancée par un test d'automation ou un commandlet. | trace C++ du premier scénario lisible par `compare-digests.mjs` ; **premier rapport réel** de premier tick divergent, archivé |
| `nav-service-001` | Vagues 2 et 3 restantes : `navService.js` (file, budget A* par tick, cache — qui figure dans la sauvegarde), `crowdNav.js`, `separateCrowdedActors`, `moveActor` complet, `logicalLod.js`. Ferme les écarts n° 4 et 5. Sans ce socle, la section `actors` diverge dès le premier pas et masque tout le reste. | section `actors` identique sur un scénario sans décision (habitants envoyés vers des cibles fixes) |

Ordre : `js-ref-pin-001`, puis `sim-scenario-001` et `sim-state-reader-001` en parallèle, puis
`sim-digest-emitter-001`. `nav-service-001` peut démarrer dès `js-ref-pin-001`.

`DEC` à prendre par Alexandre, déjà posé dans `P2_MODELE_DONNEES.md` : existe-t-il une
sauvegarde JS qui compte ? Si oui, le lecteur devient un livrable et couvre les 151 champs dès
`sim-state-reader-001`.

---

## 4. Jalon B — fermer la table de décision

But : les 25 lignes d'`adultScores` calculées pour tous les habitants, et la décision qui
consomme `sim.rng` dans l'ordre de la référence. Les écarts n° 1, 2, 9, 10 et 11 se ferment ici.

| Mission | Lignes et mécanismes | Domaine JS | Dépend de |
|---|---|---|---|
| `goal-noise-001` | `goalNoise` sur chaque ligne (même non portée : le tirage compte, pas la valeur), reconsidération aléatoire, `goalStickinessBonus`, `noteGoalFailure`. **Un seul flux `sim.rng`** : les rumeurs et le reste du village cessent de tirer sur un flux propre (écart n° 16). | `npc.js`, `ai/*` | jalon A |
| `goals-body-001` | `relieve`, `eatTogether`, `hearthInviteScore` (scène de foyer, aujourd'hui 0) | `life/needs.js`, `life/domestic.js` | `goal-noise-001` |
| `goals-resources-001` | `gatherWood`, `gatherStone`, `helpFarm`, `explore` (`exploreTarget`), `rollCraftMiss`, gather pour les sans-métier | `npc.js`, `craftWork.js`, `fieldWorkPosts.js` | `goal-noise-001` |
| `economy-gold-001` | L'or et le marché : `sell`, `buy` (`buy_food` de Noûs), `statusBias` (misère), `market.stock` agrégé, `believedStock` du marché | chantier « économie et travail » (3 656 lignes) | `goals-resources-001` |
| `build-002` | Déjà annoncé par build-001 : ouverture de chantier par les habitants, livraisons de bois et de pierre (écart n° 18) | `simulation.js`, `urban/*` | `goals-resources-001` |
| `goals-work-001` | `craft`, `maintain`, `fetchInput`, `closeWorkplace`, marché de l'emploi (embauche au-delà de `AssignWorkplace`) | `craftWork.js`, `metiers/*` | `economy-gold-001` |
| `goals-haul-001` | `haulJob` et la file de transport | chantier « transport et logistique » (3 350 lignes) | `goals-work-001` |
| `goals-family-001` | `aidHousehold`, `visitFamily`, `confront`. **Tire en avance** la partie « personne et famille » de la vague 6 (foyers complets, liens de parenté) et les frictions (`updateConflictsDaily`). | `life/*` | `goal-noise-001` |

**Fin du jalon `DEC`.** `UnportedGoalsFloor` disparaît du code. Le scénario « endurance » et un
scénario « fondateurs, 1 jour » passent le harnais sans masque sur la décision.

Les missions de ce jalon gardent la forme des tranches verticales (une fiche, des vecteurs, un
banc PIE si visible). Ce qui change : **chacune doit faire avancer l'indicateur du §2**.

---

## 5. Jalon C — vague 5, la boucle de simulation

`simulation.js` (8 663 lignes) se découpe selon la structure qu'il a déjà `OBS` :

| Mission | Partie de `simulation.js` et modules liés | Taille JS (code) |
|---|---|---|
| `day-critical-001` | `onNewDay` critique : production, gâchis, exports, entretien, loyers, achats et agrandissements de maisons, dividendes | noyau de boucle (14 013, partagé) |
| `day-deferred-001` | Les 15 travaux de minuit encore vides (`collective`, `socialOrders`, `colonyDoctrine`, `growthChapter`, `founderCharter`, `colonySites`, `transportProjects`, `roadEvolution`, `watchPosts`, `lifeDaily`, `careers`, `founders`, `romanCouncils`, `animalsDaily`, …) — une mission par groupe cohérent | société et institutions (6 629) |
| `urban-001` | `urban/intent.js` et urbanisme ; ferme l'écart n° 3 (points d'accès) | 3 954 |
| `animals-001` | `updateAnimalsTick`, `animalState` | 1 031 |
| `transport-001` | `tickTransport`, charrettes, porteurs (si `goals-haul-001` ne l'a pas déjà tout pris) | reste de 3 350 |
| `chronicle-001` | chronique et mémoire collective, journal du village (`logs`) | 2 483 |
| `sagas-kosmos-001` | `tickSagas`, Kosmos (`resolveUneBouchePlus`, transitions) | à mesurer |

Chaque mission retire les masques correspondants des scénarios. **Fin du jalon** : le scénario
« fondateurs » tient un jour sans aucun masque de système de la vague 5.

---

## 6. Jalon D — vague 6, le reste de la vie

Cognition (7 281), parole et narration (4 547, dont le générateur de répliques, écart n° 16),
rites et culture (4 072), langue (2 293), le reste de « personne et famille » non tiré par
`goals-family-001`. Le découpage fin se fera à l'ouverture du jalon, sur l'inventaire
régénéré : ces modules auront changé d'ici là.

**Fin de la phase `DEC`** : scénario « fondateurs », graine 33344, 3 jours, aucun masque,
toutes les sections de `serialize` comparées, 0 divergence.

---

## 7. Règles de la phase

- **Ordre des dépendances, pas de l'intérêt gameplay** (`PORTAGE.md`). Une tranche visible qui
  ne fait pas avancer l'indicateur attend la fin du jalon B.
- **Un module JS porté l'est en entier**, ou sa fiche dit quelles fonctions restent et pourquoi.
  Fini le « réduit » silencieux.
- **Tout écart se déclare dans le commit qui l'introduit** : fiche dans
  `Source/AnastasisSim/ECARTS.md`, marque `ecart n°N` dans le code, section `## ECARTS` de la
  passation. `finish` le contrôle (`docs/migration/PROTOCOLE_ECARTS.md`).
- **Aucun nouveau flux aléatoire.** Tout tirage passe par `sim.rng` dans l'ordre de la
  référence ; un écart de tirage est une divergence, pas un détail.
- **Les vecteurs ne se corrigent jamais à la main**, et les masques de scénario ne s'ajoutent
  jamais pour faire passer un rapport : un masque se retire, il ne s'ajoute qu'à la création du
  scénario.
- **La référence JS est lue, jamais écrite.** Les scénarios et les masques vivent dans
  `tools/migration/` de ce dépôt.
- Cycle de mission inchangé : skill `anastasis-mission`, fiche dans `docs/unreal/handoffs/`,
  `finish` → `HANDOFF_READY::YES`, PASS / KNOWN_EXPECTED_FAILURE / FAIL séparés.

## 8. Ce qui peut se faire hors du poste Windows

`INF` Un agent cloud (Linux, sans Unreal) peut faire la partie JS : scénarios, masques,
générateurs de vecteurs, régénération de l'inventaire. Il ne peut ni compiler ni lancer les
tests C++. Condition : la référence JS épinglée doit être sur GitHub (`js-ref-pin-001`). Au
2026-10-01, `TheCompilateur/Jeux-IV-Kingdoms` s'arrête à `f461eda` (2026-08-27) : `fee66ae`,
la référence de tout le portage, n'y est pas `OBS`.

## 9. Ce que ce plan ne revendique pas

- Les tailles par mission sont celles des chantiers de l'inventaire du 2026-09-13, pas une
  mesure par mission.
- Que l'émetteur JS puisse masquer **tous** les systèmes de l'extérieur n'est pas vérifié : les
  fonctions importées par `simulation.js` (`updateAnimalsTick`, `tickTransport`…) ne se
  remplacent pas comme des méthodes. `sim-scenario-001` doit le trancher, et dire quels
  systèmes ne se masquent qu'en vidant leur état (pas d'animaux, pas de transport).
- Aucune estimation de durée.
