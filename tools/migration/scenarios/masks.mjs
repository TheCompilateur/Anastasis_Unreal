// MASQUES DE SCENARIO — neutraliser de l'exterieur ce que le C++ ne porte pas.
//
// Le harnais compare JS et Unreal sur un etat reduit. Tant qu'un systeme JS
// n'est pas porte, le laisser tourner cote JS fait diverger la trace des le
// premier tick ou il agit, et cache tout le reste. Un MASQUE le neutralise cote
// JS, sans jamais toucher au depot de reference: tout se joue sur l'instance
// `sim` que l'emetteur a chargee.
//
// Trois techniques, et seulement trois:
//
//   methode        une methode de `sim` remplacee sur l'instance (la propriete
//                  propre masque celle du prototype). Le tick de reference
//                  l'appelle sans le savoir.
//   tick-recompose les systemes du tick sont des FONCTIONS IMPORTEES par
//                  simulation.js (`tickTransport`, `updateAnimalsTick`,
//                  `tickSagas`…): une liaison d'import ES ne se remplace pas.
//                  On remplace donc `sim.tick` par `tickRecompose`, la meme
//                  suite d'etapes, dans le meme ordre, appelee depuis les memes
//                  exports de la reference, et chaque etape masquee est sautee.
//                  `tickRecompose` sans masque rend exactement le tick de
//                  reference: l'autotest le prouve trace contre trace.
//   file-de-minuit un travail de `enqueueDayDeferred` garde sa place dans la
//                  file (le C++ la tient aussi, 17 travaux, 2 par tick) mais
//                  son `run` est remplace. La liste des travaux vient de la
//                  reference elle-meme: on appelle l'original, puis on remplace.
//
// Une quatrieme facon de neutraliser n'est PAS un masque: l'etat vide (pas
// d'animal, pas de charrette, population sous le seuil du LOD logique). Elle
// est dite dans le scenario, a cote des masques.
//
// REGLE: un masque ne change pas l'ORDRE des appels des systemes qui restent.
// `tickRecompose` reprend l'ordre du tick de reference; une methode masquee est
// appelee au meme endroit. Un systeme masque qui tirait dans `sim.rng` ne tire
// plus: c'est une divergence ATTENDUE (le C++ ne tire pas non plus), et le
// scenario dit combien de tirages chaque masque supprime (`auditRng`).
//
// La table est epinglee sur la reference (`anastasis-ref-p3`): si la file de
// minuit de la reference n'a plus ses 17 travaux dans cet ordre, l'application
// des masques echoue au lieu de masquer a cote.

import { join } from "node:path";
import { pathToFileURL } from "node:url";

/** Les 17 travaux de minuit de la reference, dans l'ordre (simulation.js, enqueueDayDeferred). */
export const TRAVAUX_DE_MINUIT = [
  "landRegen", "collective", "socialOrders", "colonyDoctrine", "growthChapter", "founderCharter",
  "colonySites", "transportProjects", "roadEvolution", "watchPosts", "lifeDaily", "careers",
  "founders", "romanCouncils", "memory", "animalsDaily", "immigration",
];

/** Charge, depuis la reference, tout ce dont les masques et l'emetteur ont besoin. */
export async function chargerReference(REF) {
  const url = (rel) => pathToFileURL(join(REF, rel)).href;
  const sim = await import(url("src/sim/simulation.js"));
  const save = await import(url("src/sim/save.js"));
  const budget = await import(url("src/sim/simulationBudget.js"));
  const nav = await import(url("src/sim/navService.js"));
  const lod = await import(url("src/sim/logicalLod.js"));
  const transport = await import(url("src/sim/transport/index.js"));
  const animaux = await import(url("src/sim/animaux/index.js"));
  const sagas = await import(url("src/sim/sagas.js"));
  const kosmos = await import(url("src/life/kosmos1204UneBouchePlus.js"));
  const grille = await import(url("src/sim/spatialGrid.js"));
  const npc = await import(url("src/sim/npc.js"));
  const domestic = await import(url("src/life/domestic.js"));
  const ai = await import(url("src/ai/index.js"));
  const util = await import(url("src/sim/util.js"));
  const chemins = await import(url("src/sim/pathfinding.js"));
  return {
    dist: util.dist,
    findPath: chemins.findPath,
    Simulation: sim.Simulation,
    DAY_LENGTH: sim.DAY_LENGTH,
    DAY_DEFERRED_JOBS_PER_TICK: sim.DAY_DEFERRED_JOBS_PER_TICK,
    serialize: save.serialize,
    deserialize: save.deserialize,
    createSimulationBudgetDirector: budget.createSimulationBudgetDirector,
    resetSimulationBudgetStats: budget.resetSimulationBudgetStats,
    simulationBudgetMultipliers: budget.simulationBudgetMultipliers,
    pinSimulationView: budget.pinSimulationView,
    beginNavTick: nav.beginNavTick,
    processNavQueue: nav.processNavQueue,
    tickLogicalVillageLod: lod.tickLogicalVillageLod,
    tickTransport: transport.tickTransport,
    creditColonyStock: transport.creditColonyStock,
    consumeFromColonyStock: transport.consumeFromColonyStock,
    updateAnimalsTick: animaux.updateAnimalsTick,
    tickSagas: sagas.tickSagas,
    resolveUneBouchePlus: kosmos.resolveUneBouchePlus,
    attemptUneBouchePlusTransition: kosmos.attemptUneBouchePlusTransition,
    rebuildActorSpatialIndex: grille.rebuildActorSpatialIndex,
    updateNpc: npc.updateNpc,
    assignSheltersDaily: domestic.assignSheltersDaily,
    forgetStale: ai.forgetStale,
    forgetStalePeople: ai.forgetStalePeople,
  };
}

// --- Le registre ------------------------------------------------------------
// `porte`: ce que le C++ fait de ce systeme aujourd'hui. `mission`: celle qui
// retirera le masque (P3_PLAN.md). Un masque ne s'ajoute qu'a la creation d'un
// scenario; il se retire quand sa mission a porte le systeme.

const TICK = "tick-recompose";
const METHODE = "methode";
const MINUIT = "file-de-minuit";

export const MASQUES = {
  kosmos: {
    technique: TICK, cible: "resolveUneBouchePlus + attemptUneBouchePlusTransition (debut de tick)",
    porte: "non", mission: "sagas-kosmos-001",
  },
  sagas: {
    technique: TICK, cible: "tickSagas (apres la file de minuit)",
    porte: "non", mission: "sagas-kosmos-001",
  },
  urbanIntent: {
    technique: TICK, cible: "refreshUrbanIntents({ force: true }) quand une route a change",
    porte: "non", mission: "urban-001",
  },
  lodLogique: {
    technique: TICK, cible: "tickLogicalVillageLod et sa sortie anticipee du tick",
    porte: "non", mission: "nav-service-001",
  },
  separationFoule: {
    technique: METHODE, cible: "sim.separateCrowdedActors(dt) (apres updateNpc)",
    porte: "non", mission: "nav-service-001",
  },
  animaux: {
    technique: TICK, cible: "updateAnimalsTick et son budget (_animalBudgetDt)",
    porte: "non", mission: "animals-001",
  },
  transport: {
    technique: TICK, cible: "tickTransport et son budget (_transportBudgetDt)",
    porte: "non", mission: "transport-001 / goals-haul-001",
  },
  economieJournaliere: {
    technique: METHODE,
    cible: "sim.onNewDay : section critique de minuit reduite a ce que le C++ porte "
      + "(rations du jour, assignSheltersDaily, enqueueDayDeferred). Masques : ensureWorldSagas, "
      + "updateMarketVisitorDaily, decayPassageTrafficDaily, production, pourriture, exports, "
      + "rebuildMarketAggregate, entretien, registre de nourriture, penuries, achats de maison, "
      + "ensureWorkplacesDaily, agrandissements, routes murissantes, loyers, dividendes, moral, "
      + "transformation, circulation, journal du jour, fadeVillageLogs, arrivee du pretre",
    porte: "assignSheltersDaily seulement", mission: "day-critical-001",
  },
  "minuit.collective": { technique: MINUIT, cible: "travail `collective`", porte: "non (place tenue)", mission: "day-deferred-001" },
  "minuit.socialOrders": { technique: MINUIT, cible: "travail `socialOrders`", porte: "non (place tenue)", mission: "day-deferred-001" },
  "minuit.colonyDoctrine": { technique: MINUIT, cible: "travail `colonyDoctrine`", porte: "non (place tenue)", mission: "day-deferred-001" },
  "minuit.growthChapter": { technique: MINUIT, cible: "travail `growthChapter`", porte: "non (place tenue)", mission: "day-deferred-001" },
  "minuit.founderCharter": { technique: MINUIT, cible: "travail `founderCharter`", porte: "non (place tenue)", mission: "day-deferred-001" },
  "minuit.colonySites": { technique: MINUIT, cible: "travail `colonySites`", porte: "non (place tenue)", mission: "day-deferred-001" },
  "minuit.transportProjects": { technique: MINUIT, cible: "travail `transportProjects`", porte: "non (place tenue)", mission: "day-deferred-001" },
  "minuit.roadEvolution": { technique: MINUIT, cible: "travail `roadEvolution`", porte: "non (place tenue)", mission: "day-deferred-001" },
  "minuit.watchPosts": { technique: MINUIT, cible: "travail `watchPosts`", porte: "non (place tenue)", mission: "day-deferred-001" },
  "minuit.lifeDaily": { technique: MINUIT, cible: "travail `lifeDaily`", porte: "non (place tenue)", mission: "day-deferred-001" },
  "minuit.careers": { technique: MINUIT, cible: "travail `careers` (reallocation des metiers)", porte: "non (place tenue)", mission: "day-deferred-001" },
  "minuit.founders": { technique: MINUIT, cible: "travail `founders`", porte: "non (place tenue)", mission: "day-deferred-001" },
  "minuit.romanCouncils": { technique: MINUIT, cible: "travail `romanCouncils`", porte: "non (place tenue)", mission: "day-deferred-001" },
  "minuit.memory.partiel": {
    technique: MINUIT,
    cible: "travail `memory` reduit a forgetStale + forgetStalePeople par habitant ; masques : fadeEpisodes, updateAmbitionsDaily, updateDayIntentsDaily",
    porte: "forgetStale + forgetStalePeople", mission: "day-deferred-001",
  },
  "minuit.animalsDaily": { technique: MINUIT, cible: "travail `animalsDaily`", porte: "non (place tenue)", mission: "animals-001" },
  "minuit.immigration": { technique: MINUIT, cible: "travail `immigration` (maybeImmigrate)", porte: "non (place tenue)", mission: "day-deferred-001" },
};

// Systemes qu'aucun masque ne neutralise de l'exterieur. Le scenario les
// recopie: ils sont la raison pour laquelle une section diverge alors que
// tout ce qui pouvait etre masque l'a ete.
export const NON_MASQUABLES = [
  {
    id: "navService",
    systeme: "beginNavTick / processNavQueue : file A*, budget par tick, cache de chemins (dans la sauvegarde : navCache)",
    pourquoi: "le masquer arrete toute marche : aucune requete de chemin n'est plus servie. C'est le socle que porte nav-service-001 ; d'ici la, la section `actors` diverge des le premier pas (ecarts n° 4 et 5).",
  },
  {
    id: "dansUpdateNpc",
    systeme: "tout ce qu'appelle updateNpc et que le C++ ne porte pas : table de decision complete, goalNoise et reconsideration (sim.rng), achat / vente, episodes, texte des repliques, rumeurs hors gisements, marche de l'emploi interne",
    pourquoi: "updateNpc EST la boucle comparee. Le remplacer, ce serait ecrire une autre simulation. Ecarts n° 1 a 18 de Village/AnastasisVillage.h ; jalon B.",
  },
  {
    id: "compteurDeJournal",
    systeme: "identifiants du journal de village (`logs[].id`), compteur de module",
    pourquoi: "etat de module, pas de `sim` : une trace = un processus neuf (P2_HARNAIS_DIFFERENTIEL.md). Section `logs` hors perimetre.",
  },
];

// --- Application ------------------------------------------------------------

/**
 * Applique `ids` a `sim`. Rend { tick, etiquette } :
 *   tick(dt)   la fonction a appeler a chaque pas (tick de reference, ou
 *              recompose si un masque de tick est actif ou si `recompose`)
 * Leve si un identifiant est inconnu: un masque mal orthographie qui ne masque
 * rien rendrait une trace fausse en silence.
 */
export function appliquerMasques(sim, ref, ids, { recompose = false, etiqueteur = null } = {}) {
  const inconnus = ids.filter((id) => !MASQUES[id]);
  if (inconnus.length) throw new Error(`Masque(s) inconnu(s) : ${inconnus.join(", ")}`);
  const actifs = new Set(ids);
  const proto = Object.getPrototypeOf(sim);
  const etiquette = etiqueteur ?? { courante: "hors-tick", poser(e) { const p = this.courante; this.courante = e; return p; } };

  if (actifs.has("separationFoule")) sim.separateCrowdedActors = function separateCrowdedActorsMasque() {};

  if (actifs.has("economieJournaliere")) {
    sim.onNewDay = function onNewDayMasque(options = {}) {
      const defer = options.defer === true;
      if (this._dayDeferred?.length && !defer) this.flushDayDeferred();
      if (this.life) {
        this.life.rationsGrantedToday = 0;
        this.life.rationsDay = this.day;
      }
      ref.assignSheltersDaily(this);
      this.enqueueDayDeferred();
      if (!defer) this.flushDayDeferred();
    };
  }

  const masquesMinuit = new Set([...actifs].filter((id) => id.startsWith("minuit.")));
  const enqueueOriginal = proto.enqueueDayDeferred;
  sim.enqueueDayDeferred = function enqueueDayDeferredEtiquete() {
    const avant = this._dayDeferred?.length ? this._dayDeferred.length : 0;
    enqueueOriginal.call(this);
    const ajoutes = this._dayDeferred.slice(avant);
    const ids = ajoutes.map((j) => j.id);
    if (ids.join(",") !== TRAVAUX_DE_MINUIT.join(",")) {
      throw new Error(`File de minuit inattendue (reference changee ?) : ${ids.join(",")}`);
    }
    for (const job of ajoutes) {
      const original = job.run;
      let run = original;
      if (job.id === "memory" && masquesMinuit.has("minuit.memory.partiel")) {
        run = (s) => {
          for (const npc of s.actors) {
            ref.forgetStale(s, npc);
            ref.forgetStalePeople(s, npc);
          }
        };
      } else if (masquesMinuit.has(`minuit.${job.id}`)) {
        run = () => {};
      }
      job.run = (s) => {
        const p = etiquette.poser(`minuit.${job.id}`);
        try { return run(s); } finally { etiquette.poser(p); }
      };
    }
  };

  const tickMasques = ["kosmos", "sagas", "urbanIntent", "lodLogique", "animaux", "transport"].filter((id) => actifs.has(id));
  const utiliseRecompose = recompose || tickMasques.length > 0 || etiqueteur !== null;
  const tick = utiliseRecompose
    ? (dt) => tickRecompose(sim, ref, dt, actifs, etiquette)
    : (dt) => sim.tick(dt);
  return { tick, recompose: utiliseRecompose, etiquette };
}

/**
 * Le tick de reference (simulation.js, `tick(dt)`), etape par etape, dans son
 * ordre, depuis les exports de la reference. Seules differences voulues: les
 * etapes masquees sont sautees, et les spans `obs` (chronometrage de debug,
 * etat de module) ne sont pas ouverts.
 */
export function tickRecompose(sim, ref, dt, actifs, etiquette) {
  const e = (nom, fn) => {
    const p = etiquette.poser(nom);
    try { return fn(); } finally { etiquette.poser(p); }
  };
  if (sim.bootDeferred) return;
  if (!actifs.has("kosmos")) {
    e("kosmos", () => {
      ref.resolveUneBouchePlus(sim);
      ref.attemptUneBouchePlusTransition(sim);
    });
  }
  if (!sim.simulationBudget) sim.simulationBudget = ref.createSimulationBudgetDirector();
  ref.resetSimulationBudgetStats(sim.simulationBudget);
  const budgetMul = ref.simulationBudgetMultipliers(sim.simulationBudget);
  sim.simulationBudget.stats.pathBudgetMul = budgetMul.pathBudgetMul;
  sim.simulationBudget.stats.animalTickMul = budgetMul.animalTickMul;
  sim.simulationBudget.stats.transportTickMul = budgetMul.transportTickMul;
  sim.time += dt;
  const newDay = 1 + Math.floor(sim.time / ref.DAY_LENGTH);
  if (newDay !== sim.day) {
    sim.day = newDay;
    e("onNewDay", () => sim.onNewDay({ defer: true }));
  }
  sim.processDayDeferred(ref.DAY_DEFERRED_JOBS_PER_TICK);
  if (!actifs.has("sagas")) e("sagas", () => ref.tickSagas(sim));
  if (!actifs.has("urbanIntent") && sim._urbanIntentRoadDirty) {
    e("urbanIntent", () => sim.refreshUrbanIntents({ force: true }));
  }
  if (!actifs.has("lodLogique")) {
    const logical = e("lodLogique", () => ref.tickLogicalVillageLod(sim, dt));
    if (logical.stockDelta) {
      if (logical.stockDelta.wood > 0) ref.creditColonyStock(sim, "wood", logical.stockDelta.wood);
      if (logical.stockDelta.stone > 0) ref.creditColonyStock(sim, "stone", logical.stockDelta.stone);
      if (logical.stockDelta.food > 0) ref.creditColonyStock(sim, "food", logical.stockDelta.food);
      if (logical.stockDelta.foodUse > 0) ref.consumeFromColonyStock(sim, "food", logical.stockDelta.foodUse);
    }
    if (logical.active) return;
  }
  ref.rebuildActorSpatialIndex(sim);
  e("navService", () => {
    ref.beginNavTick(sim, { budgetMul: budgetMul.pathBudgetMul });
    ref.processNavQueue(sim);
  });
  e("updateNpc", () => { for (const npc of sim.actors) ref.updateNpc(sim, npc, dt); });
  e("navService", () => ref.processNavQueue(sim));
  e("separationFoule", () => sim.separateCrowdedActors(dt));
  if (!actifs.has("animaux")) {
    sim._animalBudgetDt = (sim._animalBudgetDt || 0) + dt;
    if (budgetMul.animalTickMul >= 0.99 || sim._animalBudgetDt >= dt / Math.max(0.1, budgetMul.animalTickMul)) {
      const animalDt = sim._animalBudgetDt;
      sim._animalBudgetDt = 0;
      e("animaux", () => ref.updateAnimalsTick(sim, animalDt));
    }
  }
  if (!actifs.has("transport")) {
    sim._transportBudgetDt = (sim._transportBudgetDt || 0) + dt;
    if (budgetMul.transportTickMul >= 0.99 || sim._transportBudgetDt >= dt / Math.max(0.1, budgetMul.transportTickMul)) {
      const transportDt = sim._transportBudgetDt;
      sim._transportBudgetDt = 0;
      e("transport", () => ref.tickTransport(sim, transportDt));
    }
  }
}

/**
 * Compteur de tirages `sim.rng`, attribues a l'etape du tick en cours
 * (etiquettes de `tickRecompose` et des travaux de minuit). Remplace `sim.rng`
 * par une enveloppe qui garde `state` / `setState`: les modules appellent
 * `sim.rng()` a chaque tirage, ou recoivent la fonction (`createNpc(this.rng, …)`).
 */
export function compterTirages(sim, etiquette) {
  const original = sim.rng;
  const compte = {};
  const enveloppe = () => {
    compte[etiquette.courante] = (compte[etiquette.courante] || 0) + 1;
    return original();
  };
  enveloppe.state = () => original.state();
  enveloppe.setState = (s) => original.setState(s);
  sim.rng = enveloppe;
  return compte;
}
