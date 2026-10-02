// FIXTURES DU PLANIFICATEUR — mission planner-module-001.
//
//   node tools/migration/gen-planner-fixtures.mjs -ref <clone anastasis-ref-p3>
//
// Des VARIANTES du scenario endurance (etats naturels aux ticks 0, 32, 196, puis batiments
// ajoutes, en chantier ou acheves, maisons occupees ou vides, metiers, stocks, niveaux de
// priorite, focus du jour, charte vivante ou echue, rapport de stock perime, jours differents).
// Pour chaque variante, la reference EXECUTE (rien n'est recopie) :
//   - par habitant, dans l'ordre : buildingNeedScore, collectiveGoalBias (15 buts),
//     collectiveGoalFloor, collectiveUrgencyBiasMap (texte de npc.js evalue tel quel),
//     isWoodBootstrapDraftee, isFoodRush, farmStaffingGap ;
//   - puis les ECRITURES : vacantSinceDay et stock des batiments, rapport de stock, etat du flux,
//     charte, cache des effets, corvee de bois ;
//   - et, sur une copie fraiche : measureJobNeeds, scoreBuildingProjects, boostedBuildingScores,
//     collectiveBuildingNeedScore, les cinq pending, frontierForestDensity.
// Les nombres sont ecrits par leurs bits (chaine hexadecimale) : la comparaison C++ est au bit.

import { readFileSync, writeFileSync, mkdirSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";

import { chargerReference, appliquerMasques } from "./scenarios/masks.mjs";
import { chargerScenario } from "./scenarios/scenario-format.mjs";

const ICI = dirname(fileURLToPath(import.meta.url));
const RACINE = join(ICI, "..", "..");
const argv = process.argv.slice(2);
const argOf = (f, d) => { const i = argv.indexOf(f); return i >= 0 && argv[i + 1] !== undefined ? argv[i + 1] : d; };
const REF = argOf("-ref", null);
if (!REF) {
  console.error("-ref <clone de anastasis-ref-p3> requis");
  process.exit(2);
}
const REF_DIR = REF.split(String.fromCharCode(92)).join("/");
const OUT = argOf("-out", join(RACINE, "tools", "migration", "planner", "planner-fixtures.json"));
const url = (rel) => pathToFileURL(join(REF_DIR, rel)).href;

const ref = await chargerReference(REF_DIR);
const save = await import(url("src/sim/save.js"));
const cp = await import(url("src/sim/collectivePriorities.js"));
const doctrine = await import(url("src/sim/colonizationDoctrine.js"));
const scenario = chargerScenario(join(ICI, "scenarios", "endurance.json"));
const rngMod = await import(url("src/sim/rng.js"));

/** Un etat du flux dont le premier tirage tombe sous `sous` : pour forcer la rumeur du rapport. */
function etatTirantSous(sous) {
  const r = rngMod.makeRng(0);
  for (let s = 1; s < 1e6; s += 1) {
    r.setState(s);
    if (r() < sous) return s;
  }
  throw new Error("aucun etat");
}

// --- collectiveUrgencyBiasMap : le texte de npc.js, evalue avec ses dependances ----------------
function extraire(src, name) {
  const start = src.indexOf(`function ${name}(`);
  if (start < 0) throw new Error(`${name} introuvable`);
  let i = src.indexOf("{", start);
  let depth = 0;
  for (let j = i; j < src.length; j += 1) {
    if (src[j] === "{") depth += 1;
    else if (src[j] === "}") {
      depth -= 1;
      if (depth === 0) return src.slice(start, j + 1);
    }
  }
  throw new Error(`${name} mal forme`);
}
const npcSrc = readFileSync(join(REF_DIR, "src/sim/npc.js"), "utf8");
const urgencyCode = ["addGoalBias", "readCollectiveUrgencySnapshot", "collectiveUrgencyBiasMap"].map((n) => extraire(npcSrc, n)).join("\n");
// eslint-disable-next-line no-new-func
const urgencyMap = Function(
  "collectiveHydrationStress", "collectiveHousingSaturation", "collectiveAccessStress", "villageAmenityPending", "isFoodRush",
  `"use strict";\n${urgencyCode}\nreturn collectiveUrgencyBiasMap;`,
)(cp.collectiveHydrationStress, cp.collectiveHousingSaturation, cp.collectiveAccessStress, cp.villageAmenityPending, cp.isFoodRush);

// --- Bits ------------------------------------------------------------------------------------------
const bits = (x) => {
  const v = new DataView(new ArrayBuffer(8));
  v.setFloat64(0, Number(x), true);
  return "0x" + v.getBigUint64(0, true).toString(16).padStart(16, "0");
};
const optBits = (x) => (x === undefined || x === null ? null : bits(x));
const mapBits = (obj) => Object.entries(obj || {}).map(([k, v]) => [k, bits(v)]);

// --- Etats de base -------------------------------------------------------------------------------
function etatAuTick(ticks) {
  const sim = new ref.Simulation({ deferred: true, seed: scenario.seed });
  ref.deserialize(sim, JSON.parse(JSON.stringify(scenario.save)));
  if (scenario.vue) ref.pinSimulationView(sim.simulationBudget, scenario.vue.x, scenario.vue.y);
  const etiq = { courante: "", poser(e) { const p = this.courante; this.courante = e; return p; } };
  const { tick } = appliquerMasques(sim, ref, [...scenario.masques], { recompose: true, etiqueteur: etiq });
  for (let t = 1; t <= ticks; t += 1) tick(scenario.dt);
  return JSON.stringify(save.serialize(sim));
}

function copie(etat) {
  const sim = new ref.Simulation({ deferred: true, seed: scenario.seed });
  ref.deserialize(sim, JSON.parse(etat));
  if (scenario.vue) ref.pinSimulationView(sim.simulationBudget, scenario.vue.x, scenario.vue.y);
  // Caches de depart vides, comme la vue C++.
  const p = sim.colony?.priorities;
  if (p) {
    delete p._effects;
    delete p._woodDraft;
    delete p._woodDraftDay;
  }
  sim._npcCollectiveUrgency = null;
  return sim;
}

// --- Variantes ----------------------------------------------------------------------------------
let nextId = 900;
function batir(sim, type, x, y, extra = {}) {
  const b = { id: `building-${nextId++}`, type, x, y, progress: 1, createdDay: 1, ...extra };
  sim.buildings.push(b);
  sim._completedBuildingEntries = null;
  if (type === "market" && (b.progress ?? 1) >= 1) sim._marketPos = { x, y };
  return b;
}
function stock(b, res, physical, reserved = 0) {
  b.stock ??= {};
  b.stock[res] = { physical, reserved };
}

const VARIANTES = [
  { nom: "endurance-t0", tick: 0, poser() {} },
  { nom: "endurance-t32", tick: 32, poser() {} },
  { nom: "endurance-t196", tick: 196, poser() {} },
  {
    nom: "rapport-perime-et-depot-lointain", tick: 196,
    poser(sim) {
      const far = batir(sim, "warehouse", sim.settlement.x + 22, sim.settlement.y + 4);
      stock(far, "wood", 60);
      stock(far, "stone", 24);
      sim.market.stock.wood = 60;
      sim.market.stock.stone = 24;
      sim.colony.stockReport.lastRefreshDay = (sim.day | 0) - 2;
      sim.colony.stockReport.day = (sim.day | 0) - 2;
      sim.colony.stockReport.rumor = null;
    },
  },
  {
    nom: "ferme-sans-fermier-et-vivres", tick: 32,
    poser(sim) {
      batir(sim, "farm", sim.settlement.x - 6, sim.settlement.y + 3);
      sim.market.stock.food = 30;
      const g = sim.buildings.find((b) => b.type === "granary");
      if (g) stock(g, "food", 30);
    },
  },
  {
    nom: "maisons-vides-et-sans-toit", tick: 32,
    poser(sim) {
      batir(sim, "house", sim.settlement.x + 3, sim.settlement.y - 5);
      const owned = batir(sim, "house", sim.settlement.x - 4, sim.settlement.y - 5, { owner: sim.actors[0].id, vacantSinceDay: 1 });
      sim.actors[0].home = owned;
      batir(sim, "house", sim.settlement.x + 6, sim.settlement.y + 6, { progress: 0.4, materialsNeeded: { wood: 24, stone: 8 }, materialsConsumed: { wood: 4 }, piecesPlaced: 2 });
    },
  },
  {
    nom: "priorites-hautes-et-scores-stockes", tick: 32,
    poser(sim) {
      const pr = sim.colony.priorities.priorities;
      pr.food.level = 85;
      pr.housing.level = 70;
      pr.tools.level = 50;
      pr.transport.level = 50;
      pr.security.level = 50;
      pr.labor.level = 30;
      sim.colony.priorities.jobs.builder.need = 72;
      sim.colony.priorities.jobs.farmer.need = 90;
      sim.colony.priorities.jobs.guard.surplus = 5;
      sim.colony.priorities.buildingScores = { sawmill: 50, lumbercamp: 45, house: 80 };
      batir(sim, "farm", sim.settlement.x - 6, sim.settlement.y + 3);
    },
  },
  {
    nom: "focus-outils-sans-atelier", tick: 32,
    poser(sim) { sim.colony.priorities.dailyFocus = { id: "tools", day: sim.day | 0 }; },
  },
  {
    nom: "focus-vivres-fondation", tick: 32,
    poser(sim) { sim.colony.priorities.dailyFocus = { id: "food", day: sim.day | 0, forced: "foundingFood" }; },
  },
  {
    nom: "focus-pierre-et-logement", tick: 32,
    poser(sim) {
      sim.colony.priorities.dailyFocus = { id: "stone", day: sim.day | 0 };
      sim.colony.priorities.priorities.housing.level = 46;
    },
  },
  {
    nom: "charte-vivante-loger", tick: 32,
    poser(sim) { sim.colony.charter = { themeId: "house", untilDay: (sim.day | 0) + 2, since: sim.day | 0 }; },
  },
  {
    nom: "charte-echue", tick: 32,
    poser(sim) {
      sim.day = (sim.day | 0) + 3;
      sim.colony.charter = { themeId: "forge", untilDay: (sim.day | 0) - 1, since: 1 };
    },
  },
  {
    nom: "chantiers-et-corvee-de-bois", tick: 32,
    poser(sim) {
      const a = batir(sim, "farm", sim.settlement.x - 6, sim.settlement.y + 3, { progress: 0.1, materialsNeeded: { wood: 16, stone: 4 }, materialsConsumed: { wood: 2 }, piecesPlaced: 1 });
      stock(a, "wood", 1);
      sim.colony.priorities.siteWatch = { sites: { [a.id]: { stalledSinceDay: (sim.day | 0) - 3 } } };
      sim.actors[1].inventory.wood = 2;
    },
  },
  {
    nom: "chantier-posable-en-dette", tick: 32,
    poser(sim) {
      sim.day = (sim.day | 0) + 4;
      const a = batir(sim, "farm", sim.settlement.x - 6, sim.settlement.y + 3, { progress: 0.2, materialsNeeded: { wood: 16, stone: 4 }, materialsConsumed: { wood: 2, stone: 1 }, piecesPlaced: 2 });
      stock(a, "wood", 9);
      stock(a, "stone", 3);
      sim.colony.priorities.siteWatch = { sites: { [a.id]: { stalledSinceDay: (sim.day | 0) - 3 } } };
    },
  },
  {
    nom: "industrie-et-amenites", tick: 32,
    poser(sim) {
      sim.day = 20;
      const sx = sim.settlement.x;
      const sy = sim.settlement.y;
      batir(sim, "farm", sx - 6, sy + 3);
      const camp = batir(sim, "lumbercamp", sx - 9, sy - 2);
      stock(camp, "wood", 40);
      batir(sim, "sawmill", sx - 9, sy + 6);
      const q = batir(sim, "quarry", sx + 10, sy - 6);
      stock(q, "stone", 30);
      batir(sim, "workshop", sx + 4, sy + 8);
      batir(sim, "forge", sx + 7, sy + 8);
      Object.assign(sim.market.stock, { wood: 40, stone: 30, food: 24, tools: 3, planks: 4 });
      sim.colony.stockReport.lastRefreshDay = 19;
      sim.colony.stockReport.day = 19;
    },
  },
  {
    nom: "puits-bati-hiver-et-disette", tick: 32,
    poser(sim) {
      sim.day = 95;
      batir(sim, "well", sim.settlement.x + 2, sim.settlement.y + 9);
      sim.market.stock.food = 6;
      sim.colony.stockReport.lastRefreshDay = 94;
      sim.colony.stockReport.day = 93;
    },
  },
  {
    nom: "rumeur-tiree", tick: 196,
    poser(sim) {
      const far = batir(sim, "warehouse", sim.settlement.x + 24, sim.settlement.y - 6);
      stock(far, "stone", 40);
      sim.market.stock.stone = 40;
      sim.colony.stockReport.lastRefreshDay = (sim.day | 0) - 1;
      sim.colony.stockReport.day = (sim.day | 0) - 1;
      sim.colony.stockReport.rumor = null;
      sim.rng.setState(etatTirantSous(0.1));
    },
  },
  {
    nom: "rumeur-vivante", tick: 32,
    poser(sim) {
      Object.assign(sim.market.stock, { wood: 20, food: 15 });
      const g = sim.buildings.find((b) => b.type === "granary");
      if (g) stock(g, "food", 15);
      sim.colony.stockReport.lastRefreshDay = (sim.day | 0) - 1;
      sim.colony.stockReport.rumor = { resource: "food", mul: 1.22, untilDay: (sim.day | 0) + 2, cause: "rumeur d'abondance" };
    },
  },
  {
    nom: "sans-puits-et-forge", tick: 32,
    poser(sim) {
      sim.buildings = sim.buildings.filter((b) => b.type !== "well");
      sim._completedBuildingEntries = null;
      batir(sim, "forge", sim.settlement.x + 7, sim.settlement.y + 8);
      sim.day = 12;
      sim.colony.stockReport.lastRefreshDay = 12;
      sim.colony.stockReport.day = 12;
    },
  },
  {
    nom: "marche-bati-et-scierie-planches", tick: 32,
    poser(sim) {
      const m = batir(sim, "market", sim.settlement.x + 5, sim.settlement.y - 3);
      stock(m, "food", 12);
      batir(sim, "sawmill", sim.settlement.x - 9, sim.settlement.y + 6);
      batir(sim, "lumbercamp", sim.settlement.x - 9, sim.settlement.y - 2);
      Object.assign(sim.market.stock, { planks: 6, wood: 30, food: 12 });
      sim.colony.stockReport.lastRefreshDay = (sim.day | 0) - 1;
    },
  },
];

// --- La vue ---------------------------------------------------------------------------------------
function vue(sim) {
  const s = sim.settlement;
  const c = sim.colony;
  const p = c?.priorities;
  const band = doctrine.colonizationBandRange(sim);
  const cx = Math.floor(s?.x ?? 0);
  const cy = Math.floor(s?.y ?? 0);
  const tiles = [];
  for (let y = Math.floor(cy - band.max) - 1; y <= Math.ceil(cy + band.max) + 1; y += 1) {
    for (let x = Math.floor(cx - band.max) - 1; x <= Math.ceil(cx + band.max) + 1; x += 1) {
      const t = sim.tileAt(x, y);
      if (t) tiles.push([x, y, t.type || "", t.resource || "", bits(t.amount || 0)]);
    }
  }
  return {
    day: bits(sim.day), time: bits(sim.time || 0), w: sim.w, h: sim.h,
    settlement: s ? { x: bits(s.x), y: bits(s.y), clearRadius: optBits(s.clearRadius), marketDx: optBits(s.marketDx), marketDy: optBits(s.marketDy) } : null,
    marketPosCache: sim._marketPos ? [bits(sim._marketPos.x), bits(sim._marketPos.y)] : null,
    buildings: sim.buildings.map((b) => ({
      id: b.id, type: b.type, progress: optBits(b.progress), x: bits(b.x ?? 0), y: bits(b.y ?? 0),
      owner: b.owner ? String(b.owner?.id ?? b.owner) : "",
      vacantSinceDay: optBits(b.vacantSinceDay), createdDay: optBits(b.createdDay), housePhase: optBits(b.housePhase),
      materialsNeeded: b.materialsNeeded ? mapBits(b.materialsNeeded) : null,
      materialsConsumed: mapBits(b.materialsConsumed),
      piecesPlaced: b.piecesPlaced | 0,
      stock: b.stock && typeof b.stock === "object"
        ? Object.entries(b.stock).map(([res, slot]) => [res, slot?.physical | 0, slot?.reserved | 0])
        : null,
    })),
    actors: sim.actors.map((a) => ({
      id: a.id, lifeStage: a.lifeStage || "", jobId: a.jobId || "", alive: a.alive !== false,
      home: a.home ? String(a.home?.id ?? a.home) : "", shelter: a.shelter ? String(a.shelter?.id ?? a.shelter) : "",
      workplace: a.workplace?.id ? String(a.workplace.id) : "",
      traitGather: bits(Number(a.trait?.gather) || 0), inventoryWood: a.inventory?.wood | 0,
    })),
    colony: c ? {
      morale: optBits(c.morale),
      hotPads: c.doctrine?.hotPads?.length || 0,
      expansionBonus: bits(Number.isFinite(c.doctrine?.expansionBonus) ? c.doctrine.expansionBonus : 0),
      priorities: {
        levels: ["food", "housing", "tools", "labor", "transport", "security"].map((t) => [t, bits(p?.priorities?.[t]?.level || 0)]),
        jobs: Object.values(p?.jobs || {}).map((j) => [j.jobId, bits(j.current || 0), bits(j.needed || 0), bits(j.need || 0), bits(j.surplus || 0)]),
        buildingScores: mapBits(p?.buildingScores),
        dailyFocus: p?.dailyFocus?.id ? { id: p.dailyFocus.id, forced: p.dailyFocus.forced || "" } : null,
        siteWatch: Object.entries(p?.siteWatch?.sites || {})
          .filter(([, w]) => Number.isFinite(w?.stalledSinceDay))
          .map(([id, w]) => [id, bits(w.stalledSinceDay)]),
      },
      stockReport: rapport(c.stockReport),
      charter: c.charter ? { themeId: c.charter.themeId, untilDay: bits(c.charter.untilDay) } : null,
    } : null,
    market: mapBits(sim.market?.stock),
    scarce: [bits(sim.archetype?.scarceSeed?.wood || 0), bits(sim.archetype?.scarceSeed?.stone || 0)],
    tiles,
    rng: sim.rng.state(),
  };
}

function rapport(r) {
  if (!r) return null;
  return {
    day: bits(r.day), lastRefreshDay: bits(r.lastRefreshDay ?? -1), stock: mapBits(r.stock),
    rumor: r.rumor ? { resource: r.rumor.resource, mul: bits(r.rumor.mul), untilDay: bits(r.rumor.untilDay), cause: r.rumor.cause } : null,
    blind: (r.blind || []).map((b) => [b.resource, bits(b.missed), b.buildingType, b.buildingId || "", bits(b.dist)]),
    certifiedNear: bits(r.certifiedNear || 0), ignoredFar: bits(r.ignoredFar || 0),
  };
}

const GOALS = ["build", "craft", "gatherFood", "helpFarm", "gatherWood", "gatherStone", "haulJob", "haul", "haulCart",
  "deliver", "sell", "buy", "fetchInput", "maintain", "explore"];

function decisions(sim) {
  const out = [];
  for (const npc of sim.actors) {
    const need = cp.collectiveBuildingNeedScore(sim);
    const goalBias = GOALS.map((g) => [g, bits(cp.collectiveGoalBias(sim, g))]);
    const floors = Object.keys(sim.colony?.priorities?._effects?.goalFloor || {}).map((g) => [g, bits(cp.collectiveGoalFloor(sim, g))]);
    const urgency = Object.entries(urgencyMap(sim, npc)).map(([g, v]) => [g, bits(v)]);
    out.push({
      id: npc.id, need: bits(need), goalBias, goalFloor: floors, urgency,
      draftee: cp.isWoodBootstrapDraftee(sim, npc), foodRush: cp.isFoodRush(sim), farmGap: cp.farmStaffingGap(sim).gap,
    });
  }
  return out;
}

function ecritures(sim) {
  const p = sim.colony?.priorities;
  return {
    buildings: sim.buildings.map((b) => ({
      id: b.id, vacantSinceDay: optBits(b.vacantSinceDay),
      stock: b.stock && typeof b.stock === "object" ? Object.entries(b.stock).map(([res, slot]) => [res, slot?.physical | 0, slot?.reserved | 0]) : null,
    })),
    stockReport: rapport(sim.colony?.stockReport),
    charter: sim.colony?.charter ? { themeId: sim.colony.charter.themeId, untilDay: bits(sim.colony.charter.untilDay) } : null,
    effectsBias: p?._effects ? mapBits(p._effects.goalBias) : null,
    effectsFloor: p?._effects ? mapBits(p._effects.goalFloor) : null,
    woodDraft: p?._woodDraft ? [...p._woodDraft] : null,
    rng: sim.rng.state(),
  };
}

function bruts(sim) {
  return {
    jobs: cp.measureJobNeeds(sim) ? Object.values(cp.measureJobNeeds(sim)).map((j) => [j.jobId, bits(j.current), bits(j.needed), bits(j.need), bits(j.surplus)]) : [],
    scores: mapBits(cp.scoreBuildingProjects(sim)),
    boosted: mapBits(cp.boostedBuildingScores(sim)),
    needScore: bits(cp.collectiveBuildingNeedScore(sim)),
    pending: [cp.exploitSpinePending(sim), cp.villageAmenityPending(sim), cp.villageCraftPending(sim), cp.villageHerdPending(sim), cp.craftBootstrapPending(sim)].map((x) => x || ""),
    forest: bits(doctrine.frontierForestDensity(sim)),
    rng: sim.rng.state(),
  };
}

const bases = new Map();
const fixtures = [];
for (const v of VARIANTES) {
  if (!bases.has(v.tick)) bases.set(v.tick, etatAuTick(v.tick));
  // L'etat de la variante, fige une fois : chaque mesure part d'une copie fraiche.
  const prepare = copie(bases.get(v.tick));
  nextId = 900;
  v.poser(prepare);
  const etat = JSON.stringify(save.serialize(prepare));
  const a = copie(etat);
  const entree = vue(a);
  const decs = decisions(a);
  const ecr = ecritures(a);
  const b = copie(etat);
  const brut = bruts(b);
  fixtures.push({ nom: v.nom, vue: entree, decisions: decs, ecritures: ecr, bruts: brut });
  console.error(`${v.nom} : besoin ${cp.collectiveBuildingNeedScore(copie(etat))}, ${decs.length} habitants, rng ${entree.rng} -> ${ecr.rng}, pending ${brut.pending.join("/")}`);
}

mkdirSync(dirname(OUT), { recursive: true });
writeFileSync(OUT, JSON.stringify({ reference: "anastasis-ref-p3", scenario: "endurance", fixtures }, null, 0) + "\n", "utf8");
console.error(`Ecrit: ${OUT} — ${fixtures.length} variantes`);
