// Parite des besoins — `src/life/needs.js`, la partie que le puits met en jeu.
//
// Le premier batiment porte est le puits (mission first-building-001) : sa
// boucle traverse urgeScore -> needGoalScores -> tickNeeds -> satisfyDrink.
// Ce sont des fonctions a entrees et sorties scalaires, donc declarees ici
// plutot qu'ecrites dans un generateur dedie.
//
// La maison (mission house-rest-001) ajoute la branche interieure `rest` de
// tickNeeds, satisfyRest et sleepQuality. Le grenier ajoute la branche `eat`.
// Socialiser et souffler (mission social-relax-001) ajoutent les branches
// `socialize` et `relax` (dedans, ou dehors tant que c'est le but), satisfySocial
// et satisfyRelax. Reste sans boucle : `relieve`.

import { join, dirname } from "node:path";
import { fileURLToPath } from "node:url";
import { croiser } from "../parity-kit.mjs";

const ICI = dirname(fileURLToPath(import.meta.url));
const RACINE = join(ICI, "..", "..", "..");

// Pressions posees sur les bornes de urgeScore : 0, urge*0.55, urge, critical,
// et de part et d'autre. Un seuil teste en son milieu ne prouve pas ou il est.
const PRESSIONS = [-1, 0, 0.5, 21.99, 22, 22.01, 39.99, 40, 40.01, 55, 67.99, 68, 68.01, 88, 100];

// Habitants choisis pour traverser chaque branche de needGoalScores : sante sous
// healthUrge / healthCritical, moral sous moraleUrge / moraleCritical, soif letale
// (>= parchedAt) avec un besoin non paye plus fort, et un habitant ordinaire.
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
  [50, 50, 50, 50, 50, 88, 28, 22],
];

const npcDe = ([hunger, energy, social, leisure, hygiene, thirst, health, morale], goal = "observer") => ({
  hunger, energy, social, leisure, hygiene, thirst, health, morale, goal, starvingDays: 0,
});

const SCORES = ["eat", "rest", "socialize", "relax", "relieve", "drink"].map((name) => ({ name, type: "double" }));
const METRES = ["hunger", "energy", "social", "leisure", "hygiene", "thirst", "health", "morale"]
  .map((name) => ({ name, type: "double" }));

// Un monde minimal : `atDrinkSpot` ne lit que la tuile sous l'habitant et la
// liste des puits. Une tuile d'eau vaut point d'eau, une prairie non.
const simPour = (auPointDEau) => ({
  tileAt: () => ({ type: auPointDEau ? "water" : "grass", shore: 0 }),
  buildings: [],
  time: 0,
});

const DTS = [1 / 60, 0.5, 3];

// Fractions de jour : nuit (0.05, 0.9), aube, matin, midi, apres-midi, soir,
// et les deux bords de la nuit (21h pile, 5h pile).
const FRACS = [0.05, 0.25, 0.42, 0.5, 0.65, 0.8, 0.875, 0.8749, 0.20833333333333334, 0.9];

// Cas de lieu pour sleepQuality / satisfyRest :
//   0 dehors sans toit | 1 dehors avec toit | 2 dans son foyer | 3 chez autrui | 4 dans son abri
const LIEUX = [0, 1, 2, 3, 4];
const npcLieu = (npc, lieu) => {
  npc.inside = lieu >= 2 ? { goal: "rest", buildingId: lieu === 3 ? "b2" : "b1" } : null;
  npc.home = lieu === 1 || lieu === 2 || lieu === 3 ? { id: "b1" } : null;
  npc.shelter = lieu === 4 ? { id: "b1" } : null;
  return npc;
};
const simJour = (frac) => ({ dayFrac: () => frac, time: frac * 90, buildings: [] });

export default {
  module: "src/life/needs.js",
  out: join(RACINE, "Source", "AnastasisSim", "Private", "Tests", "AnastasisNeedsVectors.inl"),

  cases: [
    {
      name: "UrgeScore",
      comment: "urgeScore, seuils de la soif (40 / 68)",
      args: ["double", "double", "double"],
      ret: "double",
      inputs: PRESSIONS.map((p) => [p, 40, 68]),
      call: (mod, [p, u, c]) => mod.urgeScore(p, u, c),
    },

    {
      name: "NeedGoalScores",
      comment: "needGoalScores, puits et tavernes comptes par le monde",
      args: ["double", "double", "double", "double", "double", "double", "double", "double", "int", "int"],
      ret: SCORES,
      inputs: croiser(HABITANTS, [0, 2], [0, 1]).map(([h, puits, tavernes]) => [...h, puits, tavernes]),
      call: (mod, a) => mod.needGoalScores(
        { countBuildings: (type) => (type === "well" ? a[8] : type === "tavern" ? a[9] : 0) },
        npcDe(a.slice(0, 8)),
      ),
    },

    {
      name: "TickNeeds",
      comment: "tickNeeds hors interieur (boit / defaut / travail) puis tickVitality",
      args: ["double", "double", "double", "double", "double", "double", "double", "double", "string", "bool", "double"],
      ret: METRES,
      inputs: croiser(HABITANTS, [["drink", true], ["drink", false], ["observer", false], ["gatherWood", false]], DTS)
        .map(([h, [goal, eau], dt]) => [...h, goal, eau, dt]),
      call: (mod, a) => {
        const npc = npcDe(a.slice(0, 8), a[8]);
        mod.tickNeeds(simPour(a[9]), npc, a[10]);
        return npc;
      },
    },

    {
      name: "TickNeedsRest",
      comment: "tickNeeds, branche interieure rest (nuit / sieste, foyer / ailleurs)",
      args: ["double", "double", "double", "double", "double", "double", "double", "double", "double", "bool", "double"],
      ret: METRES,
      inputs: croiser(HABITANTS, FRACS.filter((_, i) => i % 3 === 0 || i === 1), [true, false], DTS)
        .map(([h, frac, foyer, dt]) => [...h, frac, foyer, dt]),
      call: (mod, a) => {
        const npc = npcDe(a.slice(0, 8), "rest");
        npcLieu(npc, a[9] ? 2 : 3);
        mod.tickNeeds(simJour(a[8]), npc, a[10]);
        return npc;
      },
    },

    {
      name: "SatisfyRest",
      comment: "satisfyRest : nuit / jour, dedans / dehors, foyer / abri / ailleurs",
      args: ["double", "double", "double", "double", "int"],
      ret: [
        { name: "energy", type: "double" },
        { name: "leisure", type: "double" },
        { name: "morale", type: "double" },
      ],
      inputs: croiser([[5, 20, 40], [40, 60, 0], [80, 97, 55], [99, 100, 99]], [0.05, 0.42, 0.9], LIEUX)
        .map(([[energy, leisure, morale], frac, lieu]) => [energy, leisure, morale, frac, lieu]),
      call: (mod, [energy, leisure, morale, frac, lieu]) => {
        const npc = npcLieu({ hunger: 10, energy, social: 60, leisure, hygiene: 60, thirst: 10, health: 90, morale, jobId: "farmer", starvingDays: 0 }, lieu);
        mod.satisfyRest(simJour(frac), npc);
        return npc;
      },
    },

    {
      name: "TickNeedsEat",
      comment: "tickNeeds, branche interieure eat (grenier mission granary-eat-001)",
      args: ["double", "double", "double", "double", "double", "double", "double", "double", "double"],
      ret: METRES,
      inputs: croiser(HABITANTS, DTS).map(([h, dt]) => [...h, dt]),
      call: (mod, a) => {
        const npc = npcDe(a.slice(0, 8), "eat");
        npc.inside = { goal: "eat", buildingId: "b9" };
        mod.tickNeeds(simJour(0.42), npc, a[8]);
        return npc;
      },
    },

    {
      name: "SatisfyEat",
      comment: "satisfyEat : dedans / dehors, foyer / ailleurs",
      args: ["double", "double", "double", "double", "double", "int"],
      ret: [
        { name: "hunger", type: "double" },
        { name: "morale", type: "double" },
        { name: "leisure", type: "double" },
        { name: "health", type: "double" },
        { name: "hygiene", type: "double" },
      ],
      inputs: croiser([[90, 50, 60, 70, 40], [30, 0, 97, 97, 98], [12, 99, 10, 50, 60], [70, 20, 40, 20, 5]], [0, 1, 2, 3, 4])
        .map(([[hunger, morale, leisure, health, hygiene], lieu]) => [hunger, morale, leisure, health, hygiene, lieu]),
      call: (mod, [hunger, morale, leisure, health, hygiene, lieu]) => {
        const npc = npcLieu({ hunger, energy: 70, social: 60, leisure, hygiene, thirst: 10, health, morale, starvingDays: 2 }, lieu);
        if (npc.inside) npc.inside.goal = "eat";
        mod.satisfyEat(npc);
        return npc;
      },
    },

    {
      name: "SatisfyDrink",
      comment: "satisfyDrink, moral nul compris (npc.morale || 50)",
      args: ["double", "double", "double", "double"],
      ret: [
        { name: "thirst", type: "double" },
        { name: "hygiene", type: "double" },
        { name: "morale", type: "double" },
        { name: "health", type: "double" },
      ],
      inputs: [
        [80, 50, 50, 70], [30, 97, 99, 98], [62, 0, 0, 0], [0, 100, 100, 100], [100, 20, 1, 50], [61.5, 94.25, 0, 96.5],
      ],
      call: (mod, [thirst, hygiene, morale, health]) => {
        const npc = { hunger: 0, energy: 70, social: 60, leisure: 60, thirst, hygiene, morale, health, starvingDays: 0 };
        mod.satisfyDrink(npc);
        return npc;
      },
    },

    {
      name: "TickNeedsSocial",
      comment: "tickNeeds, branche socialize : dedans (insideGoal) ou dehors (but socialize, gain x 0,45)",
      args: ["double", "double", "double", "double", "double", "double", "double", "double", "bool", "double"],
      ret: METRES,
      inputs: croiser(HABITANTS, [true, false], DTS).map(([h, dedans, dt]) => [...h, dedans, dt]),
      call: (mod, a) => {
        const npc = npcDe(a.slice(0, 8), "socialize");
        npc.inside = a[8] ? { goal: "socialize", buildingId: "b9" } : null;
        mod.tickNeeds(simPour(false), npc, a[9]);
        return npc;
      },
    },

    {
      name: "TickNeedsRelax",
      comment: "tickNeeds, branche relax : dedans ou dehors, meme formule",
      args: ["double", "double", "double", "double", "double", "double", "double", "double", "bool", "double"],
      ret: METRES,
      inputs: croiser(HABITANTS, [true, false], DTS).map(([h, dedans, dt]) => [...h, dedans, dt]),
      call: (mod, a) => {
        const npc = npcDe(a.slice(0, 8), "relax");
        npc.inside = a[8] ? { goal: "relax", buildingId: "b9" } : null;
        mod.tickNeeds(simPour(false), npc, a[9]);
        return npc;
      },
    },

    {
      name: "SatisfySocial",
      comment: "satisfySocial(npc, amount) : ambiant 14, conversation 38 / 44, dedans / dehors",
      args: ["double", "double", "double", "double", "bool"],
      ret: [
        { name: "social", type: "double" },
        { name: "morale", type: "double" },
        { name: "leisure", type: "double" },
      ],
      inputs: croiser([[0, 0, 0], [30, 22, 50], [70, 60, 97], [99, 100, 100]], [14, 38, 44], [true, false])
        .map(([[social, morale, leisure], amount, dedans]) => [social, morale, leisure, amount, dedans]),
      call: (mod, [social, morale, leisure, amount, dedans]) => {
        const npc = { hunger: 10, energy: 60, social, leisure, hygiene: 60, thirst: 10, health: 90, morale, starvingDays: 0 };
        npc.inside = dedans ? { goal: "socialize", buildingId: "b9" } : null;
        mod.satisfySocial(npc, amount);
        return npc;
      },
    },

    {
      name: "SatisfyRelax",
      comment: "satisfyRelax(npc) : dedans vers 78 de loisir, dehors +46",
      args: ["double", "double", "double", "bool"],
      ret: [
        { name: "leisure", type: "double" },
        { name: "energy", type: "double" },
        { name: "morale", type: "double" },
      ],
      inputs: croiser([[0, 10, 0], [40, 50, 30], [77.99, 95, 60], [78, 100, 98], [99, 97, 100]], [true, false])
        .map(([[leisure, energy, morale], dedans]) => [leisure, energy, morale, dedans]),
      call: (mod, [leisure, energy, morale, dedans]) => {
        const npc = { hunger: 10, energy, social: 60, leisure, hygiene: 60, thirst: 10, health: 90, morale, starvingDays: 0 };
        npc.inside = dedans ? { goal: "relax", buildingId: "b9" } : null;
        mod.satisfyRelax(npc);
        return npc;
      },
    },
  ],
};
