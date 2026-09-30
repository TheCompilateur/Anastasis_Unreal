// Parite de Noûs — la decision algorithmique du cycle faim (src/ai/algorithmic).
//
// Actif PAR DEFAUT dans la reference (ALGORITHMIC_NPC_V1 = true). Ce qui se
// declare ici est pur : scoreHungerCandidates sur un contexte FOURNI
// (`options.context` court-circuite perceiveFoodContext), et evaluateInertia.
// La perception, les reservations et le pont vers la table de buts lisent le
// village : ils se prouvent par les tests d'assemblage Anastasis.Sim.Village.Grenier.*.

import { join, dirname } from "node:path";
import { fileURLToPath } from "node:url";
import { croiser } from "../parity-kit.mjs";

const ICI = dirname(fileURLToPath(import.meta.url));
const RACINE = join(ICI, "..", "..", "..");

// Faim sur les bornes d'urgence (36, 58) et au-dela ; source connue ou non,
// proche ou lointaine ; inventaire ; cooldown sur seek_food.
const FAIMS = [0, 10, 35.99, 36, 47, 57.99, 58, 75, 98];
const CONTEXTES = [
  // inv, source, distance, estime, certitude, or, croyanceMarche
  [0, "", NaN, 0, 0.08, 0, 0],
  [0, "building-3", 6.2, 12, 0.9, 0, 0],
  [0, "building-3", 25, 12, 0.9, 0, 0],
  [0, "building-3", 6.2, 0, 0.9, 0, 0],
  [0, "building-3", 6.2, 12, 0.3, 0, 0],
  [1, "", NaN, 0, 1, 0, 0],
  [0, "", NaN, 0, 0.5, 5, 20],
];
const ENERGIES = [80, 30];
const EXCLUS = ["", "seek_food"];

const SORTIE = [
  { name: "bestType", type: "string" },
  { name: "bestScore", type: "double" },
  { name: "bestUrgency", type: "double" },
  { name: "order", type: "string" },
  { name: "eat", type: "double" },
  { name: "seek", type: "double" },
  { name: "buy", type: "double" },
  { name: "work", type: "double" },
  { name: "sleep", type: "double" },
  { name: "wait", type: "double" },
];

const scoreDe = (candidats, type) => candidats.find((c) => c.type === type)?.score ?? -1;

export default {
  module: "src/ai/algorithmic/hungerUtility.js",
  out: join(RACINE, "Source", "AnastasisSim", "Private", "Tests", "AnastasisNousVectors.inl"),

  cases: [
    {
      name: "ScoreHunger",
      comment: "scoreHungerCandidates(sim, npc, { context, excludeTypes })",
      args: ["double", "double", "int", "string", "double", "double", "double", "int", "double", "string"],
      ret: SORTIE,
      inputs: croiser(FAIMS, CONTEXTES, ENERGIES, EXCLUS)
        .map(([h, [inv, src, d, est, cert, gold, believed], energy, ex]) => [h, energy, inv, src, d, est, cert, gold, believed, ex]),
      call: (mod, [hunger, energy, inv, src, d, est, cert, gold, believed, ex]) => {
        const context = {
          hunger,
          inventoryFood: inv,
          believedFood: believed,
          bestSourceBuildingId: src || null,
          bestSourceDistance: Number.isNaN(d) ? null : d,
          bestSourceEstimated: est,
          bestSourceConfidence: cert,
          certainty: cert,
          dangerNear: false,
          gold,
        };
        const npc = { id: "npc-0", energy, speed: 4, hunger };
        const r = mod.scoreHungerCandidates({ time: 12.5 }, npc, { context, excludeTypes: ex ? [ex] : [] });
        return {
          bestType: r.best?.type ?? "",
          bestScore: r.best?.score ?? -1,
          bestUrgency: r.best?.urgency ?? -1,
          order: r.candidates.map((c) => c.type).join(","),
          eat: scoreDe(r.candidates, "eat"),
          seek: scoreDe(r.candidates, "seek_food"),
          buy: scoreDe(r.candidates, "buy_food"),
          work: scoreDe(r.candidates, "work"),
          sleep: scoreDe(r.candidates, "sleep"),
          wait: scoreDe(r.candidates, "wait"),
        };
      },
    },
  ],
};
