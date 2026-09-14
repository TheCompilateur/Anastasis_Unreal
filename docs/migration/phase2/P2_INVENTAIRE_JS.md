# Inventaire du simulateur JS — porter / generer / jeter

**Genere. Ne pas editer a la main.**

```bash
node tools/migration/inventory-js-sim.mjs -out docs/migration/phase2/P2_INVENTAIRE_JS.md
```

Reference : `C:/dev/Jeux IV Kingdoms` — HEAD `fee66ae 2026-08-30`  
Perimetre : `src/sim/`, `src/life/`, `src/ai/`, `src/lang/`, `src/runtime/` — le rendu, l'UI, l'audio et le debug sont hors sujet par decision (AGENTS.md).

Le portage ne se mesure pas en lignes de JS. Une table de contenu devient une table de
donnees, pas du C++ ecrit a la main ; un filet de securite navigateur se re-decide dans
Unreal, il ne se traduit pas. Ce document dit, fichier par fichier, dans quel seau il tombe.

## Ce qu'il y a devant

| | fichiers | lignes | dont code |
| --- | ---: | ---: | ---: |
| **A porter** | 198 | 79659 | 63492 |
| **A generer** (donnees) | 4 | 2101 | 1278 |
| **A jeter** | 24 | 5055 | 3909 |
| Deja porte | 8 | 2347 | 1805 |
| **Total** | 234 | 89162 | 70484 |

Sur les 79659 lignes a porter, 8731 sont du commentaire et
5556 des lignes vides : **63492 lignes de code** portent la simulation.
12 de ces modules melangent logique et table de contenu : la table s'extrait, le selecteur se porte.

## Reste a porter, par vague

L'ordre est celui de `Source/AnastasisSim/PORTAGE.md` — il suit les dependances reelles,
pas l'interet du gameplay.

| Vague | fichiers | lignes de code |
| --- | ---: | ---: |
| 2 — navigation | 9 | 1491 |
| 3 — budget et LOD logique | 2 | 394 |
| 4 — etat du monde et sauvegarde | 3 | 778 |
| 5 — boucle de simulation | 80 | 35116 |
| 6 — vie, IA, langue | 104 | 25713 |

Les vagues 5 et 6 ne sont pas des vagues, ce sont des marecages : 184 modules a
elles deux. Elles se decoupent en chantiers, et c'est a ce grain qu'un module se confie.

| Vague | Chantier | fichiers | lignes de code | plus gros module |
| --- | --- | ---: | ---: | --- |
| 2 | navigation | 9 | 1491 | `sim/navService.js` (391) |
| 3 | budget et LOD | 2 | 394 | `sim/logicalLod.js` (228) |
| 4 | etat et sauvegarde | 3 | 778 | `sim/save.js` (656) |
| 5 | noyau de boucle | 9 | 14013 | `sim/simulation.js` (7196) |
| 5 | societe et institutions | 8 | 6629 | `sim/collectivePriorities.js` (3035) |
| 5 | urbanisme | 14 | 3954 | `sim/urban/intent.js` (489) |
| 5 | economie et travail | 18 | 3656 | `sim/craftWork.js` (964) |
| 5 | transport et logistique | 13 | 3350 | `sim/transport/delivery.js` (770) |
| 5 | chronique et memoire collective | 11 | 2483 | `sim/villageChronicle.js` (414) |
| 5 | regne animal | 7 | 1031 | `sim/animaux/updateAnimals.js` (465) |
| 6 | personne et famille | 33 | 7520 | `life/lifeScenes.js` (818) |
| 6 | cognition | 33 | 7281 | `ai/memory.js` (873) |
| 6 | parole et narration | 15 | 4547 | `life/talk.js` (1887) |
| 6 | rites et culture | 14 | 4072 | `life/kosmos1204UneBouchePlus.js` (640) |
| 6 | langue | 9 | 2293 | `lang/lexicon.js` (646) |

## Les regles

Elles s'appliquent dans cet ordre, la premiere qui match gagne.

1. **Deja porte** — inscrit dans `PORTAGE.md`, couches 0 et 1.
2. **Jeter / navigateur** — filets de securite et amarres DOM. `PORTAGE.md` pose deja la regle :
   Unreal a ses propres equivalents, *le choix est a refaire, pas a traduire*.
3. **Jeter / presentation** — lit la simulation pour la raconter a une UI qui n'existera pas
   sous cette forme. Refait contre le HUD Unreal.
4. **Jeter / baril** — `index.js` de re-export : sans objet en C++.
5. **Jeter / non atteint** — aucun chemin depuis `src/main.js`. A brancher ou a enterrer,
   decision de conception, pas de portage.
6. **Generer** — table de contenu. Ajouter un batiment, une espece ou une replique ne doit
   jamais demander une recompilation.
7. **Porter** — le reste. Marque *scinder* quand une table de contenu y est melee.

## Ce que cet inventaire ne sait pas

- Les verdicts venus d'une liste explicite sont des **decisions**, pas des mesures. Elles
  sont dans `tools/migration/inventory-js-sim.mjs`, chacune avec sa raison, et se discutent.
- `code` compte les lignes de noms d'un baril de re-export : le total de la colonne **A jeter**
  est surevalue d'environ 800 lignes pour cette raison. Sans consequence, on les jette.
- La part de litteraux rate les gabarits multi-lignes. Un module peut porter plus de contenu
  que la colonne `txt` ne le dit — le seuil *scinder* est un plancher, pas un plafond.
- **Non atteint depuis `main.js`** ne veut pas dire mort. `sim/villageSpectrum.js` (partition
  spectrale du village par vecteur de Fiedler) est ecrit, documente, et branche nulle part :
  c'est une decision de conception en attente, pas un dechet.
- Deux ports sont partiels et le tableau ne le dit pas : `worldArchetypes.js` n'a livre que ses
  reglages de simulation (air, foret, garde-robe restent a la presentation), et `world.js`
  garde un vecteur `Fbm` divergent d'environ 2,5 ulp. Voir `PORTAGE.md`.

## Le detail

`code` exclut commentaires, lignes vides et litteraux de texte. `txt` est le nombre de lignes
qui ne sont qu'un litteral. `imp` = nombre de modules du noyau qui importent celui-ci.

| Verdict | Module | lignes | code | txt | imp | Vague | Chantier | Note |
| --- | --- | ---: | ---: | ---: | ---: | --- | --- | --- |
| porter | `sim/navService.js` | 511 | 391 | 3 | 2 | 2 | navigation |  |
| porter | `sim/navGrid.js` | 435 | 364 | 2 | 6 | 2 | navigation |  |
| porter | `sim/crowdNav.js` | 280 | 228 | 3 | 1 | 2 | navigation |  |
| porter | `sim/pathfinding.js` | 212 | 179 | 0 | 3 | 2 | navigation |  |
| porter | `sim/percolation.js` | 205 | 127 | 0 | 1 | 2 | navigation |  |
| porter | `sim/trafficDecay.js` | 106 | 73 | 0 | 1 | 2 | navigation |  |
| porter | `sim/destination.js` | 85 | 50 | 0 | 1 | 2 | navigation |  |
| porter | `sim/landRoads.js` | 80 | 44 | 0 | 2 | 2 | navigation |  |
| porter | `sim/landExtent.js` | 58 | 35 | 0 | 2 | 2 | navigation |  |
| porter | `sim/logicalLod.js` | 253 | 228 | 0 | 1 | 3 | budget et LOD |  |
| porter | `sim/simulationBudget.js` | 275 | 166 | 0 | 2 | 3 | budget et LOD |  |
| porter | `sim/save.js` | 829 | 656 | 2 | 0 | 4 | etat et sauvegarde |  |
| porter | `sim/worldChange.js` | 107 | 83 | 0 | 0 | 4 | etat et sauvegarde |  |
| porter | `sim/pristineWorld.js` | 56 | 39 | 0 | 2 | 4 | etat et sauvegarde |  |
| porter | `sim/simulation.js` | 8664 | 7196 | 16 | 1 | 5 | noyau de boucle |  |
| porter | `sim/npc.js` | 6070 | 5169 | 41 | 3 | 5 | noyau de boucle |  |
| porter | `sim/collectivePriorities.js` | 3847 | 3035 | 24 | 18 | 5 | societe et institutions |  |
| porter | `sim/craftWork.js` | 1133 | 964 | 0 | 4 | 5 | economie et travail |  |
| porter | `sim/colonizationDoctrine.js` | 1018 | 799 | 4 | 8 | 5 | societe et institutions |  |
| porter | `sim/transport/delivery.js` | 875 | 770 | 2 | 12 | 5 | transport et logistique |  |
| porter | `sim/socialOrders.js` | 912 | 749 | 38 | 11 | 5 | societe et institutions |  |
| porter | `sim/jobMarket.js` | 828 | 680 | 2 | 3 | 5 | economie et travail |  |
| porter | `sim/socialAbduction.js` | 649 | 543 | 38 | 4 | 5 | societe et institutions |  |
| porter | `sim/life.js` | 656 | 540 | 18 | 2 | 5 | noyau de boucle |  |
| porter | `sim/oecumene.js` | 624 | 509 | 37 | 8 | 5 | societe et institutions |  |
| porter | `sim/urban/intent.js` | 554 | 489 | 0 | 3 | 5 | urbanisme |  |
| porter | `sim/animaux/updateAnimals.js` | 594 | 465 | 0 | 4 | 5 | regne animal |  |
| porter | `sim/transport/carts.js` | 520 | 449 | 1 | 3 | 5 | transport et logistique |  |
| porter | `sim/transportProjects.js` | 535 | 437 | 6 | 2 | 5 | urbanisme |  |
| porter | `sim/urban/taxonomy.js` | 502 | 421 | 17 | 8 | 5 | urbanisme |  |
| porter | `sim/cohesionClimate.js` | 516 | 414 | 29 | 5 | 5 | societe et institutions |  |
| porter | `sim/villageChronicle.js` | 516 | 414 | 21 | 4 | 5 | chronique et memoire collective |  |
| porter | `sim/transport/stockLedger.js` | 498 | 395 | 7 | 18 | 5 | transport et logistique |  |
| porter | `sim/clioscopeSensitivity.js` | 466 | 389 | 21 | 1 | 5 | chronique et memoire collective |  |
| porter | `sim/founderCharter.js` | 517 | 381 | 5 | 6 | 5 | urbanisme |  |
| porter | `sim/colonySite.js` | 451 | 363 | 10 | 5 | 5 | urbanisme |  |
| porter | `sim/socialCycles.js` | 447 | 361 | 17 | 7 | 5 | societe et institutions |  |
| porter | `sim/transport/congestion.js` | 394 | 327 | 7 | 3 | 5 | transport et logistique |  |
| porter + scinder | `sim/romanChronicle.js` | 456 | 322 | 99 | 7 | 5 | chronique et memoire collective | formules de chronique + declenchement |
| porter | `sim/sagas.js` | 429 | 312 | 2 | 3 | 5 | chronique et memoire collective |  |
| porter | `sim/urban/terrainMorphology.js` | 346 | 310 | 0 | 3 | 5 | urbanisme |  |
| porter | `sim/urban/potentialField.js` | 346 | 301 | 0 | 2 | 5 | urbanisme |  |
| porter | `sim/transport/diagnostics.js` | 318 | 284 | 0 | 1 | 5 | transport et logistique |  |
| porter | `sim/economyLoopMetrics.js` | 342 | 279 | 5 | 1 | 5 | economie et travail |  |
| porter | `sim/weather.js` | 357 | 268 | 0 | 2 | 5 | noyau de boucle |  |
| porter | `sim/transport/index.js` | 302 | 267 | 0 | 7 | 5 | transport et logistique |  |
| porter | `sim/colonyStockReport.js` | 305 | 254 | 0 | 5 | 5 | economie et travail |  |
| porter | `sim/urban/districtBehavior.js` | 294 | 249 | 2 | 3 | 5 | urbanisme |  |
| porter | `sim/settlementSite.js` | 296 | 247 | 0 | 2 | 5 | urbanisme |  |
| porter | `sim/urban/cadastre.js` | 313 | 244 | 1 | 4 | 5 | urbanisme |  |
| porter | `sim/clioscope.js` | 279 | 234 | 2 | 4 | 5 | chronique et memoire collective |  |
| porter | `sim/transport/porters.js` | 322 | 234 | 0 | 1 | 5 | transport et logistique |  |
| porter | `sim/constructionPipeline.js` | 316 | 232 | 1 | 2 | 5 | economie et travail |  |
| porter | `sim/clioscopeScenarios.js` | 288 | 231 | 25 | 3 | 5 | chronique et memoire collective |  |
| porter | `sim/eraClimateBridge.js` | 287 | 226 | 9 | 4 | 5 | chronique et memoire collective |  |
| porter | `sim/collectivePulse.js` | 293 | 219 | 26 | 1 | 5 | societe et institutions |  |
| porter | `sim/playerGenesis.js` | 254 | 218 | 11 | 0 | 5 | noyau de boucle |  |
| porter | `sim/lifestyle.js` | 249 | 216 | 9 | 4 | 5 | noyau de boucle |  |
| porter | `sim/transport/haulThroughput.js` | 266 | 202 | 0 | 4 | 5 | transport et logistique |  |
| porter | `sim/forestSustain.js` | 257 | 182 | 0 | 5 | 5 | economie et travail |  |
| porter | `sim/animaux/livestockEconomy.js` | 213 | 177 | 0 | 1 | 5 | regne animal |  |
| porter | `sim/watchPosts.js` | 208 | 174 | 0 | 2 | 5 | urbanisme |  |
| porter | `sim/content.js` | 261 | 171 | 0 | 24 | 5 | noyau de boucle |  |
| porter | `sim/resourceRelay.js` | 203 | 171 | 2 | 4 | 5 | economie et travail |  |
| porter | `sim/craftHandoff.js` | 215 | 168 | 1 | 1 | 5 | economie et travail |  |
| porter | `sim/clioscopeBatch.js` | 195 | 158 | 7 | 1 | 5 | chronique et memoire collective |  |
| porter | `sim/transport/orphanReserve.js` | 227 | 158 | 2 | 3 | 5 | transport et logistique |  |
| porter | `sim/animaux/hunting.js` | 213 | 155 | 0 | 3 | 5 | regne animal |  |
| porter | `sim/eventPrimitives.js` | 165 | 146 | 0 | 1 | 5 | noyau de boucle |  |
| porter | `sim/craftPairHelp.js` | 174 | 137 | 0 | 1 | 5 | economie et travail |  |
| porter | `sim/growthChapter.js` | 184 | 134 | 9 | 4 | 5 | chronique et memoire collective |  |
| porter | `sim/villageCrown.js` | 163 | 114 | 0 | 2 | 5 | urbanisme |  |
| porter | `sim/animaux/seedAnimals.js` | 139 | 113 | 0 | 1 | 5 | regne animal |  |
| porter | `sim/economy.js` | 145 | 112 | 0 | 3 | 5 | economie et travail |  |
| porter | `sim/landWaterPresets.js` | 151 | 112 | 1 | 1 | 5 | urbanisme |  |
| porter | `sim/urban/districtLoyalty.js` | 134 | 112 | 0 | 3 | 5 | urbanisme |  |
| porter | `sim/fieldWorkPosts.js` | 127 | 104 | 0 | 2 | 5 | economie et travail |  |
| porter | `sim/transport/spoilage.js` | 131 | 101 | 0 | 2 | 5 | transport et logistique |  |
| porter | `sim/transport/haulWatchdog.js` | 123 | 91 | 3 | 1 | 5 | transport et logistique |  |
| porter | `sim/decisionProvider.js` | 183 | 89 | 1 | 1 | 5 | noyau de boucle |  |
| porter | `sim/craftMiss.js` | 115 | 78 | 0 | 1 | 5 | economie et travail |  |
| porter | `sim/craftToolSwitch.js` | 96 | 65 | 0 | 1 | 5 | economie et travail |  |
| porter | `sim/constructionPieces.js` | 77 | 62 | 0 | 1 | 5 | economie et travail |  |
| porter | `sim/chronicleKindBias.js` | 77 | 57 | 0 | 2 | 5 | chronique et memoire collective |  |
| porter | `sim/animaux/createAnimal.js` | 87 | 55 | 0 | 3 | 5 | regne animal |  |
| porter | `sim/transport/reserveReconcile.js` | 85 | 51 | 0 | 1 | 5 | transport et logistique |  |
| porter | `sim/metiers/extractionPost.js` | 115 | 44 | 0 | 2 | 5 | economie et travail |  |
| porter | `sim/craftFatigue.js` | 71 | 43 | 0 | 2 | 5 | economie et travail |  |
| porter | `sim/metiers/porterJob.js` | 119 | 43 | 0 | 3 | 5 | economie et travail |  |
| porter | `sim/animaux/landFauna.js` | 77 | 42 | 0 | 3 | 5 | regne animal |  |
| porter | `sim/haulLoad.js` | 65 | 38 | 0 | 2 | 5 | economie et travail |  |
| porter | `sim/animaux/animalDeath.js` | 42 | 24 | 0 | 4 | 5 | regne animal |  |
| porter | `sim/transport/roadTraffic.js` | 25 | 21 | 0 | 2 | 5 | transport et logistique |  |
| porter | `sim/valmireSignatures.js` | 17 | 6 | 0 | 2 | 5 | chronique et memoire collective |  |
| porter | `life/talk.js` | 2220 | 1887 | 0 | 4 | 6 | parole et narration |  |
| porter | `ai/memory.js` | 1050 | 873 | 14 | 6 | 6 | cognition |  |
| porter | `life/lifeScenes.js` | 949 | 818 | 2 | 6 | 6 | personne et famille |  |
| porter | `ai/ambitions.js` | 835 | 685 | 53 | 5 | 6 | cognition |  |
| porter | `lang/lexicon.js` | 817 | 646 | 0 | 2 | 6 | langue |  |
| porter | `life/kosmos1204UneBouchePlus.js` | 736 | 640 | 39 | 1 | 6 | rites et culture |  |
| porter + scinder | `ai/episodes.js` | 963 | 636 | 146 | 17 | 6 | cognition | gabarits d'episodes + machine a etats |
| porter | `life/techniques.js` | 684 | 554 | 15 | 4 | 6 | personne et famille |  |
| porter | `life/speechActs.js` | 613 | 553 | 21 | 5 | 6 | parole et narration |  |
| porter + scinder | `life/followVignette.js` | 740 | 480 | 139 | 2 | 6 | parole et narration | vignettes + composition |
| porter | `ai/householdPlan.js` | 529 | 469 | 11 | 6 | 6 | cognition |  |
| porter | `life/needs.js` | 702 | 466 | 0 | 21 | 6 | personne et famille |  |
| porter | `life/nature.js` | 526 | 464 | 1 | 8 | 6 | personne et famille |  |
| porter | `life/orthodoxBaptism1204.js` | 490 | 441 | 11 | 3 | 6 | rites et culture |  |
| porter | `ai/socialMemory.js` | 515 | 407 | 1 | 10 | 6 | cognition |  |
| porter | `life/orthodoxMarriage1204.js` | 423 | 378 | 7 | 3 | 6 | rites et culture |  |
| porter | `ai/algorithmic/mealReservation.js` | 439 | 377 | 0 | 7 | 6 | cognition |  |
| porter | `ai/socialCognition.js` | 550 | 366 | 6 | 3 | 6 | cognition |  |
| porter | `life/orthodoxFunerary1204.js` | 410 | 366 | 5 | 2 | 6 | rites et culture |  |
| porter + scinder | `life/colonySeals.js` | 528 | 362 | 123 | 7 | 6 | rites et culture | sceaux + conditions |
| porter | `life/domestic.js` | 434 | 361 | 1 | 8 | 6 | personne et famille |  |
| porter | `life/witnessMemory.js` | 473 | 356 | 49 | 2 | 6 | parole et narration |  |
| porter | `life/careers.js` | 421 | 351 | 6 | 3 | 6 | personne et famille |  |
| porter | `lang/phonology.js` | 472 | 350 | 0 | 7 | 6 | langue |  |
| porter | `life/culture.js` | 449 | 350 | 16 | 15 | 6 | rites et culture |  |
| porter | `life/villageRhythm.js` | 412 | 343 | 5 | 10 | 6 | personne et famille |  |
| porter | `lang/decode.js` | 445 | 340 | 0 | 1 | 6 | langue |  |
| porter + scinder | `life/names.js` | 441 | 340 | 10 | 5 | 6 | personne et famille | pools onomastiques + selection |
| porter | `lang/generate.js` | 422 | 335 | 1 | 3 | 6 | langue |  |
| porter | `life/romanCouncils.js` | 372 | 330 | 5 | 1 | 6 | rites et culture |  |
| porter | `life/bonds.js` | 422 | 328 | 0 | 6 | 6 | personne et famille |  |
| porter | `ai/dayIntent.js` | 378 | 319 | 13 | 5 | 6 | cognition |  |
| porter | `life/orthodoxParish1204.js` | 368 | 314 | 10 | 4 | 6 | rites et culture |  |
| porter | `life/momentTalk.js` | 412 | 313 | 50 | 5 | 6 | parole et narration |  |
| porter | `life/lineage.js` | 355 | 302 | 4 | 4 | 6 | personne et famille |  |
| porter | `life/socialGesture.js` | 351 | 302 | 0 | 7 | 6 | personne et famille |  |
| porter | `life/culturalMemory.js` | 351 | 298 | 24 | 3 | 6 | rites et culture |  |
| porter | `life/standing.js` | 324 | 266 | 1 | 3 | 6 | personne et famille |  |
| porter | `life/identityFactions.js` | 290 | 256 | 8 | 5 | 6 | personne et famille |  |
| porter | `life/nameForge.js` | 322 | 251 | 0 | 3 | 6 | personne et famille |  |
| porter | `ai/algorithmic/runtime.js` | 247 | 224 | 0 | 2 | 6 | cognition |  |
| porter | `life/moodlets.js` | 291 | 220 | 32 | 6 | 6 | personne et famille |  |
| porter + scinder | `life/collectiveTalk.js` | 356 | 206 | 99 | 1 | 6 | parole et narration | formules + agregation |
| porter | `ai/algorithmic/hungerAction.js` | 230 | 205 | 0 | 4 | 6 | cognition |  |
| porter | `ai/algorithmic/hungerUtility.js` | 245 | 204 | 2 | 4 | 6 | cognition |  |
| porter + scinder | `life/foundingCeremony.js` | 307 | 201 | 49 | 2 | 6 | rites et culture | ceremonie + deroule |
| porter | `life/chronicleEvents.js` | 240 | 195 | 23 | 2 | 6 | personne et famille |  |
| porter | `ai/haulStreetSignal.js` | 222 | 194 | 2 | 1 | 6 | cognition |  |
| porter | `ai/algorithmic/bridge.js` | 215 | 191 | 2 | 1 | 6 | cognition |  |
| porter | `lang/fromSpeechAct.js` | 229 | 187 | 0 | 1 | 6 | langue |  |
| porter | `ai/accessMemory.js` | 209 | 186 | 0 | 2 | 6 | cognition |  |
| porter | `life/pneuma/causalSignals.js` | 215 | 186 | 6 | 4 | 6 | personne et famille |  |
| porter | `ai/ageSignature.js` | 218 | 184 | 0 | 5 | 6 | cognition |  |
| porter | `life/mortality.js` | 262 | 184 | 3 | 2 | 6 | personne et famille |  |
| porter | `lang/fromNpc.js` | 250 | 183 | 1 | 2 | 6 | langue |  |
| porter | `life/socialMemoryIntegrity.js` | 189 | 176 | 0 | 2 | 6 | personne et famille |  |
| porter | `life/genome.js` | 293 | 173 | 0 | 2 | 6 | personne et famille |  |
| porter | `ai/failureMemory.js` | 202 | 168 | 3 | 4 | 6 | cognition |  |
| porter | `ai/weatherGoalBias.js` | 212 | 166 | 0 | 4 | 6 | cognition |  |
| porter | `ai/negativeKnowledge.js` | 195 | 164 | 1 | 3 | 6 | cognition |  |
| porter | `ai/socialStanding.js` | 206 | 161 | 0 | 1 | 6 | cognition |  |
| porter | `life/cultureDaily.js` | 194 | 157 | 1 | 2 | 6 | rites et culture |  |
| porter | `ai/rumorSpread.js` | 186 | 151 | 2 | 2 | 6 | cognition |  |
| porter | `life/nameLedger.js` | 198 | 146 | 0 | 2 | 6 | personne et famille |  |
| porter | `lang/ir.js` | 167 | 128 | 0 | 4 | 6 | langue |  |
| porter + scinder | `life/sealTalk.js` | 255 | 128 | 92 | 5 | 6 | parole et narration | formules + declenchement |
| porter | `life/skills.js` | 150 | 122 | 0 | 7 | 6 | personne et famille |  |
| porter | `ai/siteDesirability.js` | 162 | 120 | 0 | 1 | 6 | cognition |  |
| porter + scinder | `life/historicalRumors.js` | 176 | 120 | 41 | 1 | 6 | parole et narration | corpus de rumeurs + propagation |
| porter | `ai/dailyRoutine.js` | 171 | 117 | 0 | 1 | 6 | cognition |  |
| porter | `ai/moralPressure.js` | 165 | 116 | 0 | 2 | 6 | cognition |  |
| porter | `life/household.js` | 148 | 116 | 0 | 4 | 6 | personne et famille |  |
| porter + scinder | `life/landTalk.js` | 227 | 112 | 73 | 2 | 6 | parole et narration | formules + declenchement |
| porter | `ai/algorithmic/foodPerception.js` | 136 | 108 | 0 | 3 | 6 | cognition |  |
| porter | `ai/streetSignal.js` | 148 | 108 | 0 | 2 | 6 | cognition |  |
| porter | `life/orthodoxPriestArrival1204.js` | 119 | 107 | 1 | 1 | 6 | rites et culture |  |
| porter | `life/liturgicalCalendar.js` | 143 | 101 | 0 | 4 | 6 | rites et culture |  |
| porter | `ai/householdRoles.js` | 130 | 96 | 0 | 1 | 6 | cognition |  |
| porter | `life/liturgicalNarrative.js` | 118 | 96 | 0 | 1 | 6 | parole et narration |  |
| porter | `life/elders.js` | 120 | 95 | 0 | 3 | 6 | personne et famille |  |
| porter | `life/sagaFamine.js` | 158 | 84 | 5 | 1 | 6 | personne et famille |  |
| porter | `life/seasonNarrative.js` | 112 | 84 | 3 | 2 | 6 | parole et narration |  |
| porter + scinder | `life/talkCanon1204.js` | 124 | 75 | 39 | 1 | 6 | parole et narration | canon 1204 + selection |
| porter | `lang/channel.js` | 112 | 73 | 0 | 1 | 6 | langue |  |
| porter | `life/restTraces.js` | 101 | 72 | 0 | 2 | 6 | personne et famille |  |
| porter | `life/workShift.js` | 205 | 69 | 0 | 5 | 6 | personne et famille |  |
| porter | `ai/algorithmic/memoryEvent.js` | 80 | 66 | 0 | 3 | 6 | cognition |  |
| porter | `life/jobTransition.js` | 92 | 61 | 0 | 3 | 6 | personne et famille |  |
| porter | `life/needActNarrative.js` | 100 | 61 | 0 | 3 | 6 | parole et narration |  |
| porter | `life/landAmbition.js` | 101 | 60 | 0 | 3 | 6 | personne et famille |  |
| porter + scinder | `life/arrivalContext.js` | 77 | 53 | 13 | 2 | 6 | personne et famille | logique posee sur une table (20% de litteraux) |
| porter | `ai/algorithmic/scheduler.js` | 66 | 51 | 0 | 2 | 6 | cognition |  |
| porter | `lang/idiolect.js` | 102 | 51 | 0 | 2 | 6 | langue |  |
| porter | `life/conditioning.js` | 109 | 48 | 0 | 3 | 6 | personne et famille |  |
| porter | `life/followMotif.js` | 85 | 43 | 0 | 4 | 6 | parole et narration |  |
| porter | `ai/algorithmic/inertia.js` | 55 | 41 | 0 | 4 | 6 | cognition |  |
| porter | `ai/algorithmic/metrics.js` | 48 | 41 | 0 | 4 | 6 | cognition |  |
| porter | `ai/algorithmic/decision.js` | 59 | 40 | 0 | 4 | 6 | cognition |  |
| porter | `life/familyVisual.js` | 52 | 38 | 0 | 1 | 6 | personne et famille |  |
| porter | `ai/goalUtility.js` | 55 | 37 | 0 | 1 | 6 | cognition |  |
| porter | `life/generationNarrative.js` | 46 | 33 | 1 | 2 | 6 | parole et narration |  |
| porter | `life/orthodoxAssembly1204.js` | 35 | 27 | 0 | 5 | 6 | rites et culture |  |
| porter | `life/goalTransition.js` | 72 | 20 | 0 | 2 | 6 | personne et famille |  |
| porter | `ai/algorithmic/flags.js` | 19 | 10 | 0 | 3 | 6 | cognition |  |
| generer | `sim/batiments/catalog.js` | 551 | 528 | 15 | 4 | — | — | catalogue des batiments |
| generer | `sim/animaux/catalog.js` | 349 | 285 | 0 | 10 | — | — | catalogue des especes |
| generer | `sim/metiers/catalog.js` | 129 | 116 | 1 | 2 | — | — | catalogue des metiers |
| generer | `life/talkCatalog.js` | 1072 | 349 | 652 | 1 | — | — | catalogue de repliques |
| jeter | `runtime/sessionLaunchProfile.js` | 466 | 362 | 5 | 1 | — | — | profil de lancement navigateur |
| jeter | `runtime/index.js` | 426 | 303 | 6 | 0 | — | — | amorcage du runtime navigateur (cable les filets ci-dessus) |
| jeter | `runtime/faultShield.js` | 317 | 234 | 0 | 1 | — | — | filet de securite navigateur |
| jeter | `runtime/crashReport.js` | 268 | 229 | 0 | 1 | — | — | filet de securite navigateur |
| jeter | `runtime/flightRecorder.js` | 226 | 169 | 0 | 1 | — | — | filet de securite navigateur |
| jeter | `runtime/postMortemUi.js` | 180 | 136 | 0 | 1 | — | — | filet de securite navigateur |
| jeter | `sim/villageSpectrum.js` | 199 | 132 | 0 | 0 | — | — | non atteint depuis src/main.js — a brancher ou a enterrer, pas a porter |
| jeter | `sim/saveStore.js` | 177 | 127 | 0 | 0 | — | — | localStorage -> SaveGame d'Unreal |
| jeter | `runtime/bootTelemetry.js` | 177 | 126 | 0 | 0 | — | — | telemetrie de demarrage navigateur |
| jeter | `runtime/stateRewind.js` | 160 | 100 | 0 | 1 | — | — | rembobinage outil de dev, a refaire sur le save Unreal |
| jeter | `runtime/watchdog.js` | 142 | 95 | 0 | 1 | — | — | filet de securite navigateur |
| jeter | `sim/collectivePrioritiesPanel.js` | 99 | 77 | 0 | 0 | — | — | panneau UI |
| jeter | `sim/observability.js` | 87 | 62 | 0 | 4 | — | — | compteurs pour l'UI de dev |
| jeter | `sim/animaux/index.js` | 62 | 47 | 0 | 2 | — | — | baril de re-export, sans equivalent C++ |
| jeter | `runtime/gameLoop.js` | 45 | 31 | 0 | 0 | — | — | requestAnimationFrame -> Tick d'Unreal |
| jeter | `life/index.js` | 531 | 494 | 0 | 2 | — | — | baril de re-export, sans equivalent C++ |
| jeter | `life/pneuma/PneumaBubbleDirector.js` | 388 | 305 | 1 | 0 | — | — | politique d'attention des bulles (rendu three.js) |
| jeter | `ai/index.js` | 277 | 241 | 0 | 2 | — | — | baril de re-export, sans equivalent C++ |
| jeter | `ai/explainGoal.js` | 280 | 228 | 0 | 3 | — | — | texte d'explication pour l'UI de debug |
| jeter | `ai/npcInspector.js` | 229 | 163 | 27 | 1 | — | — | inspecteur Observatoire F3 (Unreal: AnastasisInspectTools) |
| jeter | `ai/algorithmic/index.js` | 111 | 96 | 0 | 2 | — | — | baril de re-export, sans equivalent C++ |
| jeter | `life/engineBridge.js` | 90 | 81 | 0 | 0 | — | — | facade de lecture pour main.js et l'UI |
| jeter | `ai/algorithmic/debug.js` | 79 | 60 | 10 | 1 | — | — | sondes de debug |
| jeter | `ai/goalLabels.js` | 39 | 11 | 24 | 5 | — | — | libelles d'objectifs pour l'UI |
| porte | `runtime/simClock.js` | 217 | 128 | 0 | 1 | 0 | — | Core/AnastasisSimClock |
| porte | `sim/spatialGrid.js` | 142 | 100 | 0 | 8 | 0 | — | Core/AnastasisSpatialGrid |
| porte | `sim/util.js` | 55 | 27 | 0 | 69 | 0 | — | Core/AnastasisSimMath |
| porte | `sim/rng.js` | 40 | 25 | 0 | 14 | 0 | — | Core/AnastasisRng |
| porte | `sim/worldArchetypes.js` | 866 | 752 | 7 | 4 | 1 | — | World/AnastasisWorldArchetype (knobs sim seuls) |
| porte | `sim/hydrology.js` | 445 | 333 | 0 | 1 | 1 | — | World/AnastasisHydrology |
| porte | `sim/world.js` | 427 | 320 | 0 | 3 | 1 | — | World/AnastasisWorld (+ WorldNoise) |
| porte | `sim/fieldCrops.js` | 155 | 120 | 0 | 11 | 1 | — | World/AnastasisWorld::PickFieldCropId |
