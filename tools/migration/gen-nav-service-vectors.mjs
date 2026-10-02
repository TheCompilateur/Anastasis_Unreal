// Vecteurs de parite du service de navigation: JS -> C++.
//
//   node tools/migration/gen-nav-service-vectors.mjs -ref <clone de anastasis-ref-p3>
//        [-out <.inl>] [-dump <fichier texte>]
//
// `navService.js` est un module A ETAT: une file, un cache ordonne, un budget
// par tick. Comparer ses fonctions une a une ne dit pas grand-chose; ce qui
// compte est la SUITE des decisions — quel PNJ obtient un A* ce tick, quel
// chemin le cache de zone sert a qui, quelle entree le garde-fou memoire efface.
//
// D'ou des scenarios: une suite d'operations (avancer le temps, ouvrir un tick,
// demander un chemin, vider la file, deplacer un PNJ, changer sa cible, le
// retirer...) rejouee des deux cotes. Apres CHAQUE operation, l'etat complet du
// service — file, index des attentes, cache dans son ordre, champs de chemin de
// chaque PNJ, metriques, anneau de trace — est ecrit dans un texte canonique,
// dont le .inl garde le SHA-1. Le C++ ecrit le meme texte; le premier pas ou
// les empreintes different nomme l'operation fautive. `-dump` ecrit les textes
// JS complets, pour lire la difference a l'oeil.
//
// Le monde est celui de gen-nav-vectors.mjs: `generateWorld`, l'eau bloquee,
// `rebuildMoveCosts`. Pas de `Simulation`: le service ne lit sur `sim` que le
// temps, la vitesse, la version de navigation, les acteurs et l'A*.

import { createHash } from "node:crypto";
import { writeFileSync, mkdirSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";

const ICI = dirname(fileURLToPath(import.meta.url));
const RACINE = join(ICI, "..", "..");

const argv = process.argv.slice(2);
const argOf = (f, d) => { const i = argv.indexOf(f); return i >= 0 && argv[i + 1] !== undefined ? argv[i + 1] : d; };
const REF = argOf("-ref", null);
if (!REF) {
  console.error("-ref <clone de anastasis-ref-p3> requis (docs/migration/phase3/REFERENCE_JS.md)");
  process.exit(2);
}
const REF_DIR = REF.split(String.fromCharCode(92)).join("/");
const SORTIE = argOf("-out", join(RACINE, "Source", "AnastasisSim", "Private", "Tests", "AnastasisNavServiceVectors.inl"));
const DUMP = argOf("-dump", null);

const modUrl = (rel) => pathToFileURL(join(REF_DIR, rel)).href;
const { generateWorld } = await import(modUrl("src/sim/world.js"));
const { rebuildMoveCosts, footBlockedAt, moveCostAt, createNavMetrics, navigationTargetKey } =
  await import(modUrl("src/sim/navGrid.js"));
const NS = await import(modUrl("src/sim/navService.js"));

// --- Encodage ----------------------------------------------------------------

const bitsHex = (x) => {
  const v = new DataView(new ArrayBuffer(8));
  v.setFloat64(0, x, true);
  return v.getBigUint64(0, true).toString(16).padStart(16, "0");
};
const bits = (x) => `0x${bitsHex(x)}ull`;

/** FNV-1a 32 sur des octets ASCII. */
function fnv32(text) {
  let h = 0x811c9dc5;
  for (let i = 0; i < text.length; i += 1) {
    h ^= text.charCodeAt(i);
    h = Math.imul(h, 0x01000193) >>> 0;
  }
  return h.toString(16).padStart(8, "0");
}

/** Un chemin: sa longueur et l'empreinte de ses points, motif binaire compris. */
function pathSig(path) {
  if (!path) return "0:00000000";
  let s = "";
  for (const p of path) s += `${bitsHex(p.x)},${bitsHex(p.y)};`;
  return `${path.length}:${fnv32(s)}`;
}

const str = (v) => (v == null ? "" : String(v));
const sha1 = (text) => createHash("sha1").update(text, "utf8").digest("hex");

/** Chaine C, ASCII seulement (voir PORTAGE.md: pas de TCHAR dans un initialiseur statique). */
const cstr = (s) => `"${String(s).replace(/\\/g, "\\\\").replace(/"/g, '\\"')}"`;

// --- Le monde ----------------------------------------------------------------

const W = 64;
const H = 64;

function makeNavSim(seed) {
  const world = generateWorld(seed, W, H);
  const sim = { w: W, h: H, tiles: world.tiles };
  sim.blocked = new Uint8Array(W * H);
  for (let i = 0; i < W * H; i += 1) {
    if (sim.tiles[i].type === "water") sim.blocked[i] = 1;
  }
  sim.blockedAt = (x, y) => (x < 0 || y < 0 || x >= W || y >= H ? true : sim.blocked[y * W + x] === 1);
  sim.tileAt = (x, y) => (x < 0 || y < 0 || x >= W || y >= H ? null : sim.tiles[y * W + x]);
  rebuildMoveCosts(sim);
  sim.footBlockedAt = (x, y) => footBlockedAt(sim, x, y);
  sim.tileTraversalCost = (x, y, baseCost = 10) => {
    const mult = moveCostAt(sim, x, y);
    if (!Number.isFinite(mult)) return Infinity;
    return Math.max(3, baseCost * mult);
  };
  return sim;
}

function freeCells(sim) {
  const out = [];
  for (let y = 0; y < H; y += 1) {
    for (let x = 0; x < W; x += 1) {
      if (!sim.footBlockedAt(x, y)) out.push({ x, y });
    }
  }
  return out;
}

function waterCells(sim) {
  const out = [];
  for (let y = 0; y < H; y += 1) {
    for (let x = 0; x < W; x += 1) {
      if (sim.tiles[y * W + x].type === "water") out.push({ x, y });
    }
  }
  return out;
}

// mulberry32 — le hasard du GENERATEUR, pas celui de la simulation.
function rngOf(seed) {
  let a = seed >>> 0;
  return () => {
    a = (a + 0x6d2b79f5) >>> 0;
    let t = a;
    t = Math.imul(t ^ (t >>> 15), t | 1);
    t ^= t + Math.imul(t ^ (t >>> 7), t | 61);
    return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
  };
}

// --- Les operations ----------------------------------------------------------
// Le C++ interprete la meme table (AnastasisNavServiceTests.cpp). Un code
// ajoute ici s'ajoute la-bas.

const OP = {
  time: 0, // A = sim.time
  speed: 1, // A = sim.speedScale
  version: 2, // I = sim.navVersion (le cache n'est PAS vide: entrees perimees)
  begin: 3, // A = budgetMul
  request: 4, // actor, A/B = cible, I = priorite (-1 absente), J = allowBlockedTarget (-1 absente)
  process: 5, // I = maxJobs (-1 absent), actor = favorActorId (-1 absent)
  move: 6, // actor, A/B = position
  goal: 7, // actor, texte = goal, A = hunger, B = thirst, C = energy
  target: 8, // actor, I = 1 cible A/B, 0 aucune
  remove: 9, // actor quitte sim.actors
  restore: 10, // actor revient a la fin de sim.actors
  clear: 11, // clearNavCache
  sweep: 12, // sweepNavCache
  lookup: 13, // A/B depart, C/D cible
  cacheEmpty: 14, // sim.navService.cache.clear() — ce que fait bumpNavVersion
};

const ACTOR_COUNT = 8;

function runScenario(nom, seed, script) {
  const sim = makeNavSim(seed);
  sim.time = 0;
  sim.speedScale = 1;
  sim.navVersion = 0;
  sim.navMetrics = createNavMetrics();
  sim.navService = NS.createNavService();
  const roster = [];
  for (let i = 0; i < ACTOR_COUNT; i += 1) {
    roster.push({
      id: `npc-${i}`,
      x: 0.5, y: 0.5,
      goal: "wander", hunger: 0, thirst: 0, energy: 100,
      target: null,
      path: null, pathStep: 0, pathGoal: null, pathFailed: false, pathFailStreak: 0, pathCooldown: 0,
    });
  }
  sim.actors = roster.slice();

  const ops = script(sim, roster);
  const steps = [];
  for (const op of ops) {
    const result = apply(sim, roster, op);
    const text = canon(sim, roster, result);
    steps.push({ op, text, sha: sha1(text), queue: sim.navService.queue.length, cache: sim.navService.cache.size });
  }
  return { nom, seed, steps };
}

function apply(sim, roster, op) {
  const actor = op.actor >= 0 ? roster[op.actor] : null;
  switch (op.kind) {
    case OP.time: sim.time = op.a; return "";
    case OP.speed: sim.speedScale = op.a; return "";
    case OP.version: sim.navVersion = op.i; return "";
    case OP.begin: NS.beginNavTick(sim, { budgetMul: op.a }); return "";
    case OP.request: {
      const options = {};
      if (op.i >= 0) options.priority = op.i;
      if (op.j >= 0) options.allowBlockedTarget = op.j === 1;
      const path = NS.requestPath(sim, actor, { x: op.a, y: op.b }, options);
      return path ? `path:${pathSig(path)}` : "null";
    }
    case OP.process: {
      const options = {};
      if (op.i >= 0) options.maxJobs = op.i;
      if (actor) options.favorActorId = actor.id;
      return String(NS.processNavQueue(sim, options));
    }
    case OP.move: actor.x = op.a; actor.y = op.b; return "";
    case OP.goal:
      actor.goal = op.text; actor.hunger = op.a; actor.thirst = op.b; actor.energy = op.c;
      return "";
    case OP.target: actor.target = op.i === 1 ? { x: op.a, y: op.b } : null; return "";
    case OP.remove: {
      const idx = sim.actors.indexOf(actor);
      if (idx >= 0) sim.actors.splice(idx, 1);
      return "";
    }
    case OP.restore: if (!sim.actors.includes(actor)) sim.actors.push(actor); return "";
    case OP.clear: NS.clearNavCache(sim); return "";
    case OP.sweep: return String(NS.sweepNavCache(sim));
    case OP.lookup: {
      const path = NS.lookupCachedPath(sim, { x: op.a, y: op.b }, { x: op.c, y: op.d });
      return path ? `path:${pathSig(path)}` : "null";
    }
    case OP.cacheEmpty: sim.navService.cache.clear(); return "";
    default: throw new Error(`operation inconnue ${op.kind}`);
  }
}

/**
 * Le texte canonique. AnastasisNavServiceTests.cpp ecrit EXACTEMENT le meme:
 * une ligne par element, champs separes par `|`, doubles par leur motif.
 */
function canon(sim, roster, result) {
  const svc = sim.navService;
  const L = [];
  L.push(`R|${result}`);
  L.push(`S|${svc.calcThisTick}|${svc.budget.maxCalcs}|${bitsHex(svc.cacheTtl)}|${bitsHex(svc._lastSweepAt)}`);
  for (const job of svc.queue) {
    L.push(`Q|${job.actorId}|${job.priority}|${bitsHex(job.requestedAt)}|${str(job.goalKey)}|${job.allowBlockedTarget ? 1 : 0}`
      + `|${bitsHex(job.destination.x)}|${bitsHex(job.destination.y)}|${bitsHex(job.start.x)}|${bitsHex(job.start.y)}`);
  }
  const pend = [...svc.pendingByActor.entries()].sort((a, b) => (a[0] < b[0] ? -1 : a[0] > b[0] ? 1 : 0));
  L.push(`P|${pend.map(([k, v]) => `${k}=${v}`).join(";")}`);
  for (const [key, entry] of svc.cache) {
    L.push(`C|${key}|${entry.navVersion}|${bitsHex(entry.storedAt)}|${pathSig(entry.path)}`);
  }
  for (const a of roster) {
    const nav = a.navigation;
    L.push(`A|${a.id}|${sim.actors.includes(a) ? 1 : 0}|${bitsHex(a.x)}|${bitsHex(a.y)}|${pathSig(a.path)}|${a.pathStep}`
      + `|${a.pathGoal ? `${bitsHex(a.pathGoal.x)},${bitsHex(a.pathGoal.y)}` : "-"}`
      + `|${a.pathFailed ? 1 : 0}|${a.pathFailStreak | 0}|${bitsHex(a.pathCooldown)}`
      + `|${str(nav?.targetKey)}|${bitsHex(nav ? nav.requestedAt : 0)}|${nav ? nav.navVersion : -1}`);
  }
  const m = sim.navMetrics;
  L.push(`M|${m.pathRequests}|${m.pathHits}|${m.pathFails}|${m.cacheHits}|${m.cacheMisses}|${m.cacheSize}`
    + `|${m.queueDepth}|${m.queueSolved}|${m.calcsThisTick}|${m.pathQueueEnqueued}|${m.pathCacheHits}`
    + `|${m.navTraceNext | 0}|${m.navTraceTotal | 0}`);
  for (const t of m.navTrace) {
    L.push(`T|${t.type}|${bitsHex(t.time)}|${str(t.actorId)}|${str(t.goal)}|${str(t.targetKey)}|${t.navVersion}`
      + `|${str(t.from)}|${str(t.to)}|${str(t.reason)}|${t.pathLength == null ? -1 : t.pathLength}|${t.queueDepth}`);
  }
  return L.join("\n") + "\n";
}

// --- Les scenarios -------------------------------------------------------------

const op = (kind, fields = {}) => ({
  kind, actor: -1, a: 0, b: 0, c: 0, d: 0, i: -1, j: -1, text: "", ...fields,
});
const center = (cell) => ({ x: cell.x + 0.5, y: cell.y + 0.5 });

/** Un tick comme simulation.js l'ouvre: temps, beginNavTick, file, PNJ, file. */
function tickOps(t, mul, middle) {
  return [op(OP.time, { a: t }), op(OP.begin, { a: mul }), op(OP.process), ...middle, op(OP.process)];
}

const scenarios = [];

// 1. Les branches une a une, dans l'ordre ou un lecteur les cherche.
scenarios.push(runScenario("branches", 33344, (sim) => {
  const free = freeCells(sim);
  const rnd = rngOf(1);
  const pick = () => center(free[Math.floor(rnd() * free.length)]);
  const water = waterCells(sim);
  const hub = pick();
  const ops = [];
  // Places de depart: deux voisins dans la meme zone 8x8, les autres disperses.
  const base = free.find((c) => c.x % 8 === 2 && c.y % 8 === 2 && free.some((d) => d.x === c.x + 1 && d.y === c.y));
  const starts = [center(base), { x: base.x + 1.5, y: base.y + 0.5 }];
  for (let i = 2; i < ACTOR_COUNT; i += 1) starts.push(pick());
  starts.forEach((s, i) => ops.push(op(OP.move, { actor: i, a: s.x, b: s.y })));
  // Cible commune pour tous, puis la meme pour le voisin: exact puis zone.
  ops.push(...tickOps(1, 1, [
    op(OP.target, { actor: 0, a: hub.x, b: hub.y, i: 1 }),
    op(OP.request, { actor: 0, a: hub.x, b: hub.y }),
    op(OP.request, { actor: 1, a: hub.x, b: hub.y }),
    op(OP.request, { actor: 0, a: hub.x, b: hub.y }),
  ]));
  // Budget a sec: budgetMul 0.15 -> 1 calcul. Tous les autres attendent.
  ops.push(...tickOps(2, 0.15, [2, 3, 4, 5, 6, 7].map((i) => op(OP.request, { actor: i, a: hub.x, b: hub.y }))));
  // Priorites: un affame, un livreur, un flaneur, un fuyard (option explicite).
  ops.push(op(OP.goal, { actor: 2, text: "eat", a: 90, b: 0, c: 100 }));
  ops.push(op(OP.goal, { actor: 3, text: "deliver", a: 0, b: 0, c: 100 }));
  ops.push(op(OP.goal, { actor: 4, text: "rest", a: 0, b: 0, c: 0 }));
  ops.push(op(OP.goal, { actor: 5, text: "rest", a: 0, b: 0, c: 10 }));
  const far = pick();
  ops.push(...tickOps(3, 0.15, [
    op(OP.request, { actor: 4, a: far.x, b: far.y }),
    op(OP.request, { actor: 5, a: far.x, b: far.y }),
    op(OP.request, { actor: 3, a: far.x, b: far.y }),
    op(OP.request, { actor: 2, a: far.x, b: far.y }),
    op(OP.request, { actor: 6, a: far.x, b: far.y, i: 0 }),
  ]));
  // Faveur et maxJobs.
  ops.push(op(OP.time, { a: 4 }), op(OP.begin, { a: 1 }));
  ops.push(op(OP.process, { i: 1, actor: 7 }));
  ops.push(op(OP.process, { i: 0 }));
  ops.push(op(OP.process));
  // Une demande deja en file, mise a jour: nouvelle cible, priorite qui ne remonte pas.
  ops.push(op(OP.time, { a: 5 }), op(OP.begin, { a: 0.15 }));
  const other = pick();
  ops.push(op(OP.request, { actor: 0, a: other.x, b: other.y }));
  ops.push(op(OP.request, { actor: 1, a: other.x, b: other.y }));
  ops.push(op(OP.request, { actor: 1, a: far.x, b: far.y, i: 3 }));
  ops.push(op(OP.request, { actor: 1, a: hub.x, b: hub.y, i: 0, j: 0 }));
  // Cible changee entre la demande et la resolution: le job est jete.
  ops.push(op(OP.target, { actor: 1, a: other.x, b: other.y, i: 1 }));
  // Acteur retire: son job est abandonne.
  ops.push(op(OP.remove, { actor: 0 }));
  ops.push(op(OP.time, { a: 6 }), op(OP.begin, { a: 1 }), op(OP.process));
  ops.push(op(OP.restore, { actor: 0 }));
  // Un job en file resolu PAR LE CACHE (source "cache"): le 4 attend, le 5 se
  // met sur sa case et calcule le meme trajet en ligne, puis la file sert le 4.
  // Cible neuve: aucune entree, ni exacte ni de zone, ne peut la servir.
  const t4 = pick();
  const near4 = free.find((c) => Math.abs(c.x - Math.floor(t4.x)) + Math.abs(c.y - Math.floor(t4.y)) === 5) || free[0];
  const s4 = center(near4);
  ops.push(op(OP.move, { actor: 4, a: s4.x, b: s4.y }));
  ops.push(op(OP.goal, { actor: 4, text: "wander", a: 0, b: 0, c: 100 }));
  ops.push(op(OP.target, { actor: 4, i: 0 }));
  ops.push(op(OP.time, { a: 6.5 }), op(OP.begin, { a: 0.15 }));
  ops.push(op(OP.request, { actor: 7, a: other.x, b: other.y, i: 0 }));
  ops.push(op(OP.request, { actor: 4, a: t4.x, b: t4.y }));
  ops.push(op(OP.time, { a: 6.6 }), op(OP.begin, { a: 1 }));
  ops.push(op(OP.process, { i: 0 }));
  ops.push(op(OP.move, { actor: 5, a: s4.x, b: s4.y }));
  ops.push(op(OP.request, { actor: 5, a: t4.x, b: t4.y }));
  ops.push(op(OP.process));
  // Depart = arrivee: chemin vide, rangé comme un echec, mais mis en cache.
  const here = starts[6];
  ops.push(op(OP.move, { actor: 6, a: here.x, b: here.y }));
  ops.push(...tickOps(7, 1, [
    op(OP.request, { actor: 6, a: here.x, b: here.y }),
    op(OP.request, { actor: 6, a: here.x, b: here.y }),
  ]));
  // L'eau: refus, puis tolerance.
  if (water.length) {
    const w = center(water[Math.floor(water.length / 2)]);
    ops.push(...tickOps(8, 1, [
      op(OP.request, { actor: 7, a: w.x, b: w.y }),
      op(OP.request, { actor: 5, a: w.x, b: w.y, j: 1 }),
    ]));
  }
  // Lecture directe, puis TTL: le cache vieillit au-dela de 20 s.
  ops.push(op(OP.lookup, { a: starts[0].x, b: starts[0].y, c: hub.x, d: hub.y }));
  ops.push(op(OP.lookup, { a: starts[1].x + 3, b: starts[1].y, c: hub.x, d: hub.y }));
  ops.push(op(OP.time, { a: 27.5 }));
  ops.push(op(OP.lookup, { a: starts[0].x, b: starts[0].y, c: hub.x, d: hub.y }));
  ops.push(op(OP.begin, { a: 1 }));
  // Version: entrees perimees a la lecture, puis sweep explicite.
  ops.push(...tickOps(28, 1, [op(OP.request, { actor: 3, a: hub.x, b: hub.y })]));
  ops.push(op(OP.version, { i: 1 }));
  ops.push(op(OP.lookup, { a: starts[3].x, b: starts[3].y, c: hub.x, d: hub.y }));
  ops.push(op(OP.sweep));
  ops.push(...tickOps(29, 1, [op(OP.request, { actor: 3, a: hub.x, b: hub.y })]));
  ops.push(op(OP.cacheEmpty), op(OP.clear));
  // Vitesses: TTL, sweep et budget suivent le fat step.
  for (const speed of [2, 5, 10, 0]) {
    ops.push(op(OP.speed, { a: speed }));
    ops.push(...tickOps(30 + speed, 1, [op(OP.request, { actor: 2, a: hub.x, b: hub.y })]));
  }
  // budgetMul hors bornes et NaN.
  for (const mul of [NaN, 0, 3, -1, 0.5]) ops.push(op(OP.begin, { a: mul }));
  return ops;
}));

// 2. Le garde-fou memoire: plus de 480 cles, les 80 premieres s'en vont.
// Departs a 14 cases au plus de trois cibles: au-dela, `maxCost` (320 = 32 pas
// droits) refuse le chemin et rien n'entre au cache. Le temps avance de 0.02 s
// par demande: tout reste frais (TTL 20 s), seul le garde-fou retire.
scenarios.push(runScenario("garde-fou", 7, (sim) => {
  const free = freeCells(sim);
  const rnd = rngOf(2);
  // Beaucoup de cibles: le cache de ZONE sert toute demande dont la zone 8x8
  // et la cible sont deja connues, et avec trois cibles il servait presque tout.
  const targets = [];
  for (let k = 0; k < 60; k += 1) targets.push(free[Math.floor(rnd() * free.length)]);
  const near = targets.map((t) => free.filter((c) => Math.max(Math.abs(c.x - t.x), Math.abs(c.y - t.y)) <= 14));
  const ops = [];
  let t = 0;
  for (let n = 0; n < 700; n += 1) {
    const k = Math.floor(rnd() * targets.length);
    const s = center(near[k][Math.floor(rnd() * near[k].length)]);
    const target = center(targets[k]);
    t += 0.02;
    ops.push(op(OP.time, { a: t }), op(OP.begin, { a: 1 }));
    ops.push(op(OP.move, { actor: n % ACTOR_COUNT, a: s.x, b: s.y }));
    ops.push(op(OP.request, { actor: n % ACTOR_COUNT, a: target.x, b: target.y }));
  }
  return ops;
}));

// 3. Des ticks au hasard: ce que la vie d'un village fait au service.
for (const [nom, seed, genSeed] of [["hasard-a", 1204, 11], ["hasard-b", 33344, 12], ["hasard-c", 7, 13]]) {
  scenarios.push(runScenario(nom, seed, (sim) => {
    const free = freeCells(sim);
    const water = waterCells(sim);
    const rnd = rngOf(genSeed);
    const pickCell = () => free[Math.floor(rnd() * free.length)];
    const hubs = [center(pickCell()), center(pickCell()), center(pickCell())];
    const goals = ["eat", "drink", "rest", "flee", "deliver", "build", "wander", "explore", "idle", "socialize"];
    const ops = [];
    for (let i = 0; i < ACTOR_COUNT; i += 1) {
      const c = center(pickCell());
      ops.push(op(OP.move, { actor: i, a: c.x, b: c.y }));
    }
    let t = 0;
    let version = 0;
    const speeds = [1, 1, 1, 2, 5, 10];
    for (let tick = 0; tick < 120; tick += 1) {
      if (rnd() < 0.05) ops.push(op(OP.speed, { a: speeds[Math.floor(rnd() * speeds.length)] }));
      t += 0.25 + rnd() * 2;
      const mul = rnd() < 0.3 ? 0.15 + rnd() * 0.85 : 1;
      const middle = [];
      const n = 1 + Math.floor(rnd() * 5);
      for (let k = 0; k < n; k += 1) {
        const actor = Math.floor(rnd() * ACTOR_COUNT);
        const r = rnd();
        if (r < 0.5) {
          const hub = hubs[Math.floor(rnd() * hubs.length)];
          const opts = {};
          if (rnd() < 0.1) opts.i = Math.floor(rnd() * 4);
          if (rnd() < 0.05) opts.j = rnd() < 0.5 ? 1 : 0;
          middle.push(op(OP.request, { actor, a: hub.x, b: hub.y, ...opts }));
        } else if (r < 0.62) {
          const c = center(pickCell());
          middle.push(op(OP.request, { actor, a: c.x, b: c.y }));
        } else if (r < 0.75) {
          // Petit pas: reste souvent dans la meme zone 8x8.
          middle.push(op(OP.move, {
            actor,
            a: Math.min(W - 0.5, Math.max(0.5, 0.5 + Math.floor((rnd() - 0.5) * 6) + (rnd() < 0.5 ? 10 : 30))),
            b: Math.min(H - 0.5, Math.max(0.5, 0.5 + Math.floor(rnd() * 60))),
          }));
        } else if (r < 0.83) {
          middle.push(op(OP.goal, {
            actor, text: goals[Math.floor(rnd() * goals.length)],
            a: Math.floor(rnd() * 100), b: Math.floor(rnd() * 100), c: Math.floor(rnd() * 100),
          }));
        } else if (r < 0.9) {
          const hub = hubs[Math.floor(rnd() * hubs.length)];
          middle.push(rnd() < 0.7
            ? op(OP.target, { actor, a: hub.x, b: hub.y, i: 1 })
            : op(OP.target, { actor, i: 0 }));
        } else if (r < 0.94) {
          middle.push(op(rnd() < 0.5 ? OP.remove : OP.restore, { actor }));
        } else if (r < 0.97 && water.length) {
          const w = center(water[Math.floor(rnd() * water.length)]);
          middle.push(op(OP.request, { actor, a: w.x, b: w.y, j: rnd() < 0.5 ? 1 : -1 }));
        } else {
          version += 1;
          middle.push(op(OP.version, { i: version }));
          if (rnd() < 0.5) middle.push(op(OP.cacheEmpty));
        }
      }
      ops.push(...tickOps(t, mul, middle));
    }
    return ops;
  }));
}

// --- Fonctions pures -----------------------------------------------------------

const SPEEDS = [NaN, -1, 0, 0.5, 1, 1 + 1e-12, 1 + 1e-8, 1.5, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 20, 100, Infinity];
for (let s = 1; s <= 10.0001; s += 0.05) SPEEDS.push(Math.round(s * 100) / 100);
const speedRows = SPEEDS.map((s) => ({
  s, mult: NS.navStepMultForSpeed(s), ttl: NS.navCacheTtlForSpeed(s),
  sweep: NS.navCacheSweepIntervalForSpeed(s), calcs: NS.pathBudgetForSpeed(s).maxCalcs,
}));

const KEY_POINTS = [
  [{ x: 3.5, y: 4.5 }, { x: 10.2, y: 11.9 }],
  [{ x: 0, y: 0 }, { x: 0, y: 0 }],
  [{ x: -0.5, y: -0.0 }, { x: -7.99, y: 63.999 }],
  [{ x: 15.99, y: 16 }, { x: 8, y: 7.999999999 }],
  [{ x: -9, y: -17 }, { x: 1e6 + 0.5, y: -1e6 - 0.5 }],
  [{ x: NaN, y: 2 }, { x: Infinity, y: -Infinity }],
];
const keyRows = [];
for (const [s, t] of KEY_POINTS) {
  for (const v of [0, 7, -3]) {
    for (const zone of [false, true]) keyRows.push({ s, t, v, zone, key: NS.cacheKeyFor(s, t, v, zone) });
  }
}
const targetKeyRows = [
  { x: 3.7, y: 9.2 }, { x: -0.1, y: -0.0 }, { x: NaN, y: 1 }, { x: 1, y: Infinity }, { x: 1e9 + 0.25, y: -2.5 },
].map((p) => ({ p, key: navigationTargetKey(p) }));

const GOALS = ["flee", "shelter", "eat", "drink", "rest", "deliver", "sell", "craft", "build", "haulJob",
  "gatherWood", "gatherStone", "gatherFood", "helpFarm", "explore", "observer", "wander", "socialize", "idle", ""];
const NEEDS = [[0, 0, 100], [79, 0, 100], [78, 78, 18], [0, 79, 17.9], [NaN, NaN, NaN], [0, 0, 0], [100, 100, 17]];
const priorityRows = [];
for (const g of GOALS) {
  for (const [hunger, thirst, energy] of NEEDS) {
    priorityRows.push({ g, hunger, thirst, energy, p: NS.priorityForGoal(g, { goal: g, hunger, thirst, energy }) });
  }
}

// --- Emission --------------------------------------------------------------------

const L = [];
const emit = (s = "") => L.push(s);
emit("// GENERE AUTOMATIQUEMENT - ne pas editer a la main.");
emit("// Source: tools/migration/gen-nav-service-vectors.mjs (reference anastasis-ref-p3)");
emit("//");
emit("// Fonctions pures de src/sim/navService.js, puis scenarios: une suite");
emit("// d'operations rejouee des deux cotes, et le SHA-1 du texte canonique de");
emit("// l'etat du service apres chacune. Ne jamais corriger un vecteur a la main.");
emit("");
emit("// clang-format off");
emit("");
emit(`static constexpr int32 NavServiceWorldW = ${W};`);
emit(`static constexpr int32 NavServiceWorldH = ${H};`);
emit(`static constexpr int32 NavServiceActorCount = ${ACTOR_COUNT};`);
emit("");
emit("struct FNavServiceSpeedVector { uint64 SpeedBits; uint64 StepMultBits; uint64 TtlBits; uint64 SweepBits; int32 MaxCalcs; };");
emit("static const FNavServiceSpeedVector NavServiceSpeedVectors[] = {");
for (const r of speedRows) emit(`\t{ ${bits(r.s)}, ${bits(r.mult)}, ${bits(r.ttl)}, ${bits(r.sweep)}, ${r.calcs} },`);
emit("};");
emit("");
emit("struct FNavServiceKeyVector { uint64 SX; uint64 SY; uint64 TX; uint64 TY; int32 Version; int32 Zone; const char* Key; };");
emit("static const FNavServiceKeyVector NavServiceKeyVectors[] = {");
for (const r of keyRows) {
  emit(`\t{ ${bits(r.s.x)}, ${bits(r.s.y)}, ${bits(r.t.x)}, ${bits(r.t.y)}, ${r.v}, ${r.zone ? 1 : 0}, ${cstr(r.key)} },`);
}
emit("};");
emit("");
emit("struct FNavServiceTargetKeyVector { uint64 X; uint64 Y; const char* Key; };");
emit("static const FNavServiceTargetKeyVector NavServiceTargetKeyVectors[] = {");
for (const r of targetKeyRows) emit(`\t{ ${bits(r.p.x)}, ${bits(r.p.y)}, ${cstr(str(r.key))} },`);
emit("};");
emit("");
emit("struct FNavServicePriorityVector { const char* Goal; uint64 Hunger; uint64 Thirst; uint64 Energy; int32 Priority; };");
emit("static const FNavServicePriorityVector NavServicePriorityVectors[] = {");
for (const r of priorityRows) {
  emit(`\t{ ${cstr(r.g)}, ${bits(r.hunger)}, ${bits(r.thirst)}, ${bits(r.energy)}, ${r.p} },`);
}
emit("};");
emit("");
emit("struct FNavServiceOp {");
emit("\tint32 Kind; int32 Actor;");
emit("\tuint64 A; uint64 B; uint64 C; uint64 D;");
emit("\tint32 I; int32 J;");
emit("\tconst char* Text;");
emit("\tconst char* Sha1;       // empreinte du texte canonique APRES l'operation");
emit("\tint32 QueueLength; int32 CacheSize;");
emit("};");
emit("");
emit("struct FNavServiceScenario { const char* Name; uint32 Seed; int32 FirstOp; int32 OpCount; };");
emit("");
emit("static const FNavServiceOp NavServiceOps[] = {");
let first = 0;
const scenarioRows = [];
const dump = [];
for (const sc of scenarios) {
  scenarioRows.push(`\t{ ${cstr(sc.nom)}, ${sc.seed}u, ${first}, ${sc.steps.length} },`);
  sc.steps.forEach((st, index) => {
    const o = st.op;
    emit(`\t{ ${o.kind}, ${o.actor}, ${bits(o.a)}, ${bits(o.b)}, ${bits(o.c)}, ${bits(o.d)}, ${o.i}, ${o.j}, `
      + `${cstr(o.text)}, "${st.sha}", ${st.queue}, ${st.cache} },`);
    if (DUMP) dump.push(`=== ${sc.nom} #${index} op=${o.kind}\n${st.text}`);
  });
  first += sc.steps.length;
}
emit("};");
emit("");
emit("static const FNavServiceScenario NavServiceScenarios[] = {");
for (const r of scenarioRows) emit(r);
emit("};");

mkdirSync(dirname(SORTIE), { recursive: true });
writeFileSync(SORTIE, L.join("\n") + "\n", "utf8");
if (DUMP) writeFileSync(DUMP, dump.join(""), "utf8");

const stats = scenarios.map((sc) => {
  const maxCache = Math.max(...sc.steps.map((s) => s.cache));
  const maxQueue = Math.max(...sc.steps.map((s) => s.queue));
  return `${sc.nom}: ${sc.steps.length} ops, file max ${maxQueue}, cache max ${maxCache}`;
});
console.error(`Ecrit: ${SORTIE}\n  ${speedRows.length} vitesses, ${keyRows.length} cles, ${priorityRows.length} priorites\n  ${stats.join("\n  ")}`);
