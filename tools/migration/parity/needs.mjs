// Parite des besoins — `src/life/needs.js`, la partie que le puits met en jeu.
//
// Le premier batiment porte est le puits (mission first-building-001) : sa
// boucle traverse urgeScore -> needGoalScores -> tickNeeds -> satisfyDrink.
// Ce sont des fonctions a entrees et sorties scalaires, donc declarees ici
// plutot qu'ecrites dans un generateur dedie.
//
// Hors perimetre, et pourquoi : les branches interieures de tickNeeds exigent
// `npc.inside` (chantier domestique, non porte). Les entrees ci-dessous ne les
// empruntent jamais — un but "rest" SANS interieur passe par la branche par
// defaut, exactement comme dans la reference.

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
  ],
};
