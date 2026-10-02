// Vecteurs de l'exploration et des tirages de la decision — mission perception-explore-001.
//
//   node tools/migration/gen-explore-vectors.mjs -ref <clone anastasis-ref-p3> [-ticks 5400]
//
// Deux familles :
//
//   1. `exploreTarget` (src/ai/memory.js) et `randomWalkTarget` (Simulation, simulation.js),
//      executes tels quels sur un monde de test (generateWorld + eau bloquee, comme
//      gen-nav-vectors.mjs) : regions connues ou non, cases bloquees, repli en promenade,
//      repli au centre du village. On compare la cible, l'etat du flux apres, et le nombre
//      de tirages.
//
//   2. Les decisions MESUREES du scenario endurance (rng-trace-lib.mjs) : pour chaque
//      `chooseGoal`, la photo de l'habitant (position, regions connues, intention du jour),
//      l'etat du flux au premier tirage porte (`exploreTarget`), le nombre de tirages
//      d'exploreTarget et de goalNoise, et l'etat du flux apres le dernier bruit. Le test de
//      village (Anastasis.Sim.Village.TiragesDecision) reprend le scenario, pose la photo,
//      appelle la decision C++ et compare.

import { writeFileSync, mkdirSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";

import { releverTirages, decisionsDe } from "./rng-trace-lib.mjs";
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
const TICKS = Number(argOf("-ticks", 5400));
const SORTIE = argOf("-out", join(RACINE, "Source", "AnastasisSim", "Private", "Tests", "AnastasisExploreVectors.inl"));

const url = (rel) => pathToFileURL(join(REF_DIR, rel)).href;
const { generateWorld } = await import(url("src/sim/world.js"));
const { rebuildMoveCosts, footBlockedAt } = await import(url("src/sim/navGrid.js"));
const { exploreTarget } = await import(url("src/ai/memory.js"));
const { Simulation } = await import(url("src/sim/simulation.js"));
const { makeRng } = await import(url("src/sim/rng.js"));

const bits = (x) => {
  const v = new DataView(new ArrayBuffer(8));
  v.setFloat64(0, x, true);
  return "0x" + v.getBigUint64(0, true).toString(16).padStart(16, "0") + "ull";
};

// --- 1. Monde de test ------------------------------------------------------------------

const W = 64;
const H = 64;
const CELL = 8;
const COLS = Math.ceil(W / CELL);

/** Le monde vu par l'exploration : eau bloquee, plus un bloc de bati optionnel. */
function mondeDe(seed, bloc) {
  const world = generateWorld(seed, W, H);
  const sim = { w: W, h: H, tiles: world.tiles, settlement: { x: 31.5, y: 30.25 } };
  sim.blocked = new Uint8Array(W * H);
  for (let i = 0; i < W * H; i += 1) if (sim.tiles[i].type === "water") sim.blocked[i] = 1;
  if (bloc) {
    for (let y = bloc.y0; y <= bloc.y1; y += 1) for (let x = bloc.x0; x <= bloc.x1; x += 1) sim.blocked[y * W + x] = 1;
  }
  sim.blockedAt = (x, y) => (x < 0 || y < 0 || x >= W || y >= H ? true : sim.blocked[y * W + x] === 1);
  sim.tileAt = (x, y) => (x < 0 || y < 0 || x >= W || y >= H ? null : sim.tiles[y * W + x]);
  rebuildMoveCosts(sim);
  // Les deux questions de la reference, telles que Simulation les pose.
  sim.isBlocked = (x, y) => sim.blockedAt(Math.floor(x), Math.floor(y));
  sim.isFootBlocked = (x, y) => footBlockedAt(sim, Math.floor(x) | 0, Math.floor(y) | 0);
  sim.randomWalkTarget = function randomWalkTarget(actor) { return Simulation.prototype.randomWalkTarget.call(this, actor); };
  return sim;
}

/** Les regions de la fenetre 7x7 autour de l'origine (toutes celles qu'exploreTarget peut viser). */
function fenetre(x, y) {
  const ox = Math.floor(x / CELL);
  const oy = Math.floor(y / CELL);
  const out = [];
  for (let dy = -3; dy <= 3; dy += 1) {
    for (let dx = -3; dx <= 3; dx += 1) {
      const cx = Math.min(COLS - 1, Math.max(0, ox + dx));
      const cy = Math.min(Math.ceil(H / CELL) - 1, Math.max(0, oy + dy));
      out.push(cy * COLS + cx);
    }
  }
  return [...new Set(out)].sort((a, b) => a - b);
}

const GRAINES_MONDE = [33344, 7, 1204];
const ETATS = [0, 12345, 2576143622, 4294967295];
const POSITIONS = [[8.5, 8.5], [31.2, 30.7], [55.9, 4.1], [2.0, 61.5], [40.25, 22.75]];
const BLOC = { x0: 10, y0: 10, x1: 54, y1: 54 };

const cas = [];
for (const seed of GRAINES_MONDE) {
  for (const [x, y] of POSITIONS) {
    for (const etat of ETATS) {
      const tout = fenetre(x, y);
      const moitie = tout.filter((_, i) => i % 2 === 0);
      for (const [nom, cells, bloc] of [["vide", [], null], ["moitie", moitie, null], ["fenetre", tout, null]]) {
        cas.push({ nom, seed, x, y, etat, cells, bloc });
      }
    }
  }
}
// Repli au centre : tout est connu ET tout est bati autour.
for (const etat of ETATS) cas.push({ nom: "centre", seed: 7, x: 32.5, y: 32.5, etat, cells: fenetre(32.5, 32.5), bloc: BLOC });
// Dans le bloc, regions inconnues : exploreTarget tombe sur du bati (rates) puis sort.
for (const etat of ETATS) cas.push({ nom: "bati", seed: 7, x: 30.5, y: 30.5, etat, cells: [], bloc: BLOC });

const mondes = new Map();
const mondePour = (c) => {
  const cle = `${c.seed}:${c.bloc ? 1 : 0}`;
  if (!mondes.has(cle)) mondes.set(cle, mondeDe(c.seed, c.bloc));
  return mondes.get(cle);
};

const purs = [];
for (const c of cas) {
  const sim = mondePour(c);
  const base = makeRng(0);
  base.setState(c.etat);
  let tirages = 0;
  sim.rng = () => { tirages += 1; return base(); };
  const npc = { x: c.x, y: c.y, mind: { cells: Object.fromEntries(c.cells.map((k) => [k, 1])) } };
  const p = exploreTarget(sim, npc);
  // Le repli au centre rend une COPIE de `this.settlement` : on le reconnait au nombre de
  // tirages (14 essais puis 16 promenades, tous rates) et aux coordonnees.
  const auCentre = tirages === 2 * 14 + 2 * 16 && p.x === sim.settlement.x && p.y === sim.settlement.y;
  purs.push({ ...c, px: p.x, py: p.y, apres: base.state(), tirages, centre: auCentre });
}

// randomWalkTarget seul : sur le monde ouvert, et dans le bloc (repli au centre).
const marches = [];
for (const [seed, bloc] of [[33344, null], [7, BLOC]]) {
  const sim = mondeDe(seed, bloc);
  for (const [x, y] of [...POSITIONS, [32.5, 32.5]]) {
    for (const etat of ETATS) {
      const base = makeRng(0);
      base.setState(etat);
      let tirages = 0;
      sim.rng = () => { tirages += 1; return base(); };
      const p = sim.randomWalkTarget({ x, y });
      const auCentre = tirages === 2 * 16 && p.x === sim.settlement.x && p.y === sim.settlement.y;
      marches.push({ seed, bloc: Boolean(bloc), x, y, etat, px: p.x, py: p.y, apres: base.state(), tirages, centre: auCentre });
    }
  }
}

// --- 2. Decisions mesurees ---------------------------------------------------------------

const scenario = chargerScenario(join(ICI, "scenarios", "endurance.json"));
const { tirages } = await releverTirages(REF_DIR, scenario, TICKS);
const decisions = [];
for (const d of decisionsDe(tirages)) {
  const photo = d.tirages[0].photo;
  const explore = d.tirages.filter((x) => x.site.startsWith("exploreTarget "));
  const bruits = d.tirages.filter((x) => x.site.startsWith("goalNoise "));
  const autres = d.tirages.filter((x) => !x.site.startsWith("exploreTarget ") && !x.site.startsWith("goalNoise "));
  const premierPorte = d.tirages.findIndex((x) => x.site.startsWith("exploreTarget ") || x.site.startsWith("goalNoise "));
  // Le bloc porte doit etre CONTIGU (exploreTarget puis bruits) pour etre rejoue depuis son etat.
  const bloc = d.tirages.slice(premierPorte);
  const contigu = bloc.length === explore.length + bruits.length
    && bloc.every((x, i) => (i < explore.length ? x.site.startsWith("exploreTarget ") : x.site.startsWith("goalNoise ")));
  const dernier = bloc[bloc.length - 1];
  const rng = makeRng(0);
  rng.setState(dernier.etat);
  rng();
  decisions.push({
    t: d.t, npc: d.npc, photo,
    etat: bloc[0].etat, apres: rng.state(),
    explore: explore.length, bruits: bruits.length,
    autres: autres.map((x) => x.site.split(" ")[0]),
    contigu,
  });
}

// --- Emission ------------------------------------------------------------------------------

const L = [];
const emit = (s = "") => L.push(s);
emit("// GENERE AUTOMATIQUEMENT - ne pas editer a la main.");
emit("// Source: tools/migration/gen-explore-vectors.mjs (reference anastasis-ref-p3)");
emit("//");
emit("// exploreTarget (memory.js) et randomWalkTarget (simulation.js) executes tels quels ;");
emit(`// decisions mesurees du scenario endurance, ${TICKS} ticks. Ne jamais corriger un vecteur a la main.`);
emit("");
emit("// clang-format off");
emit("");
emit(`static constexpr int32 ExploreWorldW = ${W};`);
emit(`static constexpr int32 ExploreWorldH = ${H};`);
emit(`static constexpr double ExploreSettlementX = 31.5;`);
emit(`static constexpr double ExploreSettlementY = 30.25;`);
emit(`static constexpr int32 ExploreBlockX0 = ${BLOC.x0}, ExploreBlockY0 = ${BLOC.y0}, ExploreBlockX1 = ${BLOC.x1}, ExploreBlockY1 = ${BLOC.y1};`);
emit("");
emit("static const int32 ExploreCells[] = {");
const lignesCas = [];
let premier = 0;
const cellsLigne = [];
for (const p of purs) {
  for (const k of p.cells) cellsLigne.push(k);
  lignesCas.push(`\t{ "${p.nom}", ${p.seed}u, ${p.bloc ? 1 : 0}, ${bits(p.x)}, ${bits(p.y)}, ${p.etat >>> 0}u, ${premier}, ${p.cells.length}, `
    + `${bits(p.px)}, ${bits(p.py)}, ${p.apres >>> 0}u, ${p.tirages}, ${p.centre ? 1 : 0} },`);
  premier += p.cells.length;
}
emit(cellsLigne.length ? `\t${cellsLigne.join(", ")}` : "\t-1");
emit("};");
emit("");
emit("struct FExploreCaseVector { const char* Name; uint32 Seed; int32 Block; uint64 XBits; uint64 YBits; uint32 State; int32 FirstCell; int32 CellCount;");
emit("\tuint64 PXBits; uint64 PYBits; uint32 StateAfter; int32 Draws; int32 Settlement; };");
emit("static const FExploreCaseVector ExploreCases[] = {");
for (const l of lignesCas) emit(l);
emit("};");
emit("");
emit("struct FRandomWalkVector { uint32 Seed; int32 Block; uint64 XBits; uint64 YBits; uint32 State; uint64 PXBits; uint64 PYBits; uint32 StateAfter; int32 Draws; int32 Settlement; };");
emit("static const FRandomWalkVector RandomWalkCases[] = {");
for (const m of marches) {
  emit(`\t{ ${m.seed}u, ${m.bloc ? 1 : 0}, ${bits(m.x)}, ${bits(m.y)}, ${m.etat >>> 0}u, ${bits(m.px)}, ${bits(m.py)}, ${m.apres >>> 0}u, ${m.tirages}, ${m.centre ? 1 : 0} },`);
}
emit("};");
emit("");
emit("// Decisions mesurees. Regions connues : DecisionCells[FirstCell .. FirstCell + CellCount).");
emit("static const int32 DecisionCells[] = {");
const dcells = [];
const lignesDec = [];
let premierD = 0;
for (const d of decisions) {
  const cells = d.photo?.cells ?? [];
  for (const k of cells) dcells.push(k);
  const intent = d.photo?.intent?.id ?? "";
  lignesDec.push(`\t{ ${d.t}, "${d.npc}", ${bits(d.photo?.x ?? NaN)}, ${bits(d.photo?.y ?? NaN)}, ${premierD}, ${cells.length}, `
    + `${d.etat >>> 0}u, ${d.apres >>> 0}u, ${d.explore}, ${d.bruits}, ${d.contigu ? 1 : 0}, "${intent}", "${d.autres.join(",")}" },`);
  premierD += cells.length;
}
emit(dcells.length ? `\t${dcells.join(", ")}` : "\t-1");
emit("};");
emit("");
emit("struct FDecisionDrawVector { int32 Tick; const char* Npc; uint64 XBits; uint64 YBits; int32 FirstCell; int32 CellCount;");
emit("\tuint32 State; uint32 StateAfter; int32 ExploreDraws; int32 NoiseDraws; int32 Contiguous; const char* DayIntent; const char* OtherDraws; };");
emit("static const FDecisionDrawVector DecisionDraws[] = {");
for (const l of lignesDec) emit(l);
emit("};");

mkdirSync(dirname(SORTIE), { recursive: true });
writeFileSync(SORTIE, L.join("\n") + "\n", "utf8");
const repli = purs.filter((p) => p.tirages > 28).length;
const centre = purs.filter((p) => p.centre).length;
console.error(`Ecrit: ${SORTIE}\n  ${purs.length} cas exploreTarget (${repli} en promenade, ${centre} au centre), `
  + `${marches.length} randomWalkTarget, ${decisions.length} decisions (${decisions.filter((d) => !d.contigu).length} non contigues, `
  + `${decisions.filter((d) => d.autres.length).length} avec d'autres tirages : ${[...new Set(decisions.flatMap((d) => d.autres))].join(", ")})`);
