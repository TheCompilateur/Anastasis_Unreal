// Parite des facteurs de besoins PAR HABITANT — `src/life/needs.js`
// (mission needs-factors-001).
//
// Le rapport 2 de la phase 3 place la premiere divergence JS / Unreal au tick 1,
// sur les besoins : dans la reference, faim, soif et energie avancent au rythme
// propre de chaque habitant. Cinq facteurs le font :
//   phenotype.hydrationLossMultiplier, .metabolicDemandMultiplier,
//   .fatigueRecoveryMultiplier, et conditioning.fatigueAdaptation,
//   .recoveryConditioning (par leurs multiplicateurs).
//
// Chaque vecteur appelle `tickNeeds` TEL QUEL sur un habitant qui porte un
// phenotype et un conditionnement, dans l'une des quatorze situations qui
// choisissent une branche, et relit les huit metres PLUS le conditionnement
// avance par le `tickConditioning` de fin de tick. La declaration existante
// (`needs.mjs`) reste intacte : ses vecteurs prouvent le cas median.

import { join, dirname } from "node:path";
import { fileURLToPath } from "node:url";
import { croiser } from "../parity-kit.mjs";

const ICI = dirname(fileURLToPath(import.meta.url));
const RACINE = join(ICI, "..", "..", "..");

// Memes habitants que needs.mjs : chaque branche de needsCritical et de
// tickVitality y passe (famine, soif letale, epuisement, moral critique).
const HABITANTS = [
  // hunger, energy, social, leisure, hygiene, thirst, health, morale
  [10, 80, 70, 70, 70, 5, 95, 60],
  [20, 60, 50, 55, 60, 45, 90, 50],
  [40, 45, 40, 30, 40, 70, 60, 40],
  [60, 20, 20, 20, 30, 88, 41, 37],
  [30, 10, 10, 10, 10, 95, 27, 21],
  [90, 5, 5, 5, 5, 99, 5, 0],
  [0, 100, 100, 100, 100, 0, 100, 100],
  [50, 50, 50, 50, 50, 87.99, 42, 38],
  [50, 68, 50, 50, 50, 20, 80, 60],
];

/**
 * Les situations, par code. Le test C++ choisit la meme branche par le meme code.
 *   goal, inside.goal (null = dehors), point d'eau, fraction de jour, lieu de sleepQuality
 */
const SITUATIONS = [
  /* 0 */ ["drink", null, true, 0.42, 0], // boit au point d'eau
  /* 1 */ ["drink", null, false, 0.42, 0], // veut boire, loin de l'eau : branche par defaut
  /* 2 */ ["observer", null, false, 0.42, 0],
  /* 3 */ ["gatherWood", null, false, 0.42, 0], // travaille dehors
  /* 4 */ ["rest", "rest", false, 0.05, 2], // dort chez soi, la nuit
  /* 5 */ ["rest", "rest", false, 0.42, 3], // sieste chez autrui, le jour
  /* 6 */ ["eat", "eat", false, 0.42, 3],
  /* 7 */ ["socialize", "socialize", false, 0.42, 3],
  /* 8 */ ["socialize", null, false, 0.42, 0],
  /* 9 */ ["relax", null, false, 0.42, 0],
  /* 10 */ ["relieve", "relieve", false, 0.42, 3],
  /* 11 */ ["craft", "craft", false, 0.42, 3], // travaille DEDANS : branche par defaut, conditionnement de travail
  /* 12 */ ["rest", null, false, 0.42, 0], // veut dormir, dehors : branche par defaut, conditionnement de repos
  /* 13 */ ["relax", "relax", false, 0.42, 3],
];

// absent, hydratation, metabolisme, recuperation, workConditioning, fatigueAdaptation, recoveryConditioning
const FACTEURS = [
  [false, 1, 1, 1, 0.5, 0.5, 0.5],
  [false, 1.1063, 0.9218, 1.0457, 0.3, 0.92, 0.08],
  [false, 0.75, 1.25, 0.75, 1, 0, 1],
  [false, 1.25, 0.75, 1.25, 0, 1, 0],
  [true, 1, 1, 1, 0.5, 0.5, 0.5], // ni phenotype ni conditionnement sur l'habitant
];

const DTS = [1 / 60, 0.5, 3];

const npcLieu = (npc, lieu) => {
  if (npc.inside) npc.inside.buildingId = lieu === 3 ? "b2" : "b1";
  npc.home = lieu === 1 || lieu === 2 || lieu === 3 ? { id: "b1" } : null;
  npc.shelter = lieu === 4 ? { id: "b1" } : null;
  return npc;
};

const simPour = (eau, frac) => ({
  tileAt: () => ({ type: eau ? "water" : "grass", shore: 0 }),
  buildings: [],
  dayFrac: () => frac,
  time: frac * 90,
});

const METRES = ["hunger", "energy", "social", "leisure", "hygiene", "thirst", "health", "morale"];
const RETOUR = [...METRES, "workConditioning", "fatigueAdaptation", "recoveryConditioning"]
  .map((name) => ({ name, type: "double" }));

export default {
  module: "src/life/needs.js",
  out: join(RACINE, "Source", "AnastasisSim", "Private", "Tests", "AnastasisNeedsFactorsVectors.inl"),

  cases: [
    {
      name: "TickNeedsFactors",
      comment: "tickNeeds avec phenotype et conditionnement, quatorze situations, puis tickConditioning",
      // 8 metres, situation, fraction de jour, lieu, absent, 3 multiplicateurs, 3 meres, dt
      args: [
        "double", "double", "double", "double", "double", "double", "double", "double",
        "int", "double", "int",
        "bool", "double", "double", "double", "double", "double", "double",
        "double",
      ],
      ret: RETOUR,
      inputs: croiser(HABITANTS, SITUATIONS.map((_, i) => i), FACTEURS, DTS)
        .map(([h, code, f, dt]) => [...h, code, SITUATIONS[code][3], SITUATIONS[code][4], ...f, dt]),
      call: (mod, a) => {
        const [goal, insideGoal, eau, frac, lieu] = SITUATIONS[a[8]];
        const [absent, hyd, met, rec, work, fa, rc] = a.slice(11, 18);
        const npc = {
          hunger: a[0], energy: a[1], social: a[2], leisure: a[3], hygiene: a[4],
          thirst: a[5], health: a[6], morale: a[7], goal, starvingDays: 0,
          inside: insideGoal ? { goal: insideGoal } : null,
        };
        npcLieu(npc, lieu);
        if (!absent) {
          npc.phenotype = {
            hydrationLossMultiplier: hyd,
            metabolicDemandMultiplier: met,
            fatigueRecoveryMultiplier: rec,
          };
          npc.conditioning = { version: 2, workConditioning: work, fatigueAdaptation: fa, recoveryConditioning: rc };
        }
        mod.tickNeeds(simPour(eau, frac), npc, a[18]);
        return { ...npc, ...npc.conditioning };
      },
    },
    {
      name: "NeedsCritical",
      comment: "needsCritical — chaque seuil critique, de part et d'autre",
      args: ["double", "double", "double", "double", "double", "double", "double", "double"],
      ret: "bool",
      inputs: [
        ...HABITANTS,
        [57.99, 48.01, 35.01, 32.01, 34.01, 67.99, 28.01, 22],
        [58, 50, 50, 50, 50, 10, 90, 60],
        [10, 48, 50, 50, 50, 10, 90, 60],
        [10, 80, 35, 50, 50, 10, 90, 60],
        [10, 80, 50, 32, 50, 10, 90, 60],
        [10, 80, 50, 50, 34, 10, 90, 60],
        [10, 80, 50, 50, 50, 68, 90, 60],
        [10, 80, 50, 50, 50, 10, 28, 60],
        [10, 80, 50, 50, 50, 10, 90, 21.99],
      ],
      call: (mod, a) => mod.needsCritical({
        hunger: a[0], energy: a[1], social: a[2], leisure: a[3], hygiene: a[4],
        thirst: a[5], health: a[6], morale: a[7], starvingDays: 0,
      }),
    },
  ],
};
