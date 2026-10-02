// Parite du conditionnement — `src/life/conditioning.js` (mission needs-factors-001).
//
// tickConditioning fait deriver les deux meres que les besoins lisent
// (fatigueAdaptation, recoveryConditioning) : chaque combinaison des quatre
// drapeaux, sur des valeurs aux bornes, et des pas de temps de 1/60 s a un
// pas tres long qui fait toucher les bornes de clamp01.

import { join, dirname } from "node:path";
import { fileURLToPath } from "node:url";
import { croiser } from "../parity-kit.mjs";

const ICI = dirname(fileURLToPath(import.meta.url));
const RACINE = join(ICI, "..", "..", "..");

const ETATS = [
  // workConditioning, fatigueAdaptation, recoveryConditioning
  [0.5, 0.5, 0.5],
  [0, 1, 0.25],
  [1, 0, 0.75],
  [0.3127, 0.8813, 0.0042],
];
const DRAPEAUX = croiser([false, true], [false, true], [false, true], [false, true]);
const DTS = [1 / 60, 0.25, 3, 200];
const VALEURS = [-1, -0, 0, 0.1 + 0.2, 0.5, 0.75, 1, 2, NaN];

export default {
  module: "src/life/conditioning.js",
  out: join(RACINE, "Source", "AnastasisSim", "Private", "Tests", "AnastasisConditioningVectors.inl"),

  cases: [
    {
      name: "ConditioningTick",
      comment: "tickConditioning — working, resting, overworked, fatigued",
      args: ["double", "double", "double", "double", "bool", "bool", "bool", "bool"],
      ret: [
        { name: "workConditioning", type: "double" },
        { name: "fatigueAdaptation", type: "double" },
        { name: "recoveryConditioning", type: "double" },
      ],
      inputs: croiser(ETATS, DTS, DRAPEAUX).map(([[w, f, r], dt, flags]) => [w, f, r, dt, ...flags]),
      call: (mod, [w, f, r, dt, working, resting, overworked, fatigued]) => {
        const npc = { conditioning: { version: 2, workConditioning: w, fatigueAdaptation: f, recoveryConditioning: r } };
        mod.tickConditioning(npc, dt, { working, resting, overworked, fatigued });
        return npc.conditioning;
      },
    },
    {
      name: "ConditioningMultipliers",
      comment: "fatigueAdaptationEnergyFallMultiplier / recoveryConditioningEnergyGainMultiplier",
      args: ["double"],
      ret: [
        { name: "energyFall", type: "double" },
        { name: "energyGain", type: "double" },
      ],
      inputs: VALEURS.map((v) => [v]),
      call: (mod, [v]) => ({
        energyFall: mod.fatigueAdaptationEnergyFallMultiplier(v),
        energyGain: mod.recoveryConditioningEnergyGainMultiplier(v),
      }),
    },
    {
      name: "ConditioningNeutral",
      comment: "valeur absente (`?? 0.5`) et ensureConditioning",
      args: [],
      ret: [
        { name: "energyFall", type: "double" },
        { name: "energyGain", type: "double" },
        { name: "version", type: "int" },
        { name: "workConditioning", type: "double" },
        { name: "fatigueAdaptation", type: "double" },
        { name: "recoveryConditioning", type: "double" },
      ],
      inputs: [[]],
      call: (mod) => {
        const c = mod.ensureConditioning({});
        return {
          energyFall: mod.fatigueAdaptationEnergyFallMultiplier(undefined),
          energyGain: mod.recoveryConditioningEnergyGainMultiplier(undefined),
          ...c,
        };
      },
    },
  ],
};
