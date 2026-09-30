// Parite du rythme de Valmire — `src/life/villageRhythm.js`.
//
// C'est le rythme, pas la fatigue seule, qui couche le village : phaseBias(rest)
// vaut ~+91 la nuit pour qui a un toit. Le portage le prend pour TOUS les buts,
// parce que le plancher des buts non portes (AnastasisVillage.h) en a besoin.

import { join, dirname } from "node:path";
import { fileURLToPath } from "node:url";
import { croiser } from "../parity-kit.mjs";

const ICI = dirname(fileURLToPath(import.meta.url));
const RACINE = join(ICI, "..", "..", "..");

// Chaque phase, ses deux bords, et des fractions hors [0,1) (le `%` de JS).
const H = (h) => h / 24;
const FRACS = [0, H(4.999), H(5), H(6.99), H(7), H(11.49), H(11.5), H(13.49), H(13.5), H(17.49), H(17.5),
  H(20.99), H(21), 0.9999, 1.25, -0.1, 0.42, 0.875];

// Tous les buts de la table adulte, plus play (LIFE_GOALS).
const BUTS = ["eat", "eatTogether", "rest", "relax", "relieve", "drink", "gatherWood", "gatherStone",
  "gatherFood", "helpFarm", "sell", "buy", "build", "craft", "maintain", "deliver", "fetchInput", "haulJob",
  "aidHousehold", "visitFamily", "explore", "socialize", "confront", "shelterRain", "closeWorkplace", "play"];

// toit, energie, faim — energie 0 teste `npc?.energy || 100`.
const HABITANTS = [[true, 80, 20], [false, 30, 50], [true, 0, 45], [false, 44, 30]];

export default {
  module: "src/life/villageRhythm.js",
  out: join(RACINE, "Source", "AnastasisSim", "Private", "Tests", "AnastasisRhythmVectors.inl"),

  cases: [
    {
      name: "VillagePhase",
      comment: "villagePhase(frac).id",
      args: ["double"],
      ret: "string",
      inputs: FRACS.map((f) => [f]),
      call: (mod, [f]) => mod.villagePhase(f).id,
    },

    {
      name: "PhaseBias",
      comment: "phaseBias : adulte, pas garde, sans famille ni mode de vie",
      args: ["double", "string", "bool", "double", "double"],
      ret: "double",
      inputs: croiser([0.05, 0.25, 0.42, 0.5, 0.65, 0.8], BUTS, HABITANTS)
        .map(([f, but, [toit, energie, faim]]) => [f, but, toit, energie, faim]),
      call: (mod, [f, but, toit, energy, hunger]) => mod.phaseBias(
        { dayFrac: () => f },
        { home: toit ? { id: "b1" } : null, shelter: null, energy, hunger, jobId: "farmer", lifeStage: "adult" },
        but,
      ),
    },
  ],
};
