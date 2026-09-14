// Vecteurs de parite de la navigation: JS -> C++.
//
// PORTAGE.md pose l'exigence: « l'ordre d'exploration de l'A* doit etre
// identique : deux chemins de meme cout, et les PNJ ne prennent pas la meme
// rue ». Un chemin de meme longueur n'est pas le meme chemin, et un vecteur qui
// ne comparerait que le cout total laisserait passer exactement l'erreur qu'on
// craint.
//
// D'ou des vecteurs qui comparent la SUITE COMPLETE des points, chacun par son
// motif binaire.
//
//   node tools/migration/gen-nav-vectors.mjs
//
// Le monde de test est construit ici comme le fera le C++: `generateWorld`,
// puis `blocked` sur l'eau, puis `rebuildMoveCosts`. Pas de `Simulation`: la
// navigation ne depend que de cette couche, et l'y reduire garantit qu'on
// compare deux fois la meme chose.

import { writeFileSync, mkdirSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";

const ICI = dirname(fileURLToPath(import.meta.url));
const RACINE = join(ICI, "..", "..");

const argv = process.argv.slice(2);
const argOf = (f, d) => { const i = argv.indexOf(f); return i >= 0 && argv[i + 1] !== undefined ? argv[i + 1] : d; };
const REF = argOf("-ref", "C:/dev/Jeux IV Kingdoms").split(String.fromCharCode(92)).join("/");
const SORTIE = argOf("-out", join(RACINE, "Source", "AnastasisSim", "Private", "Tests", "AnastasisNavVectors.inl"));

const modUrl = (rel) => pathToFileURL(join(REF, rel)).href;
const { generateWorld } = await import(modUrl("src/sim/world.js"));
const { rebuildMoveCosts, footBlockedAt, moveCostAt } = await import(modUrl("src/sim/navGrid.js"));
const { findPath } = await import(modUrl("src/sim/pathfinding.js"));

/**
 * Le monde vu par la navigation, et rien d'autre.
 *
 * `tileTraversalCost` reprend `simulation.js` sans la congestion des
 * transports: elle appartient a la vague 5, et elle vaut 1 tant qu'aucune
 * charrette ne roule. Le C++ a la meme lacune, au meme endroit.
 */
function makeNavSim(seed, w, h) {
  const world = generateWorld(seed, w, h);
  const sim = { w, h, tiles: world.tiles };
  sim.blocked = new Uint8Array(w * h);
  for (let i = 0; i < w * h; i += 1) {
    if (sim.tiles[i].type === "water") sim.blocked[i] = 1;
  }
  sim.blockedAt = (x, y) => (x < 0 || y < 0 || x >= w || y >= h ? true : sim.blocked[y * w + x] === 1);
  sim.tileAt = (x, y) => (x < 0 || y < 0 || x >= w || y >= h ? null : sim.tiles[y * w + x]);
  rebuildMoveCosts(sim);
  sim.footBlockedAt = (x, y) => footBlockedAt(sim, x, y);
  sim.tileTraversalCost = (x, y, baseCost = 10) => {
    const mult = moveCostAt(sim, x, y);
    if (!Number.isFinite(mult)) return Infinity;
    return Math.max(3, baseCost * mult);
  };
  return sim;
}

// --- La batterie ------------------------------------------------------------
// Un monde de 64x64 suffit: ce qu'on teste est l'ordre d'exploration, pas la
// taille. Les cas sont choisis pour toucher chaque branche du JS.

const SEEDS = [33344, 7, 1204];
const W = 64;
const H = 64;

const sims = new Map();
const simFor = (seed) => {
  if (!sims.has(seed)) sims.set(seed, makeNavSim(seed, W, H));
  return sims.get(seed);
};

/** Premiere case libre trouvee a partir de (x, y), en spirale carree. */
function freeCellNear(sim, x, y) {
  for (let r = 0; r < Math.max(W, H); r += 1) {
    for (let dy = -r; dy <= r; dy += 1) {
      for (let dx = -r; dx <= r; dx += 1) {
        if (Math.max(Math.abs(dx), Math.abs(dy)) !== r) continue;
        const cx = x + dx;
        const cy = y + dy;
        if (cx < 0 || cy < 0 || cx >= W || cy >= H) continue;
        if (!sim.footBlockedAt(cx, cy)) return { x: cx + 0.5, y: cy + 0.5 };
      }
    }
  }
  return null;
}

const cas = [];
const ajouter = (nom, seed, start, target, options = {}) => {
  cas.push({ nom, seed, start, target, options });
};

for (const seed of SEEDS) {
  const sim = simFor(seed);
  const a = freeCellNear(sim, 8, 8);
  const b = freeCellNear(sim, 55, 55);
  const c = freeCellNear(sim, 8, 55);
  const d = freeCellNear(sim, 32, 32);

  ajouter(`traversee.${seed}`, seed, a, b);
  ajouter(`diagonale.${seed}`, seed, c, d);
  ajouter(`retour.${seed}`, seed, b, a);
  // Court: le voisinage immediat, la ou l'ordre des voisins decide seul.
  ajouter(`voisin.${seed}`, seed, d, { x: d.x + 1, y: d.y });
  ajouter(`voisin-diag.${seed}`, seed, d, { x: d.x + 1, y: d.y + 1 });
  // Budget d'expansion serre: force la sortie par `maxExpanded`.
  ajouter(`budget-expansion.${seed}`, seed, a, b, { maxExpanded: 40 });
  // Budget de cout serre: force la sortie par `maxCost`.
  ajouter(`budget-cout.${seed}`, seed, a, b, { maxCost: 12 });
  // Depart = arrivee: chemin vide, et c'est un succes.
  ajouter(`surplace.${seed}`, seed, d, { x: d.x, y: d.y });
  // Hors bornes des deux cotes.
  ajouter(`hors-bornes.${seed}`, seed, { x: -3, y: 2 }, d);
  ajouter(`hors-bornes-cible.${seed}`, seed, d, { x: W + 5, y: 2 });
}

// Cas d'eau: cible infranchissable, avec et sans tolerance.
{
  const seed = SEEDS[0];
  const sim = simFor(seed);
  let eau = null;
  for (let y = 0; y < H && !eau; y += 1) {
    for (let x = 0; x < W; x += 1) {
      if (sim.tiles[y * W + x].type === "water") { eau = { x: x + 0.5, y: y + 0.5 }; break; }
    }
  }
  const depart = freeCellNear(sim, 32, 32);
  if (eau) {
    ajouter(`eau.refus.${seed}`, seed, depart, eau);
    ajouter(`eau.tolere.${seed}`, seed, depart, eau, { allowBlockedTarget: true });
  }
}

// --- Emission ---------------------------------------------------------------

const bits = (x) => {
  const v = new DataView(new ArrayBuffer(8));
  v.setFloat64(0, x, true);
  return "0x" + v.getBigUint64(0, true).toString(16).padStart(16, "0") + "ull";
};

const L = [];
const emit = (s = "") => L.push(s);

emit("// GENERE AUTOMATIQUEMENT - ne pas editer a la main.");
emit("// Source: tools/migration/gen-nav-vectors.mjs");
emit("//");
emit("// Comparent la SUITE COMPLETE des points d'un chemin, chacun par son motif");
emit("// binaire. Un vecteur qui ne comparerait que le cout total laisserait passer");
emit("// un chemin de meme longueur passant par une autre rue — c'est-a-dire");
emit("// exactement ce que PORTAGE.md demande de ne pas laisser passer.");
emit("//");
emit("// Si un cas ne passe plus: soit le portage a devie, soit la reference JS a");
emit("// change et il faut regenerer. Ne jamais corriger un vecteur a la main.");
emit("");
emit("// clang-format off");
emit("");

emit(`static constexpr int32 NavWorldW = ${W};`);
emit(`static constexpr int32 NavWorldH = ${H};`);
emit(`static const uint32 NavSeeds[] = { ${SEEDS.map((s) => `${s}u`).join(", ")} };`);
emit("");

// Empreinte de la grille de couts: prouve que le stockage f32 concorde AVANT
// qu'un chemin ne soit cherche. Sans cela, un desaccord de chemin laisserait
// planer le doute sur la grille.
emit("struct FNavCostVector { uint32 Seed; int32 X; int32 Y; uint64 MoveCostBits; uint64 TraversalBits; int32 FootBlocked; };");
emit("static const FNavCostVector NavCostVectors[] = {");
for (const seed of SEEDS) {
  const sim = simFor(seed);
  for (const [x, y] of [[0, 0], [1, 3], [17, 5], [31, 31], [32, 33], [47, 12], [63, 63], [8, 55]]) {
    const mult = moveCostAt(sim, x, y);
    const trav = sim.tileTraversalCost(x, y, 10);
    emit(`\t{ ${seed}u, ${x}, ${y}, ${bits(mult)}, ${bits(trav)}, ${sim.footBlockedAt(x, y) ? 1 : 0} },`);
  }
}
emit("};");
emit("");

emit("struct FNavPathVector {");
emit("\tconst TCHAR* Name;");
emit("\tuint32 Seed;");
emit("\tuint64 StartXBits; uint64 StartYBits;");
emit("\tuint64 TargetXBits; uint64 TargetYBits;");
emit("\tuint64 MaxCostBits; int32 MaxExpanded;");
emit("\tint32 AllowBlockedStart; int32 AllowBlockedTarget;");
emit("\tint32 Found;          // 0 = la reference rend null");
emit("\tint32 PointCount;");
emit("\tint32 FirstPoint;     // index dans NavPathPoints");
emit("};");
emit("");

const points = [];
const lignes = [];
let echecs = 0;
for (const c of cas) {
  const sim = simFor(c.seed);
  const options = {
    maxCost: c.options.maxCost ?? 320,
    maxExpanded: c.options.maxExpanded ?? 3200,
    allowBlockedStart: c.options.allowBlockedStart ?? true,
    allowBlockedTarget: c.options.allowBlockedTarget ?? false,
  };
  const chemin = findPath(sim, c.start, c.target, options);
  const premier = points.length;
  if (Array.isArray(chemin)) {
    for (const p of chemin) points.push([p.x, p.y]);
  } else {
    echecs += 1;
  }
  lignes.push(
    `\t{ TEXT("${c.nom}"), ${c.seed}u, ` +
    `${bits(c.start.x)}, ${bits(c.start.y)}, ${bits(c.target.x)}, ${bits(c.target.y)}, ` +
    `${bits(options.maxCost)}, ${options.maxExpanded}, ` +
    `${options.allowBlockedStart ? 1 : 0}, ${options.allowBlockedTarget ? 1 : 0}, ` +
    `${Array.isArray(chemin) ? 1 : 0}, ${Array.isArray(chemin) ? chemin.length : 0}, ${premier} },`);
}

emit("static const uint64 NavPathPoints[][2] = {");
if (points.length === 0) emit("\t{ 0ull, 0ull },");
for (const [x, y] of points) emit(`\t{ ${bits(x)}, ${bits(y)} },`);
emit("};");
emit("");
emit("static const FNavPathVector NavPathVectors[] = {");
for (const l of lignes) emit(l);
emit("};");

mkdirSync(dirname(SORTIE), { recursive: true });
writeFileSync(SORTIE, L.join("\n") + "\n", "utf8");
console.error(
  `Ecrit: ${SORTIE} — ${cas.length} chemins (${echecs} sans solution), ` +
  `${points.length} points, ${SEEDS.length * 8} cases de cout`);
