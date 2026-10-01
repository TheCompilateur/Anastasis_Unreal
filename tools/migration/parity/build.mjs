// Parite du chantier — un batiment monte piece par piece sous les coups d'un batisseur.
//
// Ce qui se declare ici est pur, ou pur a batiment fourni :
//
//   simulation.js        buildCost / costMultiplier, siteCanPlacePiece, consumeSiteMaterials
//   constructionPieces   placeConstructionPieces, CONSTRUCTION_BLUEPRINT
//   craftWork.js         swingPeriodFor(npc, "build")
//   craftToolSwitch.js   craftToolSwitchSeconds
//   npc.js               createNpc (competence craft teintee), completionBias (session `build`)
//   metiers/catalog.js   JOBS[job].priority, traitBias.build ; content.js TRAITS
//
// `buildScore` et `pickBuildSite` sont internes a npc.js : leurs entrees (metier,
// trait, chantier) se declarent ici par les tables qu'ils lisent, et leur
// assemblage se prouve par Anastasis.Sim.Village.Chantier.*.

import { join, dirname } from "node:path";
import { fileURLToPath } from "node:url";
import { croiser } from "../parity-kit.mjs";

const ICI = dirname(fileURLToPath(import.meta.url));
const RACINE = join(ICI, "..", "..", "..");

const NATURE_NEUTRE = { corps: 1, esprit: 1, coeur: 1, qualities: [], flaws: [] };
const JOBS = ["settler", "farmer", "builder"];
const TRAITS = [0, 1, 2, 3, 4, 5];
const TYPES = ["well", "house", "granary"];

function habitant(m, trait, options = {}) {
  const rng = () => 0.5;
  const npc = m.npc.createNpc(rng, "npc-0", "Test", 10.5, 10.5, {
    jobId: options.jobId ?? "builder",
    trait: { ...m.content.TRAITS[trait] },
    nature: NATURE_NEUTRE,
    skill: options.skill ?? 1,
    skills: { craft: 1, gather: 1, trade: 1, care: 1 },
    age: 30,
  });
  Object.assign(npc, { hunger: 10, thirst: 10, energy: 90, social: 90, leisure: 90, hygiene: 90, health: 95, morale: 60 });
  return npc;
}

/** Le strict minimum que lisent les methodes du chantier, appelees hors de leur Simulation. */
function simChantier(m, acheves = 0) {
  const proto = m.simulation.Simulation.prototype;
  return {
    buildings: [],
    actors: [],
    market: { stock: { planks: 0 } },
    countBuildings: () => acheves,
    hasSawCapacity: () => false,
    costMultiplier: proto.costMultiplier,
    siteCanPlacePiece: proto.siteCanPlacePiece,
  };
}

function chantier(type, poses, devis, consomme, stock) {
  return {
    id: "building-9",
    type,
    x: 4,
    y: 4,
    progress: poses / 22,
    piecesPlaced: poses,
    materialsNeeded: { wood: devis[0], stone: devis[1] },
    materialsConsumed: { wood: consomme[0], stone: consomme[1] },
    stock: {
      wood: { physical: stock[0], reserved: 0 },
      stone: { physical: stock[1], reserved: 0 },
    },
  };
}

export default {
  modules: {
    npc: "src/sim/npc.js",
    content: "src/sim/content.js",
    craft: "src/sim/craftWork.js",
    toolSwitch: "src/sim/craftToolSwitch.js",
    pieces: "src/sim/constructionPieces.js",
    simulation: "src/sim/simulation.js",
  },
  out: join(RACINE, "Source", "AnastasisSim", "Private", "Tests", "AnastasisBuildVectors.inl"),

  cases: [
    {
      name: "Cost",
      comment: "buildCost(type) : devis bois / pierre selon les acheves du type (costMultiplier), sans scierie",
      args: ["string", "int"],
      ret: [
        { name: "wood", type: "int" },
        { name: "stone", type: "int" },
        { name: "mul", type: "double" },
      ],
      inputs: croiser(TYPES, [0, 1, 2, 3, 4, 10, 50, 102, 103, 200]),
      call: (m, [type, acheves]) => {
        const sim = simChantier(m, acheves);
        const cost = m.simulation.Simulation.prototype.buildCost.call(sim, type);
        return { wood: cost.wood | 0, stone: cost.stone | 0, mul: sim.costMultiplier(type) };
      },
    },
    {
      name: "Jobs",
      comment: "JOBS[job].priority.indexOf(build) -> jobPriority ; traitBias.build ; builderFit (buildScore)",
      args: ["string"],
      ret: [
        { name: "priority", type: "double" },
        { name: "traitBias", type: "double" },
      ],
      inputs: JOBS.map((j) => [j]),
      call: (m, [jobId]) => {
        const job = m.content.JOBS[jobId];
        const rank = job.priority.indexOf("build");
        return { priority: rank < 0 ? 0 : Math.max(0, 18 - rank * 2.5), traitBias: job.traitBias.build };
      },
    },
    {
      name: "Traits",
      comment: "TRAITS[i].build, et la competence craft de depart teintee (createNpc)",
      args: ["int"],
      ret: [
        { name: "build", type: "double" },
        { name: "craft", type: "double" },
      ],
      inputs: TRAITS.map((t) => [t]),
      call: (m, [trait]) => {
        const npc = habitant(m, trait);
        return { build: m.content.TRAITS[trait].build, craft: npc.skills.craft };
      },
    },
    {
      name: "Swing",
      comment: "swingPeriodFor(npc, build) — competence, coups de la session, energie",
      args: ["double", "int", "double"],
      ret: "double",
      inputs: croiser([0.7, 1, 1.5, 2.6, 2.67], [0, 4, 5, 10, 14, 20], [100, 42, 30, 5]),
      call: (m, [skill, coups, energie]) => {
        const npc = habitant(m, 3, { skill });
        npc.energy = energie;
        npc.workSession = { craftId: "build", buildingId: "building-9", tileX: 4, tileY: 4, swingsDone: coups };
        return m.craft.swingPeriodFor(npc, "build");
      },
    },
    {
      name: "ToolSwitch",
      comment: "craftToolSwitchSeconds(from, to) — '' = pas de session precedente",
      args: ["string", "string"],
      ret: "double",
      inputs: croiser(["", "farm", "build", "maintain", "chop", "workshop"], ["build", "farm", "maintain", "workshop"]),
      call: (m, [from, to]) => m.toolSwitch.craftToolSwitchSeconds(from || null, to),
    },
    {
      name: "Pieces",
      comment: "placeConstructionPieces(building, 1) depuis k pieces posees",
      args: ["int"],
      ret: [
        { name: "placed", type: "int" },
        { name: "progress", type: "double" },
        { name: "kind", type: "string" },
        { name: "moved", type: "bool" },
      ],
      inputs: Array.from({ length: 22 }, (_, k) => [k]),
      call: (m, [k]) => {
        const b = { type: "house", progress: k / 22, piecesPlaced: k };
        const piece = m.pieces.placeConstructionPieces(b, 1, 10);
        return { placed: b.piecesPlaced, progress: b.progress, kind: piece ? piece.kind : "", moved: Boolean(piece) };
      },
    },
    {
      name: "Materials",
      comment: "siteCanPlacePiece puis consumeSiteMaterials(building, 1) : part par piece, stock du site",
      args: ["int", "int", "int", "int", "int", "int", "int"],
      ret: [
        { name: "ok", type: "bool" },
        { name: "consumedWood", type: "int" },
        { name: "consumedStone", type: "int" },
        { name: "stockWood", type: "int" },
        { name: "stockStone", type: "int" },
      ],
      inputs: [
        ...croiser([0, 1, 5, 10, 20, 21], [[10, 18], [24, 8], [26, 16], [25, 9]], [0, 1, 3, 100])
          .map(([poses, [w, s], stock]) => [poses, w, s, Math.min(w, Math.floor((w * poses) / 22)), Math.min(s, Math.floor((s * poses) / 22)), Math.min(stock, 80), Math.min(stock, 60)]),
        // Une ressource deja soldee, l'autre a sec ; et un stock d'une seule ressource.
        [5, 10, 18, 10, 4, 0, 30],
        [5, 10, 18, 4, 18, 30, 0],
        [21, 24, 8, 23, 7, 1, 1],
        [21, 24, 8, 20, 7, 3, 1],
        [0, 0, 18, 0, 0, 0, 5],
      ],
      call: (m, [poses, devisW, devisS, consW, consS, stockW, stockS]) => {
        const sim = simChantier(m, 0);
        const b = chantier("house", poses, [devisW, devisS], [consW, consS], [stockW, stockS]);
        const ok = m.simulation.Simulation.prototype.consumeSiteMaterials.call(sim, b, 1);
        return {
          ok,
          consumedWood: b.materialsConsumed.wood | 0,
          consumedStone: b.materialsConsumed.stone | 0,
          stockWood: b.stock.wood.physical | 0,
          stockStone: b.stock.stone.physical | 0,
        };
      },
    },
    {
      name: "Completion",
      comment: "completionBias(sim, npc, goal) — session de chantier (craft build) ou de recolte, sac",
      args: ["string", "string", "int", "double"],
      ret: "double",
      inputs: croiser(
        ["build", "gatherFood", "deliver", "eat", "socialize", "maintain", "craft"],
        ["", "build", "farm"],
        [0, 3, 6],
        [10, 58],
      ),
      call: (m, [goal, craftId, sac, faim]) => {
        const npc = habitant(m, 3);
        npc.inventory.food = sac;
        npc.hunger = faim;
        npc.workSession = craftId ? { craftId, buildingId: "building-9", tileX: 4, tileY: 4, swingsDone: 0 } : null;
        return m.npc.completionBias({ actors: [], buildings: [], day: 1, time: 100 }, npc, goal);
      },
    },
  ],
};
